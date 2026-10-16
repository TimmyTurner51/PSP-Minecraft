// ---------------------------------------------------------------------------
// Development-build work counters -- see Scripts/PerfCounters.h for what they are
// for and how to read the logged line. Nothing here is compiled into a release
// build; in a release the whole file is an empty translation unit.
// ---------------------------------------------------------------------------

#include "PerfCounters.h"

#if MC_PERF_ENABLED

#include <pspkernel.h>

#include <Aurora/Utils/Logger.h>
#include "ColumnSaver.h"

namespace Perf
{
    unsigned int gCounters[kCounterCount];
    unsigned int gWorstPhaseUs[kPhaseCount];

    namespace
    {
        // Accumulated over the reporting second. The counters are reset every second, so the log
        // reads as a rate; these two are the intervals those rates belong to.
        float gSecond = 0.0f;
        float gFramesThisSecond = 0.0f;
        float gSecondsElapsed = 0.0f;

        // Frame period, measured between Frame() calls rather than inside the renderer: Frame() is
        // called once per frame from UpdateWorldProcesses, which runs at the top of the play state's
        // Draw, so the interval between two calls is the frame the player felt -- including the
        // drawing, which is the half a phase timer would miss.
        unsigned int gLastFrameStamp = 0;
        unsigned int gProcessStart = 0;

        // Per-frame phase attribution. `gFramePhaseUs` accumulates the frame currently being built and
        // is copied wholesale into `gWorstPhaseUs` when that frame turns out to be the slowest of the
        // second, which is the frame the dip complaint is about. One start stamp per phase because a
        // phase brackets one region; phases do not nest, so no stack is needed.
        unsigned int gFramePhaseUs[kPhaseCount];
        unsigned int gPhaseStart[kPhaseCount];
        // Nesting depth per phase. The phases DID nest in practice -- a column's relight reads its
        // neighbours, which can bring another column in, which re-enters the same phase bracket -- and
        // a plain begin/end pair then overwrites the outer start stamp, books one interval twice and
        // attributes time to a phase that did not run. Counting depth fixes that: only the outermost
        // begin stamps and only the outermost end accumulates.
        unsigned int gPhaseDepth[kPhaseCount];

#if MC_FRAME_PROFILE
        // Totals over the profile window. Phases are summed over every frame, not just the worst,
        // because the question here is throughput -- where the average frame goes -- not a dip.
        const float kProfileSeconds = 15.0f;
        float gProfSec = 0.0f;
        unsigned int gProfFrames = 0;
        unsigned int gProfFrameUs = 0;
        unsigned int gProfWorstUs = 0;
        // Frames by how many vblanks they took: 1 (60 fps), 2 (30), 3 (20), more.
        unsigned int gProfBucket[4];
        unsigned int gProfPhaseUs[kPhaseCount];
        unsigned int gProfMeshDone = 0;
        unsigned int gProfMeshQuads = 0;
        unsigned int gProfColumnsDone = 0;
        // The saver thread's running totals at the start of the window, so the line shows the window.
        unsigned int gProfSaverDoneAt = 0;
        unsigned int gProfSaverFailAt = 0;
        unsigned int gProfSaverUsAt = 0;

        /// Hundredths of a millisecond per frame, printed as an integer.
        unsigned int PerFrame(unsigned int totalUs, unsigned int frames)
        {
            return frames > 0 ? (unsigned int)((totalUs / 10u) / frames) : 0u;
        }

        unsigned int Each(unsigned int totalUs, unsigned int count)
        {
            return count > 0 ? totalUs / count : 0u;
        }

        unsigned int Pct(unsigned int part, unsigned int whole)
        {
            return whole > 0 ? (100u * part + whole / 2u) / whole : 0u;
        }

