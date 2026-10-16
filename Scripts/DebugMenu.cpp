#include "DebugMenu.h"

#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/Graphics/Sprite.h>
#include <Aurora/System/SystemManager.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <stdio.h>

#include "InputHelper.h"
#include "SoundManager.h"
#include "TextureHelper.h"

using namespace Aurora::Graphics;

const float DebugMenu::kButtonX = 240.0f;
const float DebugMenu::kButtonY0 = 38.0f;
const float DebugMenu::kButtonPitch = 26.0f;
const float DebugMenu::kButtonScaleX = 2.0f;
const float DebugMenu::kButtonScaleY = 2.0f;
const float DebugMenu::kTitleY = 14.0f;
const float DebugMenu::kFooterY = 250.0f;

const float DebugMenu::kAnalogDead = 0.6f;
const float DebugMenu::kAnalogWait = 0.18f;

// A label's baseline sits this far below the button's centre for every unit of font size, so the
// text lands on the middle of the button. Calibrated against the pause menu, which draws its button
// labels 9 px below a button centre at the 0.5 size those buttons use (StatePlay.cpp, the options
// menu: buttons on a y=40 start and 30 pitch, labels at 49, 79, 109...). intraFont takes a baseline
// rather than a top, so drawing at the button's centre -- the obvious thing -- sits the text in the
// button's upper half and reads as a broken button.
const float DebugMenu::kLabelBaseline = 18.0f;

// How far the screen is dimmed behind the menu. Low enough that the paused world is clearly visible
// through it, which is the whole point of a menu you open to look at something.
const float DebugMenu::kDimAlpha = 0.45f;

// TEXT_ options: 0x200 is intraFont's centre alignment, matching StatePlay::DrawText, so every x
// below is a centre line rather than a left edge.
#define DEBUG_FONT_OPTIONS 0x00000200

DebugMenu::DebugMenu()
{
	open = false;
	centred = false;
	selected = 0;
	page = 0;
	result = RESULT_NONE;
	swallowFirstFrame = false;
	analogWait = 0.0f;
	dimSprite = NULL;
	buttonSprite = NULL;
	buttonSelSprite = NULL;
}

DebugMenu::~DebugMenu()
{
	if(dimSprite != NULL)
		delete dimSprite;
	if(buttonSprite != NULL)
		delete buttonSprite;
	if(buttonSelSprite != NULL)
		delete buttonSelSprite;
}

// Sprites are built on first use rather than in the constructor: the menu is a member of the play
// states, which are constructed before the texture pack has been opened.
void DebugMenu::EnsureSprites()
{
	if(dimSprite == NULL)
	{
		// A 16x16 black square stretched over the screen. Drawn with alpha so the paused world stays
		// visible behind the menu -- a fully black screen would hide the very mob the menu was opened
		// on, and being able to see it is half the value of a testing tool.
		//
		// The alpha only works because Sprite::Alpha is now real: it used to be an empty stub, which
		// is exactly why this drew an opaque black screen no matter what was passed in. It is a tint
		// on a black texture, so the dim is the only thing 0.62 controls -- 0 shows the world at full
		// brightness and 1 hides it completely.
		dimSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Black));
		dimSprite->Scale(30.0f, 17.0f);
		dimSprite->SetPosition(240.0f, 136.0f);
		dimSprite->Alpha(kDimAlpha);
	}

	if(buttonSprite == NULL)
	{
		buttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons), 0, 0, 95, 12);
		buttonSprite->Scale(kButtonScaleX, kButtonScaleY);
	}

	if(buttonSelSprite == NULL)
	{
		buttonSelSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons), 0, 12, 95, 12);
		buttonSelSprite->Scale(kButtonScaleX, kButtonScaleY);
	}
}

void DebugMenu::Open(const std::string& newTitle, const std::vector<std::string>& newOptions,
                     const std::string& newBody)
{
	title = newTitle;
	options = newOptions;
	body = newBody;
	selected = 0;
	page = 0;
	result = RESULT_NONE;
	open = (options.empty() == false);
	// See the member's comment: the press that opened the menu must not also confirm it.
	swallowFirstFrame = open;
	EnsureSprites();
}

void DebugMenu::Close()
{
	open = false;
}

int DebugMenu::PageOf(int index) const
{
	if(index < 0)
		return 0;
	return index / kPageSize;
}

int DebugMenu::PageCount() const
{
	int count = ((int)options.size() + kPageSize - 1) / kPageSize;
	if(count < 1)
		count = 1;
	return count;
}

