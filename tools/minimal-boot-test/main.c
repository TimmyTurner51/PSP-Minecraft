/*
 * Minimal boot test for the PSP -- baseline and single-variable variants.
 *
 * Why this exists
 * ---------------
 * When a homebrew EBOOT produces no log and no screen, the game itself is only one of two possible
 * causes, and from the game alone the two are indistinguishable:
 *
 *   1. the game's own build is at fault, or
 *   2. this console cannot launch a PSPSDK user-mode EBOOT built with this toolchain at all.
 *
 * The default (variant A) build answers question 2 and nothing else. It is deliberately tiny: it
 * imports nothing beyond the kernel, reads no data files, allocates a small fixed heap and asks for no
 * expanded memory, so if it will not start then the game's contents are ruled out and the toolchain or
 * the custom firmware is the subject.
 *
 * The other two variants exist for the case where A *does* start -- which is the situation this was
 * written in. A working A means the fault is in the game's build, and the next question is which of its
 * module settings does it. Those settings are the only ones that can stop a module before it executes a
 * single instruction of its own, so they are the ones worth isolating, one at a time:
 *
 *   variant A   vanilla                        main thread USER, 64 KB stack, 1 MB heap, MEMSIZE=2
 *   variant B   the game's module settings      main thread VFPU|USER, 512 KB stack, heap = all
 *                                               remaining minus 2 MB, PSP_FW_VERSION=371 so MEMSIZE=1
 *   variant C   only the VFPU thread attribute  A's settings, but the main thread asks for VFPU
 *
 * B and C are not arbitrary: they change the three globals the C runtime reads when it creates the main
 * thread and the heap (see pspmoduleinfo.h). If that creation fails, the module still "loads" -- the
 * console reports nothing and nothing is drawn, because the failure happens before any of the game's
 * code is ever entered. That is exactly the symptom being chased. C isolates the single most unusual of
 * the three, because THREAD_ATTR_VFPU is the one that is not present in a default build.
 *
 * NOTE: MEMSIZE is not set from here. It is derived by the SDK from PSP_FW_VERSION and
 * PSP_LARGE_MEMORY (the EXPAND_MEMORY block in build.mak) and baked into the PARAM.SFO, so variant B
 * needs those on the make command line. See the Makefile.
 *
 * Reading the results
 * -------------------
 * Each variant writes files named for itself into its own folder, so one test session identifies the
 * cause without needing another round trip:
 *
 *   A: boot.txt   / Log.txt
 *   B: bootB.txt  / LogB.txt
 *   C: bootC.txt  / LogC.txt
 *
 *   bootX.txt present   the module was loaded, its constructors ran, and main() was reached
 *   bootX.txt only      the module loaded and its constructors ran, but it died before main()
 *   neither             the console refused that module before any of its own code ran
 *
 * So: if B produces no files, the game's module settings are the cause and C says whether it is
 * specifically the VFPU attribute. If B works but the game does not, the settings are exonerated and
 * the cause is in what the game's build emits -- size, or the sections this newer toolchain produces.
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspiofilemgr.h>
#include <pspctrl.h>
#include <pspdisplay.h>

/* ---------------------------------------------------------------------------
 * The three variants. Only the module-level settings differ.
 * ------------------------------------------------------------------------- */

#if defined(VARIANT_GAME_SETTINGS)

	#define VARIANT_NAME "B"
	#define MARKER_BOOT  "bootB.txt"
	#define MARKER_LOG   "LogB.txt"
	#define VARIANT_DESC "B: the game's module settings"

	/* Exactly what Scripts.cpp declares for the game. */
	PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);
	PSP_MAIN_THREAD_STACK_SIZE_KB(512);
	PSP_HEAP_SIZE_KB(-2048);

