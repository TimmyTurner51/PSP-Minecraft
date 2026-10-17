#include "LauncherUpdate.h"
#include "Launcher.h"
#include "LauncherFiles.h"

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <pspdebug.h>

#include <stdio.h>
#include <string.h>

#include "Screen.h"
#include "Updater.h"
// MC_VERSION_CODE, the launcher's own version. The launcher ships from the same tree as the game and
// is bumped by the same release, so one number describes both; that is what a `target|launcher` offer
// is compared against. See Scripts/Version.h.
#include "Version.h"

#include <Aurora/Utils/Logger.h>

using namespace Aurora::Utils;

/*
 * The update flow, which is the reason the launcher exists.
 *
 * WHAT IS DIFFERENT FROM THE IN-GAME UPDATER IT REPLACES
 * -----------------------------------------------------
 * Almost nothing about the protocol: this calls the same Update:: functions the game did, from the same
 * source file, so the two cannot disagree about a package's format. What changed is the position it
 * runs from, and that removes the three hard problems the old flow was built around:
 *
 *   - There is no handoff file and no second program. The launcher is not the file it is installing,
 *     so it writes the game directly. The whole "you cannot replace the file you are running" rule
 *     simply does not apply to it.
 *   - There is no refusal of packages containing EBOOT.PBP. The old updater had to refuse them because
 *     that entry was the file it was running from; here EBOOT.PBP inside an install is the GAME, and
 *     installing it is the point.
 *   - There is no one-shot launch afterwards. The player stays in the launcher and can start the new
 *     build from the menu, which is one ordinary loadexec rather than a second one.
 *
 * WHY THE INSTALL GOES INTO A NEW FOLDER
 * --------------------------------------
 * So that the previous version survives, which is what makes the launcher's rollback real rather than
 * promised: if the new build will not boot, the install it replaced is still sitting there, complete,
 * and selecting it starts it. A full package is simply extracted. A patch package -- what the
 * packaging script produces when it is given a --base to diff against -- has nothing to be applied to
 * on its own, so the selected install is COPIED first and the patch applied to the copy. That copy is
 * the slowest thing in this file; see LauncherFiles.h for why it is worth it.
 */

// A display list for the bare clear the system dialog needs. Screen owns its own and does not publish
// it, and borrowing it would mean the dialog's clear depended on Screen's frame state.
static unsigned int sDialogList[16] __attribute__((aligned(64)));

#define UPDATE_URL_FILE "update_url.txt"

// Where an install's own log is saved when its folder is about to be deleted. Named for what it is
// rather than for the version, because it is overwritten by the next failure and the useful question
// is "what did the last failed install do", not "what did the one three versions ago do".
#define INSTALL_LOG_FILE "install-failed.txt"

static const char* const kServerPlaceholderIp = "192.168.0.0";
static const int kServerPort = 8000;

// Progress redraws are rate limited, and that is not a nicety: the callbacks fire once per write, and
// the memory stick is free to write in small pieces -- thousands of calls for one file. Drawing a frame
// and waiting for vblank on every one of them turns a two-second write into a thirty-second one.
#define REDRAW_US 150000

static unsigned int gLastDraw = 0;
static bool gCancelled = false;

// Only used by the text fallback, to stop it redrawing the same status sixty times a second.
static std::string gLastTextStatus;

// ---------------------------------------------------------------------------------------------
// Presentation
// ---------------------------------------------------------------------------------------------

/*
 * One frame of the progress screen, or a line of text when there is no screen.
 *
 * The update is the one place the launcher is still a single-purpose progress display, so it uses
 * Screen::Frame -- the same three-line screen the Updater showed, which is defined in terms of the same
 * primitives the menu uses and therefore cannot drift from it.
 */
static void Status(const char* line, const char* detail, const char* prompt, float progress)
{
	if (Screen::Instance()->Ready())
	{
		Screen::Instance()->Frame(line, detail, prompt, progress);
		return;
	}

	// No screen: the card has no Assets folder, which is exactly the card an update is most likely to
	// be run on. Redrawn only when the line changes, or this would scroll itself off the display.
	std::string current = (line != 0) ? line : "";
	if (current == gLastTextStatus)
	{
		return;
	}
	gLastTextStatus = current;

	pspDebugScreenClear();
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenPrintf("MineCraft Launcher\n\n%s\n\n%s\n", line ? line : "", detail ? detail : "");
}

// The two halves of pumping a system dialog (sceUtilityNetconfUpdate). They must be on opposite sides
// of the pump: clearing after the dialog has drawn would erase the frame just drawn. Copied from
// RenderManager::StartDialog/EndDialog, including the explicit clear colour -- without it the clear
// uses whatever the game last set and every part of the screen a dialog does not paint comes back in
// that colour.
static bool DialogClear(void* ctx)
{
	(void)ctx;

	sceGuStart(GU_DIRECT, sDialogList);
	sceGuClearColor(0xFF000000);
	sceGuClearDepth(0);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuFinish();
	sceGuSync(0, 0);
	return true;
}

static bool DialogPresent(void* ctx)
{
	(void)ctx;

	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
	return true;
}

// ---------------------------------------------------------------------------------------------
// Progress callbacks
// ---------------------------------------------------------------------------------------------

// Reads the pad and latches a cancel. Every blocking network call has to be cancellable, and this is
// the only place in the flow where input can be read while one is in progress.
static void PollCancel()
{
	SceCtrlData pad;
	memset(&pad, 0, sizeof(pad));
	sceCtrlReadBufferPositive(&pad, 1);

	if (pad.Buttons & PSP_CTRL_CIRCLE)
	{
		gCancelled = true;
	}
}

static bool DownloadProgress(void* ctx, unsigned int done, unsigned int total)
{
	(void)ctx;

	unsigned int now = sceKernelGetSystemTimeLow();
	if (gLastDraw != 0 && (now - gLastDraw) < REDRAW_US)
	{
		// Still polled, even when the screen is not redrawn: a cancel that only registers on a frame
		// boundary is a cancel the player holds the button through.
		PollCancel();
		return !gCancelled;
	}
	gLastDraw = now;

	char detail[64];
	if (total > 0)
	{
		snprintf(detail, sizeof(detail), "%u / %u KB", done / 1024, total / 1024);
	}
	else
	{
		snprintf(detail, sizeof(detail), "%u KB", done / 1024);
	}

	float fraction = (total > 0) ? (float)done / (float)total : -1.0f;
	Status("Downloading...", detail, "O to cancel", fraction);

	PollCancel();
	return !gCancelled;
}

// Return value is ignored by InstallPackage (there is no safe point to stop part way through a
// package), so this only draws.
static bool InstallProgress(void* ctx, unsigned int done, unsigned int total)
{
	(void)ctx;

	unsigned int now = sceKernelGetSystemTimeLow();
	if (gLastDraw != 0 && (now - gLastDraw) < REDRAW_US)
	{
		return true;
	}
	gLastDraw = now;

	char detail[64];
	if (total > 0)
	{
		snprintf(detail, sizeof(detail), "%u / %u KB", done / 1024, total / 1024);
	}
	else
	{
		snprintf(detail, sizeof(detail), "%u KB", done / 1024);
	}

	float fraction = (total > 0) ? (float)done / (float)total : -1.0f;
	Status("Installing...", detail, 0, fraction);
	return true;
}

