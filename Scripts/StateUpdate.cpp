#include "StateUpdate.h"

#include <pspctrl.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <pspsysmem.h>
#include <psputility.h>
#include <cstdio>
#include <cstring>

#include "TextureHelper.h"
#include "Version.h"
#include "CraftWorld2.h"

#include <Aurora/Utils/Logger.h>

using namespace Aurora::Graphics;
using namespace Aurora::Utils;
using namespace Aurora::System;
using namespace Aurora;

// The busy screen borrows the loading screen's look -- dirt tiles with the Load bar over them --
// because that is already the game's way of saying "working, do not touch". Matching it means the
// updater does not need any art of its own, and the player recognises it instantly.
#define UPDATE_FONT_SIZE 0.5f
// No shadow, then the outline flag the rest of the game uses for small text. Copied from
// LoadingScreen::RunLoadingScreen, which is the closest thing to this screen in the codebase.
#define UPDATE_FONT_OPTIONS (0x00000200 | 0x00000000)

// One line of text. See ReadBaseUrl. Kept short so a typo is obvious and the file is easy to write
// from a phone or a shell one-liner.
#define UPDATE_URL_FILE "update_url.txt"

// The server address the player is asked for the first time a check is made.
//
// 192.168.0.0 is the network address of the 192.168.0.x range, so it is never the address of a host:
// it is a placeholder that is obvious at a glance, cannot accidentally reach something, and cannot
// be mistaken for a working configuration. That is the point -- the real address is a guess nobody
// can make for the player, and the failure mode of guessing 192.168.1.100 silently is a "could not
// reach" message that sends them looking at the network instead of at the setting. So the check
// refuses to run until this has been replaced.
static const char* kServerPlaceholderIp = "192.168.0.0";
static const int kServerPort = 8000;

// The screen's own menu title and its options, in one place. Update() reopens this menu from several
// different steps (backing out of "Later" and dismissing a result), and separate copies of these
// strings would eventually disagree.
static const char* kMenuTitle = "MineCraft Updater";
static const char* kMenuCheck = "Check for update";
static const char* kMenuAddress = "Set server address";
static const char* kMenuBack = "Back";

// Menu indices, named because the switch below reads them and a bare 1 in an else-if is how the
// wrong option gets wired to the wrong action.
enum { kPickCheck = 0, kPickAddress = 1, kPickBack = 2 };

StateUpdate::StateUpdate()
{
	step = STEP_MENU;
	backSprite = 0;
	barSprite = 0;
	barBackSprite = 0;
	mRender = 0;
	mSystemMgr = 0;
	mSoundMgr = 0;
	dt = 0.0f;
	progressDone = 0;
	progressTotal = 0;
}

StateUpdate::~StateUpdate()
{
}

void StateUpdate::Init()
{
	mRender = RenderManager::InstancePtr();
	mSystemMgr = SystemManager::Instance();
	mSoundMgr = SoundManager::Instance();
	netReady = false;

	// Dirt, tiled 8x5 at 64 px with a 2x scale, exactly as the loading screen lays it out.
	backSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Dirt), 0, 0, 32, 32);
	backSprite->Scale(2, 2);

	// The Load texture is a 16x16 PNG holding two 16x8 bar halves, and which half is which was measured
	// from the file rather than assumed: rows 0-7 are RGB(181,249,127) (a light green) and rows 8-15
	// are RGB(48,74,40) (a dark green).
	//
	// Rows 0-7 are the FILLED half and rows 8-15 the EMPTY one, which is the opposite of what this
	// code did at first -- so the bar grew in dark green along a light green track. LoadingScreen.cpp
	// is the reference and names them the same way round: rows 0-7 are its `loadSprite`, the part that
	// grows, and rows 8-15 its `subLoadSprite`. The comment that used to stand here had the two halves
	// backwards, and the two assignments below had simply followed it.
	barSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Load), 0, 0, 16, 8);
	barBackSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Load), 0, 8, 16, 8);
}

// One definition of the main menu's options, so the several places that reopen it cannot drift.
static std::vector<std::string> MainMenuOptions()
{
	return std::vector<std::string>{kMenuCheck, kMenuAddress, kMenuBack};
}

