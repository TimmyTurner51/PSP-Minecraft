#include "Screen.h"

#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psppower.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <Aurora/Graphics/TextureManager.h>
#include <Aurora/Graphics/Sprite.h>
#include <Aurora/Graphics/intraFont/intraFont.h>
#include <Aurora/Utils/Logger.h>

using namespace Aurora::Graphics;
using namespace Aurora::Utils;

/*
 * How much memory the console says is free right now, in Log.txt.
 *
 * A short line to read, and the one number that separates "this program is running out of memory" from
 * every other explanation for a fault inside a loader that allocates. The heap is fixed at link time
 * (PSP_HEAP_SIZE_KB in main.cpp), so this is the free figure *after* that heap was taken, and comparing
 * it against the same figure in the emulator is the whole point of printing it.
 */
static void LogMemory(const char* when)
{
	Logger::Instance()->LogMessage("screen: memory %s free=%d largest=%d\n",
		when, sceKernelTotalFreeMemSize(), sceKernelMaxFreeMemSize());
}

/*
 * The framebuffer layout, copied from RenderManager.h rather than invented.
 *
 * These are the game's own addresses, and they are duplicated here on purpose: the two programs run one
 * at a time and never share a display, but they do share the console, and a screen the player sees for
 * three seconds should not be laid out at an address the game would disagree with. 8888 at 512 stride,
 * the second buffer immediately after the first, depth after that.
 */
#define SCR_WIDTH  480
#define SCR_HEIGHT 272
#define BUF_WIDTH  512
#define FRAME_BYTES (BUF_WIDTH * SCR_HEIGHT * 4)

#define VRAM_FRONT  ((void*)0x00000000)
#define VRAM_BACK   ((void*)(FRAME_BYTES))
#define VRAM_DEPTH  ((void*)(FRAME_BYTES * 2))

/*
 * The command list, and the one allocation in this file whose SIZE IS A CORRECTNESS REQUIREMENT rather
 * than a budget.
 *
 * It used to be 0x4000 words -- 64 KB, described as "twice the heaviest frame this screen has been
 * measured at". That measured frame was the update screen, and the menu has grown a long way past it
 * since: the changelog is wrapped and every line of it is DRAWN (the scissor hides the overflow, it
 * does not skip the work), the gallery strip draws four border panels per tile, and a line of text is
 * two vertices per glyph of 24 bytes each -- color, x, y, z, u, v. A long changelog is a few hundred
 * glyphs before anything else is on screen, and the frame that raised the repair prompt measured
 * larger than 64 KB. It then did not stop.
 *
 * WHY IT DID NOT STOP, WHICH IS THE POINT OF THIS COMMENT. sceGuStart takes no size -- the SDK keeps
 * its own idea of how long the list is, and its own samples declare 262144 elements (1 MB). Nothing
 * here ever told it 0x4000. So sceGuGetMemory happily handed out vertices past the end of this array,
 * and what follows it in .bss is Launcher.cpp's globals. Measured, not deduced: the frame that raised
 * the update prompt wrote display-list vertices over gReleases, gInstalls and gVersionSel, and the
 * next frame faulted reading rel->installIndex out of a release list whose storage pointer had become
 * 0xFFFFFFFF -- a white vertex colour. The corruption is silent, it is in a different translation unit
 * from the code that caused it, and it only surfaces a frame later as a crash somewhere unrelated.
 *
 * So the array has to be at least as large as whatever the SDK assumes. Anything smaller means the
 * SDK's own guard is not the binding constraint and the real limit is this declaration, which the SDK
 * cannot see. 1 MB matches the SDK's samples, which is the only published number for the default, and
 * it is affordable here: the launcher is not the program that is short of memory, and .bss costs
 * nothing in the PBP because it is not stored.
 */
static unsigned int sDisplayList[0x40000] __attribute__((aligned(64)));

#define ART_DIR "Assets/Textures/"
#define FONT_FILE "Assets/Fonts/eng_letters.pgf"

// The game's layout, taken from LoadingScreen::RunLoadingScreen and StateUpdate::DrawProgressFrame so
// the three screens are the same screen: dirt tiles 8x5 at 64 px, the bar 160 wide at y=150, the status
// line centred at y=100.
#define TILE_STEP   64.0f
#define BAR_WIDTH   160.0f
#define BAR_X       160.0f
#define BAR_Y       150.0f
#define STATUS_Y    100
#define DETAIL_Y    172
#define PROMPT_Y    250

/*
 * The game's button, and where the four numbers come from.
 *
 * menu_elements.png holds the button as two 95x12 sub-rects stacked: the resting state at (0,0) and the
 * lit one at (0,12). Drawn at scale 2 that is 190x24 on screen. All four numbers are DebugMenu's
 * kButtonScaleX/kButtonScaleY/kButtonPitch/kLabelBaseline, and DebugMenu's are themselves measured
 * against the pause menu -- so a button here and a button in the game are the same button rather than
 * two things that look similar.
 *
 * The baseline offset is the one worth explaining: intraFont takes a BASELINE, not a top. Drawing a
 * label at the button's centre -- the obvious thing -- puts the text in the button's upper half and
 * reads as broken. 18 px per unit of font size, at the 0.5 this screen uses, is 9 px.
 */
#define BUTTON_SCALE_X  2.0f
#define BUTTON_SCALE_Y  2.0f
#define BUTTON_HALF_H   12.0f

/*
 * Where a label sits inside its button, below the button's centre.
 *
 * The game's own menus use 9 px at this font size: DebugMenu's kLabelBaseline is 18 per unit of size,
 * and the pause menu draws a button centred at y=70 with its label at y=79. So 9 is the number that
 * makes this screen's buttons identical to the game's.
 *
 * It is deliberately NOT 9 here. 9 px sits the text visibly low in the button -- low enough that it was
 * reported as a bug on sight -- and measuring the pause menu shows the same 9, so the reading is of a
 * convention rather than of this code being wrong. intraFont takes a BASELINE, not a centre, so the
 * offset that *looks* centred is smaller than the offset that *is* the game's. 6 puts the visual mass
 * of a capital letter on the button's midline: the glyph spans roughly 30 px of cap height above the
 * baseline at size 0.5, so an offset of 6 leaves about 9 px of ink above the centre line and 9 below.
 *
 * The cost is that a launcher button is a few pixels off a game button. That is a deliberate trade, and
 * the place to revisit it is here.
 */
