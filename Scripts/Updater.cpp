#include "Updater.h"
#include "HttpsClient.h"
#include "Version.h"

#include <Aurora/Utils/Logger.h>
#include <Aurora/Utils/pgeZip.h>

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psputility.h>
#include <psputility_netconf.h>
#include <psputility_netmodules.h>
#include <psputility_netparam.h>
#include <psputility_sysparam.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <psphttp.h>
#include <pspssl.h>

// The handoff (see the long note in Updater.h). pspinit.h for the apitype, ploadexec_kernel.h for the
// parameter struct the CFW call takes, and systemctrl.h for the call itself. systemctrl.h is the
// kernel header and it is included on purpose -- the *user-mode* stub is what gets linked
// (-lpspsystemctrl_user, module SystemCtrlForUser), and that is exactly how the CFW means for this to
// be called from a game.
#include <pspinit.h>
#include <psploadexec_kernel.h>
#include <systemctrl.h>

#include <zlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>

namespace Update
{
	// How much of a "small" resource we are willing to hold in RAM. update.txt is six lines; the cap
	// exists so a server that answers with an error page, a directory listing or the wrong file turns
	// into a clean failure instead of an allocation a PSP cannot satisfy.
	static const unsigned int kMaxInfoBytes = 16 * 1024;

	// Bytes per sceHttpReadData / sceIoWrite. 4 KB rather than pgeNetGetFile's 8 KB: this buffer lives
	// on a 512 KB thread stack, and it is also the granularity at which progress is reported and the
	// transfer can be cancelled.
	static const unsigned int kChunkBytes = 4 * 1024;

	// Timeouts, in microseconds. pgeNetGetFile sets resolve/recv/send but *not* connect, so a TCP
	// connect to a host that is switched off can sit there far longer than any of the others -- which
	// on a handheld looks exactly like a crash and can only be escaped with the power switch. All four
	// are set here.
	static const unsigned int kResolveTimeoutUs = 5 * 1000 * 1000;
	static const unsigned int kConnectTimeoutUs = 15 * 1000 * 1000;
	static const unsigned int kRecvTimeoutUs = 30 * 1000 * 1000;
	static const unsigned int kSendTimeoutUs = 30 * 1000 * 1000;

	// Set by NetInit so NetShutdown can be called unconditionally (the caller should not have to
	// remember whether it got that far).
	static bool sNetUp = false;

	// True only when *this* module actually brought sceNet up, i.e. sceNetInit succeeded. The game's
	// main menu reaches the same init through Aurora::System::NetworkManager::InitializeConnection,
	// so a second call from here fails with "already initialised" -- and in that case the stack is not
	// ours to tear down on the way out.
	static bool sNetOwner = false;

	/*
	 * Which net modules THIS program loaded, one bit each.
	 *
	 * A load that reports ALREADY_LOADED (0x80110802) means somebody else put that module there -- the
	 * XMB's own network setup, or the game's NetworkManager -- and it is not ours to unload. That
	 * matters because unloading a module another part of the system is still using is not a no-op: it
	 * takes the code and data out from under it. The launcher is routinely started while a connection
	 * made in the XMB's settings is live, and in that case every load reports ALREADY_LOADED.
	 *
	 * Set on success and never cleared except by NetShutdown, so the bits accumulate across the
	 * repeated NetInit calls this program makes (boot connect, then the update flow): the second call's
	 * ALREADY_LOADED reports leave the first call's bits alone, which is what keeps a module we did
	 * load from being left behind.
	 */
	static unsigned int sModulesLoaded = 0;
	static const unsigned int kLoadedCommon = 1u << 0;
	static const unsigned int kLoadedInet = 1u << 1;
	static const unsigned int kLoadedParseUri = 1u << 2;
	static const unsigned int kLoadedParseHttp = 1u << 3;
	static const unsigned int kLoadedHttp = 1u << 4;
	static const unsigned int kLoadedSsl = 1u << 5;

	// sceSslInit / sceHttpsInit ran, and must be paired with sceHttpsEnd / sceSslEnd. Separate from
	// the module bit: a load that reported ALREADY_LOADED still leaves us owning the SSL heap if we
	// were the ones who called sceSslInit.
	static bool sSslUp = false;
	static void ShutdownHttps();

	// The kernel error codes this module can actually provoke, named so the log does not need a header
	// lookup -- the one this console hits, 0x80020190, is SCE_KERNEL_ERROR_NO_MEMORY and its meaning is
	// the opposite of what a bare number invites you to guess. Not a general decoder: a code not in
	// this list prints as a number, because inventing a meaning for it would be worse than saying
	// nothing.
	static const char* KernelErrorName(unsigned int code)
	{
		switch(code)
		{
			case 0x80020190u: return "SCE_KERNEL_ERROR_NO_MEMORY";
			case 0x80020191u: return "SCE_KERNEL_ERROR_ILLEGAL_ATTR";
			case 0x80020192u: return "SCE_KERNEL_ERROR_ILLEGAL_ENTRY";
			case 0x80020193u: return "SCE_KERNEL_ERROR_ILLEGAL_PRIORITY";
			case 0x80020194u: return "SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE";
			case 0x80020195u: return "SCE_KERNEL_ERROR_ILLEGAL_MODE";
			case 0x80020196u: return "SCE_KERNEL_ERROR_ILLEGAL_MASK";
			default:          return "unrecognised code";
		}
	}

	// The sceHttp error space, for the same reason KernelErrorName exists: a bare number in the log
	// invites a guess, and the guess is usually wrong. 0x80431068 in particular reads like a protocol
	// complaint and is actually SCE_HTTP_ERROR_TIMEOUT ("could not connect to the server in time"),
	// i.e. a network problem -- which is the difference between looking at the firewall and looking at
	// the request. Names are the console's own, from the sceHttp error list; codes outside this table
	// are reported as numbers, not guessed at.
	static const char* HttpErrorName(unsigned int code)
	{
		switch(code)
		{
			case 0x80431001u: return "SCE_HTTP_ERROR_BEFORE_INIT";
			case 0x80431020u: return "SCE_HTTP_ERROR_ALREADY_INITED";
			case 0x80431021u: return "SCE_HTTP_ERROR_BUSY";
			case 0x80431022u: return "SCE_HTTP_ERROR_OUT_OF_MEMORY";
			case 0x80431025u: return "SCE_HTTP_ERROR_NOT_FOUND";
			case 0x80431061u: return "SCE_HTTP_ERROR_UNKNOWN_SCHEME";
			case 0x80431063u: return "SCE_HTTP_ERROR_NETWORK";
			case 0x80431064u: return "SCE_HTTP_ERROR_BAD_RESPONSE";
			case 0x80431065u: return "SCE_HTTP_ERROR_BEFORE_SEND";
			case 0x80431066u: return "SCE_HTTP_ERROR_AFTER_SEND";
			case 0x80431068u: return "SCE_HTTP_ERROR_TIMEOUT";
			case 0x8043106Au: return "SCE_HTTP_ERROR_INVALID_VERSION";
			case 0x8043106Bu: return "SCE_HTTP_ERROR_UNKNOWN_METHOD";
			case 0x80431071u: return "SCE_HTTP_ERROR_NO_CONTENT_LENGTH";
			case 0x80431072u: return "SCE_HTTP_ERROR_CHUNK_ENC";
			case 0x80431073u: return "SCE_HTTP_ERROR_TOO_LARGE_RESPONSE_HEADER";
			case 0x80431075u: return "SCE_HTTP_ERROR_SSL";
			case 0x80431080u: return "SCE_HTTP_ERROR_ABORTED";
			case 0x80431081u: return "SCE_HTTP_ERROR_UNKNOWN";
			case 0x80431100u: return "SCE_HTTP_ERROR_INVALID_ID";
			case 0x80431104u: return "SCE_HTTP_ERROR_OUT_OF_SIZE";
			case 0x80433060u: return "SCE_HTTP_ERROR_INVALID_URL";
			case 0x80436002u: return "SCE_HTTP_ERROR_RESOLVER_ENODNS";
			case 0x80436003u: return "SCE_HTTP_ERROR_RESOLVER_ETIMEDOUT";
			case 0x80436007u: return "SCE_HTTP_ERROR_RESOLVER_ENOHOST";
			default:          return "unrecognised code";
		}
	}

	// ---------------------------------------------------------------------------------------
	// Logging helper. Every failure in here is invisible on a handheld -- there is no console and the
	// interesting one happens before the game is even drawn -- so each step that can fail names
	// itself in Log.txt. That is what makes a "the update just does nothing" report actionable.
	// ---------------------------------------------------------------------------------------
	static char sLastHttpError[256];

	const char* LastHttpError()
	{
		return sLastHttpError;
	}

	void UpdaterTrace(const char* fmt, ...)
	{
		char buffer[512];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
		snprintf(sLastHttpError, sizeof(sLastHttpError), "%s", buffer);
		Aurora::Utils::Logger::Instance()->LogMessage("updater: %s\n", buffer);
	}

	static void Trace(const char* fmt, ...)
	{
		char buffer[512];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
		UpdaterTrace("%s", buffer);
	}

	// pgeZip's own diagnostics, into this module's log. See the declaration in Updater.h for why this
	// exists; the short version is that pgeZip prints to stdout on its error paths and this program has
	// no stdout, so a message that named the failing file was being discarded.
	//
	// C linkage because the caller is pgeZip.c. The formatting happens here so the message arrives as one
	// line in the same shape as every other entry -- pgeZip's own strings end with a newline, and Trace
	// adds one, so a trailing newline is trimmed rather than left to produce a blank line.
	extern "C" int UpdateZipLog(const char* format, ...)
	{
		char buffer[256];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);

		size_t length = strlen(buffer);
		while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r'))
		{
			buffer[--length] = '\0';
		}