void DebugMenu::ClampSelection()
{
	if(options.empty() == true)
	{
		selected = 0;
		page = 0;
		return;
	}
	if(selected < 0)
		selected = (int)options.size() - 1;
	if(selected >= (int)options.size())
		selected = 0;

	page = PageOf(selected);
}

int DebugMenu::ConsumeResult()
{
	int r = result;
	result = RESULT_NONE;
	return r;
}

void DebugMenu::Update(float dt, Aurora::System::SystemManager* sys, SoundManager* sound)
{
	if(open == false || sys == NULL)
		return;

	if(swallowFirstFrame == true)
	{
		swallowFirstFrame = false;
		return;
	}

	ClampSelection();

	analogWait -= dt;
	if(analogWait < 0.0f)
		analogWait = 0.0f;

	bool moved = false;
	int step = 0;
	int pageStep = 0;

	// Both the d-pad and the stick, so the menu behaves either way. The stick is held off for a
	// moment after a step so a resting thumb cannot run the selection down the list.
	if(sys->KeyPressed(PSP_CTRL_UP))
		step = -1;
	else if(sys->KeyPressed(PSP_CTRL_DOWN))
		step = 1;
	else if(sys->KeyPressed(PSP_CTRL_LEFT))
		pageStep = -1;
	else if(sys->KeyPressed(PSP_CTRL_RIGHT))
		pageStep = 1;
	else if(analogWait <= 0.0f)
	{
		float ay = sys->GetAnalogY();
		float ax = sys->GetAnalogX();
		if(ay < -kAnalogDead)
		{
			step = -1;
			analogWait = kAnalogWait;
		}
		else if(ay > kAnalogDead)
		{
			step = 1;
			analogWait = kAnalogWait;
		}
		else if(ax < -kAnalogDead)
		{
			pageStep = -1;
			analogWait = kAnalogWait;
		}
		else if(ax > kAnalogDead)
		{
			pageStep = 1;
			analogWait = kAnalogWait;
		}
	}

	// Wrapping and paging interact: stepping off the end of a page has to carry the page with it, or
	// the selection moves to an invisible button. Keeping the selection and the page derived from one
	// number makes that impossible to get wrong.
	if(step != 0)
	{
		selected += step;
		ClampSelection();
		moved = true;
	}
	else if(pageStep != 0)
	{
		int pages = PageCount();
		page += pageStep;
		while(page < 0)
			page += pages;
		while(page >= pages)
			page -= pages;
		// Land on the first entry of the new page, so the highlight is always on a visible button.
		selected = page * kPageSize;
		if(selected >= (int)options.size())
			selected = (int)options.size() - 1;
		moved = true;
	}

	if(moved == true && sound != NULL)
		sound->PlayMenuSound();

	// Cancel. This is the same key the pause menu closes on, so the muscle memory carries over.
	if(sys->KeyPressed(PSP_CTRL_CIRCLE))
	{
		result = RESULT_CANCELLED;
		open = false;
		if(sound != NULL)
			sound->PlayMenuSound();
		return;
	}

	// Confirm. The menu closes on the choice: the caller usually has to let the world run to see the
	// result, and a menu still holding the pause would hide it.
	if(sys->KeyPressed(PSP_CTRL_CROSS))
	{
		result = selected;
		open = false;
		if(sound != NULL)
			sound->PlayMenuSound2();
		return;
	}

	// Start also confirms, because the action button is on the right of the PSP and the cross is at
	// the bottom: both are a natural "yes" depending on which thumb did the work.
	if(sys->KeyPressed(PSP_CTRL_START))
	{
		result = selected;
		open = false;
		if(sound != NULL)
			sound->PlayMenuSound2();
		return;
	}
}

// The message body's font size, line pitch and wrap width. Smaller than the title (0.6) so a full
// sentence fits across the screen in a few lines, and wrapped well inside the 480 px width so a long
// sentence never touches the screen edge.
static const float kBodySize = 0.45f;
static const float kBodyLineH = 13.0f;
static const int kBodyMaxWidth = 420;

// Half a menu button's height: the 95x12 menu_elements cell drawn at kButtonScaleY (2.0), so 24 px
// tall. The layout works in button *centres* -- Sprite::SetPosition takes a centre, and SetPosition's
// vertices are built from -width/2 to +width/2 -- so anywhere a button is placed relative to
// something that is not another button, this has to be added on each side. Leaving it out is what put
// the first button through the last line of the message it sat under.
static const float kButtonHalfH = 12.0f;

