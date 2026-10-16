#include <math.h>
#include <psptypes.h>
#include <pspkernel.h>

#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/Utils/Logger.h>
#include <Aurora/Utils/Timer.h>
#include <Aurora/System/SystemManager.h>
#include <Aurora/Graphics/Camera.h>
#include <Aurora/Utils/StateManager.h>

#include "Scripts/StateMenu.h"
#include "Scripts/StatePlay.h"
#include "Scripts/CraftWorld2.h"
#include "Scripts/SplashScreen.h"
#include "Scripts/SoundManager.h"
#include "Scripts/TextureHelper.h"
#include "Scripts/DebugConfig.h"
#include "Scripts/BootTrace.h"
#include "Scripts/FpuControl.h"
#include "Scripts/InitTrace.h"
#include "Scripts/PerfCounters.h"

using namespace Aurora::Graphics;
using namespace Aurora::Utils;
using namespace Aurora::System;
using namespace Aurora;

static void RunFrame(StateManager& stateManager)
{
	stateManager.HandleEvents();
	MC_PERF_PHASE_BEGIN(kPhStateUpdate);
	stateManager.Update();
	MC_PERF_PHASE_END(kPhStateUpdate);
	MC_PERF_PHASE_BEGIN(kPhStateDraw);
	stateManager.Draw();
	MC_PERF_PHASE_END(kPhStateDraw);
	MC_PERF_PHASE_ADD(kPhGeWait, RenderManager::InstancePtr()->lastGeWaitUs);
	MC_PERF_PHASE_ADD(kPhVblank, RenderManager::InstancePtr()->lastVblankUs);
}

