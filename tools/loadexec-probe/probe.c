/*
 * Can this console launch another EBOOT at run time? -- probe, resumable across reboots.
 *
 * WHY THIS EXISTS
 * ---------------
 * The online updater can already download a package and prove it arrived intact, and it correctly
 * refuses to apply one containing EBOOT.PBP, because a running game cannot replace the file it is
 * executing. The plan for the last step is to hand off to a separate Updater EBOOT *at run time*
 * instead of asking the player to launch it from the XMB -- which is what would make the whole thing
 * one confirmed button press.
 *
 * WHAT HAS BEEN MEASURED SO FAR, IN ORDER
 * ---------------------------------------
 * Run 1 tested the plain SDK call, eight ways. Every one returned
 *
 *     0x80020149 = SCE_KERNEL_ERROR_ILLEGAL_PERM_CALL
 *
 * and never 0x80020148 (UNSUPPORTED_PRX_TYPE), so nothing was rejected as a bad binary: the call is
 * refused on *privilege*. Filename, path form, param supplied or NULL, and key NULL or "game" all made
 * no difference. That is psploadexec.h's "limited when not running in kernel mode" being enforced --
 * and this project is user mode (PSP_MODULE_INFO attribute 0, not PSP_MODULE_KERNEL 0x1000), so the
 * limit is ours. The CFW does not lift it for this call.
 *
 * Run 2 moved to the loader the CFW *does* expose to user mode:
 *
 *     int sctrlKernelLoadExecVSHWithApitype(int apitype, const char *file,
 *                                          struct SceKernelLoadExecVSHParam *param);
 *
 * from stub module "SystemCtrlForUser" (libpspsystemctrl_user.a). Its first attempt was
 *
 *     sctrlKernelLoadExecVSHWithApitype(0x141, "PAYLOAD.PBP", &vsh)   // relative path
 *
 * and that call NEVER RETURNED -- no result line reached the log, and the payload never started. So it
 * is not permission-refused the way sceKernelLoadExec is: the CFW took the request and acted on it, and
 * the console then failed to come up. That is a much better failure to have.
 *
 * The leading explanation, and the reason the case order below changed: a relative path cannot survive
 * a loadexec reboot. This program's cwd is ".../MCPSP LoadExec Probe" while it runs, but the reboot
 * re-enters the loader with a different cwd (almost certainly ms0:/), so "PAYLOAD.PBP" would be looked
 * for at ms0:/PAYLOAD.PBP, not be found, and leave nothing on screen. Note that run 2's first attempt
 * had *two* possible faults at once -- a relative path and a non-standard filename -- which is exactly
 * what the reordering here is meant to stop: absolute paths, canonical EBOOT.PBP name, and the most
 * standard apitype now go first, and the relative forms are kept last for the record.
 *
 * The apitypes come from pspinit.h. MS2 (0x141) is the one psploadexec_kernel.h describes as "used by
 * the firmware to execute games (and homebrew :P) from a memory stick", so it is the natural choice;
 * GAME_EBOOT (0x110) is tried rather than assumed. The key values ("game") are the ones the kernel
 * header documents for SceKernelLoadExecVSHParam -- the struct this call actually takes, and the reason
 * the snippet that started all this looked plausible while naming the wrong function: the mechanism and
 * the key values are real, they just do not belong to sceKernelLoadExec.
 *
 * WHY THIS IS RESUMABLE, AND WHY THAT MATTERS MOST
 * ------------------------------------------------
 * Each attempt can power the console down, and every such failure costs a trip to the PC. A probe that
 * tested one configuration per trip would need six trips to answer the question. So this one keeps its
 * place: before making a call it writes the index of the *next* case to ms0:/loadexec-probe-state.txt,
 * and on the next launch it starts from there. One session with a few power cycles works through the
 * whole list. The log is appended, not truncated, so earlier runs survive to be read together.
 *
 * The one thing that is destructive is a launch that *succeeds* -- then the payload runs and this
 * program is gone. That is the outcome being looked for, and the payload announces itself.
 *
 * THIS PROGRAM AND ITS PAYLOAD ARE HARMLESS BY CONSTRUCTION
 * ---------------------------------------------------------
 * Neither one touches the game. They live in their own folder, they write only their own log files, and
 * the payload's entire job is to record that it was executed and then exit. Nothing here replaces,
 * renames, deletes or opens any file belonging to Minecraft. A failed attempt costs a power cycle and
 * nothing else -- loadexec does not write to the card or to flash.
 *
 * READING THE RESULT
 * ------------------
 *   ms0:/loadexec-payload-ctor.txt   the payload's module was loaded and its constructor ran
 *   ms0:/loadexec-payload.txt        the payload reached main() -- the handoff fully works
 *   ms0:/loadexec-probe.txt          every attempt, its arguments, and every return value
 *   ms0:/loadexec-probe-state.txt    which case is next (delete it, or re-run deploy, to start over)
 *
 * Two payload files because one answer is not enough here. A launch that gets as far as loading the
 * module but dies before main() -- a failed main-thread or heap creation, which this project has been
 * bitten by before -- would look identical to "never launched" if only the second file were written.
 * The constructor runs earlier than main(), so its file splits those two cases apart.
 *
 * If the state file reaches the end of the list with no payload file anywhere, the answer is that no
 * configuration tried here can hand off, and the permanent XMB-launchable Updater is the design to
 * build -- which is the safer one regardless.
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

/*
 * Deliberately the *baseline* module settings from tools/minimal-boot-test, not the game's. The probe
 * is the instrument, so it should be the least unusual module this toolchain can produce: plain USER
 * thread, no VFPU attribute, small fixed heap. A probe that failed to start would tell us nothing, and
 * the game's own settings are already known to be worth isolating separately.
 */