static bool CopyProgress(void* ctx, FileSize done, FileSize total, const char* currentFile)
{
	(void)ctx;
	(void)currentFile;

	unsigned int now = sceKernelGetSystemTimeLow();
	if (gLastDraw != 0 && (now - gLastDraw) < REDRAW_US)
	{
		return true;
	}
	gLastDraw = now;

	char detail[64];
	snprintf(detail, sizeof(detail), "%u / %u MB",
		(unsigned int)(done / (1024 * 1024)), (unsigned int)(total / (1024 * 1024)));

	float fraction = (total > 0) ? (float)((double)done / (double)total) : -1.0f;
	Status("Preparing the update...", detail, "This can take a minute", fraction);
	return true;
}

// ---------------------------------------------------------------------------------------------
// The server address
// ---------------------------------------------------------------------------------------------

/*
 * Where to look. The same file the game used, so a card that was already updating keeps working with
 * no reconfiguration -- and one line of text, because it has to be writable by hand from a PC.
 *
 * A missing, empty or placeholder file now means the game repo's GitHub Releases, not "not
 * configured". Shipping cards have no file to write, and a silent default that is the real channel
 * is the only answer that lets Check for updates work out of the box. A LAN address in the file
 * still wins, which is how a development card keeps talking to the machine on the desk.
 *
 * NOTE: the in-game updater also had an on-screen keyboard to set this. The launcher does not yet, so
 * this file is currently the only way to override the server.
 */
static std::string ReadBaseUrl()
{
	std::string raw;
	std::string line;
	if (ReadTextFile(UPDATE_URL_FILE, &raw))
	{
		size_t end = raw.find_first_of("\r\n");
		line = (end == std::string::npos) ? raw : raw.substr(0, end);
	}

	if (line.empty() || line == kServerPlaceholderIp)
	{
		return Update::DefaultReleaseUrl();
	}

	// A full URL is taken as given; a bare host gets the scheme and port the project's server uses.
	if (line.compare(0, 7, "http://") == 0 || line.compare(0, 8, "https://") == 0)
	{
		if (line[line.size() - 1] != '/')
		{
			line += "/";
		}
		return line;
	}

	char buffer[192];
	snprintf(buffer, sizeof(buffer), "http://%s:%d/", line.c_str(), kServerPort);
	return buffer;
}

// ---------------------------------------------------------------------------------------------
// The flow
// ---------------------------------------------------------------------------------------------

// Everything that has to happen on the way out, so no early return forgets it. The net modules in
// particular hold a large amount of memory, and the launcher's next act may be to start the game.
static void Teardown(bool netWasUp)
{
	if (netWasUp)
	{
		// Named on both sides because this is the one step in the flow that dismantles the network
		// stack, and it was the last thing to run before the launcher died on hardware once. The pair
		// says whether a next occurrence is inside it or after it.
		LauncherPhase("teardown: shutting the network down");
		Update::NetShutdown();
		LauncherPhase("teardown: done");
	}
}

/*
 * Rescue the install's own log from a folder that is about to be deleted.
 *
 * InstallPackage runs with the working directory set to the install folder -- it has to, the manifest
 * holds relative paths -- and Logger writes to "Log.txt" relative to that directory. So the thousands
 * of per-file lines an install produces land in the install folder, and the failure path below then
 * deletes the folder they are in. What survives is the one summary line copied into the launcher's log,
 * which is the least useful part: it names the file that ended up missing, not the step that failed on
 * it, and the difference between "could not read it out of the package", "could not open it to write"
 * and "wrote 8192 of 13363600 bytes" is the entire diagnosis.
 *
 * Copied rather than moved, and before the folder goes, so the checksum of the original does not matter
 * here. Failure to copy is not itself worth reporting to the player -- they are already being told the
 * install failed -- but it is logged, because a missing install-failed.txt is itself a thing to explain.
 */
