#ifndef LAUNCHERAPP_LAUNCHER_H
#define LAUNCHERAPP_LAUNCHER_H

#include <string>
#include <vector>

/*
 * The launcher: what the XMB runs.
 *
 * WHY THE LAUNCHER EXISTS AT ALL, in one paragraph, because the shape of everything in this file
 * follows from it. A running program cannot replace the EBOOT.PBP it is executing -- the PSP maps the
 * file in order to run it. That single fact is what the old in-game updater was built around: the game
 * could download, but installing always needed a second program, handed off to through a file, with a
 * launcher call that could not come back. Turning the arrangement inside out removes all of it. The
 * launcher is the EBOOT the XMB runs, and the game is a different file beside it. The launcher wrote
 * that file; it is not running from it; so it can replace it whenever it likes, with no handoff, no
 * second program and no one-way launch. What is left is a single ordinary launch, launcher -> game,
 * which is the one hop the firmware allows.
 *
 * LAYOUT -- the launcher owns the folder, installs live under it:
 *
 *     PSP/GAME/Minecraft/
 *         EBOOT.PBP              the launcher; the only thing the XMB lists
 *         Assets/ Config/        the launcher's own small UI assets (dirt, load, buttons, font)
 *         update_url.txt         the update server, the same file the game used
 *         Installs/
 *             3.0.1/             one self-contained install
 *                 EBOOT.PBP      the game, under the name it was built with
 *                 Assets/ Config/ Save/
 *                 install.txt    what this folder is, for the list below
 *             3.0.0/             kept, which is what makes rollback real rather than promised
 *
 * The game inside an install KEEPS the name EBOOT.PBP rather than being renamed to something like
 * GAME.PBP, and that is worth a sentence because the rename is the obvious idea. Two folders can hold
 * a file of the same name; the XMB lists only PSP/GAME/<folder>/EBOOT.PBP and never descends below
 * that, so an EBOOT.PBP two levels down is invisible to it and the launcher stays the only entry in the
 * Game menu. What the shared name buys is that a package's manifest -- whose paths are relative to a
 * game folder and have always named EBOOT.PBP, because that is what the packaging script produces --
 * maps onto an install with no renaming step in between, and no second place for the two to disagree.
 *
 * Each install is COMPLETE. That is deliberate: keeping the previous version is keeping a folder that
 * still works, rather than a partial restore of one that has already been overwritten. The cost is
 * that a patch package has to be applied to a copy, which is argued out in LauncherFiles.h.
 */

// One installed copy of the game, as described by its own install.txt.
struct Install
{
	std::string folder;		// relative to the launcher root: "Installs/3.0.1"
	std::string version;	// "3.0.1"
	int versionCode;		// 30001, the integer the protocol compares. See Scripts/Version.h.

	Install() : versionCode(0) {}
};

// Rescans Installs/ into `out`, newest version first. Returns how many were found.
//
// A folder with no readable install.txt is still listed, using its directory name as the version and a
// code of 0. That is not leniency for its own sake: an install can be put on the card by copying a
// folder, and a launcher that refuses to see it would be the only way to end up with a game on the
// card that cannot be started.
unsigned int ScanInstalls(std::vector<Install>* out);

/*
 * A modal line of text, and the launcher's only dialog.
 *
 * `question` false shows the text with a "press X" prompt. `question` true draws a choice and returns
 * true for yes, false for no. Drawn over the tiled background through the same primitives as the menu,
 * so it cannot drift from it. Used for errors, confirmations and the results of an update, all of
 * which are two lines and a button -- which is the reason there is one dialog here rather than a
 * screen per case.
 */
bool ShowMessage(const char* line, const char* detail, bool question);

/*
 * Runs the launcher's menu loop.
 *
 * Returns only when the player chooses to quit. A successful launch does NOT return: loadexec resets
 * the console, which is the whole point of the launcher's position in the chain.
 */
void LauncherMain();

// The launcher's own folder, or an empty string if it could not be determined. Re-exported here so the
// update flow and the menu agree on what "the root" means by construction rather than by convention.
std::string LauncherRootPath();

#endif