// Reads back the numbers that decide whether sceNetInit can get its socket pool.
//
// Three different memories are in play here and telling them apart is the whole difficulty: the
// game's own malloc heap (CraftWorld::HeapFreeKB), and the system's share of the user partition
// (sceKernelTotalFreeMemSize). The heap cannot lend to the system, however free it looks, because it
// is already a separate allocation.
//
// sce_newlib_heap_kb_size and sce_newlib_heap_threshold_kb_size are what PSP_HEAP_SIZE_KB and
// PSP_HEAP_THRESHOLD_SIZE_KB in Scripts.cpp define when that file compiles, and _sbrk consumes them.
// Reading them back at runtime is the only way to confirm that what is linked into this build is what
// that file asks for: the linker map proves the symbol exists, not what it holds, and neither the heap
// nor the partition reports the reserve directly.
//
// Both are logged because only one of them matters. _sbrk sizes the heap as
// "sceKernelMaxFreeMemSize() minus the threshold" and merely sign-tests the heap size, so a run where
// thresholdKB is 0 or 512 is a run where the reserve did not take effect however the other number reads.
// 512 is the hard-coded fallback for "no threshold declared".
extern "C" int sce_newlib_heap_kb_size;
extern "C" int sce_newlib_heap_threshold_kb_size;

static void LogMemoryPicture(const char* when)
{
	Logger::Instance()->LogMessage(
		"mem: %s | heap_kb_size=%d thresholdKB=%d | kernel totalFreeKB=%u maxFreeKB=%u | game heap freeKB=%d\n",
		when,
		sce_newlib_heap_kb_size,
		sce_newlib_heap_threshold_kb_size,
		(unsigned)(sceKernelTotalFreeMemSize() / 1024),
		(unsigned)(sceKernelMaxFreeMemSize() / 1024),
		CraftWorld::HeapFreeKB());
}

void StateUpdate::Enter()
{
	mRender->SetOrtho(0, 0, 0, 0, 0, 0);

	step = STEP_MENU;
	baseUrl = ReadBaseUrl();

	// Messages in this screen are a sentence and one button, so they read as a modal and belong in
	// the middle of the screen rather than under a title at the top. Set on this instance only: every
	// state owns its own DebugMenu, so this cannot reach the debug stick's menus.
	menu.SetCentred(true);

	// The result is kept rather than ignored: the most likely reason it fails is that the net modules
	// and their socket pool did not fit in the free memory, which is worth saying plainly instead of
	// letting the player discover it as "could not reach the server".
	//
	// The free heap is logged first because that is the number that decides it, and it is the one thing
	// this screen cannot find out any other way: sceNetInit reports only NO_MEMORY, which says nothing
	// about how close it was or whether a world was still resident.
	LogMemoryPicture("before net init");
	netReady = Update::NetInit();

	Logger::Instance()->LogMessage("updater: screen opened, server is '%s' (configured=%d, net=%d)\n",
		baseUrl.c_str(), HasServerAddress() ? 1 : 0, netReady ? 1 : 0);

	menu.Open(kMenuTitle, MainMenuOptions());
}

void StateUpdate::CleanUp()
{
	// The net modules are left loaded for as long as the screen is up, because the player may check,
	// back out of installing, and check again. Releasing them here rather than per operation also
	// keeps the connection alive between the two steps, so the second one does not show the access
	// point dialog again.
	Update::NetShutdown();

	delete backSprite;
	delete barSprite;
	delete barBackSprite;
	backSprite = 0;
	barSprite = 0;
	barBackSprite = 0;
}

void StateUpdate::Pause()
{
}

void StateUpdate::Resume()
{
	mRender->SetOrtho(0, 0, 0, 0, 0, 0);
}

void StateUpdate::HandleEvents(StateManager* sManager)
{
	mSystemMgr->InputUpdate();

	// No background input while a dialog is up: the menu handles every button itself, and letting
	// Circle fall through here as well would back out two levels on one press.
}

// A URL from a host, in the one shape the rest of this file builds: http, port, trailing slash.
static std::string MakeUrl(const std::string& host)
{
	char buffer[160];
	snprintf(buffer, sizeof(buffer), "http://%s:%d/", host.c_str(), kServerPort);
	return buffer;
}

// The host part of a URL: no scheme, no port, no path. Used both to pre-fill the editor with what is
// already saved and to recognise the placeholder, and it is deliberately tolerant -- it has to cope
// with the file having been hand-written with a trailing slash, a port, or neither.
std::string StateUpdate::AddressFromUrl(const std::string& url)
{
	std::string s = url;

	if (s.compare(0, 8, "https://") == 0)
	{
		s.erase(0, 8);
	}
	else if (s.compare(0, 7, "http://") == 0)
	{
		s.erase(0, 7);
	}

	size_t end = s.size();
	size_t slash = s.find('/');
	if (slash != std::string::npos && slash < end)
	{
		end = slash;
	}
	size_t colon = s.find(':');
	if (colon != std::string::npos && colon < end)
	{
		end = colon;
	}

	s.erase(end);
	return s;
}

