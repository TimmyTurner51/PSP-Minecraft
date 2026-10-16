// ---------------------------------------------------------------------------
// Development-build heap instrumentation (see Scripts/DebugConfig.h).
//
// The crash this was written for was an "Invalid Memory Access: Write Word at
// 0x03f0000c" with the PC inside _malloc_r -- i.e. something had already walked
// off the end of a heap block and smashed malloc's free list, and the crash only
// surfaced later when an unrelated allocation touched the damage. That gives no
// clue which buffer was overrun.
//
// With MC_DEBUG_MODE on, every C++ new/delete goes through a guard band:
//
//      [ 32-byte header | payload ... | 8-byte tail ]
//
// The header carries a magic, the requested size and a sequence number; the tail
// repeats a magic and the size. Blocks are also kept in a doubly-linked list, so a
// checkpoint can walk every live block and report an overrun *at the milestone
// where it appeared* instead of at the free that happens to trip over it.
//
// Nothing in this file is compiled into a release build except the two no-op
// entry points, so the shipping game pays nothing for it.
// ---------------------------------------------------------------------------

#include "DebugConfig.h"
#include "InitTrace.h"

#include <new>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <pspkernel.h>
#include <pspiofilemgr.h>

#if MC_DEBUG_MODE

namespace
{
    const unsigned int kHeadMagic = 0x4D435042u; // "MCPB"
    const unsigned int kTailMagic = 0x4D435054u; // "MCPT"
    const unsigned int kFreedMagic = 0x4D434652u; // "MCFR" stamped over a freed head

    // 32 bytes keeps the payload aligned exactly like malloc left it (including 16-byte
    // alignment for VFPU-friendly buffers), and gives room for the list pointers and the
    // address that called new, so a report can name the call site.
    struct DbgBlock
    {
        unsigned int headMagic;
        unsigned int size;       // payload size the caller asked for
        unsigned int seq;        // allocation order, so a report can be matched to a log line
        unsigned int pad;
        DbgBlock* prev;
        DbgBlock* next;
        void* allocCaller;       // __builtin_return_address(0) inside operator new
    };
    enum { kHeaderBytes = 32 };
    static_assert(sizeof(DbgBlock) <= kHeaderBytes, "guard header outgrew its slot");
    enum { kTailBytes = 8 };

    // Where the tail guard goes, measured from the payload, rounded up to a word boundary.
    //
    // This is the fix for the crash this file spent a long time hunting. MIPS has no unaligned store:
    // a 4-byte store to an address that is not a multiple of four raises an Address Error exception and
    // the console stops. PPSSPP accepts it silently, so the fault is invisible in the emulator and
    // fatal on the device.
    //
    // The tail used to be placed at base + kHeaderBytes + size -- with no rounding. That is aligned only
    // when the requested size happens to be a multiple of four. The first allocation this game ever
    // makes asks for 31 bytes (libstdc++ growing the 16-character "Assets/Textures/" past its
    // small-string buffer), which put the tail at base+63: three bytes off, every time, on the very
    // first store the guard band performs. Measured on the console: base=0x08bc70d0, so the tail was
    // 0x08bc710f.
    //
    // Rounding here costs at most three bytes of padding and makes the address provably aligned
    // whenever the payload is word-aligned, which malloc guarantees and the payload trace confirms.
    static inline unsigned int TailOffset(unsigned int size)
    {
        return (size + 3u) & ~3u;
    }

    unsigned int gLiveBlocks = 0;
    unsigned int gLiveBytes = 0;
    unsigned int gPeakBlocks = 0;
    unsigned int gPeakBytes = 0;
    unsigned int gAllocSeq = 0;
    unsigned int gAllocCount = 0;
    unsigned int gFreeCount = 0;
    unsigned int gErrorCount = 0;
    DbgBlock* gHead = 0;

    // Every free is remembered for a while, so a second delete of the same address can say
    // *when* it was first freed instead of just "already freed". That gap is what points at
    // the code that deleted the object twice (a first free during world setup, a second one
    // either a few hundred allocations later or at save time).
    struct FreeRecord
    {
        void* payload;
        unsigned int seq;       // allocation sequence of the block
        unsigned int order;     // which free this was
        unsigned int size;      // payload size, so the report can name the object size
        void* allocCaller;      // who called new for it
        void* freeCaller;       // who called delete the first time
    };
    enum { kFreeRingSize = 1024 };
    FreeRecord gFreeRing[kFreeRingSize];

