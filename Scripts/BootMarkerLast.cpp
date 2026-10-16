// ---------------------------------------------------------------------------
// The other end of the constructor phase.
//
// Scripts/BootTrace.cpp drops a marker whose constructor runs *first*. That splits "the console refused
// the EBOOT" from "a global constructor faulted" from "main() ran", but it cannot say where in the
// constructor list a fault happened, and that gap is not academic here: the game has a lot of globals --
// Matrix3's statics, RenderManager, TextureHelper, SoundManager, the state singletons -- and every one
// of them runs before main() and before Log.txt is even truncated.
//
// This object records that the whole phase completed, so the two markers together turn "died in the
// constructors" into "died between these two points in the constructor order", which is a handful of
// files to bisect instead of all of them.
//
// It is listed FIRST in the Makefile's OBJS, and that is what makes it run LAST, because the runtime
// walks .ctors from the end back to the start. The Makefile carries the measurement that established
// this and the reason it matters; the short version is that listing an object last makes its
// constructor run first, which is the opposite of what the order of that file looks like it means.
//
// It also re-reads FCR31 on the way past. If a constructor or a library reached through one turns the
// exception enables back on, this is where that shows up rather than as an unexplained crash later.
// ---------------------------------------------------------------------------

#include "InitTrace.h"
#include "FpuControl.h"

__attribute__((constructor))
static void LatestBootMarker()
{
	MC_TraceUInt("ctors: fcr31 at end of constructor phase", MC_ReadFcr31());
	MC_DisableFpuExceptions();
	MC_TraceUInt("ctors: fcr31 after re-clear", MC_ReadFcr31());
	MC_Trace("ctors: all global constructors complete");
}
