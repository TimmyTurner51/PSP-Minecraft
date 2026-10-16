#ifndef UPDATERAPP_SCREEN_H
#define UPDATERAPP_SCREEN_H

// The two types the interface mentions. Included here rather than forward-declared because Sprite is a
// class with a constructor this file's interface depends on, and intraFont is a typedef from C.
#include <Aurora/Graphics/Sprite.h>
#include <Aurora/Graphics/intraFont/intraFont.h>

// For the cache of buttons at non-default widths. See ButtonW.
#include <vector>

/*
 * The cells of title/buttons.png, named.
 *
 * The launcher asks for ICON_CROSS rather than for cell 2, so the order of the art is stated in one
 * place instead of being implied by two. tools/make-button-icons.py writes the sheet in this order and
 * repeats it in its own comment: the two are the only copies, and they are one line each.
 */
enum { ICON_TRIANGLE = 0, ICON_CIRCLE, ICON_CROSS, ICON_SQUARE, ICON_L, ICON_R, ICON_COUNT };


/*
 * The Minecraft look, as a small set of drawing primitives.
 *
 * WHAT THIS IS NOW
 * ----------------
 * It began as a single fixed screen: one status line, one detail line, one bar, drawn by one function
 * called Frame(). That was right for a program whose whole life was "show progress, install, exit" --
 * there was exactly one thing to draw and nothing to navigate.
 *
 * The Launcher is a different program with the same look. It has a title, a list you move through, a
 * set of buttons, and only sometimes a bar. Drawing that through Frame() would mean either growing
 * Frame() a parameter per widget or giving the Launcher a second renderer, and a second renderer is
 * how a program ends up with two subtly different ideas of what a button looks like. So the fixed
 * screen is now built out of the pieces, and the pieces are what this class publishes:
 *
 *     BeginFrame();                      // matrices, clear, tiled dirt
 *     Text(240, 16, "Minecraft Launcher");
 *     Button(130, 62, "Minecraft 3.0.1", true);
 *     Bar(160, 150, 160, progress);
 *     EndFrame();                        // submit, wait for vblank, swap
 *
 * Frame() survives as a four-line wrapper over exactly these calls, because the update path still
 * wants the old screen and because keeping it means that screen cannot drift from the launcher's.
 *
 * THE WIDGETS ARE THE GAME'S, DOWN TO THE PIXEL
 * ---------------------------------------------
 * A button here is the game's button: title/menu_elements.png, the 95x12 sub-rect at (0,0) for the
 * normal state and the one at (0,12) for the selected state, drawn at scale 2 so it is 190x24 on
 * screen, with the label centred and sitting 9 px below the button's centre at the game's 0.5 font
 * size. Those numbers are not invented here -- they are DebugMenu's kButtonScaleX/Y, kButtonPitch and
 * kLabelBaseline, which are themselves calibrated against the pause menu. A button that is one pixel
 * off reads as a different program, which is the whole thing this class exists to avoid.
 *
 * TEXT IS SHADOWED, and that is also the game's choice rather than a flourish. These labels sit on a
 * dirt texture whose brightness varies from tile to tile, so an unshadowed thin font loses contrast
 * across the screen; DebugMenu draws every label twice, once black at y+1 and once white at y, and
 * this does the same for the same reason.
 *
 * WHY IT IS STILL NOT RenderManager
 * ---------------------------------
 * Unchanged, and worth restating because the temptation is now stronger: the Launcher draws far more
 * than the updater did, and RenderManager would make all of it pixel-exact for free. It is still
 * refused. RenderManager brings the game's camera, model, matrix, world and VRAM subsystems to draw
 * two sprites and a line of text, and this program is the one that has to work when the game does not
 * -- including when the game has just failed to boot, which is precisely when a player reaches for the
 * launcher. Every dependency added here is another reason the repair tool can fail alongside the thing
 * it repairs.
 *
 * So it speaks GU directly: about eighty lines of setup lifted from RenderManager::Init so the
 * configuration is the game's rather than a guess. What it shares with the game it shares as SOURCE --
 * TextureManager, Sprite, intraFont, and the PNG fix in TextureManager.cpp -- not as a link.
 *
 * WHAT IT DOES WHEN IT CANNOT DRAW
 * --------------------------------
 * Init() can fail, and it will fail on a card with no Assets folder -- which is exactly the card most
 * likely to be running a repair tool. It is not allowed to be fatal. Init() returns false and
 * everything here becomes a no-op; main.cpp keeps pspDebugScreen up as the fallback and prints the
 * same messages through that. A screen is a courtesy, and this one never becomes a requirement.
 */
