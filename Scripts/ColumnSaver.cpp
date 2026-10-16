#include "ColumnSaver.h"
#include "ChunkColumn.h"

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <zlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

namespace
{
    enum { kSlots = 2, kPathLen = 256 };
    // deflate at windowBits 12 / memLevel 5 asks for about 40 KB in five allocations; 64 KB leaves
    // room for a zlib that sizes its pending buffer more generously.
    enum { kArenaBytes = 64 * 1024, kOutBytes = 8 * 1024 };
    enum { kWorkerPriority = 40, kWorkerStack = 32 * 1024 };
    enum JobState { kFree = 0, kQueued, kWriting, kDone };

    struct Job
    {
        volatile int state;
        ChunkColumn* col;
        char path[kPathLen];
        int seed;
        int stamp;
        unsigned int seq;
        bool ok;
    };

    Job gJobs[kSlots];
    unsigned int gSeq = 0;

    SceUID gThread = -1;
    SceUID gWorkSema = -1;
    volatile bool gStop = false;
    volatile unsigned int gSavesDone = 0;
    volatile unsigned int gSavesFailed = 0;
    volatile unsigned int gWorkerUs = 0;
    // Where the last failed write stopped (see FailStage) and the sce/zlib code it got.
    volatile int gLastFailStage = 0;
    volatile int gLastFailCode = 0;
    enum FailStage { kFailNone = 0, kFailDeflateInit, kFailOpen, kFailDeflate, kFailWrite, kFailFinish, kFailChdir };

    // The main thread's folder, copied before the worker starts. sceIo keeps a working directory
    // per thread, and a thread that has never called sceIoChdir gets SCE_KERNEL_ERROR_NOCWD
    // (0x8002032c) from sceIoOpen -- on the console, even for an absolute path. PPSSPP does not
    // enforce that, which is why the same writes succeeded there.
    char gCwd[kPathLen];

    // Worker-only. The main thread never reads or writes these.
    unsigned char gArena[kArenaBytes] __attribute__((aligned(16)));
    unsigned int gArenaUsed = 0;
    unsigned char gOut[kOutBytes] __attribute__((aligned(64)));

    voidpf ArenaAlloc(voidpf, uInt items, uInt size)
    {
        unsigned int bytes = ((unsigned int)items * (unsigned int)size + 15u) & ~15u;
        if(gArenaUsed + bytes > (unsigned int)kArenaBytes)
            return Z_NULL;
        void* p = gArena + gArenaUsed;
        gArenaUsed += bytes;
        return p;
    }

    void ArenaFree(voidpf, voidpf)
    {
        // Everything is handed back at once when the job ends.
    }

    bool WriteAll(SceUID fd, const unsigned char* data, int len)
    {
        while(len > 0)
        {
            int n = sceIoWrite(fd, data, len);
            if(n <= 0)
            {
                gLastFailStage = kFailWrite;
                gLastFailCode = n;
                return false;
            }
            data += n;
            len -= n;
        }
        return true;
    }

    bool FlushOut(z_stream& zs, SceUID fd)
    {
        const int have = kOutBytes - (int)zs.avail_out;
        if(have > 0 && !WriteAll(fd, gOut, have))
            return false;
        zs.next_out = gOut;
        zs.avail_out = kOutBytes;
        return true;
    }

