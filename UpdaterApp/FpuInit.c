/*
 * Clears the FPU exception enables before any other constructor runs.
 *
 * This is the updater's copy of what Scripts/BootTrace.cpp does for the game, and it is not optional on
 * hardware. FCR31's bits 7..11 are the FPU exception enables, and a 6.61 console hands three of them
 * set -- boot.txt records the value this project measured on this console, 0x00000E00. With them set,
 * an operation that overflows, divides by zero or is otherwise invalid raises a hardware exception
 * instead of quietly producing inf or nan, and the default handler does not return: it spins and the
 * console powers off.
 *
 * That is what this program's first hardware run looked like -- four lines in Log.txt, then nothing,
 * then a console back in the CFW's USB mode with no further record. The game does not have this problem
 * only because BootTrace.cpp clears the register in a constructor, and this program links none of that
 * file. Scripts/FpuControl.h has the full account, including why PPSSPP never reproduces it.
 *
 * WHERE THIS SITS IN THE CONSTRUCTOR ORDER
 *
 * The game runs its clear from BootTrace.o, which MakefileGame places LAST in OBJS, because the runtime
 * walks .ctors backwards and the last object listed is therefore the first constructor to run. This
 * file is placed last for the same reason and by the same mechanism (see UpdaterApp/Makefile).
 *
 * That ordering is belt and braces on top of the call at the top of main(), which is the one that
 * actually matters: main() clears again before it does anything else, so the value is right however the
 * constructors happen to end up ordered.
 */

#include "FpuControl.h"

// What the register held on entry, and what it held afterwards. Kept so main() can put both in the log:
// "before" is the evidence that this is the console's value rather than something this program set, and
// having both makes the fix verifiable instead of merely plausible.
static unsigned int g_fcr31Before = 0xFFFFFFFFu;
static unsigned int g_fcr31After = 0xFFFFFFFFu;

__attribute__((constructor)) static void McUpdaterClearFpuExceptions(void)
{
	g_fcr31Before = MC_DisableFpuExceptions();
	g_fcr31After = MC_ReadFcr31();
}

// C linkage, because main.cpp is C++ and declares these itself the same way.
unsigned int McUpdaterFcr31Before(void) { return g_fcr31Before; }
unsigned int McUpdaterFcr31After(void) { return g_fcr31After; }