std::string StateUpdate::ReadSavedUrl()
{
	SceUID fd = sceIoOpen(UPDATE_URL_FILE, PSP_O_RDONLY, 0777);
	if (fd < 0)
	{
		return "";
	}

	char buffer[256];
	int read = sceIoRead(fd, buffer, sizeof(buffer) - 1);
	sceIoClose(fd);

	if (read <= 0)
	{
		return "";
	}

	buffer[read] = '\0';
	std::string url = buffer;

	// Trim, because a file written by hand on Windows will have CRLF and may have spaces.
	while (url.empty() == false && (url[url.size() - 1] == '\n' || url[url.size() - 1] == '\r' ||
									url[url.size() - 1] == ' ' || url[url.size() - 1] == '\t'))
	{
		url.erase(url.size() - 1);
	}
	size_t start = 0;
	while (start < url.size() && (url[start] == ' ' || url[start] == '\t'))
	{
		++start;
	}
	if (start > 0)
	{
		url.erase(0, start);
	}

	return url;
}

bool StateUpdate::HasServerAddress()
{
	std::string host = AddressFromUrl(ReadSavedUrl());
	return host.empty() == false && host != kServerPlaceholderIp;
}

bool StateUpdate::SaveBaseUrl(const std::string& url)
{
	// O_TRUNC matters for the same reason it does in the downloader: writing a shorter address over a
	// longer one would otherwise leave the tail of the old one in place, and the URL that reloads
	// would be a mixture of the two.
	SceUID fd = sceIoOpen(UPDATE_URL_FILE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0)
	{
		Logger::Instance()->LogMessage("updater: cannot write %s: 0x%08X\n",
			UPDATE_URL_FILE, (unsigned int)fd);
		return false;
	}

	int length = (int)url.size();
	int written = sceIoWrite(fd, url.c_str(), length);
	sceIoClose(fd);

	if (written != length)
	{
		Logger::Instance()->LogMessage("updater: short write to %s (%d of %d bytes)\n",
			UPDATE_URL_FILE, written, length);
		return false;
	}

	return true;
}

std::string StateUpdate::ReadBaseUrl()
{
	std::string saved = ReadSavedUrl();
	if (saved.empty())
	{
		Logger::Instance()->LogMessage("updater: no %s; offering the placeholder %s\n",
			UPDATE_URL_FILE, kServerPlaceholderIp);
		return MakeUrl(kServerPlaceholderIp);
	}

	// A trailing slash is required by the concatenation at the call sites and is the one character a
	// human will forget, so it is added rather than complained about.
	if (saved[saved.size() - 1] != '/')
	{
		saved += "/";
	}
	return saved;
}

void StateUpdate::SetResult(const std::string& text)
{
	// The sentence goes to the menu as its body. It used to be stored in a member that nothing ever
	// read, which is why every one of these dialogs showed a bare "Message" over an "OK" button and
	// the player had to guess what had happened.
	step = STEP_RESULT;
	menu.Open("Message", std::vector<std::string>{"OK"}, text);
}

bool StateUpdate::ProgressThunk(void* ctx, unsigned int done, unsigned int total)
{
	StateUpdate* self = static_cast<StateUpdate*>(ctx);
	self->progressDone = done;
	self->progressTotal = total;
	return self->PumpProgress();
}