#define BUTTON_LABEL    6.0f

#define FONT_SIZE 0.5f

/*
 * How a button that cannot act is drawn. See the note on ButtonW.
 *
 * These two numbers come from the four brightness levels the row has to keep apart, and they were
 * measured rather than chosen. The sprites are cut from menu_elements.png at scale 2 over dirt of about
 * 23, and that file's two halves are only 32 levels apart to begin with: the resting half averages a
 * body of 127 and the lit half a body of 159. There is not much room to dim within.
 *
 *     focused, can act        159   lit, full             white label
 *     focused, cannot act     148   lit, barely muted     grey label
 *     unfocused, can act      127   resting, full         white label
 *     unfocused, cannot act    96   resting, well down    grey label
 *
 * The order is the requirement and the alphas are whatever produces it. The focused-but-unavailable
 * level must stay clearly ABOVE unfocused-and-available, or the cursor ends up looking duller than the
 * buttons beside it -- which is the same fault as the one this fixes, just turned around. A more
 * emphatic dim, 0.8, was tried first and put that state at 132 against 127: five levels apart, which is
 * not a distinction anyone can see.
 */
#define BUTTON_DIM_ALPHA          0.70f	// a button that cannot act, without the cursor on it
#define BUTTON_DIM_ALPHA_FOCUSED  0.92f	// the same, with the cursor on it
#define BUTTON_DIM_LABEL          0xFF9AA0A6

// The button prompt icons. CELL is the size of one cell in title/buttons.png and is fixed there by
// tools/make-button-icons.py; BOX is the size one is drawn at on screen.
//
// BOX EQUALS CELL, and that is the point rather than a coincidence. The icons are pixel art with no
// anti-aliasing in them at all, so any scale factor other than 1:1 either resamples their edges into
// grey or drops and duplicates rows, and both undo the crispness the art was drawn for. 16 px puts a
// 12 px button beside 10 px letters, which is the ratio the real prompts use.
#define ICON_CELL 16.0f
#define ICON_BOX  16.0f

// Present next to the launcher to enable DumpFrame. See the note on it in Screen.h.
#define DUMP_SWITCH "dump_frames.txt"

Screen* Screen::Instance()
{
	static Screen instance;
	return &instance;
}

const char* Screen::DumpSwitchName()
{
	return DUMP_SWITCH;
}

Screen::Screen()
	: ready(false)
	, dumping(false)
	, backSprite(0)
	, barSprite(0)
	, barBackSprite(0)
	, buttonSprite(0)
	, buttonSelSprite(0)
	, buttonTexture(-1)
	, panelSprite(0)
	, panelTexture(-1)
	, iconTexture(-1)
	, font(0)
{
	for (int i = 0; i < ICON_COUNT; ++i)
	{
		iconSprites[i] = 0;
	}
}

Screen::~Screen()
{
	delete backSprite;
	delete barSprite;
	delete barBackSprite;
	delete buttonSprite;
	delete buttonSelSprite;

	for (int i = 0; i < ICON_COUNT; ++i)
	{
		delete iconSprites[i];
	}

	for (unsigned int i = 0; i < buttonCache.size(); ++i)
	{
		delete buttonCache[i].normal;
		delete buttonCache[i].sel;
	}
	buttonCache.clear();

	// Through FreePanelImage, because the texture has to be released alongside the sprite; deleting
	// the sprite alone would leave the image's pixels in TextureManager with no handle to them. The
	// strip is the same arrangement and is released first for the ordering reason on LoadStrip.
	FreeStrip();
	FreePanelImage();
}

/*
 * The texture pack folder, from Config/texturepack.cf, defaulting to the game's own default.
 *
 * The format is a 32-bit length followed by that many bytes, written by the game's
 * TextureHelper::SaveTexturePackPath. The length is bounded before it is trusted: this file comes off
 * the card and a foreign or truncated one would otherwise be a length of up to 4 GB, which is what the
 * same read in TextureHelper.cpp is commented about at length. Here it matters less -- a wrong folder
 * means the art does not load and this screen falls back to text -- but the read is the same read, and
 * copying a known bug because the consequences are smaller is how the bug survives.
 */
void Screen::PackFolder(char* out, unsigned int size)
{
	strcpy(out, "Minecraft/");

	SceUID fd = sceIoOpen("Config/texturepack.cf", PSP_O_RDONLY, 0777);
	if (fd < 0)
	{
		return;
	}

	unsigned int length = 0;
	if (sceIoRead(fd, &length, sizeof(length)) == (int)sizeof(length) &&
		length > 0 && length < 64)
	{
		char name[64];
		int got = sceIoRead(fd, name, length);
		if (got > 0)
		{
			name[got] = '\0';
			if (strlen(name) + 1 < size)
			{
				strcpy(out, name);
			}
		}
	}

	sceIoClose(fd);
}

/*
 * GU, configured the way RenderManager::Init configures it.
 *
 * Every call here is copied from that function rather than reasoned out, because the differences
 * between a screen that renders correctly and one that renders shifted, unblended, or upside down are
 * exactly the kind of thing that is invisible in an emulator and obvious on hardware.
 */
bool Screen::SetupGu()
{
	/* GU's own initialisation is separated from its configuration here, and each half is logged,
	 * because on hardware this function is the prime suspect and the two halves fail for different
	 * reasons: sceGuInit brings up the GE and its interrupt plumbing, while the block below submits the
	 * display setup. The emulator run of this screen got through both, so whichever one the console
	 * stops in is the one that is lying about what the real hardware allows. */
	Logger::Instance()->LogMessage("screen: sceGuInit\n");
	sceGuInit();
	Logger::Instance()->LogMessage("screen: sceGuInit done, configuring the display\n");

	sceGuStart(GU_DIRECT, sDisplayList);
	sceGuDrawBuffer(GU_PSM_8888, VRAM_FRONT, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, VRAM_BACK, BUF_WIDTH);
	sceGuDepthBuffer(VRAM_DEPTH, BUF_WIDTH);

	sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
	sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);

	sceGuDepthRange(50000, 10000);

	sceGuEnable(GU_SCISSOR_TEST);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);

	sceGuDisable(GU_TEXTURE_2D);
	sceGuEnable(GU_CLIP_PLANES);

	sceGuEnable(GU_CULL_FACE);
	sceGuFrontFace(GU_CCW);

	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuAlphaFunc(GU_GREATER, 0.0f, 0xff);

	sceGuStencilFunc(GU_ALWAYS, 1, 1);
	sceGuStencilOp(GU_KEEP, GU_KEEP, GU_REPLACE);

	sceGuTexFilter(GU_LINEAR, GU_LINEAR);

	sceGuFinish();
	sceGuSync(0, 0);
	Logger::Instance()->LogMessage("screen: display setup submitted and synced\n");

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	sceGuShadeModel(GU_SMOOTH);
	Logger::Instance()->LogMessage("screen: display on\n");
	return true;
}

