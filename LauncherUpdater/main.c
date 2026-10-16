/*
 * MineCraft Launcher Updater -- the second program that swaps the launcher's own EBOOT.PBP.
 *
 * WHY THIS EXISTS AT ALL
 * ----------------------
 * A running program cannot replace the file it is executing, and the launcher IS the file the XMB
 * runs. So the launcher installs everything in its own update package itself -- it has a working
 * directory and can write any path in its folder -- with exactly one exception: its own EBOOT.PBP.
 * That file is staged as EBOOT.PBP.new, a one-line handoff is written, and this program is started by
 * loadexec to rename it into place. It then removes the handoff and exits to the XMB.
 *
 * WHY IT IS THIS SMALL, AND WHY IT SHARES NO CODE
 * -----------------------------------------------
 * This is the program that has to work when nothing else does -- if it fails part way, the console
 * has no launcher. It therefore links nothing: not Aurora, not the logger, not zlib, not the updater
 * protocol, not even the launcher's own file helpers. It includes one header (FpuControl.h, which is
 * static inline) and nothing else, so there is no library init, no shared global state and no
 * dependency that could be the thing that fails. That is the same argument the launcher makes for not
 * using RenderManager, taken to its end: the file the launcher's protocol defines is not read here,
 * because reading it would mean linking the code that writes it. The handoff format is one line,
 * repeated in both places on purpose, and documented in Scripts/Updater.h next to the writer.
 *
 * THE HANDOFF FORMAT, which is the whole interface:
 *
 *     <absolute staged EBOOT path>|<absolute target EBOOT path>\n
 *
 * at a FIXED path on ms0: and ef0:, because this program is started by loadexec and has no working
 * directory -- the same fact that shapes every other cross-program path in this project.
 *
 * THE ORDER OF OPERATIONS IS THE SAFETY PROPERTY
 * ----------------------------------------------
 * The old launcher is renamed to EBOOT.PBP.bak BEFORE the new one is moved in, so the card is never
 * without a launcher: if the staged file turns out to be unreadable, the backup is renamed back and
 * the console keeps the version it already had. The .bak is deliberately left in place afterwards
 * rather than deleted -- it is the rollback, and it costs ~1.1 MB.
 */

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <string.h>
#include <stdio.h>

// MC_DisableFpuExceptions, static inline and dependency-free. Called before anything else for the
// reason in that header: this console hands a module three FPU exception enables that turn a quiet
// nan into a hardware fault, and the fault does not return.
#include "FpuControl.h"

PSP_MODULE_INFO("MineCraftLauncherUpdater", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);
// Deliberately small, unlike the launcher's 512/8192. This program copies nothing through RAM -- the
// swap is two renames -- so this is only the C runtime's own headroom, and a repair tool whose whole
// point is being the smallest thing on the card should not ask the system for more than it needs.
PSP_MAIN_THREAD_STACK_SIZE_KB(64);
PSP_HEAP_SIZE_KB(1024);

// Must match Scripts/Updater.h's LauncherSwapFileName().
#define SWAP_FILE "mclauncher-swap.txt"

// The devices the handoff is looked for on, the same pair the launcher's own handoff uses. The writer
// picks the device from the target path; this side cannot, so it tries both.
static const char* kDevices[] = { "ms0:", "ef0:" };
#define DEVICE_COUNT 2

/* ---------------------------------------------------------------------------
 * Ways out. Registered before anything can fail, or the only way off the screen
 * is the power switch -- the same rule every program in this project follows.
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

static void SetupCallbacks(void)
{
	int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, THREAD_ATTR_USER, 0);
	if (thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}
}

/* ---------------------------------------------------------------------------
 * The two file operations this program needs, and nothing else.
 * ------------------------------------------------------------------------- */

// Reads a whole small file. Returns the number of bytes, or -1. The cap is the caller's buffer: this
// file is one line, so anything approaching it is not the file we wrote.
static int ReadWholeFile(const char* path, char* buffer, int cap)
{
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0)
	{
		return -1;
	}

	int total = 0;
	while (total < cap - 1)
	{
		int got = sceIoRead(fd, buffer + total, cap - 1 - total);
		if (got <= 0)
		{
			break;
		}
		total += got;
	}

	sceIoClose(fd);
	buffer[total] = '\0';
	return total;
}

