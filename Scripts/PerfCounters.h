#ifndef MC_PERFCOUNTERS_H
#define MC_PERFCOUNTERS_H

#include "DebugConfig.h"

// ---------------------------------------------------------------------------
// Development-build *work* counters (Scripts/PerfCounters.cpp).
//
// "Is it slow?" is answered by a frame rate; "what made it slow?" is not. Two
// things on this platform are worth separating and neither is visible from a
// frame rate:
//
//   * **Work that is bounded but expensive** -- one 16x16x16 section remesh per
//     frame is 4,096 cells walked and is the standing budget, so it is not a bug
//     however much it costs.
//   * **Work that is unbounded** -- a producer that outruns its consumer, a scan
//     that grows with how much redstone is nearby, a queue that never empties.
//     That is the shape every "and then everything got slower" report has had in
//     this port, and it is invisible in a frame rate because a slow frame and a
//     frame doing ten times the work look the same.
//
// So these count *operations*, not milliseconds: cells walked, floods run, chunks
// drained, edges taken. Counts do not depend on the clock speed and they rank the
// same on the PSP and in PPSSPP, which is what makes them usable without a device
// in the loop. One interval and one duration are kept (frame period, and the time
// spent inside UpdateWorldProcesses) because those are the two numbers that say
// whether the machine is out of time or out of work.
//
// A line is logged once a second while a world is running:
//
//   perf 12.0s frames=61 fps=61 fpsMin=48 process=31% ...
//
// Counters are reset each second, so every line reads as a rate. High-water marks
// are the peak *queue depth* seen in that second, which is how a queue that never
// drains shows itself.
//
// Everything here disappears from a release build: the macros expand to nothing,
// so an instrumented call site costs exactly zero there. -DMC_DEBUG_MODE=0. Also
// see -DMC_PERF_LOG=0 below, which keeps the dev build's other tooling (the heap
// guards, the boot banner) while silencing this, for when the log itself is in
// the way of what is being measured.
// ---------------------------------------------------------------------------
#ifndef MC_PERF_LOG
    #define MC_PERF_LOG MC_DEBUG_MODE
#endif

// Frame profile: works in a release build (-DMC_DEBUG_MODE=0 -DMC_FRAME_PROFILE=1), so the numbers
// come from the build the player runs rather than one slowed by the heap guards. Instead of the
// per-second lines it keeps totals and writes three lines every kProfileSeconds: the average frame
// split into update / draw / GE wait / vblank wait, each phase as milliseconds per frame, and the
// section and column throughput with the average cost of one of each. Each write flushes the card,
// so it is kept rare.
#ifndef MC_FRAME_PROFILE
    #define MC_FRAME_PROFILE 0
#endif

#define MC_PERF_ENABLED ((MC_DEBUG_MODE && MC_PERF_LOG) || MC_FRAME_PROFILE)

/// Per-frame time attribution, for the question the counters cannot answer: the frame rate says
/// a frame was slow, the counters say what work was queued, and neither says WHICH PART of the
/// frame spent the time. These are wall-clock phases, summed per frame and reported for the
/// worst frame of the second -- because the complaint is a DIP, and a dip is one frame, which an
/// average over a second averages away.
///
/// The split is the one the reported hitch called for: dropping into new ground. `gen` is the
/// terrain generator for one column, `light` is the column's first lighting plus the shared-edge
/// relight that follows it, `demand*` are the same two things when a block READ rather than the
/// streamer drags the column in, `meshjob`/`meshtick` are the geometry catch-up, `sim` is the
/// per-tick world work, `anim` is the animated-tile atlas rewrite, and `drawworld` is mesh
/// submission. Every phase nests inside `process` except drawworld; their sum is not the frame, it
/// is the answer to "where did the frame go" for the phases that are timed.
///
/// Deliberately OUTSIDE the debug guard, so the phase constants exist in a release build too: the
/// call sites pick a phase with a runtime expression (`perfInStreaming ? kPhGen : kPhDemandGen`)
/// and that expression must compile even where PhaseBegin does not exist.
enum PerfPhase
{
    kPhGen = 0,       // WorldGenerator::GenerateColumn, from the streamer
    kPhLight,         // InitColumnLighting + RelightSharedEdges for a new column
    kPhDemandGen,     // GenerateColumn triggered by a block READ rather than the streamer
    kPhDemandLight,   // and the lighting that follows that on-demand column
    kPhStreamOther,   // the rest of UpdateStreaming (scan, unload, pressure, load)
    kPhMeshJob,       // AdvanceMeshJob slice
    kPhMeshSetup,     // the mesh job's setup: BeginMeshCaches + beginRebuild (scratch reserve)
    kPhMeshFinish,    // the mesh job's end step: build/upload/alloc, run outside the slice budget
    kPhMeshDrain,     // the toUpdate drain + FlushRedstoneMeshes
    kPhMeshTick,      // TryMeshOneSection: the scan that picks the next section
    kPhUnmeshed,      // TickUnmeshedColumns: the per-tick column housekeeping
    kPhSim,           // SpawnMobs + the per-tick world processes
    kPhAnim,          // UpdateClockTexture + UpdateWorldAnimation (atlas texel copies)
    kPhDrawWorld,     // mWorld->drawWorld submission
    kPhStateUpdate,   // the whole of the state's Update, UpdateWorldProcesses included
    kPhStateDraw,     // the whole of the state's Draw, EndFrame's two waits included
    kPhGeWait,        // EndFrame: sceGuSync, the GE drawing the frame's list
    kPhVblank,        // EndFrame: sceDisplayWaitVblankStart
    kPhSave,          // SaveColumnFile: deflate + memory-stick write of an unloaded column
    kPhLoad,          // LoadColumnFile: memory-stick read + inflate
    kPhaseCount
};