        void ProfileReset()
        {
            gProfSec = 0.0f;
            gProfFrames = 0;
            gProfFrameUs = 0;
            gProfWorstUs = 0;
            for(int i = 0; i < 4; i++)
                gProfBucket[i] = 0;
            for(int i = 0; i < kPhaseCount; i++)
                gProfPhaseUs[i] = 0;
            gProfMeshDone = 0;
            gProfMeshQuads = 0;
            gProfColumnsDone = 0;
            gProfSaverDoneAt = ColumnSaver::SavesDone();
            gProfSaverFailAt = ColumnSaver::SavesFailed();
            gProfSaverUsAt = ColumnSaver::WorkerUs();
        }

        void ProfileReport()
        {
            const unsigned int f = gProfFrames;
            const unsigned int fps10 = gProfSec > 0.0f ? (unsigned int)((float)f * 10.0f / gProfSec + 0.5f) : 0u;
            const unsigned int* p = gProfPhaseUs;
            // Draw minus its two waits is the CPU time spent building the display list.
            const unsigned int waits = p[kPhGeWait] + p[kPhVblank];
            const unsigned int drawCpu = p[kPhStateDraw] > waits ? p[kPhStateDraw] - waits : 0u;
            // Scan is what the mesh job spent outside its cache fill and its end step.
            const unsigned int setupFinish = p[kPhMeshSetup] + p[kPhMeshFinish];
            const unsigned int scan = p[kPhMeshJob] > setupFinish ? p[kPhMeshJob] - setupFinish : 0u;
            const unsigned int genAll = p[kPhGen] + p[kPhDemandGen];
            const unsigned int lightAll = p[kPhLight] + p[kPhDemandLight];
            const unsigned int secs = (unsigned int)(gProfSec + 0.5f);

            Aurora::Utils::Logger::Instance()->LogMessage(
                "prof %us frames=%u fps=%u.%u vblanks 1=%u%% 2=%u%% 3=%u%% 4+=%u%% avgFrame=%u.%02ums worst=%ums",
                (unsigned int)gSecondsElapsed, f, fps10 / 10u, fps10 % 10u,
                Pct(gProfBucket[0], f), Pct(gProfBucket[1], f), Pct(gProfBucket[2], f), Pct(gProfBucket[3], f),
                PerFrame(gProfFrameUs, f) / 100u, PerFrame(gProfFrameUs, f) % 100u, gProfWorstUs / 1000u);

            Aurora::Utils::Logger::Instance()->LogMessage(
                "  ms/frame x100: update=%u (stream=%u gen=%u light=%u save=%u load=%u meshJob=%u [fill=%u scan=%u end=%u] meshTick=%u drain=%u unmeshed=%u sim=%u anim=%u) drawCpu=%u (drawWorld=%u) geWait=%u vblank=%u",
                PerFrame(p[kPhStateUpdate], f), PerFrame(p[kPhStreamOther], f), PerFrame(genAll, f),
                PerFrame(lightAll, f), PerFrame(p[kPhSave], f), PerFrame(p[kPhLoad], f),
                PerFrame(p[kPhMeshJob], f), PerFrame(p[kPhMeshSetup], f),
                PerFrame(scan, f), PerFrame(p[kPhMeshFinish], f), PerFrame(p[kPhMeshTick], f),
                PerFrame(p[kPhMeshDrain], f), PerFrame(p[kPhUnmeshed], f), PerFrame(p[kPhSim], f),
                PerFrame(p[kPhAnim], f), PerFrame(drawCpu, f), PerFrame(p[kPhDrawWorld], f),
                PerFrame(p[kPhGeWait], f), PerFrame(p[kPhVblank], f));

            const unsigned int saverDone = ColumnSaver::SavesDone() - gProfSaverDoneAt;
            const unsigned int saverFail = ColumnSaver::SavesFailed() - gProfSaverFailAt;
            const unsigned int saverUs = ColumnSaver::WorkerUs() - gProfSaverUsAt;
            Aurora::Utils::Logger::Instance()->LogMessage(
                "  sections=%u (%u.%u/s) quads each=%u us each: fill=%u scan=%u end=%u total=%u | columns=%u (%u.%u/s) us each: gen=%u light=%u | saver: %u written %u failed, %u us each off-frame",
                gProfMeshDone, secs ? (gProfMeshDone * 10u / secs) / 10u : 0u, secs ? (gProfMeshDone * 10u / secs) % 10u : 0u,
                Each(gProfMeshQuads, gProfMeshDone),
                Each(p[kPhMeshSetup], gProfMeshDone), Each(scan, gProfMeshDone), Each(p[kPhMeshFinish], gProfMeshDone),
                Each(p[kPhMeshJob], gProfMeshDone),
                gProfColumnsDone, secs ? (gProfColumnsDone * 10u / secs) / 10u : 0u, secs ? (gProfColumnsDone * 10u / secs) % 10u : 0u,
                Each(genAll, gProfColumnsDone), Each(lightAll, gProfColumnsDone),
                saverDone, saverFail, Each(saverUs, saverDone + saverFail));
        }
#endif