/*
 * The textures, straight out of the pack the game is using.
 *
 * Constructing a Sprite by number loads nothing -- the texture is loaded first and its number checked,
 * which is the split the note below explains. Vram is 0 on that path (TextureManager::LoadPng's
 * one-argument overload), so the textures live on the heap and cannot collide with the framebuffers
 * above -- which matters here because this file chooses its own VRAM addresses rather than sharing
 * RenderManager's allocator.
 *
 * Any sprite failing is fatal for this screen on purpose. A dirt background with no bar, or buttons
 * that are not there, is not a half-decent version of the game's look; it is a broken-looking screen,
 * and the text fallback is better than that.
 */
bool Screen::LoadArt(const char* dirtPath, const char* loadPath, const char* menuPath,
	const char* iconPath)
{
	LogMemory("before the art");

	/* The load and the Sprite are split here, and that split is the instrument.
	 *
	 * Two different faults live in `new Sprite(path, ...)`: the PNG loader dying, and the constructor
	 * indexing TextureManager's image list with a number it got back when the load produced nothing.
	 * The emulator log prints "Loaded texture:" between them, so a log that stops before that line says
	 * loader and one that stops after says constructor -- but only because the loader happens to log
	 * there. Calling the loader directly, and naming the number it produced, makes the distinction
	 * explicit rather than inferred, and it is the number that matters: Sprite's constructor reads
	 * Images[number] whatever number is, so -1 is a read from before the vector.
	 */
	Logger::Instance()->LogMessage("screen: loading the dirt texture\n");
	TextureManager::Instance()->LoadTexture(dirtPath);

	int dirtNumber = TextureManager::Instance()->GetTextureNumber(dirtPath);
	Logger::Instance()->LogMessage("screen: dirt loaded as number %d\n", dirtNumber);
	if (dirtNumber < 0)
	{
		return false;
	}

	Logger::Instance()->LogMessage("screen: building the dirt sprite\n");
	backSprite = new Sprite(dirtNumber, 0, 0, 32, 32);
	if (backSprite == 0)
	{
		return false;
	}
	// Scale is called exactly once, here, and that is not style: Sprite::Scale multiplies the existing
	// vertex positions, so a second call on the same sprite squares the scale rather than reapplying it.
	backSprite->Scale(2, 2);
	LogMemory("after the dirt sprite");

	// Rows 0-7 of load.png are the filled half and 8-15 the empty one. That is the same split
	// LoadingScreen names, and the wrong way round produces a bar that fills with the track colour.
	Logger::Instance()->LogMessage("screen: loading the bar art\n");
	TextureManager::Instance()->LoadTexture(loadPath);

	int barNumber = TextureManager::Instance()->GetTextureNumber(loadPath);
	Logger::Instance()->LogMessage("screen: bar loaded as number %d\n", barNumber);
	if (barNumber < 0)
	{
		return false;
	}

	barSprite = new Sprite(barNumber, 0, 0, 16, 8);
	barBackSprite = new Sprite(barNumber, 0, 8, 16, 8);
	LogMemory("after the bar sprites");

	// The button. Two sub-rects of the same image, so they share one texture number and one heap copy.
	Logger::Instance()->LogMessage("screen: loading the button art\n");
	TextureManager::Instance()->LoadTexture(menuPath);

	int menuNumber = TextureManager::Instance()->GetTextureNumber(menuPath);
	Logger::Instance()->LogMessage("screen: menu art loaded as number %d\n", menuNumber);
	if (menuNumber < 0)
	{
		return false;
	}

	buttonSprite = new Sprite(menuNumber, 0, 0, 95, 12);
	buttonSelSprite = new Sprite(menuNumber, 0, 12, 95, 12);

	// Kept for the wider buttons the cache builds later. See ButtonW.
	buttonTexture = menuNumber;

	// Once each, for the reason spelled out above the dirt sprite's Scale call.
	if (buttonSprite != 0)
	{
		buttonSprite->Scale(BUTTON_SCALE_X, BUTTON_SCALE_Y);
	}
	if (buttonSelSprite != 0)
	{
		buttonSelSprite->Scale(BUTTON_SCALE_X, BUTTON_SCALE_Y);
	}
	LogMemory("after the button sprites");

	/* The button prompt icons, and the one piece of art here whose absence is not fatal.
	 *
	 * Everything above is the game's look: a screen with no dirt or no buttons is not a worse version
	 * of the menu, it is a broken one, so those failures stop. This sheet is a prompt decoration. A card
	 * that predates it should still get a working launcher, printing "press X" the way it used to, and
	 * refusing to start over a missing glyph would be a far worse failure than the one it avoids.
	 *
	 * So this is loaded best-effort and the first failure cleans up after itself, leaving iconTexture at
	 * -1. HasIcons() reports that, and the callers fall back to words.
	 */
	iconTexture = -1;
	for (int i = 0; i < ICON_COUNT; ++i)
	{
		iconSprites[i] = 0;
	}

	Logger::Instance()->LogMessage("screen: loading the button icons\n");
	TextureManager::Instance()->LoadTexture(iconPath);

	int icons = TextureManager::Instance()->GetTextureNumber(iconPath);
	Logger::Instance()->LogMessage("screen: icons loaded as number %d\n", icons);
	if (icons >= 0)
	{
		iconTexture = icons;
		for (int i = 0; i < ICON_COUNT; ++i)
		{
			// One cell per icon, cut by index: the cells are uniform, which is why this needs no table of
			// rectangles and why the art is generated on a grid. See tools/make-button-icons.py.
			iconSprites[i] = new Sprite(icons, (int)(ICON_CELL * i), 0, (int)ICON_CELL, (int)ICON_CELL);
			if (iconSprites[i] == 0)
			{
				iconTexture = -1;
				break;
			}
			iconSprites[i]->Scale(ICON_BOX / ICON_CELL, ICON_BOX / ICON_CELL);
		}

		if (iconTexture < 0)
		{
			Logger::Instance()->LogMessage(
				"screen: a button icon would not build, prompts will name the buttons in words\n");
		}
	}
	else
	{
		Logger::Instance()->LogMessage(
			"screen: no button icons, prompts will name the buttons in words\n");
	}
	LogMemory("after the button icons");

	return barSprite != 0 && barBackSprite != 0 && buttonSprite != 0 && buttonSelSprite != 0;
}

