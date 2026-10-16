#ifndef MC_DEBUGCONFIG_H
#define MC_DEBUGCONFIG_H

#include <Aurora/Utils/Logger.h>

// ---------------------------------------------------------------------------
// Master development switch.
//
//   MC_DEBUG_MODE 1  ->  development build (default while working on the game)
//   MC_DEBUG_MODE 0  ->  release build
//
// In development mode:
//   * Log.txt is truncated at boot, so the file always describes exactly the run
//     that just happened (it used to grow to tens of thousands of lines spanning
//     dozens of sessions, which made "which boot crashed?" impossible to answer).
//   * A banner records the build time and the EBOOT timestamp, so a log or a
//     crash screen can always be traced back to the exact binary that produced it.
//   * Global operator new/delete gain guard bands (DebugHeap.cpp) so a heap
//     overflow or a double free is reported with the size of the block involved
//     instead of surfacing later as an unexplained "Invalid Memory Access"
//     inside malloc.
//   * MC_DEBUG_LOG breadcrumbs are written at the expensive milestones (world
//     create, generate, mesh, save, load, state changes).
//
// Everything above disappears from the release build: MC_DEBUG_LOG expands to
// nothing and the operator new/delete overrides are not compiled at all.
//
// Defined here, but wrapped so a release build can also force it off from the
// command line with -DMC_DEBUG_MODE=0 without editing this file.
// ---------------------------------------------------------------------------
#ifndef MC_DEBUG_MODE
    #define MC_DEBUG_MODE 1
#endif

// ---------------------------------------------------------------------------
// Force the memory mode (off by default: decide from the heap that was granted).
//
//   -1  decide from the heap the runtime was given (default)
//    0  always run in 32MB mode
//    1  always run in 64MB (expanded) mode
//
// In user mode there is no way to ask what hardware this is: sceKernelGetModel is kernel-only and has
// no user-mode import stub, so the model cannot be named, only the memory partition can be measured.
// On a PSP-2000/3000 whose firmware does not apply MEMSIZE=1 that measurement correctly reports the
// base partition, which is the unwelcome answer this override exists for.
//
//   make EXTRA_DEFS=-DMC_FORCE_MEMORY_MODE=1
// ---------------------------------------------------------------------------
#ifndef MC_FORCE_MEMORY_MODE
    #define MC_FORCE_MEMORY_MODE -1
#endif

// ---------------------------------------------------------------------------
// Work-counter self-test (development builds only, off by default).
//
//   -DMC_PERF_PROBE=1
//
// With this on, booting skips the menu: it generates a world, benchmarks one of each
// redstone/mesh/light operation inside it, and logs the cost of each, then plays the world
// normally with the per-second `perf` lines Scripts/PerfCounters.h describes. That is the only
// way to price those counters without a device in the loop -- the counters say how much work a
// frame did, and this says what a unit of that work costs.
//
// It writes wire and torches into generated terrain and never saves, so it is not a build to
// give anyone to play. Off unless asked for on the command line:
//
//   make EXTRA_DEFS=-DMC_PERF_PROBE=1
// ---------------------------------------------------------------------------
#ifndef MC_PERF_PROBE
    #define MC_PERF_PROBE 0
#endif

// ---------------------------------------------------------------------------
// Nether generator probe (development builds only, OFF by default).
//
//   -DMC_NETHER_PROBE=1
//
// Two halves, both temporary and both meant to come out once the nether generator is settled:
//
//   * boot skips the menu and generates a nether world directly, the way MC_PERF_PROBE does, so the
//     terrain can be looked at without navigating the world-creation menu (whose first entry is an
//     on-screen keyboard).
//   * the generator logs what it produced for the spawn region -- block counts and the open fraction
//     of each height band -- because "it looks like the nether" is not a check that would catch an
//     all-solid world, a world that is all lava, or a spawn inside the lava sea.
//
//   make EXTRA_DEFS=-DMC_NETHER_PROBE=1
// ---------------------------------------------------------------------------
#ifndef MC_NETHER_PROBE
    #define MC_NETHER_PROBE 0
