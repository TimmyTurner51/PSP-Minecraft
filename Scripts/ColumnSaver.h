#ifndef COLUMNSAVER_H
#define COLUMNSAVER_H

// Column saves off the frame.
//
// On the console a column unload cost the frame about 39 ms: gzip at level 1 and the memory stick
// write, both on the main thread, between two frames that each had 16 ms. At sprint-fly speed that
// is three or four of them a second. Here the column is handed to a lower-priority thread, which
// compresses and writes it while the main thread is waiting for the GE or the vblank -- time that
// was idle. The main thread keeps every allocation: the worker compresses through a fixed arena
// (zlib's allocator hooks), writes with sceIo, and never touches malloc. The column is deleted by
// the main thread once the worker has marked the job done (Reap).
//
// The file written is the same gzip member SaveColumnFile writes (same header, same payload), so
// LoadColumnFile and every existing save read it unchanged. The deflate window is 4 KB rather than
// 32 KB so the arena stays small; the inflater accepts any window up to its own.
//
// Two jobs may be in flight. If both slots are busy the caller falls back to the synchronous save,
// so memory held by unsaved columns is bounded whatever the card does.

class ChunkColumn;

namespace ColumnSaver
{
    // Starts the worker thread. Safe to call more than once.
    bool Start();
    // Waits for every queued save, then stops the worker. Columns still held are left for Reap.
    void Stop();

    // Takes ownership of `col` and writes it to `path`. Returns false (and takes nothing) if no
    // slot is free; the caller then saves synchronously.
    bool Enqueue(ChunkColumn* col, const char* path, int seed, int stamp);

    // Columns whose file is written. Fills up to `max` entries and returns the count; each is now
    // the caller's to delete, and its file is on the card.
    int Reap(ChunkColumn** out, int* outCx, int* outCz, int max);

    // If the column at (cx, cz) is queued or being written, takes it back: a queued one is simply
    // withdrawn (no file is written, the caller still has the live column), one being written is
    // waited for. Returns the column or 0. Used when the streamer wants a column back before the
    // save finished -- the player turned around.
    ChunkColumn* Reclaim(int cx, int cz);

    // True while any job is queued, running or finished-but-unreaped.
    bool Busy();
    // Blocks until every queued or running save has finished (finished jobs still need Reap).
    void Drain();

    // Counters for the profile line: saves done and failed by the worker, and worker-side
    // microseconds.
    unsigned int SavesDone();
    unsigned int SavesFailed();
    unsigned int WorkerUs();
    // Where the last failed write stopped: 1 deflateInit, 2 open, 3 deflate, 4 write, 5 finish,
    // 6 the worker's sceIoChdir; and the sce or zlib code it got. 0 if none has failed.
    void LastFailure(int& stage, int& code);
}

#endif