    const FreeRecord* FindFreeRecord(void* payload)
    {
        unsigned int n = gFreeCount < kFreeRingSize ? gFreeCount : kFreeRingSize;
        for(unsigned int i = 0; i < n; i++)
        {
            const FreeRecord& r = gFreeRing[i];
            if(r.payload == payload)
                return &r;
        }
        return 0;
    }

    void RememberFree(void* payload, const DbgBlock* b, void* freeCaller)
    {
        FreeRecord& r = gFreeRing[gFreeCount % kFreeRingSize];
        r.payload = payload;
        r.seq = b->seq;
        r.order = gFreeCount;
        r.size = b->size;
        r.allocCaller = b->allocCaller;
        r.freeCaller = freeCaller;
    }

    // Kept tiny and allocation-free: this runs inside operator new/delete.
    void GuardReport(const char* what, unsigned int seq, unsigned int size)
    {
        gErrorCount++;
        Aurora::Utils::Logger::Instance()->LogMessage(
            "HEAP GUARD: %s (seq=%u size=%u liveBlocks=%u liveBytes=%u)\n",
            what, seq, size, gLiveBlocks, gLiveBytes);
    }

    void* GuardAlloc(size_t size)
    {
        if(size == 0)
            size = 1;

        // The first few allocations are traced in full -- request, what malloc gave back, and the
        // alignment of the payload -- because the program's first allocation is where a console that
        // stops during static initialisation is most likely to be stopping (see the constructor trace
        // in Scripts/TextureHelper.cpp). Bounded to a handful of lines: past those, the heap is known
        // to work and the noise would drown the signal.
        //
        // The alignment line is the one worth understanding. This guard band adds 32 bytes in front of
        // the payload, which the comment below claims preserves malloc's alignment. It preserves it
        // only if malloc returned 16-byte aligned memory in the first place; if it returned 8-byte
        // aligned memory, every payload here is 8-byte aligned, and a 16-byte VFPU or sceGu access on
        // one of those buffers faults on a console while the emulator accepts it. Reporting the low
        // four bits settles it with a number instead of an argument.
        bool traceAlloc = (gAllocSeq < 12);
        if(traceAlloc)
            MC_TraceUInt("alloc: request bytes", (unsigned int)size);

        char* base = (char*)malloc(kHeaderBytes + TailOffset((unsigned int)size) + kTailBytes);
        if(base == 0)
        {
            if(traceAlloc)
                MC_Trace("alloc: malloc returned NULL");
            return 0;
        }

        if(traceAlloc)
        {
            MC_TracePtr("alloc: malloc base", base);
            MC_TraceUInt("alloc: payload mod 16", (unsigned int)(base + kHeaderBytes) & 15u);
        }

        DbgBlock* b = (DbgBlock*)base;
        b->headMagic = kHeadMagic;
        b->size = (unsigned int)size;
        b->seq = ++gAllocSeq;
        b->pad = 0;
        b->prev = 0;
        b->next = gHead;
        b->allocCaller = __builtin_return_address(0);
        if(gHead)
            gHead->prev = b;
        gHead = b;

        unsigned int* tail = (unsigned int*)(base + kHeaderBytes + TailOffset((unsigned int)size));
        tail[0] = kTailMagic;
        tail[1] = (unsigned int)size;

        // Reported after the tail write, so this line is evidence that the store at that address
        // completed. On the console the previous build stopped one statement short of here, with the
        // arithmetic above saying the address was three bytes off a word boundary; a tail mod 4 of 0
        // here, on a run that then continues, is what confirms the diagnosis rather than assuming it.
        if(traceAlloc)
        {
            MC_TraceUInt("alloc: tail address mod 4", (unsigned int)tail & 3u);
            MC_TraceUInt("alloc: did not stop at the tail write", 1);
        }

        gLiveBlocks++;
        gLiveBytes += (unsigned int)size;
        gAllocCount++;
        if(gLiveBlocks > gPeakBlocks)
            gPeakBlocks = gLiveBlocks;
        if(gLiveBytes > gPeakBytes)
            gPeakBytes = gLiveBytes;

        return base + kHeaderBytes;
    }