class Screen
{
public:
	static Screen* Instance();

	/*
	 * Sets up GU, loads the textures and the font. Any failure returns false and leaves the object
	 * unusable, with a line in Log.txt naming what was missing. Safe to call once; later calls return
	 * the first result.
	 */
	bool Init();

	// True once Init() has succeeded, i.e. the draw calls below do anything.
	bool Ready() const { return ready; }

	// ---------------------------------------------------------------------------------------
	// A frame the caller composes. BeginFrame clears and tiles the dirt; EndFrame submits and
	// presents. Everything drawn between them must be drawn in that order -- sprites first, text
	// very last -- because intraFont binds its own texture and every sprite drawn after a text call
	// would sample the font atlas instead of its own image. EndFrame rebinds to be safe, but
	// ordering the calls correctly is what actually keeps it right.
	// ---------------------------------------------------------------------------------------
	void BeginFrame();
	void EndFrame();

	// ---------------------------------------------------------------------------------------
	// Primitives. Coordinates are pixels with the origin top-left, so these are the same numbers
	// used in LoadingScreen and StateUpdate. x is a CENTRE for Text() and for Button(), and a LEFT
	// EDGE for TextLeft() and Bar().
	// ---------------------------------------------------------------------------------------

	// White text with a black drop shadow, centred on x.
	void Text(float x, float y, const char* text);

	// The same, left-aligned at x. For values that change width (sizes, versions) so they do not
	// wander sideways as they update.
	void TextLeft(float x, float y, const char* text);

	// Yellow, centred. Used for the row the cursor is on when it is not a button of its own.
	void TextHighlight(float x, float y, const char* text);

	// The same, right-aligned at x. For the menu's notice line, which shares its row with the status over
	// the version button: see the note where it is drawn for the measurement that forced it off centre.
	void TextHighlightRight(float x, float y, const char* text);

	// A Minecraft button, 190x24, centred on (cx, cy). `selected` picks the lit halves of the sprite;
	// it is not a focus ring, it is the button's own two states, exactly as DebugMenu uses them.
	void Button(float cx, float cy, const char* label, bool selected);

	// The same button at a chosen on-screen width.
	//
	// The width has to be a parameter now because the bottom row holds a version dropdown, Play and
	// Delete, and three 190 px buttons do not fit in 480 px. Each distinct width costs its own sprite:
	// Sprite::Scale bakes the scale into the vertices and NewScale only moves one edge, which is how
	// the progress bar grows but not how a centred button is made. The sprites are cached by width
	// rather than rebuilt per frame.
	//
	// `selected` and `enabled` are TWO questions and were once one, which was a bug worth spelling out.
	// selected is "the cursor is on this button"; enabled is "this button can act". They were ANDed at
	// the call site, so a button that could not act also could not light up -- and when the selected
	// version was not installed that meant pressing right moved the cursor off the version button and
	// nothing lit up anywhere, because both Play and Delete were refusing to show the cursor. The row
	// looked frozen on the one button that was always available.
	//
	// A button that is focused and cannot act now draws the lit sprite at reduced alpha with a grey
	// label: it reads as "you are here, and this will not do anything", which is two facts rather than
	// one, and it is why the cursor never disappears.
	//
	// A label too wide for the button is shrunk to fit, the same way DebugMenu does it, so a long
	// version string degrades to smaller text instead of spilling over the edge.
	void ButtonW(float cx, float cy, float width, const char* label, bool selected,
		bool enabled = true);

	// A solid rectangle. The changelog sits on one so the text has a known background instead of
	// whatever shade of dirt happens to be under each glyph.
	void Panel(float x, float y, float w, float h, unsigned int argb);

	// Text at an arbitrary size and alignment, and its width in pixels.
	//
	// Text() is fixed at the game's 0.5 and centred, which is right for labels and wrong for a
	// paragraph of changelog that has to be wrapped and left-aligned.
	void TextSized(float x, float y, float size, unsigned int color, int align, const char* text);
	float MeasureText(float size, const char* text);

	// The Load bar. `progress` is 0..1; NEGATIVE draws nothing at all, which is deliberately
	// different from an empty track: an empty track beside work that is happening reads as a hang,
	// and the game's own screen makes the same distinction. The filled half is floored at one pixel
	// so a step that has started looks different from one that has not.
	void Bar(float x, float y, float width, float progress);