PSP_MODULE_INFO("MCPSPLoadExecProbe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(64);
PSP_HEAP_SIZE_KB(1024);

/* ---------------------------------------------------------------------------
 * Files.
 *
 * Everything is written to ms0:/ by absolute path now. The relative copies that run 1 kept were dropped
 * on purpose: after a loadexec reboot this program's cwd is not the folder it was launched from, so a
 * relative write from the *payload* could land anywhere and prove nothing about where it ran. The
 * payload records its own getcwd() instead, which is the fact actually wanted.
 * ------------------------------------------------------------------------- */

#define LOG_ABS          "ms0:/loadexec-probe.txt"
#define STATE_ABS        "ms0:/loadexec-probe-state.txt"
#define PAYLOAD_CTOR_ABS "ms0:/loadexec-payload-ctor.txt"
#define PAYLOAD_MAIN_ABS "ms0:/loadexec-payload.txt"

static void WriteFile(const char* path, const char* text, int append)
{
	int flags = PSP_O_WRONLY | PSP_O_CREAT;
	flags |= append ? PSP_O_APPEND : PSP_O_TRUNC;

	SceUID fd = sceIoOpen(path, flags, 0777);
	if (fd < 0)
		return;

	unsigned int len = 0;
	while (text[len] != '\0')
		len++;

	sceIoWrite(fd, text, len);
	sceIoClose(fd);
}

static int FileExists(const char* path)
{
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return 0;

	sceIoClose(fd);
	return 1;
}

/* Appended, never truncated: a run that ends in a power cut is exactly the run whose earlier lines are
 * needed later. */
static void LogLine(const char* fmt, ...)
{
	char buffer[512];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	WriteFile(LOG_ABS, buffer, 1);
}

/* ---------------------------------------------------------------------------
 * Where we are in the list.
 *
 * Two integers: the next case to attempt, and a run counter for the log banner. Written *before* each
 * call, so a call that never returns still leaves the correct resume point behind.
 * ------------------------------------------------------------------------- */

static int ReadStateFile(int* nextCase, int* runCount)
{
	*nextCase = 0;
	*runCount = 0;

	SceUID fd = sceIoOpen(STATE_ABS, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return 0;

	char buffer[64];
	int n = sceIoRead(fd, buffer, sizeof(buffer) - 1);
	sceIoClose(fd);

	if (n <= 0)
		return 0;

	buffer[n] = '\0';

	if (sscanf(buffer, "%d %d", nextCase, runCount) != 2)
	{
		*nextCase = 0;
		*runCount = 0;
		return 0;
	}

	return 1;
}

static void WriteStateFile(int nextCase, int runCount)
{
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%d %d\n", nextCase, runCount);
	WriteFile(STATE_ABS, buffer, 0);
}

/* ---------------------------------------------------------------------------
 * The cases.
 * ------------------------------------------------------------------------- */

typedef enum
{
	MECH_SCE_LOADEXEC,  /* sceKernelLoadExec(target, &SceKernelLoadExecParam) -- the user-mode SDK call */
	MECH_SCTRL_APITYPE, /* sctrlKernelLoadExecVSHWithApitype(apitype, target, &...VSHParam) -- the CFW's */
} Mechanism;

typedef struct
{
	const char* label;       /* what to call this case in the log */
	Mechanism   mechanism;   /* which call is made */
	int         apitype;     /* MECH_SCTRL_APITYPE only */
	int         useAbsolute; /* resolve `target` against the directory this EBOOT was launched from */
	const char* target;      /* path handed to the loader, absolute once resolved */
	const char* key;         /* NULL, or the key value to try */
	int         nullParam;   /* MECH_SCE_LOADEXEC only: pass NULL instead of a filled struct */
	int         useArg;      /* also put `target` in args/argp */
} ProbeCase;

/*
 * Ordered so that the configurations most likely to work come first, because a success -- or a hang --
 * ends the run. The two sceKernelLoadExec controls are cheap: they return immediately with the refusal
 * already measured, and they keep the log honest about the environment not having changed.
 *
 * The relative forms are kept, at the end, precisely because run 2 died on one. If an absolute path
 * works and a relative one does not, that is a finding worth having in writing rather than a guess.
 */
static const ProbeCase cases[] = {
	{ "control: sceKernelLoadExec, param=NULL",
	  MECH_SCE_LOADEXEC,  0, 0, "PAYLOAD.PBP", NULL, 1, 0 },
	{ "control: sceKernelLoadExec, key=NULL",
	  MECH_SCE_LOADEXEC,  0, 0, "PAYLOAD.PBP", NULL, 0, 0 },

	{ "sctrl apitype=0x141 (MS2), abs TARGET/EBOOT.PBP, key=\"game\"",
	  MECH_SCTRL_APITYPE, PSP_INIT_APITYPE_MS2, 1, "TARGET/EBOOT.PBP", "game", 0, 0 },
	{ "sctrl apitype=0x141 (MS2), abs TARGET/EBOOT.PBP, key=NULL",
	  MECH_SCTRL_APITYPE, PSP_INIT_APITYPE_MS2, 1, "TARGET/EBOOT.PBP", NULL,   0, 0 },
	{ "sctrl apitype=0x110 (GAME_EBOOT), abs TARGET/EBOOT.PBP, key=\"game\"",
	  MECH_SCTRL_APITYPE, PSP_INIT_APITYPE_GAME_EBOOT, 1, "TARGET/EBOOT.PBP", "game", 0, 0 },
	{ "sctrl apitype=0x141 (MS2), abs TARGET/EBOOT.PBP, argv=target",
	  MECH_SCTRL_APITYPE, PSP_INIT_APITYPE_MS2, 1, "TARGET/EBOOT.PBP", "game", 0, 1 },

	{ "sctrl apitype=0x141 (MS2), relative TARGET/EBOOT.PBP, key=\"game\"",
	  MECH_SCTRL_APITYPE, PSP_INIT_APITYPE_MS2, 0, "TARGET/EBOOT.PBP", "game", 0, 0 },
	{ "sctrl apitype=0x141 (MS2), relative PAYLOAD.PBP, key=NULL -- the one that hung in run 2",
	  MECH_SCTRL_APITYPE, PSP_INIT_APITYPE_MS2, 0, "PAYLOAD.PBP",      NULL,   0, 0 },
};

#define TOTAL_CASES ((int)(sizeof(cases) / sizeof(cases[0])))

static char absDir[256];        /* from getcwd(): the folder this EBOOT was launched from */
static char absPathBuffer[512]; /* holds the resolved absolute target for the current case only */

/* ---------------------------------------------------------------------------
 * Ways out.
 *
 * Registering the exit callback is not optional for a program that parks itself on a screen: without it
 * the HOME button does nothing and the only way off the screen is to hold the power switch. That is an
 * unpleasant thing to discover while holding a PSP, so it is registered as the very first act of
 * main(), before the display is even up. A button is polled as well, so the explicit way out does not
 * depend on the HOME button either.
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
	if (thid >= 0)
		sceKernelStartThread(thid, 0, 0);
}

static void WaitForCrossPress()
{
	SceCtrlData pad;

	/* Short delay so a button held from the previous screen is not read as this press. */
	sceKernelDelayThread(300000);

	for (;;)
	{
		sceCtrlReadBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_CROSS)
			break;
		sceDisplayWaitVblankStart();
	}
}