    void GuardFree(void* p)
    {
        if(p == 0)
            return;

        DbgBlock* b = (DbgBlock*)((char*)p - kHeaderBytes);

        if(b->headMagic != kHeadMagic)
        {
            // Either this block was already deleted (head magic overwritten on free) or it
            // never came from operator new. Either way do not trust the header.
            unsigned int staleSeq = b->seq;
            unsigned int staleSize = b->size;
            void* staleCaller = b->allocCaller;
            const FreeRecord* prev = FindFreeRecord(p);
            void* deleteCaller = __builtin_return_address(0);

            if(b->headMagic == kFreedMagic || prev != 0)
            {
                gErrorCount++;
                if(prev != 0)
                {
                    Aurora::Utils::Logger::Instance()->LogMessage(
                        "HEAP GUARD: DOUBLE DELETE of %p (%u bytes, seq #%u)\n",
                        p, prev->size, prev->seq);
                    Aurora::Utils::Logger::Instance()->LogMessage(
                        "HEAP GUARD:   new called from %p ; first delete from %p (free #%u) ; "
                        "delete again from %p (free #%u, %u frees / %u allocations later)\n",
                        prev->allocCaller, prev->freeCaller, prev->order,
                        deleteCaller, gFreeCount, gFreeCount - prev->order, gAllocSeq - prev->seq);
                }
                else
                {
                    Aurora::Utils::Logger::Instance()->LogMessage(
                        "HEAP GUARD: DOUBLE DELETE of %p -- head was already poisoned (allocated as #%u, "
                        "%u bytes, new from %p), no matching free left in the ring. delete from %p\n",
                        p, staleSeq, staleSize, staleCaller, deleteCaller);
                }
            }
            else
            {
                gErrorCount++;
                Aurora::Utils::Logger::Instance()->LogMessage(
                    "HEAP GUARD: delete of %p, which never came from new (head magic %08x, delete from %p) -- "
                    "a malloc'd buffer released with delete, or a wild pointer.\n", p, b->headMagic, deleteCaller);
            }

            free(p); // matches the behaviour this build would have had without the guard
            return;
        }

        unsigned int* tail = (unsigned int*)((char*)p + TailOffset(b->size));
        if(tail[0] != kTailMagic || tail[1] != b->size)
        {
            gErrorCount++;
            Aurora::Utils::Logger::Instance()->LogMessage(
                "HEAP GUARD: BUFFER OVERRUN on %p (seq=%u size=%u): tail magic=%08x/%u expected %08x/%u\n",
                p, b->seq, b->size, tail[0], tail[1], kTailMagic, b->size);
        }

        // Unlink, then poison so a second delete is caught above.
        if(b->prev)
            b->prev->next = b->next;
        else
            gHead = b->next;
        if(b->next)
            b->next->prev = b->prev;

        gLiveBlocks--;
        gLiveBytes -= b->size;
        RememberFree(p, b, __builtin_return_address(0));
        gFreeCount++;

        b->headMagic = kFreedMagic;
        b->size = 0;
        b->prev = 0;
        b->next = 0;
        free(b);
    }

    void GuardOutOfMemory(size_t size)
    {
        Aurora::Utils::Logger::Instance()->LogMessage(
            "HEAP GUARD: OUT OF MEMORY allocating %u bytes (liveBlocks=%u liveBytes=%u errors=%u)\n",
            (unsigned int)size, gLiveBlocks, gLiveBytes, gErrorCount);
        abort(); // operator new may not return null; -fno-exceptions means we cannot throw
    }

    void LogHeapLine(const char* tag)
    {
        struct mallinfo mi = mallinfo();
        Aurora::Utils::Logger::Instance()->LogMessage(
            "heap %-22s trackedBlocks=%u trackedBytes=%u peakBlocks=%u peakBytes=%u "
            "allocSeq=%u frees=%u mallFreeKB=%d mallUsedKB=%d mallArenaKB=%d errors=%u\n",
            tag, gLiveBlocks, gLiveBytes, gPeakBlocks, gPeakBytes, gAllocSeq, gFreeCount,
            (int)(mi.fordblks / 1024), (int)(mi.uordblks / 1024), (int)(mi.arena / 1024), gErrorCount);
    }
}

// ---------------------------------------------------------------------------
// Global allocation functions. Defining these replaces newlib's, so every C++
// allocation in the game passes through the guard band while debugging.
// ---------------------------------------------------------------------------

void* operator new(size_t size)
{
    void* p = GuardAlloc(size);
    if(p == 0)
        GuardOutOfMemory(size);
    return p;
}

