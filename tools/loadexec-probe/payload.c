/*
 * The payload for the launcher probe. It has exactly two jobs: record that it was reached, and exit.
 *
 * It deliberately does nothing else. It does not read, write, create, rename or delete anything except
 * its own two marker files. It never touches the game, and it cannot: it opens no path it was not given.
 * A handoff that works has been demonstrated the moment these files appear, so there is no reason for
 * this program to be capable of harm -- and every reason for it not to be, since the whole point is to
 * test a mechanism before anything is built on top of it.
 *
 * WHY THERE ARE TWO MARKERS
 * -------------------------
 * A launch that gets as far as loading this module but dies before main() -- a failed main-thread or
 * heap creation, which this project has been bitten by before, or the heap being smaller in whatever
 * context loadexec creates -- is indistinguishable from "never launched at all" if only main() writes
 * anything. So:
 *
 *   loadexec-payload-ctor.txt   written by a constructor, which runs after the module is loaded and
 *                               before main() is entered. Present = the module was loaded and started.
 *   loadexec-payload.txt        written by main(). Present = the handoff works end to end.
 *
 * ctor only  ->  loading works, starting does not. That is a fixable, specific problem.
 * neither    ->  the file was never loaded. A path or apitype problem, not this program's fault.
 * both       ->  done.
 *
 * WHY IT LOGS ITS ARGV AND CWD
 * ----------------------------
 * args/argp are the only channel by which a launcher can tell the launched program anything, and that
 * is what a real updater would need -- it has to be told which package to apply and where. The SDK
 * documents the field as "Size of the argument string" / "Pointer to the arg string", which reads as
 * "this becomes the new program's argv", but the widely-copied snippet for the other API passes the
 * executable path there instead, which would mean something quite different. Rather than pick a
 * reading, this prints what it actually received.
 *
 * cwd matters for the same reason from the other side: after a loadexec reboot the launched program's
 * working directory is not the one its launcher had, which is why everything here is written to ms0:/
 * by absolute path and why run 2's relative-path attempt could not have worked.
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspiofilemgr.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psploadexec.h>
#include <pspinit.h>
#include <systemctrl.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

/* Same baseline module settings as the probe, for the same reason: this is an instrument, not a
 * product, so it should be the least unusual module the toolchain can produce. */
PSP_MODULE_INFO("MCPSPLoadExecTarget", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(64);
PSP_HEAP_SIZE_KB(1024);

#define MARK_CTOR "ms0:/loadexec-payload-ctor.txt"
#define MARK_MAIN "ms0:/loadexec-payload.txt"

/*
 * THE SECOND-HOP EXPERIMENT.
 *
 * Everything the probe has proved so far is that the FIRST launcher works: the probe is started by the
 * XMB and calls loadexec once, and this program starts. The Minecraft Updater is in a different and
 * harder position -- it is ITSELF the product of a loadexec, and it is then asked to call the same
 * function again. That has now failed on hardware six times: the install completes, the log stops on
 * the launch line, and the console powers off.
 *
 * Six fixes were landed on the strength of plausible mechanisms and none of them was the cause, which
 * is the reason this experiment exists. The question is narrow and has two possible answers, and this
 * program is the cheapest thing that can produce either:
 *
 *   this program was launched BY loadexec, so calling sctrlKernelLoadExecVSHWithApitype from here is
 *   exactly the Updater's situation. If the probe starts again afterwards, a chained launch is possible
 *   and the Updater has a bug to find. If the console goes down, chaining is not possible from this
 *   position on this firmware, and no amount of fixing the Updater would have helped.
 *
 * The target is the PROBE, not the game, on purpose: the probe is tiny. Every launch this project has
 * got working was of a small file, and the one that fails is 4.9 MB, so launching something small
 * removes size from the question and leaves only the chained-ness.
 *
 * Both out comes leave a file. A refusal returns and is logged with its code; a working launch does not
 * return, so the evidence is that the probe runs again after this line and that this log has no line
 * after it. Nothing here can loop: the attempt is made once per session, guarded by a marker file, so
 * the payload the probe launches on its later cases does not try again.
 */
#define MARK_SECONDHOP "ms0:/loadexec-secondhop.txt"
#define MARK_ATTEMPTED "ms0:/loadexec-secondhop-attempted.txt"
#define MARK_RUNS "ms0:/loadexec-payload-runs.txt"

/* The probe, by the absolute path tools/loadexec-probe/deploy.ps1 puts it at. Absolute because a
 * relative path is the one thing the probe's own run 2 proved cannot work from this position: a
 * loadexec'd program has no inherited working directory. */
#define PROBE_ABS "ms0:/PSP/GAME/MCPSP LoadExec Probe/EBOOT.PBP"

static void WriteFile(const char* path, const char* text)
{
	SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0)
		return;

	unsigned int len = 0;
	while (text[len] != '\0')
		len++;

	sceIoWrite(fd, text, len);
	sceIoClose(fd);
}