		Trace("%s", buffer);
		return 0;
	}

	// ---------------------------------------------------------------------------------------
	// Line protocol. Pipe separated, one record per line, CRLF tolerated because a package built on
	// Windows and served by a Windows script will have them.
	// ---------------------------------------------------------------------------------------

	// Splits on '|'. Empty fields are kept, because a record with a missing field must be rejected as
	// malformed rather than silently shifting the ones after it into the wrong slots.
	static void SplitFields(const std::string& line, std::vector<std::string>* out)
	{
		out->clear();
		std::string current;
		for (size_t i = 0; i < line.size(); ++i)
		{
			if (line[i] == '|')
			{
				out->push_back(current);
				current.clear();
			}
			else
			{
				current += line[i];
			}
		}
		out->push_back(current);
	}

	// One record per line, split into (key, fields...). Returns false on a line that is not a record
	// (blank lines are skipped rather than rejected -- a trailing newline is normal).
	static bool NextRecord(const std::string& text, size_t* pos, std::string* key, std::vector<std::string>* fields)
	{
		while (*pos < text.size())
		{
			size_t end = text.find('\n', *pos);
			if (end == std::string::npos)
			{
				end = text.size();
			}

			std::string line = text.substr(*pos, end - *pos);
			*pos = end + 1;

			// Trim a trailing CR and any surrounding blanks.
			while (!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == ' ' || line[line.size() - 1] == '\t'))
			{
				line.erase(line.size() - 1);
			}
			size_t start = 0;
			while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
			{
				++start;
			}
			if (start > 0)
			{
				line.erase(0, start);
			}

			if (line.empty())
			{
				continue;
			}

			SplitFields(line, fields);
			*key = (*fields)[0];
			fields->erase(fields->begin());
			return true;
		}
		return false;
	}

	// A number from the wire, with no locale-dependent parse and no exceptions to throw. Anything that
	// is not a clean run of digits is a malformed file, which the caller reports by name.
	static bool ParseUInt(const std::string& s, unsigned int* out)
	{
		if (s.empty())
		{
			return false;
		}
		unsigned int value = 0;
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] < '0' || s[i] > '9')
			{
				return false;
			}
			value = value * 10 + (unsigned int)(s[i] - '0');
		}
		*out = value;
		return true;
	}

	// Whether ParseOffer reads this key, and therefore whether it has to be single-valued.
	//
	// The distinction matters because of the guarantee the unknown-key branch makes: a key this build
	// does not know is IGNORED, so the server can gain a field without every older build refusing to
	// update. Checking field count before that branch silently broke the guarantee for any new field
	// carrying more than one value -- `patch|file|size|crc|base` is exactly such a field, and an older
	// build met it with "The server's reply did not make sense" and refused the whole offer rather than
	// skipping the one line it did not understand.
	static bool IsSingleValuedKey(const std::string& key)
	{
		return key == "version" || key == "code" || key == "file" || key == "size" ||
			   key == "crc" || key == "force" || key == "full" || key == "base" ||
			   key == "target";
	}

	bool ParseOffer(const std::string& text, Offer* out)
	{
		if (out == 0)
		{
			return false;
		}

		bool haveVersion = false, haveCode = false, haveFile = false, haveSize = false, haveCrc = false;

		// Defaulted here as well as in the struct, so a parse that fails part way cannot leave a caller
		// holding a force flag that came from nowhere.
		out->force = false;

		// Same reasoning, and the defaults are the safe reading: "not full" makes the launcher treat an
		// offer as a patch until the package proves otherwise, and an empty base means "nobody said",
		// which every check below treats as "cannot verify" rather than "matches".
		out->full = false;
		out->base.clear();
		out->patches.clear();

		// Defaulted here as everywhere else: an offer that does not mention `target` is a game install,
		// which is what every server written before this knew how to produce.
		out->launcher = false;

		size_t pos = 0;
		std::string key;
		std::vector<std::string> fields;

		while (NextRecord(text, &pos, &key, &fields))
		{
			// `patch` is the one record that carries more than one value, so it is handled before the
			// single-value rule below rather than exempted inside it. Four fields rather than three
			// because the base version is what makes an entry selectable: a patch that did not say what
			// it applies to could not be offered at all, only downloaded and then refused.
			if (key == "patch")
			{
				if (fields.size() != 4)
				{
					Trace("update.txt: patch wants file|size|crc|base");
					return false;
				}

				PatchOffer patch;
				patch.file = fields[0];
				if (!ParseUInt(fields[1], &patch.size) || !ParseUInt(fields[2], &patch.crc))
				{
					Trace("update.txt: patch size or crc is not a number: '%s' '%s'",
						fields[1].c_str(), fields[2].c_str());
					return false;
				}
				patch.base = fields[3];

				// The same guard the primary package's filename gets, applied here at parse time rather
				// than at use, so a bad line is refused by the parser instead of turning into a download
				// to an unexpected place. The base is checked for the same reason the primary's is not:
				// it is compared against a folder name, and a value with a separator in it is a server
				// bug that should not be carried any further.
				if (patch.file.empty() || patch.base.empty() ||
					patch.file.find('/') != std::string::npos ||
					patch.file.find('\\') != std::string::npos ||
					patch.base.find('/') != std::string::npos ||
					patch.base.find('\\') != std::string::npos)
				{
					Trace("update.txt: unusable patch entry: file='%s' base='%s'",
						patch.file.c_str(), patch.base.c_str());
					return false;
				}

				out->patches.push_back(patch);
				continue;
			}

			if (!IsSingleValuedKey(key))
			{
				// An unknown key is ignored rather than fatal: it is how the server can gain a field
				// (say, a signature) without every older build refusing to update, which would be a
				// self-inflicted one-way door. Checked BEFORE the arity rule below, so a future field
				// may carry as many values as it likes and this build will still skip it cleanly.
				Trace("update.txt: ignoring unknown key '%s'", key.c_str());
				continue;
			}

			if (fields.size() != 1)
			{
				Trace("update.txt: '%s' wants exactly one value", key.c_str());
				return false;
			}

			if (key == "version")
			{
				out->version = fields[0];
				haveVersion = true;
			}
			else if (key == "code")
			{
				unsigned int code = 0;
				if (!ParseUInt(fields[0], &code))
				{
					Trace("update.txt: code is not a number: '%s'", fields[0].c_str());
					return false;
				}
				out->versionCode = (int)code;
				haveCode = true;
			}
			else if (key == "file")
			{
				out->file = fields[0];
				haveFile = true;
			}
			else if (key == "size")
			{
				if (!ParseUInt(fields[0], &out->size))
				{
					Trace("update.txt: size is not a number: '%s'", fields[0].c_str());
					return false;
				}
				haveSize = true;
			}
			else if (key == "crc")
			{
				if (!ParseUInt(fields[0], &out->crc))
				{
					Trace("update.txt: crc is not a number: '%s'", fields[0].c_str());
					return false;
				}
				haveCrc = true;
			}
			else if (key == "force")
			{
				// Accepts any non-zero value, and only "0" turns it off, so writing `force|1` is the
				// obvious thing and no one has to remember a keyword. A missing line leaves it false,
				// which is what keeps this from being able to affect a normal release.
				out->force = (fields[0] != "0" && fields[0].empty() == false);
			}
			else if (key == "full")
			{
				// Same convention as force, and absent means patch. See the note on Offer::full for why
				// that is the safe direction rather than an arbitrary one.
				out->full = (fields[0] != "0" && fields[0].empty() == false);
			}
			else if (key == "base")
			{
				// The version a patch is diffed against. Trimmed of anything path-like for the same
				// reason the package name is below: it is compared against a folder name, not used as a
				// path, but a value with a separator in it is a server bug worth not propagating.
				if (fields[0].find('/') == std::string::npos && fields[0].find('\\') == std::string::npos)
				{
					out->base = fields[0];
				}
			}
			else if (key == "target")
			{
				// Which program this package is for. Only one value is meaningful and it is a word, not
				// a number, so it is matched rather than parsed: "launcher" switches the offer to the
				// launcher's self-update, and anything else -- including the "game" a server may write
				// for clarity -- leaves it an ordinary game install, which is also what an offer that
				// omits the line entirely means.
				out->launcher = (fields[0] == "launcher");
			}
		}

		if (!haveVersion || !haveCode || !haveFile || !haveSize || !haveCrc)
		{
			Trace("update.txt is missing required fields (version=%d code=%d file=%d size=%d crc=%d)",
				(int)haveVersion, (int)haveCode, (int)haveFile, (int)haveSize, (int)haveCrc);
			return false;
		}

		// A filename from the network is used to build a local path, so it must not be able to point
		// out of the game folder. Rejecting '/' is heavy handed -- a package legitimately lives beside
		// the game, not inside it -- so the rule is the one that matters: no separators at all, which
		// leaves a plain name like "MineCraft-3.0.2.zip" and nothing else.
		if (out->file.find('/') != std::string::npos || out->file.find('\\') != std::string::npos)
		{
			Trace("update.txt: refusing a path in the filename: '%s'", out->file.c_str());
			return false;
		}

		return true;
	}

	Package ChoosePackage(const Offer& offer, const std::string& installedVersion, bool allowPatch)
	{
		Package chosen;
		chosen.file = offer.file;
		chosen.size = offer.size;
		chosen.crc = offer.crc;
		chosen.full = offer.full;
		chosen.base = offer.base;

		if (!allowPatch || installedVersion.empty())
		{
			return chosen;
		}

		// First match wins. Two entries for one base would be a server bug, and taking the first keeps
		// the choice deterministic rather than dependent on which happened to be written last.
		for (unsigned int i = 0; i < offer.patches.size(); ++i)
		{
			if (offer.patches[i].base == installedVersion)
			{
				chosen.file = offer.patches[i].file;
				chosen.size = offer.patches[i].size;
				chosen.crc = offer.patches[i].crc;
				chosen.full = false;
				chosen.base = offer.patches[i].base;
				return chosen;
			}
		}

		return chosen;
	}

	bool ParseManifest(const std::string& text, Manifest* out)
	{
		if (out == 0)
		{
			return false;
		}

		unsigned int fileCount = 0;

		// Defaulted before the loop, so a parse that fails part way cannot leave a caller holding a
		// force flag that came from nowhere.
		out->force = false;
		// Same reasoning as force: an older package does not carry this line, and "not full" is the
		// safe reading of that -- the launcher then treats it as a patch and copies a base install
		// before applying it, which works whether or not it needed to.
		out->full = false;
		out->base.clear();

		size_t pos = 0;
		std::string key;
		std::vector<std::string> fields;

		while (NextRecord(text, &pos, &key, &fields))
		{
			if (key == "version")
			{
				if (fields.size() != 1)
				{
					Trace("manifest: version wants one value");
					return false;
				}
				out->version = fields[0];
			}
			else if (key == "code")
			{
				if (fields.size() != 1)
				{
					Trace("manifest: code wants one value");
					return false;
				}
				unsigned int code = 0;
				if (!ParseUInt(fields[0], &code))
				{
					Trace("manifest: code is not a number: '%s'", fields[0].c_str());
					return false;
				}
				out->versionCode = (int)code;
			}
			else if (key == "file")
			{
				// file|<path>|<size>|<crc>
				if (fields.size() != 3)
				{
					Trace("manifest: a file record needs path, size and crc (got %d fields)", (int)fields.size());
					return false;
				}

				FileEntry entry;
				entry.path = fields[0];

				if (entry.path.empty())
				{
					Trace("manifest: a file record has an empty path");
					return false;
				}
				// These paths are written into the game folder, so an absolute path or a climb out of
				// the tree ("../../..") is refused. This is a local-network tool, not a hostile one,
				// but a bug in the packaging script must not be able to scatter files around the
				// memory stick, and the check is three lines.
				if (entry.path[0] == '/' || entry.path.find("..") != std::string::npos)
				{
					Trace("manifest: refusing a path that escapes the game folder: '%s'", entry.path.c_str());
					return false;
				}

				if (!ParseUInt(fields[1], &entry.size) || !ParseUInt(fields[2], &entry.crc))
				{
					Trace("manifest: size/crc are not numbers for '%s'", entry.path.c_str());
					return false;
				}

				out->files.push_back(entry);
				++fileCount;
			}
			else if (key == "delete")
			{
				if (fields.size() != 1)
				{
					Trace("manifest: delete wants one value");
					return false;
				}
				if (fields[0].empty() || fields[0][0] == '/' || fields[0].find("..") != std::string::npos)
				{
					Trace("manifest: refusing to delete a path outside the game folder: '%s'", fields[0].c_str());
					return false;
				}
				out->deletes.push_back(fields[0]);
			}
			else if (key == "force")
			{
				// Same meaning as in the offer, and carried here as well so the package stays a
				// self-describing object: the Updater EBOOT reads the manifest with no server to ask, and
				// a package that reinstalls the version already on the card needs to say so itself.
				if (fields.size() != 1)
				{
					Trace("manifest: force wants one value");
					return false;
				}
				out->force = (fields[0] != "0" && fields[0].empty() == false);
			}
			else if (key == "full")
			{
				// The package's own statement that it carries every file, so it needs nothing under it.
				if (fields.size() != 1)
				{
					Trace("manifest: full wants one value");
					return false;
				}
				out->full = (fields[0] != "0" && fields[0].empty() == false);
			}
			else if (key == "base")
			{
				// The version this package was diffed against, so a patch can be checked against what is
				// actually installed before it is applied on top of it.
				if (fields.size() != 1)
				{
					Trace("manifest: base wants one value");
					return false;
				}
				out->base = fields[0];
			}
			else if (key == "full")
			{
				// See the note on Manifest::full. Written by the packaging script when it is run
				// without --base, i.e. when the package carries every file and can be extracted into
				// an empty folder instead of being applied to a copy of an existing install.
				if (fields.size() != 1)
				{
					Trace("manifest: full wants one value");
					return false;
				}
				out->full = (fields[0] != "0" && fields[0].empty() == false);
			}
			else
			{
				Trace("manifest: ignoring unknown key '%s'", key.c_str());
			}
		}

		// A manifest with no files is not a valid patch: installing it would "succeed" while changing
		// nothing, and the game would then believe it was up to date. That is the worst possible
		// outcome of an updater, so it is refused here rather than discovered later.
		if (fileCount == 0)
		{
			Trace("manifest lists no files -- refusing it");
			return false;
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------
	// File helpers
	// ---------------------------------------------------------------------------------------

	bool GameDirectory(std::string* out)
	{
		if (out == 0)
		{
			return false;
		}
		char buffer[256];
		if (getcwd(buffer, sizeof(buffer)) == 0)
		{
			Trace("getcwd failed");
			return false;
		}
		*out = buffer;
		return true;
	}

	// ---------------------------------------------------------------------------------------
	// The handoff. See the long note in Updater.h for why any of this is shaped the way it is;
	// the short version is that the Updater has no working directory, so every path that crosses
	// between the two programs has to be absolute, and the handoff file itself has to sit at a
	// fixed location because neither program can construct the other's path.
	// ---------------------------------------------------------------------------------------

	// The devices a handoff file is looked for on. Two rather than one because a game installed on
	// ef0: is otherwise silently unsupported, and there is no way for the Updater to work out which
	// device it was that started it.
	static const char* kHandoffDevices[] = { "ms0:", "ef0:" };
	static const int kHandoffDeviceCount = 2;

	static std::string HandoffPathOn(const std::string& device)
	{
		// device carries its colon ("ms0:"), which is how sceIo wants it and how getcwd() reports it.
		return device + "/" + PendingFileName();
	}

	// "ms0:/PSP/GAME/..." -> "ms0:". A path straight from getcwd() always has a colon, but falling
	// back to ms0: is the only sensible answer if one ever does not.
	static std::string DevicePrefixOf(const std::string& absolutePath)
	{
		size_t colon = absolutePath.find(':');
		if (colon == std::string::npos)
		{
			return "ms0:";
		}
		return absolutePath.substr(0, colon + 1);
	}

	// Reads a whole small text file. Capped, because this is a file the Updater trusts only as far as
	// it has to: a truncated or scribbled-on handoff should fail cleanly rather than be parsed.
	//
	// The cap is a parameter because the two callers hold very different things. The handoff file is
	// six lines, where 4 KB is already generous and the low cap is the point -- it turns a wrong file
	// into a clean failure instead of an allocation a PSP cannot satisfy. An install's own record is a
	// line per file, so the same 4 KB would truncate every record past roughly 60 files, and truncation
	// is the one failure this file must not have: a truncated record parses as a shorter list of files,
	// and a shorter list is a check that silently stops noticing things.
	static bool ReadWholeFile(const std::string& path, std::string* out, size_t maxBytes = 4096)
	{
		out->clear();

		SceUID fd = sceIoOpen(path.c_str(), PSP_O_RDONLY, 0777);
		if (fd < 0)
		{
			return false;
		}

		char buffer[512];
		int got;
		while ((got = sceIoRead(fd, buffer, sizeof(buffer))) > 0)
		{
			out->append(buffer, (size_t)got);
			if (out->size() > maxBytes)
			{
				break;	// far larger than this file can legitimately be
			}
		}

		sceIoClose(fd);
		return true;
	}

	// Writes a whole small text file. One place does it, so the handoff file and the install record
	// cannot drift in how carefully they do it: truncate, loop on short writes, close before returning.
	static bool WriteWholeFile(const std::string& path, const std::string& text)
	{
		SceUID fd = sceIoOpen(path.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
		if (fd < 0)
		{
			return false;
		}

		size_t put = 0;
		while (put < text.size())
		{
			int now = sceIoWrite(fd, text.c_str() + put, (unsigned int)(text.size() - put));
			if (now <= 0)
			{
				sceIoClose(fd);
				return false;
			}
			put += (size_t)now;
		}

		sceIoClose(fd);
		return true;
	}

	// Splits one "name|value" line. Only the FIRST separator is consumed, so a value that contains a
	// pipe -- which a Windows path never does, but a careless edit might -- survives intact.
	static bool SplitField(const std::string& line, std::string* name, std::string* value)
	{
		size_t bar = line.find('|');
		if (bar == std::string::npos)
		{
			return false;
		}

		*name = line.substr(0, bar);
		*value = line.substr(bar + 1);
		return true;
	}

	static void ParsePending(const std::string& text, Pending* out)
	{
		out->gameDir.clear();
		out->package.clear();
		out->version.clear();
		out->versionCode = 0;
		out->state.clear();

		size_t start = 0;
		for (;;)
		{
			size_t end = text.find('\n', start);
			if (end == std::string::npos)
			{
				end = text.size();
			}

			std::string line = text.substr(start, end - start);

			// Tolerate CR, because this file is small and will inevitably get opened on a PC.
			if (!line.empty() && line[line.size() - 1] == '\r')
			{
				line.erase(line.size() - 1);
			}

			std::string name, value;
			if (SplitField(line, &name, &value))
			{
				if (name == "game_dir")
				{
					out->gameDir = value;
				}
				else if (name == "package")
				{
					out->package = value;
				}
				else if (name == "version")
				{
					out->version = value;
				}
				else if (name == "code")
				{
					out->versionCode = atoi(value.c_str());
				}
				else if (name == "state")
				{
					out->state = value;
				}
			}

			if (end >= text.size())
			{
				break;
			}

			start = end + 1;
		}
	}

	bool WritePending(const Pending& p, std::string* error)
	{
		std::string path = HandoffPathOn(DevicePrefixOf(p.gameDir));

		char code[32];
		snprintf(code, sizeof(code), "%d", p.versionCode);

		// Absolute paths only, and that is not a style choice: the Updater that reads this has no
		// working directory, so a bare "update.zip" here would resolve to nothing at all.
		std::string text;
		text += "game_dir|" + p.gameDir + "\n";
		text += "package|" + p.package + "\n";
		text += "version|" + p.version + "\n";
		text += "code|" + std::string(code) + "\n";
		text += "state|" + (p.state.empty() ? std::string("apply") : p.state) + "\n";

		if (!WriteWholeFile(path, text))
		{
			if (error != 0)
			{
				*error = "could not write " + path;
			}
			Trace("handoff: FAILED to write '%s'", path.c_str());
			return false;
		}

		Trace("handoff: wrote '%s' for game dir '%s'", path.c_str(), p.gameDir.c_str());
		return true;
	}

	bool ReadPending(Pending* out, std::string* error, std::string* pathUsed)
	{
		for (int i = 0; i < kHandoffDeviceCount; ++i)
		{
			std::string path = HandoffPathOn(kHandoffDevices[i]);
			std::string text;

			if (!ReadWholeFile(path, &text))
			{
				continue;
			}

			ParsePending(text, out);

			// A handoff with no game folder or no package cannot be acted on. Saying so here means the
			// Updater can report one clear reason instead of failing later on an empty path.
			if (out->gameDir.empty() || out->package.empty())
			{
				if (error != 0)
				{
					*error = path + " does not name both a game folder and a package";
				}
				return false;
			}

			if (pathUsed != 0)
			{
				*pathUsed = path;
			}
			return true;
		}

		if (error != 0)
		{
			*error = "there is no handoff file on any device";
		}
		return false;
	}

	void ClearPending()
	{
		for (int i = 0; i < kHandoffDeviceCount; ++i)
		{
			sceIoRemove(HandoffPathOn(kHandoffDevices[i]).c_str());
		}
	}

	// Where the swap handoff lives on a device, spelled the same way HandoffPathOn is.
	static std::string SwapPathOn(const std::string& device)
	{
		return device + "/" + LauncherSwapFileName();
	}

	bool WriteLauncherSwap(const std::string& stagedExe, const std::string& targetExe, std::string* error)
	{
		// One device, taken from the target: the helper runs beside the launcher it is replacing, so
		// both files are on the same device, and the target path is the one the helper is handed anyway.
		// Writing on every device the way the Pending handoff does would be harmless but would also mean
		// a stale file on ef0: that nobody ever clears.
		std::string path = SwapPathOn(DevicePrefixOf(targetExe));

		std::string text = stagedExe + "|" + targetExe + "\n";

		if (!WriteWholeFile(path, text))
		{
			if (error != 0)
			{
				*error = "could not write " + path;
			}
			Trace("swap: FAILED to write '%s'", path.c_str());
			return false;
		}

		Trace("swap: wrote '%s' ('%s' -> '%s')", path.c_str(), stagedExe.c_str(), targetExe.c_str());
		return true;
	}

	void ClearLauncherSwap()
	{
		for (int i = 0; i < kHandoffDeviceCount; ++i)
		{
			sceIoRemove(SwapPathOn(kHandoffDevices[i]).c_str());
		}
	}

	bool LaunchExecutable(const std::string& absolutePath, std::string* error)
	{
		// The recipe measured on hardware by tools/loadexec-probe. The absolute path is the
		// requirement; everything else here is the documented default.
		//
		// apitype MS2 (0x141) is pspinit.h's "used by the firmware to execute games (and homebrew :P)
		// from a memory stick". key is set to "game" because that is the documented value, though the
		// probe showed a NULL key launches too, so it is not load-bearing. unk5 is 0x10000 because
		// ploadexec_kernel.h documents that as the field's default.
		const char* path = absolutePath.c_str();

		struct SceKernelLoadExecVSHParam vsh;
		memset(&vsh, 0, sizeof(vsh));
		vsh.size = sizeof(vsh);
		vsh.key = "game";
		vsh.unk5 = 0x10000;

		int cfwResult = sctrlKernelLoadExecVSHWithApitype(PSP_INIT_APITYPE_MS2, path, &vsh);

		Trace("handoff: sctrlKernelLoadExecVSHWithApitype('%s') returned 0x%08X", path, (unsigned int)cfwResult);

		/* A PLAIN sceKernelLoadExec FALLBACK WAS TRIED HERE AND REMOVED. Recorded so nobody adds it back
		 * on the same reasoning, because the reasoning is sound and the outcome was not.
		 *
		 * The case for it: PPSSPP does not implement SystemCtrlForUser, so the import above resolves to a
		 * stub that logs "Unknown syscall" and answers SCE_KERNEL_ERROR_LIBRARY_NOT_YET_LINKED, and the
		 * emulator is where this whole program is developed -- so Play could never be exercised there.
		 * sceKernelLoadExec is a genuine user-mode export and PPSSPP does implement it.
		 *
		 * What it actually did, measured on PPSSPP 1.20.4:
		 *   - With the real 4.9 MB EBOOT it DID load the module, and the game got as far as its own
		 *     boot trace and then stopped after eleven lines, at `RenderManager::InitDebugFont`. Booting
		 *     the identical file directly from the XMB reaches 172 lines and its menu, so the difference
		 *     is the launch path and not the game.
		 *   - With a deliberately corrupt file it did not return AT ALL. The launcher's log ends on the
		 *     line above, so the program never reached its own error handling: the emulator wedges
		 *     instead of reporting that the file is not executable.
		 * The second is what settled it. A launcher whose failure mode is a freeze is worse than one that
		 * cannot start the game and says so -- and on hardware this call is dead code anyway, because the
		 * CFW call above it never returns. So it bought nothing real and cost the honest failure path.
		 *
		 * The consequence to know when testing: Play CANNOT WORK IN PPSSPP. It will fail with the message
		 * below every time. That is the emulator lacking the CFW's launcher, not a launcher bug, and the
		 * Play path and the LaunchDir handoff can only be exercised on hardware. */

		/* TEMPORARY DIAGNOSTIC -- REMOVE BEFORE SHIPPING.
		 *
		 * The gui.buffer.screenshot/loadexec fallback above documents why this does not belong in the
		 * shipped launcher. It is back for one experiment only: with it, PPSSPP reproduces the console's
		 * font crash exactly -- same statement, same trace -- which is the only way to iterate on that
		 * crash without a hardware round-trip per attempt. Delete this block once the crash is fixed and
		 * confirmed on the card. */
		struct SceKernelLoadExecParam plain;
		memset(&plain, 0, sizeof(plain));
		plain.size = sizeof(plain);
		plain.key = "game";
		int plainResult = sceKernelLoadExec(path, &plain);
		Trace("handoff: TEMP sceKernelLoadExec('%s') returned 0x%08X", path, (unsigned int)plainResult);

		// Not reached when the launch works: the console resets into the new program and this process
		// ceases to exist. So arriving here ALWAYS means it did not happen, and callers depend on that --
		// the game uses "we came back" as its cue to show the manual instructions.
		if (error != 0)
		{
			char buffer[160];

			// SCE_KERNEL_ERROR_LIBRARY_NOT_YET_LINKED has a specific meaning and wants a specific answer:
			// the firmware does not provide SystemCtrlForUser's launcher. That is a different problem from
			// a launch being refused -- it needs a CFW, not a retry -- so it is named rather than printed
			// as a number that invites the wrong guess.
			if ((unsigned int)cfwResult == 0x8002013Au)
			{
				snprintf(buffer, sizeof(buffer),
					"this firmware has no launcher (0x%08X)", (unsigned int)cfwResult);
			}
			else
			{
				snprintf(buffer, sizeof(buffer),
					"the launch was refused (0x%08X)", (unsigned int)cfwResult);
			}

			*error = buffer;
		}

		return false;
	}

	unsigned int CrcOfBuffer(const void* data, unsigned int len)
	{
		// crc32() takes and returns an unsigned long; masking keeps the manifest's 32-bit field
		// honest on a build where that type is wider.
		return (unsigned int)crc32(0, (const Bytef*)data, (uInt)len);
	}

	// Reads a file in chunks, feeding crc32 and summing the size in one pass so a large file is never
	// held in RAM.
	bool CrcOfFile(const std::string& path, unsigned int* outCrc, unsigned int* outSize)
	{
		SceUID fd = sceIoOpen(path.c_str(), PSP_O_RDONLY, 0777);
		if (fd < 0)
		{
			return false;
		}

		static char buffer[8192];   // static: this can be called from a thread with a small stack
		unsigned long crc = crc32(0, NULL, 0);
		unsigned int total = 0;
		int read = 0;

		while ((read = sceIoRead(fd, buffer, sizeof(buffer))) > 0)
		{
			crc = crc32(crc, (const Bytef*)buffer, (uInt)read);
			total += (unsigned int)read;
		}

		sceIoClose(fd);

		if (read < 0)
		{
			return false;
		}

		if (outCrc)
		{
			*outCrc = (unsigned int)crc;
		}
		if (outSize)
		{
			*outSize = total;
		}
		return true;
	}

	bool VerifyInstalled(const Manifest& m, std::string* error)
	{
		for (size_t i = 0; i < m.files.size(); ++i)
		{
			const FileEntry& entry = m.files[i];

			unsigned int crc = 0, size = 0;
			if (!CrcOfFile(entry.path, &crc, &size))
			{
				if (error)
				{
					*error = "missing after install: " + entry.path;
				}
				Trace("verify: '%s' is not there", entry.path.c_str());
				return false;
			}

			// Size first: it is the cheap check and it is the one that catches a truncated copy, which
			// is by far the likeliest failure of an install interrupted by a flat battery.
			if (size != entry.size)
			{
				if (error)
				{
					char note[512];
					snprintf(note, sizeof(note), "%s is %u bytes, expected %u", entry.path.c_str(), size, entry.size);
					*error = note;
				}
				Trace("verify: '%s' size %u != %u", entry.path.c_str(), size, entry.size);
				return false;
			}

			if (crc != entry.crc)
			{
				if (error)
				{
					char note[512];
					snprintf(note, sizeof(note), "%s is corrupt (crc %u, expected %u)", entry.path.c_str(), crc, entry.crc);
					*error = note;
				}
				Trace("verify: '%s' crc %u != %u", entry.path.c_str(), crc, entry.crc);
				return false;
			}
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------
	// The install's own record of what it contains
	//
	// See the long note in Updater.h. The short version: a package's manifest is a delta, so an install
	// that wants to be able to answer "am I still complete?" has to keep an accumulating description of
	// itself. That description is the thing the launcher compares against the card to decide whether the
	// button says Play or Update.
	// ---------------------------------------------------------------------------------------

	// A record is a couple of hundred kilobytes for a build this size, so the read is capped well above
	// what one can legitimately be -- the cap is there to survive a corrupt file, not to size the file.
	static const size_t kMaxRecordBytes = 512 * 1024;

	bool ReadInstalledManifest(const std::string& installFolder, Manifest* out)
	{
		if (out == 0)
		{
			return false;
		}

		// "." is the ordinary case: an install runs with its own folder as the working directory, which
		// is also why the record's relative paths mean the same thing here as they do to a package.
		std::string path = installFolder.empty()
			? std::string(InstallManifestName())
			: (installFolder + "/" + InstallManifestName());

		std::string text;
		if (!ReadWholeFile(path, &text, kMaxRecordBytes))
		{
			return false;
		}

		Manifest parsed;
		if (!ParseManifest(text, &parsed))
		{
			Trace("record: '%s' did not parse", path.c_str());
			return false;
		}

		*out = parsed;
		return true;
	}

	bool RecordInstalledManifest(const Manifest& applied, std::string* error)
	{
		// What is already recorded. A full package replaces it; a patch folds into it.
		Manifest existing;
		bool haveExisting = ReadInstalledManifest(".", &existing);

		Manifest merged;

		if (applied.full || !haveExisting)
		{
			// A full package defines the folder outright -- every file in it is there because this
			// package put it there. With nothing to merge into, the package's own list is also the best
			// account of the folder available, and it errs in the safe direction: the check can only
			// report on files it knows about, so a short record under-reports rather than inventing
			// problems.
			merged.files = applied.files;
		}
		else
		{
			// Indexed rather than searched per entry: a patch is a handful of files against a record of
			// a couple of thousand, which is the wrong ratio for the naive double loop.
			std::map<std::string, FileEntry> byPath;

			for (size_t i = 0; i < existing.files.size(); ++i)
			{
				byPath[existing.files[i].path] = existing.files[i];
			}

			for (size_t i = 0; i < applied.files.size(); ++i)
			{
				byPath[applied.files[i].path] = applied.files[i];
			}

			// A package that renames a file deletes the old name. Dropping it here rather than keeping
			// it is what stops the record from growing a phantom entry that the check would then report
			// as missing forever.
			for (size_t i = 0; i < applied.deletes.size(); ++i)
			{
				byPath.erase(applied.deletes[i]);
			}

			merged.files.reserve(byPath.size());
			for (std::map<std::string, FileEntry>::const_iterator it = byPath.begin(); it != byPath.end(); ++it)
			{
				merged.files.push_back(it->second);
			}
		}

		merged.version = applied.version;
		merged.versionCode = applied.versionCode;
		merged.full = true;			// the record always describes the whole folder

		// Runtime-mutated paths are dropped from the record as well as from the check, and both halves
		// are needed. The check skipping them is what makes "complete" reachable; the record not carrying
		// them is what stops it growing entries that can only ever look wrong, and what lets the fix
		// reach an install whose record was written before any of this existed.
		{
			std::vector<FileEntry> kept;
			kept.reserve(merged.files.size());

			for (size_t i = 0; i < merged.files.size(); ++i)
			{
				if (!IsRuntimePath(merged.files[i].path))
				{
					kept.push_back(merged.files[i]);
				}
			}

			if (kept.size() != merged.files.size())
			{
				Trace("record: dropped %u runtime path(s) from the record",
					  (unsigned int)(merged.files.size() - kept.size()));
			}
			merged.files.swap(kept);
		}

		merged.force = false;
		merged.base.clear();
		merged.deletes.clear();

		if (merged.files.empty())
		{
			// Nothing to write. An absent record reads as "unknown", which is the honest answer, and
			// better than a file that ParseManifest would refuse on the way back in.
			Trace("record: nothing to record");
			return true;
		}

		// Serialized in the format ParseManifest already reads, so the record is a manifest and there is
		// no second format to keep in step with the first.
		std::string text;
		char line[512];

		snprintf(line, sizeof(line), "version|%s\n", merged.version.c_str());
		text += line;
		snprintf(line, sizeof(line), "code|%d\n", merged.versionCode);
		text += line;

		unsigned int unwritable = 0;
		for (size_t i = 0; i < merged.files.size(); ++i)
		{
			const FileEntry& entry = merged.files[i];

			// A path containing the format's own separators cannot be written and read back, and writing
			// it anyway would corrupt every line after it. Skipped rather than escaped: no path this
			// project produces has one, and an escape scheme would be a second thing for the writer and
			// the reader to agree on.
			if (entry.path.empty() || entry.path.find('|') != std::string::npos ||
				entry.path.find('\n') != std::string::npos || entry.path.find('\r') != std::string::npos)
			{
				++unwritable;
				continue;
			}

			snprintf(line, sizeof(line), "file|%s|%u|%u\n",
				entry.path.c_str(), entry.size, entry.crc);
			text += line;
		}

		if (unwritable > 0)
		{
			// Logged rather than silent: a path this writer cannot express is a bug in whatever produced
			// it, and the consequence is a file that will never be checked.
			Trace("record: %u path(s) could not be written", unwritable);
		}

		if (!WriteWholeFile(InstallManifestName(), text))
		{
			if (error != 0)
			{
				*error = "could not write " + std::string(InstallManifestName());
			}
			Trace("record: FAILED to write '%s'", InstallManifestName());
			return false;
		}

		Trace("record: %u file(s) recorded", (unsigned int)merged.files.size());
		return true;
	}

	// ---------------------------------------------------------------------------------------
	// The tree scan. See the note on TreeScan in the header for why this is a directory walk rather
	// than a loop of sceIoGetstat; this is where that decision is cashed in.
	// ---------------------------------------------------------------------------------------

	/*
	 * A path with its letters folded to upper case, for comparison and only for comparison.
	 *
	 * WHY THE COMPARISON CANNOT BE LITERAL, measured on the emulator and true of the card for the same
	 * reason. FAT stores a name that fits 8.3 as an upper-case short name plus a flag saying the original
	 * was lower case, and the driver is free to ignore that flag -- so `Assets` comes back out of
	 * sceIoDread as `ASSETS`, and did. The record, meanwhile, is written on a PC by the packaging script
	 * from the paths as they are written on disk. Comparing the two literally therefore reported 1392 of
	 * 1479 files missing on an install that was complete but for one file, because `ASSETS/...` is not
	 * `Assets/...` to a string.
	 *
	 * Folding both sides is not a workaround for that, it is the correct comparison: the card is
	 * case-INSENSITIVE, so two names differing only in case are the same file to it, and there can be no
	 * pair of entries that folding would wrongly collapse.
	 */
	static std::string FoldPath(const std::string& path)
	{
		std::string out = path;
		for (unsigned int i = 0; i < (unsigned int)out.size(); ++i)
		{
			char c = out[i];
			if (c >= 'a' && c <= 'z')
			{
				out[i] = (char)(c - 'a' + 'A');
			}
		}
		return out;
	}

	void BeginTreeScan(const std::string& installFolder, TreeScan* out)
	{
		if (out == 0)
		{
			return;
		}

		out->root = installFolder;
		out->dirs.clear();
		out->sizes.clear();
		out->dirsRead = 0;
		out->filesSeen = 0;

		// The empty string is the install root itself. It is the one directory that is NOT built by
		// joining, because the join would double the separator or leave a trailing one, and a name with
		// either does not resolve on the card.
		out->dirs.push_back(std::string());
	}

	bool StepTreeScan(TreeScan* scan, unsigned int budgetUs)
	{
		if (scan == 0)
		{
			return true;
		}

		unsigned int start = sceKernelGetSystemTimeLow();

		while (!scan->dirs.empty())
		{
			std::string rel = scan->dirs.back();
			scan->dirs.pop_back();

			std::string abs = rel.empty() ? scan->root : (scan->root + "/" + rel);

			SceUID dir = sceIoDopen(abs.c_str());
			if (dir < 0)
			{
				// A directory that cannot be opened gets no error of its own. Every file under it simply
				// will not be found, and the comparison below names those files missing -- which is the
				// answer that matters, and the correct one: a directory the card will not read IS an
				// install that will not boot.
				continue;
			}

			SceIoDirent entry;
			while (true)
			{
				memset(&entry, 0, sizeof(entry));
				if (sceIoDread(dir, &entry) <= 0)
				{
					break;
				}

				if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
				{
					continue;
				}

				std::string childRel = rel.empty()
					? std::string(entry.d_name)
					: (rel + "/" + entry.d_name);

				// A directory goes on the queue; a file's length is taken from the dirent that was read
				// anyway. THAT is the saving: the old form asked the card about this same file a second
				// time, by full name, for a number that had already been handed over here.
				if (FIO_S_ISDIR(entry.d_stat.st_mode))
				{
					scan->dirs.push_back(childRel);
					continue;
				}

				scan->sizes[FoldPath(childRel)] = (unsigned int)entry.d_stat.st_size;
				++scan->filesSeen;
			}

			sceIoDclose(dir);
			++scan->dirsRead;

			// The budget is checked between directories rather than inside one, which is a deliberate
			// exception to the slicing everywhere else in this file. The largest directory in the install
			// this was written against holds 89 entries, so the overshoot on any single frame is bounded
			// and small -- and splitting a directory across frames would mean holding an open handle and
			// a half-read cursor in the caller's state, which is real complexity bought for a stall that
			// cannot be felt.
			if ((sceKernelGetSystemTimeLow() - start) > budgetUs)
			{
				break;
			}
		}

		return scan->dirs.empty();
	}

	void CompareTreeScan(const TreeScan& scan, const Manifest& m, Integrity* out)
	{
		if (out == 0)
		{
			return;
		}

		for (unsigned int i = 0; i < (unsigned int)m.files.size(); ++i)
		{
			// Read the record forwards rather than the tree: the record is what defines the install, and a
			// file the tree has that the record does not is not a fault. A saved world is exactly that,
			// and so is anything a player has put in the folder.
			const FileEntry& entry = m.files[i];

			// Runtime-mutated paths are not evidence that an install is broken, and this is the line that
			// decides whether "complete" is reachable at all. Without it 39 of the 42 files a healthy
			// install was reported as failing were files the game rewrites on its own account -- its
			// input config, its probe tiles, its logs -- so the install could never read as complete and
			// the repair prompt fired on every boot. See IsRuntimePath.
			if (IsRuntimePath(entry.path))
			{
				continue;
			}

			++out->checked;

			// Folded on both sides -- see the note on FoldPath. The name reported in a problem is the
			// record's own spelling, because that is the one a person reading the log can find on a PC.
			std::map<std::string, unsigned int>::const_iterator found =
				scan.sizes.find(FoldPath(entry.path));
			if (found == scan.sizes.end())
			{
				++out->missing;
				if (out->firstProblem.empty())
				{
					out->firstProblem = entry.path + " is missing";
				}
				continue;
			}

			if (found->second != entry.size)
			{
				++out->wrongSize;
				if (out->firstProblem.empty())
				{
					char note[512];
					snprintf(note, sizeof(note), "%s is %u bytes, not %u",
						entry.path.c_str(), found->second, entry.size);
					out->firstProblem = note;
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------
	// Install
	// ---------------------------------------------------------------------------------------

	bool ReadManifestFromZip(const std::string& zipPath, Manifest* out, std::string* error)
	{
		if (out == 0)
		{
			return false;
		}

		pgeZip* zip = pgeZipOpen(zipPath.c_str());
		if (zip == 0)
		{
			if (error)
			{
				*error = "The package could not be opened. It may have been truncated in transfer.";
			}
			Trace("read manifest: pgeZipOpen failed on '%s'", zipPath.c_str());
			return false;
		}

		// The manifest lives inside the archive rather than beside it, so a package is a single file
		// that cannot be separated from its own description. If the two could drift apart, a stale
		// manifest next to a fresh zip would verify entirely the wrong things.
		bool ok = false;
		pgeZipFile* entry = pgeZipFileRead(zip, "manifest.txt", 0);

		if (entry == 0)
		{
			if (error)
			{
				*error = "The package has no manifest, so there is no way to tell what it contains.";
			}
			Trace("read manifest: '%s' has no manifest.txt", zipPath.c_str());
		}
		else
		{
			std::string text(reinterpret_cast<const char*>(entry->data), (size_t)entry->size);
			pgeZipFileFree(entry);
			ok = ParseManifest(text, out);
			if (!ok && error)
			{
				*error = "The package's manifest is malformed.";
			}
		}

		pgeZipClose(zip);
		return ok;
	}

	// Creates every directory in a package path's parent chain, relative to the working directory --
	// which for both callers is the game folder, because the game runs there and the Updater chdirs
	// there before touching anything.
	//
	// The return value of sceIoMkdir is deliberately ignored. "Already exists" is the normal case on
	// every install after the first, and treating it as a failure would refuse to install a package
	// that only changed the contents of a directory the game has always had.
	static void MakeParentDirectories(const std::string& path)
	{
		for (size_t i = 0; i < path.size(); ++i)
		{
			if (path[i] != '/')
			{
				continue;
			}

			std::string dir = path.substr(0, i);
			if (!dir.empty())
			{
				sceIoMkdir(dir.c_str(), 0777);
			}
		}
	}

	/*
	 * One file out of a package, at a path of the caller's choosing.
	 *
	 * Everything InstallPackage does, minus the parts that only make sense for a whole tree: this reads
	 * the package's manifest (so the entry is verified against what the package advertised rather than
	 * trusted), streams exactly that one entry out with the same 32 KB buffer, and checks the size and
	 * crc32 after writing. It exists for the one file the launcher cannot install in place -- its own
	 * EBOOT.PBP, which has to be staged under a different name and swapped by the helper -- so it
	 * deliberately has no skip-if-identical, no delete list and no install-record bookkeeping.
	 *
	 * The destination path is the CALLER's, not the manifest's, which is the whole point: the manifest
	 * names "EBOOT.PBP", and writing that name here would be writing over the running file.
	 */
	bool ExtractOneFromZip(const std::string& zipPath, const std::string& entryName,
						   const std::string& destPath, std::string* error)
	{
		Manifest manifest;
		if (!ReadManifestFromZip(zipPath, &manifest, error))
		{
			return false;
		}

		const FileEntry* want = 0;
		for (size_t i = 0; i < manifest.files.size(); ++i)
		{
			if (manifest.files[i].path == entryName)
			{
				want = &manifest.files[i];
				break;
			}
		}

		if (want == 0)
		{
			if (error != 0)
			{
				*error = "the package does not contain " + entryName;
			}
			Trace("extract1: '%s' names no entry '%s'", zipPath.c_str(), entryName.c_str());
			return false;
		}

		pgeZip* zip = pgeZipOpen(zipPath.c_str());
		if (zip == 0)
		{
			if (error != 0)
			{
				*error = "the package could not be reopened for extraction";
			}
			return false;
		}

		unsigned int streamSize = 0;
		if (pgeZipStreamOpen(zip, entryName.c_str(), 0, &streamSize) != 0)
		{
			pgeZipClose(zip);
			if (error != 0)
			{
				*error = "could not open " + entryName + " in the package";
			}
			return false;
		}

		if (streamSize != want->size)
		{
			pgeZipStreamClose(zip);
			pgeZipClose(zip);
			if (error != 0)
			{
				*error = "the package's directory and manifest disagree about " + entryName;
			}
			return false;
		}

		// Parent directories before the file. A launcher package is written in place, so a path it
		// introduces that the launcher has never written to -- a new subfolder -- would otherwise fail
		// on the open below. Walked on the DESTINATION rather than the manifest name because the two
		// differ here by design (the caller chooses the destination), and Mkdir on a directory that
		// already exists is not an error worth stopping for.
		for (size_t s = 0; s < destPath.size(); ++s)
		{
			if (destPath[s] == '/')
			{
				std::string dir = destPath.substr(0, s);
				if (!dir.empty())
				{
					sceIoMkdir(dir.c_str(), 0777);
				}
			}
		}

		SceUID fd = sceIoOpen(destPath.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
		if (fd < 0)
		{
			pgeZipStreamClose(zip);
			pgeZipClose(zip);
			if (error != 0)
			{
				*error = "could not open " + destPath + " to write";
			}
			return false;
		}
		static unsigned char chunk[32 * 1024];
		unsigned int wrote = 0;
		bool failed = false;

		while (wrote < want->size)
		{
			unsigned int ask = want->size - wrote;
			if (ask > (unsigned int)sizeof(chunk))
			{
				ask = (unsigned int)sizeof(chunk);
			}

			int got = pgeZipStreamRead(zip, chunk, ask);
			if (got <= 0)
			{
				failed = true;
				break;
			}

			int put = 0;
			while (put < got)
			{
				int now = sceIoWrite(fd, chunk + put, (unsigned int)(got - put));
				if (now <= 0)
				{
					failed = true;
					break;
				}
				put += now;
			}
			if (failed)
			{
				break;
			}

			wrote += (unsigned int)got;
		}

		sceIoClose(fd);
		pgeZipStreamClose(zip);
		pgeZipClose(zip);

		if (failed || wrote != want->size)
		{
			sceIoRemove(destPath.c_str());
			if (error != 0)
			{
				*error = entryName + " did not extract completely";
			}
			Trace("extract1: '%s' incomplete (%u of %u)", destPath.c_str(), wrote, want->size);
			return false;
		}

		unsigned int gotCrc = 0, gotSize = 0;
		if (!CrcOfFile(destPath, &gotCrc, &gotSize) || gotSize != want->size || gotCrc != want->crc)
		{
			sceIoRemove(destPath.c_str());
			if (error != 0)
			{
				*error = entryName + " did not match the manifest after extraction";
			}
			Trace("extract1: '%s' crc mismatch", destPath.c_str());
			return false;
		}

		Trace("extract1: wrote '%s' (%u bytes, crc %08X)", destPath.c_str(), gotSize, gotCrc);
		return true;
	}

	bool InstallPackage(const std::string& zipPath, Manifest* outManifest, std::string* error,
						ProgressFn progress, void* ctx)
	{
		Manifest manifest;
		if (!ReadManifestFromZip(zipPath, &manifest, error))
		{
			return false;
		}

		Trace("install: '%s' claims v%s (%d), %u file(s), %u deletion(s)",
			zipPath.c_str(), manifest.version.c_str(), manifest.versionCode,
			(unsigned int)manifest.files.size(), (unsigned int)manifest.deletes.size());

		Trace("install: reopening the package for extraction");

		pgeZip* zip = pgeZipOpen(zipPath.c_str());
		if (zip == 0)
		{
			if (error)
			{
				*error = "The package could not be reopened for extraction.";
			}
			Trace("install: second pgeZipOpen failed on '%s'", zipPath.c_str());
			return false;
		}

		Trace("install: reopened, extracting");

		// Extraction is driven by the MANIFEST, not by walking the archive, and every file is read with
		// pgeZipFileRead and written with sceIo. That is a deliberate departure from pgeZipExtractCounted,
		// which was tried first and killed the console on hardware -- twice, on the first entry, before a
		// single byte reached the card.
		//
		// What it replaces: pgeZip's extractor walks its own entry list and writes each one through
		// newlib's fopen/fwrite/fclose. What is used instead is only what this program had ALREADY
		// proved on this console -- pgeZipFileRead (ReadManifestFromZip above succeeds with it every
		// run) and sceIoOpen/sceIoWrite (the handoff file and the log are written with them). Neither
		// half of the old path is needed: the manifest names every file that should come out, so the
		// archive never has to be enumerated, and pgeZipFileRead locates an entry by NAME rather than by
		// position, so the entries can be read in any order.
		//
		// The cost is that an entry read this way is held in RAM whole. That is why the sizes are checked
		// against the manifest before anything is allocated for them, and it is also what makes this
		// correct: read-by-name cannot silently extract the wrong entry, and a mismatch is caught before
		// the file on the card is truncated.
		int written = 0, failed = 0, skipped = 0;

		// The denominator for the bar, from the manifest rather than from the archive, because the
		// manifest is what this loop is driven by and what verification will be measured against.
		unsigned int totalBytes = 0;
		unsigned int doneBytes = 0;
		for (size_t i = 0; i < manifest.files.size(); ++i)
		{
			totalBytes += manifest.files[i].size;
		}

		for (size_t i = 0; i < manifest.files.size(); ++i)
		{
			const FileEntry& entry = manifest.files[i];

			/*
			 * Already correct on disk, so there is nothing to write.
			 *
			 * This is what makes an install touch only the files that actually changed. A patch built
			 * against a base carries just the differing files, but the same package applied twice -- a
			 * retry after a failure, or a forced re-push of the build already on the card -- would
			 * otherwise rewrite every entry it lists, and the in-place patch case rewrites files that
			 * are byte-identical to what is already there whenever the diff was built from a slightly
			 * different tree.
			 *
			 * The comparison is the manifest's own size and crc32, which is the same pair verification
			 * uses afterwards, so a file this skips is a file verification would have accepted. That
			 * equivalence is the whole justification: skipping can only ever skip something already
			 * accepted, so it cannot turn a good install into a bad one. Skips are still counted and
			 * logged, because "0 written, 1479 skipped" and "1479 written" look identical otherwise.
			 */
			unsigned int haveCrc = 0, haveSize = 0;
			if (CrcOfFile(entry.path, &haveCrc, &haveSize) &&
				haveSize == entry.size && haveCrc == entry.crc)
			{
				Trace("install: [%u/%u] '%s' already matches, skipping",
					(unsigned int)(i + 1), (unsigned int)manifest.files.size(), entry.path.c_str());
				skipped++;
				doneBytes += entry.size;
				if (progress != 0)
				{
					progress(ctx, doneBytes, totalBytes);
				}
				continue;
			}

			Trace("install: [%u/%u] '%s' (%u bytes)",
				(unsigned int)(i + 1), (unsigned int)manifest.files.size(),
				entry.path.c_str(), entry.size);

			/*
			 * Read out of the package a chunk at a time, not all at once.
			 *
			 * pgeZipFileRead would allocate the entry's whole uncompressed size, which makes the caller's
			 * memory requirement equal to the largest single file in the archive. That is a ceiling this
			 * program cannot meet: the launchable heap is around 15 MB and the build carries a 12.7 MB
			 * asset, so the biggest file could not be extracted through that API at all -- and it failed
			 * as an allocation error, which arrives later as "missing after install" and names the file
			 * rather than the memory. Streaming costs a fixed 32 KB buffer instead, so the install no
			 * longer depends on how large any individual file is.
			 */
			unsigned int streamSize = 0;
			if (pgeZipStreamOpen(zip, entry.path.c_str(), 0, &streamSize) != 0)
			{
				Trace("install: could not open '%s' in the package", entry.path.c_str());
				failed++;
				continue;
			}

			if (streamSize != entry.size)
			{
				Trace("install: '%s' is %u bytes in the package, the manifest promised %u",
					entry.path.c_str(), streamSize, entry.size);
				pgeZipStreamClose(zip);
				failed++;
				continue;
			}

			// Parent directories before the file, because a package that adds a path the game has never
			// written to is ordinary and the open would otherwise fail.
			MakeParentDirectories(entry.path);

			SceUID fd = sceIoOpen(entry.path.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
			if (fd < 0)
			{
				Trace("install: could not open '%s' to write (0x%08X)",
					entry.path.c_str(), (unsigned int)fd);
				pgeZipStreamClose(zip);
				failed++;
				continue;
			}

			// One buffer for every entry, on the stack of this function rather than the heap, so the
			// peak memory of an install is this and not the size of the biggest file in it.
			static unsigned char chunk[32 * 1024];
			unsigned int wrote = 0;
			bool writeFailed = false;

			while (wrote < entry.size)
			{
				unsigned int want = entry.size - wrote;
				if (want > (unsigned int)sizeof(chunk))
				{
					want = (unsigned int)sizeof(chunk);
				}

				int got = pgeZipStreamRead(zip, chunk, want);
				if (got < 0)
				{
					Trace("install: reading '%s' failed after %u bytes (0x%08X)",
						entry.path.c_str(), wrote, (unsigned int)got);
					writeFailed = true;
					break;
				}
				if (got == 0)
				{
					// End of the entry before the promised size: a truncated archive, which the manifest
					// size check above cannot catch because it only compares what the directory claims.
					break;
				}

				// Looped, because sceIoWrite is allowed to write less than it was given. Assuming
				// otherwise is how a 4.9 MB EBOOT ends up silently truncated to whatever one call
				// managed -- and with a 32 KB buffer that risk is on every chunk, not just the last.
				int put = 0;
				while (put < got)
				{
					int now = sceIoWrite(fd, chunk + put, (unsigned int)(got - put));
					if (now <= 0)
					{
						Trace("install: write to '%s' failed after %u bytes (0x%08X)",
							entry.path.c_str(), wrote + (unsigned int)put, (unsigned int)now);
						writeFailed = true;
						break;
					}
					put += now;
				}
				if (writeFailed)
				{
					break;
				}

				wrote += (unsigned int)got;

				// Per chunk, not per file: the point of the bar is that it moves during the one big
				// file, which is the only interesting case -- a 4.9 MB EBOOT is a single entry, so
				// per-file reporting would step from nothing to everything and show a bar that sat
				// still for the whole install.
				doneBytes += (unsigned int)got;
				if (progress != 0)
				{
					progress(ctx, doneBytes, totalBytes);
				}
			}

			sceIoClose(fd);
			pgeZipStreamClose(zip);

			if (writeFailed || wrote != entry.size)
			{
				Trace("install: '%s' is incomplete on the card (%u of %u bytes)",
					entry.path.c_str(), wrote, entry.size);
				failed++;
				continue;
			}

			Trace("install: [%u/%u] wrote '%s' (%u bytes)",
				(unsigned int)(i + 1), (unsigned int)manifest.files.size(),
				entry.path.c_str(), wrote);
			written++;
		}

		Trace("install: extraction finished, closing the package");

		pgeZipClose(zip);

		Trace("install: wrote %d file(s), %d already current, %d failed", written, skipped, failed);

		// The delete list is applied before verification, not after. A package that renames a file
		// deletes the old name and writes the new one; verifying first would be fine, but deleting
		// last means a crash between the two leaves both names present, and for a texture atlas that
		// is a visible bug rather than a harmless leftover.
		for (size_t i = 0; i < manifest.deletes.size(); ++i)
		{
			int result = sceIoRemove(manifest.deletes[i].c_str());
			if (result < 0)
			{
				// Not fatal. The file may legitimately already be gone (a second install of the same
				// package), and a leftover file costs a few bytes rather than breaking the update.
				Trace("install: could not delete '%s' (0x%08X)", manifest.deletes[i].c_str(), (unsigned int)result);
			}
			else
			{
				Trace("install: deleted '%s'", manifest.deletes[i].c_str());
			}
		}

		// Verify against the manifest rather than against the extractor's own count. The count says
		// how many entries were attempted; this says whether the bytes on the card are the bytes that
		// were promised, which is the only question that matters. It is also what names the file when
		// something is wrong -- and the file it will name, if the EBOOT cannot be replaced while the
		// game holds it, is EBOOT.PBP.
		std::string why;
		if (!VerifyInstalled(manifest, &why))
		{
			if (error)
			{
				*error = why;
			}
			Trace("install: verification failed -- %s", why.c_str());
			return false;
		}

		if (outManifest)
		{
			*outManifest = manifest;
		}

		/*
		 * The install's own record of what it now contains, written before the card is flushed below so
		 * the two land together.
		 *
		 * Done here rather than left to the caller because "installed" and "described" have to be the
		 * same event. A caller that forgot would produce an install nothing can check, and the record
		 * has to be right the first time it is wanted -- which is the next time the launcher draws its
		 * menu, possibly with no package left on the card to rebuild it from.
		 *
		 * A failure here is logged and not fatal: every file verified above. The cost is that this
		 * install cannot be checked for missing files later, which is worth a line and nothing more.
		 */
		{
			std::string recordError;
			if (!RecordInstalledManifest(manifest, &recordError))
			{
				Trace("install: %s", recordError.c_str());
			}
		}

		// The last thing a successful install does, and the reason it is here rather than left to the
		// caller: "installed" has to mean "on the card", not "in the cache".
		//
		// Verification above re-reads each file, but it reads through the same driver cache that is
		// holding the writes, so it can pass while the card still has nothing. What follows proves the
		// difference: the Updater installs a new EBOOT.PBP and immediately asks loadexec to run it, and
		// loadexec takes the file's size and cluster chain from the DIRECTORY. Closing the file did not
		// write those, so without this flush the loader is handed a file the card does not yet agree
		// exists -- which is exactly what happened on hardware: the install reported success, the log
		// stopped on the launch line, and the console went down with the freshly written EBOOT.PBP on
		// the stick but its directory entry not, and the removal of update.zip lost for the same reason.
		Aurora::Utils::Logger::SyncCard();

		Trace("install: complete, v%s", manifest.version.c_str());
		return true;
	}

	// ---------------------------------------------------------------------------------------
	// Network
	// ---------------------------------------------------------------------------------------

	bool NetInit()
	{
		/*
		 * ALREADY UP? Then there is nothing here to do, and doing it anyway is what this guard exists to
		 * stop.
		 *
		 * Every call below is a one-time operation, and this function is called twice in a normal
		 * launcher session: once at boot for the silent connect, and again when the update flow starts.
		 * It used to re-run all of them on the second call. On the console that is not harmless --
		 * sceNetInit answers success a second time while the layers above it report "already
		 * initialised", so the socket pool ends up initialised twice against a single teardown. The
		 * emulator absorbs that because every one of these calls is a stub returning 0; the console does
		 * not, and both of this launcher's hardware faults so far sit downstream of exactly this second
		 * init and nothing else. The runs that did not crash never made a second call.
		 *
		 * The gate is the ownership flag, which is a fact about this process rather than a guess about the
		 * stack: sNetOwner is set by the call that actually brought sceNet up, and cleared by the
		 * teardown, so it is exactly "the stack is up and this program put it there".
		 *
		 * Probing instead -- sceNetApctlGetState, which the end of this function uses to tell whether the
		 * stack came up -- was rejected here on purpose. It would also catch a stack somebody else
		 * brought up, but a probe that answers before apctl is initialised would skip this init on a
		 * cold start, and that failure mode is a launcher with no networking at all. The flag cannot be
		 * wrong that way.
		 */
		if (sNetOwner)
		{
			sNetUp = true;
			Trace("net already up and ours, reusing it (no second init)");
			return true;
		}

		// Load the five net modules by hand rather than through sceUtilityLoadNetModule's "common"
		// bundle: HTTP needs COMMON, INET, PARSEURI, PARSEHTTP and HTTP, and loading them explicitly
		// means a failure names the module it failed on.
		//
		// A module load that fails is *not* fatal on its own. Loading a module twice reports an error,
		// and this function is allowed to be called twice (the game checks at boot and again if the
		// player opens the menu), so the return value here is logged and then ignored -- the sceNet*
		// inits below are the real gate, because they fail for every reason that actually matters.
		int module = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
		if (module < 0) Trace("load net module COMMON: 0x%08X (continuing)", (unsigned int)module);
		else sModulesLoaded |= kLoadedCommon;
		module = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
		if (module < 0) Trace("load net module INET: 0x%08X (continuing)", (unsigned int)module);
		else sModulesLoaded |= kLoadedInet;
		module = sceUtilityLoadNetModule(PSP_NET_MODULE_PARSEURI);
		if (module < 0) Trace("load net module PARSEURI: 0x%08X (continuing)", (unsigned int)module);
		else sModulesLoaded |= kLoadedParseUri;
		module = sceUtilityLoadNetModule(PSP_NET_MODULE_PARSEHTTP);
		if (module < 0) Trace("load net module PARSEHTTP: 0x%08X (continuing)", (unsigned int)module);
		else sModulesLoaded |= kLoadedParseHttp;
		module = sceUtilityLoadNetModule(PSP_NET_MODULE_HTTP);
		if (module < 0) Trace("load net module HTTP: 0x%08X (continuing)", (unsigned int)module);
		else sModulesLoaded |= kLoadedHttp;

		// Raised here rather than after the inits below, so that a failure part way through still gets
		// unwound: NetShutdown is guarded by this flag, and setting it only on total success would mean
		// a run that got as far as sceNetInit and then failed leaves the socket pool allocated for the
		// rest of the session. The teardown calls are individually safe to make when their own init
		// never ran -- they return an error, which is ignored -- so unwinding too much costs nothing
		// and unwinding too little leaks.
		sNetUp = true;

		// The pool sizes are pgeNetInit's, which are the values this engine is known to cope with on a
		// PSP-1000's smaller RAM: 128 KB for the socket layer, 4 KB each for the inet and apctl
		// threads. Do not shrink these to save memory without testing on a 1000.
		//
		// Every one of these is a one-time init, and a second call in the same process fails --
		// Aurora::System::NetworkManager::InitializeConnection performs this same sequence and the main
		// menu calls it, so the stack already being up is a perfectly good state, not a failure.
		//
		// A failure here is therefore not treated as "networking is off" on its own. What decides the
		// outcome is the apctl probe at the end, which separates the two reasons sceNetInit can fail --
		// already up, or the pool could not be allocated -- by asking something only a live stack can
		// answer, instead of guessing from the code.
		int result = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
		if (result >= 0)
		{
			sNetOwner = true;
		}
		else
		{
			// 0x80020190 is SCE_KERNEL_ERROR_NO_MEMORY, which is what this console actually reports.
			// It is why "already initialised" was the wrong reading of it: no pool means no socket
			// layer, and no amount of connecting will supply one.
			Trace("sceNetInit: 0x%08X (%s)", (unsigned int)result, KernelErrorName((unsigned int)result));
		}

		if (sNetOwner)
		{
			// Only the owner runs these; a non-owner's calls would fail with the same "already
			// initialised" and say nothing useful.
			result = sceNetInetInit();
			if (result < 0)
			{
				Trace("sceNetInetInit: 0x%08X (%s)", (unsigned int)result, KernelErrorName((unsigned int)result));
			}

			result = sceNetApctlInit(0x10000, 48);
			if (result < 0)
			{
				Trace("sceNetApctlInit: 0x%08X (%s)", (unsigned int)result, KernelErrorName((unsigned int)result));
			}

			result = sceNetResolverInit();
			if (result < 0)
			{
				Trace("sceNetResolverInit: 0x%08X (%s)", (unsigned int)result, KernelErrorName((unsigned int)result));
			}
		}

		// One question decides every path above: can apctl answer? It only can once it has been
		// initialised, which is exactly what has to be true for the connection dialog and the address
		// check to mean anything.
		int apctlState = 0;
		if (sceNetApctlGetState(&apctlState) < 0)
		{
			Trace("net is not usable: apctl does not answer, so the stack never came up");
			return false;
		}

		Trace("net ready (owner=%d)", (int)sNetOwner);
		return true;
	}

	void NetShutdown()
	{
		if (!sNetUp)
		{
			return;
		}

		// If this module did not bring the stack up, it does not get to take it down. The main menu's
		// NetworkManager performs the same init and keeps its own "initialised" flag, so tearing the
		// stack down from here would leave it believing in a connection it no longer has, and the next
		// thing to use the network would fail with nothing to say why. It is also the friendlier
		// arrangement: connect once in the menu, then update without being asked to connect again.
		if (!sNetOwner)
		{
			sNetUp = false;
			Trace("net left up (owned by another part of the game)");
			return;
		}

		/*
		 * Bring the connection DOWN first, and wait for it to actually go.
		 *
		 * This is not tidiness, and the order here is the whole point. Terminating the stack while a
		 * connection is still established leaves the radio and the console's own network service part
		 * way through a teardown, and this launcher reaches here holding a connection it did not make
		 * more often than not: the player connects in the XMB's settings, then presses Check for
		 * updates, and the launcher never made a connection at all. Unloading the INET module under a
		 * live link is the kind of thing that faults on hardware and cannot fault in an emulator,
		 * because the emulator has no radio to leave in that state.
		 *
		 * The wait is bounded. The only thing worse than not waiting for the disconnect is waiting
		 * forever for it at the end of an update the player has already watched finish.
		 */
		Trace("net shutdown: bringing the connection down");

		SceNetApctlInfo info;
		memset(&info, 0, sizeof(info));
		if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) == 0 && info.ip[0] != '\0')
		{
			sceNetApctlDisconnect();

			// 3 s, then go anyway. DISCONNECTED is the state that means the link is gone; anything
			// else means it is still on the way down.
			for (int i = 0; i < 60; ++i)
			{
				int state = PSP_NET_APCTL_STATE_DISCONNECTED;
				if (sceNetApctlGetState(&state) < 0)
				{
					break;
				}
				if (state == PSP_NET_APCTL_STATE_DISCONNECTED)
				{
					break;
				}
				sceKernelDelayThread(50 * 1000);
			}
		}

		sNetUp = false;

		// HTTPS is torn down before HTTP is unloaded: sceHttps sits on both the HTTP module and the
		// SSL heap. A LAN-only run never brought it up, and ShutdownHttps is a no-op then.
		ShutdownHttps();

		// Reverse order of NetInit. Individual failures are ignored: the point of this call is to
		// leave the machine clean for whatever runs next, and it cannot do better than try.
		Trace("net shutdown: apctl term");
		sceNetApctlTerm();
		Trace("net shutdown: resolver term");
		sceNetResolverTerm();
		Trace("net shutdown: inet term");
		sceNetInetTerm();
		Trace("net shutdown: net term");
		sceNetTerm();

		/*
		 * Only the modules this program loaded. A load that reported ALREADY_LOADED was somebody
		 * else's -- see sModulesLoaded -- and unloading it would take the code and data out from under
		 * whatever is still using it. That is the case the launcher meets most often, because the
		 * player connects in the XMB's settings and every load then reports ALREADY_LOADED.
		 *
		 * Unloaded last-in-first-out so HTTP's dependants go before the parser and socket modules they
		 * sit on, which is the reverse of how they were loaded.
		 */
		if (sModulesLoaded & kLoadedHttp)
		{
			Trace("net shutdown: unload HTTP (loaded here)");
			sceUtilityUnloadNetModule(PSP_NET_MODULE_HTTP);
		}
		if (sModulesLoaded & kLoadedParseHttp)
		{
			Trace("net shutdown: unload PARSEHTTP (loaded here)");
			sceUtilityUnloadNetModule(PSP_NET_MODULE_PARSEHTTP);
		}
		if (sModulesLoaded & kLoadedParseUri)
		{
			Trace("net shutdown: unload PARSEURI (loaded here)");
			sceUtilityUnloadNetModule(PSP_NET_MODULE_PARSEURI);
		}
		if (sModulesLoaded & kLoadedInet)
		{
			Trace("net shutdown: unload INET (loaded here)");
			sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
		}
		if (sModulesLoaded & kLoadedCommon)
		{
			Trace("net shutdown: unload COMMON (loaded here)");
			sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
		}

		if (sModulesLoaded != 0)
		{
			Trace("net shutdown: module bits 0x%02X were ours", sModulesLoaded);
		}
		else
		{
			Trace("net shutdown: every module was already loaded elsewhere, nothing unloaded");
		}

		sModulesLoaded = 0;
		sNetOwner = false;
		Trace("net down");
	}

	static bool UrlIsHttps(const std::string& url)
	{
		return url.size() >= 8 && url.compare(0, 8, "https://") == 0;
	}

	// SSL is brought up on the first HTTPS request, not in NetInit. A LAN update never pays the
	// 160 KB sceSsl heap, and a card that only ever talks to 192.168.x keeps the old path exactly.
	static bool EnsureHttps()
	{
		if (sSslUp)
		{
			return true;
		}

		int module = sceUtilityLoadNetModule(PSP_NET_MODULE_SSL);
		if (module < 0)
		{
			Trace("load net module SSL: 0x%08X (continuing)", (unsigned int)module);
		}
		else
		{
			sModulesLoaded |= kLoadedSsl;
		}

		int result = sceSslInit(0x28000);
		if (result < 0)
		{
			Trace("sceSslInit failed: 0x%08X", (unsigned int)result);
			return false;
		}

		result = sceHttpsInit(0, 0, 0, 0);
		if (result < 0)
		{
			Trace("sceHttpsInit failed: 0x%08X", (unsigned int)result);
			sceSslEnd();
			return false;
		}

		result = sceHttpsLoadDefaultCert(0, 0);
		if (result < 0)
		{
			Trace("sceHttpsLoadDefaultCert: 0x%08X (continuing)", (unsigned int)result);
		}

		sSslUp = true;
		Trace("https ready");
		return true;
	}

	static void ShutdownHttps()
	{
		if (sSslUp)
		{
			Trace("net shutdown: https end");
			sceHttpsEnd();
			Trace("net shutdown: ssl end");
			sceSslEnd();
			sSslUp = false;
		}

		if (sModulesLoaded & kLoadedSsl)
		{
			Trace("net shutdown: unload SSL (loaded here)");
			sceUtilityUnloadNetModule(PSP_NET_MODULE_SSL);
		}
	}

	// Reads the address apctl is holding right now. Empty means none, and that is also the answer when
	// the stack never came up.
	//
	// PSP_NET_APCTL_INFO_IP is the field the 2008 pge helper passed as a bare `8`, and it took a
	// char[32]. The modern SDK takes a union instead, so this is written against the real signature
	// rather than the old one -- using the constant also means the next reader does not have to go and
	// find out what 8 was.
	//
	// A successful read of a non-empty address *is* the test for "connected", and it is a better one
	// than the apctl state machine: an access point that has dropped leaves the state showing GOT_IP
	// for a while, but the address is cleared.
	static bool ReadCurrentAddress(char* out, unsigned int size)
	{
		if (out != 0 && size > 0)
		{
			out[0] = '\0';
		}

		// Nothing to ask if the stack never came up: sceNetApctlGetInfo on an uninitialised apctl is a
		// call into a module whose init failed, and the answer it gives is meaningless either way.
		if (!sNetUp)
		{
			return false;
		}

		SceNetApctlInfo info;
		memset(&info, 0, sizeof(info));
		if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) != 0)
		{
			return false;
		}
		if (info.ip[0] == '\0')
		{
			return false;
		}

		if (out != 0 && size > 0)
		{
			snprintf(out, size, "%s", info.ip);
		}
		return true;
	}

	bool NetIsConnected()
	{
		return ReadCurrentAddress(0, 0);
	}

	bool NetCurrentAddress(std::string* address)
	{
		if (address != 0)
		{
			address->clear();
		}

		char buffer[64];
		bool ok = ReadCurrentAddress(buffer, sizeof(buffer));
		if (ok && address != 0)
		{
			*address = buffer;
		}
		return ok;
	}

	// Saved connections are numbered but NOT contiguous, which is the bug this replaces: the first
	// version stopped at the first missing id, so a list whose lowest entry had been deleted read as
	// empty and the launcher announced "no saved connections" on a console that had one. Ten is the
	// console's own limit (`sceUtilityCheckNetParam` documents ids 1..n) and also bounds the loop, so a
	// firmware that answered every id could not turn this into a hang at boot.
	bool NetConnectionValid(int index)
	{
		if (index < 1 || index > 10)
		{
			return false;
		}
		return sceUtilityCheckNetParam(index) == 0;
	}

	int NetConnectionCount()
	{
		int count = 0;
		for (int i = 1; i <= 10; ++i)
		{
			if (NetConnectionValid(i))
			{
				++count;
			}
		}
		return count;
	}

	bool NetConnectionName(int index, std::string* name)
	{
		netData data;
		memset(&data, 0, sizeof(data));

		if (sceUtilityGetNetParam(index, PSP_NETPARAM_NAME, &data) != 0)
		{
			return false;
		}

		// The field is a fixed 128-byte buffer that the firmware may fill without a terminator when
		// the name is long. Fortify the last byte rather than trust it.
		data.asString[sizeof(data.asString) - 1] = '\0';

		if (name != 0)
		{
			name->assign(data.asString);
		}
		return true;
	}

	bool NetConnectSilent(int connIndex, FrameFn pump, void* ctx, unsigned int timeoutMs)
	{
		if (connIndex < 1)
		{
			return false;
		}

		if (!NetIsConnected())
		{
			int result = sceNetApctlConnect(connIndex);
			if (result < 0)
			{
				Trace("sceNetApctlConnect(%d): 0x%08X (%s)", connIndex, (unsigned int)result,
					KernelErrorName((unsigned int)result));
				return false;
			}

			// Polled rather than blocked: sceNetApctlConnect returns as soon as the attempt is under
			// way, and this state machine is the only thing that knows whether it worked. The timeout
			// matters more here than it does in the dialog -- nothing on screen is asking the player to
			// wait, so an association that never completes has to give up by itself.
			unsigned int waitedMs = 0;
			int state = PSP_NET_APCTL_STATE_DISCONNECTED;

			for (;;)
			{
				if (pump != 0 && !pump(ctx))
				{
					Trace("silent connect to %d cancelled", connIndex);
					return false;
				}

				if (sceNetApctlGetState(&state) < 0)
				{
					Trace("sceNetApctlGetState failed");
					return false;
				}

				if (state == PSP_NET_APCTL_STATE_GOT_IP)
				{
					break;
				}

				// There is no single "failed" state in this enum, so a DISCONNECTED after the attempt
				// has had time to leave the starting state is the attempt being over and unsuccessful.
				// The short grace period is what keeps the loop from reading the one frame the state
				// machine spends at DISCONNECTED before it starts scanning as a failure.
				if (state == PSP_NET_APCTL_STATE_DISCONNECTED && waitedMs > 500)
				{
					Trace("silent connect to %d dropped out after %d ms", connIndex, (int)waitedMs);
					return false;
				}

				if (waitedMs >= timeoutMs)
				{
					Trace("silent connect to %d timed out after %d ms (state %d)",
						connIndex, (int)waitedMs, state);
					return false;
				}

				sceKernelDelayThread(50 * 1000);
				waitedMs += 50;
			}
		}

		// GOT_IP is not proof of a usable address; ask for the address, as NetIsConnected does.
		if (!NetIsConnected())
		{
			Trace("silent connect to %d reached GOT_IP but there is no address", connIndex);
			return false;
		}

		// The address, not just the fact of one. "The console joined a network" and "the console is on
		// the network the update server is on" are different statements, and on a console with several
		// saved connections only the address tells them apart -- which is exactly the question the
		// boot connect has to answer, and the one the first hardware session could not.
		std::string address;
		if (NetCurrentAddress(&address))
		{
			Trace("silent connect: connected via saved connection %d, address %s", connIndex,
				address.c_str());
		}
		else
		{
			Trace("silent connect: connected via saved connection %d", connIndex);
		}
		return true;
	}

	void NetDisconnect()
	{
		if (!sNetUp)
		{
			return;
		}

		int result = sceNetApctlDisconnect();
		if (result < 0)
		{
			Trace("sceNetApctlDisconnect: 0x%08X (%s)", (unsigned int)result,
				KernelErrorName((unsigned int)result));
		}
	}

	void NetLogConnections()
	{
		// Six is enough to say what the enumeration is doing: the first ids are where a gap or a
		// firmware that answers nothing at all will show up, and a console with more than six saved
		// networks is not the case this is diagnosing.
		for (int i = 1; i <= 6; ++i)
		{
			int check = sceUtilityCheckNetParam(i);

			std::string name;
			bool named = NetConnectionName(i, &name);

			Trace("saved connection %d -> check=0x%08X name=%s", i, (unsigned int)check,
				named ? name.c_str() : "(unreadable)");
		}
	}

	bool NetConnectDialog(FrameFn clear, FrameFn present, void* ctx)
	{
		pspUtilityNetconfData data;
		memset(&data, 0, sizeof(data));

		data.base.size = sizeof(data);

		// The dialog's own language and button layout come from the system settings, not from the
		// game's font setting: this is a system dialog and drawing it in English with the wrong
		// confirm button on a Japanese console would be worse than not showing it.
		int language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
		sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &language);
		int buttonSwap = 0;
		sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP, &buttonSwap);

		data.base.language = language;
		data.base.buttonSwap = buttonSwap;

		// Thread priorities. These are the values every pspdev sample uses; the dialogs do their
		// rendering on these threads and mis-ordering them against the main thread shows up as a
		// flickering or half-drawn dialog rather than as a clean failure.
		data.base.graphicsThread = 17;
		data.base.accessThread = 19;
		data.base.fontThread = 18;
		data.base.soundThread = 16;

		data.action = PSP_NETCONF_ACTION_CONNECTAP;
		data.hotspot = 0;
		data.wifisp = 0;

		int result = sceUtilityNetconfInitStart(&data);
		if (result < 0)
		{
			Trace("sceUtilityNetconfInitStart failed: 0x%08X", (unsigned int)result);
			return false;
		}

		bool aborted = false;
		// Separates "not started yet" from "has been and gone": NONE is what this reports in both
		// cases, and taking the first for the second is what makes a system dialog flash up for a
		// moment and then report that there is no address.
		bool dialogStarted = false;

		bool dialogDone = false;

		while (true)
		{
			// Cleared before the pump and presented after it, with the dialog drawing in between. This
			// order is the whole reason there are two callbacks; see the note in Updater.h.
			if (!clear(ctx))
			{
				aborted = true;
				sceUtilityNetconfShutdownStart();
				break;
			}

			int status = sceUtilityNetconfGetStatus();
			if (status < 0)
			{
				Trace("sceUtilityNetconfGetStatus failed: 0x%08X", (unsigned int)status);
				sceUtilityNetconfShutdownStart();
				return false;
			}

			if (status == PSP_UTILITY_DIALOG_QUIT)
			{
				// The dialog is asking to be torn down. Do that, then keep pumping until it reports
				// FINISHED -- returning here instead is the usual way to leave the utility in a state
				// the next one cannot start from.
				sceUtilityNetconfShutdownStart();
			}
			else if (status == PSP_UTILITY_DIALOG_FINISHED ||
					 (status == PSP_UTILITY_DIALOG_NONE && dialogStarted))
			{
				dialogDone = true;
			}
			else
			{
				// INIT, VISIBLE, or a NONE that has not been started yet -- every one of which wants
				// the dialog pumped so it can get where it is going.
				if (status != PSP_UTILITY_DIALOG_NONE)
				{
					dialogStarted = true;
				}
				sceUtilityNetconfUpdate(1);
			}

			// Presented every iteration regardless of the state, because the dialog's own frames live on
			// the graphics thread and only appear during the swap.
			if (!present(ctx))
			{
				aborted = true;
				sceUtilityNetconfShutdownStart();
				break;
			}

			if (dialogDone)
			{
				break;
			}
		}

		if (aborted)
		{
			Trace("connection dialog cancelled by the player");
			return false;
		}

		// The dialog closing is not proof of a connection: the player may have confirmed a connection
		// that then failed to associate. Ask the address instead of trusting the dialog.
		if (!NetIsConnected())
		{
			Trace("connection dialog finished but there is no address");
			return false;
		}

		Trace("connected");
		return true;
	}

	// ---------------------------------------------------------------------------------------
	// HTTP
	// ---------------------------------------------------------------------------------------

	// A single GET, pumped into either a std::string or a file descriptor. One function because the
	// two cases differ in three lines and must not differ in the parts that are easy to get wrong --
	// the timeout set, the status check, the cancel path and the cleanup.
	//
	// rangeStart >= 0 turns it into a partial fetch: a "Range: bytes=S-E" header is added to the
	// request and HTTP 206 is the only accepted answer. That mode exists so the launcher can read a
	// zip's index without pulling the whole archive, and it is deliberately strict about the reply --
	// see the status check below, because a server that ignores Range would otherwise hand back the
	// entire file and look like a success.
	//
	// maxMemoryBytes raises the in-RAM cap for a range fetch. A zip index is ~130 KB and the 16 KB
	// text cap would refuse it; passing 0 keeps the old, small limit for the text resources.
	static bool HttpGet(const std::string& url, std::string* toMemory, SceUID* toFile,
						ProgressFn progress, void* ctx, unsigned int* outSize,
						long rangeStart, long rangeEnd, unsigned int maxMemoryBytes)
	{
		if (outSize)
		{
			*outSize = 0;
		}

		const bool wantsRange = (rangeStart >= 0);
		const unsigned int memoryCap = (maxMemoryBytes != 0) ? maxMemoryBytes : kMaxInfoBytes;

		// GitHub (and any other https:// URL) goes through mbedtls. sceSsl cannot finish a
		// modern TLS 1.2 handshake -- hardware returned SCE_HTTP_ERROR_SSL (0x80431075) on
		// github.com -- so the Sony HTTPS path is kept only as unused init/teardown.
		if (UrlIsHttps(url))
		{
			return HttpsGet(url, toMemory, toFile, progress, ctx, outSize,
				rangeStart, rangeEnd, memoryCap);
		}

		int result = sceHttpInit(20 * 1024);
		if (result < 0)
		{
			Trace("sceHttpInit failed: 0x%08X (%s)", (unsigned int)result,
				HttpErrorName((unsigned int)result));
			return false;
		}

		if (UrlIsHttps(url) && !EnsureHttps())
		{
			sceHttpEnd();
			return false;
		}

		int templateId = sceHttpCreateTemplate((char*)"MineCraft-PSP/1.0", 1, 1);
		if (templateId < 0)
		{
			Trace("sceHttpCreateTemplate failed: 0x%08X", (unsigned int)templateId);
			sceHttpEnd();
			return false;
		}

		sceHttpSetResolveTimeOut(templateId, kResolveTimeoutUs);
		sceHttpSetConnectTimeOut(templateId, kConnectTimeoutUs);
		sceHttpSetRecvTimeOut(templateId, kRecvTimeoutUs);
		sceHttpSetSendTimeOut(templateId, kSendTimeoutUs);

		// GitHub's latest/download/ URL is a redirect onto the current tag, then another onto the
		// asset CDN. A LAN server is expected to serve the file at the name we asked for, so a 301
		// there still means "this is not the file" and is refused below. HTTPS is the GitHub channel.
		if (UrlIsHttps(url))
		{
			int enabled = sceHttpEnableRedirect(templateId);
			Trace("https: following redirects for '%s' (enable 0x%08X)",
				url.c_str(), (unsigned int)enabled);
		}

		int connectionId = sceHttpCreateConnectionWithURL(templateId, url.c_str(), 0);
		if (connectionId < 0)
		{
			Trace("sceHttpCreateConnectionWithURL failed: 0x%08X", (unsigned int)connectionId);
			sceHttpDeleteTemplate(templateId);
			sceHttpEnd();
			return false;
		}

		int requestId = sceHttpCreateRequestWithURL(connectionId, PSP_HTTP_METHOD_GET, (char*)url.c_str(), 0);
		if (requestId < 0)
		{
			Trace("sceHttpCreateRequestWithURL failed: 0x%08X", (unsigned int)requestId);
			sceHttpDeleteConnection(connectionId);
			sceHttpDeleteTemplate(templateId);
			sceHttpEnd();
			return false;
		}

		// The Range header goes on the REQUEST id, not on the template or the connection: it is a
		// property of this one fetch. sceHttpAddExtraHeader's return value is logged rather than
		// treated as fatal, because whether it took effect is only provable from the status code that
		// comes back -- and a clean "the server ignored it" is a more useful failure than a bail-out
		// here that says nothing about why.
		if (wantsRange)
		{
			char header[64];
			if (rangeEnd >= 0)
			{
				snprintf(header, sizeof(header), "bytes=%ld-%ld", rangeStart, rangeEnd);
			}
			else
			{
				snprintf(header, sizeof(header), "bytes=%ld-", rangeStart);
			}

			int added = sceHttpAddExtraHeader(requestId, (char*)"Range", header, 0);
			Trace("range: %s -> sceHttpAddExtraHeader 0x%08X", header, (unsigned int)added);
		}

		bool ok = false;
		int status = 0;

		result = sceHttpSendRequest(requestId, 0, 0);
		if (result < 0)
		{
			Trace("sceHttpSendRequest failed: 0x%08X (%s)", (unsigned int)result,
				HttpErrorName((unsigned int)result));
			goto cleanup;
		}

		result = sceHttpGetStatusCode(requestId, &status);
		if (result < 0)
		{
			Trace("sceHttpGetStatusCode failed: 0x%08X (%s)", (unsigned int)result,
				HttpErrorName((unsigned int)result));
			goto cleanup;
		}

		// 200 only, normally. A 301/302 means the script is not serving what the game asked for by that
		// name, and silently following redirects is how a "local update" ends up pulling from somewhere
		// else entirely.
		//
		// A range request is the one case where 206 is the wanted answer and 200 is the dangerous one.
		// 200 means the server ignored the Range header and is sending the WHOLE file: the 135 MB
		// download this feature exists to avoid. It is refused rather than accepted, because accepting
		// it would work, would be slow, and would leave no trace of the fact that the whole design
		// silently degraded into the thing it replaced.
		if (wantsRange)
		{
			if (status == 200)
			{
				Trace("range: server IGNORED Range and sent the whole resource -- refusing it");
				goto cleanup;
			}
			if (status != 206)
			{
				Trace("GET %s (range) -> HTTP %d, wanted 206", url.c_str(), status);
				goto cleanup;
			}
		}
		else if (status != 200)
		{
			Trace("GET %s -> HTTP %d", url.c_str(), status);
			goto cleanup;
		}

		{
			SceULong64 contentLength = 0;
			sceHttpGetContentLength(requestId, &contentLength);
			unsigned int total = (unsigned int)contentLength;
			unsigned int done = 0;

			// The cap is on the *body*, and Content-Length is the honest way to enforce it before a
			// single byte is buffered.
			if (toMemory != 0 && total > memoryCap)
			{
				Trace("refusing a %u byte in-memory resource (cap %u)", total, memoryCap);
				goto cleanup;
			}

			static char buffer[kChunkBytes];

			while (true)
			{
				int read = sceHttpReadData(requestId, buffer, kChunkBytes);

				if (read < 0)
				{
					Trace("sceHttpReadData failed: 0x%08X (%s)", (unsigned int)read,
						HttpErrorName((unsigned int)read));
					goto cleanup;
				}
				if (read == 0)
				{
					break;
				}

				done += (unsigned int)read;

				// The cap again, for the case where no Content-Length arrived (chunked encoding) and
				// the check above could not fire.
				if (toMemory != 0 && done > memoryCap)
				{
					Trace("in-memory resource exceeded %u bytes with no Content-Length", memoryCap);
					goto cleanup;
				}

				if (toMemory != 0)
				{
					toMemory->append(buffer, (size_t)read);
				}
				else
				{
					// sceIoWrite can write less than it was given, and ignoring that is another bug
					// inherited from pgeNetGetFile: it produces a short file that then fails its own
					// crc check, with nothing to say why.
					int written = 0;
					while (written < read)
					{
						int step = sceIoWrite(*toFile, buffer + written, read - written);
						if (step <= 0)
						{
							Trace("sceIoWrite failed: 0x%08X", (unsigned int)step);
							goto cleanup;
						}
						written += step;
					}
				}

				if (progress != 0 && !progress(ctx, done, total))
				{
					Trace("transfer cancelled by the player at %u/%u", done, total);
					goto cleanup;
				}
			}

			if (outSize)
			{
				*outSize = done;
			}

			// And once more, because a truncated transfer is the failure this whole layer exists to
			// catch, and the server telling us the length is what makes it detectable.
			if (total != 0 && done != total)
			{
				Trace("short transfer: got %u of %u bytes", done, total);
				goto cleanup;
			}
		}

		ok = true;

	cleanup:
		sceHttpDeleteRequest(requestId);
		sceHttpDeleteConnection(connectionId);
		sceHttpDeleteTemplate(templateId);
		sceHttpEnd();
		return ok;
	}

	bool HttpGetToString(const std::string& url, std::string* out, ProgressFn progress, void* ctx)
	{
		if (out == 0)
		{
			return false;
		}
		out->clear();
		unsigned int size = 0;
		return HttpGet(url, out, 0, progress, ctx, &size, -1, -1, 0);
	}

	// A partial fetch, into RAM. Only ever used for a zip's index, which is the case the range mode was
	// built for: ~130 KB of central directory instead of the whole archive.
	bool HttpGetRangeToString(const std::string& url, long start, long end, std::string* out,
							  unsigned int maxBytes, ProgressFn progress, void* ctx)
	{
		if (out == 0)
		{
			return false;
		}
		out->clear();
		unsigned int size = 0;
		return HttpGet(url, out, 0, progress, ctx, &size, start, end, maxBytes);
	}

	bool HttpGetToFile(const std::string& url, const std::string& destPath, ProgressFn progress, void* ctx)
	{
		// PSP_O_TRUNC is the fix for pgeNetGetFile's real bug: without it, downloading over an
		// existing file leaves whatever tail the old one had, so a re-download of a shorter package
		// produces a file that is neither version and passes a naive size check.
		SceUID fd = sceIoOpen(destPath.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
		if (fd < 0)
		{
			Trace("cannot open '%s' for writing: 0x%08X", destPath.c_str(), (unsigned int)fd);
			return false;
		}

		unsigned int size = 0;
		bool ok = HttpGet(url, 0, &fd, progress, ctx, &size, -1, -1, 0);

		sceIoClose(fd);

		if (!ok)
		{
			// Remove the partial file. Leaving it would be worse than useless: the next attempt would
			// find a file that looks plausible and a manifest that disagrees with it.
			sceIoRemove(destPath.c_str());
			return false;
		}

		Trace("downloaded %u bytes to '%s'", size, destPath.c_str());
		return true;
	}

	// ---------------------------------------------------------------------------------------
	// Reading a remote zip's index
	//
	// The arrangement this serves: the server is a static file host publishing release zips and a list
	// of them, exactly as a GitHub releases page does. It is given no instructions to follow, because
	// every decision -- which release, which file, whether anything is needed at all -- is made here.
	// The first question that has to be answered is "what is inside the zip?", and answering it by
	// downloading 135 MB is the thing being designed away.
	//
	// A zip is readable from its end. The last record in the file is the End Of Central Directory,
	// which states the offset and length of the central directory: a table naming every file, with its
	// CRC32, its compressed size and where its data begins. That table is all that is needed to decide
	// what differs locally, so the archive itself never has to move -- only its index.
	// ---------------------------------------------------------------------------------------

	// The most of an archive's index this will hold in RAM. Our own central directory is ~130 KB; the
	// cap is here so a corrupt or hostile length field cannot ask for an allocation the console cannot
	// satisfy -- and it is checked before the fetch, not after.
	static const unsigned int kMaxIndexBytes = 8 * 1024 * 1024;

	// How much of the end of the archive to pull first. The EOCD sits within the last 65557 bytes (its
	// own 22 plus a maximum-length comment), and the central directory ends immediately before it, so
	// this one window normally contains both and the second fetch never happens.
	static const unsigned int kIndexTailBytes = 512 * 1024;

	// RemoteEntry and ReleaseInfo are declared in Updater.h, because the launcher needs to be able to
	// name them; the reasoning behind their fields is documented there.

	static unsigned int ReadU16(const unsigned char* p)
	{
		return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
	}

	static unsigned int ReadU32(const unsigned char* p)
	{
		return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
			   ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
	}

	static void TrimSpaces(std::string* text)
	{
		size_t first = text->find_first_not_of(" \t\r");
		size_t last = text->find_last_not_of(" \t\r");
		if (first == std::string::npos)
		{
			text->clear();
			return;
		}
		*text = text->substr(first, last - first + 1);
	}

	// Small enough to be worth having locally: the release and index parsers build their failure
	// messages by hand, and a parser that cannot say WHICH line was wrong is much harder to fix.
	static std::string NumberToString(int value)
	{
		char text[16];
		snprintf(text, sizeof(text), "%d", value);
		return std::string(text);
	}

	// releases.txt, one record per release. A "release|<version>|<code>" line opens a record and the
	// lines after it describe that release's assets, until the next "release" line. That shape is a
	// small state machine rather than the flat key|value that ParseOffer reads, because a page of
	// releases is a list and an offer was a single item.
	// Declared in Updater.h, so not static: the launcher calls this one.
	bool ParseReleases(const std::string& text, std::vector<ReleaseInfo>* out, std::string* error)
	{
		out->clear();

		size_t pos = 0;
		int line = 0;

		while (pos <= text.size())
		{
			size_t end = text.find('\n', pos);
			std::string raw = text.substr(pos, (end == std::string::npos) ? std::string::npos : end - pos);
			pos = (end == std::string::npos) ? text.size() + 1 : end + 1;
			++line;

			TrimSpaces(&raw);
			if (raw.empty() || raw[0] == '#')
			{
				continue;
			}

			std::vector<std::string> parts;
			size_t at = 0;
			while (true)
			{
				size_t bar = raw.find('|', at);
				if (bar == std::string::npos)
				{
					parts.push_back(raw.substr(at));
					break;
				}
				parts.push_back(raw.substr(at, bar - at));
				at = bar + 1;
			}
			for (size_t i = 0; i < parts.size(); ++i)
			{
				TrimSpaces(&parts[i]);
			}

			const std::string& key = parts[0];

			if (key == "release")
			{
				if (parts.size() < 3)
				{
					if (error) { *error = "releases.txt line " + NumberToString(line) + ": 'release' needs a version and a code"; }
					return false;
				}
				ReleaseInfo info;
				info.version = parts[1];
				info.versionCode = atoi(parts[2].c_str());
				info.gameZipSize = 0;
				info.launcherZipSize = 0;
				info.shotsZipSize = 0;
				out->push_back(info);
				continue;
			}

			if (out->empty())
			{
				if (error) { *error = "releases.txt line " + NumberToString(line) + ": '" + key + "' before any 'release' line"; }
				return false;
			}

			ReleaseInfo& current = out->back();

			if (parts.size() < 3)
			{
				if (error) { *error = "releases.txt line " + NumberToString(line) + ": '" + key + "' needs a filename and a size"; }
				return false;
			}
			const std::string& file = parts[1];
			const unsigned int size = (unsigned int)strtoul(parts[2].c_str(), 0, 10);

			// A filename with a separator in it is refused here for the same reason ParseOffer refuses
			// one: the console's failure for it is a network-looking one, and this is where it is
			// fixable.
			if (file.find('/') != std::string::npos || file.find('\\') != std::string::npos)
			{
				if (error) { *error = "releases.txt line " + NumberToString(line) + ": '" + file + "' has a path in it"; }
				return false;
			}

			if (key == "game")
			{
				current.gameZip = file;
				current.gameZipSize = size;
			}
			else if (key == "launcher")
			{
				current.launcherZip = file;
				current.launcherZipSize = size;
			}
			else if (key == "shots")
			{
				current.shotsZip = file;
				current.shotsZipSize = size;
			}
			else
			{
				if (error) { *error = "releases.txt line " + NumberToString(line) + ": unknown key '" + key + "'"; }
				return false;
			}
		}

		if (out->empty())
		{
			if (error) { *error = "releases.txt lists no releases"; }
			return false;
		}
		return true;
	}

	// Locates the End Of Central Directory record and reads the three fields that matter. The record is
	// found by scanning backwards for its signature rather than by computing an offset, because a
	// variable-length comment can follow it -- 4 bytes past the fixed part is only correct for an
	// archive whose comment is empty, which is exactly the kind of assumption that works until it does
	// not.
	static bool ReadEndOfCentralDirectory(const unsigned char* data, size_t size,
										  unsigned int* outOffset, unsigned int* outSize,
										  unsigned int* outCount, std::string* error)
	{
		if (size < 22)
		{
			if (error) { *error = "the fetched tail is too small to contain a zip end-of-directory record"; }
			return false;
		}

		size_t lowest = 0;
		if (size > 22 + 65535)
		{
			lowest = size - 22 - 65535;
		}

		long found = -1;
		for (long i = (long)size - 22; i >= (long)lowest; --i)
		{
			if (ReadU32(data + i) == 0x06054B50u)
			{
				found = i;
				break;
			}
		}

		if (found < 0)
		{
			if (error) { *error = "no zip end-of-directory record in the fetched bytes"; }
			return false;
		}

		const unsigned char* e = data + found;
		const unsigned int count = ReadU16(e + 10);
		const unsigned int cdSize = ReadU32(e + 12);
		const unsigned int cdOffset = ReadU32(e + 16);

		// ZIP64 hides the real values behind 0xFFFFFFFF placeholders and puts them in a separate record.
		// Refusing clearly is the whole point: mis-reading the sentinel as a real offset would send the
		// entry parser somewhere arbitrary and produce nonsense rather than an error.
		if (cdOffset == 0xFFFFFFFFu || cdSize == 0xFFFFFFFFu || count == 0xFFFFu)
		{
			if (error) { *error = "the archive is ZIP64, which this reader does not support"; }
			return false;
		}

		// The offset is returned RELATIVE TO THE FILE, exactly as it is stored, and is rebased against a
		// buffer in ParseCentralDirectory and nowhere else. Rebasing it here as well is what broke the
		// first version of this reader, and it failed in a way worth recording: the number was
		// subtracted twice, so an offset of 98,900,114 came back as 386,079, the window looked as if it
		// were in front of the directory rather than behind it, and the parser refused an index it had
		// actually read correctly. One place does the conversion; this is not it.
		*outOffset = cdOffset;
		*outSize = cdSize;
		*outCount = count;
		return true;
	}

	// Walks the central directory in a buffer that starts at `bufferStart` in the file. Returns entries
	// in the folder's own order; a name whose bytes are truncated by the window is an error rather than
	// a guess.
	static bool ParseCentralDirectory(const unsigned char* data, size_t size, unsigned int bufferStart,
									  unsigned int cdOffsetInFile, unsigned int cdSize, unsigned int count,
									  std::vector<RemoteEntry>* out, std::string* error)
	{
		out->clear();
		out->reserve(count);

		if (cdOffsetInFile < bufferStart)
		{
			if (error) { *error = "the central directory is not inside the fetched bytes"; }
			return false;
		}

		const size_t begin = (size_t)cdOffsetInFile - bufferStart;
		if (begin + cdSize > size)
		{
			if (error) { *error = "the fetched bytes are shorter than the central directory"; }
			return false;
		}

		size_t at = begin;
		for (unsigned int i = 0; i < count; ++i)
		{
			if (at + 46 > size)
			{
				if (error) { *error = "the central directory is truncated at entry " + NumberToString(i); }
				return false;
			}
			if (ReadU32(data + at) != 0x02014B50u)
			{
				if (error) { *error = "bad central-directory signature at entry " + NumberToString(i); }
				return false;
			}

			RemoteEntry entry;
			entry.method = ReadU16(data + at + 10);
			entry.crc = ReadU32(data + at + 16);
			entry.compressedSize = ReadU32(data + at + 20);
			entry.uncompressedSize = ReadU32(data + at + 24);
			const unsigned int nameLen = ReadU16(data + at + 28);
			const unsigned int extraLen = ReadU16(data + at + 30);
			const unsigned int commentLen = ReadU16(data + at + 32);
			entry.localHeaderOffset = ReadU32(data + at + 42);

			if (at + 46 + nameLen > size)
			{
				if (error) { *error = "an entry name is truncated at entry " + NumberToString(i); }
				return false;
			}
			entry.name.assign((const char*)(data + at + 46), (size_t)nameLen);

			if (entry.compressedSize == 0xFFFFFFFFu || entry.uncompressedSize == 0xFFFFFFFFu ||
				entry.localHeaderOffset == 0xFFFFFFFFu)
			{
				if (error) { *error = "entry '" + entry.name + "' is ZIP64, which is not supported"; }
				return false;
			}

			out->push_back(entry);
			at += 46 + nameLen + extraLen + commentLen;
		}

		return true;
	}

	// The whole index read: one range fetch of the tail in the normal case, a second exact fetch only
	// when the central directory reaches further back than the window. Either way the archive's own data
	// is never touched, which is what makes this cheap enough to do on every launch.
	bool FetchZipIndex(const std::string& url, unsigned int zipSize,
					   std::vector<RemoteEntry>* out, std::string* error)
	{
		out->clear();

		if (zipSize < 22)
		{
			if (error) { *error = "the archive is too small to contain a zip directory"; }
			return false;
		}

		const unsigned int start = (zipSize > kIndexTailBytes) ? (zipSize - kIndexTailBytes) : 0;

		std::string tail;
		if (!HttpGetRangeToString(url, (long)start, (long)zipSize - 1, &tail, kIndexTailBytes, 0, 0))
		{
			if (error) { *error = "the archive's index could not be fetched"; }
			return false;
		}

		Trace("index: fetched the last %u bytes of a %u byte archive (%.1f%% of it)",
			  (unsigned int)tail.size(), zipSize,
			  100.0 * (double)tail.size() / (double)zipSize);

		// TEMPORARY DIAGNOSTIC. The parser is known to succeed against this exact archive on the host, so
		// if it fails here the bytes that arrived are not the bytes on disk -- and these lines are what
		// tells a transfer fault apart from a parsing one. The signature PK\x05\x06 belongs 22 bytes
		// before the end of a comment-less archive; if it is not there, the window is not where it looks.
		if (tail.size() >= 26)
		{
			const unsigned char* t = (const unsigned char*)tail.data();
			const size_t n = tail.size();
			Trace("index: first4=%02X%02X%02X%02X  at(size-22)=%02X%02X%02X%02X  last4=%02X%02X%02X%02X",
				  t[0], t[1], t[2], t[3],
				  t[n - 22], t[n - 21], t[n - 20], t[n - 19],
				  t[n - 4], t[n - 3], t[n - 2], t[n - 1]);
		}

		unsigned int cdOffset = 0, cdSize = 0, count = 0;
		if (!ReadEndOfCentralDirectory((const unsigned char*)tail.data(), tail.size(),
									   &cdOffset, &cdSize, &count, error))
		{
			return false;
		}

		Trace("index: parsed count=%u cdSize=%u cdOffset=%u (window starts at %u)",
			  count, cdSize, cdOffset, start);

		if (cdSize > kMaxIndexBytes)
		{
			if (error) { *error = "the central directory is " + NumberToString(cdSize) + " bytes, over the cap"; }
			return false;
		}

		// In the common case the directory is already inside the window. When it is not, fetch exactly
		// it -- the second range is sized from the EOCD, so it is still the directory and nothing else.
		if ((unsigned long)start + tail.size() >= (unsigned long)cdOffset + cdSize)
		{
			return ParseCentralDirectory((const unsigned char*)tail.data(), tail.size(), start,
										 cdOffset, cdSize, count, out, error);
		}

		Trace("index: the directory starts before the window; fetching %u bytes at offset %u", cdSize, cdOffset);

		std::string directory;
		if (!HttpGetRangeToString(url, (long)cdOffset, (long)(cdOffset + cdSize - 1), &directory,
								  kMaxIndexBytes, 0, 0))
		{
			if (error) { *error = "the archive's central directory could not be fetched"; }
			return false;
		}

		return ParseCentralDirectory((const unsigned char*)directory.data(), directory.size(), cdOffset,
									 cdOffset, cdSize, count, out, error);
	}

	// A diagnostic, and the piece that proves the arrangement end to end without changing anything on
	// the card: fetch a release zip's index by range and say what is in it. It writes nothing, so it is
	// safe to run against any release, and its log line is the measurement that matters -- how many
	// bytes crossed the wire to learn the contents of a whole archive.
	bool InspectRemoteZip(const std::string& url, unsigned int zipSize)
	{
		Trace("probe: inspecting %s (%u bytes)", url.c_str(), zipSize);

		std::vector<RemoteEntry> entries;
		std::string error;

		if (!FetchZipIndex(url, zipSize, &entries, &error))
		{
			Trace("probe: FAILED -- %s", error.c_str());
			return false;
		}

		unsigned long long total = 0;
		unsigned int stored = 0, deflated = 0, other = 0;

		for (size_t i = 0; i < entries.size(); ++i)
		{
			total += entries[i].uncompressedSize;
			if (entries[i].method == 0)      { ++stored; }
			else if (entries[i].method == 8) { ++deflated; }
			else                             { ++other; }
		}

		Trace("probe: OK -- %u entries, %llu uncompressed bytes (stored %u, deflated %u, other %u)",
			  (unsigned int)entries.size(), total, stored, deflated, other);

		for (size_t i = 0; i < entries.size() && i < 8; ++i)
		{
			Trace("probe:   %s  crc %08X  csize %u  usize %u  method %u",
				  entries[i].name.c_str(), entries[i].crc, entries[i].compressedSize,
				  entries[i].uncompressedSize, entries[i].method);
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------
	// Repairing an install by fetching only what is wrong with it.
	// ---------------------------------------------------------------------------------------

	// The largest compressed entry this will hold in RAM while refetching it. The heap is 8 MB and this
	// buffer is the whole of the cost -- the uncompressed side is streamed to the card -- so the cap is
	// set below the heap rather than at it. An entry over this is not an error: it makes the plan
	// unusable, which sends the caller down the full-download path it already has.
	static const unsigned int kMaxRefetchBytes = 6 * 1024 * 1024;

	// The runtime paths, in the folded spelling IsRuntimePath compares against. A table rather than a
	// chain of tests so the set is legible in one place -- and so it is obvious that it must stay in step
	// with the same list in tools/make-update-package.py, which keeps them out of the manifest.
	static const char* kRuntimeFolders[] = { "SAVE/", "CONFIG/", "PERFPROBE.LMSDD/" };
	static const char* kRuntimeFiles[]   = { "LOG.TXT", "TRACE.TXT", "BOOT.TXT" };

	bool IsRuntimePath(const std::string& path)
	{
		std::string folded = FoldPath(path);
		while (!folded.empty() && (folded[0] == '/' || folded[0] == '.'))
		{
			folded.erase(0, 1);
		}

		for (unsigned int i = 0; i < sizeof(kRuntimeFolders) / sizeof(kRuntimeFolders[0]); ++i)
		{
			const std::string prefix(kRuntimeFolders[i]);   // includes the trailing '/'
			// Strictly longer than the prefix, so the bare folder name is not a file inside it.
			if (folded.size() > prefix.size() && folded.compare(0, prefix.size(), prefix) == 0)
			{
				return true;
			}
		}

		for (unsigned int i = 0; i < sizeof(kRuntimeFiles) / sizeof(kRuntimeFiles[0]); ++i)
		{
			if (folded == kRuntimeFiles[i])
			{
				return true;
			}
		}

		return false;
	}

	void CollectArchiveDiff(const TreeScan& scan, const Manifest& record,
							const std::vector<RemoteEntry>& index, std::vector<std::string>* out)
	{
		if (out == 0)
		{
			return;
		}
		out->clear();

		// The record, folded and indexed: it is both the "what should be here" list and the only cheap
		// source of a file's CONTENT. Pointers rather than copies because it is ~1500 entries and this
		// runs while a status line is waiting on it.
		std::map<std::string, const FileEntry*> known;
		for (unsigned int i = 0; i < (unsigned int)record.files.size(); ++i)
		{
			known[FoldPath(record.files[i].path)] = &record.files[i];
		}

		unsigned int absent = 0;
		unsigned int wrongLength = 0;
		unsigned int changedContent = 0;

		for (unsigned int i = 0; i < (unsigned int)index.size(); ++i)
		{
			const RemoteEntry& e = index[i];

			// The record is fetched separately and must not be counted as a file to repair. Runtime
			// paths are not in the archive any more, but an older release may still carry them, and
			// wrapping the archive in the same exclusion keeps the two sources of truth agreeing.
			if (FoldPath(e.name) == FoldPath(InstallManifestName()) || IsRuntimePath(e.name))
			{
				continue;
			}

			std::map<std::string, unsigned int>::const_iterator there =
				scan.sizes.find(FoldPath(e.name));

			if (there == scan.sizes.end())
			{
				++absent;
				out->push_back(e.name);
				continue;
			}

			if (there->second != e.uncompressedSize)
			{
				++wrongLength;
				out->push_back(e.name);
				continue;
			}

			// The same length, so only the record can say whether the content is the archive's. An entry
			// the record does not mention at all is one this install has never had: the lengths matching
			// is a coincidence, not evidence, so it is fetched.
			std::map<std::string, const FileEntry*>::const_iterator inRecord = known.find(FoldPath(e.name));
			if (inRecord == known.end() || inRecord->second->crc != e.crc)
			{
				++changedContent;
				out->push_back(e.name);
			}
		}

		Trace("repair: diff against the archive -- %u absent, %u wrong length, %u changed content "
			  "(%u entries in the archive, %u named by the record)",
			  absent, wrongLength, changedContent,
			  (unsigned int)index.size(), (unsigned int)record.files.size());
	}

	bool PlanRangeRepair(const std::vector<std::string>& badPaths, const std::vector<RemoteEntry>& index,
						 RepairPlan* out, std::string* error)
	{
		if (out == 0)
		{
			return false;
		}
		out->entries.clear();
		out->bytes = 0;
		out->usable = false;

		// Folded once, rather than per lookup: the card hands names back upper-cased and the archive
		// carries them as written on the PC, so every match has to be folded. The name reported is the
		// record's spelling, which is the one a person can find on a PC.
		std::map<std::string, unsigned int> byName;
		for (unsigned int i = 0; i < (unsigned int)index.size(); ++i)
		{
			byName[FoldPath(index[i].name)] = i;
		}

		for (unsigned int i = 0; i < (unsigned int)badPaths.size(); ++i)
		{
			std::map<std::string, unsigned int>::const_iterator found =
				byName.find(FoldPath(badPaths[i]));

			if (found == byName.end())
			{
				// Not "skip it". A repair that quietly omits a file leaves an install that still does not
				// work and no record of the reason, which is worse than a full download.
				if (error) { *error = "the archive has no entry for " + badPaths[i]; }
				return false;
			}

			const RemoteEntry& e = index[found->second];

			if (e.method != 0 && e.method != 8)
			{
				if (error) { *error = badPaths[i] + " is stored with method " + NumberToString((int)e.method) + ", which cannot be unpacked here"; }
				return false;
			}

			if (e.compressedSize > kMaxRefetchBytes)
			{
				if (error) { *error = badPaths[i] + " is " + NumberToString((int)(e.compressedSize / 1024)) + " KB compressed, over the refetch limit"; }
				return false;
			}

			out->entries.push_back(e);
			out->bytes += e.compressedSize;
		}

		// The release's own record, which the repair has to leave behind it. Required rather than
		// optional -- see the note on RepairPlan::record -- so an archive without one is a reason to
		// leave this path rather than to skip the step. Its bytes are part of the price quoted, because
		// they are fetched like anything else.
		std::map<std::string, unsigned int>::const_iterator rec =
			byName.find(FoldPath(InstallManifestName()));

		if (rec == byName.end())
		{
			if (error) { *error = "the archive carries no " + std::string(InstallManifestName()) + " to update the install's record with"; }
			return false;
		}

		out->record = index[rec->second];
		out->hasRecord = true;
		out->bytes += out->record.compressedSize;

		out->usable = true;

		Trace("repair: plan -- %u file(s) plus the record, %llu compressed bytes (%.1f KB) instead of the whole archive",
			  (unsigned int)out->entries.size(), out->bytes, (double)out->bytes / 1024.0);
		return true;
	}

	// One entry's bytes, inflated straight onto the card.
	//
	// Streaming rather than inflating into a buffer is not a refinement: the archive's largest entry is a
	// 13 MB WAV and this program's heap is 8 MB, so an approach that materialised the uncompressed side
	// would work on every file but the ones big enough to matter. Only the compressed bytes are held, and
	// they arrived over the wire, so that cost is bounded by the transfer rather than by the file.
	//
	// The write goes to "<path>.part" and is renamed into place only after the size and crc match, so a
	// transfer that dies mid-entry cannot leave a half-file where a working one used to be.
	static bool InflateEntryToFile(const unsigned char* in, unsigned int inLen,
								   const std::string& destPath, unsigned int expectSize,
								   unsigned int expectCrc, std::string* error)
	{
		std::string partPath = destPath + ".part";

		MakeParentDirectories(destPath);

		SceUID fd = sceIoOpen(partPath.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
		if (fd < 0)
		{
			if (error) { *error = "cannot write " + destPath; }
			Trace("repair: sceIoOpen failed on '%s': 0x%08X", partPath.c_str(), (unsigned int)fd);
			return false;
		}

		z_stream zs;
		memset(&zs, 0, sizeof(zs));

		if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)     // -MAX_WBITS: a zip entry is a raw deflate stream
		{
			sceIoClose(fd);
			sceIoRemove(partPath.c_str());
			if (error) { *error = "cannot start the decompressor"; }
			return false;
		}

		zs.next_in = (Bytef*)in;
		zs.avail_in = inLen;

		unsigned char out[8192];
		unsigned int written = 0;
		unsigned int crc = (unsigned int)crc32(0, 0, 0);
		bool failed = false;

		for (;;)
		{
			zs.next_out = out;
			zs.avail_out = sizeof(out);

			int step = inflate(&zs, Z_NO_FLUSH);
			if (step != Z_OK && step != Z_STREAM_END && step != Z_BUF_ERROR)
			{
				Trace("repair: inflate failed (%d) after %u bytes", step, written);
				failed = true;
				break;
			}

			unsigned int produced = (unsigned int)(sizeof(out) - zs.avail_out);

			if (produced > 0)
			{
				unsigned int put = 0;
				while (put < produced)
				{
					int now = sceIoWrite(fd, out + put, produced - put);
					if (now <= 0)
					{
						Trace("repair: sceIoWrite failed: 0x%08X", (unsigned int)now);
						failed = true;
						break;
					}
					put += (unsigned int)now;
				}
				if (failed)
				{
					break;
				}

				crc = (unsigned int)crc32(crc, out, produced);
				written += produced;

				// A stream that produces more than the directory promised is not this file, whatever it
				// decoded to. Stop rather than write the excess.
				if (written > expectSize)
				{
					Trace("repair: '%s' inflated past its promised %u bytes", destPath.c_str(), expectSize);
					failed = true;
					break;
				}
			}

			if (step == Z_STREAM_END)
			{
				break;
			}
			if (step == Z_BUF_ERROR && produced == 0)
			{
				break;                                  // no progress and no end: truncated input
			}
		}

		inflateEnd(&zs);
		sceIoClose(fd);

		if (failed || written != expectSize || crc != expectCrc)
		{
			sceIoRemove(partPath.c_str());
			if (error)
			{
				*error = destPath + " did not match the archive";
			}
			Trace("repair: '%s' rejected: %u of %u bytes, crc %08X vs %08X",
				  destPath.c_str(), written, expectSize, crc, expectCrc);
			return false;
		}

		sceIoRemove(destPath.c_str());                  // rename will not replace an existing name
		if (sceIoRename(partPath.c_str(), destPath.c_str()) < 0)
		{
			sceIoRemove(partPath.c_str());
			if (error) { *error = "cannot move " + destPath + " into place"; }
			return false;
		}

		Trace("repair: wrote '%s' (%u bytes, crc %08X)", destPath.c_str(), written, crc);
		return true;
	}

	// One entry out of the archive, fetched by range and inflated onto the card.
	//
	// The data does not start at a fixed offset from the entry: it follows the LOCAL header, whose name
	// and extra lengths are separate fields from the central directory's and are allowed to differ. So
	// one fetch is sized to cover the header plus the data, with room for an extra field, and the lengths
	// read out of the header are then CHECKED rather than assumed -- if the room turns out not to be
	// enough, the exact data range is fetched instead, now that it is known rather than guessed.
	static bool FetchAndInflateEntry(const std::string& url, unsigned int zipSize,
									 const RemoteEntry& e, const std::string& target,
									 std::string* error)
	{
		unsigned int span = 30 + (unsigned int)e.name.size() + 1024 + e.compressedSize;
		if (span > zipSize)
		{
			span = zipSize;
		}

		unsigned long long from = e.localHeaderOffset;
		unsigned long long to = from + span - 1;
		if (to >= zipSize)
		{
			to = (unsigned long long)zipSize - 1;
		}

		std::string chunk;
		if (!HttpGetRangeToString(url, (long)from, (long)to, &chunk, span + 64, 0, 0))
		{
			if (error) { *error = "could not fetch " + e.name; }
			return false;
		}

		const unsigned char* p = (const unsigned char*)chunk.data();

		if (chunk.size() < 30 || ReadU32(p) != 0x04034B50u)
		{
			if (error) { *error = "the archive's header for " + e.name + " is unreadable"; }
			return false;
		}

		const unsigned int nameLen = ReadU16(p + 26);
		const unsigned int extraLen = ReadU16(p + 28);
		const unsigned int dataStart = 30 + nameLen + extraLen;

		if (chunk.size() < (size_t)dataStart + e.compressedSize)
		{
			unsigned long long dataFrom = from + dataStart;
			if (!HttpGetRangeToString(url, (long)dataFrom, (long)(dataFrom + e.compressedSize - 1),
									  &chunk, e.compressedSize + 64, 0, 0))
			{
				if (error) { *error = "could not fetch " + e.name; }
				return false;
			}
			if (chunk.size() < e.compressedSize)
			{
				if (error) { *error = "the archive sent a short entry for " + e.name; }
				return false;
			}
			p = (const unsigned char*)chunk.data();
		}
		else
		{
			p = p + dataStart;
		}

		// Uncompressed entries are copied rather than inflated; they are rare here (the whole release is
		// deflated) but writing one through inflate() would fail rather than degrade.
		if (e.method == 0)
		{
			MakeParentDirectories(target);

			std::string part = target + ".part";
			SceUID fd = sceIoOpen(part.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
			if (fd < 0)
			{
				if (error) { *error = "cannot write " + target; }
				return false;
			}

			unsigned int put = 0;
			while (put < e.compressedSize)
			{
				int now = sceIoWrite(fd, p + put, e.compressedSize - put);
				if (now <= 0) { break; }
				put += (unsigned int)now;
			}
			sceIoClose(fd);

			if (put != e.compressedSize || (unsigned int)crc32(crc32(0, 0, 0), p, e.compressedSize) != e.crc)
			{
				sceIoRemove(part.c_str());
				if (error) { *error = target + " did not match the archive"; }
				return false;
			}
			sceIoRemove(target.c_str());
			if (sceIoRename(part.c_str(), target.c_str()) < 0)
			{
				sceIoRemove(part.c_str());
				if (error) { *error = "cannot move " + target + " into place"; }
				return false;
			}
			Trace("repair: wrote '%s' (%u bytes stored, crc %08X)", target.c_str(), put, e.crc);
			return true;
		}

		return InflateEntryToFile(p, e.compressedSize, target, e.uncompressedSize, e.crc, error);
	}

	bool ApplyRangeRepair(const std::string& url, unsigned int zipSize,
						  const std::string& installFolder, const RepairPlan& plan,
						  ProgressFn progress, void* ctx, std::string* error)
	{
		if (!plan.usable)
		{
			if (error) { *error = "there is no plan to apply"; }
			return false;
		}

		unsigned long long done = 0;

		for (unsigned int i = 0; i < (unsigned int)plan.entries.size(); ++i)
		{
			const RemoteEntry& e = plan.entries[i];

			if (!FetchAndInflateEntry(url, zipSize, e, installFolder + "/" + e.name, error))
			{
				return false;
			}

			done += e.compressedSize;

			if (progress != 0 && !progress(ctx, (unsigned int)done, (unsigned int)plan.bytes))
			{
				// Between entries is the only safe place to stop: mid-entry there is a partial file and
				// no way to tell it from a finished one.
				if (error) { *error = "cancelled"; }
				return false;
			}
		}

		// The record, LAST, so that the tree and its own description of itself change as near together
		// as this can manage. Without it the check would measure the files just written against the
		// previous release's sizes, flag them, and offer the same repair again -- see RepairPlan::record.
		if (!plan.hasRecord)
		{
			if (error) { *error = "the plan carries no record to update the install with"; }
			return false;
		}

		const std::string recordPath = installFolder + "/" + InstallManifestName();
		if (!FetchAndInflateEntry(url, zipSize, plan.record, recordPath, error))
		{
			return false;
		}

		Trace("repair: applied %u file(s), %llu compressed bytes fetched; the record is now the release's",
			  (unsigned int)plan.entries.size(), done);
		return true;
	}
}