static void PreserveInstallLog(const std::string& installFolder)
{
	std::string from = Join(installFolder, "Log.txt");

	SceUID in = sceIoOpen(from.c_str(), PSP_O_RDONLY, 0777);
	if (in < 0)
	{
		// Nothing was written there, which is the ordinary case for a failure that happened before
		// extraction -- there is no log to rescue and nothing was lost.
		return;
	}

	SceUID out = sceIoOpen(INSTALL_LOG_FILE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (out < 0)
	{
		sceIoClose(in);
		Logger::Instance()->LogMessage("launcher: could not save the install log (0x%08X)\n",
			(unsigned int)out);
		return;
	}

	char buffer[4096];
	while (true)
	{
		int got = sceIoRead(in, buffer, sizeof(buffer));
		if (got <= 0)
		{
			break;
		}

		int put = 0;
		while (put < got)
		{
			int now = sceIoWrite(out, buffer + put, (unsigned int)(got - put));
			if (now <= 0)
			{
				break;
			}
			put += now;
		}
		if (put < got)
		{
			break;
		}
	}

	sceIoClose(out);
	sceIoClose(in);
}

/*
 * The launcher replacing itself.
 *
 * This is the one install the launcher cannot do the ordinary way, and the reason is the first fact in
 * Updater.h: a running program cannot replace the file it is executing, and the launcher IS the file
 * the XMB runs. Every other entry in a package the launcher writes itself; EBOOT.PBP it stages under a
 * different name and hands to a second, deliberately tiny program (LauncherUpdater, at Updater/EBOOT.PBP
 * under the launcher root) which renames it into place and exits. That program is the second hop of a
 * chain the launcher starts -- and it works because the launcher is itself a FIRST hop, started by the
 * XMB, so its one loadexec is still available. The old in-game updater could not do this because it was
 * already the product of a loadexec; see UpdaterApp/main.cpp for the whole account.
 *
 * Everything else is ordinary: the same manifest, the same per-file size and crc checks (inside
 * ExtractOneFromZip), the same delete list. The one departure is that the files are written in place
 * rather than into a new folder. That is safe here where it would not be for a game install: the
 * launcher root is not versioned -- it is the program itself, and the files a launcher package carries
 * are replacements for the ones already there, not a tree that has to survive beside an old one. A bad
 * swap is recoverable by running the helper again from a card that still has EBOOT.PBP.bak.
 *
 * WHY THE PACKAGE MUST ALREADY CONTAIN THE HELPER, and why the helper cannot be downloaded now: the
 * swap is the moment there is no launcher left to recover with. If fetching the helper were part of the
 * update and that fetch half-succeeded, the handoff would point at a truncated program and the console
 * would have no launcher at all. So the helper is a shipped, stable file and a missing one is a plain
 * refusal.
 */
static void LauncherSelfUpdate(const Update::Offer& offer, const std::string& baseUrl, bool netWasUp)
{
	std::string root = LauncherRootPath();
	if (root.empty())
	{
		Teardown(netWasUp);
		ShowMessage("Cannot find the launcher folder", "There is no working directory.", false);
		return;
	}

	// Newer than the launcher that is running? MC_VERSION_CODE is the launcher's own version; see the
	// include note above. `force` overrides, exactly as it does for a game offer.
	if (offer.versionCode <= MC_VERSION_CODE && !offer.force)
	{
		Teardown(netWasUp);
		ShowMessage("The launcher is up to date",
			("Version " MC_VERSION_STRING " is already installed."), false);
		return;
	}

	// The helper has to be on the card already; see the note above. Checked before the download so the
	// player is not made to wait for a package that cannot be applied.
	std::string helper = Join(root, "Updater/EBOOT.PBP");
	if (!FileExists(helper))
	{
		Teardown(netWasUp);
		std::string detail = "The helper is missing from " + helper + ".";
		ShowMessage("The launcher updater is missing", detail.c_str(), false);
		return;
	}

	char sizeText[160];
	snprintf(sizeText, sizeof(sizeText), "Download %.1f MB and replace this launcher?",
		(double)offer.size / (1024.0 * 1024.0));

	std::string question = "Launcher " + offer.version + " is available";
	if (!ShowMessage(question.c_str(), sizeText, true))
	{
		Teardown(netWasUp);
		return;
	}

	std::string zipPath = Join(root, "update.zip");

	Status("Downloading...", "Starting", "O to cancel", 0.0f);
	gCancelled = false;
	gLastDraw = 0;

	if (!Update::HttpGetToFile(baseUrl + offer.file, zipPath, DownloadProgress, 0))
	{
		Teardown(netWasUp);
		if (gCancelled)
		{
			sceIoRemove(zipPath.c_str());
			return;
		}
		ShowMessage("The download did not complete", "Nothing was changed.", false);
		return;
	}

	Update::Manifest manifest;
	std::string why;
	if (!Update::ReadManifestFromZip(zipPath, &manifest, &why))
	{
		Teardown(netWasUp);
		ShowMessage("The package is not readable", why.c_str(), false);
		return;
	}

	// It has to carry the launcher itself. An offer marked `target|launcher` that carries no EBOOT.PBP
	// is the wrong package or a broken build, and either way there is nothing to swap.
	bool carriesLauncher = false;
	for (size_t i = 0; i < manifest.files.size(); ++i)
	{
		if (manifest.files[i].path == Update::ExecutableName())
		{
			carriesLauncher = true;
			break;
		}
	}

	if (!carriesLauncher)
	{
		Teardown(netWasUp);
		sceIoRemove(zipPath.c_str());
		ShowMessage("That package has no launcher in it",
			"It is offered as a launcher update but carries no EBOOT.PBP.", false);
		return;
	}

	// Every file except the launcher itself, written in place, each verified against the manifest by
	// ExtractOneFromZip. A failure here is before any swap, so the launcher on the card is untouched.
	Status("Updating the launcher...", "Starting", 0, 0.0f);
	gLastDraw = 0;

	for (size_t i = 0; i < manifest.files.size(); ++i)
	{
		const std::string& rel = manifest.files[i].path;
		if (rel == Update::ExecutableName())
		{
			continue;	// staged below, renamed into place by the helper
		}

		Status("Updating the launcher...", rel.c_str(), 0,
			(float)(i + 1) / (float)(manifest.files.size() + 2));

		if (!Update::ExtractOneFromZip(zipPath, rel, Join(root, rel), &why))
		{
			Logger::Instance()->LogMessage("launcher: self-update could not write '%s': %s\n",
				rel.c_str(), why.c_str());
			Teardown(netWasUp);
			ShowMessage("The launcher update did not finish", why.c_str(), false);
			return;
		}
	}

	// The delete list, which a launcher package uses the same way an install one does.
	for (size_t i = 0; i < manifest.deletes.size(); ++i)
	{
		sceIoRemove(Join(root, manifest.deletes[i]).c_str());
	}

	// Stage the new launcher under a name that is NOT the running file, then leave the helper the
	// one line telling it where that file is and where it goes.
	std::string staged = Join(root, "EBOOT.PBP.new");
	if (!Update::ExtractOneFromZip(zipPath, Update::ExecutableName(), staged, &why))
	{
		Teardown(netWasUp);
		ShowMessage("Could not prepare the new launcher", why.c_str(), false);
		return;
	}

	std::string target = Join(root, Update::ExecutableName());
	if (!Update::WriteLauncherSwap(staged, target, &why))
	{
		sceIoRemove(staged.c_str());
		Teardown(netWasUp);
		ShowMessage("Could not write the swap handoff", why.c_str(), false);
		return;
	}

	// Both the handoff and the staged file have to be ON THE CARD before the helper reads them, and the
	// helper starts by a reset that does not flush anything the launcher left in a cache.
	SyncCard();

	Logger::Instance()->LogMessage("launcher: self-update v%s staged as '%s', handing off to '%s'\n",
		offer.version.c_str(), staged.c_str(), helper.c_str());

	Status("Restarting the launcher...", offer.version.c_str(), 0, 1.0f);

	// The hop. A return always means it failed, because loadexec resets the console on success.
	std::string error;
	if (!Update::LaunchExecutable(helper, &error))
	{
		// Still here, so the handoff must not survive: it would have the next launcher start -- including
		// one the player starts from the XMB -- swap in the version this run never got to try.
		Update::ClearLauncherSwap();
		sceIoRemove(staged.c_str());

		Logger::Instance()->LogMessage("launcher: self-update handoff failed: %s\n", error.c_str());

		Teardown(netWasUp);
		ShowMessage("The launcher could not be restarted", error.c_str(), false);
		return;
	}
}

// Parses a dotted-quad IPv4 literal. False for anything else, including a hostname -- which is the
// answer the caller below wants, because an unknown host must not be read as "cannot reach".
static bool ParseIpv4(const std::string& text, unsigned int octets[4])
{
	int part = 0;
	unsigned int value = 0;
	bool hasDigit = false;

	// One past the end, where the synthetic separator flushes the final octet.
	for (size_t i = 0; i <= text.size(); ++i)
	{
		char c = (i < text.size()) ? text[i] : '.';

		if (c >= '0' && c <= '9')
		{
			value = value * 10 + (unsigned int)(c - '0');
			hasDigit = true;
			if (value > 255)
			{
				return false;
			}
			continue;
		}

		if (c != '.')
		{
			return false;
		}

		if (!hasDigit || part >= 4)
		{
			return false;
		}

		octets[part++] = value;
		value = 0;
		hasDigit = false;
	}

	return part == 4;
}

// The host part of a URL: past the scheme, up to the first ':' or '/'.
static std::string UrlHost(const std::string& url)
{
	size_t start = url.find("://");
	start = (start == std::string::npos) ? 0 : start + 3;

	for (size_t i = start; i < url.size(); ++i)
	{
		if (url[i] == ':' || url[i] == '/')
		{
			return url.substr(start, i - start);
		}
	}
	return url.substr(start);
}

bool LauncherAddressCouldReachServer(const std::string& address)
{
	// Nothing to judge by, in either half. Both mean "no claim", and a claim this function cannot support
	// is worse than no test at all: the caller uses a false here to abandon a connection.
	if (address.empty())
	{
		return true;
	}

	unsigned int here[4];
	if (!ParseIpv4(address, here))
	{
		return true;
	}

	unsigned int server[4];
	if (!ParseIpv4(UrlHost(ReadBaseUrl()), server))
	{
		// The server is named by a hostname, so its network is not knowable from the URL.
		return true;
	}

	// The first three octets. A /24 is the unit a home LAN is laid out in, and two addresses in
	// different /24s are on different networks as far as this console is concerned.
	return here[0] == server[0] && here[1] == server[1] && here[2] == server[2];
}

// Adapts the boot connect's `bool (*)(void*)` pump to the ProgressFn the HTTP layer calls, so a fetch
// can keep the screen painting and stay cancellable without this header having to expose Update's types.
static bool (*sReachPump)(void*) = 0;
static void* sReachCtx = 0;

static bool ReachProgress(void* ctx, unsigned int done, unsigned int total)
{
	(void)ctx;
	(void)done;
	(void)total;
	return (sReachPump != 0) ? sReachPump(sReachCtx) : true;
}

bool LauncherServerReachable(bool (*pump)(void*), void* ctx)
{
	LauncherPhase("reach: reading update_url.txt");

	std::string baseUrl = ReadBaseUrl();
	if (baseUrl.empty())
	{
		return false;
	}

	sReachPump = pump;
	sReachCtx = ctx;

	LauncherPhase("reach: fetching update.txt");

	std::string body;
	bool ok = Update::HttpGetToString(baseUrl + "update.txt", &body, ReachProgress, 0);

	sReachPump = 0;
	sReachCtx = 0;

	if (!ok)
	{
		return false;
	}

	// Parsed, not merely fetched. A captive portal, a proxy error page or a wrong address on the same
	// network all return 200 with a body that is not an offer, and treating any body as success is
	// exactly how a connection that goes nowhere gets adopted.
	Update::Offer offer;
	if (!Update::ParseOffer(body, &offer))
	{
		Logger::Instance()->LogMessage("launcher: boot connect -- the server answered but not with an offer\n");
		return false;
	}

	return true;
}

int LauncherProbeRelease(std::string* version, unsigned int* files, double* kb, unsigned int* archiveSize)
{
	if (version != 0) version->clear();
	if (files != 0) *files = 0;
	if (kb != 0) *kb = 0.0;
	if (archiveSize != 0) *archiveSize = 0;

	std::string root = LauncherRootPath();
	std::string baseUrl = ReadBaseUrl();
	if (root.empty() || baseUrl.empty())
	{
		return -1;
	}

	std::string body;
	if (!Update::HttpGetToString(baseUrl + "update.txt", &body, 0, 0))
	{
		Logger::Instance()->LogMessage("launcher: boot check -- could not fetch update.txt\n");
		return -1;
	}

	Update::Offer offer;
	if (!Update::ParseOffer(body, &offer))
	{
		Logger::Instance()->LogMessage("launcher: boot check -- the offer did not parse\n");
		return -1;
	}

	if (offer.launcher)
	{
		// A launcher update is a loadexec hand-off, not something to raise while the menu is drawing.
		Logger::Instance()->LogMessage("launcher: boot check -- the offer is a launcher update, skipping\n");
		return 0;
	}

	// Only a release for a version ALREADY on the card is in scope. A NEWER version is the button's
	// business: deciding to fetch one is a bigger question than "this file changed", and answering it
	// unasked at boot is how a launcher becomes something players learn to skip past.
	std::vector<Install> installs;
	ScanInstalls(&installs);

	const Install* installed = 0;
	for (unsigned int i = 0; i < installs.size(); ++i)
	{
		if (installs[i].version == offer.version)
		{
			installed = &installs[i];
			break;
		}
	}

	if (installed == 0)
	{
		Logger::Instance()->LogMessage("launcher: boot check -- v%s is offered, not installed here\n",
			offer.version.c_str());
		return 0;
	}

	// A patch's manifest is a delta, so the same guard the update path uses applies here: only a full
	// package can be compared with the install entry by entry. A patch is already the small download
	// this check exists to find, so leaving it to the button costs nothing.
	Update::Package chosen = Update::ChoosePackage(offer, offer.version, true);
	if (!chosen.full)
	{
		Logger::Instance()->LogMessage("launcher: boot check -- the offer is a patch, leaving it to the button\n");
		return 0;
	}

	std::string installAbs = Join(root, installed->folder);

	Update::Manifest record;
	if (!Update::ReadInstalledManifest(installAbs, &record) || record.files.empty())
	{
		Logger::Instance()->LogMessage("launcher: boot check -- no install record\n");
		return -1;
	}

	Status("Looking for updates...", offer.version.c_str(), 0, -1.0f);

	std::vector<Update::RemoteEntry> index;
	std::string why;
	if (!Update::FetchZipIndex(baseUrl + chosen.file, chosen.size, &index, &why))
	{
		Logger::Instance()->LogMessage("launcher: boot check -- no index (%s)\n", why.c_str());
		return -1;
	}

	// The same walk the menu makes, and for the same reason: the record names ~1500 files and asking
	// the card about each by name takes half a minute. It is not split across frames because the
	// player is already watching a status line for this exact answer.
	Update::TreeScan scan;
	Update::BeginTreeScan(installAbs, &scan);
	while (!Update::StepTreeScan(&scan, 40000))
	{
	}

	std::vector<std::string> bad;
	Update::CollectArchiveDiff(scan, record, index, &bad);

	if (bad.empty())
	{
		Logger::Instance()->LogMessage("launcher: boot check -- the card already holds v%s\n",
			offer.version.c_str());
		return 0;
	}

	// Priced rather than merely counted, because the number the player cares about is the bytes. A plan
	// that cannot be made still leaves the count, which is why this is not fatal.
	Update::RepairPlan plan;
	unsigned long long planBytes = 0;
	if (Update::PlanRangeRepair(bad, index, &plan, &why))
	{
		planBytes = plan.bytes;
	}
	else
	{
		Logger::Instance()->LogMessage("launcher: boot check -- could not price the diff (%s)\n", why.c_str());
	}

	Logger::Instance()->LogMessage(
		"launcher: boot check -- v%s differs from the release: %u file(s), %.1f KB\n",
		offer.version.c_str(), (unsigned int)bad.size(), (double)planBytes / 1024.0);

	if (version != 0) *version = offer.version;
	if (files != 0) *files = (unsigned int)bad.size();
	if (kb != 0) *kb = (double)planBytes / 1024.0;
	if (archiveSize != 0) *archiveSize = chosen.size;
	return 1;
}

bool LauncherFetchReleaseList()
{
	std::string baseUrl = ReadBaseUrl();
	std::string root = LauncherRootPath();
	if (baseUrl.empty() || root.empty() || !Update::NetIsConnected())
	{
		return false;
	}

	std::string path = Join(root, "versions.txt");
	std::string body;
	bool haveList = false;

	if (Update::HttpGetToString(baseUrl + "versions.txt", &body, 0, 0) &&
		body.find("version|") != std::string::npos)
	{
		haveList = true;
		Logger::Instance()->LogMessage("launcher: fetched versions.txt from the server\n");
	}
	else
	{
		body.clear();
		ReadTextFile(path, &body);
	}

	std::string offerBody;
	Update::Offer offer;
	if (Update::HttpGetToString(baseUrl + "update.txt", &offerBody, 0, 0) &&
		Update::ParseOffer(offerBody, &offer) &&
		!offer.launcher &&
		!offer.version.empty())
	{
		const std::string tag = std::string("version|") + offer.version;
		if (body.find(tag) == std::string::npos)
		{
			if (!body.empty() && body[body.size() - 1] != '\n')
			{
				body += "\n";
			}
			if (!body.empty())
			{
				body += "\n";
			}

			char line[160];
			snprintf(line, sizeof(line),
				"version|%s\ncode|%d\nnotes|Available as an update.\n",
				offer.version.c_str(), offer.versionCode);
			body += line;
			haveList = true;
			Logger::Instance()->LogMessage("launcher: added v%s from update.txt to the version list\n",
				offer.version.c_str());
		}
	}

	if (!haveList || body.find("version|") == std::string::npos)
	{
		return false;
	}

	if (!WriteTextFile(path, body))
	{
		Logger::Instance()->LogMessage("launcher: could not write versions.txt\n");
		return false;
	}

	return true;
}

void LauncherRunUpdate(const std::string& baseInstallFolder, bool repair)
{
	std::string root = LauncherRootPath();
	if (root.empty())
	{
		ShowMessage("Cannot find the launcher folder", "There is no working directory.", false);
		return;
	}

	std::string baseUrl = ReadBaseUrl();
	if (baseUrl.empty())
	{
		ShowMessage("No update server is set",
			"Put the address in " UPDATE_URL_FILE " on the card.", false);
		return;
	}

	Logger::Instance()->LogMessage("launcher: update check against '%s'\n", baseUrl.c_str());

	/*
	 * Networking. The stack is very likely already up -- the boot connect does this before the menu
	 * draws -- so NetInit here is usually the second call, which it is written to allow. The system
	 * dialog is the fallback, not the interface: it is what the button still offers when the boot
	 * connect could not find anything, or when the player wants to use a connection the console has
	 * never been told about. (The old note here claimed homebrew could not read the saved list. It can;
	 * see NetConnectionValid in Scripts/Updater.cpp.)
	 */
	LauncherPhase("update: starting networking");
	Status("Starting networking...", 0, 0, -1.0f);

	if (!Update::NetInit())
	{
		ShowMessage("Networking is not available", "The network modules could not be loaded.", false);
		return;
	}

	bool netUp = true;

	if (!Update::NetIsConnected())
	{
		LauncherPhase("update: connection dialog");

		if (!Update::NetConnectDialog(DialogClear, DialogPresent, 0))
		{
			Teardown(netUp);
			ShowMessage("Not connected", "No access point was chosen.", false);
			return;
		}
	}

	// The check.
	LauncherPhase("update: fetching update.txt");
	Status("Checking for updates...", baseUrl.c_str(), 0, -1.0f);

	std::string body;
	gCancelled = false;
	gLastDraw = 0;

	if (!Update::HttpGetToString(baseUrl + "update.txt", &body, DownloadProgress, 0))
	{
		// Snapshot before Teardown. Shutdown traces ("net down") would otherwise replace the
		// fetch error, which is what the last run put on the dialog.
		std::string why = baseUrl;
		const char* http = Update::LastHttpError();
		if (http != 0 && http[0] != '\0')
		{
			why += "\n";
			why += http;
		}
		Teardown(netUp);
		if (gCancelled)
		{
			return;		// the player asked to stop; not an error, so nothing to explain
		}
		ShowMessage("Could not reach the update server", why.c_str(), false);
		return;
	}

	LauncherPhase("update: parsing the offer");
	Update::Offer offer;
	if (!Update::ParseOffer(body, &offer))
	{
		Teardown(netUp);
		ShowMessage("The server's reply did not make sense", "Nothing was changed.", false);
		return;
	}

	/*
	 * The standalone index probe that used to sit here is gone.
	 *
	 * It dated from before the diff existed: its job was to measure how many bytes have to cross the
	 * wire to learn a package's contents, and it printed that and nothing else. The range path below now
	 * makes exactly the same measurement, with a verdict attached, so leaving it in meant reading the
	 * same ~512 KB index twice per update -- on the very path that has been crashing on hardware, which
	 * is a bad place to spend a second full parse of 1382 entries for a log line nobody reads any more.
	 */

	/*
	 * A launcher update takes the whole flow sideways, here and nowhere else.
	 *
	 * It is dispatched before the installed-versions scan below because nothing that follows applies to
	 * it: the comparison is against the launcher's own version rather than the newest install on the
	 * card, the destination is the launcher root rather than a new Installs/<version> folder, and the
	 * package's EBOOT.PBP is the launcher rather than a game. `target|launcher` in the offer is the
	 * only thing that selects this -- see Offer::launcher.
	 */
	if (offer.launcher)
	{
		LauncherSelfUpdate(offer, baseUrl, netUp);
		return;
	}

	// What is already here. Asked of the card rather than remembered, because installs can be added and
	// removed by other means and a stale idea of "the newest" is how an update gets skipped.
	std::vector<Install> installs;
	ScanInstalls(&installs);

	int highest = -1;
	for (unsigned int i = 0; i < installs.size(); ++i)
	{
		if (installs[i].versionCode > highest)
		{
			highest = installs[i].versionCode;
		}
	}

	Logger::Instance()->LogMessage("launcher: offered v%s (code %d), highest installed code %d, force %d, repair %d\n",
		offer.version.c_str(), offer.versionCode, highest, offer.force ? 1 : 0, repair ? 1 : 0);

	/*
	 * Repair: the player asked for the install they already have to be made whole.
	 *
	 * Which means the offer has to be that version. Installing whatever the server offers instead would
	 * leave the incomplete folder where it is -- still incomplete, still selected, still showing as the
	 * thing the button was pressed for -- and put a second, complete install beside it. That is a
	 * perfectly good upgrade and a useless repair, so it is refused with the reason rather than guessed
	 * at, and the version that IS on the server remains reachable from the list.
	 *
	 * Checked before the newer-version comparison below, because that comparison is exactly what a repair
	 * has to ignore: the package being reinstalled is by definition not newer.
	 */
	if (repair)
	{
		std::string wanted = BaseName(baseInstallFolder);

		if (offer.version != wanted)
		{
			Teardown(netUp);
			std::string detail = "This install is " + wanted + " and the server has " + offer.version + ".";
			ShowMessage("That version is no longer offered", detail.c_str(), false);
			return;
		}
	}

	if (!repair && offer.versionCode <= highest && !offer.force)
	{
		Teardown(netUp);
		ShowMessage("Minecraft is up to date", ("Version " + offer.version + " is already installed.").c_str(), false);
		return;
	}

	/*
	 * Which package to fetch, now that an offer may list more than one.
	 *
	 * A release publishes the full package and a patch for each version it can upgrade from, and the
	 * choice between them is made here rather than by the server. The server cannot know what is on the
	 * card; the launcher can, and it is the difference between a 95 MB wait and a few megabytes for
	 * someone on the previous version.
	 *
	 * The version a patch has to match is the install this flow already picked to patch -- the selected
	 * release's own folder, or nothing when it is not installed -- which is the same value the manifest
	 * is checked against after the download. Deciding it here from the same source keeps the two from
	 * disagreeing: a patch chosen for 3.0.1 and then offered to a 3.0.0 install would be refused after
	 * the download, which is the wait-after-the-wait the selection exists to avoid.
	 *
	 * A repair is never given a patch. ChoosePackage carries the reason; in short, a patch does not
	 * contain files that did not change between its base and the new version, and those are exactly the
	 * files a repair may be missing.
	 */
	std::string installedVersion = baseInstallFolder.empty() ? std::string() : BaseName(baseInstallFolder);
	Update::Package chosen = Update::ChoosePackage(offer, installedVersion, !repair);

	Logger::Instance()->LogMessage("launcher: chose '%s' (%u bytes, full=%d, base='%s') for installed '%s'"
		" out of %u offered package(s)\n",
		chosen.file.c_str(), chosen.size, chosen.full ? 1 : 0, chosen.base.c_str(),
		installedVersion.empty() ? "(none)" : installedVersion.c_str(),
		(unsigned int)(offer.patches.size() + 1));

	/*
	 * A patch that cannot be applied here, refused BEFORE the download.
	 *
	 * The offer says whether it is a full package or a diff, so this no longer has to be discovered by
	 * downloading the whole thing and reading the manifest out of it. That ordering matters: refusing
	 * after the download spends the player's time on a package that was never going to install, which
	 * is the worst of both -- no update, and the wait paid anyway.
	 *
	 * An offer with no `full` line is treated as a patch (the safe reading; see Offer::full), so this is
	 * also what happens against an older server that does not send the field.
	 */
	if (!chosen.full && baseInstallFolder.empty())
	{
		Teardown(netUp);
		ShowMessage("This update is a patch, not a full install",
			"Install a full package first, then patches can be applied.", false);
		return;
	}

	/*
	 * The range path: work out what installing this release would actually fetch, BEFORE the player is
	 * asked to approve it.
	 *
	 * WHY IT IS PRICED BEFORE THE PROMPT RATHER THAN AFTER. The prompt is where the cost is stated, and
	 * the cost is the entire question -- "Download 93.7 MB and install?" and "Fetch 23 files, 1.9 MB?"
	 * describe the same update and are completely different decisions. What makes the smaller figure
	 * knowable in advance is the archive's index, a few hundred KB read by range, so paying for that
	 * before the prompt costs a fraction of the download it may avoid.
	 *
	 * TWO WAYS IN, ONE CASE. It runs when the release is the version already on the card, because that is
	 * when the install lands IN PLACE -- the destination the flow computes below is the folder it is
	 * already in, so the files can be written straight over it. That is the same situation whether the
	 * player arrived through Update (a repair of a broken install) or through Check for updates (an
	 * update of a healthy one); the only reason the second used to miss this path is that "repair" was
	 * standing in for "lands in place" and the two are not the same thing. A release for a DIFFERENT
	 * version still falls through: its destination is a new folder built from a copy of the base, and
	 * writing over the base is not what is being asked for there.
	 *
	 * Everything here is best-effort on purpose. No record, no index, or an archive that cannot answer
	 * for one of the files all leave haveRangePlan false, and the flow below is exactly what it always
	 * was -- the full package, downloaded once. This is the cheap path, not the only one.
	 */
	std::string installAbs = baseInstallFolder.empty() ? std::string() : Join(root, baseInstallFolder);

	bool landsInPlace = !installedVersion.empty() && (offer.version == installedVersion);

	Update::RepairPlan rangeRepair;
	bool haveRangePlan = false;

	// `chosen.full` is not a tidiness check, it is a correctness one. ChoosePackage picks a PATCH when the
	// offer has one that fits, and a patch's manifest.txt is a DELTA -- the handful of files that changed
	// between two releases. PlanRangeRepair treats the archive's manifest.txt as the install's new record,
	// so running this against a patch would write that delta over a ~1500-entry record and leave the
	// check able to see only the handful of files the patch happened to mention. A patch is already the
	// small download this path exists to produce, so sending it down the normal route costs nothing.
	if ((repair || landsInPlace) && chosen.full && !installAbs.empty())
	{
		Update::Manifest record;
		std::string why;

		if (!Update::ReadInstalledManifest(installAbs, &record) || record.files.empty())
		{
			Logger::Instance()->LogMessage("launcher: no install record, so the range path cannot be priced\n");
		}
		else
		{
			// The index comes FIRST now, because what needs fetching is judged against the ARCHIVE rather
			// than against the install's own record -- see CollectArchiveDiff for why the record makes the
			// wrong comparison. It is ~500 KB out of a 94 MB archive.
			std::vector<Update::RemoteEntry> index;

			LauncherPhase("update: fetching the archive index");
			if (!Update::FetchZipIndex(baseUrl + chosen.file, chosen.size, &index, &why))
			{
				Logger::Instance()->LogMessage("launcher: no index (%s); using the whole archive\n", why.c_str());
			}
			else
			{
				Status("Checking what needs repair...", baseInstallFolder.c_str(), 0, -1.0f);

				Update::TreeScan scan;
				Update::BeginTreeScan(installAbs, &scan);
				LauncherPhase("update: scanning the install");
				while (!Update::StepTreeScan(&scan, 40000))
				{
					// The same walk the menu makes, and for the same reason: the record names ~1500 files,
					// and asking the card about each by name takes half a minute. It is not split across
					// frames here only because the player is already watching a status line for this exact
					// answer.
				}

				std::vector<std::string> bad;
				Update::CollectArchiveDiff(scan, record, index, &bad);

				if (bad.empty())
				{
					// The card already holds this release -- every entry in the archive is present at the
					// right length and the record agrees with its content. Nothing to fetch, and the last
					// thing worth doing about it is downloading 94 MB to prove it. Worth saying out loud
					// for an update especially: "nothing to do" is a real answer, not a silent no-op.
					Teardown(netUp);
					Logger::Instance()->LogMessage("launcher: the install already holds this release\n");
					ShowMessage(repair ? "Nothing is missing" : "Already up to date",
						"This install already matches the release.", false);
					return;
				}

				if (!Update::PlanRangeRepair(bad, index, &rangeRepair, &why))
				{
					Logger::Instance()->LogMessage("launcher: the range path is unusable (%s); using the whole archive\n", why.c_str());
				}
				else
				{
					haveRangePlan = true;
					Logger::Instance()->LogMessage(
						"launcher: range path priced -- %u file(s), %llu bytes (%.1f KB) of a %u byte archive\n",
						(unsigned int)rangeRepair.entries.size(), rangeRepair.bytes,
						(double)rangeRepair.bytes / 1024.0, chosen.size);
				}
			}
		}
	}

	// Confirm before anything large is downloaded. The size is shown because on this connection it is
	// the difference between a few seconds and a few minutes, and the kind is shown because a patch and
	// a full install of wildly different sizes used to look identical at this point.
	//
	// A priced range operation is asked about in the smallest words available, because the whole point of
	// having priced it is that the wait is now a different decision. That holds for an update as much as
	// for a repair: "Version 3.0.1 is available" over a 3.0.1 that is already on the card would read as a
	// mistake, and the player deciding whether to spend the wait needs to know what the wait buys --
	// which here is the changed files and nothing else.
	//
	// Every one of these is two lines: the question, then the size in brackets on its own line. The
	// split is not decoration -- a dialog's width is fixed, so a sentence carrying both a sentence's
	// worth of words and a figure runs over its frame (see DrawDetail) -- and the figure on its own
	// line is the part the player is actually deciding about.
	char sizeText[160];
	if (haveRangePlan)
	{
		// Two lines, and the size on the second: "Fetch 22 file(s) from the game package?" is the
		// question, and the figures beside it are what the answer costs. The old wording packed the
		// count, the size and the archive's size into one sentence, which read as arithmetic rather
		// than as a question -- and the range path's whole point is that the answer is now small.
		snprintf(sizeText, sizeof(sizeText),
			"Fetch %u file(s) from the game package?\n(%.1f KB instead of %.1f MB)",
			(unsigned int)rangeRepair.entries.size(),
			(double)rangeRepair.bytes / 1024.0,
			(double)chosen.size / (1024.0 * 1024.0));
	}
	else if (repair)
	{
		snprintf(sizeText, sizeof(sizeText),
			"Download and replace what is missing?\n(%.1f MB)",
			(double)chosen.size / (1024.0 * 1024.0));
	}
	else if (chosen.full)
	{
		snprintf(sizeText, sizeof(sizeText), "Download and install this version?\n(%.1f MB)",
			(double)chosen.size / (1024.0 * 1024.0));
	}
	else
	{
		// The patch case, and the wording says which install it is for because that is what decides
		// whether this small download is the right one: the patch is applied to a copy of the install
		// named here, not to whatever happens to be newest.
		snprintf(sizeText, sizeof(sizeText), "Update %s from %s?\n(%.1f MB)",
			offer.version.c_str(), chosen.base.c_str(), (double)chosen.size / (1024.0 * 1024.0));
	}

	std::string question = repair
		? ("Repair version " + offer.version + "?")
		: ("Version " + offer.version + " is available");
	LauncherPhase("update: waiting on the confirm");
	if (!ShowMessage(question.c_str(), sizeText, true))
	{
		Teardown(netUp);
		return;
	}

	/*
	 * The range path, when one was priced and approved: fetch those entries and nothing else.
	 *
	 * A failure here does NOT fall through to the full download. The player approved "fetch 23 files", and
	 * quietly turning that into 93.7 MB is the exact behaviour this path exists to remove -- so a failure
	 * is reported, the archive is left alone, and the full package is still one button press away.
	 *
	 * The wording follows `repair` rather than being fixed, and that is not politeness. The same fetch is
	 * a correction for one player and a routine upgrade for another, and a player who pressed Check for
	 * updates and is then told the "repair" finished has been given a word for something that did not
	 * happen to them.
	 */
	if (haveRangePlan)
	{
		Status(repair ? "Repairing..." : "Updating...", "Starting", "O to cancel", 0.0f);
		gCancelled = false;
		gLastDraw = 0;

		std::string why;
		LauncherPhase("update: applying the range fetch");
		if (Update::ApplyRangeRepair(baseUrl + chosen.file, chosen.size, installAbs,
									 rangeRepair, DownloadProgress, 0, &why))
		{
			LauncherPhase("update: finished (range fetch applied)");
			SyncCard();
			Teardown(netUp);

			Logger::Instance()->LogMessage(
				"launcher: %s v%s in '%s' -- %u file(s), %llu bytes fetched instead of %u; "
				"the install's record is now the release's\n",
				repair ? "repaired" : "updated", offer.version.c_str(), baseInstallFolder.c_str(),
				(unsigned int)rangeRepair.entries.size(), rangeRepair.bytes, chosen.size);

			ShowMessage(repair ? "Repair finished" : "Update finished",
				"Only the files that were missing or changed were fetched.", false);
			return;
		}

		Teardown(netUp);

		if (gCancelled)
		{
			return;		// the player stopped it; not an error, so nothing to explain
		}

		Logger::Instance()->LogMessage("launcher: the range %s failed: %s\n",
			repair ? "repair" : "update", why.c_str());
		ShowMessage(repair ? "The repair did not finish" : "The update did not finish", why.c_str(), false);
		return;
	}

	// The download.
	std::string zipPath = Join(root, "update.zip");

	Status("Downloading...", "Starting", "O to cancel", 0.0f);
	gCancelled = false;
	gLastDraw = 0;

	if (!Update::HttpGetToFile(baseUrl + chosen.file, zipPath, DownloadProgress, 0))
	{
		Teardown(netUp);
		if (gCancelled)
		{
			// A half-written zip that survives would be mistaken for a complete one next time, so the
			// cancel path removes it. HttpGetToFile does this itself; this is the belt to that braces.
			sceIoRemove(zipPath.c_str());
			return;
		}
		ShowMessage("The download did not complete", "Nothing was changed.", false);
		return;
	}

	// What the package says it is going to do, read before anything is created.
	Update::Manifest manifest;
	std::string why;
	if (!Update::ReadManifestFromZip(zipPath, &manifest, &why))
	{
		Teardown(netUp);
		ShowMessage("The package is not readable", why.c_str(), false);
		return;
	}

	Logger::Instance()->LogMessage("launcher: package v%s code %d, %u file(s), full=%d, base='%s'\n",
		manifest.version.c_str(), manifest.versionCode,
		(unsigned int)manifest.files.size(), manifest.full ? 1 : 0, manifest.base.c_str());

	// A patch with nothing to patch, when the offer did not say so. The offer's copy is what lets the
	// check happen before the download; this is the same check against the package's own manifest, which
	// is the one that actually travelled with the bytes and cannot be stale.
	if (!manifest.full && baseInstallFolder.empty())
	{
		Teardown(netUp);
		sceIoRemove(zipPath.c_str());
		ShowMessage("This update is a patch, not a full install",
			"Install a full package first, then patches can be applied.", false);
		return;
	}

	// A patch says which version it was diffed against. Applying it to a different one produces a tree
	// that is neither version -- and a tree that half works, which is worse than one that plainly fails
	// because nothing downstream can tell it is wrong. So a stated base that does not match what is
	// under it is refused rather than merged.
	if (!manifest.full && !manifest.base.empty())
	{
		// The base install's version is its folder name -- "Installs/3.0.1" -> "3.0.1" -- because that
		// is the rule the destination is named by, both here and in ScanInstalls.
		std::string have = BaseName(baseInstallFolder);
		if (have != manifest.base)
		{
			Teardown(netUp);
			sceIoRemove(zipPath.c_str());

			std::string detail = "It updates " + manifest.base + ", and " +
								 (have.empty() ? std::string("nothing") : have) + " is installed.";
			ShowMessage("This patch is for a different version", detail.c_str(), false);
			return;
		}
	}

	// The destination. Named for the version the PACKAGE declares, not the one the offer advertised:
	// the manifest is what is about to be written to the card, and the two disagreeing would mean the
	// folder was named for something it does not contain.
	std::string newFolder = Join("Installs", manifest.version);
	std::string newAbs = Join(root, newFolder);

	std::string baseAbs = baseInstallFolder.empty() ? std::string() : Join(root, baseInstallFolder);

	/*
	 * Patching an install onto itself, which is the case the packaging script's --force workflow
	 * produces and the case a repair always is: the tree being written to is the tree that is already
	 * there.
	 *
	 * That cannot go through the "replace it?" path below. Deleting the destination would delete the
	 * base, and the copy that follows would have nothing to read -- the update would fail AND take the
	 * player's only install with it. It does not need the copy either: the package is a diff against
	 * exactly the tree that is already there, so extracting it over that tree produces the same result
	 * the copy-then-patch path produces, without moving 135 MB across the card to get there.
	 *
	 * It is checked BEFORE the full-package branch, and that ordering is the repair feature. A repair
	 * reinstalls the full package that describes the version already on the card, so it arrives here as
	 * full=1 with the destination equal to the base -- and the full-package path would answer that by
	 * deleting the install and extracting every one of its 1500 files. Extracting over it instead lets
	 * the installer's per-file size and crc check do its job, which is what turns a repair of two missing
	 * files into two writes rather than a second full install.
	 */
	bool inPlace = (!baseAbs.empty() && newAbs == baseAbs);

	/*
	 * Build the destination.
	 *
	 * A full package is extracted into an empty folder and needs nothing else. A patch has to be
	 * applied to something, and what it is applied to is a COPY of the selected install -- the copy is
	 * what leaves the previous version intact and working, which is the whole mechanism of the
	 * launcher's rollback.
	 */
	if (inPlace)
	{
		Logger::Instance()->LogMessage("launcher: %s '%s' in place, no copy needed\n",
			repair ? "repairing" : "patching", newAbs.c_str());
	}
	else if (manifest.full)
	{
		// A full package carries every file, so an existing folder is replaced outright rather than
		// merged into -- files left over from the old version would otherwise survive the install.
		if (FileExists(newAbs))
		{
			if (!ShowMessage("That version is already installed", "Replace it?", true))
			{
				Teardown(netUp);
				return;
			}

			RemoveTree(newAbs, 0);
			SyncCard();
		}

		if (!MakeDir(newAbs))
		{
			Teardown(netUp);
			ShowMessage("Could not create the install folder", newFolder.c_str(), false);
			return;
		}
	}
	else
	{
		if (FileExists(newAbs))
		{
			if (!ShowMessage("That version is already installed", "Replace it?", true))
			{
				Teardown(netUp);
				return;
			}

			RemoveTree(newAbs, 0);
			SyncCard();
		}

		Status("Preparing the update...", "Copying the installed game", "This can take a minute", 0.0f);
		gLastDraw = 0;

		if (!CopyTree(baseAbs, newAbs, CopyProgress, 0))
		{
			Teardown(netUp);
			// The partial copy is removed: an install that is missing files looks exactly like a
			// complete one to the menu, which would then offer to play it.
			RemoveTree(newAbs, 0);
			SyncCard();
			ShowMessage("Could not copy the installed game", "Nothing was changed.", false);
			return;
		}
	}

	/*
	 * Apply the package.
	 *
	 * ExtractPackage writes each archive entry to the path recorded in it, relative to the working
	 * directory -- so the working directory has to BE the new install folder for the duration. That is
	 * the same constraint the old Updater solved with a chdir, and it is solved the same way here; the
	 * difference is that the launcher also has somewhere to go back to, so the directory is restored
	 * afterwards rather than being left wherever it ended up.
	 */
	bool installed = false;

	if (sceIoChdir(newAbs.c_str()) < 0)
	{
		Teardown(netUp);
		ShowMessage("Could not open the new install folder", newFolder.c_str(), false);
		return;
	}

	Update::Manifest written;
	Status("Installing...", "Starting", 0, 0.0f);
	gLastDraw = 0;

	installed = Update::InstallPackage(zipPath, &written, &why, InstallProgress, 0);

	// Back to the launcher's own folder, unconditionally -- the error paths below read and write paths
	// relative to the root, and one of them is about to.
	sceIoChdir(root.c_str());

	if (!installed)
	{
		Teardown(netUp);
		Logger::Instance()->LogMessage("launcher: install FAILED: %s\n", why.c_str());

		/*
		 * The half-installed folder goes.
		 *
		 * This is the one place the launcher is not "keep everything for diagnosis". A directory that
		 * is missing files still looks like an install to the menu -- it has a plausible name, and for
		 * the patch case it has a complete copy of the previous version with some files half replaced
		 * -- so leaving it behind produces an entry that the player can select and that will not boot.
		 * That is worse than no entry.
		 *
		 * The in-place install is the exception, and it is the opposite case rather than a variant of
		 * this one. That folder is not something the launcher just created; it is the game the player
		 * already had, and deleting it would turn a failed update into no game at all. A tree whose
		 * files are part old and part new is at worst the previous version, which still starts, so it
		 * is kept and the message says the install is the one that was there before.
		 *
		 * What IS kept either way is the PACKAGE. It verified against the server, so deleting it would
		 * throw away a good download over an install problem, and it is the only copy of the update on
		 * the card. Diagnosis is therefore still possible: the zip is in the launcher folder, and this
		 * run's log names the file the install failed on.
		 */
		if (!inPlace)
		{
			PreserveInstallLog(newAbs);
			RemoveTree(newAbs, 0);
			SyncCard();
		}

		ShowMessage("The install did not finish",
			inPlace ? (repair ? "The install was checked and something in it still needs repair."
							 : "The installed game was left as it was.")
					: why.c_str(), false);
		return;
	}

	// install.txt, so the menu can describe this install without inferring anything from its name. This
	// is the only record of what the folder is, which is why it is written only after the install was
	// verified -- a descriptor for an install that failed would make a broken folder selectable.
	//
	// A repair rewrites it too, which is harmless and occasionally the point: an install whose
	// install.txt was itself one of the missing files gets it back here.
	char descriptor[192];
	snprintf(descriptor, sizeof(descriptor), "name|Minecraft\nversion|%s\ncode|%d\n",
		written.version.c_str(), written.versionCode);

	if (!WriteTextFile(Join(newAbs, "install.txt"), descriptor))
	{
		Logger::Instance()->LogMessage("launcher: WARNING could not write install.txt for %s\n",
			newFolder.c_str());
	}

	// Gone only now. The package survived every failure path above, deliberately: it verified against
	// the server, so deleting it on a failure would throw away a good download over an install problem,
	// and it is the only copy of the update on the card.
	sceIoRemove(zipPath.c_str());
	SyncCard();

	Teardown(netUp);

	Logger::Instance()->LogMessage("launcher: %s v%s into '%s'\n",
		repair ? "repaired" : "installed", written.version.c_str(), newFolder.c_str());

	if (repair)
	{
		// Deliberately not "choose Play": the launcher re-checks the install when it rescans, and if
		// something is still missing that check will say so and the button will still say Update. This
		// message reports what just happened, and the menu decides what to say about the result.
		ShowMessage("Repair finished", "Every file in the package is now on the card.", false);
	}
	else
	{
		std::string done = "Installed version " + written.version;
		ShowMessage(done.c_str(), "Choose Play to start it.", false);
	}
}