/*
 * One line appended to a log, synced to the card before returning.
 *
 * The sync is not optional. This program's whole purpose is to leave a readable record of a step that
 * may take the console down, and this project has already measured that closing a file on the stick
 * does not write its directory or FAT entries -- which is exactly how a crash log comes to lose the one
 * line that matters. UpdaterApp/main.cpp depends on the same flush, and pgeZip.c was fixed for the same
 * reason.
 */
static void LogSecondHop(const char* format, ...)
{
	char buffer[512];
	va_list args;

	va_start(args, format);
	int length = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	if (length <= 0)
		return;

	SceUID fd = sceIoOpen(MARK_SECONDHOP, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
	if (fd < 0)
		return;

	sceIoWrite(fd, buffer, (unsigned int)length);
	sceIoClose(fd);

	sceIoSync("ms0:", 0);
	sceIoSync("msfat0:", 0);
}

static int FileExists(const char* path)
{
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return 0;

	sceIoClose(fd);
	return 1;
}

/*
 * How many times this program has been reached.
 *
 * The number that matters after a working chained launch is 2, and a counter is the only evidence that
 * survives being overwritten: the probe truncates its own log every run, so "the probe ran again" is
 * not visible in the probe's log, but "the payload was reached a second time" is visible here.
 */
static int CountThisLaunch()
{
	char buffer[32];
	int previous = 0;

	SceUID fd = sceIoOpen(MARK_RUNS, PSP_O_RDONLY, 0777);
	if (fd >= 0)
	{
		int got = sceIoRead(fd, buffer, sizeof(buffer) - 1);
		sceIoClose(fd);
		if (got > 0)
		{
			buffer[got] = '\0';
			previous = atoi(buffer);
		}
	}

	int current = previous + 1;
	int length = snprintf(buffer, sizeof(buffer), "%d\n", current);

	fd = sceIoOpen(MARK_RUNS, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd >= 0)
	{
		sceIoWrite(fd, buffer, (unsigned int)length);
		sceIoClose(fd);
	}

	return current;
}

static void TrySecondHop()
{
	if (FileExists(MARK_ATTEMPTED))
	{
		LogSecondHop("second hop: already attempted this session, not repeating\n");
		return;
	}

	WriteFile(MARK_ATTEMPTED, "the second hop was attempted\n");

	LogSecondHop("\n=== second hop ===\n");
	LogSecondHop("this program was launched BY loadexec, which is the Updater's position exactly\n");
	LogSecondHop("target: '%s'\n", PROBE_ABS);
	LogSecondHop("memory before the call: free=%u largest block=%u\n",
		sceKernelTotalFreeMemSize(), sceKernelMaxFreeMemSize());

	struct SceKernelLoadExecVSHParam vsh;
	memset(&vsh, 0, sizeof(vsh));
	vsh.size = sizeof(vsh);
	vsh.key = "game";
	vsh.unk5 = 0x10000;

	LogSecondHop("calling sctrlKernelLoadExecVSHWithApitype(0x141, target) -- the Updater's own call\n");

	int result = sctrlKernelLoadExecVSHWithApitype(PSP_INIT_APITYPE_MS2, PROBE_ABS, &vsh);

	/* Reached only if the call was REFUSED. A working launch never comes back, so this line's absence
	 * from the card -- next to the probe running again and the launch counter reading 2 -- is the proof
	 * that a chained launch works. */
	LogSecondHop("second hop: RETURNED 0x%08X -- a working launch never returns, so it refused\n",
		(unsigned int)result);
}

/*
 * The earliest marker. Written from a constructor rather than from main() because "did the module even
 * get loaded and started" is the question, and a constructor runs after loading but before main. It is
 * also the half that catches a failure to create the main thread, since the constructor runs on that
 * thread. Nothing is set up first -- not the display, not the callbacks -- so that if this file is
 * missing there is nothing left that could have written it.
 */
__attribute__((constructor))
static void EarliestMarker()
{
	WriteFile(MARK_CTOR, "the payload's module was loaded and its constructor ran\n");
}

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
	if (thid >= 0)
		sceKernelStartThread(thid, 0, 0);
}