    bool WriteColumn(const Job& job)
    {
        const ChunkColumn* col = job.col;
        if(col == 0)
            return false;

        gArenaUsed = 0;
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        zs.zalloc = ArenaAlloc;
        zs.zfree = ArenaFree;
        // Level 1 as before; windowBits 12 + 16 is a 4 KB window inside a gzip member.
        int zr = deflateInit2(&zs, 1, Z_DEFLATED, 12 + 16, 5, Z_DEFAULT_STRATEGY);
        if(zr != Z_OK)
        {
            gLastFailStage = kFailDeflateInit;
            gLastFailCode = zr;
            return false;
        }

        SceUID fd = sceIoOpen(job.path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if(fd < 0)
        {
            gLastFailStage = kFailOpen;
            gLastFailCode = fd;
            deflateEnd(&zs);
            return false;
        }

        // Exactly what SaveColumnFile writes with gzwrite: seven native ints, then the two arrays.
        int header[7];
        header[0] = 0x3243484C; // LHC2
        header[1] = job.seed;
        header[2] = job.stamp;
        header[3] = col->cx;
        header[4] = col->cz;
        header[5] = col->chunkSize;
        header[6] = col->height;
        const int vol = col->Volume();

        const unsigned char* parts[3];
        unsigned int lens[3];
        parts[0] = (const unsigned char*)header;      lens[0] = sizeof(header);
        parts[1] = (const unsigned char*)col->blocks;  lens[1] = sizeof(block_t) * (unsigned int)vol;
        parts[2] = (const unsigned char*)col->settings; lens[2] = sizeof(block_settings_t) * (unsigned int)vol;

        zs.next_out = gOut;
        zs.avail_out = kOutBytes;
        bool ok = true;
        for(int p = 0; p < 3 && ok; p++)
        {
            zs.next_in = (Bytef*)parts[p];
            zs.avail_in = lens[p];
            while(zs.avail_in > 0 && ok)
            {
                int r = deflate(&zs, Z_NO_FLUSH);
                if(r != Z_OK && r != Z_BUF_ERROR)
                {
                    gLastFailStage = kFailDeflate;
                    gLastFailCode = r;
                    ok = false;
                }
                else if(zs.avail_out == 0)
                    ok = FlushOut(zs, fd);
            }
        }
        while(ok)
        {
            int r = deflate(&zs, Z_FINISH);
            if(r == Z_STREAM_END)
            {
                ok = FlushOut(zs, fd);
                break;
            }
            if(r != Z_OK && r != Z_BUF_ERROR)
            {
                gLastFailStage = kFailFinish;
                gLastFailCode = r;
                ok = false;
                break;
            }
            ok = FlushOut(zs, fd);
        }
        deflateEnd(&zs);
        sceIoClose(fd);
        // A partial file is worse than none: LoadColumnFile would reject and log it on every visit.
        if(!ok)
            sceIoRemove(job.path);
        return ok;
    }

    int WorkerMain(SceSize, void*)
    {
        if(gCwd[0] != 0)
        {
            int chdirResult = sceIoChdir(gCwd);
            if(chdirResult < 0)
            {
                gLastFailStage = kFailChdir;
                gLastFailCode = chdirResult;
            }
        }
        for(;;)
        {
            sceKernelWaitSema(gWorkSema, 1, 0);
            if(gStop)
                break;
            int pick = -1;
            for(int i = 0; i < kSlots; i++)
            {
                if(gJobs[i].state != kQueued)
                    continue;
                if(pick < 0 || (int)(gJobs[i].seq - gJobs[pick].seq) < 0)
                    pick = i;
            }
            if(pick < 0)
                continue;   // withdrawn by Reclaim after it was signalled
            Job& job = gJobs[pick];
            job.state = kWriting;
            const unsigned int t0 = sceKernelGetSystemTimeLow();
            job.ok = WriteColumn(job);
            gWorkerUs += sceKernelGetSystemTimeLow() - t0;
            if(job.ok)
                gSavesDone++;
            else
                gSavesFailed++;
            job.state = kDone;
        }
        return 0;
    }

    // Lets the lower-priority worker run. The main thread is the only one that can be here.
    void Yield()
    {
        sceKernelDelayThread(500);
    }
}

namespace ColumnSaver
{
    bool Start()
    {
        if(gThread >= 0)
            return true;
        for(int i = 0; i < kSlots; i++)
        {
            gJobs[i].state = kFree;
            gJobs[i].col = 0;
        }
        gStop = false;
        gCwd[0] = 0;
        if(getcwd(gCwd, sizeof(gCwd)) == 0)
            gCwd[0] = 0;
        else
        {
            int n = (int)strlen(gCwd);
            while(n > 1 && gCwd[n - 1] == '/')
                gCwd[--n] = 0;
        }
        gWorkSema = sceKernelCreateSema("colsave_work", 0, 0, 64, 0);
        if(gWorkSema < 0)
            return false;
        gThread = sceKernelCreateThread("colsave", WorkerMain, kWorkerPriority, kWorkerStack, PSP_THREAD_ATTR_USER, 0);
        if(gThread < 0)
        {
            sceKernelDeleteSema(gWorkSema);
            gWorkSema = -1;
            return false;
        }
        sceKernelStartThread(gThread, 0, 0);
        return true;
    }