/* ---------------------------------------------------------------------------
 * One attempt.
 * ------------------------------------------------------------------------- */

static void RunCase(int index, const ProbeCase* c)
{
	/* Resolve the path first, so a case that cannot be run is reported as skipped rather than silently
	 * sending a relative path to the loader -- which is the mistake that cost run 2. */
	const char* target = c->target;

	if (c->useAbsolute)
	{
		if (absDir[0] == '\0')
		{
			LogLine("\n--- case %d: %s\n    SKIPPED: getcwd() gave nothing to build an absolute path from\n",
				index, c->label);
			return;
		}

		snprintf(absPathBuffer, sizeof(absPathBuffer), "%s/%s", absDir, c->target);
		target = absPathBuffer;
	}

	LogLine("\n--- case %d: %s\n", index, c->label);
	LogLine("    target=\"%s\"  key=%s  param=%s  argv=%s  apitype=0x%X  absolute=%s\n",
		target,
		c->key ? c->key : "(null)",
		c->nullParam ? "NULL" : "filled",
		c->useArg ? "target" : "(null)",
		(unsigned int)c->apitype,
		c->useAbsolute ? "yes" : "no");

	int result;

	if (c->mechanism == MECH_SCTRL_APITYPE)
	{
		/* The CFW launcher takes the kernel SceKernelLoadExecVSHParam -- the struct whose `key` field is
		 * documented "usually game, updater or vsh". unk5 is set to 0x10000 because the header documents
		 * that as its default; configfile is left zero, which selects the default /kd/pspbtcnf_game.txt. */
		char argBuffer[512];

		struct SceKernelLoadExecVSHParam vsh;
		memset(&vsh, 0, sizeof(vsh));
		vsh.size = sizeof(vsh);
		vsh.key = c->key;
		vsh.unk5 = 0x10000;

		if (c->useArg)
		{
			/* args/argp are the argument string the launched program's argv is built from, and that is
			 * the only channel by which a real updater could be told which package to apply and where.
			 * Worth a case here rather than a later trip: if a launch works at all, passing an argument
			 * is the very next thing the design needs. The payload logs its argv, so what arrives is
			 * measured rather than assumed. */
			strncpy(argBuffer, target, sizeof(argBuffer) - 1);
			argBuffer[sizeof(argBuffer) - 1] = '\0';
			vsh.args = (SceSize)(strlen(argBuffer) + 1);
			vsh.argp = argBuffer;
		}

		result = sctrlKernelLoadExecVSHWithApitype(c->apitype, target, &vsh);
	}
	else
	{
		struct SceKernelLoadExecParam param;
		struct SceKernelLoadExecParam* paramPtr = NULL;

		if (!c->nullParam)
		{
			memset(&param, 0, sizeof(param));
			param.size = sizeof(param);
			param.key = c->key;
			paramPtr = &param;
		}

		result = sceKernelLoadExec(target, paramPtr);
	}

	/* Reaching this line means the call did NOT replace us, which is the definitive result for this case.
	 * Logged immediately, because the next case may be the one that does not come back. */
	LogLine("    returned 0x%08X (%d)\n", (unsigned int)result, result);
	pspDebugScreenPrintf("  case %d returned 0x%08X\n", index, (unsigned int)result);
}

