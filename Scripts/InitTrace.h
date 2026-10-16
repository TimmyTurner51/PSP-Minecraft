#ifndef MC_INITTRACE_H
#define MC_INITTRACE_H

// ---------------------------------------------------------------------------
// A line-at-a-time boot tracer, for locating a crash that Log.txt cannot describe.
//
// WHY THIS EXISTS ALONGSIDE BootTrace AND Logger
//
// BootTrace::Stage records a milestone before the risky work, and Logger flushes Log.txt to the card,
// and between them they answer "which named step did not finish". That is the right granularity once
// the neighbourhood of the fault is known. It is the wrong granularity while it is not: "stopped
// somewhere inside SoundManager::Init" covers a hundred file loads, and the emulator cannot narrow it
// because the emulator does not crash.
//
// This writes its own file, trace.txt, for two reasons.
//
//   * Volume. Log.txt is read as a narrative and is small on purpose; this is a firehose that is
//     only interesting for its last line or two, and mixing the two would destroy both.
//   * Independence. If the fault turns out to be inside Logger -- fopen, vsnprintf, the file open
//     itself -- then a trace that logs through Logger reports nothing at all. This depends on
//     sceIoOpen/sceIoWrite and nothing else, so a silent trace means the fault is below this file.
//
// Every line carries a sequence number and a millisecond timestamp. The sequence is what makes a
// truncated file readable: it says whether the trace stopped because the console died or because
// nothing else was instrumented, and the timestamps say whether a step was slow or fatal.
//
// WHY EVERY LINE IS FORCED TO THE CARD
//
// sceIoWrite into a cached filesystem is not on the card yet, and a hardware exception powers the
// console off without unwinding anything, so an unflushed tail is exactly the part that gets lost.
// sceIoSync after each line makes what is on the card current as of the last completed call. That
// costs a device flush per line and is worth it here for the same reason it is worth it in Logger:
// the value of the whole file is its last line.
//
// Reading the result:
//
//   trace.txt absent                 the module never reached its first constructor.
//   trace.txt ends mid-sequence      the console died during the call named by the last line.
//   trace.txt ends after "splash"    init completed; see the notes in Scripts.cpp on what was
//                                    deliberately not reached.
//
// In C++11 the compiler may reorder the two calls on either side of an +=-free statement, so a
// trace point means "this statement is about to run". That is the intent: the last line names the
// step that did not finish, exactly as in BootTrace.
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Writes "tag" as a line and forces it to the card. Cheap enough to call for every step of startup.
void MC_Trace(const char* tag);

// Same, with the value appended as " =123". The integer forms are here rather than a printf-style
// call so this file does not depend on newlib, which is itself part of what is being diagnosed.
void MC_TraceInt(const char* tag, int value);
void MC_TraceUInt(const char* tag, unsigned int value);

// Appends the pointer as " =0x1234abcd", for the places where the useful question is whether a
// constructor returned a real object or a null one.
void MC_TracePtr(const char* tag, const void* pointer);

// Appends a caller-supplied string, for paths and names. Truncated rather than unbounded.
void MC_TraceStr(const char* tag, const char* text);

#ifdef __cplusplus
}
#endif

#endif