    void Stop()
    {
        if(gThread < 0)
            return;
        Drain();
        gStop = true;
        sceKernelSignalSema(gWorkSema, 1);
        sceKernelWaitThreadEnd(gThread, 0);
        sceKernelDeleteThread(gThread);
        sceKernelDeleteSema(gWorkSema);
        gThread = -1;
        gWorkSema = -1;
    }

    // The game's paths are relative to its own folder, which only the main thread's C library knows:
    // the firmware (and PPSSPP) keep a current directory per thread, and the worker has none. So the
    // path is made absolute here, on the main thread, with the folder getcwd reports.
    static bool AbsolutePath(const char* rel, char* out, int outLen)
    {
        if(strchr(rel, ':') != 0)
        {
            if((int)strlen(rel) >= outLen)
                return false;
            strcpy(out, rel);
            return true;
        }
        char cwd[kPathLen];
        if(getcwd(cwd, sizeof(cwd)) == 0)
            return false;
        int n = (int)strlen(cwd);
        while(n > 0 && cwd[n - 1] == '/')
            cwd[--n] = 0;
        if(n + 1 + (int)strlen(rel) >= outLen)
            return false;
        sprintf(out, "%s/%s", cwd, rel);
        return true;
    }

    bool Enqueue(ChunkColumn* col, const char* path, int seed, int stamp)
    {
        if(col == 0 || path == 0)
            return false;
        char full[kPathLen];
        if(!AbsolutePath(path, full, kPathLen))
            return false;
        if(!Start())
            return false;
        for(int i = 0; i < kSlots; i++)
        {
            Job& job = gJobs[i];
            if(job.state != kFree)
                continue;
            job.col = col;
            strcpy(job.path, full);
            job.seed = seed;
            job.stamp = stamp;
            job.seq = gSeq++;
            job.ok = false;
            job.state = kQueued;
            sceKernelSignalSema(gWorkSema, 1);
            return true;
        }
        return false;
    }

    int Reap(ChunkColumn** out, int* outCx, int* outCz, int max)
    {
        int n = 0;
        for(int i = 0; i < kSlots && n < max; i++)
        {
            Job& job = gJobs[i];
            if(job.state != kDone)
                continue;
            out[n] = job.col;
            outCx[n] = job.col ? job.col->cx : 0;
            outCz[n] = job.col ? job.col->cz : 0;
            // A failed write leaves no file; report the column with no coordinates so the caller
            // does not mark one present.
            if(!job.ok)
            {
                outCx[n] = 0x7fffffff;
                outCz[n] = 0x7fffffff;
            }
            n++;
            job.col = 0;
            job.state = kFree;
        }
        return n;
    }

    ChunkColumn* Reclaim(int cx, int cz)
    {
        for(int i = 0; i < kSlots; i++)
        {
            Job& job = gJobs[i];
            if(job.state == kFree || job.col == 0)
                continue;
            if(job.col->cx != cx || job.col->cz != cz)
                continue;
            if(job.state == kQueued)
            {
                // Not started: withdraw it. The worker will wake, find nothing queued, and wait again.
                ChunkColumn* col = job.col;
                job.col = 0;
                job.state = kFree;
                return col;
            }
            while(job.state == kWriting)
                Yield();
            // kDone: the file is on the card and the column is still whole.
            ChunkColumn* col = job.col;
            job.col = 0;
            job.state = kFree;
            return col;
        }
        return 0;
    }

    bool Busy()
    {
        for(int i = 0; i < kSlots; i++)
            if(gJobs[i].state != kFree)
                return true;
        return false;
    }

    void Drain()
    {
        if(gThread < 0)
            return;
        for(;;)
        {
            bool pending = false;
            for(int i = 0; i < kSlots; i++)
                if(gJobs[i].state == kQueued || gJobs[i].state == kWriting)
                    pending = true;
            if(!pending)
                return;
            Yield();
        }
    }

    unsigned int SavesDone() { return gSavesDone; }
    unsigned int SavesFailed() { return gSavesFailed; }
    unsigned int WorkerUs() { return gWorkerUs; }

    void LastFailure(int& stage, int& code)
    {
        stage = gLastFailStage;
        code = gLastFailCode;
    }
}