#endif

// ---------------------------------------------------------------------------
// Auto-walk probe (development builds only, OFF by default).
//
//   -DMC_AUTO_WALK=1
//
// Holds the analog stick forward (turning through a square) so the player walks the whole session with
// nobody at the controls. It exists because the symptoms still open on this build -- block textures
// that never load until a block is placed, gaps opening between chunks while walking, fps dipping as
// new ground streams in -- appear ONLY while moving, and movement cannot be driven from outside the
// game: this game walks with the analog nub, and PPSSPP's debugger cannot hold that stick (measured
// against five event shapes; the one that looks like it works deflects the stick for a single frame
// and is overwritten by the next real pad poll -- see Tools/Emulator/analogtest.ps1). Injecting at the pad
// is a dead end, so the probe injects where the game itself reads the pad: the analog -> direction
// conversion in StatePlay::keyHold. What is measured is therefore the real movement path -- speed,
// camera-relative direction, collision, streaming and meshing -- rather than a side door into the
// player's position that would skip all of it.
//
// Not a build anyone plays: the player walks off on its own. Off unless asked for:
//
//   make EXTRA_DEFS=-DMC_AUTO_WALK=1
// ---------------------------------------------------------------------------
#ifndef MC_AUTO_WALK
    #define MC_AUTO_WALK 0
#endif

// ---------------------------------------------------------------------------
// Test environment (OFF by default).
//
//   -DMC_TEST_ENV=1 [-DMC_TEST_WEATHER=1] [-DMC_TEST_TIME=12] [-DMC_TEST_SEED=20261015]
//
// Boots straight into a fresh survival overworld called "testenv" (testenv.lmsd beside the EBOOT), with
// the weather and the time of day pinned, so a test starts in a known world with nobody at the menus.
// MC_TEST_WEATHER: 0 clear, 1 rain, held for the whole run. MC_TEST_TIME: hour of the day, or -1 to
// leave the generator's. Combine with MC_AUTO_WALK to walk it.
//
// A run ends with the emulator being killed, so the world cannot be relied on to delete itself at
// exit. It is deleted at the start of the next run instead, and again on a clean exit.
// ---------------------------------------------------------------------------
#ifndef MC_TEST_ENV
    #define MC_TEST_ENV 0
#endif
#ifndef MC_TEST_WEATHER
    #define MC_TEST_WEATHER 0
#endif
#ifndef MC_TEST_TIME
    #define MC_TEST_TIME -1
#endif
#ifndef MC_TEST_SEED
    #define MC_TEST_SEED 20261015u
#endif
// Named mob planted a few blocks in front of the camera after the test world boots. Empty
// (the default) plants nothing. Pass the mob's ordinary name:
//
//   -DMC_TEST_MOB=chicken
//
// The mob is pose-locked: it does not walk, panic, or get parked. It faces you and stays
// there so a screenshot or a walk-around can look at the model. Off in every playable build.
#ifndef MC_TEST_MOB
    #define MC_TEST_MOB
#endif
#define MC_TEST_MOB_STR1(x) #x
#define MC_TEST_MOB_STR(x) MC_TEST_MOB_STR1(x)

// ---------------------------------------------------------------------------
// Auto-fly probe (OFF by default).
//
//   -DMC_AUTO_FLY=1
//
// The case the holes are reported in: creative sprint-flying, 11.2 blocks/s, where the walk above
// covers 4. The player is carried at MC_AUTO_FLY_SPEED along a staircase (east, then north, then east
// ...) at a fixed height, facing the way it travels and pitched slightly down, so every leg runs into
// ground that has never been loaded and every turn brings a new edge into view. Unlike the walk this
// does place the player directly -- the survival state has no flight -- so it measures streaming and
// meshing against a flight path, not the movement code.
// ---------------------------------------------------------------------------
#ifndef MC_AUTO_FLY
    #define MC_AUTO_FLY 0