/*
 * The game's font.
 *
 * intraFont is a C library the game links directly, so this is the same renderer that draws every
 * label in Minecraft -- including the ones on the loading screen this file is copying. Handle checked
 * because a null here is not a missing label, it is a fault inside intraFont's draw path, which is
 * somewhere far less obvious than the load that failed.
 */
bool Screen::LoadFont()
{
	Logger::Instance()->LogMessage("screen: intraFontInit\n");
	intraFontInit();

	Logger::Instance()->LogMessage("screen: loading " FONT_FILE "\n");
	font = intraFontLoad(FONT_FILE, INTRAFONT_CACHE_ASCII);

	/* The handle is logged as a number rather than only being tested, for the same reason as the texture
	 * numbers above: a null handle means this failed, but a handle that is present but wrong is the
	 * other way intraFont goes wrong, and only the number distinguishes them. */
	Logger::Instance()->LogMessage("screen: font handle 0x%08X\n", (unsigned int)font);
	if (font == 0)
	{
		return false;
	}

	// Sized and coloured like RenderManager::InitDebugFont sets the english font. The style is set
	// again per call by the primitives below, because they need both alignments and intraFont keeps
	// alignment in the style -- but setting it here means a caller that prints before making one of
	// those calls still gets the game's font rather than intraFont's defaults.
	intraFontSetStyle(font, FONT_SIZE, 0xFFFFFFFF, 0xFF000000, INTRAFONT_ALIGN_CENTER);
	return true;
}

/*
 * Whether a file is there, without loading it.
 *
 * This exists so that the assets can be checked BEFORE any of GU is touched. Two things depend on that
 * ordering and both are about the fallback:
 *
 *   - If SetupGu ran and the art was then found missing, main would call pspDebugScreenInit on top of an
 *     already-initialised GU. Two sceGuInit calls and two incompatible display setups in one process is
 *     not a state worth finding out about on hardware.
 *   - Loading is not the safe way to test for a file here. TextureManager::GetTextureNumber returns -1
 *     for a name it never loaded, and Sprite's constructor indexes Images with that number -- an
 *     unchecked Images[-1] is the exact fault TextureManager.cpp has a comment about at length.
 *
 * So: ask the question first, and only commit to the graphics path once it is answered.
 */
static bool FilePresent(const char* path)
{
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0)
	{
		return false;
	}

	sceIoClose(fd);
	return true;
}

bool Screen::Init()
{
	static bool attempted = false;
	if (attempted)
	{
		return ready;
	}
	attempted = true;

	LogMemory("at screen init start");

	char folder[96];
	char dirt[160];
	char load[160];
	char menu[160];
	char icons[160];

	/* Each stage below is named in the log before it runs, and the naming is not decoration. The first
	 * hardware run of this screen stopped with nothing after the package line in Log.txt and no line
	 * from this function at all, which left four candidates -- the pack lookup, the asset check, GU, or
	 * intraFont -- and no way to choose between them. A logger line before each one costs nothing and
	 * makes the next run answer the question instead of narrowing it. See Scripts/BootTrace.h: this is
	 * the same "name the step before doing it" rule the game's boot uses, for the same reason. */
	Logger::Instance()->LogMessage("screen: looking for the texture pack folder\n");

	PackFolder(folder, sizeof(folder));
	snprintf(dirt, sizeof(dirt), ART_DIR "%s" "title/dirt.png", folder);
	snprintf(load, sizeof(load), ART_DIR "%s" "title/load.png", folder);
	snprintf(menu, sizeof(menu), ART_DIR "%s" "title/menu_elements.png", folder);
	snprintf(icons, sizeof(icons), ART_DIR "%s" "title/buttons.png", folder);

	Logger::Instance()->LogMessage("screen: pack '%s', checking the art\n", folder);

	/* Each file is named individually when it is missing, rather than all four being reduced to one
	 * message. On a card where the pack folder is not "Minecraft/" this is the line that says which
	 * asset the pack did not provide, and telling those apart costs one comparison each.
	 *
	 * buttons.png is reported but not required. It is the button prompt icons: the menu works without
	 * them, printing the button's name instead, and a card from before that sheet existed should still
	 * get a launcher rather than the text fallback. See LoadArt. */
	if (!FilePresent(dirt)) { Logger::Instance()->LogMessage("screen: missing %s\n", dirt); }
	if (!FilePresent(load)) { Logger::Instance()->LogMessage("screen: missing %s\n", load); }
	if (!FilePresent(menu)) { Logger::Instance()->LogMessage("screen: missing %s\n", menu); }
	if (!FilePresent(icons)) { Logger::Instance()->LogMessage("screen: missing %s\n", icons); }
	if (!FilePresent(FONT_FILE)) { Logger::Instance()->LogMessage("screen: missing %s\n", FONT_FILE); }

	if (!FilePresent(dirt) || !FilePresent(load) || !FilePresent(menu) || !FilePresent(FONT_FILE))
	{
		Logger::Instance()->LogMessage(
			"screen: the art or the font is not on the card, using the text screen\n");
		return false;
	}

	Logger::Instance()->LogMessage("screen: art present, starting GU\n");

	if (!SetupGu())
	{
		Logger::Instance()->LogMessage("screen: GU setup failed, falling back to text\n");
		return false;
	}

	Logger::Instance()->LogMessage("screen: GU up, loading the art\n");

	if (!LoadArt(dirt, load, menu, icons))
	{
		Logger::Instance()->LogMessage(
			"screen: could not load the dirt/bar/button art, falling back to text\n");
		return false;
	}

	Logger::Instance()->LogMessage("screen: art loaded, loading the font\n");

	if (!LoadFont())
	{
		Logger::Instance()->LogMessage(
			"screen: could not load " FONT_FILE ", falling back to text\n");
		return false;
	}

	/* The debug dump switch, read once here rather than per frame. Checked last, after the assets, so a
	 * card that is missing its art still reports the missing art first -- that is the more useful
	 * message, and a dump of a fallback text screen would only be confusing. */
	dumping = FilePresent(DUMP_SWITCH);
	Logger::Instance()->LogMessage("screen: frame dumping %s\n",
		dumping ? "ON, writing framebuffer.bmp" : "off");

	Logger::Instance()->LogMessage("screen: ready, drawing the game's own look\n");
	ready = true;
	return true;
}

