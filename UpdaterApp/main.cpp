/*
 * MineCraft Launcher -- the EBOOT the XMB runs.
 *
 * WHAT CHANGED, AND WHY IT IS THE WHOLE POINT
 * -------------------------------------------
 * This program was the in-game updater: a second EBOOT that the game handed off to through a file,
 * whose job was to apply a package containing EBOOT.PBP, and which then could not start the game again
 * because loadexec can only be called once from any one process.
 *
 * Every one of those constraints came from the same place: the updater was started BY the game, so it
 * was already the product of a loadexec, and the game was running from the file that had to be
 * replaced. Both are gone now, because the arrangement was turned inside out. This program is the
 * EBOOT the XMB starts, and the game is a different file beside it, under Installs/<version>/. So:
 *
 *   - It CAN replace the game, because it is not running from it. No handoff file, no second program,
 *     no "you cannot replace the file you are running".
 *   - It CAN start the game afterwards, because this is its first and only loadexec. One hop, which is
 *     the one the firmware allows.
 *   - It can keep several versions side by side, so a build that will not boot is a menu selection away
 *     from the one that will.
 *
 * What is left in this file is the shell: callbacks, clock, the screen, and one call into the menu.
 * The menu is Launcher.cpp, the update flow is LauncherUpdate.cpp.
 *
 * WHAT IT SHARES WITH THE GAME, AND WHY THAT IS NOT OPTIONAL
 * --------------------------------------------------------
 * Scripts/Updater.cpp -- the whole update protocol, compiled from the same source the game used to
 * compile it. The two binaries have to agree on every byte of a package, and one implementation in both
 * is the only way to guarantee that. pgeZip, Logger and the PNG loader are shared the same way.
 *
 * The screen shares the game's TEXTURES and FONT but not its renderer: see Screen.h, which argues that
 * out. The short version is that this program is the one that has to work when the game does not, and
 * RenderManager would bring the camera, the model loader and the world to draw a menu.
 *
 * THE WORKING DIRECTORY, WHICH IS NOW ASSUMED RATHER THAN IMPOSED
 * --------------------------------------------------------------
 * The old updater had none -- loadexec starts a payload with `cwd : "(failed)"`, measured on hardware
 * -- and half of that program existed to work around it. This one is started by the XMB and therefore
 * HAS a working directory: its own folder. That is what makes "Assets/...", "Installs/..." and
 * "Log.txt" all mean what they say here, and it is why the launcher can be this much simpler.
 *
 * The other half of that arrangement is still needed, in the other direction: a game started from here
 * does not get a working directory, so the launcher writes the install's path to a launch file that the
 * game's first constructor reads. See Scripts/LaunchDir.h and Launcher.cpp's DoPlay.
 *
 * THE LOG
 * -------
 * Log.txt, in this folder. It is NOT the game's log -- the game has its own, in its own install folder,
 * and truncates it on boot. The old updater had to copy its log aside before the game could destroy it
 * (see PreserveLog, and Update::UpdaterLogName); with one log per folder the problem does not exist,
 * and the launcher's record of an update survives the player starting what it installed.
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspiofilemgr.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <string.h>
#include <stdio.h>

// The game's own look, with a text fallback inside it. See Screen.h for why it is not RenderManager.
#include "Screen.h"
// The menu, and the one call this file makes into it. Launcher.h documents the card layout.
#include "Launcher.h"
// The project's FPU exception control, shared with the game rather than reimplemented here: the
// register handling and the account of why it is needed are already written down in that header.
#include "FpuControl.h"

#include <Aurora/Utils/Logger.h>

using namespace Aurora::Utils;

// Defined in FpuInit.c, which clears the FPU exception enables from a constructor placed last in the
// object list so that it runs first. These report what the register held when that happened.
extern "C" unsigned int McUpdaterFcr31Before(void);
extern "C" unsigned int McUpdaterFcr31After(void);

/*
 * THE GAME'S SETTINGS, not the baseline ones.
 *
 * This program links Scripts/Updater.cpp, Logger.cpp and pgeZip.c -- the same sources the game
 * compiles -- and those sources have only ever run on this console under the game's settings.
 * Updater.cpp says so itself: its 4 KB transfer buffer "lives on a 512 KB thread stack". The first
 * version of the updater used tools/minimal-boot-test's baseline settings (no VFPU, a 64 KB stack) on
 * the reasoning that the probe EBOOT had run under them, but the probe's payload did almost nothing and
 * shared none of that code, so it was not evidence about any of it. Bringing the settings into line
 * removes a whole class of difference between the two processes at once, which matters because a
 * difference here presents as a crash part way through an install with no way to tell which one it was.
 *
 * THE HEAP IS BOUNDED, and that is still right even though the launch chain changed. libcglue's _sbrk
 * shifts the value and branches on its sign, so a negative value means "size the heap from the largest
 * free block" (MakefileGame says the same at length). That is right for the game and wrong here: with
 * -1 it claimed essentially the entire user partition at startup, and a run measured it -- 764 KB free
 * with 512 KB of it in one block. The largest allocation in this program is ONE WHOLE ARCHIVE ENTRY,
 * because pgeZipFileRead hands it back in RAM -- 4.9 MB for the current EBOOT. That is what sets the
 * floor, and 8 MB is comfortably above it and is written as a plain size rather than -1 so that both
 * numbers can be read side by side.
 *
 * PSP_HEAP_THRESHOLD_SIZE_KB, which the game sets to 2048 for the five net modules and the system
 * dialogs, is deliberately absent. The launcher never brings networking up at boot; it does so only
 * while an update is actually running, and by then the modules are loaded explicitly (see
 * Update::NetInit) rather than being reserved for at startup.
 */
