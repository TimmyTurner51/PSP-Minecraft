// ---------------------------------------------------------------------------
// Boot breadcrumbs. See BootTrace.h for why this is not an exception handler.
//
// The whole file is a static buffer and one comparison, because it exists to be called from the parts
// of startup that are most likely to be the thing that is broken.
// ---------------------------------------------------------------------------

#include "BootTrace.h"

#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <string.h>
#include <malloc.h>

#include "DebugConfig.h"
#include "FpuControl.h"
#include "InitTrace.h"

namespace
{
	// The console's own value of FCR31, and what was written back over it. The reason these are
	// recorded rather than just cleared is in Scripts/FpuControl.h: an emulator cannot show this
	// difference, so the console has to be asked.
	//
	// This runs in a constructor, so it must be self-contained: no newlib, no printf.
	unsigned int gFcr31Before = 0;
	unsigned int gFcr31After = 0;

	// Appends value as eight lower-case hex digits.
	char* AppendHex(char* out, unsigned int value)
	{
		static const char kDigits[] = "0123456789abcdef";
		int shift = 28;
		while(shift >= 0)
		{
			*out++ = kDigits[(value >> shift) & 0xF];
			shift -= 4;
		}
		return out;
	}

	// sceIoClose does not put the memory stick's cached directory and FAT entries on the card -- the
	// console writes them back lazily. A hard crash, which is what an unhandled hardware exception
	// produces, therefore loses the files that were created moments before it, which is precisely
	// when a breadcrumb matters. This is not a theoretical concern here: a crash that discards
	// boot.txt is indistinguishable, on the card, from a module that was never executed at all, and
	// that ambiguity is what the whole file exists to avoid.
	//
	// Called straight after the writes rather than on a timer, so what survives a power-off is
	// current as of the last breadcrumb and no later. msfat0: is the layer the FAT itself lives at;
	// the call is a no-op returning an error if the name is not a device on this firmware.
	void FlushCard()
	{
		sceIoSync("ms0:", 0);
		sceIoSync("msfat0:", 0);
	}
}