void StateUpdate::DrawProgressFrame()
{
	// A whole frame, drawn from inside a blocking call. This is deliberate and it is why the updater
	// is a state of its own: a 5 MB transfer over PSP wifi is tens of seconds, and a screen that does
	// not change for tens of seconds is indistinguishable from a hang. The alternative -- an
	// incremental download stepped once per frame -- would avoid re-entering the renderer, but it
	// would also mean threading a resumable state machine through every call in Updater.cpp for no
	// benefit the player can see.
	mRender->SetOrtho(0, 0, 0, 0, 0, 0);
	mRender->StartFrame(0.466f, 0.72f, 1.0f);

	sceGuDisable(GU_DEPTH_TEST);
	sceGuEnable(GU_BLEND);
	sceGuColor(GU_COLOR(1, 1, 1, 1.0f));

	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 5; y++)
		{
			backSprite->SetPosition((float)(x * 64), (float)(y * 64));
			backSprite->Draw();
		}
	}

	// The bar. 160 px is the loading screen's width, and progressTotal == 0 means the server sent no
	// Content-Length, in which case showing a full bar would be a lie -- so it stays empty and the
	// byte count below carries the information instead.
	const float barWidth = 160.0f;
	float filled = 0.0f;
	if (progressTotal > 0)
	{
		filled = barWidth * ((float)progressDone / (float)progressTotal);
		if (filled > barWidth)
		{
			filled = barWidth;
		}
		if (filled < 1.0f)
		{
			filled = 1.0f;
		}
	}

	barBackSprite->NewScale(barWidth);
	barBackSprite->SetPosition(160, 150);

	if (progressTotal > 0)
	{
		barSprite->NewScale(filled);
		barSprite->SetPosition(160, 150);
	}

	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_DEPTH_TEST);

	// The order matters: the background bar first, then the filled one over it.
	if (progressTotal > 0)
	{
		sceGuDisable(GU_DEPTH_TEST);
		sceGuEnable(GU_BLEND);
		sceGuColor(GU_COLOR(1, 1, 1, 1.0f));
		barBackSprite->Draw();
		barSprite->Draw();
		sceGuDisable(GU_BLEND);
		sceGuEnable(GU_DEPTH_TEST);
	}

	mRender->SetFontStyle(UPDATE_FONT_SIZE, 0xFFFFFFFF, 0, UPDATE_FONT_OPTIONS);
	mRender->DebugPrint(240, 100, "%s", statusLine.c_str());

	if (progressTotal > 0)
	{
		mRender->DebugPrint(240, 172, "%u / %u KB", progressDone / 1024, progressTotal / 1024);
	}
	else if (progressDone > 0)
	{
		mRender->DebugPrint(240, 172, "%u KB", progressDone / 1024);
	}

	mRender->DebugPrint(240, 250, "Circle to cancel");

	mRender->EndFrame();
}

void StateUpdate::ClearDialogFrame()
{
	mRender->StartDialog();
}

void StateUpdate::PresentDialogFrame()
{
	mRender->EndDialog();
}

bool StateUpdate::ClearDialogThunk(void* ctx)
{
	static_cast<StateUpdate*>(ctx)->ClearDialogFrame();
	return true;
}

bool StateUpdate::PresentDialogThunk(void* ctx)
{
	static_cast<StateUpdate*>(ctx)->PresentDialogFrame();
	return true;
}

