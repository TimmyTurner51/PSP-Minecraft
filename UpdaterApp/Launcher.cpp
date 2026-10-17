#include "Launcher.h"
#include "LauncherFiles.h"
#include "LauncherUpdate.h"

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <pspdebug.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>

#include "Screen.h"
#include "Updater.h"
#include "LaunchDir.h"

#include <Aurora/Utils/Logger.h>

using namespace Aurora::Utils;

/*
 * The layout.
 *
 * ONE SCREEN, FOUR ZONES -- two stacked in a window that scrolls, and a row of buttons beneath:
 *
 *     +---------------------------------------------+
 *     |  preview image (the screenshot)             |   one scrollable column,
 *     |  version name                               |   Up/Down to move it,
 *     |  changelog for the highlighted version      |   clipped at the window edges
 *     +---------------------------------------------+
 *     [ 3.0.1 v ]  ......  [    Play    ]  [ Delete ]
 *
 * The version selector is a dropdown at the BOTTOM LEFT that opens UPWARDS, so the list of versions
 * grows into the empty middle of the screen instead of off the bottom edge, and so the thing you are
 * choosing between never covers the thing you are reading about it. Play is bottom-centre-right and
 * Delete sits to its right; the three buttons are narrower than the game's 190 px button because three
 * of those do not fit across 480 px.
 *
 * WHY THE BOTTOM ROW IS THREE BUTTONS AND NOT TWO
 * -----------------------------------------------
 * The obvious arrangement is a dropdown and one button whose label changes -- Play when the chosen
 * version is installed, Install when it is not. That was rejected: a button that changes what it does
 * is worse than a button that is sometimes unavailable, because the player has to read it every time
 * to know which it is. Play stays Play, and it stays lit, whatever version is highlighted.
 *
 * What Play does for a version that is NOT installed is download it -- the one thing that button could
 * usefully do in that state, and the reason it is not dimmed. An unavailable Play would need somewhere
 * else to put the download, and the only candidates were a fourth button or a label that changes on the
 * third one, which is the arrangement this paragraph just rejected. So Play is always available and
 * always means the same thing: start this version, fetching it first if it is not here yet.
 */

// The top window: ONE scrollable column, the screenshot first and the changelog underneath it, rather
// than the side-by-side pair this used to be. That is forced by the controls rather than chosen for
// looks: the screenshot and the text are two rungs of the same ladder climbed with Up and Down, so they
// have to be stacked along that axis.
//
// The scroll is measured in PIXELS, not rows, because the two things in the column are different
// heights -- the screenshot block is taller than any line of text -- and one offset has to span both.
#define TOP_X0        8.0f
#define TOP_Y0        8.0f
#define TOP_W       464.0f
#define TOP_H       206.0f
#define TOP_TEXT_X   14.0f
#define TOP_LINE_H   11.0f
#define NOTE_SIZE     0.42f

// Where a line of changelog sits relative to its row.
//
// intraFont lands the BASELINE of the text on the y it is given rather than the top of the line: a line
// drawn at y has its ink at roughly [y - 6, y] at this size, which was measured off a dump rather than
// assumed. So a row's y is its bottom edge, and a row of text starts NOTE_ASCENT above that -- which is
// also why the focus band under the changelog has to start NOTE_ASCENT above the first line's y. Drawn
// from the first line's y instead, the band covered every line but the first, which is how it looked.
#define NOTE_ASCENT   6.0f

// The gallery strip: a row of the release's screenshots, 16:9 so the fitter's letterbox is zero on the
// sides. THUMB_W is the width a tile takes when the row has space for it; a release with more shots
// than that shares the width out, so the size actually drawn lives in gStripTile* rather than being one
// of these. The block is a tile plus the version caption underneath it.
#define THUMB_W     136.0f
#define THUMB_GAP     8.0f
#define CAPTION_H    24.0f

// The gallery cursor's line width. Three pixels, not two: two read as a crisp outline in the emulator,
// which upscales the whole screen, and washed out on the console's own 480x272 panel where there is no
// such help. A cursor the player cannot see is the same as no cursor.
#define CURSOR_T      3.0f

// The fullscreen viewer.
#define FULL_CX     240.0f
#define FULL_CY     128.0f
#define FULL_BOX_W  448.0f
#define FULL_BOX_H  208.0f

// The bottom row. 246 leaves 14 px below a 24 px button.
#define ROW_Y       246.0f
#define DD_CX        85.0f
#define DD_W        150.0f
#define PLAY_CX     245.0f
#define PLAY_W      150.0f
#define MANAGE_CX   390.0f
#define MANAGE_W    120.0f

// The Manage menu, which opens upward out of the Manage button the way the version list opens out of
// its own. Wider than the button because one of its rows is a sentence rather than a word, and the same
// 26 px pitch as every other list here.
#define MM_W        176.0f
#define MM_PITCH    26.0f

// The Manage screen's list. The same pitch as the version list, because it is the same kind of thing,
// but wider and higher up: a row here names a folder rather than a version, and nothing opens over it.
#define MAN_CX      240.0f
#define MAN_W       420.0f
#define MAN_Y0      54.0f
#define MAN_ROWS    6
#define MAN_PITCH   26.0f

// The install state, above the version button rather than under the screenshot. It reads as a property
// of the chosen version, and the button it sits over is the control that chooses it; under the picture
// it looked like part of the screenshot's caption. It also frees a line of the top window for text.
//
// 228 rather than 220: the label is drawn with its baseline on this y, so its ink sits ABOVE it and at
// 220 the top of the text collided with the bottom of the changelog panel, which ends at TOP_Y0+TOP_H.
#define STATUS_Y    228.0f

// The opened dropdown. Six rows at the game's 26 px pitch reach from y=90 to y=232, which is the whole
// space between the top of the screen and the row of buttons.
#define DD_ROWS        6
#define DD_PITCH    26.0f

// The fake "fetched" release list. See LoadReleases: absent, the launcher simply lists what is on the
// card, so a build with no server still works.
#define VERSIONS_FILE "versions.txt"

struct Release
{
	std::string version;
	int versionCode;
	std::string changelog;
	std::vector<std::string> images;	// relative to the launcher root; may be empty
	int installIndex;					// index into gInstalls, or -1

	Release() : versionCode(0), installIndex(-1) {}
};

// What the bottom row can be focused on. The third was Delete, which could only ever act on the one
// install the version list pointed at. It is Manage now, because the installs it has to reach -- the
// duplicates, and the folders copied onto the card by hand -- are exactly the ones that list cannot
// point at.
enum { FOCUS_DROPDOWN = 0, FOCUS_PLAY, FOCUS_MANAGE, FOCUS_COUNT };

static std::vector<Install> gInstalls;
static std::vector<Release> gReleases;

static int gFocus = FOCUS_PLAY;
static bool gDropdownOpen = false;
static int gVersionSel = 0;
static int gVersionTop = 0;			// first visible row of the opened dropdown

// Which rung of the interface the d-pad is steering. They form a ladder: the row of buttons at the
// bottom, the changelog above it, the screenshot above that, and the fullscreen viewer entered from the
// screenshot. Up and Down climb it, O steps back down one rung. Only one rung is ever live.
//
// Scrolling the changelog is a rung of its own rather than something Up and Down do while standing on
// the changelog. That is deliberate: when the same keys both moved between rungs and scrolled the text,
// the text slid whenever the cursor stepped past it, and the only way down to the buttons was to scroll
// the whole changelog back to the bottom. X enters this rung from the changelog and O leaves it.
enum { LEVEL_BUTTONS = 0, LEVEL_CHANGELOG, LEVEL_NOTE_SCROLL, LEVEL_SHOT, LEVEL_FULLSCREEN, LEVEL_MANAGE };
static int gLevel = LEVEL_BUTTONS;

// The Manage screen's cursor and its first visible row. A list of its own rather than a reuse of the
// version list's, because it is a list of a different thing -- folders under Installs/, not versions on
// offer -- and the two have different lengths, different orders and different actions.
static int gManageSel = 0;
static int gManageTop = 0;

// The Manage menu: the Manage BUTTON's own list, the same shape as the version list rather than a screen
// of its own -- opens upward out of the control that owns it, up and down move, X chooses, O backs out.
//
// Its rows are actions rather than things, and whether Delete is one of them depends on what is
// selected, so the list is derived from the selection rather than fixed at compile time. The screen the
// button used to open is still reachable from the frame-dump switch, which is the only way to
// photograph a rung; nothing in play reaches it any more.
static bool gManageOpen = false;
static int gManageMenuSel = 0;

// How far the top window's column is scrolled, in pixels from the top of its content. Pixels rather
// than rows because the screenshot block is taller than a line of text and both share the one offset.
static float gTopScroll = 0.0f;

// Which of the selected version's screenshots is showing. Shared between the preview and the viewer, so
// stepping through them fullscreen leaves the preview on the one you stopped at.
static int gImageSel = 0;

// The size the strip's tiles were loaded at, which is the size they are drawn at: a sprite's scale is
// baked into its vertices, so the loader and the drawer have to agree on this number rather than each
// working it out. Both read it from here, and RefreshImages is the only thing that sets it.
static float gStripTileW = THUMB_W;
static float gStripTileH = THUMB_W * 9.0f / 16.0f;

// The strip's block in the scrolling column: the tiles plus the caption under them.
static float StripBlockH()
{
	return gStripTileH + CAPTION_H;
}

static std::string gNotice;

/*
 * The "Update Required" modal.
 *
 * It is a PROMPT drawn over the menu rather than a rung on the level ladder, and the difference matters.
 * A level is a place the d-pad moves between; this is raised by the integrity scan the moment an install
 * is found broken, while the player may be standing somewhere else entirely -- so it is drawn over
 * whatever is there and swallows input until it is answered.
 *
 * `gUpdatePromptVersion` is the version this has already been raised for, and it does the same work
 * whether the player dismissed it or repaired it. O has to mean "not now" rather than "ask me again
 * next frame": the scan goes on reporting the install broken, and a modal that reappeared sixty times a
 * second could not be dismissed at all. A DIFFERENT broken version raises it again, because that is a
 * new problem rather than the same one being repeated.
 *
 * It is set when the prompt is RAISED rather than when it is dismissed, which is why the X path clears
 * it again: a repair that leaves the install broken is still the answer the scan is giving, and X is
 * the press that says "and again". O is the press that leaves the version recorded, and that is the
 * difference between the two answers the modal offers.
 *
 * `gUpdatePromptAuto` records that the SCAN raised this rather than a button. It decides whether "not
 * now" is on offer: a player who has just pressed Update has already said what they want, and giving
 * them a way to decline the thing they asked for would be a nonsense. The scan is the only thing that
 * raises it today, so this is true whenever the modal is up -- the flag is what a raise from a rule of
 * thumb, such as Play over an install the scan has already called broken, would need in order to offer
 * the other behaviour, and it is read rather than assumed so that path cannot be added and forgotten.
 *
 * `gUpdatePromptDetail` is the reason line. It is built by the scan and kept here because the counts it
 * names are released the moment the scan finishes -- by the time the modal is drawn there is nothing
 * left to count. See PumpIntegrityCheck.
 *
 * `gUpdatePromptRepair` says which flow X runs when the modal is answered with X. The integrity scan
 * raises this for an install that does not match its OWN record, and there the answer is a repair: the
 * files it is missing are ones the release already describes. The boot check raises it for an install
 * that matches its own record but not the RELEASE, and there the answer is an update -- both arrive at
 * the same range path, but "Repair version 3.0.1?" over a card whose only fault is being a build behind
 * would name a fault that is not there.
 */
static bool gUpdatePrompt = false;
static bool gUpdatePromptAuto = false;
static bool gUpdatePromptRepair = true;
static std::string gUpdatePromptVersion;
static std::string gUpdatePromptDetail;

// Defined with the rest of the integrity machinery, below, but needed by LoadReleases: the answers it
// caches are about the installs that LoadReleases rebuilds, so they cannot outlive it.
static void ForgetIntegrity();

// The changelog, already wrapped. Rebuilt only when the selection changes, because measuring every
// line every frame to lay out text that has not changed is work the PSP does not need to do 60 times a
// second.
static std::vector<std::string> gNoteLines;

// The last button state, for edge detection. Held here rather than per-loop so that a dialog and the
// menu that opened it share one idea of what has already been pressed -- otherwise the X that opens a
// confirmation is still held when the confirmation starts listening, and answers itself.
static unsigned int gLastButtons = 0;