// ---------------------------------------------------------------------------
// The earliest marker the module can leave.
//
// Everything else -- Log.txt being cleared, every singleton being constructed, main() running at all --
// happens after the C++ global constructors have finished. On a console that stops before writing
// anything, that leaves the most important question open: did this EBOOT execute any of its own code,
// or was it refused at load time?
//
// A constructor with no priority, and Scripts/BootTrace.o listed *last* in the Makefile's OBJS, which
// is what makes it run ahead of every other global in the game (Matrix3's statics, RenderManager,
// TextureHelper, SoundManager). The order of that list reads backwards on purpose -- the runtime walks
// .ctors from the end to the start -- and the Makefile carries the measurement that established it
// rather than leaving it as an assumption, because the original arrangement relied on the opposite and
// was wrong.
//
// It writes its own file rather than adding a line to Log.txt, because MC_BootSession truncates Log.txt
// later and would erase the evidence.
//
// Reading the result afterwards:
//
//   no boot.txt            the module never ran its own code. The console refused the EBOOT -- wrong
//                          firmware, a bad PARAM.SFO, a corrupt file -- so the fault is in the package
//                          or the console, and not in anything below. The file is flushed to the card
//                          before this returns, so this reading no longer has to allow for a write
//                          that was lost in the crash.
//   boot.txt, no Log.txt   a global constructor faulted. The game died before main(), which is exactly
//                          the case the breadcrumbs in this file cannot see.
//   boot.txt + Log.txt     main() ran. The last "stage:" line names the step that did not finish.
//
// The fcr31 values on the second line are the console's FPU control register before and after the
// exception enables were cleared. Equal values mean this firmware ships with the enables already
// clear, and the FPU is not the problem. A before value with bits 7..11 set means every inexact float
// operation on this console traps, and PPSSPP could never have shown that.
//
// This costs one file write at startup and is worth it for the way it splits the search: every "it does
// not start" report falls into one of those three buckets, and they have nothing in common.
// ---------------------------------------------------------------------------
__attribute__((constructor))
static void EarliestBootMarker()
{
	// The very first thing this module records anywhere. It opens trace.txt (see Scripts/InitTrace.h);
	// if no trace.txt exists afterwards, nothing in the game's own code ran at all.
	MC_Trace("boot: first constructor entered");

	// Before the file write, because a trap can only be prevented before the first float instruction.
	// The return value is what the console had set, which is the whole point of recording it.
	MC_TraceUInt("boot: fcr31 as handed over", MC_ReadFcr31());
	gFcr31Before = MC_DisableFpuExceptions();
	gFcr31After = MC_ReadFcr31();
	MC_TraceUInt("boot: fcr31 after clear", gFcr31After);
	MC_Trace("boot: writing boot.txt");

	// Heap state at the earliest moment this module can report it.
	//
	// The first dynamic allocation in the whole program happens a few constructors later, inside
	// TextureHelper's constructor -- its first std::string member is one character longer than
	// libstdc++'s small-string buffer, so it is the first thing that reaches operator new (see
	// Scripts/TextureHelper.cpp). That makes the heap the prime suspect for a console that stops in
	// the constructor phase, and this is the only place the heap's state can be seen before anything
	// has allocated from it. A zero or implausible arena here, on a run that then dies in that
	// constructor, is the whole answer.
	struct mallinfo bootHeap = mallinfo();
	MC_TraceUInt("boot: heap arenaKB", (unsigned int)(bootHeap.arena / 1024));
	MC_TraceUInt("boot: heap freeKB", (unsigned int)(bootHeap.fordblks / 1024));

	// Memory-mode probe. This is what decides whether "this console thinks it is a PSP-1000" is a bug
	// in the game or a fact about the console.
	//
	// The C runtime creates the heap before this constructor runs, sized "largest free block minus the
	// threshold" (PSP_HEAP_THRESHOLD_SIZE_KB(2048) in Scripts.cpp). A PSP-2000/3000 running an EBOOT that asks for
	// MEMSIZE=1 is granted 64 MB -- but that extra 32 MB is a separate physical block. If the runtime
	// sized the heap from the *largest single block* rather than the total, the second block can be
	// left sitting outside the heap entirely, and the game would then correctly conclude from its own
	// free memory that it is on a 32 MB machine while sitting inside a 64 MB one.
	//
	// So ask the kernel rather than infer: report what is still free, and try to take a block of each
	// size from the user partition, releasing each immediately. A large block that can still be taken
	// here is memory the game is not using; if none can, the heap really did take everything.
	//
	// Reading it:
	//   totalFreeKB > 2048    there is RAM outside the heap -> the game can and should use it
	//   largestBlockKB > 0    the same thing, proven allocatable rather than merely reported
	//   both at the 2048 reserve, and the game's own heap ~18 MB, on a 2000/3000 -- the console is not
	//                         granting MEMSIZE=1, and the low-memory path is the correct response
	MC_TraceUInt("boot: kernel totalFreeKB", (unsigned int)(sceKernelTotalFreeMemSize() / 1024));
	MC_TraceUInt("boot: kernel maxFreeKB", (unsigned int)(sceKernelMaxFreeMemSize() / 1024));

	static const int kProbeKB[] = {1024, 4096, 16384, 32768};
	unsigned int largestBlockKB = 0;
	for(unsigned int i = 0; i < sizeof(kProbeKB) / sizeof(kProbeKB[0]); i++)
	{
		SceUID block = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "memprobe",
		                                             PSP_SMEM_Low, (unsigned int)kProbeKB[i] * 1024, NULL);
		if(block >= 0)
		{
			largestBlockKB = (unsigned int)kProbeKB[i];
			sceKernelFreePartitionMemory(block);
		}
	}
	MC_TraceUInt("boot: outside-heap blockKB", largestBlockKB);

	static const char kHead[] = "boot.txt: module loaded, first constructor running\nfcr31 before=";
	static const char kMiddle[] = " after=";
	static const char kEnd[] = "\n";

	char line[sizeof(kHead) + sizeof(kMiddle) + sizeof(kEnd) + 16];
	char* cursor = line;
	memcpy(cursor, kHead, sizeof(kHead) - 1);
	cursor += sizeof(kHead) - 1;
	cursor = AppendHex(cursor, gFcr31Before);
	memcpy(cursor, kMiddle, sizeof(kMiddle) - 1);
	cursor += sizeof(kMiddle) - 1;
	cursor = AppendHex(cursor, gFcr31After);
	memcpy(cursor, kEnd, sizeof(kEnd) - 1);
	cursor += sizeof(kEnd) - 1;

	SceUID fd = sceIoOpen("boot.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if(fd < 0)
	{
		MC_Trace("boot: boot.txt open FAILED");
		return;
	}

	sceIoWrite(fd, line, (unsigned int)(cursor - line));
	sceIoClose(fd);
	FlushCard();
	MC_Trace("boot: boot.txt written and flushed");
}

namespace
{
	// 96 characters: long enough for a full asset path -- Assets/Textures/Minecraft/entity/sheep.png is
	// 50 -- because naming the exact file that was being read is more useful than naming the function,
	// and short enough that the buffer stays trivially safe.
	char gStage[96] = "nothing recorded";
}

namespace BootTrace
{
	void Stage(const char* name)
	{
		if(name == 0)
			return;

		// Leave a stage that is already current alone. The menu's Draw calls this every frame; without
		// this check the log would gain a line per frame, and with it each milestone is recorded once,
		// when it is first reached -- which is what a breadcrumb is for.
		if(strcmp(name, gStage) == 0)
			return;

		// Bounded manual copy rather than strncpy, so the terminator is guaranteed even if the name is
		// longer than the buffer.
		unsigned int i = 0;
		while(name[i] != '\0' && i < sizeof(gStage) - 1)
		{
			gStage[i] = name[i];
			i++;
		}
		gStage[i] = '\0';

		// On the card before this returns, so a reset one instruction later cannot lose it.
		MC_DEBUG_LOG("stage: %s\n", name);
	}
}