bool StateUpdate::PumpProgress()
{
	DrawProgressFrame();

	// Polling input here is safe even though StateManager also calls HandleEvents: InputUpdate is
	// what latches the button states, and calling it twice in a frame reads the same hardware state
	// twice rather than consuming an event.
	mSystemMgr->InputUpdate();
	if (mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
	{
		return false;
	}

	return true;
}

// ---------------------------------------------------------------------------
// Server address entry
// ---------------------------------------------------------------------------

bool StateUpdate::RunOsk(const char* description, const std::string& initial, std::string* out)
{
	// Held as function statics rather than locals: the dialog's own threads read these for as long as
	// it is up, and this is called from a deep stack.
	static SceUtilityOskData oskData;
	static SceUtilityOskParams oskParams;
	static unsigned short descText[96];
	static unsigned short inText[96];
	static unsigned short outText[96];

	memset(&oskData, 0, sizeof(oskData));
	memset(&oskParams, 0, sizeof(oskParams));
	memset(descText, 0, sizeof(descText));
	memset(inText, 0, sizeof(inText));
	memset(outText, 0, sizeof(outText));

	// The OSK takes one unsigned short per character. Copying the low byte of each is the identity for
	// the ASCII these strings are, and the reverse at the end. Not a general Unicode conversion, and it
	// does not need to be: what goes in is "Server address" and a dotted quad, and what comes back is
	// the same dotted quad.
	int i = 0;
	if (description != 0)
	{
		for (; description[i] != 0 && i < 95; ++i)
		{
			descText[i] = (unsigned short)(unsigned char)description[i];
		}
	}
	for (i = 0; i < (int)initial.size() && i < 95; ++i)
	{
		inText[i] = (unsigned short)(unsigned char)initial[i];
	}

	oskData.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
	oskData.lines = 1;
	oskData.unk_24 = 1;
	// Digits and punctuation. The scheme and the port are added by MakeUrl, so a full URL typed here
	// would be wrong twice over -- and restricting the input is what stops a player entering one and
	// then reading "could not reach http://http://192.168.0.0:8000/".
	oskData.inputtype = PSP_UTILITY_OSK_INPUTTYPE_LATIN_DIGIT | PSP_UTILITY_OSK_INPUTTYPE_LATIN_SYMBOL;
	oskData.desc = descText;
	oskData.intext = inText;
	oskData.outtextlength = 95;
	oskData.outtextlimit = 95;
	oskData.outtext = outText;

	oskParams.base.size = sizeof(oskParams);
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &oskParams.base.language);
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &oskParams.base.buttonSwap);
	oskParams.base.graphicsThread = 17;
	oskParams.base.accessThread = 19;
	oskParams.base.fontThread = 18;
	oskParams.base.soundThread = 16;
	oskParams.datacount = 1;
	oskParams.data = &oskData;

	int result = sceUtilityOskInitStart(&oskParams);
	if (result < 0)
	{
		Logger::Instance()->LogMessage("updater: sceUtilityOskInitStart failed: 0x%08X\n", (unsigned int)result);
		return false;
	}

	// The status this dialog reports is the pspUtilityDialogState set -- NONE, INIT, VISIBLE, QUIT,
	// FINISHED -- and NONE is both what it says before it has begun and what it says once it has shut
	// down. So NONE only means "finished" after it has been seen running; taking it as finished from
	// the start is what makes a system dialog flash up and vanish.
	bool seen = false;
	bool oskDone = false;

	while (true)
	{
		// Cleared before the pump, never after: the dialog draws during sceUtilityOskUpdate, so clearing
		// afterwards erases the frame that was just drawn and presents the empty buffer in its place.
		ClearDialogFrame();

		int status = sceUtilityOskGetStatus();
		if (status < 0)
		{
			Logger::Instance()->LogMessage("updater: sceUtilityOskGetStatus failed: 0x%08X\n", (unsigned int)status);
			sceUtilityOskShutdownStart();
			return false;
		}

		if (status == PSP_UTILITY_DIALOG_QUIT)
		{
			sceUtilityOskShutdownStart();
		}
		else if (status == PSP_UTILITY_DIALOG_FINISHED)
		{
			// Already shutting down; the poll still has to see NONE before it is really gone.
		}
		else if (status == PSP_UTILITY_DIALOG_NONE)
		{
			if (seen)
			{
				oskDone = true;
			}
			else
			{
				sceUtilityOskUpdate(1);
			}
		}
		else
		{
			seen = true;
			sceUtilityOskUpdate(1);
		}

		// Presented every iteration, and with no input polling of our own: the OSK reads the pad itself,
		// and Circle belongs to it there -- it deletes a character, it does not cancel this screen.
		PresentDialogFrame();

		if (oskDone)
		{
			break;
		}
	}

	if (oskData.result == PSP_UTILITY_OSK_RESULT_CANCELLED)
	{
		Logger::Instance()->LogMessage("updater: address entry cancelled\n");
		return false;
	}

	// UNCHANGED means the player confirmed without editing, which is a legitimate way to say "keep
	// what is there" for an address that is already set. The dialog does not promise to fill outtext
	// in that case, so the initial string is the answer.
	if (oskData.result == PSP_UTILITY_OSK_RESULT_UNCHANGED)
	{
		if (out != 0)
		{
			*out = initial;
		}
		return true;
	}

	std::string entered;
	for (i = 0; i < 95 && outText[i] != 0; ++i)
	{
		entered += (char)(outText[i] & 0xFF);
	}

	if (out != 0)
	{
		*out = entered;
	}
	return true;
}

bool StateUpdate::AskForServerAddress()
{
	// The OSK owns the screen while this runs, so Draw() must not also be painting the menu.
	step = STEP_BUSY;
	statusLine = "Server address";
	progressDone = 0;
	progressTotal = 0;

	std::string current = AddressFromUrl(baseUrl);
	if (current.empty())
	{
		current = kServerPlaceholderIp;
	}

	std::string entered;
	if (!RunOsk("Server address of the PC running serve-update.py", current, &entered))
	{
		// Backed out. Deliberately no message: the player chose not to proceed, which is not a
		// failure, and the menu is exactly where they left it.
		step = STEP_MENU;
		menu.Open(kMenuTitle, MainMenuOptions());
		return false;
	}

	// Trim, because a trailing space is easy to accept on the keyboard and produces a URL that fails
	// in a way that looks like the server is down.
	while (entered.empty() == false &&
		   (entered[entered.size() - 1] == ' ' || entered[entered.size() - 1] == '\t'))
	{
		entered.erase(entered.size() - 1);
	}

	// The symbol page can produce characters that are not part of any address, and a stray one gives a
	// URL that fails as "could not reach" rather than as "that is not an address".
	for (size_t k = 0; k < entered.size(); ++k)
	{
		char c = entered[k];
		if ((c < '0' || c > '9') && c != '.')
		{
			SetResult("An address is digits and dots only, like 192.168.50.224.");
			return false;
		}
	}

	// The point of the placeholder is that it cannot be right, so accepting it is refused rather than
	// saved. This is also why it is not simply a default URL: a silent default is a wrong answer the
	// player never gets told about.
	if (entered.empty() || entered == kServerPlaceholderIp)
	{
		SetResult(std::string("That is still the placeholder ") + kServerPlaceholderIp +
			". Enter the address of the PC running tools/serve-update.py.");
		return false;
	}

	std::string url = MakeUrl(entered);
	if (!SaveBaseUrl(url))
	{
		SetResult(std::string("Could not write ") + UPDATE_URL_FILE +
			". Check that the memory stick is not write-protected.");
		return false;
	}

	baseUrl = url;
	Logger::Instance()->LogMessage("updater: server address set to '%s'\n", baseUrl.c_str());
	return true;
}