#if MC_PERF_ENABLED

namespace Perf
{
    /// One entry per thing that has been suspected of being unbounded. The names are what the
    /// logged line prints, and they are grouped the way the frame is: geometry, then the queues
    /// that feed it, then the redstone that queues the most, then the light engine it wakes.
    enum Counter
    {
        kFrames = 0,
        kFrameUs,           // summed, for the average frame period
        kFrameUsWorst,
        kProcessUs,         // summed time inside UpdateWorldProcesses
        kProcessUsWorst,

        // Geometry. `cells` is 4,096 per walk: the number that makes one remesh expensive.
        kMeshFull,
        kMeshOpaque,
        kMeshTransparent,
        kMeshCells,

        // Queues. Depth high-water marks are the unbounded-producer tell.
        kToUpdatePushed,
        kToUpdateDrained,
        kToUpdateDepth,
        kFastDrained,
        kRedstonePushed,
        kRedstoneDrained,
        kRedstoneDepth,
        kLightQueueDepth,
        kWakeBuried,
        kStreamOps,

        // Redstone. `rsWalk` counts cells *visited* by a dust walk, which is the O(n^2) one.
        kRsEvents,
        kRsSeedCells,
        kRsWalkCells,
        kRsDustQueries,
        kTorchFlips,
        kRepeaterFlips,

        // Light. A scan is the 19^3 box a flip queues; a flood is the frontier walk LightFlood runs.
        kLightScans,
        kLightScanCells,
        kLightProcessed,
        kLightFloods,
        kLightFloodCalls,
        kLightFrontierOverflow,   // nodes the frontier could not hold: any value here is lost light
        kLightRemoves,
        kLightRemoveCells,

        // Completions, read by the frame profile: a section whose mesh job reached end(), and a
        // column that finished generating or loading.
        kMeshDone,
        kMeshQuads,         // vertices / 4 across the finished sections' opaque and transparent meshes
        kColumnsDone,

        kCounterCount
    };

    extern unsigned int gWorstPhaseUs[PerfPhase::kPhaseCount];

    /// Add the time between Begin and End to one phase for the current frame. Reentrant per phase is
    /// not supported (nor needed): each call site brackets one region.
    void PhaseBegin(int phase);
    void PhaseEnd(int phase);
    /// Book a duration measured elsewhere (EndFrame's waits) to a phase.
    void PhaseAdd(int phase, unsigned int us);

    extern unsigned int gCounters[kCounterCount];

    /// Report one frame, and log if a second has gone by. The entity counts are live list sizes
    /// logged as they stand, so a frame's cost can be read beside what was in the world.
    void Frame(float dt, int mobs, int drops, int tnts, int arrows, int particles);

    /// Time around UpdateWorldProcesses, the frame's share of the mesh/light/redstone machinery.
    void ProcessBegin();
    void ProcessEnd();

    /// Drop every counter. Called when a world is torn down so a later world's first line is not
    /// reporting the previous world's second (and so a "it only happens after a reload" report can
    /// be told apart from "it always happens").
    void ResetAll();
}

#define MC_PERF_INC(c)    (Perf::gCounters[Perf::c]++)
#define MC_PERF_ADD(c, n) (Perf::gCounters[Perf::c] += (unsigned int)(n))
#define MC_PERF_SET(c, n) (Perf::gCounters[Perf::c] = ((unsigned int)(n) > Perf::gCounters[Perf::c] ? (unsigned int)(n) : Perf::gCounters[Perf::c]))

// The three entry points that are not a counter. They are macros over the same switch for the same
// reason: a release build has no Perf namespace at all, so a bare `Perf::Frame(...)` at a call site
// would not compile out.
#define MC_PERF_FRAME(...)  Perf::Frame(__VA_ARGS__)
#define MC_PERF_RESET()     Perf::ResetAll()
#define MC_PERF_PROCESS_BEGIN() Perf::ProcessBegin()
#define MC_PERF_PROCESS_END()   Perf::ProcessEnd()
#define MC_PERF_PHASE_BEGIN(c)  Perf::PhaseBegin(c)
#define MC_PERF_PHASE_END(c)    Perf::PhaseEnd(c)
#define MC_PERF_PHASE_ADD(c, us) Perf::PhaseAdd(c, us)

#else

#define MC_PERF_INC(c)    ((void)0)
#define MC_PERF_ADD(c, n) ((void)0)
#define MC_PERF_SET(c, n) ((void)0)
#define MC_PERF_FRAME(...)      ((void)0)
#define MC_PERF_RESET()         ((void)0)
#define MC_PERF_PROCESS_BEGIN() ((void)0)
#define MC_PERF_PROCESS_END()   ((void)0)
#define MC_PERF_PHASE_BEGIN(c)  ((void)0)
#define MC_PERF_PHASE_END(c)    ((void)0)
#define MC_PERF_PHASE_ADD(c, us) ((void)0)

#endif
#endif