/* ---------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

int main(void)
{
	SetupCallbacks();

	pspDebugScreenInit();
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

	absDir[0] = '\0';
	if (getcwd(absDir, sizeof(absDir)) == 0)
		absDir[0] = '\0';

	int nextCase = 0;
	int runCount = 0;
	int resumed = ReadStateFile(&nextCase, &runCount);

	if (nextCase < 0 || nextCase > TOTAL_CASES)
		nextCase = 0;

	runCount++;

	/* Claim the slot for this run before anything else, so a hang anywhere below still leaves a state
	 * file that says which case was in flight. */
	WriteStateFile(nextCase, runCount);

	LogLine("\n\n================ run %d ================\n", runCount);
	LogLine("cwd                : \"%s\"\n", absDir[0] != '\0' ? absDir : "(failed)");
	LogLine("resuming from case : %d of %d (%s)\n", nextCase, TOTAL_CASES,
		resumed ? "state file found" : "fresh start");
	LogLine("payload ctor marker: %s\n", FileExists(PAYLOAD_CTOR_ABS) ? "PRESENT" : "absent");
	LogLine("payload main marker: %s\n", FileExists(PAYLOAD_MAIN_ABS) ? "PRESENT" : "absent");

	if (FileExists(PAYLOAD_MAIN_ABS))
		LogLine("*** the PREVIOUS run launched the payload: the handoff works. See %s\n", PAYLOAD_MAIN_ABS);
	else if (FileExists(PAYLOAD_CTOR_ABS))
		LogLine("*** the PREVIOUS run loaded the payload's module but it never reached main()\n");

	pspDebugScreenPrintf("\nMCPSP launcher probe -- resumable\n\n");
	pspDebugScreenPrintf("Absolute paths are tried first.\n");
	pspDebugScreenPrintf("Progress is kept, so power-cycling and\n");
	pspDebugScreenPrintf("relaunching resumes where it stopped.\n\n");
	pspDebugScreenPrintf("cwd: %s\n", absDir[0] != '\0' ? absDir : "(failed)");
	pspDebugScreenPrintf("resume at case %d of %d\n\n", nextCase, TOTAL_CASES);

	if (FileExists(PAYLOAD_MAIN_ABS))
		pspDebugScreenPrintf("*** PREVIOUS RUN LAUNCHED THE PAYLOAD ***\n\n");

	if (nextCase >= TOTAL_CASES)
	{
		pspDebugScreenPrintf("All %d cases already attempted.\n", TOTAL_CASES);
		pspDebugScreenPrintf("See ms0:/loadexec-probe.txt\n");
		pspDebugScreenPrintf("Delete ms0:/loadexec-probe-state.txt\n");
		pspDebugScreenPrintf("to start over.\n\n");
		pspDebugScreenPrintf("Press X to exit.\n");
		WaitForCrossPress();
		sceKernelExitGame();
		return 0;
	}

	pspDebugScreenPrintf("Press X to continue.  (X or HOME exits)\n");

	WaitForCrossPress();

	pspDebugScreenClear();
	pspDebugScreenSetXY(0, 0);

	int i;
	for (i = nextCase; i < TOTAL_CASES; ++i)
	{
		/* Advance the resume point BEFORE the call. If this iteration never comes back, the next launch
		 * starts at the following case instead of repeating the one that just took the console down. */
		WriteStateFile(i + 1, runCount);

		RunCase(i, &cases[i]);
	}

	WriteStateFile(TOTAL_CASES, runCount);
	LogLine("\nall %d cases returned without launching the payload\n", TOTAL_CASES);
	LogLine("conclusion: no configuration tried here can hand off on this console\n");

	pspDebugScreenPrintf("\n\nAll %d cases returned.\n\n", TOTAL_CASES);
	pspDebugScreenPrintf("Nothing launched the payload.\n\n");
	pspDebugScreenPrintf("Full log: ms0:/loadexec-probe.txt\n\n");
	pspDebugScreenPrintf("Press X to exit.\n");

	WaitForCrossPress();

	sceKernelExitGame();
	return 0;
}