static int FileExists(const char* path)
{
	SceIoStat st;
	return sceIoGetstat(path, &st) >= 0;
}

// Flushes both removable devices. Not conditional on which one the handoff was on: this runs once,
// and a reset that races an unflushed rename is exactly the failure that would leave the card with no
// launcher, so the cheap call is worth making on both.
static void SyncCard(void)
{
	sceIoSync("ms0:", 0);
	sceIoSync("ef0:", 0);
}

int main(void)
{
	// Before anything else can execute a floating-point instruction; see FpuControl.h. This program
	// does no float arithmetic, but the C runtime's own startup and the debug screen are not inspected
	// for it, and the cost of the clear is a register write.
	MC_DisableFpuExceptions();

	SetupCallbacks();
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

	pspDebugScreenInit();
	pspDebugScreenClear();
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenPrintf("MineCraft Launcher Updater\n\n");

	// The handoff, on whichever device has it.
	char text[1024];
	char device[16];
	text[0] = '\0';
	device[0] = '\0';

	int found = 0;
	int i;
	for (i = 0; i < DEVICE_COUNT; ++i)
	{
		char path[64];
		snprintf(path, sizeof(path), "%s/%s", kDevices[i], SWAP_FILE);
		if (ReadWholeFile(path, text, sizeof(text)) > 0)
		{
			// Strip a trailing newline so the target path does not carry one into a file name.
			size_t len = strlen(text);
			while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r'))
			{
				text[--len] = '\0';
			}
			strncpy(device, kDevices[i], sizeof(device) - 1);
			device[sizeof(device) - 1] = '\0';
			found = 1;
			break;
		}
	}

	if (!found)
	{
		// Nothing pending. This is the ordinary case when a swap has already been applied and the
		// handoff was lost, so it is reported and not treated as a fault.
		pspDebugScreenPrintf("No swap was pending.\nNothing to do.\n");
	}
	else
	{
		// Split on the FIRST '|'. The paths are absolute and built by the launcher, so neither can
		// contain a '|' -- see the note in Scripts/Updater.h.
		char* bar = strchr(text, '|');
		if (bar == 0 || bar == text || bar[1] == '\0')
		{
			pspDebugScreenPrintf("The handoff was not readable.\nNothing was changed.\n");
		}
		else
		{
			*bar = '\0';
			char* staged = text;
			char* target = bar + 1;

			char backup[512];
			snprintf(backup, sizeof(backup), "%s.bak", target);

			pspDebugScreenPrintf("Swapping in the new launcher...\n");

			if (!FileExists(staged))
			{
				// The staged file is gone. Either the swap already happened or the card was tampered
				// with; either way there is nothing to move and the launcher on the card is left alone.
				pspDebugScreenPrintf("The new launcher is missing.\nNothing was changed.\n");
			}
			else
			{
				// Back up the current launcher, then move the new one in. The order is the safety
				// property: if the second rename fails, the first is undone and the card still has a
				// launcher.
				sceIoRemove(backup);			// an older .bak, from an earlier update
				sceIoRename(target, backup);

				if (sceIoRename(staged, target) < 0)
				{
					sceIoRename(backup, target);	// put the version we had back
					pspDebugScreenPrintf("The swap failed.\nThe launcher was left as it was.\n");
				}
				else
				{
					SyncCard();
					pspDebugScreenPrintf("Done. The launcher will start next time.\n");
				}
			}
		}
	}

	// The handoff goes, on every device and whichever one it was on: a stale handoff left behind would
	// be picked up by the next run of this program and try to apply a swap that is already done. The
	// backup and the stale staged file are deliberately kept -- see the note at the top.
	for (i = 0; i < DEVICE_COUNT; ++i)
	{
		char path[64];
		snprintf(path, sizeof(path), "%s/%s", kDevices[i], SWAP_FILE);
		sceIoRemove(path);
	}
	SyncCard();

	pspDebugScreenPrintf("\nReturning to the XMB...\n");
	sceKernelDelayThread(1500000);
	sceKernelExitGame();
	return 0;
}