        unsigned int Now()
        {
            return sceKernelGetSystemTimeLow();
        }

        /// A depth, so the block below can print it as a clean integer rate.
        unsigned int Rate(unsigned int total, float seconds)
        {
            if(seconds <= 0.0f)
                return 0;
            return (unsigned int)((float)total / seconds + 0.5f);
        }
    }

    void ProcessBegin()
    {
        gProcessStart = Now();
    }

    void PhaseBegin(int phase)
    {
        if(phase < 0 || phase >= kPhaseCount)
            return;
        if(gPhaseDepth[phase]++ == 0)
            gPhaseStart[phase] = Now();
    }

    void PhaseEnd(int phase)
    {
        if(phase < 0 || phase >= kPhaseCount)
            return;
        if(gPhaseDepth[phase] == 0)
            return;
        if(--gPhaseDepth[phase] == 0)
        {
            // Signed difference: the system clock can wrap between Begin and End and, at ~71 minutes
            // of uptime, it does. A wrapped interval would otherwise read as a four-billion-
            // microsecond phase and dominate the report.
            unsigned int us = (unsigned int)((int)Now() - (int)gPhaseStart[phase]);
            gFramePhaseUs[phase] += us;
        }
    }

    void PhaseAdd(int phase, unsigned int us)
    {
        if(phase < 0 || phase >= kPhaseCount)
            return;
        gFramePhaseUs[phase] += us;
    }

    void ProcessEnd()
    {
        unsigned int us = Now() - gProcessStart;
        gCounters[kProcessUs] += us;
        if(us > gCounters[kProcessUsWorst])
            gCounters[kProcessUsWorst] = us;
    }

    void ResetAll()
    {
        for(int i = 0; i < kCounterCount; i++)
            gCounters[i] = 0;
        for(int i = 0; i < kPhaseCount; i++)
        {
            gWorstPhaseUs[i] = 0;
            gFramePhaseUs[i] = 0;
            gPhaseDepth[i] = 0;
        }
        gSecond = 0.0f;
        gFramesThisSecond = 0.0f;
        gLastFrameStamp = 0;
#if MC_FRAME_PROFILE
        ProfileReset();
#endif
    }