void StateUpdate::DoCheck()
{
	// Nothing can be fetched without somewhere to fetch from, and the placeholder is not somewhere:
	// 192.168.0.0 is the network address of the 192.168.0.x range and can never be a host. So the
	// first check asks for the address, and once it is saved the check carries on from the same press
	// rather than making the player press again.
	if (!HasServerAddress())
	{
		if (!AskForServerAddress())
		{
			return;
		}
		// Written after Enter() read the old value, so re-read rather than trusting the cached one.
		baseUrl = ReadBaseUrl();
	}

	// Networking first, and this check is not a formality. The stack needs a 128 KB socket pool on top
	// of the five net modules, and on a console running in the base 32 MB partition that pool is
	// exactly what is not available: the log from the card shows sceNetInit reporting
	// SCE_KERNEL_ERROR_NO_MEMORY (0x80020190) while the world is loaded. Retried here rather than
	// trusted from Enter(), because the free memory that decides it is released when a world is closed.
	if (!netReady)
	{
		LogMemoryPicture("before net init retry");
		netReady = Update::NetInit();
	}
	if (!netReady)
	{
		SetResult("The game could not start networking: there was not enough free memory for it. "
				  "Return to the title screen without a world loaded and try again.");
		return;
	}

	step = STEP_BUSY;
	statusLine = "Connecting...";
	progressDone = 0;
	progressTotal = 0;

	if (!PumpProgress())
	{
		SetResult("Cancelled.");
		return;
	}

	// NetInit was called on Enter; a false here means the modules would not load at all, which is
	// worth saying plainly rather than reporting as a connection failure.
	if (!Update::NetIsConnected())
	{
		statusLine = "Choose a connection...";
		if (!Update::NetConnectDialog(ClearDialogThunk, PresentDialogThunk, this))
		{
			SetResult("Not connected. The update needs a network connection, and the WLAN switch has to be on.");
			return;
		}
	}

	statusLine = "Checking for updates...";
	progressDone = 0;
	progressTotal = 0;

	std::string body;
	if (!Update::HttpGetToString(baseUrl + "update.txt", &body, ProgressThunk, this))
	{
		SetResult("Could not reach " + baseUrl + ". Is the server script running, and is this PSP on the same network?");
		return;
	}

	Update::Offer incoming;
	if (!Update::ParseOffer(body, &incoming))
	{
		SetResult("The server's update.txt could not be understood. See Log.txt for the line that failed.");
		return;
	}

	// The comparison is on the integer, never the string: "3.0.10" sorts before "3.0.9" as text.
	//
	// `force` bypasses it, and the reason is the development loop: the build you want to push to the
	// console is the one you just made, so the server advertises the version the console already has
	// and a plain comparison answers "up to date" and refuses to install what you are testing. Only
	// this comparison is skipped -- the size and crc checks, the manifest, the extractor and the
	// EBOOT.PBP refusal all still run, so a forced install is not a less careful one.
	if (incoming.versionCode <= MC_VERSION_CODE && !incoming.force)
	{
		SetResult(std::string("MineCraft v") + MC_VERSION_STRING + " is already up to date.");
		return;
	}

	if (incoming.force && incoming.versionCode <= MC_VERSION_CODE)
	{
		Logger::Instance()->LogMessage("updater: offer is forced (code %d vs %d), offering a reinstall\n",
			incoming.versionCode, MC_VERSION_CODE);
	}

	offer = incoming;

	// Say which of the two this is, so a forced offer cannot be mistaken for a real upgrade. Slightly
	// different wording rather than a different flow, because everything after this point is identical
	// on purpose.
	const bool reinstall = (incoming.versionCode <= MC_VERSION_CODE);
	step = STEP_AVAILABLE;
	menu.Open(reinstall ? ("Reinstall version " + offer.version + "?")
						: ("Version " + offer.version + " is available"),
			  std::vector<std::string>{"Install now", "Later"});
}