int main(int argc, char* argv[])
{
	SetupCallbacks();

	pspDebugScreenInit();
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

	char cwd[256];
	cwd[0] = '\0';
	if (getcwd(cwd, sizeof(cwd)) == 0)
		cwd[0] = '\0';

	char report[1024];
	int written = 0;
	int i;

	/* Read once: this increments the counter, and the counter is the evidence a chained launch produced. */
	int launchNumber = CountThisLaunch();

	written += snprintf(report + written, sizeof(report) - written,
		"the payload reached main() -- the handoff WORKS on this console\n\n");
	written += snprintf(report + written, sizeof(report) - written,
		"launch number: %d  (2 or more means a chained launch got back here)\n", launchNumber);
	written += snprintf(report + written, sizeof(report) - written,
		"cwd  : \"%s\"\n", cwd[0] != '\0' ? cwd : "(failed)");
	written += snprintf(report + written, sizeof(report) - written,
		"argc : %d\n", argc);

	for (i = 0; i < argc && i < 8; ++i)
	{
		written += snprintf(report + written, sizeof(report) - written,
			"argv[%d]: \"%s\"\n", i, argv[i] ? argv[i] : "(null)");
	}

	WriteFile(MARK_MAIN, report);

	pspDebugScreenPrintf("sceKernelLoadExec PAYLOAD ran.\n\n");
	pspDebugScreenPrintf("The handoff WORKS on this console.\n\n");
	pspDebugScreenPrintf("launch number: %d\n", launchNumber);
	pspDebugScreenPrintf("cwd: %s\n", cwd[0] != '\0' ? cwd : "(failed)");
	pspDebugScreenPrintf("argc: %d\n", argc);
	for (i = 0; i < argc && i < 4; ++i)
		pspDebugScreenPrintf("argv[%d]: %s\n", i, argv[i] ? argv[i] : "(null)");

	SceCtrlData pad;
	sceKernelDelayThread(300000);

	/*
	 * The second hop is armed by a button rather than run automatically so that its outcome cannot be
	 * misread. This program has already been through a launcher once; the question is whether it can BE
	 * one. If the screen goes down from here, the answer is no, and it went down on the single line in
	 * TrySecondHop() rather than anywhere in the Updater -- which is a different thing to go and fix.
	 */
	if (!FileExists(MARK_ATTEMPTED))
	{
		pspDebugScreenPrintf("\nThis program was started BY loadexec.\n");
		pspDebugScreenPrintf("Press X to try launching the probe from here --\n");
		pspDebugScreenPrintf("which is the Updater's position exactly.\n");
		pspDebugScreenPrintf("If the console goes down, chaining cannot work.\n\n");
	}
	else
	{
		pspDebugScreenPrintf("\nThe second hop was already attempted\n");
		pspDebugScreenPrintf("this session. See ms0:/loadexec-secondhop.txt\n\n");
	}

	pspDebugScreenPrintf("Press X to continue.\n");

	for (;;)
	{
		sceCtrlReadBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_CROSS)
			break;
		sceDisplayWaitVblankStart();
	}

	/* The experiment. If it succeeds, the probe is on screen now and none of this runs. */
	TrySecondHop();

	pspDebugScreenClear();
	pspDebugScreenPrintf("The second hop CAME BACK.\n\n");
	pspDebugScreenPrintf("A working launch never returns, so it was refused.\n");
	pspDebugScreenPrintf("Return code: ms0:/loadexec-secondhop.txt\n\n");
	pspDebugScreenPrintf("If the probe is on screen instead of this\n");
	pspDebugScreenPrintf("screen, a chained launch WORKS.\n\n");
	pspDebugScreenPrintf("Press X to exit.\n");

	for (;;)
	{
		sceCtrlReadBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_CROSS)
			break;
		sceDisplayWaitVblankStart();
	}

	sceKernelExitGame();
	return 0;
}