    void Frame(float dt, int mobs, int drops, int tnts, int arrows, int particles)
    {
        unsigned int now = Now();

        // The first call of a run has no previous stamp; take it and report nothing, so the first
        // logged second is not a frame period of zero divided into a rate.
        if(gLastFrameStamp != 0)
        {
            unsigned int frameUs = now - gLastFrameStamp;
#if MC_FRAME_PROFILE
            gProfFrames++;
            gProfFrameUs += frameUs;
            if(frameUs > gProfWorstUs)
                gProfWorstUs = frameUs;
            // A vblank is 16,683 us; the half-vblank margins absorb clock jitter.
            const unsigned int vb = (frameUs + 8341u) / 16683u;
            gProfBucket[vb <= 1u ? 0 : (vb >= 4u ? 3 : (int)vb - 1)]++;
            for(int i = 0; i < kPhaseCount; i++)
                gProfPhaseUs[i] += gFramePhaseUs[i];
#endif
            gCounters[kFrameUs] += frameUs;
            if(frameUs > gCounters[kFrameUsWorst])
            {
                gCounters[kFrameUsWorst] = frameUs;
                // Snapshot this frame's phase attribution. This is what makes the report answer the
                // question actually asked: average phase times hide a dip, and a dip is one frame.
                for(int i = 0; i < kPhaseCount; i++)
                    gWorstPhaseUs[i] = gFramePhaseUs[i];
            }
        }
        gLastFrameStamp = now;

        // The next frame's phases start from zero here, so what is read above is exactly one frame.
        for(int i = 0; i < kPhaseCount; i++)
            gFramePhaseUs[i] = 0;

        gCounters[kFrames]++;
        gFramesThisSecond += 1.0f;
        gSecond += dt;
        gSecondsElapsed += dt;

#if MC_FRAME_PROFILE
        gProfMeshDone += gCounters[kMeshDone];
        gProfMeshQuads += gCounters[kMeshQuads];
        gProfColumnsDone += gCounters[kColumnsDone];
        gCounters[kMeshDone] = 0;
        gCounters[kMeshQuads] = 0;
        gCounters[kColumnsDone] = 0;
        gProfSec += dt;
        if(gProfSec >= kProfileSeconds)
        {
            ProfileReport();
            ProfileReset();
            // The report's own card write lands in the next frame period; do not book it.
            gLastFrameStamp = 0;
        }
#endif

#if !(MC_DEBUG_MODE && MC_PERF_LOG)
        // Profile only: the per-second lines below are the development build's.
        gSecond = 0.0f;
        gFramesThisSecond = 0.0f;
        gCounters[kFrameUs] = 0;
        gCounters[kFrameUsWorst] = 0;
        gCounters[kProcessUs] = 0;
        gCounters[kProcessUsWorst] = 0;
        gCounters[kFrames] = 0;
        for(int i = kMeshFull; i < kCounterCount; i++)
            gCounters[i] = 0;
        return;
#endif

        if(gSecond < 1.0f)
            return;

        const float sec = gSecond;
        const unsigned int frames = (unsigned int)gFramesThisSecond;
        const unsigned int avgFrameUs = (frames > 0) ? (gCounters[kFrameUs] / frames) : 0;
        const unsigned int avgProcessUs = (frames > 0) ? (gCounters[kProcessUs] / frames) : 0;
        // The share of the frame the mesh/light/redstone machinery owns, which is the number that
        // says whether the rest of the game had any frame left to run in.
        const unsigned int processShare = (avgFrameUs > 0) ? (100 * avgProcessUs / avgFrameUs) : 0;

        Aurora::Utils::Logger::Instance()->LogMessage(
            "perf %us frames=%u fps=%u worstFrame=%ums process=%u%%(worst %ums) mobs=%d drops=%d tnt=%d arrows=%d part=%d",
            (unsigned int)gSecondsElapsed, frames, (unsigned int)((float)frames / sec + 0.5f),
            gCounters[kFrameUsWorst] / 1000u, processShare, gCounters[kProcessUsWorst] / 1000u,
            mobs, drops, tnts, arrows, particles);

        Aurora::Utils::Logger::Instance()->LogMessage(
            "  mesh/s full=%u opaque=%u transp=%u cells=%u | queues/s toUpdate +%u -%u peak=%u fast -%u redstone +%u -%u peak=%u light peak=%u wake=%u stream=%u",
            Rate(gCounters[kMeshFull], sec), Rate(gCounters[kMeshOpaque], sec),
            Rate(gCounters[kMeshTransparent], sec), Rate(gCounters[kMeshCells], sec),
            Rate(gCounters[kToUpdatePushed], sec), Rate(gCounters[kToUpdateDrained], sec),
            gCounters[kToUpdateDepth], Rate(gCounters[kFastDrained], sec),
            Rate(gCounters[kRedstonePushed], sec), Rate(gCounters[kRedstoneDrained], sec),
            gCounters[kRedstoneDepth], gCounters[kLightQueueDepth],
            Rate(gCounters[kWakeBuried], sec), Rate(gCounters[kStreamOps], sec));

        Aurora::Utils::Logger::Instance()->LogMessage(
            "  redstone/s events=%u seedCells=%u walkCells=%u dustQueries=%u torches=%u repeaters=%u | light/s scans=%u(%u) scanCells=%u floods=%u floodCalls=%u frontierOverflow=%u removes=%u removeCells=%u",
            Rate(gCounters[kRsEvents], sec), Rate(gCounters[kRsSeedCells], sec),
            Rate(gCounters[kRsWalkCells], sec), Rate(gCounters[kRsDustQueries], sec),
            Rate(gCounters[kTorchFlips], sec), Rate(gCounters[kRepeaterFlips], sec),
            Rate(gCounters[kLightScans], sec), Rate(gCounters[kLightProcessed], sec),
            Rate(gCounters[kLightScanCells], sec),
            Rate(gCounters[kLightFloods], sec), Rate(gCounters[kLightFloodCalls], sec),
            Rate(gCounters[kLightFrontierOverflow], sec),
            Rate(gCounters[kLightRemoves], sec), Rate(gCounters[kLightRemoveCells], sec));

        // Where the slowest frame of the second went. Every value is microseconds in ONE frame, so
        // these deliberately do not sum with the rates above. `gen` + `light` are a column arriving,
        // `meshjob` + `meshdrain` + `meshtick` are the geometry catching up to it, and the rest is
        // what the frame was doing anyway.
        Aurora::Utils::Logger::Instance()->LogMessage(
            "  worstFrame %ums phases(us): stream=%u(gen=%u light=%u) demand(gen=%u light=%u) | meshJob=%u meshSetup=%u meshFinish=%u meshDrain=%u meshTick=%u unmeshed=%u | sim=%u anim=%u drawWorld=%u",
            gCounters[kFrameUsWorst] / 1000u,
            gWorstPhaseUs[kPhStreamOther], gWorstPhaseUs[kPhGen], gWorstPhaseUs[kPhLight],
            gWorstPhaseUs[kPhDemandGen], gWorstPhaseUs[kPhDemandLight],
            gWorstPhaseUs[kPhMeshJob], gWorstPhaseUs[kPhMeshSetup], gWorstPhaseUs[kPhMeshFinish],
            gWorstPhaseUs[kPhMeshDrain], gWorstPhaseUs[kPhMeshTick], gWorstPhaseUs[kPhUnmeshed],
            gWorstPhaseUs[kPhSim], gWorstPhaseUs[kPhAnim], gWorstPhaseUs[kPhDrawWorld]);

        // Reset the per-second totals; the accumulated wall clock is above and stays.
        gCounters[kFrameUs] = 0;
        gCounters[kFrameUsWorst] = 0;
        gCounters[kProcessUs] = 0;
        gCounters[kProcessUsWorst] = 0;
        gCounters[kFrames] = 0;
        for(int i = 0; i < kPhaseCount; i++)
            gWorstPhaseUs[i] = 0;
        for(int i = kMeshFull; i < kCounterCount; i++)
            gCounters[i] = 0;

        gSecond = 0.0f;
        gFramesThisSecond = 0.0f;
    }
}

#else

// Release build: DebugConfig.h has already reduced every macro to nothing, so there is no code to
// define. This branch exists so the file is still a valid translation unit.

#endif
