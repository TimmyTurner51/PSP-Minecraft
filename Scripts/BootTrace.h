#ifndef BOOTTRACE_H_
#define BOOTTRACE_H_

// Records how far the game got, so a crash on real hardware can be located afterwards.
//
// WHY THERE IS NO STALL SCREEN HERE
//
// This started as pspDebugInstallErrorHandler(NULL), which is the usual advice and does work -- but
// not for this game. The call ends up in sceKernelRegisterDefaultExceptionHandler, which lives in
// libpspkernel, and pspexception.h describes that header as "the kernel side of ExceptionMan". A
// user-mode module cannot link it: the link fails with
//
//     libpspdebug.a(exception.o): in function `pspDebugInstallErrorHandler':
//     exception.c:33: undefined reference to `sceKernelRegisterDefaultExceptionHandler'
//
// and the SDK's own exception sample confirms why -- it declares itself a kernel module,
// PSP_MODULE_INFO("EXTEST", 0x1000, 1, 1), where 0x1000 is PSP_MODULE_KERNEL. This game is
// PSP_MODULE_USER. An in-game stall screen is therefore not available to it short of rebuilding the
// whole game as a kernel module, which risks far more than it would diagnose.
//
// WHAT IS AVAILABLE INSTEAD
//
// The log. Aurora::Utils::Logger opens Log.txt, writes the line and closes the file before returning,
// so there is no stdio buffer to lose. Closing alone was not enough, though: the console keeps the
// memory stick's directory and FAT entries in a cache and writes them back lazily, so a crash that
// powers the console off took the newest lines -- and the marker file -- with it. Logger and the
// marker below now force that cache to the card (sceIoSync) before returning, so what is on the card
// really is current as of the last completed breadcrumb. The trick is only to record the milestone
// *before* the risky work rather than after it, so the final line names the step that did not finish.
// That turns "it crashes at startup" into "it stopped while reading this file", which is the question
// that actually needs answering.
//
// A log can only describe code that ran, though, and the first thing this game did was produce no log
// at all. That is its own answer, and it is why BootTrace.cpp also drops a marker file from a
// constructor that runs before main() and before every other global: it separates "the console refused
// the EBOOT" from "a global constructor faulted" from "main() ran and stopped somewhere". See the
// comment above that constructor in BootTrace.cpp for how to read the three outcomes.

namespace BootTrace
{
	// Records the milestone just reached.
	//
	// Deliberately trivial -- a bounded copy into a static buffer plus one log line -- because it is
	// called from boot paths that are themselves under suspicion, and because it has to stay safe when
	// called every frame. The log line is written only when the name actually changes, so a per-frame
	// caller costs one comparison and adds nothing to the card.
	void Stage(const char* name);
}

#endif