#endif
#ifndef MC_AUTO_FLY_SPEED
    #define MC_AUTO_FLY_SPEED 11.2f
#endif

// ---------------------------------------------------------------------------
// Mesh only what the camera can see (ON by default; 0 restores the old behaviour).
//
//   -DMC_MESH_VISIBLE_ONLY=0
//
// Until this existed, a section was given geometry as soon as its column was resident, as long as it
// was inside a symmetric box around the player (`playerZoneBB`). That box extends equally behind, above
// and below, so roughly half of everything meshed was geometry for terrain that had never been on
// screen. On this device that is the dominant mesh cost, and mesh memory is what the nether runs out of
// (75 sections meshed for 36 resident columns, measured).
//
// With this on, a section is only built when the camera frustum can see it (with one chunk of margin),
// or when a turn has re-armed the mesher. Nothing is ever unmeshed: a section that has been on screen
// keeps its geometry. Simulation is untouched -- blocks, light, mobs, redstone and growth never read a
// mesh, and the growth tick path actually runs MORE often for an unmeshed section, not less.
//
// The switch exists so the change can be A/B'd on the emulator against the old behaviour with
// everything else identical. See SectionInView in CraftWorld2.cpp.
// ---------------------------------------------------------------------------
#ifndef MC_MESH_VISIBLE_ONLY
    #define MC_MESH_VISIBLE_ONLY 1
#endif

// ---------------------------------------------------------------------------
// Stop after the splash screen (development builds only, OFF by default).
//
//   -DMC_INIT_TRACE_STOP=1
//
// With this on, the game initialises every subsystem, draws the splash, and exits instead of
// entering the menu. The point is to make the last line of trace.txt (see Scripts/InitTrace.h)
// mean exactly one thing: everything above it completed and was named, so a console that stops
// part-way through startup is localised by that line alone and cannot be confused with a fault
// that happens later, at the menu or in a world.
//
// This was the default while the startup crash was being chased. It is now opt-in, because the
// faults it was used to find are fixed and leaving it on hides the menu. Turn it back on when a
// startup fault has to be separated from a menu or in-world one:
//
//   make EXTRA_DEFS=-DMC_INIT_TRACE_STOP=1
// ---------------------------------------------------------------------------
#ifndef MC_INIT_TRACE_STOP
    #define MC_INIT_TRACE_STOP 0
#endif

// ---------------------------------------------------------------------------
// Force the low-memory sound set (development only, off by default).
//
//   -DMC_FORCE_COMPACT_SOUNDS=1
//
// SoundManager picks Assets/SoundsLow/ (22 kHz mono, ~1.6 MB) instead of Assets/Sounds/ (44.1 kHz
// stereo, ~6.6 MB) when the free heap is under 24 MB. A desktop emulator always has more than that, a
// PSP-1000 does not, so the two run *different clip sets* -- which means the compact path is not
// covered by emulator testing at all, and a fault in it can only ever appear on hardware.
//
// This switch forces the console's choice in the emulator so that path can be exercised locally.
// ---------------------------------------------------------------------------
#ifndef MC_FORCE_COMPACT_SOUNDS
    #define MC_FORCE_COMPACT_SOUNDS 0
#endif

#if MC_DEBUG_MODE

    #define MC_DEBUG_LOG(...) Aurora::Utils::Logger::Instance()->LogMessage(__VA_ARGS__)

#else

    #define MC_DEBUG_LOG(...) ((void)0)

#endif

// Starts a session: clears the log when debugging, prints the banner, and takes a
// baseline of the heap counters. Safe to call more than once. Implemented in
// Scripts/DebugHeap.cpp, and a no-op in release builds.
void MC_BootSession();

// Logs the live-block / live-byte counters and a malloc health line. Called at
// milestones from the game so a leak or a corrupted heap is visible in the log
// *before* it turns into a crash. No-op in release builds.
void MC_DebugHeapCheckpoint(const char* tag);

#endif