// ---------------------------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------------------------

void Screen::BeginFrame()
{
	if (!ready)
	{
		return;
	}

	// The game's own ortho, from RenderManager::SetOrtho, so the coordinates below are pixels with the
	// origin top-left exactly as they are in LoadingScreen and StateUpdate.
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumOrtho(0, 480, 272, 0, -30, 30);

	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();

	// The sky blue the game clears to while it loads, so a tile that lands a fraction of a pixel out
	// shows blue rather than the magenta most of this codebase uses for "uninitialised".
	sceGuStart(GU_DIRECT, sDisplayList);
	sceGuClearColor(GU_COLOR(0.466f, 0.72f, 1.0f, 1.0f));
	sceGuClearStencil(0);
	sceGuClearDepth(0);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

	sceGuDisable(GU_DEPTH_TEST);
	sceGuEnable(GU_BLEND);
	sceGuColor(GU_COLOR(1, 1, 1, 1.0f));

	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 5; y++)
		{
			backSprite->SetPosition(x * TILE_STEP, y * TILE_STEP);
			backSprite->Draw();
		}
	}
}

void Screen::EndFrame()
{
	if (!ready)
	{
		return;
	}

	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_DEPTH_TEST);

	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

/*
 * The frame that was just presented, written out as a BMP.
 *
 * Called by the caller after EndFrame, because that is the only point at which the frame is complete
 * and the buffer being read is the one on screen.
 *
 * sceDisplayGetFrameBuf is used rather than a remembered VRAM address because the draw and display
 * buffers swap on every frame, so which address holds the finished frame alternates. Asking the display
 * is the only answer that stays correct. (There is no sceGuReadPix in this SDK; it is not an oversight
 * that this goes through the display API.)
 *
 * The BMP is written a row at a time into a static buffer rather than composed whole in memory: this is
 * a diagnostic, and it must not be able to fail for want of heap while being used to investigate a
 * problem that might itself be memory.
 */
void Screen::DumpFrame(const char* fileName, int source)
{
	if (!ready || !dumping || fileName == 0)
	{
		return;
	}

	const unsigned char* front = 0;
	int stride = BUF_WIDTH;

	if (source == DUMP_DISPLAY)
	{
		void* disp = 0;
		int s = 0;
		int format = 0;
		sceDisplayGetFrameBuf(&disp, &s, &format, 0);
		if (disp == 0 || s <= 0)
		{
			Logger::Instance()->LogMessage("screen: no framebuffer to read\n");
			return;
		}
		front = (const unsigned char*)disp;
		stride = s;
	}
	else
	{
		/* VRAM, read directly rather than through the display API.
		 *
		 * The display API is not enough, and this was measured rather than suspected: four dumps taken
		 * at frames 30, 90, 153 and 400 came back BYTE-IDENTICAL -- and the last three were taken with
		 * a dropdown open that the first did not have. So the pointer sceDisplayGetFrameBuf returns
		 * stops tracking the frame after the first few, and anything drawn later is invisible to it.
		 *
		 * GU alternates between two draw buffers, so both are written here and the caller keeps
		 * whichever one changed. 0x44000000 is the uncached alias of VRAM, which is where a
		 * framebuffer address on the uncached bus resolves; the offsets VRAM_FRONT and VRAM_BACK are
		 * the same ones handed to sceGuDrawBuffer, so they land on the same memory GU draws into. */
		unsigned int offset = (source == DUMP_FRONT)
			? (unsigned int)(size_t)VRAM_FRONT
			: (unsigned int)(size_t)VRAM_BACK;
		front = (const unsigned char*)(0x44000000u + offset);
		stride = BUF_WIDTH;
	}

	/* 8888 means one 32-bit word per pixel holding R,G,B,A in that byte order -- a memory layout rather
	 * than an API convention, so it is the same here and on the console. BMP wants 24-bit BGR, rows
	 * stored bottom-up, each row padded to a multiple of four bytes. The copy below reverses the rows
	 * and the channel order and does nothing else.
	 */
	const int rowBytes = SCR_WIDTH * 3;
	const int padBytes = (4 - (rowBytes & 3)) & 3;
	const unsigned int dataBytes = (unsigned int)(rowBytes + padBytes) * SCR_HEIGHT;

	// BMP is little-endian and so is the PSP, so every multi-byte field below can be written a byte at a
	// time with no conversion helper. Byte order is spelled out rather than memcpy'd from an int, which
	// is what keeps that independence obvious.
	unsigned char header[54];
	memset(header, 0, sizeof(header));
	unsigned int fileBytes = 54 + dataBytes;
	header[0] = 'B';
	header[1] = 'M';
	header[2] = (unsigned char)(fileBytes);
	header[3] = (unsigned char)(fileBytes >> 8);
	header[4] = (unsigned char)(fileBytes >> 16);
	header[5] = (unsigned char)(fileBytes >> 24);
	header[10] = 54;					// where the pixel data starts
	header[14] = 40;					// BITMAPINFOHEADER
	header[18] = (unsigned char)(SCR_WIDTH);
	header[19] = (unsigned char)(SCR_WIDTH >> 8);
	header[22] = (unsigned char)(SCR_HEIGHT);
	header[23] = (unsigned char)(SCR_HEIGHT >> 8);
	header[26] = 1;						// planes
	header[28] = 24;					// bits per pixel
	header[34] = (unsigned char)(dataBytes);
	header[35] = (unsigned char)(dataBytes >> 8);
	header[36] = (unsigned char)(dataBytes >> 16);
	header[37] = (unsigned char)(dataBytes >> 24);

	SceUID fd = sceIoOpen(fileName, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0)
	{
		Logger::Instance()->LogMessage("screen: cannot write %s (0x%08X)\n",
			fileName, (unsigned int)fd);
		return;
	}

	sceIoWrite(fd, header, sizeof(header));

	static unsigned char row[rowBytes + 4];
	const unsigned char zero[4] = {0, 0, 0, 0};
	const unsigned char* pixels = (const unsigned char*)front;

	for (int y = SCR_HEIGHT - 1; y >= 0; --y)
	{
		const unsigned char* src = pixels + (size_t)y * (size_t)stride * 4;
		for (int x = 0; x < SCR_WIDTH; ++x)
		{
			row[x * 3 + 0] = src[x * 4 + 2];	// blue
			row[x * 3 + 1] = src[x * 4 + 1];	// green
			row[x * 3 + 2] = src[x * 4 + 0];	// red
		}
		sceIoWrite(fd, row, rowBytes);
		if (padBytes > 0)
		{
			sceIoWrite(fd, zero, padBytes);
		}
	}

	sceIoClose(fd);
	Logger::Instance()->LogMessage("screen: wrote %s (%u bytes)\n", fileName, fileBytes);
}