// Air between the last line of a message and the button under it. Enough that a descender cannot
// reach the button's top edge, which is what kButtonHalfH alone would only just miss.
static const float kBodyGapToButton = 6.0f;

// Breaks text into lines that each fit inside maxWidth, preferring to break at spaces.
//
// Measured with whatever font style is current, so the caller must set the style before calling --
// the same requirement the button labels have, and for the same reason (intraFont measures at the
// size last handed to intraFontSetStyle). A word that cannot fit on a line of its own is emitted as
// it is rather than split: the loop still consumes a word per iteration, so a long URL overruns its
// margin instead of hanging, which is the failure worth having.
static void WrapToWidth(RenderManager* render, const std::string& text, int maxWidth,
                        std::vector<std::string>& out)
{
	std::string line;
	size_t i = 0;
	while(i < text.size())
	{
		while(i < text.size() && text[i] == ' ')
			++i;
		size_t wordStart = i;
		while(i < text.size() && text[i] != ' ')
			++i;
		if(wordStart == i)
			break;

		std::string word = text.substr(wordStart, i - wordStart);
		if(line.empty())
		{
			line = word;
			continue;
		}

		std::string candidate = line + " " + word;
		if(render->GetTextWidth(candidate.c_str()) <= maxWidth)
		{
			line = candidate;
		}
		else
		{
			out.push_back(line);
			line = word;
		}
	}
	if(line.empty() == false)
		out.push_back(line);
}

