#ifndef LAUNCHERAPP_LAUNCHER_UPDATE_H
#define LAUNCHERAPP_LAUNCHER_UPDATE_H

#include <string>

/*
 * Checks for an update, downloads it, and installs it as a new install.
 *
 * Draws through Screen and reads the pad directly, because every step of this is a blocking call that
 * still has to paint and still has to be cancellable -- Screen::Frame is that screen, and the progress
 * callbacks below are where the input is read. Returns when it is finished for any reason: installed,
 * nothing to do, failed, or the player backed out. It never exits the program and never throws.
 *
 * `baseInstallFolder` is the install that a PATCH package will be applied to a copy of, relative to the
 * launcher root ("Installs/3.0.1"). An empty string means there is none, in which case only a full
 * package can be installed -- the flow works that out from the manifest and says so rather than
 * failing part way through a copy.
 *
 * `repair` says the player asked for the install named by `baseInstallFolder` to be CORRECTED rather
 * than for a newer one to be fetched. It changes three things, and nothing else:
 *
 *   - An offer for the version already installed is accepted instead of being refused as "up to date",
 *     which is the whole point: the package being reinstalled is the one that describes the files that
 *     are missing.
 *   - The offer has to BE that version. Repairing 3.0.1 by installing whatever the server happens to be
 *     offering would leave the broken folder exactly as it was and add a second install beside it, which
 *     is not what the button promised. If the server has moved on, that is said plainly.
 *   - The package is applied to the install IN PLACE rather than to a copy or a fresh folder, so the
 *     installer's per-file size and crc check decides what to write. Only the files that are wrong or
 *     gone are rewritten, so a repair of two missing files writes two files rather than the whole tree.
 */
void LauncherRunUpdate(const std::string& baseInstallFolder, bool repair = false);

/*
 * A read-only look at the server for the boot check: is the release on offer one this card already has,
 * and does the card actually hold it?
 *
 * It exists separately from LauncherRunUpdate because it must not be able to download anything -- it
 * answers a question, and the button is still the thing that acts on the answer. The network has to be
 * up already (see Update::NetInit and Update::NetConnectSilent), and nothing is torn down here, so the
 * Check for updates button afterwards is cheap.
 *
 * It fetches update.txt and, when that names a version already installed, the archive's index -- a few
 * hundred KB altogether, which is what makes it affordable while the menu is drawing.
 *
 * Returns  1  the release differs from the install; version/files/kb/archiveSize describe the work
 *          0  nothing to report: no offer, a newer version, a launcher offer, or the card already holds
 *             this release
 *         -1  the answer could not be obtained (no server, no index, no record). The caller says
 *             nothing, because a boot check that reports its own failures is worse than a quiet one.
 */
int LauncherProbeRelease(std::string* version, unsigned int* files, double* kb, unsigned int* archiveSize);

/*
 * Is the server actually answering? Fetches update.txt and parses it, nothing more.
 *
 * This is what separates "joined a network" from "reached anything". A saved access point can associate
 * and hand out an address while going nowhere -- that is exactly what a stale or wrong entry in the
 * console's connection list looks like from here -- and the boot connect needs to tell the two apart so
 * it can move on to the next saved connection instead of settling for the first one that links.
 *
 * `pump` (may be null) is called during the fetch so the caller keeps painting and can cancel.
 */
bool LauncherServerReachable(bool (*pump)(void*), void* ctx);

/*
 * Whether an address the console was just given could possibly reach the update server.
 *
 * This is the free half of "is this the right network". The server is named by a literal address on
 * this LAN, so an address in a different /24 provably cannot reach it -- there is no route between two
 * home subnets -- and that is decided from the address alone, with no request and no timeout. A wrong
 * network is then retired in the time it took to associate with it instead of in the fifteen seconds
 * its HTTP connect timeout would have cost, which is what lets the boot connect work through several
 * saved connections and still reach the menu promptly.
 *
 * True means "no evidence against it" and covers the cases it cannot judge: no address, a hostname
 * server, or anything that is not a dotted quad. False is only returned when the two addresses are
 * provably on different networks, so a caller may treat it as a fact.
 */
bool LauncherAddressCouldReachServer(const std::string& address);

/*
 * Refresh the launcher's version catalog from the server.
 *
 * The dropdown is built from versions.txt next to the launcher. Check for updates already knew about
 * a newer game because it reads update.txt; the list did not, so a version the server was offering
 * never appeared on the left until it was already installed. This writes a fresh versions.txt from
 * the same folder as update.txt, and if that file is missing it still adds the version update.txt
 * names so the dropdown and the update button agree.
 *
 * The network has to be up. Returns true when the file on the card was rewritten.
 */
bool LauncherFetchReleaseList();

#endif
