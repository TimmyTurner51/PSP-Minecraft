#ifndef HTTPS_CLIENT_H_
#define HTTPS_CLIENT_H_

#include "Updater.h"

#include <pspiofilemgr.h>
#include <string>

namespace Update
{
	// Same contract as the sceHttp Get in Updater.cpp, for https:// only.
	//
	// sceSsl cannot complete a handshake with GitHub (SCE_HTTP_ERROR_SSL / 0x80431075): the
	// console's built-in TLS stops short of the TLS 1.2 + modern ECDHE suites those hosts
	// require. This path speaks TLS 1.2 itself, with mbedtls and the bundled public roots,
	// so a shipping card can Check for updates with no extra file or player step.
	bool HttpsGet(const std::string& url, std::string* toMemory, SceUID* toFile,
				  ProgressFn progress, void* ctx, unsigned int* outSize,
				  long rangeStart, long rangeEnd, unsigned int maxMemoryBytes);

	// Shared log + LastHttpError sink. Defined in Updater.cpp (the same Trace the rest of
	// the updater uses, so a failed GitHub fetch names itself on the "could not reach" dialog).
	void UpdaterTrace(const char* fmt, ...);
}

#endif