void StateUpdate::DoDownloadAndInstall()
{
	step = STEP_BUSY;
	progressDone = 0;
	progressTotal = 0;

	std::string gameDir;
	if (!Update::GameDirectory(&gameDir))
	{
		SetResult("Could not find the game folder. That should be impossible, so something is very wrong.");
		return;
	}

	// A fixed staging name rather than the package's own filename. The Updater EBOOT is a separate
	// program with no idea what the server called the file, so the two halves have to agree on one
	// name, and this is it -- the only thing the handoff file has to carry is the folder.
	std::string zipPath = gameDir + "/" + Update::PackageName();

	// Downloading to a temporary name and renaming afterwards would be tidier, but the card's
	// filesystem makes the rename no safer than writing directly, and it would leave two ways for the
	// install to find a half-written package. So: write update.zip directly, and delete it if the
	// download turns out to be bad.
	statusLine = "Downloading " + offer.file + "...";
	if (!Update::HttpGetToFile(baseUrl + offer.file, zipPath, ProgressThunk, this))
	{
		SetResult("The download failed or was cancelled. Nothing was changed.");
		return;
	}

	statusLine = "Checking the download...";
	if (!PumpProgress())
	{
		SetResult("Cancelled. Nothing was changed.");
		return;
	}

	// Verify the file against what the server promised before letting it near the game folder. This
	// catches a truncated transfer, which is the failure a PSP on wifi actually produces, and it is
	// the check that makes the "success" return of pgeNetGetFile untrustworthy rather than dangerous.
	unsigned int crc = 0, size = 0;
	if (!Update::CrcOfFile(zipPath, &crc, &size) || size != offer.size || crc != offer.crc)
	{
		sceIoRemove(zipPath.c_str());
		SetResult("The download did not arrive intact, so it was discarded. Try again.");
		return;
	}

	// Read the manifest before handing off. The Updater reads it too, and it is the thing that would
	// catch a bad package -- but by then the game is gone, so a package that is going to be refused is
	// better refused HERE, where the player is still in the game and the message can say what is wrong.
	//
	// This also closes a gap that a hand-off alone would open. The Updater cannot replace UPDATER.PBP --
	// loadexec has that file mapped in order to run it, exactly as the game has EBOOT.PBP mapped -- and
	// it refuses a package containing one rather than half-applying it. Nothing installs that entry
	// today (see the two-phase note in UpdaterApp/main.cpp), so a package carrying the updater is caught
	// here and explained, instead of being handed over and refused one console restart later.
	Update::Manifest peek;
	std::string why;
	if (!Update::ReadManifestFromZip(zipPath, &peek, &why))
	{
		sceIoRemove(zipPath.c_str());
		SetResult("The package is not readable: " + why);
		return;
	}

	for (size_t i = 0; i < peek.files.size(); ++i)
	{
		if (peek.files[i].path == Update::UpdaterExecutableName())
		{
			Logger::Instance()->LogMessage(
				"updater: package carries %s, which neither program can install yet\n",
				Update::UpdaterExecutableName());
			SetResult("This package also updates the updater itself, which is not supported yet. "
					  "Nothing was changed.");
			return;
		}
	}

	Logger::Instance()->LogMessage("updater: handing off to %s for %u file(s)\n",
		Update::UpdaterExecutableName(), (unsigned int)peek.files.size());

	// The whole job goes to the Updater, which installs the package and returns to the menu. There is
	// no in-place path any more and nothing after this call but its failure handling.
	HandOffToUpdater(gameDir, zipPath, offer);
}

