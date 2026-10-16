#ifndef LAUNCHERAPP_LAUNCHER_FILES_H
#define LAUNCHERAPP_LAUNCHER_FILES_H

#include <string>

/*
 * The launcher's relation to the card, in one place.
 *
 * WHY THIS IS ITS OWN FILE
 * ------------------------
 * Two things in the launcher walk the card: the install manager (which lists, creates and deletes
 * install folders) and the update flow (which downloads into one and, for a patch, copies a whole
 * install tree before it can apply anything). Both need the same five or six operations, and a second
 * recursive copy is how one of them quietly grows a bug the other does not have -- a case-insensitive
 * name comparison here, a missing sync there.
 *
 * WHY THERE IS A COPY AT ALL, since it is the expensive thing in here
 * ------------------------------------------------------------------
 * An install is self-contained: `Installs/<version>/` holds the game, its Assets, its Config and its
 * Save. That is what makes rollback real -- keeping the previous version is keeping a folder that
 * still works, not a partial restore of one that has already been overwritten. The cost is that
 * creating a new install from a *patch* package (which is what the packaging script produces when it
 * is given a --base to diff against) means copying the selected install first and applying the patch
 * on top. A package built without --base is a full distribution and skips the copy entirely, which is
 * the case the manifest's `full|1` line marks.
 *
 * The copy is therefore the one operation here that is slow enough for the player to notice -- of the
 * order of a minute for the 135 MB an install currently weighs -- so CopyTree takes a progress
 * callback rather than running blind. A minute of a frozen screen is indistinguishable from a hang.
 */

// Byte counts are 64-bit-ish on purpose. An install is already past what a 16-bit count could hold and
// the copy reports a running total, so the type is chosen once here rather than being an int that
// silently wraps somewhere in the middle of a copy.
typedef unsigned long long FileSize;

// Called during CopyTree with the running totals. Return false to abort the copy, which leaves the
// destination half-written -- the caller is expected to delete it, because a partial install that
// looks complete is worse than no install at all.
typedef bool (*CopyProgressFn)(void* ctx, FileSize done, FileSize total, const char* currentFile);

// THE LAUNCHER'S OWN FOLDER, absolute, with no trailing slash -- "ms0:/PSP/GAME/Minecraft" or the
// ef0: equivalent. Taken from getcwd(), because the launcher is started from the XMB and therefore
// has a working directory, unlike the Updater it replaces. Returns an empty string if getcwd fails,
// which every caller must treat as fatal: without it there is no way to build the absolute paths the
// launch handoff and loadexec both require.
std::string LauncherRoot();

// "a/b", tolerating a trailing slash on `a` and an empty `b`. The one place a path is assembled, so
// the doubled-slash class of bug has one place to not appear in.
std::string Join(const std::string& a, const std::string& b);

// The last path component: "Installs/3.0.1" -> "3.0.1".
std::string BaseName(const std::string& path);

/*
 * The launcher's crash breadcrumb: name the step that is about to run, in a file that is REWRITTEN
 * rather than appended, so whatever it holds after a crash is the step that did not finish.
 *
 * Same contract as BootTrace's stage lines, and it exists for the same reason -- which here is not
 * hypothetical but the actual state of things: the launcher's Log.txt is truncated at every boot, so a
 * card that has been through four runs keeps one log, and a crash in any of the first three leaves
 * nothing at all. The breadcrumb is one small file, written and flushed at a few dozen points, and it
 * survives everything a hard crash on this hardware can do to it.
 *
 * Deliberately not the log. The log is the record of what happened; this is the answer to "where did it
 * stop", and the two want different lifetimes. The file also carries the free heap at each step, since
 * a launcher that dies part way through a network operation on this hardware is far more likely to have
 * run out of something than to have been handed a bad pointer.
 */
void LauncherPhase(const char* what);

bool FileExists(const std::string& path);

// Reads a whole small file (a config line, an install.txt) into `out`, up to 4 KB. False if it could
// not be opened or was larger than that.
bool ReadTextFile(const std::string& path, std::string* out);

// Writes `text` to `path`, truncating. Uses a temporary name and a rename so an interrupted write
// cannot leave a half-written install.txt that the next boot would read as a real install.
bool WriteTextFile(const std::string& path, const std::string& text);

// Creates one directory level. False if it could not be created; does NOT treat "already exists" as
// success, because every caller here has just checked and a surprise is worth reporting.
bool MakeDir(const std::string& path);

// Removes a file, or a directory and everything under it. `removed` (optional) accumulates the file
// count. Missing paths are not an error -- deleting an install twice is not a failure.
bool RemoveTree(const std::string& path, unsigned int* removed);

// Total bytes and file count under a path, without copying anything. Used to size the progress bar
// before the copy starts, because a bar that does not know its own length cannot be drawn.
FileSize TreeSizeBytes(const std::string& path);
unsigned int TreeFileCount(const std::string& path);

// Recursive copy. Creates `dst` and every directory under it. `progress` may be NULL.
bool CopyTree(const std::string& src, const std::string& dst, CopyProgressFn progress, void* ctx);

// Flushes the card. Removals and directory creation are writes like any other, and the observation
// that made SyncCard necessary in the Updater -- a removed handoff file reaching the card while the
// package following it did not -- applies just as much to a deleted install directory.
void SyncCard();

#endif