// ---------------------------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------------------------

/*
 * A vertex for the solid rectangles Panel() draws.
 *
 * Colour first, then position, and that order is a contract with the GE rather than a style choice: a
 * vertex is read as the attributes appear in memory, and the GU orders them texture, colour, normal,
 * position no matter which way round the format flags are written. Aurora::Graphics::SimplePSPVertex
 * (Vertex.h) is the same struct for the same reason, and Sprite's TexturesPSPVertex puts u,v before x,y
 * to match. Laid out the other way round -- position first -- the GE reads x as the colour and the
 * colour as x, and every rectangle this file draws comes out invisible, which is exactly what Panel did
 * until this was found.
 */
struct FlatVertex
{
	unsigned int color;
	float x;
	float y;
	float z;
};

/*
 * One line of text, with the game's shadow.
 *
 * Drawn twice rather than by intraFont's own shadow support: black at y+1, then the colour at y. That
 * is exactly what DebugMenu::Render does for a button label and StatePlay::DrawText does for the debug
 * text, so this is the project's existing way to make thin text readable over a texture whose
 * brightness changes from tile to tile.
 *
 * TextureManager is told its binding is stale afterwards. intraFont binds the font atlas behind the
 * manager's back, so without this the next sprite would sample the font atlas with the sprite's
 * palette -- the same fix, for the same reason, that RenderManager::DebugPrint makes.
 *
 * THE COLOUR IS ABGR, NOT ARGB, and that is worth stating because nothing at the call sites suggests it.
 * 0xFF9AA0A6 comes out as (166,160,154) rather than (154,160,166) -- the red and blue bytes are read
 * the other way round. This went unnoticed for a long time because every colour this file passed was
 * white, the one value the two orders agree on. It is written down for whoever adds the next coloured
 * label: a saturated colour would come out visibly wrong and the call would look correct.
 *
 * Nothing corrects for it. The one non-white colour in use is a near-neutral grey, where a 12-level
 * swap between two channels is not perceptible, and byte-swapping here would make this argument mean
 * something different from what it looks like at every call site.
 */
static void PrintText(intraFont* font, float size, float x, float y, const char* text,
	unsigned int color, int align)
{
	if (font == 0 || text == 0 || text[0] == '\0')
	{
		return;
	}

	sceGuEnable(GU_BLEND);
	sceGuEnable(GU_TEXTURE_2D);

	intraFontSetStyle(font, size, 0xFF000000, 0, align);
	intraFontPrint(font, x, y + 1.0f, text);

	intraFontSetStyle(font, size, color, 0, align);
	intraFontPrint(font, x, y, text);

	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);

	TextureManager::Instance()->InvalidateCurrentTexture();
}

void Screen::Text(float x, float y, const char* text)
{
	PrintText(font, FONT_SIZE, x, y, text, 0xFFFFFFFF, INTRAFONT_ALIGN_CENTER);
}

void Screen::TextLeft(float x, float y, const char* text)
{
	PrintText(font, FONT_SIZE, x, y, text, 0xFFFFFFFF, INTRAFONT_ALIGN_LEFT);
}

void Screen::TextHighlight(float x, float y, const char* text)
{
	PrintText(font, FONT_SIZE, x, y, text, 0xFF00FFFF, INTRAFONT_ALIGN_CENTER);
}

void Screen::TextHighlightRight(float x, float y, const char* text)
{
	PrintText(font, FONT_SIZE, x, y, text, 0xFF00FFFF, INTRAFONT_ALIGN_RIGHT);
}

void Screen::TextSized(float x, float y, float size, unsigned int color, int align, const char* text)
{
	PrintText(font, size, x, y, text, color, align);
}

float Screen::MeasureText(float size, const char* text)
{
	if (font == 0 || text == 0 || text[0] == '\0')
	{
		return 0.0f;
	}

	// The style must be set before measuring, because intraFont measures at whatever size is currently
	// in effect. Measuring at the wrong size is the mistake this note exists to prevent: the wrapped
	// lines would then be laid out for a size they are not drawn at, and the text would either overflow
	// or leave a ragged gap down one side.
	intraFontSetStyle(font, size, 0xFFFFFFFF, 0, INTRAFONT_ALIGN_LEFT);
	float width = intraFontMeasureText(font, text);

	// The measure path binds the font atlas too, so the manager's idea of the bound texture is stale
	// afterwards -- the same fix, for the same reason, as after a print.
	TextureManager::Instance()->InvalidateCurrentTexture();
	return width;
}