PSP_MODULE_INFO("MineCraftLauncher", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(512);
PSP_HEAP_SIZE_KB(8192);

/* ---------------------------------------------------------------------------
 * Ways out.
 *
 * Same reasoning as everywhere else in this project: a program that parks itself on a screen needs a
 * working HOME button, and the exit callback has to be registered before anything can fail, or the
 * only way off the screen is holding the power switch.
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
	{
		sceKernelStartThread(thid, 0, 0);
	}
}

int main(void)
{
	// Before anything else can execute a floating-point instruction. FpuInit.c's constructor has already
	// cleared this, and doing it in both places means the register is right however the constructors
	// ended up ordered. See Scripts/FpuControl.h for what this console does when it is not cleared.
	MC_DisableFpuExceptions();

	SetupCallbacks();
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

	/*
	 * The log, reset before anything can write to it. The launcher is started by the XMB and has a
	 * working directory already, so unlike the old updater there is no window where Logger cannot write
	 * -- no chdir has to happen first, and this can be the first thing that runs.
	 */
	Logger::Instance()->ResetLog();

	// The FPU register first, because it is the one line that decides whether anything after it can be
	// trusted (see Scripts/FpuControl.h). A "before" of 0x00000E00 is this console's own value and the
	// evidence that clearing it was necessary; an "after" of 0x00000000 is the clear having happened.
	Logger::Instance()->LogMessage("launcher: fcr31 before=0x%08X after=0x%08X\n",
		McUpdaterFcr31Before(), McUpdaterFcr31After());
	Logger::Instance()->LogMessage("launcher: root '%s'\n", LauncherRootPath().c_str());

	/* The clock, set to the value the game sets.
	 *
	 * Scripts.cpp calls scePowerSetClockFrequency(333, 333, 166) immediately after installing callbacks
	 * and immediately before RenderManager::Init(), and the old updater never did -- so it ran the whole
	 * graphics and archive path on whatever clock loadexec left behind. That was the last deliberate
	 * difference from a startup known to work on this console, and it is removed here for the same
	 * reason as the thread settings above: this links the game's own sources and must not run them in
	 * conditions they have never run in.
	 *
	 * It matters more in the launcher than it did in the updater, because the launcher does the one
	 * thing that is actually CPU- and IO-bound: copying an install. The GE and EDRAM are clocked with
	 * the system, and GU is the newest thing in this program. */
	Logger::Instance()->LogMessage("launcher: setting the clock 333/333/166\n");
	scePowerSetClockFrequency(333, 333, 166);

	/* The display. Screen::Init() returning false is not a failure -- it is the text fallback being
	 * selected, which is what happens on a card with no Assets folder. That is exactly the card most
	 * likely to be running a repair tool, so the menu is drawn either way and everything after this
	 * point asks Screen which of the two it got. */
	if (!Screen::Instance()->Init())
	{
		pspDebugScreenInit();
	}

	// The menu, which only returns when the player quits. A successful launch does not return: loadexec
	// resets the console, which is the one hop this program exists to make.
	LauncherMain();

	Logger::Instance()->LogMessage("launcher: exiting to the XMB\n");
	sceKernelExitGame();
	return 0;
}
