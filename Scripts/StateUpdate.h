#ifndef STATEUPDATE_H_
#define STATEUPDATE_H_

#include <stdlib.h>
#include <string>

#include <Aurora/Utils/StateManager.h>
#include <Aurora/Utils/GameState.h>
#include <Aurora/Utils/Timer.h>
#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/System/SystemManager.h>
#include <Aurora/Graphics/Sprite.h>

#include "DebugMenu.h"
#include "SoundManager.h"
#include "Updater.h"

// The online updater screen, pushed onto the menu stack from the title screen.
//
// It is a state of its own rather than a branch inside StateMenu for two reasons. StateMenu is a
// 4,000-line switch with three near-duplicate copies of its own navigation, and the update flow needs
// to *block* -- a check and a multi-megabyte download both take real time, and the frame has to keep
// being drawn while they do. That is much easier to reason about when it owns its own Update/Draw.
//
// The dialogs are DebugMenu, which is already the game's Minecraft-styled button menu (the
// menu_elements cells at the pause menu's 2x scale) and knows nothing about what any option means.
// So the flow here is just "open a menu, read the index the player picked, do the thing".
//
// There is one installation path, and it is the Updater EBOOT. This screen downloads and verifies the
// package, then hands the whole job to the Updater and leaves the game.
//
// There used to be a choice here -- "Automatic", which tried to apply the package in place, and
// "Manual", which only staged it -- and Automatic was the one that worked out to a hand-off anyway.
// Both are gone, for two reasons that turned out to be the same reason:
//
//   - The running game cannot replace EBOOT.PBP, so any package that changes code has to go to the
//     Updater regardless. That is every release that matters.
//   - "Apply in place" never actually avoided a restart: art written under a running game still needs
//     the game restarted before it is read, which is why the in-place path ended by telling the player
//     to restart. So the in-place path cost the same restart and did it without a progress screen,
//     and with a second copy of install code to keep correct.
//
// Which leaves the Updater as the only thing that installs anything, and one flow to reason about:
// download here (with this screen's bar), then the Updater's own screen, then the menu.
class StateUpdate : public Aurora::Utils::CGameState
{
public:
	StateUpdate();
	virtual ~StateUpdate();

	void Init();
	void Enter();
	void CleanUp();

	void Pause();
	void Resume();

	void HandleEvents(Aurora::Utils::StateManager* sManager);
	void Update(Aurora::Utils::StateManager* sManager);
	void Draw(Aurora::Utils::StateManager* sManager);

private:
	enum Step
	{
		STEP_MENU,       // "Check for update" / "Back"
		STEP_AVAILABLE,  // "Install now" / "Later"
		STEP_BUSY,       // a blocking operation is running; see PumpProgress
		STEP_RESULT,     // one message, "OK"
		STEP_LEAVE       // pop back to the menu
	};

	Step step;
	DebugMenu menu;

	Sprite* backSprite;
	Sprite* barSprite;
	Sprite* barBackSprite;

	Aurora::Graphics::RenderManager* mRender;
	Aurora::System::SystemManager* mSystemMgr;
	SoundManager* mSoundMgr;
	Aurora::Utils::Timer mTimer;
	float dt;

	std::string statusLine;    // the line under the bar while busy
	// The result sentence is not kept here: it is handed straight to the menu as that page's body,
	// which is the only thing that ever consumed it. A member for it was written and never read, and
	// that is why every message page arrived blank.
	std::string baseUrl;       // where update.txt and the package are fetched from
	Update::Offer offer;       // what the server says is available
	unsigned int progressDone;
	unsigned int progressTotal;
	// Whether Update::NetInit() got a usable stack. Retried on each check when it did not, because
	// the usual reason it fails is free memory and closing a world gives that back.
	bool netReady;

	// The blocking calls in Updater.h draw and poll input through these. One adapter, because only the
	// transfer path both draws our own screen and polls our own cancel; the system dialogs go through
	// ClearDialogThunk/PresentDialogThunk instead.
	static bool ProgressThunk(void* ctx, unsigned int done, unsigned int total);

	// Paints one frame of the busy screen: the tiled background, the bar, the status line and the byte
	// count. No input and no decision; the caller does the polling. Only correct when no system dialog
	// is up -- see ClearDialogFrame for why.
	void DrawProgressFrame();

	// The two halves of a system dialog frame, and they must not be merged into one call.
	//
	// These dialogs draw themselves, on their own graphics threads, during sceUtility*Update. That puts
	// the pump between the halves: the buffer has to be cleared *before* the dialog draws into it, and
	// presented *after*. Clearing afterwards erases the frame that was just drawn and displays the
	// blank buffer in its place -- which is the tearing, and why the game's own clear colour bleeds
	// through the tear.
	//
	// Nothing of the game's own screen may be painted here. Painting into the framebuffer the dialog's
	// threads are writing fights them for it; the only correct contribution between updates is clear,
	// finish, sync, wait for vblank, swap -- exactly what SystemManager::ShowNetworkDialog does.
	void ClearDialogFrame();
	void PresentDialogFrame();

	// Frame callbacks for a system dialog. Neither cancels: the dialog owns its own cancel button, and
	// polling ours would read the same press a second time.
	static bool ClearDialogThunk(void* ctx);
	static bool PresentDialogThunk(void* ctx);

	// Draws one frame of the busy screen and returns false if the player asked to cancel. Used for
	// every blocking step, including the system connection dialog, so cancelling is always the same
	// button and the screen never appears frozen.
	bool PumpProgress();

	// The base URL, read from update_url.txt beside the EBOOT so pointing the game at a different
	// machine never needs a rebuild. Falls back to a documented default.
	std::string ReadBaseUrl();

	// update_url.txt verbatim and trimmed, or "" when the file is absent. Separate from ReadBaseUrl
	// because "nothing saved yet" and "saved, and it happens to be the placeholder" have to be
	// distinguishable -- ReadBaseUrl substitutes the default for both.
	std::string ReadSavedUrl();

	// Writes update_url.txt. Truncating, because a shorter address written over a longer one would
	// otherwise leave the tail of the old one behind and produce a URL that is neither.
	bool SaveBaseUrl(const std::string& url);

	// The host part of a URL: no scheme, no port, no path. "" if there is nothing usable in it.
	static std::string AddressFromUrl(const std::string& url);

	// True when an address has been saved and it is not the placeholder, i.e. someone has actually
	// chosen one. This is what makes the first "Check for update" ask for an address.
	bool HasServerAddress();

	// The system on-screen keyboard. Blocking, like every other dialog here: it draws its own frames
	// and returns true only if the player confirmed with a changed string. `out` is then the ASCII
	// text.
	bool RunOsk(const char* description, const std::string& initial, std::string* out);

	// Runs RunOsk for the server address, validates it, and saves it. Returns false if the player
	// backed out or the entry was rejected, in which case a message is already on screen.
	bool AskForServerAddress();

	void SetResult(const std::string& text);

	void DoCheck();
	void DoDownloadAndInstall();

	// Leaves the game and starts the Updater EBOOT, which applies the staged package and returns to
	// the menu. Does not return if the launch works -- loadexec resets the console -- so everything
	// after the launch call in there is a failure path. See the note in Updater.h for how the two
	// programs find each other.
	void HandOffToUpdater(const std::string& gameDir, const std::string& zipPath, const Update::Offer& offer);
};

#endif