PSP_MODULE_INFO("Tutorial1", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU|THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(512);

// How much of the user partition to leave OUT of the malloc heap -- and how that is actually asked for.
//
// The size below is not a size. libcglue's _sbrk loads it, shifts it left 10 and branches on whether
// the result is negative; the magnitude is never read. Every negative value means the same thing,
// "size the heap from the largest free block". It stays negative rather than being deleted so the
// intent is stated here instead of resting on this symbol happening to be undefined.
PSP_HEAP_SIZE_KB(-1);

// The reserve itself, and the only place it can be set. _sbrk sizes the heap as
// "sceKernelMaxFreeMemSize() minus this"; with no threshold declared it subtracts a hard-coded 512 KB
// (0xfff80000 in the disassembly), and that default was the entire bug. Two builds, one asking for
// 2048 and one for 4096, both measured "kernel totalFreeKB=768, maxFreeKB=512" -- byte-identical, and
// the 512 KB was the untouched default rather than either requested amount. A larger number in
// PSP_HEAP_SIZE_KB cannot fix this; only this line can.
//
// The 512 KB default is the only memory the rest of the system ever gets: the five net modules,
// sceNetInit's socket pool, the system dialogs (connection, on-screen keyboard, message) and their
// fonts. The game's own heap cannot lend to them however free it reports, because it is already a
// separate allocation. Measured: the five modules take 624 KB, which left 76 KB with a largest block
// of 48 KB, so sceNetInit could not fit its 128 KB pool and failed with SCE_KERNEL_ERROR_NO_MEMORY
// while the game heap showed 9.8 MB free. The on-screen keyboard was starved by the same shortage,
// which is why it drew corrupted rather than failing outright.
//
// 2048 covers the modules, the pool, apctl and the dialogs, and costs the game 1.5 MB of a heap that
// was showing 9.8 MB free. Raising it buys system headroom and costs game headroom KB for KB, so this
// is deliberately not generous.
PSP_HEAP_THRESHOLD_SIZE_KB(2048);

int main()
{
	// Development builds: clear Log.txt and stamp the build before anything else writes to it, so the
	// log always describes exactly this run and this binary -- and ahead of the breadcrumbs below,
	// because it truncates the very file they write into.
	//
	// trace.txt is separate and was already opened and truncated by the constructor in
	// Scripts/BootTrace.cpp, which is the earliest code in this module that runs at all. See
	// Scripts/InitTrace.h: it is the fine-grained companion to BootTrace::Stage, for the runs where
	// "which named step" is not yet a narrow enough question. Both are cut from this build when the
	// fault is understood.
	MC_Trace("main: entry");
	MC_BootSession();
	MC_Trace("main: MC_BootSession done, Log.txt reset");
	BootTrace::Stage("main() entry");

	// The constructor in Scripts/BootTrace.cpp already cleared this for the thread main() runs on, and
	// this is that thread. Repeated here deliberately: the whole game is written for an FPU where
	// 0.0f/0.0f is nan rather than a trap, so the property is worth stating in the boot path itself.
	// See Scripts/FpuControl.h for what the console ships with and why it matters.
	MC_DisableFpuExceptions();
	MC_TraceUInt("main: fcr31 after clear", MC_ReadFcr31());

	// Each stage below names the step that is about to run rather than the one that just finished. That
	// is the whole point: it makes the last line of Log.txt the step that did not complete. See
	// Scripts/BootTrace.h for why this is the available answer and a stall screen is not.
	BootTrace::Stage("installing callbacks");
	MC_Trace("main: SetupCallbacks enter");
	SetupCallbacks();
	MC_Trace("main: SetupCallbacks done");

	BootTrace::Stage("clock 333MHz");
	MC_Trace("main: setting clock 333/333/166");
	scePowerSetClockFrequency(333, 333, 166);
	MC_Trace("main: clock set");

	//initialize render manager
	BootTrace::Stage("RenderManager::Init");
	MC_Trace("main: RenderManager::Init enter");
	RenderManager::InstancePtr()->Init();
	MC_Trace("main: RenderManager::Init done");

	MC_Trace("main: CollectPerformance enter");
	RenderManager::InstancePtr()->CollectPerformance(true);
	MC_Trace("main: CollectPerformance done");

	BootTrace::Stage("RenderManager::InitDebugFont");
	MC_Trace("main: InitDebugFont enter");
	RenderManager::InstancePtr()->InitDebugFont();
	MC_Trace("main: InitDebugFont done");

	//set perspectives
	MC_Trace("main: SetOrtho enter");
	RenderManager::InstancePtr()->SetOrtho(0,0,0,0,0,0);
	MC_Trace("main: SetOrtho done");

	//init and load sounds
	// (TextureHelper::Init stages every asset it reads by name -- see FastLoadTexture)
	BootTrace::Stage("TextureHelper::Init");
	MC_Trace("main: TextureHelper::Init enter");
	TextureHelper::Instance()->Init();
	MC_Trace("main: TextureHelper::Init done");

	BootTrace::Stage("SoundManager::Init");
	MC_Trace("main: SoundManager::Init enter");
	SoundManager::Instance()->Init();
	MC_Trace("main: SoundManager::Init done");

	MC_Trace("main: srand(time) enter");
	srand(time(NULL));
	MC_Trace("main: srand done");

	//new state manager
	BootTrace::Stage("StateManager::Init");
	MC_Trace("main: StateManager::Init enter");
	StateManager stateManager;
	stateManager.Init();
	MC_Trace("main: StateManager::Init done");

	RenderManager::InstancePtr()->SetClearColour(0xFF000000);
	MC_Trace("main: clear colour black");

	//splash screens
	BootTrace::Stage("splash screen");
	MC_Trace("main: SplashScreen allocation enter");
	SplashScreen *screen = new SplashScreen(TextureHelper::Instance()->GetTexture(TextureHelper::Genesis),0,0,480,272,3);
	MC_TracePtr("main: SplashScreen constructed", screen);
	MC_Trace("main: ShowSplash enter");
	screen->ShowSplash();
	MC_Trace("main: ShowSplash done");
	delete screen;
	MC_Trace("main: SplashScreen deleted");
	// the 480x272 splash pads to a 512x512x32 texture (1 MB) and is never drawn again;
	// drop it before any state caches texture numbers
	TextureManager::Instance()->RemoveTexture(TextureHelper::Instance()->GetTexture(TextureHelper::Genesis));
	MC_Trace("main: Genesis texture released");

	RenderManager::InstancePtr()->SetClearColour(0xFFFFFFFF);
	MC_Trace("main: clear colour white");

#if MC_INIT_TRACE_STOP
	// Diagnostic build: the splash screen has drawn, so every subsystem this game initialises is up.
	// Stopping here rather than continuing into the menu is what makes the trace's last line
	// unambiguous -- everything up to this point either completed or was named by the line that
	// followed it, and nothing after this point can be mistaken for a startup fault.
	//
	// Turn it off with   make EXTRA_DEFS=-DMC_INIT_TRACE_STOP=0   to get the normal boot back; see
	// Scripts/DebugConfig.h for why this defaults on while a hardware crash is being chased.
	MC_Trace("main: STOPPING at splash as requested, menu not entered");
	MC_DEBUG_LOG("diagnostic build: stopping after the splash screen, before the menu\n");
	sceGuTerm();
	sceKernelExitGame();
	return 0;
#endif

#if MC_PERF_PROBE
	// Development-only: the work-counter probe (see DebugConfig.h). Boots straight into a generated
	// world instead of the menu, because the measurements have to happen with nobody at the
	// controls -- that is the whole point of running them in the emulator without a device.
	{
		StatePlay *probeState = new StatePlay();
		probeState->InitParametric(false, false, false, 20260927u, 0, 'S');
		// The same three calls StateMenu makes when a world is created from the menu. InitCamera is
		// not optional: Update and Draw both dereference fppCam, and without it the probe world dies
		// on its first frame -- which is exactly what the first run of this harness did.
		probeState->InitCamera();
		// A save name of its own, so a probe run can never write over a real world.
		probeState->SetWorldAndSaveName("perfprobe", "perfprobe.lmsd");
		stateManager.ChangeState(probeState);

		while ( stateManager.Running() )
		{
			RunFrame(stateManager);
		}

		sceGuTerm();
		sceKernelExitGame();
		return 0;
	}
#endif

#if MC_TEST_ENV
	// Development-only: the test environment (see DebugConfig.h). A fresh survival overworld built
	// exactly as the menu builds one, with the weather and the time of day pinned.
	{
		// A run normally ends with the emulator killed, so the last run's world is still on the stick.
		sceIoRemove("testenv.lmsd");
		CraftWorld::RemoveChunkDir("testenv.lmsd");

		StatePlay *testState = new StatePlay();
		testState->InitParametric(true, true, true, MC_TEST_SEED, 0, 0);
		testState->InitCamera();
		testState->SetWorldAndSaveName("testenv", "testenv.lmsd");
		testState->ApplyTestEnvironment(MC_TEST_WEATHER, MC_TEST_TIME);
		testState->SpawnTestMobInFront();
		stateManager.ChangeState(testState);

		while ( stateManager.Running() )
		{
			RunFrame(stateManager);
		}

		sceIoRemove("testenv.lmsd");
		CraftWorld::RemoveChunkDir("testenv.lmsd");
		sceGuTerm();
		sceKernelExitGame();
		return 0;
	}
#endif

#if MC_NETHER_PROBE
	// Development-only: boot straight into a nether world instead of the menu, so the generator can
	// be inspected without navigating the world-creation screen (whose first entry is an on-screen
	// keyboard). Same shape as the MC_PERF_PROBE block above, and the same three calls the menu makes
	// when a world is created. A save name of its own, so a probe run cannot touch a real world.
	{
		StatePlay *probeState = new StatePlay();
		probeState->InitParametric(false, false, false, 20261002u, 8, 'S');
		probeState->InitCamera();
		probeState->SetWorldAndSaveName("netherprobe", "netherprobe.lmsd");
		stateManager.ChangeState(probeState);

		// Round-trip check for the title-screen release. A world Pushes over the menu and frees the
		// menu art; StateMenu::Resume is what brings it back and it calls exactly this. Doing it here
		// proves the reload works without navigating a world-creation screen (whose first entry is an
		// on-screen keyboard), and the release is put straight back so the memory measurement below is
		// not changed by the check.
		TextureHelper::Instance()->EnsureMenuTextures();
		TextureManager::Instance()->LogTextureUsage("after ensure");
		TextureHelper::Instance()->ReleaseMenuTextures();
		TextureManager::Instance()->LogTextureUsage("after release");
		MC_DEBUG_LOG("roundtrip: menu art released, reloaded, and released again\n");
		// A plain Logger line rather than a trace, so a run that faults inside the reload above is
		// distinguishable from one that reached the frame loop. The reload is expected to FAIL here
		// and this is deliberate: it is being asked for while the whole nether world is resident,
		// which is the worst case for the allocator, not the case it actually has to serve.
		Utils::Logger::Instance()->LogMessage("netherprobe: round trip complete, entering the frame loop\n");

		while ( stateManager.Running() )
		{
			RunFrame(stateManager);
		}

		sceGuTerm();
		sceKernelExitGame();
		return 0;
	}
#endif

	//new active state
	BootTrace::Stage("StateMenu allocation");
	StateMenu *statePlay = new StateMenu();
	BootTrace::Stage("StateMenu::Init");
	statePlay->Init();
	BootTrace::Stage("StateManager::ChangeState(StateMenu)");
	stateManager.ChangeState(statePlay);
	BootTrace::Stage("menu entered");

	//trun
	while ( stateManager.Running() )
	{
		RunFrame(stateManager);
	}

	sceGuTerm();			// Terminating the Graphics System
	sceKernelExitGame();	// Quits Application

	return 0;
}