void Screen::Panel(float x, float y, float w, float h, unsigned int argb)
{
	if (!ready)
	{
		return;
	}

	/* Two vertices, GU_SPRITES: the GE reads them as opposite corners of a rectangle.
	 *
	 * The storage is carved out of the command list, and that is a fix rather than a style choice.
	 *
	 * GU_DIRECT does not copy vertices: it stores the POINTER, and the GE reads that memory when it
	 * renders the list -- long after this function has returned. A local array would therefore be a
	 * dangling read, which is why this used to be a static one. But a single static array is ONE buffer,
	 * and every call overwrote it. Four panels drawn back to back -- the gallery cursor's four edges --
	 * all pointed at the same two vertices, so at render time all four read whichever was written last,
	 * and the cursor drew as a single line: its right edge. That is exactly what it did on real
	 * hardware.
	 *
	 * It only bit in a burst, which is why the rest of the interface looked right: an isolated panel is
	 * read long before the next call overwrites the buffer. And it was invisible in PPSSPP, which
	 * snapshots the vertices as each command is submitted rather than leaving the read until render
	 * time, so the emulator dutifully drew all four edges. A difference that shows up on hardware only.
	 *
	 * sceGuGetMemory takes the vertices from the current display list, so every call gets its own and
	 * their lifetime is the list's -- which is precisely the lifetime the GE needs. */
	FlatVertex* quad = (FlatVertex*)sceGuGetMemory(2 * sizeof(FlatVertex));
	if (quad == 0)
	{
		// The list is full. Drawing nothing is the only honest option, and the caller carries on.
		return;
	}

	quad[0].x = x;			quad[0].y = y;			quad[0].z = 0.0f;	quad[0].color = argb;
	quad[1].x = x + w;		quad[1].y = y + h;		quad[1].z = 0.0f;	quad[1].color = argb;

	sceGuDisable(GU_TEXTURE_2D);
	sceGuEnable(GU_BLEND);

	/* sceGumDrawArray, not sceGuDrawArray, and the difference is the whole reason this used to draw
	 * nothing at all.
	 *
	 * The GUI keeps the current matrices in its own registers, and only the sceGum* family flushes the
	 * software matrix stack into them. sceGuDrawArray draws with whatever is already in those registers,
	 * which at this point is not the identity -- it is the model matrix left behind by the last sprite
	 * drawn, i.e. that dirt tile's translation. A rectangle at (8, 8) then lands at (8 + 448, 8 + 256),
	 * far off the screen, and the panel is simply never seen. Going through sceGum refreshes the
	 * registers first, which is what every other sprite in this file already does.
	 */
	sceGumDrawArray(GU_SPRITES, GU_TRANSFORM_3D | GU_VERTEX_32BITF | GU_COLOR_8888, 2, 0, quad);

	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_TEXTURE_2D);

	TextureManager::Instance()->InvalidateCurrentTexture();
}

/*
 * The button sprite of a given width, from the cache, built and scaled the first time it is asked for.
 */
Sprite* Screen::CachedButton(float width, bool selected)
{
	for (unsigned int i = 0; i < buttonCache.size(); ++i)
	{
		if (buttonCache[i].width == width)
		{
			return selected ? buttonCache[i].sel : buttonCache[i].normal;
		}
	}

	ButtonPair pair;
	pair.width = width;
	pair.normal = new Sprite(buttonTexture, 0, 0, 95, 12);
	pair.sel = new Sprite(buttonTexture, 0, 12, 95, 12);

	// Once each: Scale bakes into the vertices, so a second call multiplies the width again rather than
	// reapplying it. Same rule as the sprites in LoadArt.
	float scaleX = width / 95.0f;
	if (pair.normal != 0) { pair.normal->Scale(scaleX, BUTTON_SCALE_Y); }
	if (pair.sel != 0)    { pair.sel->Scale(scaleX, BUTTON_SCALE_Y); }

	buttonCache.push_back(pair);
	return selected ? pair.sel : pair.normal;
}

void Screen::Button(float cx, float cy, const char* label, bool selected)
{
	ButtonW(cx, cy, 190.0f, label, selected);
}

void Screen::ButtonW(float cx, float cy, float width, const char* label, bool selected, bool enabled)
{
	if (!ready || buttonTexture < 0)
	{
		return;
	}

	Sprite* sprite = CachedButton(width, selected);
	if (sprite == 0)
	{
		return;
	}

	/* The alpha is set on EVERY call rather than only when dimming, because the sprites are cached and
	 * shared by width: a button drawn unavailable once would otherwise leave that width dimmed for every
	 * later draw that reused it.
	 *
	 * The cursor keeps the sprite at almost full strength even when the button cannot act. Dimming it
	 * with everything else is what would hide the cursor, and the cursor being visible is the point of
	 * the fix here -- unavailability is carried by the grey label instead, and only backed up by the
	 * sprite when the button does not have the cursor. The four levels this produces are spelled out
	 * above BUTTON_DIM_ALPHA. */
	float alpha = 1.0f;
	if (!enabled)
	{
		alpha = selected ? BUTTON_DIM_ALPHA_FOCUSED : BUTTON_DIM_ALPHA;
	}
	sprite->Alpha(alpha);

	sceGuEnable(GU_BLEND);
	sprite->SetPosition(cx, cy);
	sprite->Draw();
	sceGuDisable(GU_BLEND);

	// A label that would overrun the button is shrunk to fit, and the baseline offset is then recomputed
	// from the smaller size so the text stays on the button's middle line instead of sliding below it.
	// 14 px of the width is kept as margin so the text never touches the button's edge.
	float size = FONT_SIZE;
	float usable = width - 14.0f;
	float textWidth = MeasureText(size, label);
	if (textWidth > usable && textWidth > 0.0f)
	{
		size = size * (usable / textWidth);
	}

	PrintText(font, size, cx, cy + BUTTON_LABEL * (size / FONT_SIZE), label,
		enabled ? 0xFFFFFFFF : BUTTON_DIM_LABEL, INTRAFONT_ALIGN_CENTER);
}

bool Screen::LoadPanelImage(const char* path, float boxWidth, float boxHeight)
{
	if (!ready)
	{
		return false;
	}

	// Released first, so the new image lands at the END of the texture list, which is the only place
	// RemoveTexture can take it from without shifting a handle the UI still holds. See Screen.h.
	FreePanelImage();

	if (path == 0 || path[0] == '\0' || !FilePresent(path))
	{
		return false;
	}

	TextureManager::Instance()->LoadTexture(path);
	int number = TextureManager::Instance()->GetTextureNumber(path);
	if (number < 0)
	{
		return false;
	}

	int imgW = TextureManager::Instance()->getWidth(number);
	int imgH = TextureManager::Instance()->getHeight(number);
	if (imgW <= 0 || imgH <= 0)
	{
		TextureManager::Instance()->RemoveTexture(number);
		return false;
	}

	panelSprite = new Sprite(number, 0, 0, imgW, imgH);
	if (panelSprite == 0)
	{
		TextureManager::Instance()->RemoveTexture(number);
		return false;
	}

	// Fitted rather than stretched to the box. The box is a fixed shape and a source image will not
	// usually match it, so the scale is the smaller of the two ratios -- which letterboxes the image
	// inside its panel rather than distorting it.
	float scaleW = boxWidth / (float)imgW;
	float scaleH = boxHeight / (float)imgH;
	float scale = (scaleW < scaleH) ? scaleW : scaleH;
	panelSprite->Scale(scale, scale);

	panelTexture = number;
	Logger::Instance()->LogMessage("screen: panel image '%s' %dx%d as %d, scale %.3f\n",
		path, imgW, imgH, number, scale);
	return true;
}

