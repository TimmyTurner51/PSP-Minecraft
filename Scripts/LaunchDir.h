#ifndef MC_LAUNCHDIR_H
#define MC_LAUNCHDIR_H

/*
 * The working directory a launched game runs in, and why it needs restoring.
 *
 * Launching Minecraft from the XMB gives it a working directory: the folder the EBOOT lives in. Every
 * relative path in the game depends on that -- "Assets/...", "Save/", "Config/", and Log.txt itself --
 * so it is not a detail, it is the floor the rest of the program stands on.
 *
 * The Launcher does not launch from the XMB, and that is the whole problem this file exists for. It
 * starts Minecraft through the CFW's loadexec, and loadexec DOES NOT PRESERVE A WORKING DIRECTORY --
 * measured on this console, not assumed: tools/loadexec-probe recorded `cwd : "(failed)"` in a payload
 * started exactly that way. So a game started by the Launcher would reach its first asset read with no
 * directory at all and fail on a path that is perfectly correct.
 *
 * The fix is a one-line handoff in the other direction: the Launcher writes the install's absolute
 * path to a file at the device root, and the game reads it, chdirs, and removes it. That is the same
 * shape as the old in-game updater's handoff file, and for the same reason -- it is the only channel
 * available across loadexec. The direction is simply reversed: there, the game told the updater where
 * the game folder was; here, the launcher tells the game where the game folder is.
 *
 * WHEN THERE IS NO FILE, NOTHING HAPPENS, and that case matters as much as the other one. A build run
 * from the XMB (every development run of the game) has a correct working directory already; a chdir to
 * a remembered path would be a step sideways. The file is consumed when it is used, so a stale one
 * cannot follow a player into a different install either.
 *
 * ---------------------------------------------------------------------------------------------
 * WHICH HALF IS COMPILED INTO WHICH PROGRAM, and this is the reason the implementation is two files
 * rather than one. The two halves are not interchangeable and must not both be present everywhere:
 *
 *   LaunchFile.c   writes and clears the handoff. Used by the LAUNCHER.
 *   LaunchDir.c    reads the handoff and chdirs, from a constructor ordered to run first. Used by the
 *                  GAME.
 *
 * The launcher deliberately does NOT link the reader. It is not launched by loadexec, so it already
 * has the right working directory -- and running the reader there would be actively harmful: a handoff
 * left behind by a launch that failed, or by a game that died before its first constructor, would
 * silently move the LAUNCHER into an install folder at boot, and every path it builds from its root
 * would then be wrong at once. Keeping the halves in separate objects means the launcher cannot pick
 * up that behaviour by accident.
 * ---------------------------------------------------------------------------------------------
 */

// The handoff, at the device root because it must be readable before a working directory exists.
#define MC_LAUNCH_FILE_NAME "mc-launch.txt"

// The devices a handoff can be on, in probe order, as complete paths. Defined once in LaunchFile.c and
// shared, rather than spelled out in each half: a writer that disagrees with a reader about where the
// file lives is a handoff that silently never happens, which is the one failure this whole mechanism
// exists to avoid.
//
// Both devices a homebrew can be installed on are listed -- the memory stick, and the PSP Go's internal
// storage. Written in the same order Updater.cpp probes them, for the same reason: one of them is wrong
// and costs an open that fails immediately.
#define MC_LAUNCH_FILE_COUNT 2
extern const char* const MC_LaunchFilePaths[MC_LAUNCH_FILE_COUNT];

#ifdef __cplusplus
extern "C" {
#endif

// --- LaunchDir.c: the game's half ------------------------------------------------------------

// Applies the handoff if one is present. Runs automatically from a constructor ordered to be the first
// code in the module (see MakefileGame's note on OBJS order); exposed so it can also be called
// explicitly.
void MC_ApplyLaunchDir(void);

// --- LaunchFile.c: the launcher's half -------------------------------------------------------

// Writes the handoff for "absoluteDir", on the same device that directory is on. Returns 0 on success,
// or a negative SceIo error. The Launcher calls this immediately before loadexec.
int MC_WriteLaunchDir(const char* absoluteDir);

// Removes the handoff from every device it could be on, ignoring whether it was there.
//
// Used when a launch is attempted and FAILS. A successful launch never comes back, and the game
// consumes the file on its way in -- but a failed one leaves it behind, and a stale handoff would
// silently point the next run at an install the player did not choose.
void MC_ClearLaunchDir(void);

#ifdef __cplusplus
}
#endif

#endif
