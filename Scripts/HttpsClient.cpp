#include "HttpsClient.h"
#include "HttpsRoots.inc"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <pspkernel.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace Update
{
	static const unsigned int kChunkBytes = 4 * 1024;
	static const int kConnectTimeoutSec = 15;
	static const int kIoTimeoutSec = 30;
	static const int kMaxRedirects = 8;
	// GitHub's 302 from latest/download/ carries a pile of Set-Cookie lines; 4 KB
	// stopped mid-header on hardware. 16 KB is the usual HTTP/1.x ceiling.
	static const unsigned int kMaxHeaderBytes = 16 * 1024;

	static mbedtls_x509_crt sRoots;
	static bool sRootsReady = false;

	static void MbedError(const char* what, int code)
	{
		char name[80];
		mbedtls_strerror(code, name, sizeof(name));
		UpdaterTrace("%s: %s (-0x%04X)", what, name, (unsigned int)(-code));
	}

	// The console clock is usually 2000-01-01 until someone sets it. GitHub's certs then look
	// "not yet valid" and a required-verify handshake dies on the date flags alone. Hostname
	// and signature are still checked; only the calendar is ignored.
	static int SkipClock(void* data, mbedtls_x509_crt* crt, int depth, uint32_t* flags)
	{
		(void)data;
		(void)crt;
		(void)depth;
		if (flags != 0)
		{
			*flags &= ~(MBEDTLS_X509_BADCERT_EXPIRED | MBEDTLS_X509_BADCERT_FUTURE);
		}
		return 0;
	}

	static int PspEntropy(void* ctx, unsigned char* out, size_t len)
	{
		(void)ctx;
		u64 now = sceKernelGetSystemTimeWide();
		unsigned int mix = (unsigned int)now ^ (unsigned int)(now >> 32) ^
			(unsigned int)(unsigned long)&now ^ (unsigned int)sceKernelGetSystemTimeLow();

		for (size_t i = 0; i < len; ++i)
		{
			mix = mix * 1664525u + 1013904223u + (unsigned int)i;
			mix ^= (unsigned int)(sceKernelGetSystemTimeWide() >> (i & 31));
			out[i] = (unsigned char)(mix >> 16);
		}
		return 0;
	}

	static bool EnsureRoots()
	{
		if (sRootsReady)
		{
			return true;
		}

		mbedtls_x509_crt_init(&sRoots);
		int parsed = mbedtls_x509_crt_parse(&sRoots,
			(const unsigned char*)kTrustAnchors, sizeof(kTrustAnchors));
		if (parsed < 0)
		{
			MbedError("trust anchors", parsed);
			mbedtls_x509_crt_free(&sRoots);
			return false;
		}
		if (parsed > 0)
		{
			UpdaterTrace("trust anchors: %d certificate(s) skipped", parsed);
		}

		int count = 0;
		for (mbedtls_x509_crt* c = &sRoots; c != 0; c = c->next)
		{
			if (c->version != 0)
			{
				++count;
			}
		}
		if (count == 0)
		{
			UpdaterTrace("trust anchors: none loaded");
			mbedtls_x509_crt_free(&sRoots);
			return false;
		}

		sRootsReady = true;
		UpdaterTrace("https: %d trust anchor(s), mbedtls TLS 1.2", count);
		return true;
	}

	struct UrlParts
	{
		char host[256];
		char path[1536];
		int port;
	};

	static bool ParseHttpsUrl(const std::string& url, UrlParts* out)
	{
		if (out == 0 || url.size() < 8 || url.compare(0, 8, "https://") != 0)
		{
			return false;
		}

		memset(out, 0, sizeof(*out));
		out->port = 443;

		const char* p = url.c_str() + 8;
		const char* slash = strchr(p, '/');
		const char* colon = strchr(p, ':');
		if (colon != 0 && (slash == 0 || colon < slash))
		{
			size_t hostLen = (size_t)(colon - p);
			if (hostLen == 0 || hostLen >= sizeof(out->host))
			{
				return false;
			}
			memcpy(out->host, p, hostLen);
			out->host[hostLen] = '\0';
			out->port = atoi(colon + 1);
			if (out->port <= 0 || out->port > 65535)
			{
				return false;
			}
			p = (slash != 0) ? slash : "";
		}
		else
		{
			size_t hostLen = (slash != 0) ? (size_t)(slash - p) : strlen(p);
			if (hostLen == 0 || hostLen >= sizeof(out->host))
			{
				return false;
			}
			memcpy(out->host, p, hostLen);
			out->host[hostLen] = '\0';
			p = (slash != 0) ? slash : "/";
		}

		if (p[0] == '\0')
		{
			snprintf(out->path, sizeof(out->path), "/");
		}
		else
		{
			snprintf(out->path, sizeof(out->path), "%s", p);
		}

		char* hash = strchr(out->path, '#');
		if (hash != 0)
		{
			*hash = '\0';
		}
		return out->host[0] != '\0';
	}

	static std::string JoinUrl(const UrlParts& u)
	{
		char buffer[1800];
		if (u.port == 443)
		{
			snprintf(buffer, sizeof(buffer), "https://%s%s", u.host, u.path);
		}
		else
		{
			snprintf(buffer, sizeof(buffer), "https://%s:%d%s", u.host, u.port, u.path);
		}
		return buffer;
	}

	static bool ResolveLocation(const std::string& current, const char* location, std::string* next)
	{
		if (location == 0 || location[0] == '\0' || next == 0)
		{
			return false;
		}

		while (*location == ' ' || *location == '\t')
		{
			++location;
		}

		if (strncmp(location, "https://", 8) == 0)
		{
			*next = location;
			return true;
		}
		if (strncmp(location, "http://", 7) == 0)
		{
			UpdaterTrace("https: refusing http redirect to %s", location);
			return false;
		}
		if (location[0] == '/' && location[1] == '/')
		{
			*next = std::string("https:") + location;
			return true;
		}

		UrlParts cur;
		if (!ParseHttpsUrl(current, &cur))
		{
			return false;
		}

		if (location[0] == '/')
		{
			snprintf(cur.path, sizeof(cur.path), "%s", location);
			*next = JoinUrl(cur);
			return true;
		}

		char* slash = strrchr(cur.path, '/');
		if (slash != 0)
		{
			slash[1] = '\0';
		}
		size_t used = strlen(cur.path);
		snprintf(cur.path + used, sizeof(cur.path) - used, "%s", location);
		*next = JoinUrl(cur);
		return true;
	}

	static int WaitFd(int fd, bool write, int seconds)
	{
		fd_set set;
		FD_ZERO(&set);
		FD_SET(fd, &set);
		struct timeval tv;
		tv.tv_sec = seconds;
		tv.tv_usec = 0;
		int n = select(fd + 1, write ? 0 : &set, write ? &set : 0, 0, &tv);
		return n;
	}

	static int ConnectTcp(const char* host, int port)
	{
		struct in_addr addr;
		memset(&addr, 0, sizeof(addr));

		if (inet_aton(host, &addr) == 0)
		{
			struct hostent* he = gethostbyname((char*)host);
			if (he == 0 || he->h_addr == 0)
			{
				UpdaterTrace("https: dns failed for %s", host);
				return -1;
			}
			memcpy(&addr, he->h_addr, sizeof(addr));
		}

		int fd = socket(PF_INET, SOCK_STREAM, 0);
		if (fd < 0)
		{
			UpdaterTrace("https: socket failed (%d)", errno);
			return -1;
		}

		int nonblock = 1;
		setsockopt(fd, SOL_SOCKET, SO_NONBLOCK, &nonblock, sizeof(nonblock));

		struct sockaddr_in sa;
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_port = htons((unsigned short)port);
		sa.sin_addr = addr;

		int cr = connect(fd, (struct sockaddr*)&sa, sizeof(sa));
		if (cr < 0)
		{
			int ready = WaitFd(fd, true, kConnectTimeoutSec);
			if (ready <= 0)
			{
				UpdaterTrace("https: connect timeout %s:%d", host, port);
				close(fd);
				return -1;
			}
		}

		nonblock = 0;
		setsockopt(fd, SOL_SOCKET, SO_NONBLOCK, &nonblock, sizeof(nonblock));

		struct timeval tv;
		tv.tv_sec = kIoTimeoutSec;
		tv.tv_usec = 0;
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

		char ip[32];
		snprintf(ip, sizeof(ip), "%s", inet_ntoa(addr));
		UpdaterTrace("https: connected %s (%s:%d)", host, ip, port);
		return fd;
	}

	static int BioSend(void* ctx, const unsigned char* buf, size_t len)
	{
		int fd = *(int*)ctx;
		int n = send(fd, buf, (int)len, 0);
		if (n < 0)
		{
			return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
		}
		return n;
	}

	static int BioRecv(void* ctx, unsigned char* buf, size_t len)
	{
		int fd = *(int*)ctx;
		int ready = WaitFd(fd, false, kIoTimeoutSec);
		if (ready == 0)
		{
			return MBEDTLS_ERR_SSL_TIMEOUT;
		}
		if (ready < 0)
		{
			return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
		}

		int n = recv(fd, buf, (int)len, 0);
		if (n < 0)
		{
			return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
		}
		if (n == 0)
		{
			return 0;
		}
		return n;
	}

	struct TlsSession
	{
		int fd;
		mbedtls_ssl_context ssl;
		mbedtls_ssl_config conf;
		mbedtls_ctr_drbg_context drbg;
		unsigned char pending[kChunkBytes];
		int pendingLen;
		int pendingOff;

		TlsSession() : fd(-1), pendingLen(0), pendingOff(0)
		{
			mbedtls_ssl_init(&ssl);
			mbedtls_ssl_config_init(&conf);
			mbedtls_ctr_drbg_init(&drbg);
		}

		~TlsSession()
		{
			if (fd >= 0)
			{
				mbedtls_ssl_close_notify(&ssl);
				close(fd);
				fd = -1;
			}
			mbedtls_ssl_free(&ssl);
			mbedtls_ssl_config_free(&conf);
			mbedtls_ctr_drbg_free(&drbg);
		}
	};

	static bool OpenTls(TlsSession* s, const UrlParts& url)
	{
		if (!EnsureRoots())
		{
			return false;
		}

		const char pers[] = "mc-psp-https";
		int rc = mbedtls_ctr_drbg_seed(&s->drbg, PspEntropy, 0,
			(const unsigned char*)pers, sizeof(pers) - 1);
		if (rc != 0)
		{
			MbedError("ctr_drbg_seed", rc);
			return false;
		}

		rc = mbedtls_ssl_config_defaults(&s->conf, MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
		if (rc != 0)
		{
			MbedError("ssl_config_defaults", rc);
			return false;
		}

		mbedtls_ssl_conf_rng(&s->conf, mbedtls_ctr_drbg_random, &s->drbg);
		mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
		mbedtls_ssl_conf_ca_chain(&s->conf, &sRoots, 0);
		mbedtls_ssl_conf_verify(&s->conf, SkipClock, 0);
		mbedtls_ssl_conf_min_version(&s->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
		mbedtls_ssl_conf_max_version(&s->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);

		rc = mbedtls_ssl_setup(&s->ssl, &s->conf);
		if (rc != 0)
		{
			MbedError("ssl_setup", rc);
			return false;
		}

		rc = mbedtls_ssl_set_hostname(&s->ssl, url.host);
		if (rc != 0)
		{
			MbedError("sni", rc);
			return false;
		}

		s->fd = ConnectTcp(url.host, url.port);
		if (s->fd < 0)
		{
			return false;
		}

		mbedtls_ssl_set_bio(&s->ssl, &s->fd, BioSend, BioRecv, 0);

		UpdaterTrace("https: handshake %s", url.host);
		while ((rc = mbedtls_ssl_handshake(&s->ssl)) != 0)
		{
			if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE)
			{
				MbedError("handshake", rc);
				uint32_t flags = mbedtls_ssl_get_verify_result(&s->ssl);
				if (flags != 0)
				{
					char vbuf[256];
					mbedtls_x509_crt_verify_info(vbuf, sizeof(vbuf), "", flags);
					UpdaterTrace("https: verify %s", vbuf);
				}
				return false;
			}
		}

		UpdaterTrace("https: tls %s %s", mbedtls_ssl_get_version(&s->ssl),
			mbedtls_ssl_get_ciphersuite(&s->ssl));
		return true;
	}

	static int SslRead(TlsSession* s, unsigned char* buf, int len)
	{
		if (s->pendingOff < s->pendingLen)
		{
			int left = s->pendingLen - s->pendingOff;
			int n = (len < left) ? len : left;
			memcpy(buf, s->pending + s->pendingOff, (size_t)n);
			s->pendingOff += n;
			if (s->pendingOff >= s->pendingLen)
			{
				s->pendingOff = 0;
				s->pendingLen = 0;
			}
			return n;
		}

		int n = mbedtls_ssl_read(&s->ssl, buf, (size_t)len);
		if (n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || n == 0)
		{
			return 0;
		}
		return n;
	}

	static bool SslWriteAll(TlsSession* s, const unsigned char* buf, int len)
	{
		int sent = 0;
		while (sent < len)
		{
			int n = mbedtls_ssl_write(&s->ssl, buf + sent, (size_t)(len - sent));
			if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE)
			{
				continue;
			}
			if (n <= 0)
			{
				MbedError("ssl_write", n);
				return false;
			}
			sent += n;
		}
		return true;
	}

	static bool ReadHeaders(TlsSession* s, std::string* headers)
	{
		headers->clear();
		headers->reserve(1024);

		while (headers->size() < kMaxHeaderBytes)
		{
			unsigned char tmp[kChunkBytes];
			int n = SslRead(s, tmp, (int)sizeof(tmp));
			if (n < 0)
			{
				MbedError("header read", n);
				return false;
			}
			if (n == 0)
			{
				UpdaterTrace("https: connection closed in headers (%u bytes)",
					(unsigned int)headers->size());
				return false;
			}

			headers->append((char*)tmp, (size_t)n);
			size_t end = headers->find("\r\n\r\n");
			if (end != std::string::npos)
			{
				size_t extraOff = end + 4;
				if (extraOff < headers->size())
				{
					size_t extra = headers->size() - extraOff;
					if (extra > sizeof(s->pending))
					{
						UpdaterTrace("https: leftover body %u larger than pending",
							(unsigned int)extra);
						return false;
					}
					memcpy(s->pending, headers->data() + extraOff, extra);
					s->pendingLen = (int)extra;
					s->pendingOff = 0;
				}
				headers->resize(extraOff);
				UpdaterTrace("https: headers %u bytes", (unsigned int)headers->size());
				return true;
			}
		}

		UpdaterTrace("https: headers exceeded %u bytes (got %u, no end)",
			kMaxHeaderBytes, (unsigned int)headers->size());
		return false;
	}

	static const char* FindHeader(const std::string& headers, const char* name)
	{
		size_t nameLen = strlen(name);
		size_t i = 0;
		while (i < headers.size())
		{
			size_t line = headers.find("\r\n", i);
			if (line == std::string::npos)
			{
				break;
			}
			if (line - i >= nameLen + 1)
			{
				bool match = true;
				for (size_t n = 0; n < nameLen; ++n)
				{
					char a = headers[i + n];
					char b = name[n];
					if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
					if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
					if (a != b)
					{
						match = false;
						break;
					}
				}
				if (match && headers[i + nameLen] == ':')
				{
					const char* v = headers.c_str() + i + nameLen + 1;
					while (*v == ' ' || *v == '\t')
					{
						++v;
					}
					return v;
				}
			}
			i = line + 2;
		}
		return 0;
	}

	static int ParseStatus(const std::string& headers)
	{
		if (headers.compare(0, 5, "HTTP/") != 0)
		{
			return 0;
		}
		const char* p = headers.c_str();
		while (*p != '\0' && *p != ' ')
		{
			++p;
		}
		while (*p == ' ')
		{
			++p;
		}
		return atoi(p);
	}

	static unsigned int ParseLength(const char* value)
	{
		if (value == 0)
		{
			return 0;
		}
		return (unsigned int)strtoul(value, 0, 10);
	}

	static bool HeaderIsChunked(const std::string& headers)
	{
		const char* te = FindHeader(headers, "transfer-encoding");
		if (te == 0)
		{
			return false;
		}
		char buf[32];
		snprintf(buf, sizeof(buf), "%s", te);
		for (char* p = buf; *p; ++p)
		{
			if (*p >= 'A' && *p <= 'Z')
			{
				*p = (char)(*p - 'A' + 'a');
			}
			if (*p == '\r' || *p == '\n')
			{
				*p = '\0';
				break;
			}
		}
		return strstr(buf, "chunked") != 0;
	}

	static bool CopyOut(const char* data, int len, std::string* toMemory, SceUID* toFile)
	{
		if (toMemory != 0)
		{
			toMemory->append(data, (size_t)len);
			return true;
		}

		int written = 0;
		while (written < len)
		{
			int step = sceIoWrite(*toFile, data + written, len - written);
			if (step <= 0)
			{
				UpdaterTrace("sceIoWrite failed: 0x%08X", (unsigned int)step);
				return false;
			}
			written += step;
		}
		return true;
	}

	static bool PumpBody(TlsSession* s, unsigned int remaining, bool knownLength,
						 std::string* toMemory, SceUID* toFile,
						 ProgressFn progress, void* ctx, unsigned int memoryCap,
						 unsigned int* done)
	{
		static char buffer[kChunkBytes];

		while (!knownLength || remaining > 0)
		{
			int want = (int)kChunkBytes;
			if (knownLength && remaining < (unsigned int)want)
			{
				want = (int)remaining;
			}

			int n = SslRead(s, (unsigned char*)buffer, want);
			if (n < 0)
			{
				MbedError("body read", n);
				return false;
			}
			if (n == 0)
			{
				return !knownLength || remaining == 0;
			}

			if (toMemory != 0 && *done + (unsigned int)n > memoryCap)
			{
				UpdaterTrace("in-memory resource exceeded %u bytes", memoryCap);
				return false;
			}
			if (!CopyOut(buffer, n, toMemory, toFile))
			{
				return false;
			}

			*done += (unsigned int)n;
			if (knownLength)
			{
				remaining -= (unsigned int)n;
			}

			if (progress != 0 && !progress(ctx, *done, knownLength ? (*done + remaining) : 0))
			{
				UpdaterTrace("transfer cancelled by the player at %u", *done);
				return false;
			}
		}
		return true;
	}

	static bool ReadChunked(TlsSession* s, std::string* toMemory, SceUID* toFile,
							ProgressFn progress, void* ctx, unsigned int memoryCap,
							unsigned int* done)
	{
		static char buffer[kChunkBytes];

		while (true)
		{
			char line[32];
			int lineLen = 0;
			while (lineLen < (int)sizeof(line) - 1)
			{
				unsigned char c = 0;
				int n = SslRead(s, &c, 1);
				if (n <= 0)
				{
					UpdaterTrace("https: truncated chunk size");
					return false;
				}
				line[lineLen++] = (char)c;
				if (lineLen >= 2 && line[lineLen - 2] == '\r' && line[lineLen - 1] == '\n')
				{
					break;
				}
			}
			line[lineLen] = '\0';

			char* end = 0;
			unsigned long chunk = strtoul(line, &end, 16);
			if (chunk == 0)
			{
				return true;
			}

			unsigned long left = chunk;
			while (left > 0)
			{
				int want = (int)kChunkBytes;
				if (left < (unsigned long)want)
				{
					want = (int)left;
				}
				int n = SslRead(s, (unsigned char*)buffer, want);
				if (n <= 0)
				{
					UpdaterTrace("https: truncated chunk body");
					return false;
				}
				if (toMemory != 0 && *done + (unsigned int)n > memoryCap)
				{
					UpdaterTrace("in-memory resource exceeded %u bytes", memoryCap);
					return false;
				}
				if (!CopyOut(buffer, n, toMemory, toFile))
				{
					return false;
				}
				*done += (unsigned int)n;
				left -= (unsigned long)n;
				if (progress != 0 && !progress(ctx, *done, 0))
				{
					UpdaterTrace("transfer cancelled by the player at %u", *done);
					return false;
				}
			}

			unsigned char crlf[2];
			if (SslRead(s, crlf, 1) <= 0 || SslRead(s, crlf + 1, 1) <= 0)
			{
				UpdaterTrace("https: truncated chunk trailer");
				return false;
			}
		}
	}

	static bool OneGet(const std::string& url, std::string* toMemory, SceUID* toFile,
					   ProgressFn progress, void* ctx, unsigned int* outSize,
					   long rangeStart, long rangeEnd, unsigned int memoryCap,
					   std::string* redirectTo)
	{
		UrlParts parts;
		if (!ParseHttpsUrl(url, &parts))
		{
			UpdaterTrace("https: bad url '%s'", url.c_str());
			return false;
		}

		TlsSession session;
		if (!OpenTls(&session, parts))
		{
			return false;
		}

		char hostHeader[280];
		if (parts.port == 443)
		{
			snprintf(hostHeader, sizeof(hostHeader), "%s", parts.host);
		}
		else
		{
			snprintf(hostHeader, sizeof(hostHeader), "%s:%d", parts.host, parts.port);
		}

		char request[2048];
		int reqLen;
		if (rangeStart >= 0)
		{
			if (rangeEnd >= 0)
			{
				reqLen = snprintf(request, sizeof(request),
					"GET %s HTTP/1.0\r\n"
					"Host: %s\r\n"
					"User-Agent: MineCraft-PSP/1.0\r\n"
					"Accept: */*\r\n"
					"Connection: close\r\n"
					"Range: bytes=%ld-%ld\r\n"
					"\r\n",
					parts.path, hostHeader, rangeStart, rangeEnd);
			}
			else
			{
				reqLen = snprintf(request, sizeof(request),
					"GET %s HTTP/1.0\r\n"
					"Host: %s\r\n"
					"User-Agent: MineCraft-PSP/1.0\r\n"
					"Accept: */*\r\n"
					"Connection: close\r\n"
					"Range: bytes=%ld-\r\n"
					"\r\n",
					parts.path, hostHeader, rangeStart);
			}
		}
		else
		{
			reqLen = snprintf(request, sizeof(request),
				"GET %s HTTP/1.0\r\n"
				"Host: %s\r\n"
				"User-Agent: MineCraft-PSP/1.0\r\n"
				"Accept: */*\r\n"
				"Connection: close\r\n"
				"\r\n",
				parts.path, hostHeader);
		}

		if (reqLen <= 0 || reqLen >= (int)sizeof(request))
		{
			UpdaterTrace("https: request too long");
			return false;
		}

		if (!SslWriteAll(&session, (const unsigned char*)request, reqLen))
		{
			return false;
		}

		std::string headers;
		if (!ReadHeaders(&session, &headers))
		{
			return false;
		}

		int status = ParseStatus(headers);
		UpdaterTrace("GET %s -> HTTP %d", url.c_str(), status);

		if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
		{
			const char* loc = FindHeader(headers, "location");
			if (loc == 0)
			{
				UpdaterTrace("https: redirect with no Location");
				return false;
			}
			char locBuf[1536];
			snprintf(locBuf, sizeof(locBuf), "%s", loc);
			char* cut = strpbrk(locBuf, "\r\n");
			if (cut != 0)
			{
				*cut = '\0';
			}
			if (!ResolveLocation(url, locBuf, redirectTo))
			{
				return false;
			}
			UpdaterTrace("https: redirect -> %s", redirectTo->c_str());
			return true;
		}

		const bool wantsRange = (rangeStart >= 0);
		if (wantsRange)
		{
			if (status == 200)
			{
				UpdaterTrace("range: server IGNORED Range and sent the whole resource -- refusing it");
				return false;
			}
			if (status != 206)
			{
				UpdaterTrace("GET %s (range) -> HTTP %d, wanted 206", url.c_str(), status);
				return false;
			}
		}
		else if (status != 200)
		{
			return false;
		}

		unsigned int contentLength = ParseLength(FindHeader(headers, "content-length"));
		if (toMemory != 0 && contentLength > memoryCap)
		{
			UpdaterTrace("refusing a %u byte in-memory resource (cap %u)", contentLength, memoryCap);
			return false;
		}

		unsigned int done = 0;
		bool ok;
		if (HeaderIsChunked(headers))
		{
			ok = ReadChunked(&session, toMemory, toFile, progress, ctx, memoryCap, &done);
		}
		else
		{
			ok = PumpBody(&session, contentLength, contentLength != 0,
				toMemory, toFile, progress, ctx, memoryCap, &done);
		}

		if (!ok)
		{
			return false;
		}
		if (contentLength != 0 && done != contentLength)
		{
			UpdaterTrace("short transfer: got %u of %u bytes", done, contentLength);
			return false;
		}

		if (outSize != 0)
		{
			*outSize = done;
		}
		return true;
	}

	bool HttpsGet(const std::string& url, std::string* toMemory, SceUID* toFile,
				  ProgressFn progress, void* ctx, unsigned int* outSize,
				  long rangeStart, long rangeEnd, unsigned int maxMemoryBytes)
	{
		if (outSize != 0)
		{
			*outSize = 0;
		}

		std::string current = url;
		for (int hop = 0; hop <= kMaxRedirects; ++hop)
		{
			if (toMemory != 0)
			{
				toMemory->clear();
			}

			std::string redirect;
			if (!OneGet(current, toMemory, toFile, progress, ctx, outSize,
					rangeStart, rangeEnd, maxMemoryBytes, &redirect))
			{
				return false;
			}
			if (redirect.empty())
			{
				return true;
			}
			current = redirect;
		}

		UpdaterTrace("https: too many redirects");
		return false;
	}
}
