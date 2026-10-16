#ifndef DEBUGMENU_H_
#define DEBUGMENU_H_

#include <string>
#include <vector>

namespace Aurora
{
	namespace Graphics
	{
		class RenderManager;
		class Sprite;
	}

	// Must be declared inside its namespace: a global forward declaration would sit alongside the
	// real Aurora::System::SystemManager and make every unqualified use of the name ambiguous in
	// every file that has a using-directive for it.
	namespace System
	{
		class SystemManager;
	}
}

class SoundManager;

// A reusable in-game option menu, driven by the debug stick. The caller supplies a title and the
// button labels; the menu owns navigation, paging and drawing and hands back the index the player
// picked. It knows nothing about mobs, blocks, or what any option means -- that is the entire point,
// because the same menu then serves a skeleton, a redstone lamp, or a command that wants to ask a
// question.
//
// Caller contract:
//
//     menu.Open("Skeleton", options);
//     while(menu.IsOpen())
//     {
//         menu.Update(dt, sys, sound);    // navigation, confirm, cancel
//         menu.Render(render);            // dim, title, buttons, page counter
//     }
//     int picked = menu.ConsumeResult();  // >= 0 index, or RESULT_CANCELLED
//
// The menu closes itself as soon as the player decides, which is deliberate: a debug action is
// usually something you have to *watch*, and a paused world will not animate. Pausing is the caller's
// business -- it already owns its own menu/pause state -- so the menu only reports what was chosen.
//
// Layout is one column, centred. The screen is 480x272, so eight 190x24 buttons fit under a title and
// above a page counter; more than eight options page rather than running off the bottom of the
// screen, which is the requirement the buttons must never violate.
class DebugMenu
{
public:
	DebugMenu();
	~DebugMenu();

	// "No decision yet" while the menu is open -- not an error.
	static const int RESULT_NONE = -1;
	// The player backed out. Distinct from RESULT_NONE so a caller cannot mistake "closed myself"
	// for "still waiting".
	static const int RESULT_CANCELLED = -2;

	// Opens the menu on a fresh selection and page 1. Re-opening while already open is allowed and
	// resets the selection, which is what a second right-click should do.
	//
	// `body` is an optional sentence drawn between the title and the buttons, wrapped to the width of
	// the screen. It exists for the one-button message pages: a title of "Message" over a button
	// reading "OK" tells the player nothing, and the sentence the caller composed had nowhere to go
	// before this -- it was stored and then never read, so every message dialog in the game arrived
	// blank. Omitted, the layout is exactly what it has always been.
	void Open(const std::string& title, const std::vector<std::string>& options,
	          const std::string& body = std::string());
	void Close();

	bool IsOpen() const { return open; }

	// Navigation and confirm/cancel. Call once per frame while the menu is open.
	void Update(float dt, Aurora::System::SystemManager* sys, SoundManager* sound);

	// Draws the dim, the title, the buttons and the page counter. Call once per frame while open.
	void Render(Aurora::Graphics::RenderManager* render);

	// Returns the pick and clears it, so a caller polling every frame cannot act on it twice. Always
	// call it when the menu has closed, or the result sits there until the next Open.
	int ConsumeResult();

	// Layout readouts. Exposed for the guide and for any test that wants to assert the buttons stay
	// on screen rather than take a screenshot to find out.
	int PageCount() const;
	int PageSize() const { return kPageSize; }
	int CurrentPage() const { return page; }
	int SelectedIndex() const { return selected; }

	// Opt-in vertical centring, off by default. A debug-stick menu wants its title at the top and its
	// buttons under it, which is right for a list you scan down; a message with one button wants to
	// sit in the middle of the screen, which is right for something you read once and dismiss. A
	// member rather than a parameter of Open() because each state owns its own DebugMenu, so setting
	// it once on the instance cannot leak into another screen's menus.
	void SetCentred(bool value) { centred = value; }

private:
	int PageOf(int index) const;
	void ClampSelection();
	void EnsureSprites();

	bool open;
	bool centred;
	std::string title;
	// Optional sentence drawn between the title and the buttons. Empty for every menu that is a plain
	// list of choices; set for a message page, which has one button and needs to say why it is there.
	std::string body;
	std::vector<std::string> options;
	int selected;
	int page;
	int result;

	// Set by Open() and cleared by the first Update(). The play loop runs HandleEvents before
	// Update, so the very press that opened the menu is still down one Update later; without this
	// that press would read as a confirm and fire the first option immediately.
	bool swallowFirstFrame;

	// Analog-stick repeat hold-off. A member, not a function-local static: the two play states each
	// own a menu, and a shared static would let one state's navigation throttle the other's.
	float analogWait;

	Aurora::Graphics::Sprite* dimSprite;
	Aurora::Graphics::Sprite* buttonSprite;
	Aurora::Graphics::Sprite* buttonSelSprite;

	// One column, centred: 190x24 buttons (the 95x12 menu_elements cells at the 2x scale the pause
	// menu already uses), on a 26 px pitch. Eight of them run from y=38 to y=244, leaving the title
	// above and the page counter below inside the 272 px screen.
	static const int kPageSize = 8;
	static const float kButtonX;
	static const float kButtonY0;
	static const float kButtonPitch;
	static const float kButtonScaleX;
	static const float kButtonScaleY;
	static const float kTitleY;
	static const float kFooterY;

	// Button-label baseline offset per unit of font size. See the definition in DebugMenu.cpp.
	static const float kLabelBaseline;

	// Screen dim behind the menu, 0..1. See the definition in DebugMenu.cpp.
	static const float kDimAlpha;

	// Analog stick must cross this before it counts as a direction, and then hold off for kAnalogWait
	// seconds. Same feel as the pause menu, and it stops a resting stick from scrolling the menu.
	static const float kAnalogDead;
	static const float kAnalogWait;
};

#endif