void* operator new[](size_t size)
{
    void* p = GuardAlloc(size);
    if(p == 0)
        GuardOutOfMemory(size);
    return p;
}

void* operator new(size_t size, const std::nothrow_t&) throw()
{
    return GuardAlloc(size);
}

void* operator new[](size_t size, const std::nothrow_t&) throw()
{
    return GuardAlloc(size);
}

void operator delete(void* p) throw()
{
    GuardFree(p);
}

void operator delete[](void* p) throw()
{
    GuardFree(p);
}

void operator delete(void* p, const std::nothrow_t&) throw()
{
    GuardFree(p);
}

void operator delete[](void* p, const std::nothrow_t&) throw()
{
    GuardFree(p);
}

void operator delete(void* p, size_t) throw()
{
    GuardFree(p);
}

void operator delete[](void* p, size_t) throw()
{
    GuardFree(p);
}

#endif // MC_DEBUG_MODE

// ---------------------------------------------------------------------------
// Entry points used by the game. They exist in every build; in a release build
// they do nothing and the compiler removes the calls.
// ---------------------------------------------------------------------------

void MC_BootSession()
{
#if MC_DEBUG_MODE
    Aurora::Utils::Logger::Instance()->ResetLog();

    Aurora::Utils::Logger::Instance()->LogMessage(
        "==================== Minecraft PSP development build ====================\n");
    Aurora::Utils::Logger::Instance()->LogMessage("compiled %s %s\n", __DATE__, __TIME__);
    Aurora::Utils::Logger::Instance()->LogMessage(
        "debug: log cleared at boot, new/delete guarded (overrun + double free are reported)\n");

    // Which binary is this? Stamping the EBOOT's own timestamp means a log or a crash
    // screen can always be traced back to the exact build that produced it.
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    if(sceIoGetstat("EBOOT.PBP", &st) >= 0)
    {
        // this SDK spells the fields sce_st_* (see pspiofilemgr_stat.h)
        Aurora::Utils::Logger::Instance()->LogMessage(
            "EBOOT.PBP  %04d-%02d-%02d %02d:%02d:%02d  %d bytes\n",
            st.sce_st_mtime.year, st.sce_st_mtime.month, st.sce_st_mtime.day,
            st.sce_st_mtime.hour, st.sce_st_mtime.minute, st.sce_st_mtime.second,
            (int)st.st_size);
    }
    else
    {
        Aurora::Utils::Logger::Instance()->LogMessage("EBOOT.PBP  (stat failed)\n");
    }

    struct mallinfo mi = mallinfo();
    Aurora::Utils::Logger::Instance()->LogMessage(
        "heap at boot: freeKB=%d usedKB=%d arenaKB=%d\n",
        (int)(mi.fordblks / 1024), (int)(mi.uordblks / 1024), (int)(mi.arena / 1024));
    Aurora::Utils::Logger::Instance()->LogMessage(
        "========================================================================\n");
#endif
}

void MC_DebugHeapCheckpoint(const char* tag)
{
#if MC_DEBUG_MODE
    // Walk every live block and check both guard bands. This is what turns a corruption
    // into "reported at the milestone that caused it" instead of "crashed in malloc
    // twenty minutes later".
    unsigned int checked = 0;
    unsigned int corrupt = 0;
    for(DbgBlock* b = gHead; b; b = b->next)
    {
        checked++;
        if(b->headMagic != kHeadMagic)
        {
            corrupt++;
            GuardReport("live block has a smashed header", b->seq, b->size);
            continue;
        }
        unsigned int* tail = (unsigned int*)((char*)b + kHeaderBytes + TailOffset(b->size));
        if(tail[0] != kTailMagic || tail[1] != b->size)
        {
            corrupt++;
            GuardReport("live block has been overrun (tail guard smashed)", b->seq, b->size);
            Aurora::Utils::Logger::Instance()->LogMessage(
                "HEAP GUARD:   tail magic=%08x/%u expected %08x/%u\n",
                tail[0], tail[1], kTailMagic, b->size);
        }
    }

    if(corrupt > 0)
        Aurora::Utils::Logger::Instance()->LogMessage(
            "HEAP GUARD: %u of %u live blocks corrupt at checkpoint '%s'\n", corrupt, checked, tag);

    LogHeapLine(tag);
#endif
}