std::string LauncherRootPath()
{
	return LauncherRoot();
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

// Buttons that went down since the last call. Every loop in this file polls once per frame, so a
// button held down reads as one press and not sixty.
static unsigned int PollPressed()
{
	SceCtrlData pad;
	memset(&pad, 0, sizeof(pad));
	sceCtrlReadBufferPositive(&pad, 1);

	unsigned int pressed = pad.Buttons & ~gLastButtons;
	gLastButtons = pad.Buttons;
	return pressed;
}

// Swallows whatever is held right now, and refuses to report anything for a moment. Called at the start
// of a dialog for the reason in the note on gLastButtons.
static void SettleInput()
{
	SceCtrlData pad;
	memset(&pad, 0, sizeof(pad));
	sceCtrlReadBufferPositive(&pad, 1);
	gLastButtons = pad.Buttons;

	sceKernelDelayThread(180000);
}

// ---------------------------------------------------------------------------------------------
// The install list
// ---------------------------------------------------------------------------------------------

/*
 * install.txt, parsed as the same pipe-separated lines the rest of this project uses:
 *
 *     name|Minecraft
 *     version|3.0.1
 *     code|30001
 *
 * Written by the update flow when it creates an install. Read rather than inferred from the folder
 * name because the folder name is whatever the version looked like at the time, while these fields are
 * what the protocol compares. A file that does not parse leaves the defaults in place and is not an
 * error: see the note on ScanInstalls in Launcher.h.
 */
static void ReadInstallFile(const std::string& absoluteFolder, Install* inst)
{
	std::string text;
	if (!ReadTextFile(Join(absoluteFolder, "install.txt"), &text))
	{
		return;
	}

	size_t at = 0;
	while (at < text.size())
	{
		size_t end = text.find('\n', at);
		if (end == std::string::npos)
		{
			end = text.size();
		}

		std::string line = text.substr(at, end - at);
		at = end + 1;

		// Tolerate CRLF: this file can be written by a script on a PC as easily as by the launcher.
		if (!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
		}
		if (line.empty())
		{
			continue;
		}

		size_t bar = line.find('|');
		if (bar == std::string::npos)
		{
			continue;
		}

		std::string key = line.substr(0, bar);
		std::string value = line.substr(bar + 1);

		if (key == "name")
		{
			// The launcher shows the version; the name is kept for a future multi-name list. Parsed
			// anyway so that an unknown key is not the thing that breaks the parse.
			(void)value;
		}
		else if (key == "version" && !value.empty())
		{
			inst->version = value;
		}
		else if (key == "code")
		{
			inst->versionCode = atoi(value.c_str());
		}
	}
}

static bool InstallNewer(const Install& a, const Install& b)
{
	// Descending by version code, with the folder name as the tie-break so the order is stable rather
	// than whatever the directory happened to hand back. `code` is what the protocol compares; the name
	// only breaks ties between two installs that claim the same code.
	if (a.versionCode != b.versionCode)
	{
		return a.versionCode > b.versionCode;
	}
	return a.folder > b.folder;
}

unsigned int ScanInstalls(std::vector<Install>* out)
{
	out->clear();

	std::string root = LauncherRootPath();
	if (root.empty())
	{
		return 0;
	}

	std::string base = Join(root, "Installs");

	// Created here rather than by the update flow, because this is the first thing that runs and an
	// absent Installs/ would otherwise make an empty launcher look like a broken one.
	sceIoMkdir(base.c_str(), 0777);

	SceUID dir = sceIoDopen(base.c_str());
	if (dir < 0)
	{
		Logger::Instance()->LogMessage("launcher: cannot open '%s' (0x%08X)\n",
			base.c_str(), (unsigned int)dir);
		return 0;
	}

	SceIoDirent entry;
	while (true)
	{
		memset(&entry, 0, sizeof(entry));
		if (sceIoDread(dir, &entry) <= 0)
		{
			break;
		}

		if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
		{
			continue;
		}
		if (!FIO_S_ISDIR(entry.d_stat.st_mode))
		{
			continue;
		}

		Install inst;
		inst.folder = Join("Installs", entry.d_name);
		inst.version = entry.d_name;		// the fallback, if install.txt is absent or unreadable
		inst.versionCode = 0;

		ReadInstallFile(Join(root, inst.folder), &inst);

		Logger::Instance()->LogMessage("launcher: found install '%s' version '%s' code %d\n",
			inst.folder.c_str(), inst.version.c_str(), inst.versionCode);

		out->push_back(inst);
	}

	sceIoDclose(dir);

	std::sort(out->begin(), out->end(), InstallNewer);
	return (unsigned int)out->size();
}

static void RefreshInstalls()
{
	ScanInstalls(&gInstalls);
}

// ---------------------------------------------------------------------------------------------
// The release list
// ---------------------------------------------------------------------------------------------

/*
 * Wraps `text` into `out`, one line per entry, at the given size and width.
 *
 * The width comes from Screen::MeasureText, which measures with the same font and size the text will be
 * drawn with -- so this wraps to what will actually be drawn rather than to an estimate. A word longer
 * than the panel is left overlong rather than broken, because there is nothing sensible to break it at
 * and a truncated word is worse than a wide one.
 */
static void WrapInto(const std::string& text, float size, float width, std::vector<std::string>* out)
{
	out->clear();

	std::string line;
	size_t i = 0;

	while (i < text.size())
	{
		size_t j = text.find_first_of(" \n", i);
		std::string word = (j == std::string::npos) ? text.substr(i) : text.substr(i, j - i);
		bool hardBreak = (j != std::string::npos && text[j] == '\n');
		size_t next = (j == std::string::npos) ? text.size() : j + 1;

		if (!word.empty())
		{
			std::string candidate = line.empty() ? word : (line + " " + word);

			// Wrapped only when there is already something on the line; otherwise a single word wider
			// than the panel would be pushed to a line of its own and then pushed again, forever.
			if (!line.empty() && Screen::Instance()->MeasureText(size, candidate.c_str()) > width)
			{
				out->push_back(line);
				line = word;
			}
			else
			{
				line = candidate;
			}
		}

		if (hardBreak)
		{
			out->push_back(line);
			line.clear();
		}

		i = next;
	}

	if (!line.empty())
	{
		out->push_back(line);
	}
}

/*
 * The release list, as the launcher sees it.
 *
 * "Fetched" is a fiction for now: the list is read from versions.txt next to the launcher, and is
 * therefore a file that the real fetch can start writing without anything below changing shape. When
 * the file is absent the launcher lists what is on the card instead, which is the honest answer -- it
 * knows what it has, and it will not invent versions it cannot produce.
 */
static void LoadReleases()
{
	RefreshInstalls();
	gReleases.clear();

	// The installs this is about to describe are not the ones the integrity answers were about -- this
	// runs on entry and after every update and delete, which is exactly when the tree under them may
	// have changed. Keeping a verdict across that would be reporting on files that are no longer there.
	ForgetIntegrity();

	std::string root = LauncherRootPath();
	std::string text;

	if (!root.empty() && ReadTextFile(Join(root, VERSIONS_FILE), &text))
	{
		Release cur;
		bool have = false;

		size_t at = 0;
		while (at <= text.size())
		{
			size_t end = text.find('\n', at);
			if (end == std::string::npos)
			{
				end = text.size();
			}

			std::string line = (at <= text.size()) ? text.substr(at, end - at) : std::string();
			at = end + 1;

			if (!line.empty() && line[line.size() - 1] == '\r')
			{
				line.erase(line.size() - 1);
			}

			size_t bar = line.find('|');
			if (bar == std::string::npos)
			{
				// A blank line ends an entry; anything else without a bar is skipped rather than
				// treated as an error, so a comment or a stray line cannot lose the whole list.
				if (have && line.empty())
				{
					gReleases.push_back(cur);
					cur = Release();
					have = false;
				}
				continue;
			}

			std::string key = line.substr(0, bar);
			std::string value = line.substr(bar + 1);

			if (key == "version")
			{
				// A new version starts a new entry whether or not the previous one was closed with a
				// blank line, so the format does not depend on a trailing separator surviving.
				if (have)
				{
					gReleases.push_back(cur);
					cur = Release();
				}
				cur.version = value;
				have = true;
			}
			else if (key == "code")
			{
				cur.versionCode = atoi(value.c_str());
			}
			else if (key == "image" && !value.empty())
			{
				// Repeatable: a version can ship several screenshots, and they are shown and stepped
				// through in the order the file lists them.
				cur.images.push_back(value);
			}
			else if (key == "notes")
			{
				if (!cur.changelog.empty())
				{
					cur.changelog += "\n";
				}
				cur.changelog += value;
			}
		}

		if (have)
		{
			gReleases.push_back(cur);
		}
	}

	// Which of them are on the card. Matched by version string rather than by code, because a folder
	// put there by hand has a name and possibly no install.txt at all, and a version the player can see
	// in the list should be startable if the folder is there.
	for (unsigned int r = 0; r < gReleases.size(); ++r)
	{
		for (unsigned int i = 0; i < gInstalls.size(); ++i)
		{
			if (gInstalls[i].version == gReleases[r].version)
			{
				gReleases[r].installIndex = (int)i;
				if (gReleases[r].versionCode == 0)
				{
					gReleases[r].versionCode = gInstalls[i].versionCode;
				}
				break;
			}
		}
	}

	// Anything installed that the list does not mention is still offered. A launcher that hid a game
	// sitting on the card would be the only way to end up unable to start it.
	for (unsigned int i = 0; i < gInstalls.size(); ++i)
	{
		bool listed = false;
		for (unsigned int r = 0; r < gReleases.size(); ++r)
		{
			if (gReleases[r].version == gInstalls[i].version)
			{
				listed = true;
				break;
			}
		}
		if (!listed)
		{
			Release extra;
			extra.version = gInstalls[i].version;
			extra.versionCode = gInstalls[i].versionCode;
			extra.installIndex = (int)i;
			extra.changelog = "Installed on this card. No changelog was fetched for it.";
			gReleases.push_back(extra);
		}
	}

	// Newest first, the same order as the install list, so "the top one" means the same thing in both.
	std::sort(gReleases.begin(), gReleases.end(),
		[](const Release& a, const Release& b) { return a.versionCode > b.versionCode; });

	Logger::Instance()->LogMessage("launcher: %d release(s), %d install(s)\n",
		(int)gReleases.size(), (int)gInstalls.size());
}

// The selected release, or null when there are none.
static const Release* Selected()
{
	if (gVersionSel < 0 || gVersionSel >= (int)gReleases.size())
	{
		return 0;
	}
	return &gReleases[gVersionSel];
}

// Hands back whatever the current rung loaded. Used where the point is to give the heap back rather
// than to change what is on screen -- before a launch, a delete and an update -- because the images are
// the launcher's only large allocations and the incoming work needs contiguous heap. RefreshImages is
// what puts them back.
static void FreeImages()
{
	Screen::Instance()->FreeStrip();
	Screen::Instance()->FreePanelImage();
}

/*
 * Loads what the current rung draws: the strip on the menu, the single shot in the viewer.
 *
 * The two are exclusive rather than held together. TextureManager::RemoveTexture erases an entry and
 * shifts every later index down, so a texture may only be released while it is the LAST one loaded --
 * which is why exactly one of these is ever loaded at a time, why the strip is freed before the panel
 * is loaded, and why the strip is loaded after everything else the menu needs.
 *
 * Called wherever the selection or the rung changes, and to put the images back after a launch that
 * failed. See DoPlay for why a failed launch has to restore them.
 */
static void RefreshImages()
{
	Screen* screen = Screen::Instance();
	const Release* rel = Selected();

	// Both first: whichever was loaded, it is the last texture in the list, so releasing now is safe.
	// Freeing the one that is not loaded is a no-op.
	screen->FreeStrip();
	screen->FreePanelImage();

	if (rel == 0 || rel->images.empty())
	{
		return;
	}

	if (gImageSel < 0)
	{
		gImageSel = 0;
	}
	if (gImageSel >= (int)rel->images.size())
	{
		gImageSel = (int)rel->images.size() - 1;
	}

	std::string root = LauncherRootPath();

	// The viewer draws one shot as large as the screen allows. The menu draws them all as a row.
	if (gLevel == LEVEL_FULLSCREEN)
	{
		std::string path = Join(root, rel->images[gImageSel]);
		if (!screen->LoadPanelImage(path.c_str(), FULL_BOX_W, FULL_BOX_H))
		{
			Logger::Instance()->LogMessage("launcher: no screenshot at '%s'\n", path.c_str());
		}
		return;
	}

	// Share the row's width between the shots, up to the size a tile takes when there is room. A single
	// shot does not stretch to fill the row, and a release with many does not overflow it.
	int count = (int)rel->images.size();
	float available = TOP_W - 8.0f - (float)(count - 1) * THUMB_GAP;
	gStripTileW = available / (float)count;
	if (gStripTileW > THUMB_W)
	{
		gStripTileW = THUMB_W;
	}
	gStripTileH = gStripTileW * 9.0f / 16.0f;

	std::vector<std::string> paths;
	for (unsigned int i = 0; i < rel->images.size(); ++i)
	{
		paths.push_back(Join(root, rel->images[i]));
	}

	screen->LoadStrip(paths, gStripTileW, gStripTileH);
}

// Steps to the next or previous screenshot of the current version, wrapping, and reloads it.
static void CycleImage(int delta)
{
	const Release* rel = Selected();
	if (rel == 0 || rel->images.empty())
	{
		return;
	}

	int count = (int)rel->images.size();
	gImageSel = (gImageSel + delta + count) % count;
	RefreshImages();
}

// How far the top window can scroll: the content less the window, or nothing when it already fits.
static float TopMaxScroll()
{
	float content = StripBlockH() + (float)gNoteLines.size() * TOP_LINE_H;
	float maxScroll = content - TOP_H;
	if (maxScroll < 0.0f)
	{
		maxScroll = 0.0f;
	}
	return maxScroll;
}

/*
 * Makes a release current: clamps the index, rewraps the changelog, and loads or releases the preview
 * image.
 *
 * The image is the reason this is not just an assignment. A preview costs about a megabyte of texture
 * once padded to a power of two, and the update path needs several megabytes of contiguous heap of its
 * own -- so the image is loaded for the release being looked at and released for every other one,
 * rather than being cached. The wrap is here for the cheaper reason that measuring a paragraph every
 * frame is work with no purpose.
 */
static void SelectRelease(int index)
{
	if (index < 0)
	{
		index = 0;
	}
	if (index >= (int)gReleases.size())
	{
		index = (int)gReleases.size() - 1;
	}

	gVersionSel = index;

	const Release* rel = Selected();
	if (rel == 0)
	{
		FreeImages();
		gNoteLines.clear();
		return;
	}

	gNoteLines.clear();
	if (!rel->changelog.empty())
	{
		WrapInto(rel->changelog, NOTE_SIZE, TOP_W - 12.0f, &gNoteLines);
	}

	// A new version starts at its first screenshot and at the top of its text. Carrying either over from
	// the last version would drop the reader into the middle of a paragraph they have not seen the start
	// of, and onto a screenshot they never chose.
	gImageSel = 0;
	gTopScroll = 0.0f;
	RefreshImages();
}

// Keeps the highlighted release inside the visible window of the opened dropdown.
static void ClampDropdown()
{
	if (gVersionSel < gVersionTop)
	{
		gVersionTop = gVersionSel;
	}
	if (gVersionSel >= gVersionTop + DD_ROWS)
	{
		gVersionTop = gVersionSel - DD_ROWS + 1;
	}
	if (gVersionTop < 0)
	{
		gVersionTop = 0;
	}
}

static void RefreshCatalogFromServer();

static void MoveSelection(int delta)
{
	if (gReleases.empty())
	{
		return;
	}

	int next = gVersionSel + delta;
	if (next < 0)
	{
		next = 0;								// clamped, not wrapped: a version list is not a ring,
	}											// and wrapping off the end of it is disorienting
	if (next >= (int)gReleases.size())
	{
		next = (int)gReleases.size() - 1;
	}

	if (next != gVersionSel)
	{
		SelectRelease(next);
		ClampDropdown();
	}
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------

static void DrawTextFallback()
{
	// The card has no Assets folder, so there is no dirt and no font. The launcher's whole purpose is
	// to be the thing that still works when the game does not -- which includes the case where the
	// assets are exactly what went missing -- so the menu is drawn as text rather than not at all.
	pspDebugScreenClear();
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenPrintf("MineCraft Launcher\n\n");

	const Release* rel = Selected();
	if (rel == 0)
	{
		pspDebugScreenPrintf("  No versions found.\n\n");
	}
	else
	{
		pspDebugScreenPrintf("  Version: %s  %s\n\n", rel->version.c_str(),
			(rel->installIndex >= 0) ? "(installed)" : "(not installed)");
		pspDebugScreenPrintf("%s\n\n", rel->changelog.c_str());
	}

	if (!gNotice.empty())
	{
		pspDebugScreenPrintf("%s\n\n", gNotice.c_str());
	}

	pspDebugScreenPrintf("Left/Right button   Up changelog   X scroll text\n");
	pspDebugScreenPrintf("X play or download   O back / quit\n");
}

/*
 * The row of a release's screenshots, centred on cx with its top edge at topY, and the outline cursor
 * around the chosen one.
 *
 * The cursor is drawn ON the tile's edge rather than just outside it, for the same reason the single
 * preview's frame was: it cannot then poke past the window's top edge and be cut off by the scissor.
 * It is opaque, because a translucent one over a dark screenshot reads as a dimmed tile rather than as
 * a cursor -- which is exactly how the previous treatment was described.
 *
 * If the row is wider than the window the tiles are scrolled to keep the chosen one in view, so a
 * release with more shots than fit still reaches all of them.
 */
static void DrawStrip(Screen* screen, float cx, float topY)
{
	const int count = screen->StripCount();
	if (count <= 0)
	{
		return;
	}

	const float rowW = (float)count * gStripTileW + (float)(count - 1) * THUMB_GAP;

	// Centred when it fits, scrolled when it does not. Only the chosen tile is guaranteed visible, which
	// is the one the player is steering.
	float originX = cx - rowW * 0.5f;
	const float overflow = rowW - TOP_W + 8.0f;
	if (overflow > 0.0f)
	{
		const float chosen = (float)gImageSel * (gStripTileW + THUMB_GAP) + gStripTileW * 0.5f;
		float scroll = chosen - TOP_W * 0.5f;
		if (scroll < 0.0f)
		{
			scroll = 0.0f;
		}
		if (scroll > overflow)
		{
			scroll = overflow;
		}
		originX = TOP_X0 + 4.0f - scroll;
	}

	const float drawY = floorf(topY + 0.5f);

	for (int i = 0; i < count; ++i)
	{
		const float tileX = originX + (float)i * (gStripTileW + THUMB_GAP);

		// Whole pixels, so the screenshots are not resampled by a half-pixel offset.
		screen->DrawStripItem(i, floorf(tileX + gStripTileW * 0.5f + 0.5f),
			floorf(drawY + gStripTileH * 0.5f + 0.5f));

		if (i == gImageSel && gLevel == LEVEL_SHOT)
		{
			// Drawn as four edges rather than one rectangle with a hole in it, because GU has no
			// outline primitive. They are grouped into a single call each so the four are visibly one
			// cursor -- and they are the reason Screen::Panel had to stop sharing one vertex buffer:
			// four panels in a row used to collapse into whichever edge was written last.
			//
			// CURSOR_T rather than the 2 px this started at. Two pixels of white read as an outline in
			// the emulator and washed out on the console's own panel, where there is no upscaling to
			// keep it crisp -- so it is three, which is the thinnest line that survives that screen.
			const float t = CURSOR_T;
			const float t2 = CURSOR_T * 2.0f;
			screen->Panel(tileX, drawY, gStripTileW, t, 0xFFFFFFFF);
			screen->Panel(tileX, drawY + gStripTileH - t, gStripTileW, t, 0xFFFFFFFF);
			screen->Panel(tileX, drawY + t, t, gStripTileH - t2, 0xFFFFFFFF);
			screen->Panel(tileX + gStripTileW - t, drawY + t, t, gStripTileH - t2, 0xFFFFFFFF);
		}
	}
}

/*
 * The top window: the screenshot, its version, then the changelog, as one column that scrolls under the
 * ladder's Up and Down. See the note on TOP_X0 for why those share a column.
 *
 * The caller has already clipped to the window, so a screenshot scrolled partly off the top is cut at
 * the panel edge instead of drawn over the dirt above it.
 */
static void DrawTopWindow(Screen* screen, const Release* rel)
{
	// The panel goes down first, so every glyph below sits on a known colour instead of on whatever
	// shade of dirt happens to be under it.
	screen->Panel(TOP_X0, TOP_Y0, TOP_W, TOP_H, 0x60000000);

	if (rel == 0)
	{
		screen->Text(240.0f, 108.0f, "No versions found");
		screen->Text(240.0f, 130.0f, "Put a game in Installs/ or add versions.txt");
		return;
	}

	const float shotCX = TOP_X0 + TOP_W * 0.5f;
	const float shotTop = TOP_Y0 - gTopScroll;				// the column's own y=0
	const float noteTop = TOP_Y0 - gTopScroll + StripBlockH();

	// The changelog's focus highlight, drawn before the text so the text sits on top of it. The band
	// covers only the part of the changelog the window is showing, which is why it is clamped to the
	// window: off the top when scrolled far down, off the bottom when the text is short.
	//
	// It starts NOTE_ASCENT above the first row's y. The first row's y is its baseline, so the band has
	// to reach up past it to cover the ink; starting at the y itself left the first line sitting on the
	// bare panel above the highlight.
	if (gLevel == LEVEL_CHANGELOG || gLevel == LEVEL_NOTE_SCROLL)
	{
		// A couple of pixels above the block's top: the first line's ink reaches a pixel or two past the
		// NOTE_ASCENT mark, and a band that starts exactly on it leaves that sliver sitting on the bare
		// panel -- the same complaint this fix exists for, one pixel wide.
		float y0 = noteTop - 2.0f;
		float y1 = noteTop + NOTE_ASCENT + (float)gNoteLines.size() * TOP_LINE_H;
		if (y0 < TOP_Y0) { y0 = TOP_Y0; }
		if (y1 > TOP_Y0 + TOP_H) { y1 = TOP_Y0 + TOP_H; }
		if (y1 > y0)
		{
			// Brighter while the text is actually being scrolled, so the rung you are on is legible at
			// a glance rather than only inferable from what the d-pad does.
			const unsigned int band = (gLevel == LEVEL_NOTE_SCROLL) ? 0x3CFFFFFF : 0x26FFFFFF;
			screen->Panel(TOP_X0 + 2.0f, y0, TOP_W - 4.0f, y1 - y0, band);
		}
	}

	// The row of screenshots, with the one the player has chosen outlined. Drawn here rather than
	// through DrawPanelImage because the menu holds all of a release's shots at once; see LoadStrip.
	DrawStrip(screen, shotCX, shotTop);

	screen->Text(shotCX, shotTop + gStripTileH + 9.0f, rel->version.c_str());

	// The changelog, one wrapped line per row. Rows outside the window are skipped rather than drawn
	// and clipped, because a line of text below the panel is a measure and a glyph upload that the
	// scissor throws away. The y handed to TextSized is the row's BOTTOM, because that is where intraFont
	// puts the baseline; see the note on NOTE_ASCENT.
	for (unsigned int i = 0; i < gNoteLines.size(); ++i)
	{
		float y = noteTop + NOTE_ASCENT + (float)i * TOP_LINE_H;
		if (y <= TOP_Y0 || y - NOTE_ASCENT >= TOP_Y0 + TOP_H)
		{
			continue;
		}
		screen->TextSized(TOP_TEXT_X, y, NOTE_SIZE, 0xFFFFFFFF, INTRAFONT_ALIGN_LEFT,
			gNoteLines[i].c_str());
	}

	// The outline around the chosen shot is drawn by DrawStrip, beside the tile it belongs to, so that
	// the two cannot disagree about where the tile is.
}

/*
 * The two button prompts, drawn as the buttons themselves.
 *
 * HOW A PROMPT IS SPELLED
 * -----------------------
 * A game does not write "press X" -- it draws the button. The eye picks a shape out of a line without
 * reading it, which is the whole reason the convention exists, and it also removes a class of mistake
 * this interface made: the notice that used to stand in for the download said "press U", and there is
 * no U on a PSP. A drawn button cannot be named wrongly.
 *
 * The icons come off one sheet of uniform cells; see Screen::Icon. When that sheet is missing --
 * an older card, or a texture pack that does not carry it -- the same call falls back to printing the
 * button's name, so a prompt is never blank. That is the only reason the names below exist.
 *
 * A PROMPT IS CENTRED AS A WHOLE, which is why the pieces are measured before any of them is drawn.
 * Positioned as they are drawn, a line whose icon becomes a word would grow to the right and sit
 * visibly off the middle of the screen.
 */
struct PromptPart
{
	const char* text;	// the words, or NULL when this part is a button
	int icon;			// an ICON_* value, or -1 when this part is words

	// True when this part begins a new GROUP of controls, which puts PROMPT_GROUP_GAP before it instead
	// of the ordinary gap. A prompt that offers two unrelated actions -- step the screenshots, or leave
	// -- reads as one run of controls when every seam is the same width, and the eye has to work out
	// where one instruction ends and the next begins. The wider seam does that work instead.
	//
	// Left off the initialisers below, where it therefore reads as false. That is the common case: most
	// parts continue the group they are in.
	bool startsGroup;
};

// Matches the fixed size Screen::Text draws at, so a prompt and the lines above it are the same text.
#define PROMPT_SIZE 0.5f

// How far above the baseline an icon's centre sits. The text's ink spans roughly [y - 6, y] at this
// size, so its optical middle is a few pixels above the baseline; measured off a frame dump, not
// guessed. Setting the centre on the baseline leaves the icons hanging below the words.
#define PROMPT_ICON_RISE 3.5f

// The gap between two things on a prompt line. One number, used at every seam.
//
// It has to be one number, and that is the whole point of the arithmetic in PromptLine: the button art
// is CENTRED in its 16 px cell, so a disc (12 across) has 2 px of empty cell either side and a pill
// (14 across) has 1. Spacing by the cell therefore put more air between two icons than between an icon
// and a word, and more again where a word followed one -- which is what made a prompt read as a row of
// buttons that do not quite line up with their labels. Spacing by the ink and using this gap everywhere
// makes every seam the same width.
//
// 3 rather than 2 because the gap no longer pays for the cell's empty margin, and at this size a space
// in the font is about this wide -- so an icon sits off a word the way a word does.
#define PROMPT_ICON_GAP 3.0f

// The gap between one group of controls and the next, where a prompt offers two unrelated actions. Wide
// enough to read as a break rather than as a wide space -- about the width of a short word at this size.
#define PROMPT_GROUP_GAP 20.0f

static const char* IconName(int icon)
{
	switch (icon)
	{
	case ICON_TRIANGLE: return "Triangle";
	case ICON_CIRCLE:   return "Circle";
	case ICON_CROSS:    return "Cross";
	case ICON_SQUARE:   return "Square";
	case ICON_L:        return "L";
	case ICON_R:        return "R";
	}
	return "?";
}

// What a part will actually read as: its words, or the button's name when the art is unavailable. Used
// for measuring and for drawing, so the two cannot disagree about the width of a fallback.
static const char* PromptText(const PromptPart& part, bool asIcon)
{
	return asIcon ? "" : ((part.icon >= 0) ? IconName(part.icon) : part.text);
}

/*
 * How wide a button's ink actually is inside its 16 px cell.
 *
 * This is the other half of the spacing fix described on PROMPT_ICON_GAP: laying a prompt out by the
 * cell means laying it out partly by nothing. The numbers are the sheet's own geometry rather than
 * measurements, and they are the two shapes it draws -- a face button is a centred disc 12 across and a
 * trigger is a centred tab 14 across. See tools/make-button-icons.py: DISC_R = 6, PILL_W = 14.
 *
 * This relies on the ink being centred in its cell, which is what makes "the ink starts at x" the same
 * statement as "the cell starts at x - (16 - ink) / 2". The art holds that up: every glyph on the sheet
 * is centred in its 16-pixel cell. If that ever stops being true this is what breaks, quietly, as
 * half-pixel nudges in every prompt.
 *
 * This is the one place the launcher repeats a number the art owns, so it is the number to change if
 * the tab is ever resized: an ink width that disagrees with the art shifts every prompt by half the
 * difference.
 */
static float IconInkWidth(int icon)
{
	switch (icon)
	{
	case ICON_L:
	case ICON_R:
		return 14.0f;
	}
	return 12.0f;
}

// The gap that goes before part i, which is the group gap where that part begins a new group.
static float PartGapBefore(const PromptPart* parts, int i)
{
	return (parts[i].startsGroup && i > 0) ? PROMPT_GROUP_GAP : PROMPT_ICON_GAP;
}

static void PromptLine(Screen* screen, float cx, float y, const PromptPart* parts, int count)
{
	const bool icons = screen->HasIcons();

	// Laid out by INK, not by advance, so the whole line is measured the way it is drawn -- the centring
	// is the ink's centring too, rather than the cells'. The gaps go BETWEEN the parts and nowhere else;
	// the trailing one is never drawn from. See PROMPT_ICON_GAP and PROMPT_GROUP_GAP.
	float total = 0.0f;
	for (int i = 1; i < count; ++i)
	{
		total += PartGapBefore(parts, i);
	}
	for (int i = 0; i < count; ++i)
	{
		const bool asIcon = (parts[i].icon >= 0 && icons);
		total += asIcon ? IconInkWidth(parts[i].icon)
			: screen->MeasureText(PROMPT_SIZE, PromptText(parts[i], asIcon));
	}

	float x = cx - total * 0.5f;
	for (int i = 0; i < count; ++i)
	{
		const bool asIcon = (parts[i].icon >= 0 && icons);
		if (asIcon)
		{
			// Centred on the middle of its own ink, which is what puts the glyph's left edge on x -- the
			// sprite still covers the whole cell, so its empty margin hangs either side and overlaps
			// nothing, because it is transparent.
			const float ink = IconInkWidth(parts[i].icon);

			// Placed on whole pixels, which is not a rounding nicety: the icons are 1:1 pixel art, and a
			// prompt's x is a running sum of float text widths, so without this every icon lands on a
			// fractional coordinate and the sampler resamples the whole cell -- soft edges on the one kind
			// of art that cannot survive them. The text is free to sit on a fraction because intraFont
			// draws it from an atlas that was built to be sampled; these are not.
			const float ix = floorf(x + ink * 0.5f + 0.5f);
			const float iy = floorf(y - PROMPT_ICON_RISE + 0.5f);
			screen->Icon(parts[i].icon, ix, iy);
			x += ink;
		}
		else
		{
			const char* text = PromptText(parts[i], asIcon);
			screen->TextSized(x, y, PROMPT_SIZE, 0xFFFFFFFF, INTRAFONT_ALIGN_LEFT, text);
			x += screen->MeasureText(PROMPT_SIZE, text);
		}

		if (i + 1 < count)
		{
			x += PartGapBefore(parts, i + 1);
		}
	}
}


/*
 * The fullscreen viewer. It replaces the menu rather than sitting over it: opening it reloads the shot
 * at the screen's own scale, so the menu's preview would be that same oversized sprite drawn in a
 * corner. Dimming the dirt and drawing the shot is simpler, and honest about being somewhere else.
 */
static void DrawViewer(Screen* screen, const Release* rel)
{
	screen->Panel(0.0f, 0.0f, 480.0f, 272.0f, 0xC8000000);

	if (screen->HasPanelImage())
	{
		screen->DrawPanelImage(FULL_CX, FULL_CY);
	}

	if (rel != 0 && !rel->images.empty())
	{
		char counter[32];
		snprintf(counter, sizeof(counter), "%d / %d", gImageSel + 1, (int)rel->images.size());
		screen->Text(240.0f, 248.0f, counter);
	}

	// The prompt, drawn as buttons rather than named. L and R are one control here -- either one steps
	// the shots -- so the two pills sit together ahead of the words rather than each getting a phrase.
	// No spaces around the icons: PromptLine puts the same gap at every seam. The leading spaces these
	// used to carry were hand-padding against the old model, where an icon's advance included the empty
	// margin of its cell -- so the padding was part of what made the gaps uneven. See PROMPT_ICON_GAP.
	//
	// The O starts a second group, because stepping the screenshots and leaving the viewer are two
	// different things and at one uniform gap the line read as a single run of five controls.
	static const PromptPart viewerPrompt[] = {
		{ 0, ICON_L },
		{ 0, ICON_R },
		{ "change screenshot", -1 },
		{ 0, ICON_CIRCLE, true },
		{ "back", -1 },
	};
	PromptLine(screen, 240.0f, 16.0f, viewerPrompt, 5);
}

// ---------------------------------------------------------------------------------------------
// Is the install still all there?
//
// The menu asks this so that Update can mean two things: fetch a version that is not here, and REPAIR
// one that is. The second is the reason any of this exists -- an install missing a file looks exactly
// like a complete one, and its Play button would start a game that dies on the missing asset, with
// nothing on screen having warned anyone.
//
// The answer comes from the install's own record (Update::InstallManifestName) compared against the
// card: existence and length, no file contents, because this runs while the menu is being drawn. It is
// done in slices across frames for the same reason -- 1500 lookups on a memory stick is a visible
// freeze if they all happen in one, and none of this work is urgent enough to buy that.
//
// Verdicts are cached per install folder, so moving up and down the version list does not rescan what
// has already been scanned. The cache is dropped in LoadReleases, which is the only thing that
// reshapes the install list.
// ---------------------------------------------------------------------------------------------

enum
{
	CHECK_UNKNOWN = 0,		// no record to compare against; NOT the same as broken
	CHECK_SCANNING,			// being looked at right now
	CHECK_OK,
	CHECK_BROKEN
};

struct Verdict
{
	std::string folder;
	int state;
};

static std::vector<Verdict> gVerdicts;

// The scan in progress, if any. One at a time: the menu only ever asks about the chosen version, and a
// second concurrent scan would be a second copy of the record and of the tree in memory for no benefit.
static std::string gCheckFolder;
static int gCheckState = CHECK_UNKNOWN;
static Update::Manifest gCheckRecord;
static Update::TreeScan gCheckScan;
static Update::Integrity gCheckFound;
static unsigned int gCheckStartedAt = 0;		// the clock reading the walk began at, for the log
static unsigned int gCheckNextProgress = 0;		// the directory count that next reports how far it got

// How long one frame may spend reading the card. A budget in time rather than a count, because the cost
// of a step is the card's and not this program's -- and because the unit of work is now a whole
// DIRECTORY (see Update::StepTreeScan), this caps how many open-read-close cycles a frame may do rather
// than how many files it may look at. The whole walk is a couple of hundred of them.
#define CHECK_BUDGET_US 4000

// How often the walk says where it has got to. In directories, because that is the unit it moves in, and
// coarse on purpose: this exists so that a scan which does NOT finish still leaves its last position in
// the log, on a console where there is no other way to ask.
#define CHECK_PROGRESS_STEP 64

static void ForgetIntegrity()
{
	gVerdicts.clear();
	gCheckFolder.clear();
	gCheckState = CHECK_UNKNOWN;
	gCheckRecord = Update::Manifest();
	gCheckScan = Update::TreeScan();
	gCheckFound = Update::Integrity();
	gCheckStartedAt = 0;
	gCheckNextProgress = 0;
}

static void StoreVerdict(const std::string& folder, int state)
{
	for (unsigned int i = 0; i < gVerdicts.size(); ++i)
	{
		if (gVerdicts[i].folder == folder)
		{
			gVerdicts[i].state = state;
			return;
		}
	}

	Verdict v;
	v.folder = folder;
	v.state = state;
	gVerdicts.push_back(v);
}

// The cached answer for a folder, or -1 when it has not been asked yet. The distinction matters: -1
// means "start a scan" and CHECK_UNKNOWN means "scanned, and there was nothing to judge", and
// conflating them would restart the scan on every single frame.
static int CachedVerdict(const std::string& folder)
{
	for (unsigned int i = 0; i < gVerdicts.size(); ++i)
	{
		if (gVerdicts[i].folder == folder)
		{
			return gVerdicts[i].state;
		}
	}

	return -1;
}

// What the menu should say about the chosen release's install.
static int SelectedIntegrity()
{
	const Release* rel = Selected();
	if (rel == 0 || rel->installIndex < 0)
	{
		return CHECK_UNKNOWN;
	}

	const std::string& folder = gInstalls[rel->installIndex].folder;

	// The scan that just finished is the freshest answer there is.
	if (gCheckFolder == folder && gCheckState != CHECK_SCANNING)
	{
		return gCheckState;
	}

	int cached = CachedVerdict(folder);
	return (cached < 0) ? CHECK_SCANNING : cached;
}

static void BeginCheck(const std::string& folder)
{
	gCheckFolder = folder;
	gCheckRecord = Update::Manifest();
	gCheckScan = Update::TreeScan();
	gCheckFound = Update::Integrity();
	gCheckState = CHECK_SCANNING;

	std::string root = LauncherRootPath();
	std::string abs = root.empty() ? std::string() : Join(root, folder);

	if (root.empty() ||
		!Update::ReadInstalledManifest(abs, &gCheckRecord) ||
		gCheckRecord.files.empty())
	{
		// Nothing to compare against, reported as unknown rather than broken and cached as such so this
		// is decided once. An install with no record is not a fault to be fixed -- it is one this build
		// cannot judge, because it was made by an older launcher or copied onto the card by hand. Saying
		// "Needs repair" there would be inventing a problem, and offering a repair for it would be
		// offering a 95 MB download on the strength of a guess.
		Logger::Instance()->LogMessage("launcher: '%s' has no file record, cannot check it\n", folder.c_str());
		gCheckState = CHECK_UNKNOWN;
		StoreVerdict(folder, CHECK_UNKNOWN);
		return;
	}

	// The folder the record's relative paths are resolved against. The launcher is not standing inside the
	// install, so nothing in the record means anything until the folder it describes is put in front of
	// it -- and the files beside the launcher, which those same names WOULD find, are the launcher's own
	// UI assets. The walk takes that folder once and applies it as it descends, which is also why it is
	// no longer carried around out here.
	Update::BeginTreeScan(abs, &gCheckScan);
	gCheckStartedAt = sceKernelGetSystemTimeLow();
	gCheckNextProgress = CHECK_PROGRESS_STEP;

	Logger::Instance()->LogMessage("launcher: checking '%s' -- %u file(s) in its record\n",
		folder.c_str(), (unsigned int)gCheckRecord.files.size());
}

// A slice of the scan, once per frame. Returns immediately when there is nothing to do, which is the
// ordinary case: this only works while a version is selected whose verdict is not yet known.
static void PumpIntegrityCheck()
{
	const Release* rel = Selected();
	if (rel == 0 || rel->installIndex < 0)
	{
		return;
	}

	const std::string& folder = gInstalls[rel->installIndex].folder;

	if (gCheckFolder == folder && gCheckState != CHECK_SCANNING)
	{
		return;						// already answered, and the answer is still current
	}

	if (gCheckFolder != folder)
	{
		if (CachedVerdict(folder) >= 0)
		{
			return;					// answered earlier in this session
		}

		BeginCheck(folder);
		if (gCheckState != CHECK_SCANNING)
		{
			return;					// decided before it started; see BeginCheck
		}
	}

	// Whole directories per step. The budget only decides how many of them one frame gets through, and
	// the walk returns false while it still has directories queued.
	if (!Update::StepTreeScan(&gCheckScan, CHECK_BUDGET_US))
	{
		// How far it has got, when it has got far enough to be worth saying. This is the line that tells a
		// scan which stalls on the card apart from one which never started -- without it, the only evidence
		// of a stall would be a completion line that is missing, and on the console there is nothing else
		// to ask.
		if (gCheckScan.dirsRead >= gCheckNextProgress)
		{
			Logger::Instance()->LogMessage(
				"launcher: checking '%s' -- %u folder(s) read, %u file(s) so far\n",
				folder.c_str(), gCheckScan.dirsRead, gCheckScan.filesSeen);
			gCheckNextProgress = gCheckScan.dirsRead + CHECK_PROGRESS_STEP;
		}
		return;						// more to walk next frame
	}

	Update::CompareTreeScan(gCheckScan, gCheckRecord, &gCheckFound);

	int state = gCheckFound.Ok() ? CHECK_OK : CHECK_BROKEN;
	StoreVerdict(folder, state);

	// The counts and the first offender, in one line, because "needs repair" on screen is a prompt and
	// this is the thing that says what for -- and on a card where the answer is "one file", the name of
	// that file is the whole diagnosis.
	//
	// The folder and millisecond counts are the other half of it: they are what says whether the walk is
	// affordable on the hardware it is running on, which is a question the emulator cannot answer, because
	// there the card is a desktop filesystem and every one of these numbers is meaningless.
	Logger::Instance()->LogMessage(
		"launcher: checked '%s' -- %u file(s), %u missing, %u wrong size (first: %s) "
		"[%u folder(s), %u on the card, %u ms]\n",
		folder.c_str(), gCheckFound.checked, gCheckFound.missing, gCheckFound.wrongSize,
		gCheckFound.firstProblem.empty() ? "none" : gCheckFound.firstProblem.c_str(),
		gCheckScan.dirsRead, gCheckScan.filesSeen,
		(sceKernelGetSystemTimeLow() - gCheckStartedAt) / 1000);

	// On screen the count rather than the filename: the notice line is one line wide, and a path deep
	// enough to be worth naming is also deep enough to run off the edge of it. The name is in the log,
	// which is where a specific missing file is actually useful.
	//
	// Only when the notice is empty, so this cannot overwrite a result the player just asked for -- a
	// "Deleted 3.0.1." that is immediately replaced by a repair warning reads as the delete having gone
	// wrong.
	if (state == CHECK_BROKEN && gNotice.empty())
	{
		char note[128];
		snprintf(note, sizeof(note), "%u file(s) missing or incomplete",
			gCheckFound.missing + gCheckFound.wrongSize);
		gNotice = note;
	}

	// The modal, raised by the scan rather than by a button -- the player is wherever they were when the
	// walk finished, and this is not a place they chose to be. See the note on gUpdatePrompt.
	//
	// HONEST ABOUT WHAT IT IS. It says what the scan found, in the scan's own words and counts, and the
	// two lines the modal draws are built here for that reason: the notice above is a one-line summary
	// that can be overwritten by a result the player just asked for, and this is a question that has to
	// carry its own reason with it.
	//
	// The counts have to be read HERE, because the record and the scan are released a few lines below
	// and the modal is drawn long after -- from a frame where there is nothing left to count.
	if (state == CHECK_BROKEN && rel != 0 && rel->version != gUpdatePromptVersion)
	{
		char detail[128];
		snprintf(detail, sizeof(detail), "%u file(s) missing or incomplete",
			gCheckFound.missing + gCheckFound.wrongSize);
		gUpdatePromptDetail = detail;

		// Recorded at the moment it goes up rather than when it comes down. Both answers have to leave
		// this set for the modal not to reappear the next time the scan runs -- O because "not now" is
		// the whole of its meaning, and X because DoUpdate reloads the releases, which drops every
		// verdict and starts the walk again. The X path clears it deliberately; see the note in the loop.
		gUpdatePromptVersion = rel->version;
		gUpdatePromptAuto = true;
		gUpdatePromptRepair = true;
		gUpdatePrompt = true;

		Logger::Instance()->LogMessage("launcher: raising the update prompt for '%s'\n",
			rel->version.c_str());
	}

	// The record and the tree are both hundreds of kilobytes and there is no further use for either.
	// Released rather than left for the next scan to overwrite, because the launcher's next act may be to
	// start a game, and the heap it needs is this one.
	gCheckRecord = Update::Manifest();
	gCheckScan = Update::TreeScan();
	gCheckState = state;
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------

/*
 * The Manage screen's questions, answered.
 *
 * Both are asked afresh for every row while the list is drawn, so both are linear scans of a list that
 * holds a handful of entries -- a handful being as many folders as a player will ever put under
 * Installs/. Anything cleverer here would be a faster answer to a question nothing is waiting on.
 */

// True when another install claims the same version. Matched on the VERSION STRING rather than the code,
// because the string is what the version list matches on and what a row shows: two folders holding the
// same version are the case this screen exists for, and one of them is unreachable from the menu because
// LoadReleases stops at the first match it finds.
static bool IsDuplicateInstall(int index)
{
	if (index < 0 || index >= (int)gInstalls.size())
	{
		return false;
	}

	for (unsigned int i = 0; i < gInstalls.size(); ++i)
	{
		if ((int)i != index && gInstalls[i].version == gInstalls[index].version)
		{
			return true;
		}
	}

	return false;
}

// True when this install is the one the selected version would start. Everything else in the list is a
// folder the menu cannot reach, which is the thing worth saying out loud on a screen like this one.
static bool IsInstallInUse(int index)
{
	const Release* rel = Selected();
	return (rel != 0) && (rel->installIndex == index);
}

// Keeps the highlighted install inside the list, and the list inside its window.
static void ClampManage()
{
	if (gInstalls.empty())
	{
		gManageSel = 0;
		gManageTop = 0;
		return;
	}

	if (gManageSel < 0)
	{
		gManageSel = 0;
	}
	if (gManageSel >= (int)gInstalls.size())
	{
		gManageSel = (int)gInstalls.size() - 1;
	}

	if (gManageSel < gManageTop)
	{
		gManageTop = gManageSel;
	}
	if (gManageSel >= gManageTop + MAN_ROWS)
	{
		gManageTop = gManageSel - MAN_ROWS + 1;
	}
	if (gManageTop < 0)
	{
		gManageTop = 0;
	}
}

// ---------------------------------------------------------------------------------------------
// The Manage menu's rows.
//
// Two actions, and only the first can be absent: Delete needs something to delete, and "the version the
// list is pointing at is installed" is the only thing that makes it applicable. Check for updates is
// always there, because it is the one action on this screen that does not depend on the card holding
// anything -- it is how a player with nothing installed reaches the server.
//
// Derived rather than stored, so the rows cannot describe a state the card is no longer in: delete an
// install and the Delete row is gone on the next frame, with nothing to keep in step.
// ---------------------------------------------------------------------------------------------
enum { MANAGE_DELETE = 0, MANAGE_CHECK };

static bool ManageHasDelete()
{
	const Release* rel = Selected();
	return (rel != 0) && (rel->installIndex >= 0);
}

static int ManageRowCount()
{
	return ManageHasDelete() ? 2 : 1;
}

// Which action the row at `row` performs. With no install to delete, row 0 IS the check -- so the menu
// never shows a disabled row and the cursor never sits on a thing that does nothing.
static int ManageActionAt(int row)
{
	return (ManageHasDelete() && row == 0) ? MANAGE_DELETE : MANAGE_CHECK;
}

static void ClampManageMenu()
{
	if (gManageMenuSel < 0)
	{
		gManageMenuSel = 0;
	}
	if (gManageMenuSel >= ManageRowCount())
	{
		gManageMenuSel = ManageRowCount() - 1;
	}
}

// Drawn instead of the menu when the Manage screen is up; defined below DrawMenu.
static void DrawManage(Screen* screen);

/*
 * The "Update Required" modal, drawn over whatever rung the player is standing on.
 *
 * It is drawn LAST, over the frame the rung has already composed, rather than instead of it. That is
 * the whole difference between this and the viewer: the viewer is a place and replaces the menu,
 * this is a question and the menu stays behind it, dimmed, so the player can still see where they
 * were and what the install they are being asked about is called.
 *
 * WHY A BOX AT ALL. The prompt line alone was the obvious first shape -- it is what every other
 * question in this interface is -- but a prompt with no boundary reads as part of the rung behind it,
 * and the one thing this has to say, that the install the player is looking at will not run, is not
 * part of any rung. So it gets a frame: the same 2 px border the gallery tiles use, which is the only
 * other border in the interface, so the two read as the same kind of thing.
 *
 * The body is opaque where the changelog's panel is translucent, and that is not an inconsistency.
 * The changelog holds a paragraph whose every line has to be legible for as long as it takes to read;
 * dirt moving behind thin glyphs is exactly what makes that fail. This holds two short lines and sits
 * still, and the dimmed menu behind it is the only thing on screen that says the modal is over
 * something rather than replacing it.
 *
 * `Dimensions` rather than 480 and 272, which is what it exists for -- it is the one place the
 * interface needs to know the screen it is drawing into rather than a rectangle of it.
 */

// Sized to hold its longest reason, "NN file(s) missing or incomplete", inside the frame rather than
// running into it, and fixed rather than a share of the width: a box that resized with its message
// would move under the player's eye for no gain.
#define DLG_W        300.0f
#define DLG_H        104.0f
#define DLG_CY       128.0f
#define DLG_T        2.0f

// The three baselines. Top and detail sit below the frame, and the prompt sits above the bottom of it,
// with the air between detail and prompt left for the eye to read the gap as "here are the answers".
#define DLG_TITLE_Y  (DLG_CY - DLG_H * 0.5f + 26.0f)
#define DLG_DETAIL_Y (DLG_CY - DLG_H * 0.5f + 47.0f)
#define DLG_PROMPT_Y (DLG_CY + DLG_H * 0.5f - 15.0f)

// The step between the two lines of a detail. Used by both dialogs below, which is why it lives here
// beside the box it has to fit inside rather than at either call site.
#define DLG_LINE_STEP 18.0f

/*
 * A detail, drawn as one line or two.
 *
 * A detail string may hold a '\n', and the two halves are drawn as a column rather than as one long
 * line. That is the fix for a real overflow: a dialog's width is fixed -- the box cannot grow to fit
 * its text -- so a long reason runs straight over its own frame instead of wrapping. The second half
 * is in practice the figure that made the line long (a size in KB or MB), which is also the part a
 * reader wants separated out: the sentence says WHAT is happening, the figure says what it costs.
 *
 * Only the first break is honoured. Two lines is what the layouts have room for (see DLG_DETAIL_Y and
 * DLG_PROMPT_Y: the gap between them is two steps and no more), and a detail that needed three would
 * be a detail that is too long to be a detail. Everything after the second break is drawn as part of
 * the second line rather than silently dropped.
 */
static void DrawDetail(Screen* screen, float cx, float y, const char* detail)
{
	if (detail == 0 || detail[0] == '\0')
	{
		return;
	}

	const char* brk = strchr(detail, '\n');
	if (brk == 0)
	{
		screen->Text(cx, y, detail);
		return;
	}

	std::string first(detail, (size_t)(brk - detail));
	screen->Text(cx, y, first.c_str());
	screen->Text(cx, y + DLG_LINE_STEP, brk + 1);
}

static void DrawUpdatePrompt(Screen* screen)
{
	int width = 0;
	int height = 0;
	Screen::Dimensions(&width, &height);

	// Dimmed rather than covered: the menu behind it is still the menu, and the dimming is what says
	// the modal is over something. The viewer dims by the same amount and for the same reason.
	screen->Panel(0.0f, 0.0f, (float)width, (float)height, 0xC0000000);

	const float x0 = ((float)width - DLG_W) * 0.5f;
	const float y0 = DLG_CY - DLG_H * 0.5f;

	screen->Panel(x0, y0, DLG_W, DLG_H, 0xE8000000);
	screen->Panel(x0, y0, DLG_W, DLG_T, 0xFFFFFFFF);
	screen->Panel(x0, y0 + DLG_H - DLG_T, DLG_W, DLG_T, 0xFFFFFFFF);
	screen->Panel(x0, y0 + DLG_T, DLG_T, DLG_H - DLG_T * 2.0f, 0xFFFFFFFF);
	screen->Panel(x0 + DLG_W - DLG_T, y0 + DLG_T, DLG_T, DLG_H - DLG_T * 2.0f, 0xFFFFFFFF);

	const float cx = (float)width * 0.5f;

	screen->Text(cx, DLG_TITLE_Y, "Update Required");

	if (!gUpdatePromptDetail.empty())
	{
		// One line or two; see DrawDetail. The boot check's "N file(s) differ from the release (KB)"
		// is the two-line case, and it is two lines because one ran out of the box.
		DrawDetail(screen, cx, DLG_DETAIL_Y, gUpdatePromptDetail.c_str());
	}

	// The answers, drawn as buttons rather than named -- the same convention as every other prompt
	// here, and the reason the icon sheet exists. See the note on gUpdatePromptAuto for why one of
	// these two shapes has an O in it and the other does not.
	if (gUpdatePromptAuto)
	{
		static const PromptPart updatePrompt[] = {
			{ 0, ICON_CROSS },
			{ "update", -1 },
			{ 0, ICON_CIRCLE, true },
			{ "not now", -1 },
		};
		PromptLine(screen, cx, DLG_PROMPT_Y, updatePrompt, 4);
	}
	else
	{
		static const PromptPart updatePrompt[] = {
			{ 0, ICON_CROSS },
			{ "update", -1 },
		};
		PromptLine(screen, cx, DLG_PROMPT_Y, updatePrompt, 2);
	}
}

static void DrawMenu()
{
	if (!Screen::Instance()->Ready())
	{
		DrawTextFallback();
		return;
	}

	Screen* screen = Screen::Instance();
	screen->BeginFrame();

	const Release* rel = Selected();

	// The viewer owns the whole screen, so it is drawn instead of the menu rather than on top of it.
	if (gLevel == LEVEL_FULLSCREEN)
	{
		DrawViewer(screen, rel);

		// The modal is drawn over every rung, including the two that own the whole screen. It is raised
		// by the scan rather than by a place, so it does not wait for the player to come back to the
		// button row -- a screenshot being stepped through is not a reason to leave a broken install
		// unreported. See the note on gUpdatePrompt.
		if (gUpdatePrompt)
		{
			DrawUpdatePrompt(screen);
		}

		screen->EndFrame();
		return;
	}

	// The Manage screen owns it too, and for the same reason: it is a place rather than a highlight, and
	// nothing of the menu behind it is meant to be read while it is up.
	if (gLevel == LEVEL_MANAGE)
	{
		DrawManage(screen);

		if (gUpdatePrompt)
		{
			DrawUpdatePrompt(screen);
		}

		screen->EndFrame();
		return;
	}

	// Clipped to the window so the screenshot can scroll under its top edge instead of over the dirt.
	// Released again before the bottom row, which is deliberately outside the window.
	screen->Scissor(TOP_X0, TOP_Y0, TOP_W, TOP_H);
	DrawTopWindow(screen, rel);
	screen->ScissorOff();

	// The bottom row.
	// The bottom row. The version is its own label now: it used to be printed with a trailing ASCII "v"
	// standing in for a drop-down arrow, which read as part of the version string rather than as a
	// control, and at this size the glyph sat low and to the right in a way a real arrow never would.
	char ddLabel[64];
	snprintf(ddLabel, sizeof(ddLabel), "%s", (rel != 0) ? rel->version.c_str() : "none");
	screen->ButtonW(DD_CX, ROW_Y, DD_W, ddLabel, gLevel == LEVEL_BUTTONS && gFocus == FOCUS_DROPDOWN);

	// The install state, over the button that chose the version. Drawn before the dropdown, which opens
	// upwards across this line and covers it while it is open -- which is right, because while choosing
	// a version the list is the thing being read.
	//
	// "Needs repair" is the third state, and it is not a shade of the other two: the install is here and
	// its folder is named, but files inside it are missing, so the thing to do about it is the same as
	// for a version that is not here at all. Saying "Installed" over a tree that will not boot is the
	// one answer that is actively misleading.
	const int integrity = SelectedIntegrity();
	if (rel != 0)
	{
		const char* state = "Not installed";
		if (rel->installIndex >= 0)
		{
			// Three states, not two, and the middle one is the reason. Until the walk answers, nothing is
			// known about the files -- and "Installed" was what this said while it waited, which made a
			// broken install look playable for exactly as long as the check took. On the card that was long
			// enough that the answer never arrived, so "Play" was the only thing the player ever saw.
			if (integrity == CHECK_BROKEN)
			{
				state = "Needs repair";
			}
			else if (integrity == CHECK_SCANNING)
			{
				state = "Checking...";
			}
			else
			{
				state = "Installed";
			}
		}
		screen->Text(DD_CX, STATUS_Y, state);
	}

	// The two buttons to the right are drawn dimmed when they cannot act, rather than being hidden: a
	// button that comes and goes makes the row move under the cursor.
	//
	// Focus and availability are passed separately. They used to be ANDed here, which is what made the
	// cursor vanish whenever it landed on a button that could not act -- see the note on ButtonW.
	//
	// Play counts as available whenever there is a version at all, because it does three things: it
	// plays an install, it fetches one that is missing, and it repairs one that is incomplete.
	//
	// It is labelled for whichever of the three it will actually do. "Play" over a version that is not on
	// the card was a promise the button could not keep -- pressing it downloads instead -- and it was
	// the one thing on the screen not saying so. The state line above already reads "Not installed", so
	// the two now agree: a version that is not here is marked as one to fetch, and the button says
	// "Update" rather than "Play".
	//
	// A broken install reuses that same label rather than getting one of its own. "Update" is what the
	// button does in both cases -- it makes the folder right from the server -- and the difference the
	// player cares about is already stated on the line above, in a word that could not be plainer.
	bool canPlay = (rel != 0);
	bool needsUpdate = (rel != 0) && (rel->installIndex < 0 || integrity == CHECK_BROKEN);
	const char* playLabel = needsUpdate ? "Update" : "Play";
	screen->ButtonW(PLAY_CX, ROW_Y, PLAY_W, playLabel, gLevel == LEVEL_BUTTONS && gFocus == FOCUS_PLAY, canPlay);

	// Manage is about the card, and it is a LIST rather than a screen now: pressing it opens the menu the
	// way the version button opens its own, and the rows are the actions rather than the installs.
	//
	// ALWAYS LIT, where it used to go dark with nothing under Installs/. That rule was right when Delete
	// was the only thing behind it -- there is nothing to delete on an empty card -- and stops being right
	// once Check for updates is one of its rows, because "get me a version onto this empty card" is
	// exactly the case where that action matters most. The rows themselves handle applicability: with
	// nothing installed the Delete row is simply not built (see ManageRowCount).
	screen->ButtonW(MANAGE_CX, ROW_Y, MANAGE_W, "Manage", gLevel == LEVEL_BUTTONS && gFocus == FOCUS_MANAGE, true);

	/*
	 * Say the decision out loud, once, when it changes.
	 *
	 * This is the one line of the interface that no log could otherwise describe: everything above is
	 * provable from the files and the record, but "which word ended up on the button" is a fact about a
	 * frame, and a frame is exactly what cannot be read back on this hardware -- the frame dumper was
	 * measured to return the program's first frame no matter when it is called (see DumpFrame), and a
	 * screenshot of the emulator window is at the mercy of what is on top of it. So the label reports
	 * itself. Once per change rather than per frame, because at 60 Hz that is 60 identical lines a second.
	 *
	 * It is a diagnostic worth keeping rather than test scaffolding: "the button said Play over a tree
	 * with files missing" is a bug report this answers directly.
	 */
	static int loggedLabel = -1;
	int labelId = needsUpdate ? 1 : (integrity == CHECK_SCANNING ? 2 : 0);
	if (labelId != loggedLabel)
	{
		loggedLabel = labelId;

		if (labelId == 1 && rel != 0 && rel->installIndex < 0)
		{
			Logger::Instance()->LogMessage("launcher: button reads 'Update' -- %s is not installed\n",
				rel->version.c_str());
		}
		else if (labelId == 1)
		{
			Logger::Instance()->LogMessage("launcher: button reads 'Update' -- %s is incomplete\n",
				rel->version.c_str());
		}
		else if (labelId == 2 && rel != 0)
		{
			// Not "complete": the scan has not answered yet, and the button says Play in the meantime
			// rather than flickering through a third word. Saying which of the two it is matters here,
			// because otherwise this line reads as a verdict it has not actually reached -- and the
			// transition out of this state is logged too, so the log always ends at a real answer.
			Logger::Instance()->LogMessage(
				"launcher: button reads 'Play' -- still checking %s\n", rel->version.c_str());
		}
		else if (rel != 0)
		{
			Logger::Instance()->LogMessage("launcher: button reads 'Play' -- %s is complete\n",
				rel->version.c_str());
		}
	}

	// The opened dropdown, drawn BEFORE the row of buttons so the row sits on top of it and the list
	// reads as coming out from behind them.
	if (gDropdownOpen && !gReleases.empty())
	{
		int rows = (int)gReleases.size() - gVersionTop;
		if (rows > DD_ROWS)
		{
			rows = DD_ROWS;
		}

		for (int r = 0; r < rows; ++r)
		{
			int index = gVersionTop + r;

			// Growing upwards from the button: the last row in the list is the one nearest the button,
			// so the list extends away from the control that opened it.
			float y = ROW_Y - DD_PITCH * (float)(rows - r);

			const Release* item = &gReleases[index];
			// The version, with its state, because the whole point of the list is to choose between
			// versions you may or may not already have.
			char label[64];
			snprintf(label, sizeof(label), "%s%s", item->version.c_str(),
				(item->installIndex >= 0) ? "  *" : "");

			screen->ButtonW(DD_CX, y, DD_W - (float)r, label, index == gVersionSel);
		}
	}

	// The Manage menu, drawn the same way and for the same reason as the version list above: out of its
	// own button, growing upward, one row per action. Height instead of a scissor window because there
	// are never more than two rows -- if this ever grows past the space above the button it will need the
	// same top/clamp treatment the version list has.
	if (gManageOpen)
	{
		int rows = ManageRowCount();

		for (int r = 0; r < rows; ++r)
		{
			float y = ROW_Y - MM_PITCH * (float)(rows - r);
			const char* label = (ManageActionAt(r) == MANAGE_DELETE) ? "Delete" : "Check for updates";

			screen->ButtonW(MANAGE_CX, y, MM_W, label, r == gManageMenuSel);
		}
	}

	if (!gNotice.empty())
	{
		/* Right-aligned to the top window's edge rather than centred on the screen, because this row is
		 * SHARED with the status line over the version button.
		 *
		 * Centred at 240, the longest notice this screen can post -- "42 file(s) missing or incomplete",
		 * which is what the counts add up to on the card -- is 252 px of ink and reaches back to x=113.
		 * "Needs repair" is centred on DD_CX and ends at x=133, so the two overlapped by 21 px. Measured
		 * off the emulator's framebuffer rather than deduced from the font metrics: the glyphs were
		 * interleaved, one text's letter sitting in the other's gap.
		 *
		 * Anchored to the right edge, the same string occupies x=220..472 and cannot reach the status
		 * whatever either of them says -- which is the point, because both strings vary: the status is
		 * one of four words and the notice one of several sentences. */
		screen->TextHighlightRight(TOP_X0 + TOP_W, 226.0f, gNotice.c_str());
	}

	// The last thing drawn, because it is over the rung rather than part of it.
	if (gUpdatePrompt)
	{
		DrawUpdatePrompt(screen);
	}

	screen->EndFrame();
}

/*
 * The Manage screen: every folder under Installs/, whether or not the version list can reach it.
 *
 * WHY THIS REPLACED A DELETE BUTTON. Delete acted on the release the version list pointed at, which is
 * one install out of however many are on the card -- and the version list can name only one install per
 * version, because LoadReleases stops at the first folder that matches. A second copy of a version is
 * therefore not merely awkward to reach from the menu, it is unreachable: a folder copied on by hand, an
 * install interrupted halfway, and two installs left behind by a repair that went wrong all land in that
 * blind spot, and the only tool that could see them was a file manager on a PC.
 *
 * So this lists what is IN Installs/ rather than what is on offer, which is a different list and the whole
 * point of it: every row is a real folder that can be deleted, including the ones nothing else can name.
 * Duplicates are labelled, and so is the copy the menu is currently using, because "which of these two is
 * the one it plays" is the question two identical rows raise and the one this screen has to answer.
 */
static void DrawManage(Screen* screen)
{
	screen->Text(240.0f, 18.0f, "Manage installs");

	if (gInstalls.empty())
	{
		screen->Text(240.0f, 122.0f, "There are no installs on the card.");
	}
	else
	{
		int rows = (int)gInstalls.size() - gManageTop;
		if (rows > MAN_ROWS)
		{
			rows = MAN_ROWS;
		}

		for (int r = 0; r < rows; ++r)
		{
			const int index = gManageTop + r;
			const Install& inst = gInstalls[index];

			// The folder is what this screen lists, so the folder is what a row leads with. The version
			// follows in brackets only when it disagrees with the folder's name -- which is precisely when
			// the row would otherwise be puzzling, because it is the VERSION that the list above matched on.
			std::string base = BaseName(inst.folder);
			char label[192];
			if (inst.version.empty() || inst.version == base)
			{
				snprintf(label, sizeof(label), "%s", base.c_str());
			}
			else
			{
				snprintf(label, sizeof(label), "%s  [%s]", base.c_str(), inst.version.c_str());
			}

			std::string text = label;
			if (IsInstallInUse(index))
			{
				text += "  * playing";
			}
			if (IsDuplicateInstall(index))
			{
				text += "  duplicate";
			}

			screen->ButtonW(MAN_CX, MAN_Y0 + MAN_PITCH * (float)r, MAN_W, text.c_str(),
				index == gManageSel);
		}
	}

	// What the highlighted row is, in a sentence. This is where "duplicate" stops being a word on a row
	// and becomes an explanation: the reason a folder is missing from the version list is that another
	// copy of the same version got there first, and it is that other copy the menu would start.
	if (!gInstalls.empty() && gManageSel >= 0 && gManageSel < (int)gInstalls.size())
	{
		const Install& sel = gInstalls[gManageSel];
		char info[256];

		if (IsInstallInUse(gManageSel))
		{
			snprintf(info, sizeof(info), "%s, code %d -- the copy the menu uses.",
				sel.version.c_str(), sel.versionCode);
		}
		else if (IsDuplicateInstall(gManageSel))
		{
			snprintf(info, sizeof(info),
				"%s, code %d -- another copy of this version is the one in use.",
				sel.version.c_str(), sel.versionCode);
		}
		else
		{
			snprintf(info, sizeof(info), "%s, code %d -- no version in the list offers this.",
				sel.version.c_str(), sel.versionCode);
		}

		screen->TextHighlight(240.0f, 218.0f, info);
	}

	if (!gNotice.empty())
	{
		screen->TextHighlight(240.0f, 238.0f, gNotice.c_str());
	}

	// "Delete" rather than "manage": the highlighted row already IS the selection, so the one thing X can
	// do to it here is remove it, and naming the action is more use than naming the screen the player is
	// already standing on.
	static const PromptPart managePrompt[] = {
		{ 0, ICON_CROSS },
		{ "delete", -1 },
		{ 0, ICON_CIRCLE, true },
		{ "back", -1 },
	};

	PromptLine(screen, 240.0f, 258.0f, managePrompt, 4);
}

// ---------------------------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------------------------

bool ShowMessage(const char* line, const char* detail, bool question)
{
	// The detail may carry a '\n' for its second line (see DrawDetail). Written straight to the log it
	// would split one event across two lines, which is worse than untidy: the log is read by eye after a
	// crash and grepped by phase, and a broken line makes the order of two events ambiguous. Flattened to
	// a space here, so what the log shows is the sentence with the figure after it.
	std::string flatDetail = detail ? detail : "";
	for (unsigned int i = 0; i < (unsigned int)flatDetail.size(); ++i)
	{
		if (flatDetail[i] == '\n')
		{
			flatDetail[i] = ' ';
		}
	}

	Logger::Instance()->LogMessage("launcher: message '%s' / '%s' (question %d)\n",
		line ? line : "", flatDetail.c_str(), question ? 1 : 0);

	// This function has crashed on hardware once and the log could not say where inside it. The two
	// phases split the three things it does that fail differently -- reading the pad, drawing a
	// frame, and polling for the answer -- so the next occurrence names the one that stopped.
	LauncherPhase("message: settling input");
	SettleInput();
	LauncherPhase("message: waiting for a key");

	// One phase more, set after the first frame is actually on screen. A crash in the first draw
	// leaves "waiting for a key" behind and a crash later leaves this, so the pair separates the
	// first frame from everything after it without writing the card once per frame.
	bool drewOnce = false;

	for (;;)
	{
		if (Screen::Instance()->Ready())
		{
			Screen* screen = Screen::Instance();
			screen->BeginFrame();
			screen->Text(240.0f, 96.0f, line);
			// One line or two; see DrawDetail. The update confirm uses the second line for the size, so
			// the question stays short enough to read at a glance.
			DrawDetail(screen, 240.0f, 126.0f, detail);

			// The answer, as buttons. A question offers both; a plain "press to continue" offers one.
			if (question)
			{
				static const PromptPart yesNo[] = {
					{ 0, ICON_CROSS },
					{ "yes", -1 },
					{ 0, ICON_CIRCLE, true },
					{ "no", -1 },
				};
				PromptLine(screen, 240.0f, 190.0f, yesNo, 4);
			}
			else
			{
				static const PromptPart anyKey[] = {
					{ "Press", -1 },
					{ 0, ICON_CROSS },
				};
				PromptLine(screen, 240.0f, 190.0f, anyKey, 2);
			}
			screen->EndFrame();

			if (!drewOnce)
			{
				drewOnce = true;
				LauncherPhase("message: first frame drawn");
			}
		}
		else
		{
			pspDebugScreenClear();
			pspDebugScreenSetXY(0, 0);
			pspDebugScreenPrintf("%s\n\n%s\n\n%s\n",
				line ? line : "", detail ? detail : "",
				question ? "X yes   O no" : "Press X");
		}

		unsigned int pressed = PollPressed();

		if (pressed & PSP_CTRL_CROSS)
		{
			LauncherPhase("message: dismissed");
			return true;
		}
		if (question && (pressed & PSP_CTRL_CIRCLE))
		{
			LauncherPhase("message: dismissed");
			return false;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------------------------

/*
 * Start the selected install.
 *
 * The single hop the launcher exists to make. Nothing needs a handoff file the way the old in-game
 * updater did -- but the game still needs to be told where it is, because loadexec does not preserve a
 * working directory (measured, not assumed: see Scripts/LaunchDir.h). So the launch file is written
 * first and consumed by the game's first constructor.
 *
 * Update::LaunchExecutable DOES NOT RETURN ON SUCCESS -- loadexec resets the console. A return is
 * therefore always a failure, which is why the error path below is unconditional rather than a return
 * value check with a success branch beside it.
 */
static void DoPlay(const Install& inst)
{
	std::string root = LauncherRootPath();
	if (root.empty())
	{
		ShowMessage("Cannot find the launcher folder", "There is no working directory.", false);
		return;
	}

	std::string installDir = Join(root, inst.folder);

	// The game keeps the name it was built with. It is not the file the XMB runs -- the XMB only scans
	// PSP/GAME/<one folder>/, so an EBOOT.PBP two levels down is invisible to it -- and keeping it means
	// a package's manifest paths (which are relative to a game folder and name EBOOT.PBP, because that
	// is what the packaging script has always produced) map onto an install with no renaming step in
	// between. A rename here would be one more thing that can disagree with the manifest.
	std::string gamePath = Join(installDir, Update::ExecutableName());

	if (!FileExists(gamePath))
	{
		ShowMessage("That install has no game file", gamePath.c_str(), false);
		return;
	}

	// The preview image is dropped before the launch, because a megabyte of texture is a megabyte the
	// incoming program cannot have: loadexec has to load a ~4.9 MB executable into this partition before
	// it resets, so the launcher's largest single allocation is worth handing back at the last moment it
	// can be.
	//
	// It is NOT free, and an earlier version of this comment claimed it was ("the launcher is done with
	// that texture either way"). That is true of a launch that SUCCEEDS, which never returns -- and
	// false of one that FAILS, which comes straight back here with the launcher still running and the
	// texture gone. Every drawing path asks HasStrip()/HasPanelImage(), so releasing it without putting
	// it back leaves the screenshots missing for the rest of the session. Both failure paths below
	// restore it with the same call SelectRelease uses.
	FreeImages();

	Logger::Instance()->LogMessage("launcher: playing '%s'\n", gamePath.c_str());

	int written = MC_WriteLaunchDir(installDir.c_str());
	if (written != 0)
	{
		char why[64];
		snprintf(why, sizeof(why), "Could not write the launch file (0x%08X).", (unsigned int)written);
		RefreshImages();					// released for a launch that is not going to happen
		ShowMessage("Launch cancelled", why, false);
		return;
	}

	std::string error;
	if (!Update::LaunchExecutable(gamePath, &error))
	{
		// We are still here, so the launch did not happen. The handoff must not survive: it would point
		// the next run -- including one started from the XMB -- at this install regardless of what the
		// player picks then.
		MC_ClearLaunchDir();

		Logger::Instance()->LogMessage("launcher: launch failed: %s\n", error.c_str());

		// Put the preview back before reporting, not after. The message is drawn over whatever is on
		// screen, so restoring first means the menu behind it is already correct however long the player
		// leaves the message up -- and there is no path where restoring late would be better.
		RefreshImages();
		ShowMessage("Could not start the game", error.c_str(), false);
	}
}

/*
 * Enters the Manage screen.
 *
 * This is where the old menu-level Delete went. It could only ever act on the install the version list
 * pointed at, and the version list names one install per version, so every other folder under Installs/
 * -- duplicates, hand-copied ones, half-installed ones -- was beyond it. ManageDelete below is the same
 * removal, aimed by the row the player chose rather than by the selection behind them.
 *
 * The list it shows is the one the menu already scanned rather than a fresh walk of the card, and that is
 * not laziness. gReleases holds INDICES into gInstalls, so re-scanning here without rebuilding both would
 * leave those indices describing the wrong folders -- and rebuilding both means reloading the version's
 * screenshots, which this screen has no use for. The list is therefore exactly as fresh as the menu's, and
 * every path that can change it -- an update, a delete -- reloads it anyway.
 */
static void EnterManage()
{
	gNotice.clear();
	ClampManage();
	gLevel = LEVEL_MANAGE;

	Logger::Instance()->LogMessage("launcher: managing %d install(s)\n", (int)gInstalls.size());
}

/*
 * Deletes one install, after asking.
 *
 * Named by FOLDER in the prompt rather than by version, which is the one place this differs from the
 * Delete button it replaced. Two installs can carry the same version -- that is the case the Manage
 * screen exists for -- so "Delete install 3.0.1?" would not say which of the two highlighted rows was about to
 * go, and the player would be answering a question with two possible subjects.
 *
 * Takes the INDEX rather than reading gManageSel, because there are two ways in now: the Manage screen's
 * highlighted row, and the Manage menu's Delete row acting on the version the list is pointing at. Both
 * mean "that folder", and only the caller knows which.
 */
static void DeleteInstall(int index)
{
	if (index < 0 || index >= (int)gInstalls.size())
	{
		return;
	}

	const Install inst = gInstalls[index];
	const std::string base = BaseName(inst.folder);

	std::string root = LauncherRootPath();
	if (root.empty())
	{
		ShowMessage("Cannot find the launcher folder", "There is no working directory.", false);
		return;
	}

	// Released before the confirmation rather than after, for the same reason DoPlay does it: removing a
	// tree is the case most likely to be short of contiguous heap, and the preview is the launcher's only
	// large allocation.
	FreeImages();

	char detail[224];
	snprintf(detail, sizeof(detail), "Version %s, code %d. The whole folder is removed from the card.",
		inst.version.c_str(), inst.versionCode);

	std::string question = "Delete installation " + base + "?";

	if (!ShowMessage(question.c_str(), detail, true))
	{
		SelectRelease(gVersionSel);		// brings the preview back, which FreeImages took
		return;
	}

	unsigned int removed = 0;
	bool ok = RemoveTree(Join(root, inst.folder), &removed);
	SyncCard();

	Logger::Instance()->LogMessage("launcher: deleted install '%s', %u file(s), ok=%d\n",
		inst.folder.c_str(), removed, ok ? 1 : 0);

	// Reloaded through the same path an update takes, because removing an install changes the mapping from
	// versions to folders as well as the list: the version this folder was backing may now be backed by
	// its duplicate instead, or by nothing at all.
	LoadReleases();
	ClampManage();

	// The version list is rebuilt by that call too, and the selection has to be re-resolved against it --
	// the row the player was on may now point at a different install, or at none. Done here rather than in
	// the callers so both ways in -- the Manage screen and the Manage menu's Delete row -- leave the menu
	// behind them consistent.
	SelectRelease(gVersionSel);
	ClampDropdown();

	if (!ok)
	{
		gNotice = "The install could not be removed completely.";
		return;
	}

	gNotice = "Deleted " + base + ".";
}

// The Manage SCREEN's Delete, which acts on the row the player highlighted there. The Manage MENU's
// Delete row does not come through here: it acts on the version the list is pointing at, which is a
// different folder whenever the screen was scrolled somewhere else.
static void ManageDelete()
{
	DeleteInstall(gManageSel);
}

/*
 * Fetch and install.
 *
 * The base a patch package is applied to is the selected version when it is already on the card, and
 * nothing otherwise. That is not a shortcut around the patch format -- it is what makes installing a
 * version the player does not have work at all: with no base there is nothing to patch, and the update
 * flow falls back to a full package on its own, from the manifest.
 *
 * It is also the repair path. When the selected version is installed but incomplete, the base is that
 * install's own folder, the destination works out to the same folder, and the flow applies the package
 * over itself in place -- writing only the files that are actually wrong or gone, because the installer
 * checks each entry's size and crc before it rewrites it. So the player presses Update on a tree that is
 * missing two files and gets those two files, not a fresh 135 MB install.
 */
static void DoUpdate(bool repair)
{
	LauncherPhase("do-update: pressed");

	std::string base;
	const Release* rel = Selected();
	if (rel != 0 && rel->installIndex >= 0)
	{
		base = gInstalls[rel->installIndex].folder;
	}

	gNotice.clear();

	// The preview is a megabyte of texture, and the unzip path allocates a whole entry at a time --
	// about five megabytes at its peak. Releasing here is the difference between an install that fits
	// in the heap and one that fails to allocate, and it costs nothing but a reload afterwards.
	FreeImages();

	LauncherRunUpdate(base, repair);

	// Named separately from the update itself: the flow's own phases stop at "finished", so without
	// this a crash on the way out of LauncherRunUpdate -- in its epilogue or in the rescan below --
	// would still read as a crash during the update.
	LauncherPhase("do-update: flow returned");

	// Rescanned rather than patched in place: the update flow can create an install, change the
	// selected one's contents, or do neither, and asking the card is the only way to be sure which.
	Logger::Instance()->LogMessage("launcher: update finished, rescanning\n");
	RefreshCatalogFromServer();
	LoadReleases();
	SelectRelease(gVersionSel);
	ClampDropdown();
	LauncherPhase("do-update: done");
}

// ---------------------------------------------------------------------------------------------
// The loop
// ---------------------------------------------------------------------------------------------

static void Activate()
{
	const Release* rel = Selected();

	switch (gFocus)
	{
		case FOCUS_DROPDOWN:
			// Opening and closing on the same button, which is what makes it a dropdown rather than a
			// mode. With nothing to choose between there is nothing to open.
			gDropdownOpen = !gDropdownOpen;
			if (gDropdownOpen)
			{
				gManageOpen = false;		// only one list at a time; two would overlap and neither is modal
				ClampDropdown();
			}
			break;

		case FOCUS_PLAY:
			if (rel == 0)
			{
				gNotice = "There is no version to play.";
			}
			else if (rel->installIndex >= 0 && SelectedIntegrity() == CHECK_SCANNING)
			{
				// The files have not been read yet, so there is no answer to act on -- and the one outcome
				// worth refusing is the one that cannot be taken back. A game whose assets may be missing
				// ends with the loader having already reset the console, and there is no way back to this
				// menu from a game that dies there. The wait is the cheaper of the two mistakes.
				gNotice = "Still checking this install.";
			}
			else if (rel->installIndex < 0)
			{
				// Not installed, so this X is the download. That is the only thing the button could
				// sensibly do here, and the notice that used to stand in its place was telling the
				// player to press a key that does not exist on a PSP -- the update is on Triangle and
				// the message said "U". Rather than relabel the notice, the button now does it.
				DoUpdate(false);
			}
			else if (SelectedIntegrity() == CHECK_BROKEN)
			{
				// Installed and incomplete, so this X is the repair -- and the button says so, which is
				// what keeps the press and the label the same promise. Note that Play is NOT offered
				// here even though a missing file might not matter: a game that dies on a missing asset
				// after the loader has already reset the console is a worse outcome than a wait, because
				// the player cannot get back to this menu from the game.
				DoUpdate(true);
			}
			else
			{
				DoPlay(gInstalls[rel->installIndex]);
			}
			break;

		case FOCUS_MANAGE:
			// A menu now, opened and closed on its own button, exactly like the version list -- and with
			// the same guarantee behind it: there is always at least one row, because Check for updates
			// does not depend on the card holding anything. So the button never has nothing behind it,
			// which is what the screen it replaced existed to say.
			gManageOpen = !gManageOpen;
			if (gManageOpen)
			{
				gDropdownOpen = false;		// only one list at a time
				ClampManageMenu();
			}

			// Logged because the menu is otherwise invisible to a test: opening it changes no file and
			// prints nothing, so "the button opened it" and "the button did nothing" look identical from
			// outside. The row count goes with it, because that is what the selection decides.
			Logger::Instance()->LogMessage("launcher: Manage menu %s, %d row(s), %d install(s)\n",
				gManageOpen ? "open" : "closed", ManageRowCount(), (int)gInstalls.size());
			break;
	}
}

// ---------------------------------------------------------------------------------------------
// Boot networking
// ---------------------------------------------------------------------------------------------
//
// The menu now brings the network up itself at boot, which is the opposite of what the note over the
// "Check for updates" row argues for -- and it is that note's own point: a launcher that only learns
// about a release when the player presses a key cannot tell the player about a release. What keeps
// both honest is that this path is SILENT. It never shows the system dialog and never waits on the
// player's answer, so the worst an offline card pays is the second or two an association takes to
// fail, and the dialog is still behind the button for actually choosing a connection.
//
// The one thing it writes is which saved connection worked, so the next boot tries that first.

#define NET_PROFILE_FILE "net_profile.txt"

// Display list for the system network dialog at boot. Same reason LauncherUpdate has its own: Screen
// does not publish the list it is drawing with, and the dialog has to clear on a list that is not
// already in use.
static unsigned int sBootDialogList[16] __attribute__((aligned(64)));

static bool BootDialogClear(void* ctx)
{
	(void)ctx;
	sceGuStart(GU_DIRECT, sBootDialogList);
	sceGuClearColor(0xFF000000);
	sceGuClearDepth(0);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuFinish();
	sceGuSync(0, 0);
	return true;
}

static bool BootDialogPresent(void* ctx)
{
	(void)ctx;
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
	return true;
}

static bool AskForNetwork()
{
	Logger::Instance()->LogMessage("launcher: boot connect -- asking the player to pick a network\n");
	if (Screen::Instance()->Ready())
	{
		Screen::Instance()->Frame("Choose a network...", 0, 0, -1.0f);
	}

	if (Update::NetConnectDialog(BootDialogClear, BootDialogPresent, 0) && Update::NetIsConnected())
	{
		Logger::Instance()->LogMessage("launcher: boot connect -- online via the network dialog\n");
		return true;
	}

	Logger::Instance()->LogMessage("launcher: boot connect -- no network was chosen\n");
	return false;
}

static void RefreshCatalogFromServer()
{
	if (!Update::NetIsConnected())
	{
		return;
	}

	if (Screen::Instance()->Ready())
	{
		Screen::Instance()->Frame("Checking for versions...", 0, 0, -1.0f);
	}

	if (!LauncherFetchReleaseList())
	{
		return;
	}

	LoadReleases();
	SelectRelease(0);
	ClampDropdown();
}

// Whether the boot attempt got an address. Read by the update offer, which is only worth making
// when there is something to make it over.
static bool gBootOnline = false;

// How long one saved connection gets to associate before the next is tried. A known access point links
// in a few seconds; this is a ceiling for the pathological case, not the expected cost.
static const unsigned int kBootConnectTimeoutMs = 9000;

// How many saved connections the boot will try.
//
// This is the ONLY bound on how long the boot connect can take, and that is the point: see the note
// below on the budget that used to sit beside it. Each candidate is bounded by kBootConnectTimeoutMs
// plus at most one reachability probe, so the total is bounded without a second, independently chosen
// number to disagree with.
static const int kBootConnectMaxProfiles = 3;

/*
 * THERE IS NO WALL-CLOCK BUDGET HERE ANY MORE, and having one cost a hardware session.
 *
 * The old pair was 12 s per connection and 25 s overall, which allows exactly two attempts. On a console
 * holding three saved connections the third was therefore never tried -- and the third was the one that
 * works. The card's log reads `trying 4 ('SpectrumSetup-8')`, `trying 5 ('PlayStation(R)3')`, both
 * joining and both answering nothing, then `out of time after 2 profile(s)`, with the connection that
 * reaches the update server sitting one place further down the list. Two bounds that can disagree will
 * eventually disagree; the count above is the bound that matches what the feature promises, and every
 * candidate now gets its turn.
 *
 * The wall-clock worry the budget existed for -- ten saved networks at 12 s each being two minutes of a
 * menu that has not started -- is answered by the count cap instead, and further by the subnet check in
 * the loop, which retires a wrong network in the time it takes to join it rather than in the fifteen
 * seconds its HTTP connect timeout would have taken.
 */

// The saved connection that last produced an address, as "index|name".
//
// Both halves are kept because the index alone is not safe across boots: the console numbers its
// connections, and a reorder in the settings would silently point the remembered index at a
// different network. The name is what the caller compares before trusting it.
static int LoadRememberedProfile(std::string* name)
{
	name->clear();

	std::string text;
	if (!ReadTextFile(Join(LauncherRootPath(), NET_PROFILE_FILE), &text))
	{
		return 0;
	}

	size_t bar = text.find('|');
	if (bar == std::string::npos)
	{
		return 0;
	}

	int index = atoi(text.substr(0, bar).c_str());

	*name = text.substr(bar + 1);
	while (!name->empty() &&
		   (name->at(name->size() - 1) == '\n' || name->at(name->size() - 1) == '\r'))
	{
		name->erase(name->size() - 1);
	}

	return index;
}

static void SaveRememberedProfile(int index, const std::string& name)
{
	char head[16];
	snprintf(head, sizeof(head), "%d", index);

	WriteTextFile(Join(LauncherRootPath(), NET_PROFILE_FILE), std::string(head) + "|" + name + "\n");
}

// Paints the "connecting" frame and lets the player give up on it. A progress of -1 is the
// indeterminate bar -- the same thing the update flow shows while it waits on the network -- and
// Circle-to-skip is the same cancel the rest of the flow honours.
static bool BootConnectPump(void* ctx)
{
	SceCtrlData pad;
	memset(&pad, 0, sizeof(pad));
	sceCtrlReadBufferPositive(&pad, 1);
	if (pad.Buttons & PSP_CTRL_CIRCLE)
	{
		return false;
	}

	if (Screen::Instance()->Ready())
	{
		Screen::Instance()->Frame("Connecting...", (const char*)ctx, "O to skip", -1.0f);
	}

	return true;
}

// Tries the console's saved connections with no dialog, remembering the one that works.
//
// Order: the remembered profile first, because that is the last known good and the common case, then
// every other saved connection. Trying them all is what keeps this useful on a console that roams
// between two networks -- giving up on the first failure would make the remembered one the only one
// it could ever use again.
static bool AutoConnectAtBoot()
{
	if (!Update::NetInit())
	{
		Logger::Instance()->LogMessage("launcher: net modules unavailable, no boot check\n");
		return false;
	}

	if (Update::NetIsConnected())
	{
		// The game's own NetworkManager may have left the stack up. NetInit is explicitly allowed to
		// be the second caller, and there is then nothing left to connect.
		Logger::Instance()->LogMessage("launcher: already online at boot\n");
		return true;
	}

	int count = Update::NetConnectionCount();
	if (count <= 0)
	{
		// Before giving up, say what the enumeration actually saw. "No saved connections" is true for an
		// empty console and wrong for a firmware that answers every id with an error, and only this line
		// distinguishes the two on the hardware where it happens.
		Update::NetLogConnections();
		Logger::Instance()->LogMessage("launcher: no saved connections, opening the network dialog\n");
		return AskForNetwork();
	}

	Logger::Instance()->LogMessage("launcher: %d saved connection(s) to try\n", count);

	std::string rememberedName;
	int remembered = LoadRememberedProfile(&rememberedName);

	// Every valid id, remembered one first. Built by ASKING each index rather than assuming 1..count:
	// the list is not guaranteed to start at 1, and an entry that was deleted leaves the ones after it
	// where they were. Assuming contiguity is what made this report "no saved connections" on a console
	// that had one -- see NetConnectionValid.
	int order[10];
	int orderCount = 0;

	if (remembered >= 1 && remembered <= 10 && Update::NetConnectionValid(remembered))
	{
		std::string current;
		if (Update::NetConnectionName(remembered, &current) && current == rememberedName)
		{
			order[orderCount++] = remembered;		// still the same network at that index
		}
		else
		{
			Logger::Instance()->LogMessage(
				"launcher: remembered connection %d is no longer '%s', ignoring it\n",
				remembered, rememberedName.c_str());
		}
	}

	for (int i = 1; i <= 10 && orderCount < 10; ++i)
	{
		if (!Update::NetConnectionValid(i))
		{
			continue;
		}
		if (orderCount > 0 && order[0] == i)
		{
			continue;								// already first in the list
		}
		order[orderCount++] = i;
	}

	if (orderCount == 0)
	{
		Logger::Instance()->LogMessage("launcher: no saved connections, no boot check\n");
		return false;
	}

	// Measured from just before the first attempt, so the timings below describe the radio work rather
	// than the profile enumeration above it.
	unsigned int bootStartedMs = sceKernelGetSystemTimeLow() / 1000;

	for (int n = 0; n < orderCount && n < kBootConnectMaxProfiles; ++n)
	{
		std::string name;
		Update::NetConnectionName(order[n], &name);

		char detail[128];
		snprintf(detail, sizeof(detail), "%s (connection %d)",
			name.empty() ? "saved connection" : name.c_str(), order[n]);

		unsigned int attemptMs = sceKernelGetSystemTimeLow() / 1000;

		Logger::Instance()->LogMessage("launcher: boot connect -- trying %d ('%s')\n",
			order[n], name.c_str());

		if (!Update::NetConnectSilent(order[n], BootConnectPump, detail, kBootConnectTimeoutMs))
		{
			// Leave nothing half-open for the next candidate to inherit.
			Logger::Instance()->LogMessage(
				"launcher: boot connect -- %d ('%s') did not come up after %d ms\n",
				order[n], name.c_str(), (int)((sceKernelGetSystemTimeLow() / 1000) - attemptMs));
			Update::NetDisconnect();
			continue;
		}

		unsigned int joinedMs = (sceKernelGetSystemTimeLow() / 1000) - attemptMs;

		std::string address;
		Update::NetCurrentAddress(&address);

		/*
		 * Two ways for a joined network to be the wrong one, and they are worth separating because one
		 * of them costs fifteen seconds to discover and the other costs nothing.
		 *
		 * 1. It is on a different network from the server. The update server is named by a literal
		 *    address on this LAN (see update_url.txt), so an address in a different /24 provably cannot
		 *    reach it -- there is no route between two home subnets. This is decided from the address
		 *    alone and costs nothing, and it is the common case: a console accumulates saved connections
		 *    that still associate (a phone hotspot, a router left in setup mode) but lead nowhere near
		 *    the server. Under the old budget each of those burned its full HTTP connect timeout and
		 *    then some, which is what starved the connection that works.
		 * 2. It is on the right network and the server is still not answering. Only this case is worth
		 *    a probe, and LauncherServerReachable below is the probe.
		 *
		 * Only with a fallback, on the same reasoning as the probe: with a single saved connection
		 * there is nothing to move on to, so refusing it would only turn a connection the update flow
		 * could have used into a boot that gave up early. One candidate keeps the old behaviour of
		 * letting the real fetch be the judge.
		 */
		if (orderCount > 1 && !LauncherAddressCouldReachServer(address))
		{
			Logger::Instance()->LogMessage(
				"launcher: boot connect -- %d ('%s') is on %s, not the server's network, trying the "
				"next after %d ms\n",
				order[n], name.c_str(), address.empty() ? "an unknown address" : address.c_str(),
				(int)joinedMs);

			Update::NetDisconnect();
			continue;
		}

		/*
		 * Joined is not the same as reached, and this is the check that tells them apart.
		 *
		 * Nothing in the apctl state machine can distinguish a network that works from one that
		 * associates and then goes nowhere, and a stale entry in the console's list is exactly the
		 * second kind: it hands out an address and answers nothing. Adopting the first connection that
		 * produced an IP is how the launcher ends up online, reporting the server unreachable, while a
		 * perfectly good connection sits unused further down the list.
		 *
		 * ONLY WHEN THERE IS SOMETHING TO CHOOSE BETWEEN. With a single saved connection there is no
		 * fallback, so the test cannot change the outcome -- it would only add a fetch (and, on a
		 * network that goes nowhere, its full set of timeouts) to a boot that is otherwise over. The
		 * update flow will say the server is unreachable soon enough, and it can say so to a player who
		 * is looking at the screen.
		 */
		if (orderCount > 1 && !LauncherServerReachable(BootConnectPump, detail))
		{
			Logger::Instance()->LogMessage(
				"launcher: boot connect -- %d ('%s') at %s joined but the server is unreachable, "
				"trying the next\n",
				order[n], name.c_str(), address.empty() ? "an unknown address" : address.c_str());

			Update::NetDisconnect();
			sceKernelDelayThread(300 * 1000);
			continue;
		}

		SaveRememberedProfile(order[n], name);
		Logger::Instance()->LogMessage(
			"launcher: boot connect -- online via %d ('%s') at %s after %d ms, remembered\n",
			order[n], name.c_str(), address.empty() ? "(no address)" : address.c_str(),
			(int)joinedMs);
		return true;
	}

	{
		int tried = (orderCount < kBootConnectMaxProfiles) ? orderCount : kBootConnectMaxProfiles;
		Logger::Instance()->LogMessage(
			"launcher: boot connect -- nothing came up after %d profile(s), %d ms total\n",
			tried, (int)((sceKernelGetSystemTimeLow() / 1000) - bootStartedMs));
	}

	// Silent tries exhausted. The system dialog is how the player picks a network the console has not
	// remembered, or one the silent walk could not join. Without it, Check for updates was the only
	// way onto Wi-Fi, and the version list stayed stale until then.
	return AskForNetwork();
}

// Asks the server whether the release it offers differs from what is on the card, and raises the same
// modal the integrity scan uses when it does.
//
// This is the whole reason for connecting at boot. The scan can only compare an install against its
// own record, so a card that is complete but a build behind reads as healthy to it -- and the release
// that would say otherwise was never fetched, because fetching it was behind a button. Nothing is
// downloaded here: the probe reads update.txt and the archive's index and stops, so the cost is a
// fraction of a megabyte and the player still decides whether to spend the wait.
static void BootUpdateCheck()
{
	if (!gBootOnline || gUpdatePrompt)
	{
		// Offline, or the integrity scan has already raised the modal for a real fault. That prompt is
		// the more urgent of the two, and two modals cannot share the screen.
		return;
	}

	const Release* rel = Selected();
	if (rel == 0 || rel->installIndex < 0)
	{
		// Nothing installed at the selected version, so there is no "this file changed" to find. A
		// release for a version that is absent is the button's business, not the boot's.
		return;
	}

	LauncherPhase("boot: probing the release");
	std::string version;
	unsigned int fileCount = 0;
	double kb = 0.0;
	unsigned int archiveSize = 0;

	if (LauncherProbeRelease(&version, &fileCount, &kb, &archiveSize) != 1)
	{
		return;
	}

	char detail[160];

	// Two lines: the count, then the size. One line ran out of the dialog's fixed width -- see
	// DrawDetail -- and the size is the part worth reading on its own anyway. "differs from the
	// release" rather than "differ" because the count decides the verb and one file is the common case.
	if (fileCount == 1)
	{
		snprintf(detail, sizeof(detail), "1 file differs from the release\n(%.0f KB)", kb);
	}
	else
	{
		snprintf(detail, sizeof(detail), "%u file(s) differ from the release\n(%.0f KB)", fileCount, kb);
	}

	gUpdatePromptDetail = detail;
	gUpdatePromptVersion = version;
	gUpdatePromptAuto = true;
	gUpdatePromptRepair = false;					// an update, not a repair; see gUpdatePromptRepair
	gUpdatePrompt = true;

	Logger::Instance()->LogMessage("launcher: boot check raised the prompt for v%s\n", version.c_str());
}

void LauncherMain()
{
	LauncherPhase("boot: menu start");

	Logger::Instance()->LogMessage("launcher: entering the menu\n");

	LoadReleases();
	SelectRelease(gVersionSel);
	ClampDropdown();

	// The cursor starts on the Play/Update button whenever there is a version, and only falls back to
	// the dropdown when there is nothing to act on at all. It used to start on the dropdown for any
	// version that was not installed, on the grounds that the dropdown was "the only control that does
	// anything" -- which stopped being true once the button started fetching a missing version instead
	// of just refusing. With one version offered and nothing installed, the single thing the player
	// came here to do is the button, so that is where the cursor belongs.
	const Release* first = Selected();
	gFocus = (first != 0) ? FOCUS_PLAY : FOCUS_DROPDOWN;

	/* The frame dumps read the frame the program started on, and nothing later -- see the note on
	 * DumpFrame. That was measured, twice: the display API returned byte-identical pictures for four
	 * dumps spread across frames 30 to 400, and reading VRAM directly instead returns byte-identical
	 * pictures for ten, across every state this menu can be talked into. So a state entered later in the
	 * run -- the dropdown, the changelog rung, the fullscreen viewer -- appears in none of them, and the
	 * only way to photograph a screen is to START on it.
	 *
	 * The switch file therefore carries the answer rather than just being present: its contents name the
	 * screen to begin on. Absent or unrecognised keeps the old default of the changelog rung, which is
	 * the state that shows the focus band and so is the most informative single frame. */
	if (Screen::Instance()->DumpingEnabled())
	{
		std::string mode;
		ReadTextFile(Screen::DumpSwitchName(), &mode);

		if (mode.find("view") != std::string::npos)
		{
			gLevel = LEVEL_FULLSCREEN;
			RefreshImages();				// the viewer wants the shot at the screen's own scale
		}
		else if (mode.find("shot") != std::string::npos)
		{
			gLevel = LEVEL_SHOT;
		}
		else if (mode.find("scroll") != std::string::npos)
		{
			gLevel = LEVEL_NOTE_SCROLL;
		}
		else if (mode.find("drop") != std::string::npos)
		{
			gLevel = LEVEL_BUTTONS;
			gDropdownOpen = true;
		}
		else if (mode.find("mmenu") != std::string::npos)
		{
			// The Manage menu, which has to be opened rather than selected for the same reason the
			// version list does: its rows depend on the selection, so the clamp has to run.
			gLevel = LEVEL_BUTTONS;
			gManageOpen = true;
			ClampManageMenu();
		}
		else if (mode.find("manage") != std::string::npos)
		{
			// Through the same entry point the button used to take. Nothing in play reaches this screen
			// any more -- the Manage button opens a menu instead -- so the frame dumper is the only caller
			// left, and going around EnterManage() here would leave the one code path nobody exercises.
			EnterManage();
		}
		else if (mode.find("prompt") != std::string::npos)
		{
			// The boot check's modal, seeded with the two-line detail a real differ produces, so the
			// frame dumper can photograph the layout the card cannot be asked about. The numbers are the
			// ones a 22-file repair prints; the point of the mode is the SECOND line, so the dump shows
			// whether "(1873 KB)" sits inside the frame or runs over it. Nothing else in the run reaches
			// this state without a server to differ against.
			gLevel = LEVEL_BUTTONS;
			gUpdatePromptDetail = "22 file(s) differ from the release\n(1873 KB)";
			gUpdatePromptVersion = "3.0.1";
			gUpdatePromptAuto = true;
			gUpdatePromptRepair = false;
			gUpdatePrompt = true;
		}
		else
		{
			gLevel = LEVEL_CHANGELOG;
		}

		Logger::Instance()->LogMessage("launcher: dump switch says '%s', starting on level %d\n",
			mode.c_str(), gLevel);
	}

	// Bring the network up before the loop starts, so the menu already knows whether there is a
	// release by the time it is first drawn. Skipped while dumping frames: that is a diagnostic run
	// and is not allowed to touch the network or to take time.
	if (!Screen::Instance()->DumpingEnabled())
	{
		LauncherPhase("boot: auto-connecting");
		gBootOnline = AutoConnectAtBoot();
		if (gBootOnline)
		{
			RefreshCatalogFromServer();
			BootUpdateCheck();
		}
		LauncherPhase("boot: entering the main loop");
	}

	int frameCount = 0;

	for (;;)
	{
		// A slice of the "is the install still complete" scan, before the frame is drawn so the verdict
		// the label uses is as fresh as this frame can make it. Cheap and self-cancelling: once the
		// selected version has a verdict there is nothing left to do here.
		PumpIntegrityCheck();

		DrawMenu();

		/* Frames written to the card, but only when the switch file is present.
		 *
		 * Two frames, and both buffers, and that is all this needs to do now. The pair is not redundancy:
		 * the dump reads VRAM directly because the display API does not track the frame, and GU
		 * alternates between two draw buffers, so which of the two holds the finished picture is not
		 * knowable from here. The caller keeps whichever one changed.
		 *
		 * There is deliberately no schedule of later states any more. There used to be -- a dropdown at
		 * frame 400, the changelog rung at 600, the viewer at 900 -- and every one of those dumps came
		 * back identical to the frame-30 dump, because the dumps only ever see the frame the program
		 * started on. Which screen gets photographed is decided by the switch file's contents above, and
		 * that is the only lever there is. See the note on DumpFrame in Screen.h for why any of this
		 * exists: the emulator cannot be captured from outside on this machine. */
		if (Screen::Instance()->DumpingEnabled())
		{
			++frameCount;

			if (frameCount == 30)
			{
				Screen::Instance()->DumpFrame("fb-30-front.bmp", Screen::DUMP_FRONT);
				Screen::Instance()->DumpFrame("fb-30-back.bmp", Screen::DUMP_BACK);
			}
			if (frameCount == 90)
			{
				Screen::Instance()->DumpFrame("fb-90-front.bmp", Screen::DUMP_FRONT);
				Screen::Instance()->DumpFrame("fb-90-back.bmp", Screen::DUMP_BACK);
			}
		}

		unsigned int pressed = PollPressed();

		// The update prompt is answered before anything else, including the two rungs that swallow
		// everything themselves. It is drawn over every rung (see DrawMenu) and it has to be LISTENED to
		// over every rung for the same reason: the scan decides an install is broken without asking
		// where the player is, so without this the X that starts the repair would also be the X that
		// opens a screenshot or deletes an install, depending on which rung happened to be up.
		if (gUpdatePrompt)
		{
			if (pressed & PSP_CTRL_CROSS)
			{
				gUpdatePrompt = false;

				// Forgotten rather than kept, so that a repair leaving the install broken raises the
				// modal again. DoUpdate reloads the releases, which drops every verdict and starts the
				// walk from the beginning, so the next answer the scan gives is about the install as it
				// is now -- and a modal that stayed silent about it would leave the player looking at a
				// menu that had quietly stopped mentioning a problem. O is the press that records the
				// version and stops the asking; this is the press that means "and again".
				gUpdatePromptVersion.clear();
				DoUpdate(gUpdatePromptRepair);
			}
			else if (gUpdatePromptAuto && (pressed & PSP_CTRL_CIRCLE))
			{
				// Not now. The version is already recorded -- it was set when the modal went up -- so
				// the scan reporting the same install broken does not put it straight back.
				gUpdatePrompt = false;
				Logger::Instance()->LogMessage("launcher: update prompt declined\n");
			}

			// Everything else, including Up and Down, belongs to a rung this is standing over. Swallowed
			// rather than passed on, which is what "modal" means here.
			continue;
		}

		// The fullscreen viewer first, because it swallows everything else. It is a place rather than a
		// highlight, and the only controls in it are the two that step the shots and the one that
		// leaves -- which is also why L and R mean "previous and next image" here and nothing elsewhere,
		// with no ambiguity: you leave this with O.
		if (gLevel == LEVEL_FULLSCREEN)
		{
			if (pressed & PSP_CTRL_LTRIGGER)
			{
				CycleImage(-1);
			}
			else if (pressed & PSP_CTRL_RTRIGGER)
			{
				CycleImage(1);
			}
			else if (pressed & PSP_CTRL_CIRCLE)
			{
				gLevel = LEVEL_SHOT;
				RefreshImages();				// the shots again, back at the strip's scale
			}
			continue;
		}

		// Scrolling the changelog, which is its own rung rather than something Up and Down do while
		// standing on the changelog. X goes in from the changelog, O comes back out; nothing else in the
		// interface moves the text.
		if (gLevel == LEVEL_NOTE_SCROLL)
		{
			const float maxScroll = TopMaxScroll();

			if (pressed & PSP_CTRL_UP)
			{
				gTopScroll -= TOP_LINE_H;
				if (gTopScroll < 0.0f)
				{
					gTopScroll = 0.0f;
				}
			}
			else if (pressed & PSP_CTRL_DOWN)
			{
				gTopScroll += TOP_LINE_H;
				if (gTopScroll > maxScroll)
				{
					gTopScroll = maxScroll;
				}
			}
			else if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS))
			{
				gLevel = LEVEL_CHANGELOG;
			}
			continue;
		}

		// The changelog, standing still. Up leaves for the screenshot, down for the buttons, and X is
		// what starts the scrolling.
		if (gLevel == LEVEL_CHANGELOG)
		{
			if (pressed & PSP_CTRL_CROSS)
			{
				gLevel = LEVEL_NOTE_SCROLL;
			}
			else if (pressed & PSP_CTRL_UP)
			{
				gLevel = LEVEL_SHOT;
			}
			else if (pressed & (PSP_CTRL_DOWN | PSP_CTRL_CIRCLE))
			{
				gLevel = LEVEL_BUTTONS;
			}
			continue;
		}

		if (gLevel == LEVEL_SHOT)
		{
			// Stepping the strip costs no card reads at all: every shot in it is already loaded, so only
			// the outline moves. That is the reason the whole strip is held rather than swapped as the
			// selection goes -- see LoadStrip.
			const Release* shotRel = Selected();
			const int shotCount = (shotRel != 0) ? (int)shotRel->images.size() : 0;

			if (pressed & (PSP_CTRL_LEFT | PSP_CTRL_LTRIGGER))
			{
				if (shotCount > 0)
				{
					gImageSel = (gImageSel + shotCount - 1) % shotCount;
				}
			}
			else if (pressed & (PSP_CTRL_RIGHT | PSP_CTRL_RTRIGGER))
			{
				if (shotCount > 0)
				{
					gImageSel = (gImageSel + 1) % shotCount;
				}
			}
			else if (pressed & PSP_CTRL_CROSS)
			{
				// Only the pictures open. X on this rung can only mean the one thing: show the chosen
				// shot at the screen's size.
				if (Screen::Instance()->HasStrip())
				{
					gLevel = LEVEL_FULLSCREEN;
					RefreshImages();			// the chosen shot, now at the screen's scale
				}
			}
			else if (pressed & (PSP_CTRL_DOWN | PSP_CTRL_CIRCLE))
			{
				gLevel = LEVEL_CHANGELOG;
			}
			continue;
		}

		// The Manage screen. Up and down choose a row, X removes the chosen install, O goes back to the
		// menu. It has no other exit and needs none: there is nothing behind it on the ladder to step down
		// to, so Circle is the whole of "done here".
		if (gLevel == LEVEL_MANAGE)
		{
			if (pressed & PSP_CTRL_CIRCLE)
			{
				gLevel = LEVEL_BUTTONS;
			}
			else if (pressed & PSP_CTRL_UP)
			{
				// Clamped rather than wrapped, the same as the version list: a list of installs is not a
				// ring either, and the ends of it are where a player looks for "the newest" and "the last".
				if (gManageSel > 0)
				{
					--gManageSel;
					ClampManage();
				}
			}
			else if (pressed & PSP_CTRL_DOWN)
			{
				if (gManageSel + 1 < (int)gInstalls.size())
				{
					++gManageSel;
					ClampManage();
				}
			}
			else if (pressed & PSP_CTRL_CROSS)
			{
				ManageDelete();
			}
			continue;
		}

		// The bottom row. The version list is its own little mode inside it: up and down pick, X
		// chooses, O backs out. It is also the only place the version changes now, since up and down
		// elsewhere belong to the ladder.
		if (gDropdownOpen)
		{
			if (pressed & PSP_CTRL_CIRCLE)
			{
				gDropdownOpen = false;
				continue;
			}
			if (pressed & PSP_CTRL_UP)
			{
				MoveSelection(-1);
				continue;
			}
			if (pressed & PSP_CTRL_DOWN)
			{
				MoveSelection(1);
				continue;
			}
			if (pressed & PSP_CTRL_CROSS)
			{
				// Choosing from the list closes it, which is what choosing means.
				gDropdownOpen = false;
				continue;
			}
			continue;
		}

		// The Manage menu, the version list's twin: up and down pick a row, X runs it, O backs out.
		//
		// The one difference is what X means. Choosing a version has already happened by the time the
		// cursor moves -- the list is a selector -- whereas these rows are actions, so X is what performs
		// the highlighted one. Nothing here needs a "confirm" step of its own because both rows already
		// have one where it belongs: Delete asks before it removes anything, and the update flow asks
		// before it downloads.
		if (gManageOpen)
		{
			if (pressed & PSP_CTRL_CIRCLE)
			{
				gManageOpen = false;
				continue;
			}
			if (pressed & PSP_CTRL_UP)
			{
				if (gManageMenuSel > 0)
				{
					--gManageMenuSel;
				}
				continue;
			}
			if (pressed & PSP_CTRL_DOWN)
			{
				if (gManageMenuSel + 1 < ManageRowCount())
				{
					++gManageMenuSel;
				}
				continue;
			}
			if (pressed & PSP_CTRL_CROSS)
			{
				const int action = ManageActionAt(gManageMenuSel);
				gManageOpen = false;

				if (action == MANAGE_DELETE)
				{
					const Release* rel = Selected();
					if (rel != 0)
					{
						Logger::Instance()->LogMessage("launcher: Manage -- delete '%s'\n",
							rel->version.c_str());
						DeleteInstall(rel->installIndex);
					}
				}
				else
				{
					/* Check for updates.
					 *
					 * This is where the network setup lives, and that placement is the point of the row
					 * rather than an implementation detail. The module init and the console's own
					 * access-point dialog happen inside DoUpdate, and they are a cost the player should
					 * be able to choose: asking (and being asked to pick a connection) is the right shape
					 * for a launcher that otherwise never touches the network, and the opposite of
					 * polling at boot, where the network would be brought up on the menu's behalf while
					 * the player is looking at a screenshot.
					 */
					Logger::Instance()->LogMessage("launcher: Manage -- check for updates\n");
					DoUpdate(false);
				}
				continue;
			}
			continue;
		}

		// Up leaves the buttons for the changelog. Ignored while the list is open, which is handled
		// above, so a thumb on the d-pad cannot leave the list behind with the cursor somewhere else.
		if (pressed & PSP_CTRL_UP)
		{
			gLevel = LEVEL_CHANGELOG;
			continue;
		}

		if (pressed & PSP_CTRL_CIRCLE)
		{
			Logger::Instance()->LogMessage("launcher: quit by the player\n");
			return;
		}

		// The update key. A separate button from Play rather than a mode the Play button enters, because
		// the two do very different things and the destructive one is the rarer.
		if (pressed & PSP_CTRL_TRIANGLE)
		{
			DoUpdate(false);
			continue;
		}

		// Left and right move along the bottom row.
		if (pressed & PSP_CTRL_LEFT)
		{
			gFocus = (gFocus + FOCUS_COUNT - 1) % FOCUS_COUNT;
		}
		if (pressed & PSP_CTRL_RIGHT)
		{
			gFocus = (gFocus + 1) % FOCUS_COUNT;
		}

		if (pressed & PSP_CTRL_CROSS)
		{
			Activate();
		}
	}
}