// Leaves the game and starts the Updater EBOOT.
//
// This call does not return when it works: loadexec resets the console into the new program, so the
// game is simply gone. That makes every line after it a failure path, which is what the manual
// instructions are for -- and it is also why nothing here needs to tidy up on the way out.
//
// The order matters. The handoff file is written BEFORE the launch, because the Updater has no working
// directory and no arguments: that file is the only way it can learn which folder to install into and
// which package to apply. If writing it fails there is nothing to launch into, so the game says so
// instead of resetting the console for no reason.
void StateUpdate::HandOffToUpdater(const std::string& gameDir, const std::string& zipPath, const Update::Offer& offer)
{
	Update::Pending pending;
	pending.gameDir = gameDir;
	pending.package = zipPath;		// absolute, as the Updater has no working directory
	pending.version = offer.version;
	pending.versionCode = offer.versionCode;
	pending.state = "apply";

	std::string why;
	if (!Update::WritePending(pending, &why))
	{
		Logger::Instance()->LogMessage("updater: handoff not written (%s); refusing to launch\n", why.c_str());
		SetResult("v" + offer.version + " changes the game itself and the instructions for the updater could not be saved. Nothing was changed. Try again.");
		return;
	}

	// Absolute: gameDir came from getcwd() on this side, where a working directory still exists. That
	// is the whole reason the game is the one that constructs this path.
	std::string updaterPath = gameDir + "/" + Update::UpdaterExecutableName();

	statusLine = "Starting the updater...";
	PumpProgress();

	Logger::Instance()->LogMessage("updater: launching '%s'\n", updaterPath.c_str());

	// A return from this means the launch was refused -- usually because the Updater EBOOT is not on
	// the card, which is a normal thing for an install that predates it. The package is already
	// downloaded and verified, so it is left in place and the player is told what to do.
	if (!Update::LaunchExecutable(updaterPath, &why))
	{
		Logger::Instance()->LogMessage("updater: handoff failed: %s\n", why.c_str());
		Update::ClearPending();
		SetResult("v" + offer.version + " changes the game itself, and " + Update::UpdaterExecutableName()
			+ " could not be started (" + why + "). The download is checked and waiting; launch MineCraft Updater to install it.");
		return;
	}
}

void StateUpdate::Update(StateManager* sManager)
{
	dt = mTimer.GetDeltaTime();
	if (dt > 0.1f)
	{
		dt = 0.1f;
	}

	if (step == STEP_LEAVE)
	{
		sManager->PopState();
		return;
	}

	// While busy, the blocking call is drawing its own frames and has not returned yet, so this
	// function cannot be reached. That is not an accident of timing -- DoCheck and
	// DoDownloadAndInstall only return once they are finished.
	if (step == STEP_BUSY)
	{
		return;
	}

	menu.Update(dt, mSystemMgr, mSoundMgr);

	int picked = menu.ConsumeResult();
	if (picked == DebugMenu::RESULT_NONE)
	{
		return;
	}

	// Circle backs out one level, everywhere, in one place. Doing it per step would mean four copies
	// of the rule and, eventually, one step where it does not work.
	if (picked == DebugMenu::RESULT_CANCELLED)
	{
		if (step == STEP_MENU)
		{
			step = STEP_LEAVE;
		}
		else
		{
			step = STEP_MENU;
			menu.Open(kMenuTitle, MainMenuOptions());
		}
		return;
	}

	switch (step)
	{
	case STEP_MENU:
		if (picked == kPickCheck)
		{
			DoCheck();
		}
		else if (picked == kPickAddress)
		{
			// Setting the address does not then check it. A check fired straight after the entry would
			// spend a request on an address the player had no chance to correct, and the menu they
			// come back to has "Check for update" on it.
			if (AskForServerAddress())
			{
				SetResult(std::string("Server set to ") + baseUrl);
			}
		}
		else
		{
			step = STEP_LEAVE;
		}
		break;

	case STEP_AVAILABLE:
		if (picked == 0)
		{
			// No install-mode question any more: there is one path, and it is the Updater. See the
			// note at the top of StateUpdate.h for why the in-place option is gone.
			DoDownloadAndInstall();
		}
		else
		{
			step = STEP_MENU;
			menu.Open(kMenuTitle, MainMenuOptions());
		}
		break;

	case STEP_RESULT:
		step = STEP_MENU;
		menu.Open(kMenuTitle, MainMenuOptions());
		break;

	default:
		break;
	}
}

void StateUpdate::Draw(StateManager* sManager)
{
	// The busy screen draws itself, frame by frame, from inside the blocking call.
	if (step == STEP_BUSY)
	{
		return;
	}

	mRender->SetOrtho(0, 0, 0, 0, 0, 0);
	mRender->StartFrame(1, 1, 1);

	// The world behind the menu was drawn by whoever pushed this state, but the frame has been
	// started again here, so the menu's dim needs something under it. Dirt tiles, same as the busy
	// screen, so the whole screen reads as one thing.
	sceGuDisable(GU_DEPTH_TEST);
	sceGuEnable(GU_BLEND);
	sceGuColor(GU_COLOR(1, 1, 1, 1.0f));

	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 5; y++)
		{
			backSprite->SetPosition((float)(x * 64), (float)(y * 64));
			backSprite->Draw();
		}
	}

	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_DEPTH_TEST);

	menu.Render(mRender);

	mRender->EndFrame();
}