#elif defined(VARIANT_VFPU)

	#define VARIANT_NAME "C"
	#define MARKER_BOOT  "bootC.txt"
	#define MARKER_LOG   "LogC.txt"
	#define VARIANT_DESC "C: VFPU thread attribute only"

	/* A's settings, with just the main thread attribute changed. */
	PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);
	PSP_MAIN_THREAD_STACK_SIZE_KB(64);
	PSP_HEAP_SIZE_KB(1024);

#else

	#define VARIANT_NAME "A"
	#define MARKER_BOOT  "boot.txt"
	#define MARKER_LOG   "Log.txt"
	#define VARIANT_DESC "A: vanilla (baseline)"

	PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
	PSP_MAIN_THREAD_STACK_SIZE_KB(64);
	PSP_HEAP_SIZE_KB(1024);

#endif

PSP_MODULE_INFO("MCBootTest", 0, 1, 1);

/* ---------------------------------------------------------------------------
 * Getting out again.
 *
 * Registering these is not optional for a test that parks itself on screen: without an exit callback
 * the HOME button does nothing, and the only way off the screen is to hold the power switch until the
 * console gives up. That is an annoying thing to discover while holding a PSP, so the callback is
 * registered as the very first act of main() -- before the debug screen is even set up -- so there is a
 * way out even if the display setup is the part that misbehaves. A button is polled as well, after the
 * screen is up, because the explicit way out should not depend on the HOME button either.
 * ------------------------------------------------------------------------- */

static int ExitCallback(int arg1, int arg2, void* common)
{
	sceKernelExitGame();
	return 0;
}

static int CallbackThread(SceSize args, void* argp)
{
	int cbid = sceKernelCreateCallback("Exit Callback", ExitCallback, 0);
	sceKernelRegisterExitCallback(cbid);

	sceKernelSleepThreadCB();

	return 0;
}

static void SetupCallbacks()
{
	int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, THREAD_ATTR_USER, 0);
	if(thid >= 0)
		sceKernelStartThread(thid, 0, 0);
}

static void WriteFile(const char* path, const char* text)
{
	SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if(fd < 0)
		return;

	unsigned int len = 0;
	while(text[len] != '\0')
		len++;

	sceIoWrite(fd, text, len);
	sceIoClose(fd);
}

/*
 * The first constructor. Written here rather than in main() because "did the module even get loaded and
 * start running" is the question, and a constructor runs after loading but before main. It is also the
 * half that catches a failure to create the main thread, since the constructor runs on that thread.
 */
__attribute__((constructor))
static void EarliestBootMarker()
{
	WriteFile(MARKER_BOOT, "boot marker: variant " VARIANT_NAME
		" loaded, first constructor ran (" VARIANT_DESC ")\n");
}

int main(void)
{
	/* First, so there is always a way out (see the note above SetupCallbacks). */
	SetupCallbacks();

	WriteFile(MARKER_LOG, "main() reached in variant " VARIANT_NAME "\n");

	pspDebugScreenInit();
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

	pspDebugScreenPrintf("MCPSP boot test -- variant %s\n\n", VARIANT_NAME);
	pspDebugScreenPrintf("%s\n\n", VARIANT_DESC);
	pspDebugScreenPrintf("main() reached, so this console started a\n");
	pspDebugScreenPrintf("module with these settings.\n\n");
	pspDebugScreenPrintf("%s and %s were written next to\n", MARKER_BOOT, MARKER_LOG);
	pspDebugScreenPrintf("this EBOOT.\n\n");
	pspDebugScreenPrintf("Press X (or HOME) to exit.\n");

	/* Stay up so the screen can be photographed or read, but leave a way out. */
	SceCtrlData pad;
	for(;;)
	{
		sceCtrlReadBufferPositive(&pad, 1);

		if(pad.Buttons & PSP_CTRL_CROSS)
			break;
		if(pad.Buttons & PSP_CTRL_START)
			break;

		sceDisplayWaitVblankStart();
	}

	sceKernelExitGame();

	return 0;
}