void Screen::FreePanelImage()
{
	if (panelSprite != 0)
	{
		delete panelSprite;
		panelSprite = 0;
	}
	if (panelTexture >= 0)
	{
		TextureManager::Instance()->RemoveTexture(panelTexture);
		panelTexture = -1;
	}
}

void Screen::DrawPanelImage(float cx, float cy)
{
	if (!ready || panelSprite == 0)
	{
		return;
	}

	sceGuEnable(GU_BLEND);
	panelSprite->SetPosition(cx, cy);
	panelSprite->Draw();
	sceGuDisable(GU_BLEND);
}

/*
 * The gallery strip. Fitted into the given box the same way a single preview is, so a short strip and
 * a long one are made of the same shaped tiles rather than each being scaled to fill the row.
 *
 * A path that will not load is skipped, not fatal: the strip is built from whatever of the release's
 * shots are actually on the card, and one missing file should not cost the others.
 */
bool Screen::LoadStrip(const std::vector<std::string>& paths, float boxWidth, float boxHeight)
{
	if (!ready)
	{
		return false;
	}

	FreeStrip();

	for (unsigned int i = 0; i < paths.size(); ++i)
	{
		const std::string& path = paths[i];
		if (path.empty() || !FilePresent(path.c_str()))
		{
			continue;
		}

		TextureManager::Instance()->LoadTexture(path);
		int number = TextureManager::Instance()->GetTextureNumber(path);
		if (number < 0)
		{
			continue;
		}

		int imgW = TextureManager::Instance()->getWidth(number);
		int imgH = TextureManager::Instance()->getHeight(number);
		if (imgW <= 0 || imgH <= 0)
		{
			// The texture was created by the call above, so it has to go again -- but only if it is
			// still the last entry, which it is because nothing else has loaded since.
			TextureManager::Instance()->RemoveTexture(number);
			continue;
		}

		Sprite* sprite = new Sprite(number, 0, 0, imgW, imgH);
		if (sprite == 0)
		{
			TextureManager::Instance()->RemoveTexture(number);
			continue;
		}

		float scaleW = boxWidth / (float)imgW;
		float scaleH = boxHeight / (float)imgH;
		float scale = (scaleW < scaleH) ? scaleW : scaleH;
		sprite->Scale(scale, scale);

		stripSprites.push_back(sprite);
		stripTextures.push_back(number);
	}

	Logger::Instance()->LogMessage("screen: strip built, %d of %d images\n",
		(int)stripSprites.size(), (int)paths.size());
	return !stripSprites.empty();
}

void Screen::FreeStrip()
{
	// Reverse order, so each removal takes the last entry and no live handle is shifted. See the note
	// on LoadStrip and on RemoveTexture.
	for (int i = (int)stripSprites.size() - 1; i >= 0; --i)
	{
		delete stripSprites[i];
		stripSprites[i] = 0;
		if (stripTextures[i] >= 0)
		{
			TextureManager::Instance()->RemoveTexture(stripTextures[i]);
		}
	}
	stripSprites.clear();
	stripTextures.clear();
}

void Screen::DrawStripItem(int index, float cx, float cy)
{
	if (!ready || index < 0 || index >= (int)stripSprites.size())
	{
		return;
	}

	Sprite* sprite = stripSprites[index];
	if (sprite == 0)
	{
		return;
	}

	sceGuEnable(GU_BLEND);
	sprite->SetPosition(cx, cy);
	sprite->Draw();
	sceGuDisable(GU_BLEND);
}

float Screen::IconSize() const
{
	return ICON_BOX;
}

void Screen::Icon(int index, float cx, float cy)
{
	if (!ready || iconTexture < 0 || index < 0 || index >= ICON_COUNT)
	{
		return;
	}

	Sprite* sprite = iconSprites[index];
	if (sprite == 0)
	{
		return;
	}

	// cx/cy are the icon's CENTRE, which is what Sprite::SetPosition means for the centred constructor
	// these were built with -- not a top-left corner. It is also the convention Button and DrawPanelImage
	// use, so a caller placing one of these has nothing new to remember.
	sceGuEnable(GU_BLEND);
	sprite->SetPosition(cx, cy);
	sprite->Draw();
	sceGuDisable(GU_BLEND);
}

void Screen::Scissor(float x, float y, float w, float h)
{
	if (!ready)
	{
		return;
	}

	// sceGuScissor takes a corner and a SIZE, not two corners -- see its prototype in pspgu.h, and the
	// full-screen call in SetupGu that passes the screen's dimensions. Passing x+w and y+h here would
	// shift the rectangle's far edge outwards instead of placing it, which is a mistake that only shows
	// up where the clip actually bites.
	sceGuScissor((int)x, (int)y, (int)w, (int)h);
}

void Screen::ScissorOff()
{
	if (!ready)
	{
		return;
	}

	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
}

void Screen::Bar(float x, float y, float width, float progress)
{
	if (!ready || progress < 0.0f)
	{
		return;
	}

	float filled = width * progress;
	if (filled > width)
	{
		filled = width;
	}
	if (filled < 1.0f)
	{
		filled = 1.0f;
	}

	// Positioned first and drawn after, because Sprite::Draw pushes the current matrix and the scale is
	// applied to the vertices rather than to the matrix.
	barBackSprite->NewScale(width);
	barBackSprite->SetPosition(x, y);

	barSprite->NewScale(filled);
	barSprite->SetPosition(x, y);

	barBackSprite->Draw();
	barSprite->Draw();
}

// ---------------------------------------------------------------------------------------------
// The one-screen form, in terms of everything above
// ---------------------------------------------------------------------------------------------

void Screen::Frame(const char* status, const char* detail, const char* prompt, float progress)
{
	if (!ready)
	{
		return;
	}

	// Blend is enabled by BeginFrame and the bar wants it, so it is deliberately not touched here --
	// the bar's track is a semi-transparent sprite and disabling blend around it would draw it solid.
	BeginFrame();

	Bar(BAR_X, BAR_Y, BAR_WIDTH, progress);

	Text(240, STATUS_Y, status);
	Text(240, DETAIL_Y, detail);
	Text(240, PROMPT_Y, prompt);

	EndFrame();
}