void DebugMenu::Render(RenderManager* render)
{
	if(open == false || render == NULL)
		return;

	EnsureSprites();

	sceGuDisable(GU_DEPTH_TEST);
	sceGuEnable(GU_BLEND);
	sceGuColor(GU_COLOR(1, 1, 1, 1));

	dimSprite->Draw();

	int first = page * kPageSize;
	int last = first + kPageSize;
	if(last > (int)options.size())
		last = (int)options.size();

	// The body is wrapped before anything is placed, because the number of lines decides where the
	// title and the buttons go. Measuring needs the body's font style to be current, so it is set
	// here and re-set below when the lines are drawn.
	std::vector<std::string> bodyLines;
	if(body.empty() == false)
	{
		render->SetFontStyle(kBodySize, GU_COLOR(1, 1, 1, 1), 0, DEBUG_FONT_OPTIONS);
		WrapToWidth(render, body, kBodyMaxWidth, bodyLines);
	}
	const float bodyH = kBodyLineH * (float)bodyLines.size();

	// The buttons are one column on a fixed pitch, so the height of the block is the count times the
	// pitch and centring it is a subtraction. Clamped at kButtonY0 so that a full eight-option page
	// keeps the layout it has always had rather than sliding up under the top of the screen.
	float y0 = kButtonY0;
	float titleY = kTitleY;
	float bodyY = 0.0f;
	if(centred == true)
	{
		if(bodyLines.empty())
		{
			y0 = 136.0f - (kButtonPitch * (float)(last - first)) * 0.5f;
			if(y0 < kButtonY0)
				y0 = kButtonY0;
			// One pitch above the first button, which is the same relationship the top-aligned layout
			// has, so a centred menu of eight options looks like an uncentred one.
			titleY = y0 - kButtonPitch;
			if(titleY < 4.0f)
				titleY = 4.0f;
		}
		else
		{
			// Title, then the sentence, then the button: one block, centred, with a pitch of air
			// between each part. Keeping them as one block is the point -- centring only the button
			// would leave the sentence floating at the top of an otherwise empty screen.
			//
			// The block's height has to count half a button at each end, because the buttons are placed
			// by their centres: from the first button's centre down to the last one's is
			// kButtonPitch * (n-1), and the block only reaches the buttons' top and bottom edges by
			// adding kButtonHalfH outside that.
			float blockH = kButtonPitch + bodyH + kBodyGapToButton
			               + kButtonPitch * (float)(last - first) + 2.0f * kButtonHalfH;
			float top = 136.0f - blockH * 0.5f;
			if(top < 4.0f)
				top = 4.0f;
			titleY = top;
			bodyY = titleY + kButtonPitch;
			y0 = bodyY + bodyH + kBodyGapToButton + kButtonHalfH;
		}
	}
	else if(bodyLines.empty() == false)
	{
		// Not centred: keep the debug stick's layout and put the sentence under the title, pushing the
		// buttons down so it cannot overlap them.
		bodyY = titleY + kButtonPitch;
		y0 = bodyY + bodyH + kBodyGapToButton + kButtonHalfH;
	}

	// Title, shadowed the same way StatePlay::DrawText does it, because a centred string over a
	// half-lit world is unreadable without it.
	render->SetFontStyle(0.6f, GU_COLOR(0.1f, 0.1f, 0.1f, 1), 0, DEBUG_FONT_OPTIONS);
	render->DebugPrint(240, (int)titleY + 1, "%s", title.c_str());
	render->SetFontStyle(0.6f, GU_COLOR(1.0f, 0.85f, 0.3f, 1), 0, DEBUG_FONT_OPTIONS);
	render->DebugPrint(240, (int)titleY, "%s", title.c_str());

	// The sentence, one wrapped line at a time. Same shadow-then-text pairing as the title and the
	// labels, because it lands over the same half-lit background and is otherwise unreadable. The
	// baseline offset is the label rule (kLabelBaseline per unit of size) rather than a new constant,
	// so text at this size sits on its line the same way label text sits on its button.
	for(size_t line = 0; line < bodyLines.size(); ++line)
	{
		int baseline = (int)(bodyY + kBodyLineH * (float)line + kLabelBaseline * kBodySize);
		render->SetFontStyle(kBodySize, GU_COLOR(0.1f, 0.1f, 0.1f, 1), 0, DEBUG_FONT_OPTIONS);
		render->DebugPrint(240, baseline + 1, "%s", bodyLines[line].c_str());
		render->SetFontStyle(kBodySize, GU_COLOR(0.95f, 0.95f, 0.95f, 1), 0, DEBUG_FONT_OPTIONS);
		render->DebugPrint(240, baseline, "%s", bodyLines[line].c_str());
	}

	for(int i = first; i < last; i++)
	{
		float y = y0 + kButtonPitch * (float)(i - first);
		Sprite* s = (i == selected) ? buttonSelSprite : buttonSprite;
		s->SetPosition(kButtonX, y);
		s->Draw();

		bool isSel = (i == selected);
		unsigned int color = isSel ? GU_COLOR(1.0f, 1.0f, 0.4f, 1.0f) : GU_COLOR(1.0f, 1.0f, 1.0f, 1.0f);

		// Shrink a label that would overrun the button rather than let it spill over the edge. The
		// style has to be set before measuring, because GetTextWidth measures at whatever size is
		// currently in effect. 190 px of button leaves 176 px of usable text.
		float size = 0.5f;
		const char* label = options[i].c_str();
		render->SetFontStyle(size, color, 0, DEBUG_FONT_OPTIONS);
		int width = render->GetTextWidth(label);
		if(width > 176)
		{
			size = size * (176.0f / (float)width);
			if(size < 0.34f)
				size = 0.34f;
		}

		// Computed after the shrink, because a shrunken label also needs a smaller offset to stay in
		// the middle of the button rather than below it.
		float baseline = y + kLabelBaseline * size;

		render->SetFontStyle(size, GU_COLOR(0.1f, 0.1f, 0.1f, 1), 0, DEBUG_FONT_OPTIONS);
		render->DebugPrint((int)kButtonX, (int)baseline + 1, "%s", label);
		render->SetFontStyle(size, color, 0, DEBUG_FONT_OPTIONS);
		render->DebugPrint((int)kButtonX, (int)baseline, "%s", label);
	}

	// Footer: where you are in the list, and the two keys that leave. Only shown when it has something
	// to say -- a one-page menu does not need "1/1".
	render->SetFontStyle(0.4f, GU_COLOR(0.1f, 0.1f, 0.1f, 1), 0, DEBUG_FONT_OPTIONS);
	render->DebugPrint(240, (int)kFooterY + 1, "X select    O close");
	render->SetFontStyle(0.4f, GU_COLOR(0.85f, 0.85f, 0.85f, 1), 0, DEBUG_FONT_OPTIONS);
	render->DebugPrint(240, (int)kFooterY, "X select    O close");

	if(PageCount() > 1)
	{
		char buf[32];
		sprintf(buf, "%d/%d  L/R", page + 1, PageCount());
		render->SetFontStyle(0.4f, GU_COLOR(1.0f, 0.85f, 0.3f, 1), 0, DEBUG_FONT_OPTIONS);
		render->DebugPrint(240, (int)kFooterY - 14, "%s", buf);
	}
}
