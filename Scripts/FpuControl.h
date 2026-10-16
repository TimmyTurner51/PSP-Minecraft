#ifndef MC_FPUCONTROL_H
#define MC_FPUCONTROL_H

// ---------------------------------------------------------------------------
// Clearing the FPU exception enables, and why the game dies on hardware without it.
//
// FCR31 is the FPU's control/status register. Bits 7..11 are the exception *enables*, one per
// exception, and a console leaves three of them set when a module starts:
//
//     bit 9  overflow            result is larger than the format can express
//     bit 10 divide by zero      1.0f / 0.0f
//     bit 11 invalid operation   0.0f / 0.0f, inf - inf, 0 * inf, sqrtf of a negative
//
//     bit 7  inexact             free
//     bit 8  underflow           free
//
// Measured, not assumed: boot.txt records the register as the module finds it, and on 6.61 the
// value is 0x00000E00 -- exactly those three bits (see Scripts/BootTrace.cpp). Nothing in this
// game or in pspsdk's startup writes it, so that is what the console hands over.
//
// With those three enabled, an operation that hits one of those conditions raises a hardware
// exception instead of quietly producing inf or nan. The default handler does not return: it
// spins, and the console powers off. The symptom is a black screen a few milliseconds in and no
// log on the card, which is indistinguishable from a module that never loaded.
//
// PPSSPP does not implement FPU exceptions at all -- its own author lists that as an accuracy gap
// affecting homebrew (hrydgard/ppsspp#22098). So this game was written, tuned and tested in an
// environment where 0.0f/0.0f is nan, and then shipped to a console where it is fatal. Matching
// the emulator is the fix: the arithmetic keeps its IEEE result and the game keeps running, which
// is the behaviour every other build of this code has ever been tested against.
//
// WHY THIS IS CALLED FROM MORE THAN ONE PLACE
//
// COP1 control is part of a thread's saved context, and a new thread is not guaranteed to start
// from the value this module set on the main thread. Clearing it once in a constructor therefore
// protects only the thread that constructor ran on. Every thread entry point that does float work
// calls this on entry, so the whole game runs in the environment it was developed in.
//
// Both halves are cheap -- a register read and write -- and idempotent: on a thread that has
// already been cleared they change nothing.
// ---------------------------------------------------------------------------

// PSP_FPU_ENABLE_MASK, from pspsdk's pspfpu.h: bits 7..11, the five exception enables. Defined
// here rather than included so this header stays dependency-free and usable from plain C.
#define MC_FPU_EXCEPTION_ENABLE_MASK 0x00000F80u

// Reads FCR31 without changing it.
static inline unsigned int MC_ReadFcr31(void)
{
	unsigned int value;
	__asm__ volatile("cfc1 %0, $31" : "=r"(value));
	return value;
}

// Clears the exception enables and returns the value that was there before the call, so a caller
// can tell whether this console needed it (see the boot marker in Scripts/BootTrace.cpp). The
// rounding mode, the flush-to-zero bit and the sticky exception flags are left alone.
static inline unsigned int MC_DisableFpuExceptions(void)
{
	unsigned int before = MC_ReadFcr31();
	unsigned int cleared = before & ~MC_FPU_EXCEPTION_ENABLE_MASK;
	__asm__ volatile("ctc1 %0, $31" : : "r"(cleared));
	return before;
}

#endif