	// ---------------------------------------------------------------------------------------
	// The one-screen form, kept for the update path. This is BeginFrame + dirt + bar + three lines
	// + EndFrame, in that order, and it is only a convenience: it is defined in terms of the calls
	// above so the two cannot drift.
	//
	//   status    big centred line, y=100. The step being performed.
	//   detail    smaller line under the bar, y=172. May be NULL for a blank line.
	//   prompt    the bottom line, y=250. May be NULL.
	// ---------------------------------------------------------------------------------------
	void Frame(const char* status, const char* detail, const char* prompt, float progress);

	// The screen the caller draws into, for the one thing the primitives cannot express: a dialog
	// drawn over a still-visible menu. Returns the dimensions so callers need not hardcode them.
	static void Dimensions(int* width, int* height) { *width = 480; *height = 272; }

	// ---------------------------------------------------------------------------------------
	// One image, for the preview beside the changelog.
	//
	// Loaded on demand and released before the next one is loaded, because the heap is the constraint
	// here rather than the card: a 480x272 image costs about 1 MB as a texture once it is padded to a
	// power of two, and the update path needs roughly 5 MB of contiguous heap of its own. Holding a
	// preview while an install runs is what would turn a working update into a failed allocation, so
	// the launcher frees the image before it starts one.
	//
	// The image must be the LAST texture loaded. TextureManager::RemoveTexture erases the entry and
	// shifts every later index down, so releasing it while later textures existed would leave the UI's
	// own sprite handles pointing one place too far along. Loading it after all of them is what makes
	// that safe, and FreePanelImage is the only thing that unloads one.
	// ---------------------------------------------------------------------------------------
	bool LoadPanelImage(const char* path, float boxWidth, float boxHeight);
	void FreePanelImage();
	bool HasPanelImage() const { return panelSprite != 0; }

	// Drawn centred on (cx, cy), at the size it was fitted to when it was loaded.
	void DrawPanelImage(float cx, float cy);

	// ---------------------------------------------------------------------------------------
	// The gallery strip.
	//
	// A release can carry several screenshots, and the menu shows them as a ROW rather than one at a
	// time, so what a build looks like is visible without stepping into the viewer to find out. The
	// selected one is what the viewer opens.
	//
	// They are all loaded at once rather than swapped as the selection moves. A card read and a texture
	// upload behind every press of Left and Right is a stutter in the one place the player is browsing
	// quickly, and a release ships a handful of shots at most. Each costs about half a megabyte once
	// padded to a power of two, so a strip of three is well inside what the launcher has spare.
	//
	// ORDER MATTERS, for the same reason as the panel image above: these must be the LAST textures
	// loaded, because TextureManager::RemoveTexture shifts every later index down. LoadStrip loads
	// after everything else and FreeStrip releases in reverse, so each removal takes the last entry.
	// FreeStrip also has to run before LoadPanelImage, or the panel would not be last.
	// ---------------------------------------------------------------------------------------
	bool LoadStrip(const std::vector<std::string>& paths, float boxWidth, float boxHeight);
	void FreeStrip();
	int StripCount() const { return (int)stripSprites.size(); }
	bool HasStrip() const { return !stripSprites.empty(); }

	// Drawn centred on (cx, cy). Bounds-checked, so a caller that has lost track of the count draws
	// nothing rather than reaching into the vector.
	void DrawStripItem(int index, float cx, float cy);

	// ---------------------------------------------------------------------------------------
	// Button prompt icons.
	//
	// A game does not write "press X" -- it draws the button. The eye recognises a shape without
	// reading it, which is the whole reason the convention exists, and it costs nothing to follow.
	//
	// The art is one sheet of uniform cells cut by tools/make-button-icons.py; see the note on the
	// ICON_* enum above. Icons are drawn at IconSize() square, which is sized to sit on a line of the
	// game's 0.5 text rather than to be looked at on its own.
	//
	// HasIcons() is false when the sheet is missing, and every caller is expected to fall back to
	// naming the button in words. That is deliberate: this is the one piece of art whose absence is
	// not worth refusing to start over, because the screen still works without it. See LoadArt.
	// ---------------------------------------------------------------------------------------
	bool HasIcons() const { return iconTexture >= 0; }
	float IconSize() const;
	void Icon(int index, float cx, float cy);

	// ---------------------------------------------------------------------------------------
	// Clipping, for a scrolling region.
	//
	// The launcher's top window scrolls, so a screenshot has to be cut off at the window's top edge
	// rather than drawn over the dirt above it. GU has a scissor for exactly that and it is already
	// enabled in the setup below; what was missing was a way to move it. ScissorOff restores the full
	// screen and has to be called before drawing anything meant to be outside the rectangle -- the
	// bottom row, for one.
	//
	// This only clips what is drawn after it, so the caller controls the ordering rather than the
	// Screen tracking a stack of rectangles.
	// ---------------------------------------------------------------------------------------
	void Scissor(float x, float y, float w, float h);
	void ScissorOff();

	// ---------------------------------------------------------------------------------------
	// A frame written to a file, for looking at a screen that cannot be screenshotted.
	//
	// This exists because capturing the emulator from outside is not reliable here. Reading the screen
	// -- CopyFromScreen -- returns a stale DWM composite whenever the emulator is not genuinely in
	// front, and the failure is invisible: the capture succeeds and holds a picture of some other
	// window. Asking the window to paint itself (PrintWindow) returns solid black, because PPSSPP
	// renders through D3D11 and has no GDI surface to paint. Both were measured, not assumed.
	//
	// Reading the framebuffer from inside is not subject to any of that. It is also the only method
	// that will work on the console itself, where there is no desktop to capture at all -- so this is
	// the mechanism to reach for when a screen needs to be seen, on hardware or on the emulator.
	//
	// Dumping is OFF unless a switch file is next to the launcher (DUMP_SWITCH), so a shipped build
	// never writes to the card and never pays the cost of a 400 KB write per call.
	// ---------------------------------------------------------------------------------------
	// Which buffer to read. DUMP_DISPLAY asks the display API; the other two read VRAM directly. See
	// the note on DumpFrame in Screen.cpp for why the display API is not enough on its own.
	enum { DUMP_DISPLAY = 0, DUMP_FRONT = 1, DUMP_BACK = 2 };
	void DumpFrame(const char* fileName, int source = 0);
	bool DumpingEnabled() const { return dumping; }

	// The switch file's own name, so a caller can read its CONTENTS -- which screen to start on -- without
	// this class needing to know anything about the menu's levels. See the note in Launcher.cpp for why
	// the contents matter: the dumps only ever show the frame the program started on.
	static const char* DumpSwitchName();

private:
	Screen();
	~Screen();
	Screen(const Screen&);
	Screen& operator=(const Screen&);

	// The texture pack folder, from Config/texturepack.cf, with the game's default when it is absent.
	// Only the folder -- not the whole path -- because TextureManager is given complete paths.
	static void PackFolder(char* out, unsigned int size);

	bool SetupGu();
	bool LoadArt(const char* dirtPath, const char* loadPath, const char* menuPath,
		const char* iconPath);
	bool LoadFont();

	bool ready;
	bool dumping;	// true when DUMP_SWITCH is present: DumpFrame() is allowed to write

	Aurora::Graphics::Sprite* backSprite;		// title/dirt.png, 32x32, drawn 2x
	Aurora::Graphics::Sprite* barSprite;		// title/load.png rows 0-7, the filled half
	Aurora::Graphics::Sprite* barBackSprite;	// title/load.png rows 8-15, the track
	Aurora::Graphics::Sprite* buttonSprite;		// title/menu_elements.png (0,0,95,12), the resting state
	Aurora::Graphics::Sprite* buttonSelSprite;	// title/menu_elements.png (0,12,95,12), the lit state

	// Buttons at widths other than 190, one pair per distinct width, built the first time each is
	// asked for. See the note on ButtonW: the scale lives in the vertices, so a width needs its own
	// sprite rather than a rescale.
	struct ButtonPair
	{
		float width;
		Aurora::Graphics::Sprite* normal;
		Aurora::Graphics::Sprite* sel;
	};
	std::vector<ButtonPair> buttonCache;
	Aurora::Graphics::Sprite* CachedButton(float width, bool selected);
	int buttonTexture;		// title/menu_elements.png, the image those pairs are cut from

	// The preview image. panelTexture is the number it was loaded at, needed to release it again.
	Aurora::Graphics::Sprite* panelSprite;
	int panelTexture;

	// The gallery strip: one sprite and one texture number per screenshot. stripTextures is kept so the
	// textures can be released; the sprites are only the vertices, which are cheap.
	std::vector<Aurora::Graphics::Sprite*> stripSprites;
	std::vector<int> stripTextures;

	// The button prompt icons, one sprite per cell of title/buttons.png, all cut at load time. iconTexture
	// is their shared number and is -1 when the sheet is absent, which is what HasIcons reports.
	Aurora::Graphics::Sprite* iconSprites[ICON_COUNT];
	int iconTexture;

	intraFont* font;
};

#endif
