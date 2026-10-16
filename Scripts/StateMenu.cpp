#include "StateMenu.h"
#include <Aurora/Graphics/TextureManager.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspiofilemgr.h>
#include <malloc.h>
#include <string.h>
#include "StateOptions.h"
#include "StatePlay.h"
#include "StatePlayCreative.h"
#include "LoadingScreen.h"
#include "InputHelper.h"
#include "TextureHelper.h"
#include "Version.h"
#include "BootTrace.h"
#include <Aurora/System/NetworkManager.h>
#include <zlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>

// font vars
#define default_small_size 0.35
#define default_size 0.5
#define default_big_size 0.687
#define PI 3.1415926535897f

#define ENGLISH 1

#define SURVIVAL 0
#define CREATIVE 1
#define HARDCORE 2

using namespace Aurora::Graphics;
using namespace Aurora::Utils;
using namespace Aurora::System;
using namespace Aurora;

/*
 * The changelog on the News & Converter screen.
 *
 * Kept as data rather than a run of DebugPrint calls, because the box scrolls now and has to know how
 * many lines there are to know how far it can scroll. The old run also drew "-Changed HUD Size" and
 * "-Changed Block Count (Creative)" on the same row, so the first was hidden behind the second.
 */
static const char* kNewsLines[] =
{
	"-Added The Nether",
	"-Infinite World Generation",
	"-Biomes Blend Together",
	"-Added Player Skins",
	"-Added Redstone",
	"-Added Pistons",
	"-Added Bows And Arrows",
	"-Added Skeletons",
	"-Added Spiders And Chickens",
	"-Added Third Person",
	"-Added Sprint, Sneak And Hunger",
	"-Added Creative Flight",
	"-New Lighting",
	"-Added Launcher Updates",
	"-Crash And Performance Fixes",
};
static const int kNewsLineCount = (int)(sizeof(kNewsLines) / sizeof(kNewsLines[0]));

// The box's text metrics. The panel is blackBackground scaled 60x22 -- 8x8 stretched to 480x176,
// centred at (240,116) -- so the box covers x 0..480 and y 28..204, which was checked against a
// screenshot by its flatness (the panel measures as a flat fill where the title background behind it is
// noisy). The title sits at y 29 and the lines run from just below it.
//
// Nine lines at a pitch of 17 fill the panel to y 195, which is the most that reads cleanly: the box is
// 176 tall, the title takes the first 18 of it, and a line of this font is 13 tall, so a tenth would
// either collide with the title or with the panel's bottom edge. Nine also keeps the box scrolling --
// there are more lines than fit, so Up and Down still have a page to move through -- where showing all of
// them would leave the hints pointing at nothing.
//
// That is now ten lines at a pitch of 16, which is the tightest this layout takes: the tenth line starts
// at y 190 and its 13px body ends at 203, just inside the panel's 204 edge. The pitch came down from 17
// rather than the count staying at nine because ten lines at 17 would have put the last body at 212,
// over the edge and onto the title background. Three pixels between lines is still a clear gap for a
// font this size.
static const float kNewsLineTop = 46.0f;
static const float kNewsLineStep = 16.0f;
static const int kNewsVisibleLines = 10;

static int NewsMaxScroll()
{
	int maxScroll = kNewsLineCount - kNewsVisibleLines;
	return (maxScroll > 0) ? maxScroll : 0;
}

// One line per key press inside the News & Converter screen, naming the rung the ladder is on. The
// ladder is three states deep and two of them are indistinguishable on a screenshot (a focused box and
// a scrolling box look the same until you press something), so without this the only way to check that
// Up really hands over to the box and X really enters the scroll is to watch a person use it. It also
// records a clamped scroll, which is what proves the box stops at the last line.
static void LogNewsState(int focus, int scroll, int button)
{
	Aurora::Utils::Logger::Instance()->LogMessage("news: focus=%d (0=buttons 1=box 2=scrolling) scroll=%d button=%d\n",
		focus, scroll, button);
}

StateMenu::StateMenu()
{

}

StateMenu::~StateMenu()
{

}

void StateMenu::Init()
{
    newW_width = 0;
    newW_height = 0;
    newW_length = 0;

    newW_gameMode = SURVIVAL;
    newW_deleteCaves = true;
    conversionStage = 0;
    errorType = 0;

    converterPos = 0;
    schematicExists = false;

    lol = "";


    mainStatistics.blockPlaced = 0;
    mainStatistics.blockDestroyed = 0;
    mainStatistics.daysInGame = 0;
    mainStatistics.minutesPlayed = 0;
    mainStatistics.itemsCrafted = 0;
    mainStatistics.itemsSmelted = 0;
    mainStatistics.jumps = 0;
    mainStatistics.dies = 0;
    mainStatistics.foodEaten = 0;
    mainStatistics.badlyFalls = 0;
    mainStatistics.blocksWalked = 0;
    mainStatistics.treesGrowned = 0;
    mainStatistics.cropsGrowned = 0;
    mainStatistics.soilPlowed = 0;
    mainStatistics.timeInAir = 0;
    mainStatistics.timeInWater = 0;
    mainStatistics.damageRecieved = 0;
    // end

    mainOptions.detailedSky = 1;
    mainOptions.smoothLighting = true;
    mainOptions.sounds = 1;
    mainOptions.music = 1;
    mainOptions.fogRendering = 1;
    mainOptions.fastRendering = false;
    mainOptions.mipMapTexturing = false;
    mainOptions.autoJump = 0;
    mainOptions.fov = 65;
    mainOptions.freezeDayTime = 0;
    mainOptions.worldBlockAnimation = 1;
    mainOptions.particles = 0;
    mainOptions.difficult = 2; // normal
    mainOptions.horizontalViewDistance = 6;
    mainOptions.verticalViewDistance = 2;
    mainOptions.guiDrawing = 1;



    //set render manager instance
    mRender = RenderManager::InstancePtr();
    mSystemMgr = SystemManager::Instance();
    mSoundMgr = SoundManager::Instance();

    logoSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Logo),0,0,256,64);
    logoSprite->Scale(1.5f,1.5f);
    logoSprite->SetPosition(240,50);

    backgroundSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::background));
    backgroundSprite->Scale(1.4f,1.4f);
    backgroundSprite->SetPosition(240,136);

    bx = 240;
    by = 136;
    directionx = rand() % 2;
    directiony = rand() % 2;

    rectFilledSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Rectangles),0,0,230,37);
    rectFilledSprite->SetPosition(240,150);
    rectFilledSprite->Scale(2,2);

    rectEmptySprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Rectangles),0,37,230,37);
    rectEmptySprite->SetPosition(240,150);
    rectEmptySprite->Scale(2,2);

    buttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,0,95,12); // stand
    buttonSprite->SetPosition(240,150);
    buttonSprite->Scale(2,2);

    sbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,12,95,12); // stand selected
    sbuttonSprite->SetPosition(240,150);
    sbuttonSprite->Scale(2,2);

    nbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,24,95,12); // dark
    nbuttonSprite->SetPosition(240,150);
    nbuttonSprite->Scale(2,2);

    // small buttons
    buttonSmallSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,0,95,12); // stand
    buttonSmallSprite->SetPosition(240,150);
    buttonSmallSprite->Scale(0.45f,1.0f);
    buttonSmallSprite->Scale(2,2);

    sbuttonSmallSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,12,95,12); // stand selected
    sbuttonSmallSprite->SetPosition(240,150);
    sbuttonSmallSprite->Scale(0.45f,1.0f);
    sbuttonSmallSprite->Scale(2,2);

    nbuttonSmallSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,24,95,12); // dark
    nbuttonSmallSprite->SetPosition(240,150);
    nbuttonSmallSprite->Scale(0.45f,1.0f);
    nbuttonSmallSprite->Scale(2,2);

    mbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,36,95,12); // gray
    mbuttonSprite->SetPosition(240,150);
    mbuttonSprite->Scale(2,2);

    smbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,48,95,12); // gray selected
    smbuttonSprite->SetPosition(240,150);
    smbuttonSprite->Scale(2,2);

    backSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Dirt),0,0,32,32);
    backSprite->Scale(2,2);

    lamecraftSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::lameCraft),0,0,320,90);
    lamecraftSprite->SetPosition(240,50);
    lamecraftSprite->Scale(1,1);

    blackBackground = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),87,60,8,8);
    blackBackground->SetPosition(240,116);
    blackBackground->Scale(60,22);

    // The changelog's d-pad hints. Two 16x16 cells of title/dpad.png: up at (0,0), down at (16,0).
    // Native scale, because a hint at 2x would shout louder than the changelog it is annotating.
    //
    // The number is checked before the sprite is built, not after: Sprite's constructor indexes
    // TextureManager::Images[number], and a texture the chosen pack does not provide comes back as -1,
    // so building one anyway reads Images[-1] and faults. A pack without the art therefore loses the
    // hint instead of the session -- the box still scrolls, and only the picture is missing.
    int dpadNumber = TextureHelper::Instance()->GetTexture(TextureHelper::dpadTexture);
    dpadUpSprite = NULL;
    dpadDownSprite = NULL;
    if(dpadNumber >= 0)
    {
        dpadUpSprite = new Sprite(dpadNumber,0,0,16,16);
        dpadUpSprite->SetPosition(458,44);
        dpadUpSprite->Scale(1,1);

        dpadDownSprite = new Sprite(dpadNumber,16,0,16,16);
        dpadDownSprite->SetPosition(458,188);
        dpadDownSprite->Scale(1,1);
    }

    selectPos = 0;

    //load save info
    BootTrace::Stage("StateMenu::Init reading Save/ and texture packs");
    ScanSaveFiles("Save/");
    ScanTexturePacks("Assets/Textures/");
    BootTrace::Stage("StateMenu::Init content scanned");

    menuState = -1;
    skinPos = 0;
    skinStart = 0;
    skinReturnState = 0;
    skinSpin = 0.45f;
    skinFocus = 0;
    skinTab = 0;
    skinMessage = "";
    skinBrowsePos = 0;
    skinBrowseStart = 0;
    loadSelectPos = 0;
    loadSavePos = 0;
    aboutPos = 0;
    aboutFocus = 0;
    newsScroll = 0;

    newWorldName = "New World";
    newWorldNamestr = "";
    newWorldSeed = "";

    size_f = 0.347f;

    //for map generation
    makeTrees = true;
    makeWater = true;
    makeCaves = true;

    terrainBuilder = 0;
    worldType = 0; // Old
    gameMode = SURVIVAL;
    seed_1 = 0;

    SplashNumber = rand() % 6;
    splashSize = 0.0f;

    saveSubmenu = false;
    saveSubMenuSelect = 2;
    generateSelectPose = 0;

    newWorldNamestr = newWorldName.c_str();
    currentVersion = 140;

    //input helper
    BootTrace::Stage("StateMenu::Init input helper");
    InputHelper::Instance()->Init();
    InputHelper::Instance()->Load();

	animationscreen = 1;
	fontcoloroption = 0;
	fontcolor = 0;
	srand(time(0));

    tpCurrent = 0;
	tpMax = 0;
	tpEnd = 0;
	tpStart = 0;
	tpPos = 0;
	tpSelectPos = 0;
	//timex = 2400;

	for(int i = 0; i <= 31; i++)
	{
	    worldName[i] = ' ';
	}
}

void StateMenu::Enter()
{
    BootTrace::Stage("StateMenu::Enter");
    mRender->SetOrtho(0,0,0,0,0,0);
    selectPos = 0;
}

void StateMenu::CleanUp()
{
    delete logoSprite;
    delete buttonSprite;
    delete sbuttonSprite;
    delete nbuttonSprite;
    delete mbuttonSprite;
    delete smbuttonSprite;
    delete backSprite;
    delete lamecraftSprite;

    delete buttonSmallSprite;
    delete sbuttonSmallSprite;
    delete nbuttonSmallSprite;

    delete backgroundSprite;
    delete blackBackground;
    delete dpadUpSprite;
    delete dpadDownSprite;
	/*delete screen1Sprite;
	delete screen2Sprite;
	delete screen3Sprite;
	delete screen4Sprite;*/
}

void StateMenu::Pause()
{

}

void StateMenu::Resume()
{
    // A world was Pushed over this state and freed the title-screen art while it ran; this is the
    // point where it comes back. Resume (not Init) because the menu is paused, never destroyed, so
    // its sprites and their cached texture numbers are the ones built at boot.
    TextureHelper::Instance()->EnsureMenuTextures();

    menuState = 0;
    loadSelectPos = 0;
    loadSavePos = 0;
    mRender->SetOrtho(0,0,0,0,0,0);
}

void StateMenu::HandleEvents(StateManager* sManager)
{	
    //update input
    mSystemMgr->InputUpdate();

    switch(menuState)
    {
	case -2:// Multiplayer menu
    {
		
        //up, down
        if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
        {
            selectPos--;
            if(selectPos < 0)
                selectPos = 3;

            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
        {
            selectPos++;
            if(selectPos > 3)
                selectPos = 0;

            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            selectPos = 0;
            menuState = 12;

            SplashNumber = rand() % 6;
			mSoundMgr->PlayMenuSound2();
        }
		if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
        {
			NetworkManager::Instance()->ShutDownConnection();
            if(selectPos == 0)//play state
            {
                ScanSaveFiles("Save/");

                loadSavePos = 0;
                loadSaveStart = 0;
                loadSaveEnd = saveFilesList.size();
                loadSaveMax = 4;
                if(loadSaveMax > loadSaveEnd)
                loadSaveMax = loadSaveEnd;

                menuState = 1;
                if(saveFilesList.empty() == false)
                {
                    loadSelectPos = 0;
                }
                else
                {
                    loadSelectPos = 3;
                }
            }
		}
		if(mSystemMgr->KeyPressed(PSP_CTRL_LTRIGGER))
        {
			NetworkManager::Instance()->ShutDownConnection();
            if(selectPos == 0)//play state
            {
                ScanSaveFiles("Save/");

                loadSavePos = 0;
                loadSaveStart = 0;
                loadSaveEnd = saveFilesList.size();
                loadSaveMax = 4;
                if(loadSaveMax > loadSaveEnd)
                loadSaveMax = loadSaveEnd;

                menuState = 1;
                if(saveFilesList.empty() == false)
                {
                    loadSelectPos = 0;
                }
                else
                {
                    loadSelectPos = 3;
                }
            }
		}
    }
    case  -1://main menu
    {
        /*//if triangle button pressed the exit
        if(mSystemMgr->KeyPressed(PSP_CTRL_START))
        {
            //running = false;
            sManager->Quit();
        }
        //up, down

		/*if(menuState == -1)
        {
            if (animationscreen == 1)
            {
                timex < 15000 ? timex+= 2 : animationscreen = 2;
            }
            else
            {
                timex > 2450 ? timex-= 2 : animationscreen = 1;
            }
        }*/
		NetworkManager::Instance()->ShutDownConnection();
        if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
        {
            selectPos--;
            if(selectPos < 0)
                selectPos = 4;
            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
        {
            selectPos++;
            if(selectPos > 4)
                selectPos = 0;

            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(selectPos == 0)
            {
				mSoundMgr->PlayMenuSound2();
            }
			if(selectPos == 1)
            {
				mSoundMgr->PlayMenuSound2();
            }
			if(selectPos == 2)
            {
				mSoundMgr->PlayMenuSound2();
            }
			if(selectPos == 3)
            {
				mSoundMgr->PlayMenuSound2();
            }
			if(selectPos == 4)
            {
				mSoundMgr->PlayMenuSound2();
            }
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(selectPos == 0)//play state
            {
                ScanSaveFiles("Save/");

                loadSavePos = 0;
                loadSaveStart = 0;
                loadSaveEnd = saveFilesList.size();
                loadSaveMax = 4;
                if(loadSaveMax > loadSaveEnd)
                loadSaveMax = loadSaveEnd;

                menuState = 1;
                if(saveFilesList.empty() == false)
                {
                    loadSelectPos = 0;
                }
                else
                {
                    loadSelectPos = 3;
                }
            }
            if(selectPos == 1)
            {
                //options
                StateOptions *stateOptions = new StateOptions();
                stateOptions->Init();
                sManager->PushState(stateOptions);
            }
            if(selectPos == 2)
            {
                //about
                menuState = 3;
                aboutPos = 1;
                aboutFocus = 0;
                newsScroll = 0;
            }
            if(selectPos == 3)
            {
                //textures
                tpCurrent = 0;
                tpMax = 0;
                tpEnd = 0;
                tpStart = 0;
                tpPos = 0;
                tpSelectPos = 0;

                menuState = 11;
                tpSelectPos = 0;
                tpEnd = texturePackList.size();
                tpMax = 3;
                ScanTexturePacks("Assets/Textures/");
            }
            if(selectPos == 4)
            {
                ScanSkins();
                skinPos = 0;
                skinStart = 0;
                const char* current = TextureHelper::Instance()->PlayerSkinName();
                for(int i = 0; i < (int)skinList.size(); i++)
                {
                    if(skinList[i] == current)
                        skinPos = i;
                }
                skinStart = 0;
                skinFocus = 0;
                skinTab = 0;
                skinSpin = 0.45f;
                skinMessage = "";
                skinReturnState = menuState;
                menuState = 13;
            }
        }
    }
    break;
	case  0://main menu
    {
        /*//if triangle button pressed the exit
        if(mSystemMgr->KeyPressed(PSP_CTRL_START))
        {
            //running = false;
            sManager->Quit();
        }*/

		/*if(menuState == 0)
        {
            if (animationscreen == 1)
            {
                timex < 15000 ? timex+= 2 : animationscreen = 2;
            }
            else
            {
                timex > 2450 ? timex-= 2 : animationscreen = 1;
            }
        }*/

        //up, down
        if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
        {
            selectPos--;
            if(selectPos < 0)
                selectPos = 4;

            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
        {
            selectPos++;
            if(selectPos > 4)
                selectPos = 0;

            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(selectPos == 0)
            {
				mSoundMgr->PlayMenuSound2();
            }
			if(selectPos == 1)
            {
				mSoundMgr->PlayMenuSound2();
            }
			if(selectPos == 2)
            {
				mSoundMgr->PlayMenuSound2();
            }
			if(selectPos == 3)
            {
				mSoundMgr->PlayMenuSound2();
            }
			if(selectPos == 4)
            {
				mSoundMgr->PlayMenuSound2();
            }
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(selectPos == 0)//play state
            {
                ScanSaveFiles("Save/");

                loadSavePos = 0;
                loadSaveStart = 0;
                loadSaveEnd = saveFilesList.size();
                loadSaveMax = 4;
                if(loadSaveMax > loadSaveEnd)
                loadSaveMax = loadSaveEnd;

                menuState = 1;
                if(saveFilesList.empty() == false)
                {
                    loadSelectPos = 0;
                }
                else
                {
                    loadSelectPos = 3;
                }
            }
            if(selectPos == 1)
            {
                //options
                StateOptions *stateOptions = new StateOptions();
                stateOptions->Init();
                sManager->PushState(stateOptions);
            }
            if(selectPos == 2)
            {
                //about
                menuState = 3;
                aboutPos = 1;
                aboutFocus = 0;
                newsScroll = 0;
            }
            if(selectPos == 3)
            {
                //textures
                tpCurrent = 0;
                tpMax = 0;
                tpEnd = 0;
                tpStart = 0;
                tpPos = 0;
                tpSelectPos = 0;

                menuState = 11;
                tpSelectPos = 0;
                tpEnd = texturePackList.size();
                tpMax = 3;
                ScanTexturePacks("Assets/Textures/");
            }
            if(selectPos == 4)
            {
                ScanSkins();
                skinPos = 0;
                skinStart = 0;
                const char* current = TextureHelper::Instance()->PlayerSkinName();
                for(int i = 0; i < (int)skinList.size(); i++)
                {
                    if(skinList[i] == current)
                        skinPos = i;
                }
                skinStart = 0;
                skinFocus = 0;
                skinTab = 0;
                skinSpin = 0.45f;
                skinMessage = "";
                skinReturnState = menuState;
                menuState = 13;
            }
        }
    }
    break;
    case 13://skins
    {
        const int columns = 4;
        const int rows = 3;
        int count = (int)skinList.size();

        if(skinFocus == 1)
        {
            // The preview: left and right (pad or stick) turn the figure, Cross equips, Circle
            // hands the pad back to the grid.
            float stickX = 0.0f;
            float stickY = 0.0f;
            mSystemMgr->GetAnalogDeadzone(0.25f, stickX, stickY);
            if(mSystemMgr->KeyHold(PSP_CTRL_LEFT))
                skinSpin -= 0.06f;
            if(mSystemMgr->KeyHold(PSP_CTRL_RIGHT))
                skinSpin += 0.06f;
            skinSpin += stickX * 0.08f;
            if(skinSpin > 2.0f * PI)
                skinSpin -= 2.0f * PI;
            if(skinSpin < 0.0f)
                skinSpin += 2.0f * PI;

            if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                skinFocus = 0;
                mSoundMgr->PlayMenuSound();
            }
            else if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                mSoundMgr->PlayMenuSound2();
                if(skinPos >= 0 && skinPos < count)
                {
                    const std::string& picked = skinList[skinPos];
                    const char* file = (picked == "Steve") ? "Steve" : picked.c_str();
                    if(TextureHelper::Instance()->SetPlayerSkin(file))
                        skinMessage = "";
                    else
                        skinMessage = "Not a 64x32 or 64x64 skin";
                }
            }
        }
        else if(skinFocus == 2)
        {
            // The side tabs: the skin library and the memory stick import.
            if(mSystemMgr->KeyPressed(PSP_CTRL_UP) && skinTab > 0)
            {
                skinTab--;
                mSoundMgr->PlayMenuSound();
            }
            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN) && skinTab < 1)
            {
                skinTab++;
                mSoundMgr->PlayMenuSound();
            }
            if(mSystemMgr->KeyPressed(PSP_CTRL_RIGHT))
            {
                skinFocus = 0;
                mSoundMgr->PlayMenuSound();
            }
            if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                mSoundMgr->PlayMenuSound2();
                if(skinTab == 1)
                {
                    skinTab = 0;
                    skinFocus = 0;
                    OpenSkinImport();
                }
                else
                {
                    skinFocus = 0;
                }
            }
            else if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                menuState = skinReturnState;
                mSoundMgr->PlayMenuSound();
            }
        }
        else
        {
            int col = skinPos % columns;
            if(mSystemMgr->KeyPressed(PSP_CTRL_LEFT))
            {
                if(col > 0)
                    skinPos--;
                else
                {
                    skinFocus = 2;
                    skinTab = 0;
                }
                mSoundMgr->PlayMenuSound();
            }
            if(mSystemMgr->KeyPressed(PSP_CTRL_RIGHT))
            {
                if(col + 1 < columns && skinPos + 1 < count)
                    skinPos++;
                else
                    skinFocus = 1;
                mSoundMgr->PlayMenuSound();
            }
            if(mSystemMgr->KeyPressed(PSP_CTRL_UP) && skinPos - columns >= 0)
            {
                skinPos -= columns;
                mSoundMgr->PlayMenuSound();
            }
            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
            {
                if(skinPos + columns < count)
                    skinPos += columns;
                else if(skinPos / columns < (count - 1) / columns)
                    skinPos = count - 1;
                mSoundMgr->PlayMenuSound();
            }
            if(mSystemMgr->KeyPressed(PSP_CTRL_SQUARE))
            {
                OpenSkinImport();
                mSoundMgr->PlayMenuSound();
            }
            if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                skinFocus = 1;
                mSoundMgr->PlayMenuSound2();
            }
            else if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                menuState = skinReturnState;
                mSoundMgr->PlayMenuSound();
            }
        }

        int row = skinPos / columns;
        if(row < skinStart)
            skinStart = row;
        if(row >= skinStart + rows)
            skinStart = row - rows + 1;
    }
    break;
    case 14://import a skin from the memory stick
    {
        int count = (int)skinBrowseNames.size();
        if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
        {
            if(skinBrowsePos > 0)
            {
                skinBrowsePos--;
                if(skinBrowsePos < skinBrowseStart)
                    skinBrowseStart = skinBrowsePos;
                mSoundMgr->PlayMenuSound();
            }
        }
        if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
        {
            if(skinBrowsePos + 1 < count)
            {
                skinBrowsePos++;
                if(skinBrowsePos >= skinBrowseStart + 6)
                    skinBrowseStart = skinBrowsePos - 5;
                mSoundMgr->PlayMenuSound();
            }
        }
        if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
        {
            if(skinBrowsePath == "ms0:/" || skinBrowsePath == "ef0:/")
            {
                menuState = 13;
            }
            else
            {
                std::string path = skinBrowsePath;
                if(path.size() > 1 && path[path.size() - 1] == '/')
                    path.erase(path.size() - 1);
                size_t slash = path.find_last_of('/');
                if(slash == std::string::npos || slash < 4)
                    skinBrowsePath = "ms0:/";
                else
                    skinBrowsePath = path.substr(0, slash + 1);
                ScanSkinImport();
            }
            mSoundMgr->PlayMenuSound();
        }
        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(skinBrowsePos >= 0 && skinBrowsePos < count)
            {
                if(skinBrowseDir[skinBrowsePos])
                {
                    skinBrowsePath += skinBrowseNames[skinBrowsePos];
                    skinBrowsePath += "/";
                    ScanSkinImport();
                    mSoundMgr->PlayMenuSound();
                }
                else
                {
                    ImportHighlightedSkin();
                    mSoundMgr->PlayMenuSound2();
                }
            }
        }
    }
    break;
    case 1: //select world
    {
        if(saveSubmenu)
        {
            if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
            {
                saveSubMenuSelect--;
                if(saveSubMenuSelect < 1)
                    saveSubMenuSelect = 2;

                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
            {
                saveSubMenuSelect++;
                if(saveSubMenuSelect > 2)
                    saveSubMenuSelect = 1;

                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                if(saveSubMenuSelect == 1)//remove file
                {
                    //remove file
                    if(saveFilesList.size() > 0)
                    {
                        if(saveFilesList[loadSavePos].compression)
                        {
                            //remove normal file
                            remove(saveFilesList[loadSavePos].fileName.c_str());

                            //remove the streamed chunk folder Save/worldN.lmsd
                            CraftWorld::RemoveChunkDir(saveFilesList[loadSavePos].fileName);

                            //remove compressed one
                            saveFilesList[loadSavePos].fileName += "c";
                            remove(saveFilesList[loadSavePos].fileName.c_str());

                        }
                        else
                        {
                            remove(saveFilesList[loadSavePos].fileName.c_str());
                        }

                        ScanSaveFiles("Save/");

                        loadSavePos = 0;
                        loadSaveStart = 0;
                        loadSaveEnd = saveFilesList.size();
                        loadSaveMax = 4;
                        if(loadSaveMax > loadSaveEnd)
                            loadSaveMax = loadSaveEnd;

                        saveSubmenu = false;
                        saveSubMenuSelect = 2;

                        if(saveFilesList.empty() == false)
                        {
                            loadSelectPos = 0;
                        }
                        else
                        {
                            loadSelectPos = 2;
                        }
                    }
                }
                if(saveSubMenuSelect == 2)//return
                {
                    saveSubMenuSelect = 2;
                    saveSubmenu = false;
                }
            }
        }
        else
        {
			if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
			{
				if(selectPos == 0)
				{
					mSoundMgr->PlayMenuSound2();
				}
				if(selectPos == 1)
				{
					mSoundMgr->PlayMenuSound2();
				}
				if(selectPos == 2)
				{
					mSoundMgr->PlayMenuSound2();
				}
				if(selectPos == 3)
				{
					mSoundMgr->PlayMenuSound2();
				}
			}
            if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
            {
                if(loadSelectPos == 1 || loadSelectPos == 2)
                {
                    loadSelectPos = 0;
                    mSoundMgr->PlayMenuSound();
                }
                if(loadSelectPos == 4)
                {
                    loadSelectPos = 3;
                    mSoundMgr->PlayMenuSound();
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
            {
                if(loadSelectPos == 0 || loadSelectPos == 3)
                {
                    loadSelectPos++;
                    mSoundMgr->PlayMenuSound();
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_LEFT))
            {
                if(saveFilesList.empty() == false)
                {
                    switch(loadSelectPos)
                    {
                        case 2:
                        loadSelectPos = 1;
                        mSoundMgr->PlayMenuSound();
                        break;
                        case 3:
                        loadSelectPos = 0;
                        mSoundMgr->PlayMenuSound();
                        break;
                        case 4:
                        loadSelectPos = 2;
                        mSoundMgr->PlayMenuSound();
                        break;
                    }
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_RIGHT))
            {
                switch(loadSelectPos)
                {
                    case 0:
                    loadSelectPos = 3;
                    mSoundMgr->PlayMenuSound();
                    break;
                    case 1:
                    loadSelectPos = 2;
                    mSoundMgr->PlayMenuSound();
                    break;
                    case 2:
                    loadSelectPos = 4;
                    mSoundMgr->PlayMenuSound();
                    break;
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_LTRIGGER))
            {
                if(saveFilesList.empty() == false)
                {
                    loadSavePos++;
                    if(loadSavePos == loadSaveMax)
                    {
                        loadSaveStart++;
                        loadSaveMax++;
                        if(loadSaveMax > loadSaveEnd)
                        {
                            loadSaveStart = loadSaveEnd - 4;
                            if(loadSaveStart < 0)
                                loadSaveStart = 0;
                            loadSaveMax = loadSaveEnd;
                        }
                    }
                    if(loadSavePos >= saveFilesList.size())
                    {
                        loadSavePos = 0;
                        loadSaveStart = 0;
                        loadSaveMax = 4;
                        if(loadSaveMax > loadSaveEnd)
                            loadSaveMax = loadSaveEnd;
                    }
				}
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_RTRIGGER))
            {
                /*if(saveFilesList.empty() == false)
                {
                    loadSavePos++;
                    if(loadSavePos == loadSaveMax)
                    {
                        loadSaveStart++;
                        loadSaveMax++;
                        if(loadSaveMax > loadSaveEnd)
                        {
                            loadSaveStart = loadSaveEnd - 4;
                            if(loadSaveStart < 0)
                                loadSaveStart = 0;
                            loadSaveMax = loadSaveEnd;
                        }
                    }
                    if(loadSavePos >= saveFilesList.size())
                    {
                        loadSavePos = 0;
                        loadSaveStart = 0;
                        loadSaveMax = 4;
                        if(loadSaveMax > loadSaveEnd)
                            loadSaveMax = loadSaveEnd;
                    }
                }*/
				menuState = -2;
				mSoundMgr->PlayMenuSound3();
				//checking for new update
                NetworkManager::Instance()->Init();
                // The stack first, then a connection to an access point through the system's own
                // dialog. Both branches here used to be reported backwards -- the success path printed
                // "Failed to connect to network" and the failure path printed "Connected to Network" --
                // which is worth naming, because it is exactly the kind of bug that gets a working
                // feature reported as broken and a broken one reported as working.
                if(!NetworkManager::Instance()->InitializeConnection())
                {
                    SystemManager::Instance()->ShowMessage("Could not start networking.");
                }
                else if(SystemManager::Instance()->ShowNetworkDialog())
                {
                    SystemManager::Instance()->ShowMessage("Connected to Network.");
                }
                else
                {
                    SystemManager::Instance()->ShowMessage("You must connect to an access point.");
                }

                // Deliberately left connected rather than torn down. The updater shares this stack and
                // will not tear down what it did not bring up (see Update::NetShutdown), so a player
                // who connects here is not asked to connect a second time in the update screen.
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                menuState = 0;
				mSoundMgr->PlayMenuSound3();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                if(loadSelectPos == 0)//play selected world
                {
                    if(saveFilesList.size() > 0)
                    {
                        if(saveFilesList[loadSavePos].locked == false)
                        {
                            if(saveFilesList[loadSavePos].worldGameMode == SURVIVAL || saveFilesList[loadSavePos].worldGameMode == HARDCORE)
                            {
                                StatePlay *statePlay = new StatePlay();
                                statePlay->LoadMap(saveFilesList[loadSavePos].fileName,saveFilesList[loadSavePos].compression);
                                statePlay->InitCamera();
                                sManager->PushState(statePlay);
                            }
                            else
                            {
                                StatePlayCreative *statePlay = new StatePlayCreative();
                                statePlay->LoadMap(saveFilesList[loadSavePos].fileName,saveFilesList[loadSavePos].compression);
                                statePlay->InitCamera();
                                sManager->PushState(statePlay);
                            }
                        }
                    }
                }
                if(loadSelectPos == 1)//rename world
                {
                    if(saveFilesList.size() > 0)
                    {
                        char worldNameTemp[50];

                        unsigned short test[128];
                        unsigned short opis[10] = {'W','o','r','l','d',' ','N','a','m','e'};
                        if(mSystemMgr->ShowOSK(opis,test,128) != -1)
                        {
                            std::string newWorldName = "";
                            for(int j = 0; test[j]; j++)
                            {
                                unsigned c = test[j];

                                if(32 <= c && c <= 127) // print ascii only
                                    newWorldName += c;
                            }

                            sprintf(worldNameTemp,"%s",newWorldName.c_str());
                        }

                        int saveVersionTemp = 3;
                        char worldGameModeTemp = saveFilesList[loadSavePos].worldGameMode;
                        bool locked = saveFilesList[loadSavePos].locked;

                        FILE * pFile;
                        pFile = fopen(saveFilesList[loadSavePos].fileName.c_str(),"wb");

                        if(pFile != NULL)
                        {
                            //version
                            fwrite(&saveVersionTemp, sizeof(int),1,pFile);

                            fwrite(&worldGameModeTemp, sizeof(char),1,pFile);

                            fwrite(&locked, sizeof(bool),1,pFile);
                            //name
                            fwrite(worldNameTemp ,sizeof(char),50,pFile);

                            fclose(pFile);
                        }

                        ScanSaveFiles("Save/");
                    }
                }
                if(loadSelectPos == 2)//delete world
                {
                    if(saveFilesList.size() > 0)
                    {
                        saveSubmenu = true;
                        saveSubMenuSelect = 2;

						mSoundMgr->PlayMenuSound2();
                    }
                }
                if(loadSelectPos == 3)//create new world
                {
                    generateSelectPose = 0;
                    menuState = 5;

                    makeTrees = true;
                    makeWater = true;
                    makeCaves = true;
                    terrainBuilder = 0;
                    worldType = 0;

                    newWorldName = "New World";
                    newWorldSeed = "";
                }
                if(loadSelectPos == 4)//cancel
                {
                    menuState = 0;
                }
				if(loadSelectPos == 2)//Multiplayer
                {
                    if(saveFilesList.size() > 0)
                    {
                        saveSubmenu = true;
                        saveSubMenuSelect = 2;

						mSoundMgr->PlayMenuSound2();
                    }
                }
            }
        }
    }
    break;
    case 3://about
    {
        // A three-rung ladder, the same shape the launcher's own changelog uses:
        //   buttons  --Up-->  box selected  --X-->  scrolling
        //      ^                  |                    |
        //      +------ Down ------+------- O ---------+
        //
        // Up and Down still move between the two buttons; one more Up past the topmost leaves the
        // buttons for the box above them. The scroll step is its own rung rather than something Up
        // and Down do while standing on the box, and that separation is deliberate: if the same keys
        // both stepped between rungs and scrolled the text, the changelog would slide every time the
        // cursor left the box, and the only way back down to the buttons would be to scroll the whole
        // thing to the bottom first.
        if(aboutFocus == 2)
        {
            if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
            {
                if(newsScroll > 0)
                    newsScroll--;
                LogNewsState(aboutFocus, newsScroll, aboutPos);
                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
            {
                if(newsScroll < NewsMaxScroll())
                    newsScroll++;
                LogNewsState(aboutFocus, newsScroll, aboutPos);
                mSoundMgr->PlayMenuSound();
            }

            // O leaves the scroll step and puts the box back to merely selected, so the changelog
            // stays where it was scrolled to.
            if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                aboutFocus = 1;
                LogNewsState(aboutFocus, newsScroll, aboutPos);
                mSoundMgr->PlayMenuSound();
            }
            break;
        }

        if(aboutFocus == 1)
        {
            if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                aboutFocus = 2;
                LogNewsState(aboutFocus, newsScroll, aboutPos);
                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
            {
                aboutFocus = 0;
                LogNewsState(aboutFocus, newsScroll, aboutPos);
                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                aboutFocus = 0;
                LogNewsState(aboutFocus, newsScroll, aboutPos);
                mSoundMgr->PlayMenuSound();
            }
            break;
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
        {
            // Cancelling out of the box can land the cursor on either button, so only the topmost one
            // hands over to the changelog; from the lower one, Up just moves up to the top.
            if(aboutPos == 0)
                aboutFocus = 1;
            else
                aboutPos = 0;

            LogNewsState(aboutFocus, newsScroll, aboutPos);
            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
        {
            aboutPos++;
            if(aboutPos > 1)
                aboutPos = 0;

            LogNewsState(aboutFocus, newsScroll, aboutPos);
            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(aboutPos == 1)
            {
                menuState = 0;
            }
            if(aboutPos == 0)
            {
                menuState = 6;
                converterPos = 0;

                schematicExists = fileExists("Converter/world.schematic");
                ScanSaveFiles("Save/");
            }
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
        {
            menuState = 0;
			mSoundMgr->PlayMenuSound();
        }
    }
    break;
    case 5://parametric terrain
    {

        if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
        {
            mSoundMgr->PlayMenuSound();

            if (generateSelectPose == 0)
            {
                generateSelectPose = 4;
                return;
            }

            generateSelectPose--;
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
        {
            mSoundMgr->PlayMenuSound();
            if (generateSelectPose == 4)
            {
                generateSelectPose = 0;
                return;
            }
            generateSelectPose++;

        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
        {
            ScanSaveFiles("Save/");

            menuState = 1;
            loadSavePos = 0;
            loadSaveStart = 0;
            loadSaveEnd = saveFilesList.size();
            loadSaveMax = 4;
            if(loadSaveMax > loadSaveEnd)
            loadSaveMax = loadSaveEnd;

            menuState = 1;

            loadSelectPos = 3;
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(generateSelectPose == 0)
            {
                newWorldName = "";

                unsigned short test[128];
                unsigned short opis[10] = {'W','o','r','l','d',' ','N','a','m','e'};
                if(mSystemMgr->ShowOSK(opis,test,128) != -1)
                {
                    for(int j = 0; j < 14; j++)
                    {
                        unsigned c = test[j];

                        if(32 <= c && c <= 127) // print ascii only
                            newWorldName += c;
                    }
                }
                newWorldNamestr = newWorldName.c_str();
            }

            if(generateSelectPose == 1)
            {
                seed_1 = 0;
                newWorldSeed = "";

                unsigned short test[128];
                unsigned short opis[10] = {'W','o','r','l','d',' ','s','e','e','d'};
                if(mSystemMgr->ShowOSK(opis,test,128) != -1)
                {

                    for(int j = 0; j < 14; j++)
                    {
                        unsigned c = test[j];

                        if(c >= 32 && c <= 127)
                        {
                            newWorldSeed += c;
                        }
                    }
                    seed_1 = hash(newWorldSeed.c_str(),0);
                    //seed_1 = std::atoi(newWorldSeed.c_str());
                }
            }

            if(generateSelectPose == 2)
            {
                gameMode += 1;
                if (gameMode == 3)
                {
                    gameMode = 0;
                }
            }

            if(generateSelectPose == 3)
            {
                worldType += 1;
                // The Nether is no longer a world you can create -- it is somewhere you travel to --
                // so the cycle stops at Flat Ocean (7). World type 8 still loads a world that has it
                // saved; it is only the create screen that no longer offers it.
                if(worldType > 7)
                {
                    worldType = 0;
                }
            }

            if(generateSelectPose == 4)
            {
                //terrainBuilder = 2;
                if(gameMode == 0 || gameMode == 2)
                {
                    StatePlay *statePlay = new StatePlay();
                    statePlay->InitParametric(makeTrees,makeWater,makeCaves,seed_1,worldType,gameMode);
                    statePlay->InitCamera();
                    statePlay->SetWorldAndSaveName(newWorldName,nextSaveFileName);
                    sManager->PushState(statePlay);
                }
                else
                {
                    StatePlayCreative *statePlay = new StatePlayCreative();
                    statePlay->InitParametric(makeTrees,makeWater,makeCaves,seed_1,worldType,gameMode);
                    statePlay->InitCamera();
                    statePlay->SetWorldAndSaveName(newWorldName,nextSaveFileName);
                    sManager->PushState(statePlay);
                }
                seed_1 = 0;
            }
        }
    }
    break;
    case 6://converter
    {
        if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
        {
            converterPos--;
            if(converterPos < 0)
                converterPos = 2;

            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
        {
            converterPos++;
            if(converterPos > 2)
                converterPos = 0;

            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(converterPos == 0)
            {
                newW_gameMode == SURVIVAL ? newW_gameMode = CREATIVE : newW_gameMode = SURVIVAL;
            }
            if(converterPos == 1)
            {
                errorType = 0;
                if(schematicExists)
                {
                    char empty;
                    gzFile saveFile;

                    /// (1) FETCH SCHEMATIC WIDTH HEIGHT LENGTH AND TAKING ITS BLOCK IDS
                    saveFile = gzopen("Converter/world.schematic","rb");
                    if(saveFile == 0)
                        return;
                    //compound
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));

                    for(int i = 0; i < 9; i++)
                    {
                        gzread(saveFile, &empty,sizeof(char));
                        lol += empty;
                    }

                    //height
                    gzread(saveFile, &empty,sizeof(char));

                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));

                    for(int i = 0; i < 6; i++)
                    {
                        gzread(saveFile, &empty,sizeof(char));
                        lol += empty;
                    }
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &newW_height,sizeof(char));


                    //length
                    gzread(saveFile, &empty,sizeof(char));

                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));

                    for(int i = 0; i < 6; i++)
                    {
                        gzread(saveFile, &empty,sizeof(char));
                        lol += empty;
                    }
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &newW_length,sizeof(char));


                    //width
                    gzread(saveFile, &empty,sizeof(char));

                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));

                    for(int i = 0; i < 5; i++)
                    {
                        gzread(saveFile, &empty,sizeof(char));
                        lol += empty;
                    }
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &newW_width,sizeof(char));

                    if(newW_height + newW_length + newW_width != 252+112+252)
                    {
                        errorType = 1;
                        return;
                    }

                    //blocks
                    unsigned char* m_Array1;
                    m_Array1 = new unsigned char[252 * 112 * 252];
                    memset(m_Array1, 0, sizeof(unsigned char) * 252 * 252 * 112);

                    char need[6] = {0};
                    bool again = false;
                    for(int i = 0; i < 100000000; i++)
                    {
                        for(int j = 1; j <= 5; j++)
                        {
                            need[j-1] = need[j];
                        }
                        gzread(saveFile, &empty,sizeof(char));
                        need[5] = empty;

                        if(need[0] == 'B' &&
                           need[1] == 'l' &&
                           need[2] == 'o' &&
                           need[3] == 'c' &&
                           need[4] == 'k' &&
                           need[5] == 's')
                        {
                            i = 100000000;
                            break;
                        }

                        if(need[2] == 'D' &&
                           need[3] == 'a' &&
                           need[4] == 't' &&
                           need[5] == 'a')
                        {
                            gzread(saveFile, &empty,sizeof(char));
                            gzread(saveFile, &empty,sizeof(char));
                            gzread(saveFile, &empty,sizeof(char));
                            gzread(saveFile, &empty,sizeof(char));

                            gzread(saveFile, m_Array1,sizeof(unsigned char)*(252 * 112 * 252));

                            again = true;;
                            i = 100000000;
                            break;
                        }
                    }

                    if(again)
                    {
                        memset(m_Array1, 0, sizeof(unsigned char) * 252 * 252 * 112);
                        for(int j = 0; j < 6; j++) //need[6] has 6 bytes
                        {
                            need[j] = '\0';
                        }

                        for(int i = 0; i < 100000000; i++)
                        {
                            for(int j = 1; j <= 5; j++)
                            {
                                need[j-1] = need[j];
                            }
                            gzread(saveFile, &empty,sizeof(char));
                            need[5] = empty;

                            if(need[0] == 'B' &&
                               need[1] == 'l' &&
                               need[2] == 'o' &&
                               need[3] == 'c' &&
                               need[4] == 'k' &&
                               need[5] == 's')
                            {
                                i = 100000000;
                                break;
                            }
                        }
                    }
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));

                    gzread(saveFile, m_Array1,sizeof(unsigned char)*(252 * 112 * 252));

                    gzclose(saveFile);

                    // id converter
                    std::vector <unsigned char> id_map;
                    id_map.push_back(0); //0
                    id_map.push_back(RockBlock::getID());
                    id_map.push_back(GrassBlock::getID());
                    id_map.push_back(DirtBlock::getID());
                    id_map.push_back(CobbleStone::getID());
                    id_map.push_back(OakPlanks::getID());
                    id_map.push_back(OakSapling::getID());
                    id_map.push_back(bedrock::getID());
                    id_map.push_back(WaterBlock::getID());
                    id_map.push_back(WaterBlock::getID());
                    id_map.push_back(Lava::getID());
                    id_map.push_back(Lava::getID());
                    id_map.push_back(SandBlock::getID());
                    id_map.push_back(Gravel::getID());
                    id_map.push_back(GoldBlock::getID());
                    id_map.push_back(IronOre::getID());
                    id_map.push_back(CoalOre::getID());
                    id_map.push_back(WoodBlock::getID());
                    id_map.push_back(LeavesBlock::getID());
                    id_map.push_back(Sponge::getID());
                    id_map.push_back(GlassBlock::getID());
                    id_map.push_back(LapisOre::getID());
                    id_map.push_back(LapisBlock::getID());
                    id_map.push_back(0);
                    id_map.push_back(SandStone::getID());
                    id_map.push_back(NoteBlock::getID());
                    id_map.push_back(0); // bed
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(WhiteWoolBlock::getID()); // 35
                    id_map.push_back(0);
                    id_map.push_back(Flower1::getID());
                    id_map.push_back(Flower2::getID());
                    id_map.push_back(Mooshroom1::getID());
                    id_map.push_back(Mooshroom2::getID());
                    id_map.push_back(GoldBlock::getID());
                    id_map.push_back(bedrock::getID());
                    id_map.push_back(DoubleSlab::getID());
                    id_map.push_back(Slab::getID());
                    id_map.push_back(BrickBlock::getID());
                    id_map.push_back(TNTBlock::getID());
                    id_map.push_back(ShelfBlock::getID());
                    id_map.push_back(MossyCobblestone::getID());
                    id_map.push_back(Obsidian::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(BirchPlanks::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(DiamondOre::getID());
                    id_map.push_back(Diamond::getID());
                    id_map.push_back(CraftingTable::getID());
                    id_map.push_back(WheatBlock6::getID());
                    id_map.push_back(Soil::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(CobbleStone::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(RedstoneOre::getID());
                    id_map.push_back(RedstoneOre::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(Snow2::getID());
                    id_map.push_back(IceBlock::getID());
                    id_map.push_back(SnowBlock::getID());
                    id_map.push_back(CactusBlock::getID());
                    id_map.push_back(ClayBlock::getID());
                    id_map.push_back(CaneBlock::getID());
                    id_map.push_back(JukeBox::getID());
                    id_map.push_back(WoodenFence::getID());
                    id_map.push_back(Pumpkin1::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(JackOLantern1::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(StoneBrick::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(IronBars::getID());
                    id_map.push_back(GlassPanel::getID());
                    id_map.push_back(MelonBlock::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(BrickBlock::getID());
                    id_map.push_back(0);
                    id_map.push_back(GrassBlock::getID());
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(0);
                    id_map.push_back(OakHalfBlock::getID());


                    unsigned char* m_Array2;
                    m_Array2 = new unsigned char[252 * 112 * 252];
                    memset(m_Array2, 0, sizeof(unsigned char) * 252 * 252 * 112);

                    for(int x = 0; x < 252; x++)
                    {
                        for(int z = 0; z < 252; z++)
                        {
                            for(int y = 0; y < 112; y++)
                            {
                                int take_adress = x+z*252+y*252*252;
                                int take_id = m_Array1[take_adress];

                                int put_adress = x+y*252+z*252*112;
                                int put_id = 0;

                                if(take_id < id_map.size())
                                {
                                    put_id = id_map[take_id];
                                }

                                m_Array2[put_adress] = put_id;
                            }
                        }
                    }

                    // creating height map
                    unsigned char* m_Heightmap;
                    m_Heightmap = new unsigned char[252 * 252];
                    memset(m_Heightmap, 0, sizeof(unsigned char) * 252 * 252);

                    for(int x = 0; x < 252; x++)
                    {
                        for(int z = 0; z < 252; z++)
                        {
                            int adress = x+z*252;
                            for(int y = 111; y >= 0; y--)
                            {
                                int block_adress = x+y*252+z*252*112;
                                unsigned char& block = m_Array2[block_adress];

                                if(block == DirtBlock::getID() || block == GrassBlock::getID() || block == SandBlock::getID() || block == RockBlock::getID() || block == Gravel::getID() || block == SandStone::getID())
                                {
                                    m_Heightmap[adress] = y;
                                    y = -1;
                                }
                            }
                        }
                    }

                    // delete caves
                    for(int x = 0; x < 252; x++)
                    {
                        for(int z = 0; z < 252; z++)
                        {
                            int adress = x+z*252;
                            int height = m_Heightmap[adress];
                            for(int y = 0; y <= height-1; y++)
                            {
                                int block_adress = x+y*252+z*252*112;
                                unsigned char& block = m_Array2[block_adress];


                                if(block != 0)
                                {
                                    if(block != bedrock::getID() && block != RockBlock::getID() &&  block != DirtBlock::getID() && block != GoldBlock::getID() && block != IronOre::getID() && block != LapisOre::getID() && block != RedstoneOre::getID()
                                       && block != Lava::getID() && block != WaterBlock::getID() && block != RedstoneOre::getID() && block != CoalOre::getID() && block != Gravel::getID() && block != DiamondOre::getID()
                                       && block != SandStone::getID() && block != SandBlock::getID() && block != Mooshroom1::getID() && block != Mooshroom2::getID())
                                    {
                                        y = 100;
                                        continue;
                                    }
                                }
                                if(block == 0 || block == Lava::getID() || block == WaterBlock::getID())
                                {
                                    block = RockBlock::getID();
                                }

                            }
                        }
                    }
                    delete[] m_Heightmap;


                    /// (2) FETCH SCHEMATIC DATA
                    saveFile = gzopen("Converter/world.schematic","rb");
                    if(saveFile == 0)
                        return;

                    for(int j = 0; j < 6; j++) //need[6] has 6 bytes
                    {
                        need[j] = '\0';
                    }

                    //data
                    again = false;
                    for(int i = 0; i < 100000000; i++)
                    {
                        for(int j = 1; j <= 5; j++)
                        {
                            need[j-1] = need[j];
                        }
                        gzread(saveFile, &empty,sizeof(char));
                        need[5] = empty;

                        if(need[0] == 'B' &&
                           need[1] == 'l' &&
                           need[2] == 'o' &&
                           need[3] == 'c' &&
                           need[4] == 'k' &&
                           need[5] == 's')
                        {
                            again = true;
                            i = 100000000;

                            gzread(saveFile, &empty,sizeof(char));
                            gzread(saveFile, &empty,sizeof(char));
                            gzread(saveFile, &empty,sizeof(char));
                            gzread(saveFile, &empty,sizeof(char));

                            gzread(saveFile, m_Array1,sizeof(unsigned char)*(252 * 112 * 252));

                            break;
                        }

                        if(need[2] == 'D' &&
                           need[3] == 'a' &&
                           need[4] == 't' &&
                           need[5] == 'a')
                        {
                            i = 100000000;
                            break;
                        }
                    }

                    if(again)
                    {
                        memset(m_Array1, 0, sizeof(unsigned char) * 252 * 252 * 112);
                        for(int j = 0; j < 6; j++) //need[6] has 6 bytes
                        {
                            need[j] = '\0';
                        }

                        for(int i = 0; i < 100000000; i++)
                        {
                            for(int j = 1; j <= 5; j++)
                            {
                                need[j-1] = need[j];
                            }
                            gzread(saveFile, &empty,sizeof(char));
                            need[5] = empty;

                            if(need[2] == 'D' &&
                               need[3] == 'a' &&
                               need[4] == 't' &&
                               need[5] == 'a')
                            {
                                i = 100000000;
                                break;
                            }
                        }
                    }

                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));
                    gzread(saveFile, &empty,sizeof(char));

                    memset(m_Array1, 0, sizeof(unsigned char) * 252 * 252 * 112);
                    gzread(saveFile, m_Array1,sizeof(unsigned char)*(252 * 112 * 252));

                    gzclose(saveFile);

                    // place correct blocks in our world using schematic blocks data
                    for(int x = 0; x < 252; x++)
                    {
                        for(int z = 0; z < 252; z++)
                        {
                            for(int y = 0; y < 112; y++)
                            {
                                int address = x+y*252+z*252*112;
                                int data_address = x+z*252+y*252*252;
                                unsigned char& block = m_Array2[address];
                                unsigned char data = m_Array1[data_address];

                                if(y < 111)
                                {
                                    int up_address = x+(y+1)*252+z*252*112;
                                    unsigned char& up_block = m_Array2[up_address];

                                    if(up_block == Snow2::getID() && (block == GrassBlock::getID() || block == DirtBlock::getID()))
                                    {
                                        block = SnowSoil::getID();
                                        continue;
                                    }
                                }

                                if(block == LeavesBlock::getID())
                                {
                                    if(data == 1 || data == 5 || data == 9 || data == 13)
                                    {
                                        block = SpruceLeaves::getID();
                                    }
                                    if(data == 2 || data == 6 || data == 10 || data == 14)
                                    {
                                        block = BirchLeaves::getID();
                                    }
                                    continue;
                                }

                                if(block == OakPlanks::getID())
                                {
                                    if(data == 1)
                                    {
                                        block = SprucePlanks::getID();
                                    }
                                    if(data == 2)
                                    {
                                        block = BirchPlanks::getID();
                                    }
                                    continue;
                                }
                                if(block == WoodBlock::getID())
                                {
                                    if(data == 1)
                                    {
                                        block = DarkWoodBlock::getID();
                                    }
                                    if(data == 2)
                                    {
                                        block = WhiteWoodBlock::getID();
                                    }
                                    continue;
                                }
                                if(block == WheatBlock6::getID())
                                {
                                    if(data == 0)
                                    {
                                        block = WheatBlock1::getID();
                                    }
                                    if(data == 1 || data == 2)
                                    {
                                        block = WheatBlock2::getID();
                                    }
                                    if(data == 3 || data == 4)
                                    {
                                        block = WheatBlock3::getID();
                                    }
                                    if(data >= 5)
                                    {
                                        block = WheatBlock4::getID() + (data-5);
                                    }
                                    continue;
                                }

                                if(block == DoubleSlab::getID())
                                {
                                    if(data == 1)
                                    {
                                        block = SandStone::getID();
                                    }
                                    if(data == 2)
                                    {
                                        block = OakPlanks::getID();
                                    }
                                    if(data == 3)
                                    {
                                        block = CobbleStone::getID();
                                    }
                                    if(data == 4)
                                    {
                                        block = BrickBlock::getID();
                                    }
                                    if(data == 5)
                                    {
                                        block = StoneBrick::getID();
                                    }
                                    continue;
                                }
                                if(block == Slab::getID())
                                {
                                    if(data == 2)
                                    {
                                        block = OakHalfBlock::getID();
                                    }
                                    if(data == 3)
                                    {
                                        block = CobbleHalfBlock::getID();
                                    }
                                    if(data == 4)
                                    {
                                        block = BrickHalfBlock::getID();
                                    }
                                    if(data == 5)
                                    {
                                        block = HalfStoneBrick::getID();
                                    }
                                    continue;
                                }
                                if(block == OakHalfBlock::getID())
                                {
                                    if(data == 1)
                                    {
                                        block = SpruceHalfBlock::getID();
                                    }
                                    if(data == 2)
                                    {
                                        block = BirchHalfBlock::getID();
                                    }
                                    continue;
                                }
                                if(block == StoneBrick::getID())
                                {
                                    if(data == 2)
                                    {
                                        block = CrackedStoneBrick::getID();
                                    }
                                    if(data == 3)
                                    {
                                        block = CarvedStoneBrick::getID();
                                    }
                                    continue;
                                }
                                if(block == Pumpkin1::getID())
                                {
                                    block = Pumpkin1::getID() + data;
                                    continue;
                                }
                                if(block == WhiteWoolBlock::getID())
                                {
                                    if(data == 1)
                                    {
                                        block = OrangeWoolBlock::getID();
                                    }
                                    if(data == 2)
                                    {
                                        block = VioletWoolBlock::getID();
                                    }
                                    if(data == 3)
                                    {
                                        block = LightBlueWoolBlock::getID();
                                    }
                                    if(data == 4)
                                    {
                                        block = YellowWoolBlock::getID();
                                    }
                                    if(data == 5)
                                    {
                                        block = GreenWoolBlock::getID();
                                    }
                                    if(data == 6)
                                    {
                                        block = PinkWoolBlock::getID();
                                    }
                                    if(data == 7)
                                    {
                                        block = GrayWoolBlock::getID();
                                    }
                                    if(data == 8)
                                    {
                                        block = LightGrayWoolBlock::getID();
                                    }
                                    if(data == 9)
                                    {
                                        block = PastelWoolBlock::getID();
                                    }
                                    if(data == 10)
                                    {
                                        block = VioletWoolBlock::getID();
                                    }
                                    if(data == 11)
                                    {
                                        block = BlueWoolBlock::getID();
                                    }
                                    if(data == 12)
                                    {
                                        block = BrownWoolBlock::getID();
                                    }
                                    if(data == 13)
                                    {
                                        block = DarkGreenWoolBlock::getID();
                                    }
                                    if(data == 14)
                                    {
                                        block = RedWoolBlock::getID();
                                    }
                                    if(data == 15)
                                    {
                                        block = BlackWoolBlock::getID();
                                    }
                                    continue;
                                }
                            }
                        }
                    }

                    delete[] m_Array1;
                    saveFile=0;

                    /// (2.1) SET SUITABLE PLAYER POSITION

                    Vector3 playerPos = Vector3(0,0,0);
                    for(int y = 111; y >= 40; y--)
                    {
                        int address = 128+y*252+128*252*112;
                        unsigned char block = m_Array2[address];

                        if(block != 0)
                        {
                            playerPos = Vector3(128.5f,y+1.7f,128.5f);
                            break;
                        }
                    }

                    /// (3) SAVE WORLD
                    std::string filename = nextSaveFileName;

                    //first save version and name on normal file
                    FILE * pFile;
                    pFile = fopen(filename.c_str(),"wb");

                    if(pFile == NULL)
                        return;

                    //version
                    int _worldVersion = 150;
                    fwrite(&_worldVersion,sizeof(int),1,pFile);

                    //game mode
                    fwrite(&newW_gameMode,sizeof(char),1,pFile);

                    //game mode
                    bool locked = 0;
                    fwrite(&locked,sizeof(bool),1,pFile);

                    //name
                    char _worldName[50];
                    _worldName[0] = 'C';
                    _worldName[1] = 'o';
                    _worldName[2] = 'n';
                    _worldName[3] = 'v';
                    _worldName[4] = 'e';
                    _worldName[5] = 'r';
                    _worldName[6] = 't';
                    _worldName[7] = 'e';
                    _worldName[8] = 'd';
                    _worldName[9] = 1+rand()%126;
                    _worldName[10] = '\0';
                    fwrite(_worldName,sizeof(char),50,pFile);

                    //close file
                    fclose(pFile);


                    //now save compressed map info
                    filename = filename + "c";

                    saveFile = gzopen(filename.c_str(),"wb");
                    if(saveFile == 0)
                        return;

                    //player pos

                    gzwrite(saveFile, &playerPos.x,sizeof(float));
                    gzwrite(saveFile, &playerPos.y,sizeof(float));
                    gzwrite(saveFile, &playerPos.z,sizeof(float));

                    //player spawn pos
                    Vector3 playerSpawnPointPosition = playerPos;
                    gzwrite(saveFile, &playerSpawnPointPosition.x,sizeof(float));
                    gzwrite(saveFile, &playerSpawnPointPosition.y,sizeof(float));
                    gzwrite(saveFile, &playerSpawnPointPosition.z,sizeof(float));

                    //snow biome pos
                    Vector3 snowBiomePosition = Vector3(0,0,0);
                    float snowBiomeRadius = -1;
                    gzwrite(saveFile, &snowBiomePosition.x,sizeof(float));
                    gzwrite(saveFile, &snowBiomePosition.z,sizeof(float));
                    gzwrite(saveFile, &snowBiomeRadius,sizeof(float));

                    //desert biome pos
                    Vector3 desertBiomePosition = Vector3(0,0,0);
                    float desertBiomeRadius = -1;
                    gzwrite(saveFile, &desertBiomePosition.x,sizeof(float));
                    gzwrite(saveFile, &desertBiomePosition.z,sizeof(float));
                    gzwrite(saveFile, &desertBiomeRadius,sizeof(float));

                    //size
                    int WORLD_SIZE = 512;
                    gzwrite(saveFile, &WORLD_SIZE,sizeof(int));

                    //size
                    int WORLD_HEIGHT = 112;
                    gzwrite(saveFile, &WORLD_HEIGHT,sizeof(int));

                    //chunksize
                    int CHUNK_SIZE = 50;
                    gzwrite(saveFile, &CHUNK_SIZE,sizeof(int));

                    //cubes
                    gzwrite(saveFile, m_Array2,sizeof(unsigned char)*(WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE));

                    for(int x = 0; x < 252; x++)
                    {
                        for(int z = 0; z < 252; z++)
                        {
                            for(int y = 0; y < 112; y++)
                            {
                                m_Array2[z*112*252+y*252+x] = 0;
                            }
                        }
                    }

                    //settings
                    gzwrite(saveFile, m_Array2,sizeof(unsigned char)*(WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE));

                    float worldDayTime = 12.0f;
                    //world time
                    gzwrite(saveFile, &worldDayTime,sizeof(float));
                    //sun time
                    //gzwrite(saveFile, &sunTime,sizeof(float));

                    gzwrite(saveFile, &mainStatistics,sizeof(st));

                    gzwrite(saveFile, &mainOptions,sizeof(opt));


                    short invId[36];
                    short invAm[36];
                    bool invSt[36];
                    short armorId[4];
                    short armorAm[4];
                    bool armorSt[4];

                    for(int o = 0; o <= 35; o += 1)
                    {
                        invAm[o] = -1;
                        invId[o] = -1;
                        invSt[o] = false;
                    }

                    for(int o = 0; o <= 3; o += 1)
                    {
                        armorId[o] = 324+o;
                        armorAm[o] = -1;
                        armorSt[o] = false;
                    }
                    gzwrite(saveFile, &invId,sizeof(short)*(36));

                    gzwrite(saveFile, &invAm,sizeof(short)*(36));

                    gzwrite(saveFile, &invSt,sizeof(bool)*(36));

                    gzwrite(saveFile, &armorId,sizeof(short)*(4));

                    gzwrite(saveFile, &armorAm,sizeof(short)*(4));

                    gzwrite(saveFile, &armorSt,sizeof(bool)*(4));

                    float HP = 20;
                    gzwrite(saveFile, &HP,sizeof(unsigned int));

                    float HG = 20;
                    gzwrite(saveFile, &HG,sizeof(unsigned int));

                    float OS = 10;
                    gzwrite(saveFile, &OS,sizeof(unsigned int));

                    float brightFactor = 1.0f;
                    gzwrite(saveFile, &brightFactor,sizeof(float));

                    float starsFactor = 0.0f;
                    gzwrite(saveFile, &starsFactor,sizeof(float));

                    int worldSeed = 0;
                    gzwrite(saveFile, &worldSeed,sizeof(int));

                    float skyTime = 112.0f;
                    gzwrite(saveFile, &skyTime,sizeof(float));

                    int savedWeatherType = 0;
                    gzwrite(saveFile, &savedWeatherType,sizeof(int));

                    float savedWeatherDuration = 600+rand()%300;
                    gzwrite(saveFile, &savedWeatherDuration,sizeof(float));

                    int null_var = 0;
                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    gzwrite(saveFile, &null_var,sizeof(int));

                    //close file
                    gzclose(saveFile);
                    saveFile=0;

                    delete[] m_Array2;
                }
            }
            if(converterPos == 2)
            {
                menuState = 3;
                aboutFocus = 0;
                newsScroll = 0;
            }
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
        {
            menuState = 3;
            aboutFocus = 0;
            newsScroll = 0;
			mSoundMgr->PlayMenuSound3();
        }
    }
    break;
    case 10:
    {
        if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
        {
            saveSubMenuSelect == 0 ? saveSubMenuSelect = 1 : saveSubMenuSelect = 0;

            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
        {
            saveSubMenuSelect == 1 ? saveSubMenuSelect = 0 : saveSubMenuSelect = 1;

            mSoundMgr->PlayMenuSound();
        }

		if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(tpSelectPos == 0)//play state
            {
                if(texturePackList.empty() == false)
                {
                    TextureHelper::Instance()->SetTexturePack(texturePackList[tpPos].name);
                    tpCurrent = tpPos;
                }
            }
            if(tpSelectPos == 1)//delete save
            {
                menuState = 0;
            }
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            char worldNameTemp[50];
            for(int i = 0; i <= 49; i++)
            {
                worldNameTemp[i] = saveFilesList[loadSavePos].worldName[i];
            }

            int saveVersionTemp = 3;
            char worldGameModeTemp = saveSubMenuSelect;
            bool locked = saveFilesList[loadSavePos].locked;

            FILE * pFile;
            pFile = fopen(saveFilesList[loadSavePos].fileName.c_str(),"wb");

            if(pFile != NULL)
            {
                //version
                fwrite(&saveVersionTemp, sizeof(int),1,pFile);

                fwrite(&worldGameModeTemp, sizeof(char),1,pFile);

                fwrite(&locked, sizeof(bool),1,pFile);
                //name
                fwrite(worldNameTemp ,sizeof(char),50,pFile);

                fclose(pFile);
            }

            ScanSaveFiles("Save/");

            if(saveFilesList[loadSavePos].locked == true)
            {
                loadSelectPos = 1;
            }

            menuState = 1;
        }
    }
    break;
    case 11://tp
    {
        if(mSystemMgr->KeyPressed(PSP_CTRL_LEFT))
        {
            tpSelectPos --;
            if(tpSelectPos < 0)
            {
                tpSelectPos = 1;
            }
            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_RIGHT))
        {
            tpSelectPos ++;
            if(tpSelectPos > 1)
            {
                tpSelectPos = 0;
            }
            mSoundMgr->PlayMenuSound();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
        {
            tpPos--;
            if(tpPos < tpStart)
            {
                tpStart--;
                tpMax--;

                if(tpMax < 3)
                {
                    tpStart = 0;
                    tpMax = 3;
                }
            }

            if(tpPos < 0)
            {
                tpPos = texturePackList.size() - 1;
                tpMax = tpEnd;
                tpStart = tpEnd - 3;
                if(tpStart < 0)
                    tpStart = 0;
            }
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
        {
            tpPos++;
            if(tpPos == tpMax)
            {
                tpStart++;
                tpMax++;
                if(tpMax > tpEnd)
                {
                    tpStart = tpEnd - 3;
                    if(tpStart < 0)
                    {
                        tpStart = 0;
                    }
                    tpMax = tpEnd;
                }
            }
            if(tpPos >= texturePackList.size())
            {
                tpPos = 0;
                tpStart = 0;
                tpMax = 3;
                if(tpMax > tpEnd)
                    tpMax = tpEnd;
            }
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
        {
            menuState = 0;
			mSoundMgr->PlayMenuSound3();
        }

        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(tpSelectPos == 0)//play state
            {
                if(texturePackList.empty() == false)
                {
                    TextureHelper::Instance()->SetTexturePack(texturePackList[tpPos].name);
                    tpCurrent = tpPos;
                }
            }
            if(tpSelectPos == 1)//delete save
            {
                menuState = 0;
            }
        }
    }
    break;
	case 12: //HOST select world
    {
        if(saveSubmenu)
        {
            if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
            {
                saveSubMenuSelect--;
                if(saveSubMenuSelect < 1)
                    saveSubMenuSelect = 2;

                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
            {
                saveSubMenuSelect++;
                if(saveSubMenuSelect > 2)
                    saveSubMenuSelect = 1;

                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                if(saveSubMenuSelect == 1)//remove file
                {
                    //remove file
                    if(saveFilesList.size() > 0)
                    {
                        if(saveFilesList[loadSavePos].compression)
                        {
                            //remove normal file
                            remove(saveFilesList[loadSavePos].fileName.c_str());

                            //remove the streamed chunk folder Save/worldN.lmsd
                            CraftWorld::RemoveChunkDir(saveFilesList[loadSavePos].fileName);

                            //remove compressed one
                            saveFilesList[loadSavePos].fileName += "c";
                            remove(saveFilesList[loadSavePos].fileName.c_str());

                        }
                        else
                        {
                            remove(saveFilesList[loadSavePos].fileName.c_str());
                        }

                        ScanSaveFiles("Save/");

                        loadSavePos = 0;
                        loadSaveStart = 0;
                        loadSaveEnd = saveFilesList.size();
                        loadSaveMax = 4;
                        if(loadSaveMax > loadSaveEnd)
                            loadSaveMax = loadSaveEnd;

                        saveSubmenu = false;
                        saveSubMenuSelect = 2;

                        if(saveFilesList.empty() == false)
                        {
                            loadSelectPos = 0;
                        }
                        else
                        {
                            loadSelectPos = 2;
                        }
                    }
                }
                if(saveSubMenuSelect == 2)//return
                {
                    saveSubMenuSelect = 2;
                    saveSubmenu = false;
                }
            }
        }
        else
        {
			if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
			{
				if(selectPos == 0)
				{
					mSoundMgr->PlayMenuSound2();
				}
				if(selectPos == 1)
				{
					mSoundMgr->PlayMenuSound2();
				}
				if(selectPos == 2)
				{
					mSoundMgr->PlayMenuSound2();
				}
				if(selectPos == 3)
				{
					mSoundMgr->PlayMenuSound2();
				}
			}
            if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
            {
                if(loadSelectPos == 1 || loadSelectPos == 2)
                {
                    loadSelectPos = 0;
                    mSoundMgr->PlayMenuSound();
                }
                if(loadSelectPos == 4)
                {
                    loadSelectPos = 3;
                    mSoundMgr->PlayMenuSound();
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
            {
                if(loadSelectPos == 0 || loadSelectPos == 3)
                {
                    loadSelectPos++;
                    mSoundMgr->PlayMenuSound();
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_LEFT))
            {
                if(saveFilesList.empty() == false)
                {
                    switch(loadSelectPos)
                    {
                        case 2:
                        loadSelectPos = 1;
                        mSoundMgr->PlayMenuSound();
                        break;
                        case 3:
                        loadSelectPos = 0;
                        mSoundMgr->PlayMenuSound();
                        break;
                        case 4:
                        loadSelectPos = 2;
                        mSoundMgr->PlayMenuSound();
                        break;
                    }
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_RIGHT))
            {
                switch(loadSelectPos)
                {
                    case 0:
                    loadSelectPos = 3;
                    mSoundMgr->PlayMenuSound();
                    break;
                    case 1:
                    loadSelectPos = 2;
                    mSoundMgr->PlayMenuSound();
                    break;
                    case 2:
                    loadSelectPos = 4;
                    mSoundMgr->PlayMenuSound();
                    break;
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_LTRIGGER))
            {
                if(saveFilesList.empty() == false)
                {
                    loadSavePos--;

                    if(loadSavePos < loadSaveStart)
                    {
                        loadSaveStart--;
                        loadSaveMax--;

                        if(loadSaveMax < 4)
                        {
                            loadSaveStart = 0;
                            loadSaveMax = 4;
                        }
                    }

                    if(loadSavePos < 0)
                    {
                        loadSavePos = saveFilesList.size() - 1;
                        loadSaveMax = loadSaveEnd;
                        loadSaveStart = loadSaveEnd - 4;
                        if(loadSaveStart < 0)
                            loadSaveStart = 0;
                    }
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_RTRIGGER))
            {
                if(saveFilesList.empty() == false)
                {
                    loadSavePos++;
                    if(loadSavePos == loadSaveMax)
                    {
                        loadSaveStart++;
                        loadSaveMax++;
                        if(loadSaveMax > loadSaveEnd)
                        {
                            loadSaveStart = loadSaveEnd - 4;
                            if(loadSaveStart < 0)
                                loadSaveStart = 0;
                            loadSaveMax = loadSaveEnd;
                        }
                    }
                    if(loadSavePos >= saveFilesList.size())
                    {
                        loadSavePos = 0;
                        loadSaveStart = 0;
                        loadSaveMax = 4;
                        if(loadSaveMax > loadSaveEnd)
                            loadSaveMax = loadSaveEnd;
                    }
                }
				menuState = -2;
				mSoundMgr->PlayMenuSound3();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                menuState = 0;
				mSoundMgr->PlayMenuSound3();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                if(loadSelectPos == 0)//play selected world
                {
                    if(saveFilesList.size() > 0)
                    {
                        if(saveFilesList[loadSavePos].locked == false)
                        {
                            if(saveFilesList[loadSavePos].worldGameMode == SURVIVAL || saveFilesList[loadSavePos].worldGameMode == HARDCORE)
                            {
                                StatePlay *statePlay = new StatePlay();
                                statePlay->LoadMap(saveFilesList[loadSavePos].fileName,saveFilesList[loadSavePos].compression);
                                statePlay->InitCamera();
                                sManager->PushState(statePlay);
                            }
                            else
                            {
                                StatePlayCreative *statePlay = new StatePlayCreative();
                                statePlay->LoadMap(saveFilesList[loadSavePos].fileName,saveFilesList[loadSavePos].compression);
                                statePlay->InitCamera();
                                sManager->PushState(statePlay);
                            }
                        }
                    }
                }
                if(loadSelectPos == 1)//rename world
                {
                    if(saveFilesList.size() > 0)
                    {
                        char worldNameTemp[50];

                        unsigned short test[128];
                        unsigned short opis[10] = {'W','o','r','l','d',' ','N','a','m','e'};
                        if(mSystemMgr->ShowOSK(opis,test,128) != -1)
                        {
                            std::string newWorldName = "";
                            for(int j = 0; test[j]; j++)
                            {
                                unsigned c = test[j];

                                if(32 <= c && c <= 127) // print ascii only
                                    newWorldName += c;
                            }

                            sprintf(worldNameTemp,"%s",newWorldName.c_str());
                        }

                        int saveVersionTemp = 3;
                        char worldGameModeTemp = saveFilesList[loadSavePos].worldGameMode;
                        bool locked = saveFilesList[loadSavePos].locked;

                        FILE * pFile;
                        pFile = fopen(saveFilesList[loadSavePos].fileName.c_str(),"wb");

                        if(pFile != NULL)
                        {
                            //version
                            fwrite(&saveVersionTemp, sizeof(int),1,pFile);

                            fwrite(&worldGameModeTemp, sizeof(char),1,pFile);

                            fwrite(&locked, sizeof(bool),1,pFile);
                            //name
                            fwrite(worldNameTemp ,sizeof(char),50,pFile);

                            fclose(pFile);
                        }

                        ScanSaveFiles("Save/");
                    }
                }
                if(loadSelectPos == 2)//delete world
                {
                    if(saveFilesList.size() > 0)
                    {
                        saveSubmenu = true;
                        saveSubMenuSelect = 2;

						mSoundMgr->PlayMenuSound2();
                    }
                }
                if(loadSelectPos == 3)//create new world
                {
                    generateSelectPose = 0;
                    menuState = 5;

                    makeTrees = true;
                    makeWater = true;
                    makeCaves = true;
                    terrainBuilder = 0;
                    worldType = 0;

                    newWorldName = "New World";
                    newWorldSeed = "";
                }
                if(loadSelectPos == 4)//cancel
                {
                    menuState = 0;
                }
				if(loadSelectPos == 2)
                {
                    if(saveFilesList.size() > 0)
                    {
                        saveSubmenu = true;
                        saveSubMenuSelect = 2;

						mSoundMgr->PlayMenuSound2();
                    }
                }
            }
        }
    }
    break;
    }
}
void StateMenu::Update(StateManager* sManager)
{
    if(mSoundMgr)
    {
        mSoundMgr->AudioSync();
    }
}

void StateMenu::Draw(StateManager* sManager)
{
    // One line in Log.txt, on the first frame only: BootTrace::Stage ignores a name that is already
    // current. This is what separates "stopped while setting the menu up" from "stopped drawing it".
    BootTrace::Stage("menu drawing its first frame");

    //start rendering
    mRender->SetOrtho(0,0,0,0,0,0);
    mRender->StartFrame(1,1,1);

    switch(menuState)
    {
    /*case -1://language menu
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }
        // english
        buttonSprite->SetPosition(240,100);
        buttonSprite->Draw();

        // russian
        buttonSprite->SetPosition(240,130);
        buttonSprite->Draw();

        //selected button
        sbuttonSprite->SetPosition(240,(selectPos * 30) + 100);
        sbuttonSprite->Draw();

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        selectPos == 0 ? DrawText(240,109,GU_COLOR(1,1,0.25,1) ,default_size,"English") : DrawText(240,109,GU_COLOR(1,1,1,1) ,default_size,"English");
        selectPos == 1 ? DrawText(240,139,GU_COLOR(1,1,0.25,1) ,default_size,"Russian") : DrawText(240,139,GU_COLOR(1,1,1,1) ,default_size,"Russian");

        DrawText(240,24,GU_COLOR(1,1,1,1) ,default_size,"Choose your language");
    }
    break;*/

	case -3://Join Game
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        // Join
        buttonSprite->SetPosition(240,130);
        buttonSprite->Draw();
		
		//Back
		buttonSprite->SetPosition(240,160);
        buttonSprite->Draw();

        //selected button
        sbuttonSprite->SetPosition(240,(selectPos * 30) + 130);
        sbuttonSprite->Draw();

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        selectPos == 0 ? DrawText(240,139,GU_COLOR(1,1,0.25,1) ,default_size,"Refresh") : DrawText(240,139,GU_COLOR(1,1,1,1) ,default_size,"Join");
		selectPos == 1 ? DrawText(240,169,GU_COLOR(1,1,0.25,1) ,default_size,"Back") : DrawText(240,169,GU_COLOR(1,1,1,1) ,default_size,"Back");

        DrawText(240,24,GU_COLOR(1,1,1,1) ,default_size,"Join Game");
		DrawText(240,84,GU_COLOR(1,1,1,1) ,default_size,"No Games Found");
    }

	case -2://Multiplayer menu
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }
        // Host
        buttonSprite->SetPosition(240,130);
        buttonSprite->Draw();

        //Join
        buttonSprite->SetPosition(240,160);
        buttonSprite->Draw();

        //selected button
        sbuttonSprite->SetPosition(240,(selectPos * 15) + 130);
        sbuttonSprite->Draw();

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        selectPos == 0 ? DrawText(240,139,GU_COLOR(1,1,0.25,1) ,default_size,"Host") : DrawText(240,139,GU_COLOR(1,1,1,1) ,default_size,"Host");
        selectPos == 2 ? DrawText(240,169,GU_COLOR(1,1,0.25,1) ,default_size,"Join") : DrawText(240,169,GU_COLOR(1,1,1,1) ,default_size,"Join");

        DrawText(240,24,GU_COLOR(1,1,1,1) ,default_size,"Play Multiplayer");
		DrawText(240,70,GU_COLOR(1,1,1,1) ,default_big_size,"Ad Hoc Multiplayer Options");
    }
    break;
	    case 0://main menu
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        if(bx >= 360)
        {
            directionx = false;
        }
        if(bx <= 120)
        {
            directionx = true;
        }

        if(by >= 272-68)
        {
            directiony = false;
        }
        if(by <= 68)
        {
            directiony = true;
        }

        if(directionx == true)
        {
            bx += 1/6.0f;
        }
        else
        {
            bx -= 1/6.0f;
        }

        if(directiony == true)
        {
            by += 272.0f/960.0f/6.0f;
        }
        else
        {
            by -= 272.0f/960.0f/6.0f;
        }

		/*screen1Sprite->SetPosition(-480+(-920 + (10)),136);
        screen1Sprite->Draw();
        screen2Sprite->SetPosition(0+(-920 + (10)),136);
        screen2Sprite->Draw();
        screen3Sprite->SetPosition(480+(-920 + (10)),136);
        screen3Sprite->Draw();
        screen4Sprite->SetPosition(960+(-920 + (10)),136);
        screen4Sprite->Draw();*/


        backgroundSprite->SetPosition(bx,by);
        backgroundSprite->DrawLinear();
        //logo
        lamecraftSprite->Draw();

        //Play Game
        buttonSprite->SetPosition(240,90);
        buttonSprite->Draw();

        //News & Converter
        buttonSprite->SetPosition(240,120);
        buttonSprite->Draw();

        //Help & Options
        buttonSprite->SetPosition(240,150);
        buttonSprite->Draw();

        //Texture Packs
        buttonSprite->SetPosition(240,180);
        buttonSprite->Draw();

        //Skins
        buttonSprite->SetPosition(240,210);
        buttonSprite->Draw();

        //selected button
        sbuttonSprite->SetPosition(240,(selectPos * 30) + 90);
        sbuttonSprite->Draw();

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        splashSize += 0.08f;
        if(splashSize > 2*PI)
        {
            splashSize = 0.0f;
        }

        if(RenderManager::InstancePtr()->GetFontLanguage() == ENGLISH)
        {
            selectPos == 0 ? DrawText(240,99,GU_COLOR(1,1,0.25,1) ,default_size,"Play Game") : DrawText(240,99,GU_COLOR(1,1,1,1) ,default_size,"Play Game");
            selectPos == 1 ? DrawText(240,129,GU_COLOR(1,1,0.25,1) ,default_size,"Help & Options") : DrawText(240,129,GU_COLOR(1,1,1,1) ,default_size,"Help & Options");
			selectPos == 2 ? DrawText(240,159,GU_COLOR(1,1,0.25,1) ,default_size,"News & Converter") : DrawText(240,159,GU_COLOR(1,1,1,1) ,default_size,"News & Converter");
            selectPos == 3 ? DrawText(240,189,GU_COLOR(1,1,0.25,1) ,default_size,"Texture Packs") : DrawText(240,189,GU_COLOR(1,1,1,1) ,default_size,"Texture Packs");
            selectPos == 4 ? DrawText(240,219,GU_COLOR(1,1,0.25,1) ,default_size,"Skins") : DrawText(240,219,GU_COLOR(1,1,1,1) ,default_size,"Skins");
			DrawText(50,260,GU_COLOR(1,1,1,1) ,default_small_size,MC_VERSION_TITLE);
			DrawText(350,260,GU_COLOR(1,1,1,1) ,default_small_size,"Copyright Mojang AB. Do not distribute!");

            switch(SplashNumber)
            {
                case 0: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Made by Notch!"); break;
                case 1: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Fan fiction!"); break;
                case 2: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Call your mother!"); break;
                case 3: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Now in 3D!"); break;
                case 4: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Fat free!"); break;
                case 5: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Sublime!"); break;
				case 6: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Nice to meet you!"); break;
				case 7: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Meeting expectations!"); break;
				case 8: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"idspispopd!"); break;
				case 9: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"All inclusive!"); break;
				case 10: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Falling off cliffs!"); break;
				case 11: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"More polygons!"); break;
				case 12: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Scary!"); break;
				case 13: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Getting ready to know!"); break;
				case 14: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"sqrt(-1)love you!"); break;
				case 15: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Fan fiction!"); break;
				case 16: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Kaaneeeedaaaa!"); break;
				case 17: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Cloud computing!"); break;
				case 18: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Doesn't use the U-word!"); break;
				case 19: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Totally forgot about Dre!"); break;
				case 20: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Thats no moon!"); break;
				case 21: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Minors welcome!"); break;
				case 22: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Boot with the fur!"); break;
				case 23: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Woo, tigsource!"); break;
				case 24: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Conventional!"); break;
				case 25: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Finally complete!"); break;
				case 26: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Bigger than a bread box!"); break;
				case 27: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Full of stars!"); break;
				case 28: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Awesome community!"); break;
				case 29: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Give us Gordon!"); break;
				case 30: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Treatment for your rash!"); break;
				case 31: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Woo, 2pp!"); break;
				case 32: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Casual gaming!"); break;
				case 33: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Larger than Earth!"); break;
				case 34: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Exploding creepers!"); break;
				case 35: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"DAR! DAR! DAR!"); break;
				case 36: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Boats FTW"); break;
				case 37: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"As seen on TV!"); break;
				case 38: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Its's groundbreaking!"); break;
				case 39: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Menger sponge!"); break;
				case 40: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Uninflammable!"); break;
				case 41: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Check it out!"); break;
				case 42: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Funk soul brother!"); break;
				case 43: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Synecdoche!"); break;
				case 44: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Phobos snomaly!"); break;
				case 45: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Kind of dragon free!"); break;
				case 46: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Plz reply to my tweet!"); break;
				case 47: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Joel is neat!"); break;
				case 48: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Don't bother with the clones!"); break;
				case 49: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"The work of Notch!"); break;
				case 50: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"The creeper is a spy!"); break;
				case 51: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Never dig down!"); break;
				case 52: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Pumpakungen!"); break;
				case 53: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Scary!"); break;
				case 54: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Whoa, dude!"); break;
				case 55: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"I have a suggestion!"); break;
				case 56: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Throw a blanket over it!"); break;
				case 57: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"All inclusive!"); break;
				case 58: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Watter proof!"); break;
				case 59: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"4315162342 lines of code!"); break;
				case 60: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Holy cow, man!"); break;
            }
        }
    }
    break;
    case -1://main menu
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 0; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        if(bx >= 360)
        {
			directionx = true; 
        }
        if(bx <= 120)
        {
			directionx = false;
        }

        if(by >= 272-68)
        {
        }
        if(by <= 68)
        {
        }

        if(directionx == true)
        {
            bx -= 1/6.0f;
        }
        else
        {
            bx += 1/6.0f;
        }

        if(directiony == true)
        {
        }
        else
        {
        }


        backgroundSprite->SetPosition(bx,by);
        backgroundSprite->DrawLinear();
        //logo
        lamecraftSprite->Draw();

        //Play Game
        buttonSprite->SetPosition(240,90);
        buttonSprite->Draw();

        //News & Converter
        buttonSprite->SetPosition(240,120);
        buttonSprite->Draw();

        //Help & Options
        buttonSprite->SetPosition(240,150);
        buttonSprite->Draw();

        //Texture Packs
        buttonSprite->SetPosition(240,180);
        buttonSprite->Draw();

        //Skins
        buttonSprite->SetPosition(240,210);
        buttonSprite->Draw();

        //selected button
        sbuttonSprite->SetPosition(240,(selectPos * 30) + 90);
        sbuttonSprite->Draw();

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        splashSize += 0.08f;
        if(splashSize > 2*PI)
        {
            splashSize = 0.0f;
        }

        if(RenderManager::InstancePtr()->GetFontLanguage() == ENGLISH)
        {
			selectPos == 0 ? DrawText(240, 99, GU_COLOR(1, 1, 0.25, 1), default_size, "Play Game") : DrawText(240, 99, GU_COLOR(1, 1, 1, 1), default_size, "Play Game");
			selectPos == 1 ? DrawText(240, 129, GU_COLOR(1, 1, 0.25, 1), default_size, "Help & Options") : DrawText(240, 129, GU_COLOR(1, 1, 1, 1), default_size, "Help & Options");
			selectPos == 2 ? DrawText(240, 159, GU_COLOR(1, 1, 0.25, 1), default_size, "News & Converter") : DrawText(240, 159, GU_COLOR(1, 1, 1, 1), default_size, "News & Converter");
			selectPos == 3 ? DrawText(240, 189, GU_COLOR(1, 1, 0.25, 1), default_size, "Texture Packs") : DrawText(240, 189, GU_COLOR(1, 1, 1, 1), default_size, "Texture Packs");
			selectPos == 4 ? DrawText(240, 219, GU_COLOR(1, 1, 0.25, 1), default_size, "Skins") : DrawText(240, 219, GU_COLOR(1, 1, 1, 1), default_size, "Skins");
			DrawText(50, 260, GU_COLOR(1, 1, 1, 1), default_small_size, MC_VERSION_TITLE);
			DrawText(350, 260, GU_COLOR(1, 1, 1, 1), default_small_size, "Copyright Mojang AB. Do not distribute!");


            switch(SplashNumber)
            {
                case 0: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Made by Notch!"); break;
                case 1: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Fan fiction!"); break;
                case 2: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Call your mother!"); break;
                case 3: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Now in 3D!"); break;
                case 4: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Fat free!"); break;
                case 5: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Sublime!"); break;
				case 6: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Nice to meet you!"); break;
				case 7: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Meeting expectations!"); break;
				case 8: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"idspispopd!"); break;
				case 9: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"All inclusive!"); break;
				case 10: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Falling off cliffs!"); break;
				case 11: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"More polygons!"); break;
				case 12: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Scary!"); break;
				case 13: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Getting ready to know!"); break;
				case 14: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"sqrt(-1)love you!"); break;
				case 15: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Fan fiction!"); break;
				case 16: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Kaaneeeedaaaa!"); break;
				case 17: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Cloud computing!"); break;
				case 18: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Doesn't use the U-word!"); break;
				case 19: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Totally forgot about Dre!"); break;
				case 20: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Thats no moon!"); break;
				case 21: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Minors welcome!"); break;
				case 22: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Boot with the fur!"); break;
				case 23: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Woo, tigsource!"); break;
				case 24: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Conventional!"); break;
				case 25: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Finally complete!"); break;
				case 26: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Bigger than a bread box!"); break;
				case 27: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Full of stars!"); break;
				case 28: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Awesome community!"); break;
				case 29: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Give us Gordon!"); break;
				case 30: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Treatment for your rash!"); break;
				case 31: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Woo, 2pp!"); break;
				case 32: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Casual gaming!"); break;
				case 33: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Larger than Earth!"); break;
				case 34: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Exploding creepers!"); break;
				case 35: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"DAR! DAR! DAR!"); break;
				case 36: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Boats FTW"); break;
				case 37: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"As seen on TV!"); break;
				case 38: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Its's groundbreaking!"); break;
				case 39: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Menger sponge!"); break;
				case 40: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Uninflammable!"); break;
				case 41: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Check it out!"); break;
				case 42: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Funk soul brother!"); break;
				case 43: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Synecdoche!"); break;
				case 44: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Phobos snomaly!"); break;
				case 45: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Kind of dragon free!"); break;
				case 46: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Plz reply to my tweet!"); break;
				case 47: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Joel is neat!"); break;
				case 48: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Don't bother with the clones!"); break;
				case 49: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"The work of Notch!"); break;
				case 50: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"The creeper is a spy!"); break;
				case 51: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Never dig down!"); break;
				case 52: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Pumpakungen!"); break;
				case 53: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Scary!"); break;
				case 54: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Whoa, dude!"); break;
				case 55: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"I have a suggestion!"); break;
				case 56: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Throw a blanket over it!"); break;
				case 57: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"All inclusive!"); break;
				case 58: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Watter proof!"); break;
				case 59: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"4315162342 lines of code!"); break;
				case 60: DrawText(328,60,GU_COLOR(1,1,0,1) ,0.6+sinf(splashSize)*0.04f,"Holy cow, man!"); break;
            }
        }
    }
    break;
    case 13://skins
    {
        DrawSkinStage();
    }
    break;
    case 14://import
    {
        DrawSkinImport();
    }
    break;
    case 1://select world
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        if(saveSubmenu) // delete world
        {
            buttonSprite->SetPosition(240,235);
            buttonSprite->Draw();

            buttonSprite->SetPosition(240,260);
            buttonSprite->Draw();
        }
        else
        {
            /// left part
            if(saveFilesList.empty() == false)
            {
                buttonSprite->SetPosition(120,222); // play selected world
                buttonSprite->Draw();

                buttonSmallSprite->SetPosition(67,255); // rename
                buttonSmallSprite->Draw();

                buttonSmallSprite->SetPosition(172,255); // delete
                buttonSmallSprite->Draw();
            }
            else
            {
                nbuttonSprite->SetPosition(120,222); // play selected world
                nbuttonSprite->Draw();

                nbuttonSmallSprite->SetPosition(66,255); // rename
                nbuttonSmallSprite->Draw();

                nbuttonSmallSprite->SetPosition(173,255); // delete
                nbuttonSmallSprite->Draw();
            }

            ///right part
            buttonSprite->SetPosition(360,222); // create new world
            buttonSprite->Draw();

            buttonSprite->SetPosition(360,255); // cancel
            buttonSprite->Draw();
        }


        bool smallButton = false;
        if(saveSubmenu)
        {
            sbuttonSprite->SetPosition(240,(saveSubMenuSelect * 25) + 210);
        }
        else
        {
            switch(loadSelectPos)
            {
                case 0:
                sbuttonSprite->SetPosition(120,222);// play selected world
                break;
                case 1:
                sbuttonSmallSprite->SetPosition(66,255);//rename
                smallButton = true;
                break;
                case 2:
                sbuttonSmallSprite->SetPosition(173,255);//delete
                smallButton = true;
                break;
                case 3:
                sbuttonSprite->SetPosition(360,222);
                break;
				case 4:
                sbuttonSprite->SetPosition(360,255);// Create new world
                break;
            }
        }
        smallButton == true ? sbuttonSmallSprite->Draw() : sbuttonSprite->Draw();

        blackBackground->Draw();

        //select sprite
        if(saveFilesList.size() > 0)
        {
            //save files
            for(int i = loadSaveStart; i <loadSaveMax; i++)
            {
                if(loadSavePos == i)
                {
                    mRender->SetFont(ENGLISH);
                    if(saveFilesList[i].worldName[0] != '\0')
                    {
                        mRender->SetFontStyle(0.8f,GU_COLOR(1,1,0,1),0,0x00000000);
                        mRender->DebugPrint(30,54 + (i * 41) - (loadSaveStart * 41),"%s",saveFilesList[i].worldName);
                    }
                    else
                    {
                        mRender->SetFontStyle(0.8f,GU_COLOR(0.6,0.6,0,1),0,0x00000000);
                        mRender->DebugPrint(30,54 + (i * 41) - (loadSaveStart * 41),"New World");
                    }

                    mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0,1),0,0x00000000);
                    mRender->DebugPrint(30,66 + (i * 41) - (loadSaveStart * 41),"%s",saveFilesList[i].fileName.c_str());

                    mRender->SetDefaultFont();

                    if(mRender->GetFontLanguage() == ENGLISH)
                    {
                        switch(saveFilesList[i].worldGameMode)
                        {
                            case 0:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Survival mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                            case 1:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Creative mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                            case 2:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Hardcore mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                        }
                    }
                }
                else
                {
                    mRender->SetFont(ENGLISH);

                    if(saveFilesList[i].worldName[0] != '\0')
                    {
                        mRender->SetFontStyle(0.8f,GU_COLOR(1,1,1,1),0,0x00000000);
                        mRender->DebugPrint(30,54 + (i * 41) - (loadSaveStart * 41),"%s",saveFilesList[i].worldName);
                    }
                    else
                    {
                        mRender->SetFontStyle(0.8f,GU_COLOR(0.6,0.6,0.6,1),0,0x00000000);
                        mRender->DebugPrint(30,54 + (i * 41) - (loadSaveStart * 41),"New World");
                    }

                    mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0.5,1),0,0x00000000);
                    mRender->DebugPrint(30,66 + (i * 41) - (loadSaveStart * 41),"%s",saveFilesList[i].fileName.c_str());

                    mRender->SetDefaultFont();

                    if(mRender->GetFontLanguage() == ENGLISH)
                    {
                        switch(saveFilesList[i].worldGameMode)
                        {
                            case 0:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0.5,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Survival mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                            case 1:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0.5,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Creative mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                            case 2:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0.5,1),0,0x00000000);
                            mRender->DebugPrint(30,78  + (i * 41) - (loadSaveStart * 41),"Hardcore mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                        }
                    }
                }
            }
        }

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        if(mRender->GetFontLanguage() == ENGLISH)
        {
            if(saveSubmenu)
            {
                saveSubMenuSelect == 0 ? DrawText(240,219,GU_COLOR(1,1,0.25,1) ,default_size,"Are you sure?") : DrawText(240,219,GU_COLOR(1,1,1,1) ,default_size,"Are you sure?");
                saveSubMenuSelect == 1 ? DrawText(240,244,GU_COLOR(1,1,0.25,1) ,default_size,"Yes") : DrawText(240,244,GU_COLOR(1,1,1,1) ,default_size,"Yes");
                saveSubMenuSelect == 2 ? DrawText(240,269,GU_COLOR(1,1,0.25,1) ,default_size,"No") : DrawText(240,269,GU_COLOR(1,1,1,1) ,default_size,"No");
            }
            else
            {
                float buttonTextColor = 1.0f; // for left part
                if(saveFilesList.empty() == true)
                {
                    buttonTextColor = 0.5f;
                }

                loadSelectPos == 0 ? DrawText(120,231,GU_COLOR(1,1,0.25,1),default_size,"Play Selected World") : DrawText(120,231,GU_COLOR(buttonTextColor,buttonTextColor,buttonTextColor,1),default_size,"Play Selected World");
                loadSelectPos == 1 ? DrawText(67,264,GU_COLOR(1,1,0.25,1) ,default_size,"Rename") : DrawText(67,264,GU_COLOR(buttonTextColor,buttonTextColor,buttonTextColor,1) ,default_size,"Rename");
                loadSelectPos == 2 ? DrawText(172,264,GU_COLOR(1,1,0.25,1) ,default_size,"Delete") : DrawText(172,264,GU_COLOR(buttonTextColor,buttonTextColor,buttonTextColor,1) ,default_size,"Delete");
                loadSelectPos == 3 ? DrawText(360,231,GU_COLOR(1,1,0.25,1) ,default_size,"Create New World") : DrawText(360,231,GU_COLOR(1.0f,1.0f,1.0f,1) ,default_size,"Create New World");
                loadSelectPos == 4 ? DrawText(360,264,GU_COLOR(1,1,0.25,1) ,default_size,"Cancel") : DrawText(360,264,GU_COLOR(1.0f,1.0f,1.0f,1) ,default_size,"Cancel");
            }
            DrawText(240,24,GU_COLOR(1,1,1,1) ,default_size,"Select World");
			DrawText(240,14,GU_COLOR(1,1,1,1) ,default_size,"                                                (Multiplayer)");
			DrawText(240,24,GU_COLOR(1,1,1,1) ,default_size,"                                                R Trigger");
			DrawText(240,14,GU_COLOR(1,1,1,1) ,default_size,"(Scroll Worlds)                                              ");
			DrawText(240,24,GU_COLOR(1,1,1,1) ,default_size," L Trigger                                                ");
			
        }
    }
    break;
    case 3://about
    {


        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        //check for update
        buttonSprite->SetPosition(240,225);
        buttonSprite->Draw();

        buttonSprite->SetPosition(240,255);
        buttonSprite->Draw();

        //back
        //
        // Only while the cursor is actually on the buttons. The blue selection sprite stays on the
        // Converter button otherwise, which reads as "still selected" while the cursor is up in the
        // changelog -- the same thing the yellow label used to do, and it made the Up out of the buttons
        // look like it had not taken effect. Clearing it is what leaves the box as the only highlighted
        // thing on the screen once the cursor is up there.
        if(aboutFocus == 0)
        {
            sbuttonSprite->SetPosition(240,(aboutPos * 30) + 225);
            sbuttonSprite->Draw();
        }

        blackBackground->Draw();

        // The d-pad hints, in the changelog box's top-right and bottom-right corners. Drawn after the
        // panel so they sit on top of it, and before blend is switched off so they keep their alpha.
        // NULL when the pack has no title/dpad.png; see Init.
        //
        // They light up with the box, which is what makes them read as an instruction rather than as
        // decoration: while the cursor is still on the buttons the text is dim and the hints are faint,
        // and the Up that lifts the cursor onto the box brightens both at once. Unselected they are
        // dimmed rather than hidden, so it is clear that the box scrolls before the cursor gets there.
        const float hintAlpha = (aboutFocus >= 1) ? 1.0f : 0.4f;
        if(dpadUpSprite != NULL)
        {
            dpadUpSprite->Alpha(hintAlpha);
            dpadUpSprite->Draw();
        }
        if(dpadDownSprite != NULL)
        {
            dpadDownSprite->Alpha(hintAlpha);
            dpadDownSprite->Draw();
        }

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        if(mRender->GetFontLanguage() == ENGLISH)
        {
            // The changelog, kNewsVisibleLines lines at a time from newsScroll. The loop replaced a
            // run of DebugPrint calls, two of which shared a row and so hid one another.
            //
            // The shading is the "there is more above and below" cue: dim while the cursor is on the
            // buttons, full white once Up has lifted it onto the box, matching the hints in the corners.
            const float textShade = (aboutFocus >= 1) ? 1.0f : 0.58f;
            for(int i = 0; i < kNewsVisibleLines; i++)
            {
                int line = newsScroll + i;
                if(line >= kNewsLineCount)
                    break;

                mRender->SetFontStyle(0.687, GU_COLOR(textShade, textShade, textShade, 1), 2, 0x00000000);
                mRender->DebugPrint(40, (int)(kNewsLineTop + i * kNewsLineStep), kNewsLines[line]);
            }

            // The yellow label marks the focused button, and it has to go with the blue sprite above:
            // both are cleared while the cursor is up in the changelog, so no button is left looking
            // selected. aboutPos is deliberately kept, so Down returns to the same button that Up left.
            const bool onButtons = (aboutFocus == 0);
            (onButtons && aboutPos == 0) ? DrawText(240,234,GU_COLOR(1,1,0.25,1) ,default_size,"Converter") : DrawText(240,234,GU_COLOR(1,1,1,1) ,default_size,"Converter");
            (onButtons && aboutPos == 1) ? DrawText(240,264,GU_COLOR(1,1,0.25,1) ,default_size,"Cancel") : DrawText(240,264,GU_COLOR(1,1,1,1) ,default_size,"Cancel");

            // Selecting the box uses the menu's own idiom for "this is the selected thing": the yellow
            // every other selected button uses. It is the only cue that Up has lifted the cursor off
            // the buttons, so it has to read the same way as the rest of the menu does.
            if(aboutFocus >= 1)
                DrawText(240,29,GU_COLOR(1,1,0.25,1) ,default_size,"UPDATE NEWS 3.0.0");
            else
                DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"UPDATE NEWS 3.0.0");
        }
    }
    break;
    case 5://paramateric view
	{
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        //name
        mbuttonSprite->SetPosition(240,70);
        mbuttonSprite->Draw();
        //seed
        mbuttonSprite->SetPosition(240,110);
        mbuttonSprite->Draw();

        buttonSprite->SetPosition(240,150);
        buttonSprite->Draw();

        buttonSprite->SetPosition(240,180);
        buttonSprite->Draw();

        buttonSprite->SetPosition(240,210);
        buttonSprite->Draw();

        if(generateSelectPose > 1)
        {
            sbuttonSprite->SetPosition(240,150+(generateSelectPose-2)*30);
            sbuttonSprite->Draw();
        }
        if(generateSelectPose <= 1)
        {
            smbuttonSprite->SetPosition(240,70+generateSelectPose*40);
            smbuttonSprite->Draw();
        }

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        if(mRender->GetFontLanguage() == ENGLISH)
        {
            DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Create New World");
            DrawText(240,58,GU_COLOR(0.5,0.5,0.5,1) ,default_size,"World Name");
            DrawText(240,98,GU_COLOR(0.5,0.5,0.5,1) ,default_small_size,"Seed for the World Generator");
			DrawText(240,138,GU_COLOR(0.5,0.5,0.5,1) ,default_size,"World Options");

            //draw subtitles on buttons
            if(gameMode == SURVIVAL)
            {
                generateSelectPose == 2 ? DrawText(240,159,GU_COLOR(1,1,0.25,1) ,default_size,"Game Mode: Survival") : DrawText(240,159,GU_COLOR(1,1,1,1) ,default_size,"Game Mode: Survival");
            }
            if(gameMode == CREATIVE)
            {
                generateSelectPose == 2 ? DrawText(240,159,GU_COLOR(1,1,0.25,1) ,default_size,"Game Mode: Creative") : DrawText(240,159,GU_COLOR(1,1,1,1) ,default_size,"Game Mode: Creative");
            }
            if(gameMode == HARDCORE)
            {
                generateSelectPose == 2 ? DrawText(240,159,GU_COLOR(1,1,0.25,1) ,default_size,"Game Mode: Hardcore") : DrawText(240,159,GU_COLOR(1,1,1,1) ,default_size,"Game Mode: Hardcore");
            }

            if(worldType == 0)
            {
                generateSelectPose == 3 ? DrawText(240,189,GU_COLOR(1,1,0.25,1) ,default_size,"World Type: Old") : DrawText(240,189,GU_COLOR(1,1,1,1) ,default_size,"World Type: Old");
            }
            if(worldType == 1)
            {
                generateSelectPose == 3 ? DrawText(240,189,GU_COLOR(1,1,0.25,1) ,default_size,"World Type: Superflat") : DrawText(240,189,GU_COLOR(1,1,1,1) ,default_size,"World Type: Superflat");
            }
			if (worldType == 2)
			{
				generateSelectPose == 3 ? DrawText(240, 189, GU_COLOR(1, 1, 0.25, 1), default_size, "Biome: Forest") : DrawText(240, 189, GU_COLOR(1, 1, 1, 1), default_size, "Biome: Forest");
			}
			if (worldType == 3)
			{
				generateSelectPose == 3 ? DrawText(240, 189, GU_COLOR(1, 1, 0.25, 1), default_size, "Biome: Desert Lakes") : DrawText(240, 189, GU_COLOR(1, 1, 1, 1), default_size, "Biome: Desert Lakes");
			}
			if (worldType == 4)
			{
				generateSelectPose == 3 ? DrawText(240, 189, GU_COLOR(1, 1, 0.25, 1), default_size, "Biome: Snowy Taiga") : DrawText(240, 189, GU_COLOR(1, 1, 1, 1), default_size, "Biome: Snowy Taiga");
			}
			if (worldType == 5)
			{
				generateSelectPose == 3 ? DrawText(240, 189, GU_COLOR(1, 1, 0.25, 1), default_size, "Biome: Flat Desert") : DrawText(240, 189, GU_COLOR(1, 1, 1, 1), default_size, "Biome: Flat Desert");
			}
			if (worldType == 6)
			{
				generateSelectPose == 3 ? DrawText(240, 189, GU_COLOR(1, 1, 0.25, 1), default_size, "Biome: Flat Snow") : DrawText(240, 189, GU_COLOR(1, 1, 1, 1), default_size, "Biome: Flat Snow");
			}
			if (worldType == 7)
			{
				generateSelectPose == 3 ? DrawText(240, 189, GU_COLOR(1, 1, 0.25, 1), default_size, "Biome: Flat Ocean") : DrawText(240, 189, GU_COLOR(1, 1, 1, 1), default_size, "Biome: Flat Ocean");
			}

            generateSelectPose == 4 ? DrawText(240,219,GU_COLOR(1,1,0.25,1) ,default_size,"Create New World") : DrawText(240,219,GU_COLOR(1,1,1,1) ,default_size,"Create New World");
        }

        mRender->SetFont(ENGLISH);
        mRender->SetFontStyle(default_size ,GU_COLOR(1,1,1,1),0,0x00000000|0x00004000);
        mRender->DebugPrint(159,79,"%s",newWorldName.c_str());
        if(seed_1 == 0)
        {
            mRender->SetFontStyle(default_size ,GU_COLOR(0.65,0.65,0.65,1),999,0x00000200|0x00004000);
            if(mRender->GetFontLanguage() == ENGLISH)
            {
                mRender->DebugPrint(240,119,"random");
            }
        }
        else
        {
            mRender->DebugPrint(159,119,"%s",newWorldSeed.c_str());
        }
        mRender->SetDefaultFont();
    }
    break;
    case 6://about
    {


        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        //check for update
        buttonSprite->SetPosition(240,165);
        buttonSprite->Draw();

        buttonSprite->SetPosition(240,200);
        buttonSprite->Draw();

        buttonSprite->SetPosition(240,245);
        buttonSprite->Draw();

        //back
        int y_pos = 165;
        if(converterPos == 1)
        {
            y_pos = 200;
        }
        if(converterPos == 2)
        {
            y_pos = 245;
        }
        sbuttonSprite->SetPosition(240,y_pos);
        sbuttonSprite->Draw();

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        if(mRender->GetFontLanguage() == ENGLISH)
        {
            if(schematicExists)
            {
                DrawText(240,80,GU_COLOR(0.1,0.9,0.1,1),default_big_size,"world.schematic exists!");
            }
            else
            {
                DrawText(240,80,GU_COLOR(0.9,0.1,0.1,1),default_big_size,"world.schematic doesn't exist!");
            }
            if(errorType == 1)
            {
                DrawText(240,100,GU_COLOR(0.9,0.1,0.1,1),default_big_size,"invalid schematic size");
            }
            else
            {
                DrawText(240,100,GU_COLOR(0.9,0.1,0.1,1),default_size,"This may take a while!");
            }

            if(newW_gameMode == SURVIVAL)
            {
                converterPos == 0 ? DrawText(240,174,GU_COLOR(1,1,0.25,1) ,default_size,"Game Mode: Survival") : DrawText(240,174,GU_COLOR(1,1,1,1) ,default_size,"Game Mode: Survival");
            }
            if(newW_gameMode == CREATIVE)
            {
                converterPos == 0 ? DrawText(240,174,GU_COLOR(1,1,0.25,1) ,default_size,"Game Mode: Creative") : DrawText(240,174,GU_COLOR(1,1,1,1) ,default_size,"Game Mode: Creative");
            }
			if(newW_gameMode == HARDCORE)
            {
                converterPos == 0 ? DrawText(240,174,GU_COLOR(1,1,0.25,1) ,default_size,"Game Mode: Hardcore") : DrawText(240,174,GU_COLOR(1,1,1,1) ,default_size,"Game Mode: Hardcore");
            }
            converterPos == 1 ? DrawText(240,209,GU_COLOR(1,1,0.25,1) ,default_size,"Try to convert") : DrawText(240,209,GU_COLOR(1,1,1,1) ,default_size,"Try to convert");
            converterPos == 2 ? DrawText(240,254,GU_COLOR(1,1,0.25,1) ,default_size,"Cancel") : DrawText(240,254,GU_COLOR(1,1,1,1) ,default_size,"Cancel");

            DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Converter");
        }
    }
    break;
    case 10://New or load map
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        buttonSprite->SetPosition(240,100);
        buttonSprite->Draw();

        buttonSprite->SetPosition(240,140);
        buttonSprite->Draw();

        sbuttonSprite->SetPosition(240,100+saveSubMenuSelect*40);
        sbuttonSprite->Draw();

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        if(mRender->GetFontLanguage() == ENGLISH)
        {
            DrawText(240,64,GU_COLOR(1,1,1,1) ,default_size,"Choose Game Mode");

            saveSubMenuSelect == 0 ? DrawText(240,109,GU_COLOR(1,1,0.25,1) ,default_size,"Survival") : DrawText(240,109,GU_COLOR(1,1,1,1) ,default_size,"Survival");
            saveSubMenuSelect == 1 ? DrawText(240,139,GU_COLOR(1,1,0.25,1) ,default_size,"Creative") : DrawText(240,139,GU_COLOR(1,1,1,1) ,default_size,"Creative");
			saveSubMenuSelect == 2 ? DrawText(240,169,GU_COLOR(1,1,0.25,1) ,default_size,"Hardcore") : DrawText(240,169,GU_COLOR(1,1,1,1) ,default_size,"Hardcore");
        }
    }
    break;
    case 11://textures
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        //select sprite
        if(texturePackList.size() > 0)
        {

        }

        for(int i = tpStart; i < tpMax; i++)
        {
            if(i < texturePackList.size())
            {
                sceGuEnable(GU_BLEND);

                rectFilledSprite->SetPosition(240,56 - 8 + (tpCurrent * 72) - (tpStart * 72));

                if(i == tpCurrent)
                {
                    rectFilledSprite->Draw();
                }

                rectEmptySprite->SetPosition(240,56 - 8 + (tpPos * 72) - (tpStart * 72));
                if(i == tpPos)
                {
                    sceGuBlendFunc(GU_ADD, GU_FIX,GU_FIX, 0xFFFFFFFF, 0xFFFFFFFF);
                    rectEmptySprite->Draw();
                    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
                }

                Sprite* DrawSprite = texturePackList[i].packSprite;
                DrawSprite->SetPosition(46,56 - 8 + (i * 72) - (tpStart * 72));
                DrawSprite->ConstDraw();

                sceGuDisable(GU_BLEND);

                mRender->SetFont(ENGLISH);
                if(i == tpPos)
                {
                    mRender->SetFontStyle(default_big_size,GU_COLOR(0.25,0.25,0,1),0,0x00000000);
                    mRender->DebugPrint(94,52 - 8 + (i * 72) - (tpStart * 72),"%s",texturePackList[i].name.c_str());

                    mRender->SetFontStyle(default_big_size,GU_COLOR(1,1,0,1),0,0x00000000);
                    mRender->DebugPrint(92,50 - 8 + (i * 72) - (tpStart * 72),"%s",texturePackList[i].name.c_str());

                    mRender->SetFontStyle(default_size,GU_COLOR(0.5,0.5,0,1),0,0x00000000);
                    mRender->DebugPrint(92,50 - 8 + (i * 72) - (tpStart * 72) + 18,"%s",texturePackList[i].description.c_str());
                }
                else
                {
                    mRender->SetFontStyle(default_big_size,GU_COLOR(0.25,0.25,0.25,1),0,0x00000000);
                    mRender->DebugPrint(94,52 - 8 + (i * 72) - (tpStart * 72),"%s",texturePackList[i].name.c_str());

                    mRender->SetFontStyle(default_big_size,GU_COLOR(1,1,1,1),0,0x00000000);
                    mRender->DebugPrint(92,50 - 8 + (i * 72) - (tpStart * 72),"%s",texturePackList[i].name.c_str());

                    mRender->SetFontStyle(default_size,GU_COLOR(0.5,0.5,0.5,1),0,0x00000000);
                    mRender->DebugPrint(92,50 - 8 + (i * 72) - (tpStart * 72) + 18,"%s",texturePackList[i].description.c_str());
                }
                mRender->SetDefaultFont();
            }
        }



        buttonSprite->SetPosition(120,254);
        buttonSprite->Draw();

        buttonSprite->SetPosition(360,254);
        buttonSprite->Draw();


        sbuttonSprite->SetPosition(120+(tpSelectPos * 240),254);
        sbuttonSprite->Draw();

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        if(mRender->GetFontLanguage() == ENGLISH)
        {
            tpSelectPos == 0 ? DrawText(120,263,GU_COLOR(1,1,0.25,1),default_size,"Select") : DrawText(120,263,GU_COLOR(1,1,1,1),default_size,"Select");
            tpSelectPos == 1 ? DrawText(360,263,GU_COLOR(1,1,0.25,1) ,default_size,"Cancel") : DrawText(360,263,GU_COLOR(1,1,1,1) ,default_size,"Cancel");
        }
    }
	break;
	case 12://select world HOST
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();
            }
        }

        if(saveSubmenu) // delete world
        {
            buttonSprite->SetPosition(240,235);
            buttonSprite->Draw();

            buttonSprite->SetPosition(240,260);
            buttonSprite->Draw();
        }
        else
        {
            /// left part
            if(saveFilesList.empty() == false)
            {
                buttonSprite->SetPosition(120,222); // play selected world
                buttonSprite->Draw();

                buttonSmallSprite->SetPosition(67,255); // rename
                buttonSmallSprite->Draw();

                buttonSmallSprite->SetPosition(172,255); // delete
                buttonSmallSprite->Draw();
            }
            else
            {
                nbuttonSprite->SetPosition(120,222); // play selected world
                nbuttonSprite->Draw();

                nbuttonSmallSprite->SetPosition(66,255); // rename
                nbuttonSmallSprite->Draw();

                nbuttonSmallSprite->SetPosition(173,255); // delete
                nbuttonSmallSprite->Draw();
            }

            ///right part
            buttonSprite->SetPosition(360,222); // create new world
            buttonSprite->Draw();

            buttonSprite->SetPosition(360,255); // cancel
            buttonSprite->Draw();
        }


        bool smallButton = false;
        if(saveSubmenu)
        {
            sbuttonSprite->SetPosition(240,(saveSubMenuSelect * 25) + 210);
        }
        else
        {
            switch(loadSelectPos)
            {
                case 0:
                sbuttonSprite->SetPosition(120,222);// play selected world
                break;
                case 1:
                sbuttonSmallSprite->SetPosition(66,255);//rename
                smallButton = true;
                break;
                case 2:
                sbuttonSmallSprite->SetPosition(173,255);//delete
                smallButton = true;
                break;
                case 3:
                sbuttonSprite->SetPosition(360,222);
                break;
				case 4:
                sbuttonSprite->SetPosition(360,255);// Create new world
                break;
            }
        }
        smallButton == true ? sbuttonSmallSprite->Draw() : sbuttonSprite->Draw();

        blackBackground->Draw();

        //select sprite
        if(saveFilesList.size() > 0)
        {
            //save files
            for(int i = loadSaveStart; i <loadSaveMax; i++)
            {
                if(loadSavePos == i)
                {
                    mRender->SetFont(ENGLISH);
                    if(saveFilesList[i].worldName[0] != '\0')
                    {
                        mRender->SetFontStyle(0.8f,GU_COLOR(1,1,0,1),0,0x00000000);
                        mRender->DebugPrint(30,54 + (i * 41) - (loadSaveStart * 41),"%s",saveFilesList[i].worldName);
                    }
                    else
                    {
                        mRender->SetFontStyle(0.8f,GU_COLOR(0.6,0.6,0,1),0,0x00000000);
                        mRender->DebugPrint(30,54 + (i * 41) - (loadSaveStart * 41),"New World");
                    }

                    mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0,1),0,0x00000000);
                    mRender->DebugPrint(30,66 + (i * 41) - (loadSaveStart * 41),"%s",saveFilesList[i].fileName.c_str());

                    mRender->SetDefaultFont();

                    if(mRender->GetFontLanguage() == ENGLISH)
                    {
                        switch(saveFilesList[i].worldGameMode)
                        {
                            case 0:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Survival mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                            case 1:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Creative mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                            case 2:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Hardcore mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                        }
                    }
                }
                else
                {
                    mRender->SetFont(ENGLISH);

                    if(saveFilesList[i].worldName[0] != '\0')
                    {
                        mRender->SetFontStyle(0.8f,GU_COLOR(1,1,1,1),0,0x00000000);
                        mRender->DebugPrint(30,54 + (i * 41) - (loadSaveStart * 41),"%s",saveFilesList[i].worldName);
                    }
                    else
                    {
                        mRender->SetFontStyle(0.8f,GU_COLOR(0.6,0.6,0.6,1),0,0x00000000);
                        mRender->DebugPrint(30,54 + (i * 41) - (loadSaveStart * 41),"New World");
                    }

                    mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0.5,1),0,0x00000000);
                    mRender->DebugPrint(30,66 + (i * 41) - (loadSaveStart * 41),"%s",saveFilesList[i].fileName.c_str());

                    mRender->SetDefaultFont();

                    if(mRender->GetFontLanguage() == ENGLISH)
                    {
                        switch(saveFilesList[i].worldGameMode)
                        {
                            case 0:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0.5,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Survival mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                            case 1:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0.5,1),0,0x00000000);
                            mRender->DebugPrint(30,78 + (i * 41) - (loadSaveStart * 41),"Creative mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                            case 2:
                            mRender->SetFontStyle(0.5f,GU_COLOR(0.5,0.5,0.5,1),0,0x00000000);
                            mRender->DebugPrint(30,78  + (i * 41) - (loadSaveStart * 41),"Hardcore mode (%i KB)",saveFilesList[i].saveSize/1024);
                            break;
                        }
                    }
                }
            }
        }

        sceGuDisable(GU_BLEND);
        sceGuEnable(GU_DEPTH_TEST);

        if(mRender->GetFontLanguage() == ENGLISH)
        {
            if(saveSubmenu)
            {
                saveSubMenuSelect == 0 ? DrawText(240,219,GU_COLOR(1,1,0.25,1) ,default_size,"Are you sure?") : DrawText(240,219,GU_COLOR(1,1,1,1) ,default_size,"Are you sure?");
                saveSubMenuSelect == 1 ? DrawText(240,244,GU_COLOR(1,1,0.25,1) ,default_size,"Yes") : DrawText(240,244,GU_COLOR(1,1,1,1) ,default_size,"Yes");
                saveSubMenuSelect == 2 ? DrawText(240,269,GU_COLOR(1,1,0.25,1) ,default_size,"No") : DrawText(240,269,GU_COLOR(1,1,1,1) ,default_size,"No");
            }
            else
            {
                float buttonTextColor = 1.0f; // for left part
                if(saveFilesList.empty() == true)
                {
                    buttonTextColor = 0.5f;
                }

                loadSelectPos == 0 ? DrawText(120,231,GU_COLOR(1,1,0.25,1),default_size,"Play Selected World") : DrawText(120,231,GU_COLOR(buttonTextColor,buttonTextColor,buttonTextColor,1),default_size,"Play Selected World");
                loadSelectPos == 1 ? DrawText(67,264,GU_COLOR(1,1,0.25,1) ,default_size,"Rename") : DrawText(67,264,GU_COLOR(buttonTextColor,buttonTextColor,buttonTextColor,1) ,default_size,"Rename");
                loadSelectPos == 2 ? DrawText(172,264,GU_COLOR(1,1,0.25,1) ,default_size,"Delete") : DrawText(172,264,GU_COLOR(buttonTextColor,buttonTextColor,buttonTextColor,1) ,default_size,"Delete");
                loadSelectPos == 3 ? DrawText(360,231,GU_COLOR(1,1,0.25,1) ,default_size,"Create New World") : DrawText(360,231,GU_COLOR(1.0f,1.0f,1.0f,1) ,default_size,"Create New World");
                loadSelectPos == 4 ? DrawText(360,264,GU_COLOR(1,1,0.25,1) ,default_size,"Cancel") : DrawText(360,264,GU_COLOR(1.0f,1.0f,1.0f,1) ,default_size,"Cancel");
            }
            DrawText(240,24,GU_COLOR(1,1,1,1) ,default_size,"Host: Select World");		
        }
    }
    break;
    }
    mRender->EndFrame();
}

//additional functions
void StateMenu::ScanSaveFiles(const char* dirName)
{
    //clear list with save files
    saveFilesList.clear();

    DIR *Dir = opendir(dirName);
    if (Dir == NULL)
        return;

    struct dirent *DirEntry;
    while((DirEntry = readdir(Dir)) != NULL)
    {
        SaveFile newSaveFile;
        std::string plik = dirName;
        plik += DirEntry->d_name;

        struct stat st;
        if (stat(plik.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        //dont load lmsc files
        size_t found = plik.find(".lmsc");
        size_t found2 = plik.find(".LMSc");
        if(found==std::string::npos && found2==std::string::npos)
        {
            newSaveFile.fileName = plik;
            saveFilesList.push_back(newSaveFile);
        }
    }

    closedir(Dir);


    //now update all info in save files
    for(unsigned int i = 0; i < saveFilesList.size(); i++)
    {
        FILE * pFile;
        pFile = fopen(saveFilesList[i].fileName.c_str(),"rb");

        if(pFile != NULL)
        {
            //version
            fread(&saveFilesList[i].saveVersion,sizeof(int),1,pFile);

            fread(&saveFilesList[i].worldGameMode,sizeof(char),1,pFile);

            fread(&saveFilesList[i].locked,sizeof(bool),1,pFile);
            //name
            fread(saveFilesList[i].worldName,sizeof(char),50,pFile);

            if(saveFilesList[i].saveVersion >= 2)
                saveFilesList[i].compression = true;
            else
                saveFilesList[i].compression = false;

            fclose(pFile);
        }

        saveFilesList[i].saveSize = fileSize(saveFilesList[i].fileName+"c");
    }

    //set next save name and numer
    nextSaveFileNumber = saveFilesList.size() + 1;
    {
        //ride thru all saves and check if i can use new name
        char liczba[3];
        sprintf(liczba,"%d",nextSaveFileNumber);
        nextSaveFileName = "Save/world";
        nextSaveFileName += liczba;
        nextSaveFileName += ".lms";

        for(unsigned int i = 0; i < saveFilesList.size(); i++)
        {
            if(nextSaveFileName.compare(saveFilesList[i].fileName) == 0)
            {
                nextSaveFileNumber+=1;
                char liczba[3];
                sprintf(liczba,"%d",nextSaveFileNumber);
                nextSaveFileName = "Save/world";
                nextSaveFileName += liczba;
                nextSaveFileName += ".lms";
            }
        }
    }
}

static bool SkinFileName(const char* name);

struct SkinVert
{
    float u, v, x, y, z;
};

struct SkinMeshVert
{
    float u, v;
    unsigned int color;
    float x, y, z;
};

struct SkinColorVert
{
    unsigned int color;
    float x, y, z;
};

static std::vector<Aurora::Graphics::Image*> sSkinIcons;
static Aurora::Graphics::Image* sSkinPreview = NULL;
static int sSkinPreviewFor = -2;
static SkinMeshVert* sPlayerMesh = NULL;
static int sPlayerMeshCount = 0;

static void FreeSkinImage(Aurora::Graphics::Image* image)
{
    if(image == NULL)
        return;
    if(image->ImageData != NULL && image->vRam == 0)
        free(image->ImageData);
    delete image;
}

static void ClearSkinPreviews()
{
    for(size_t i = 0; i < sSkinIcons.size(); i++)
        FreeSkinImage(sSkinIcons[i]);
    sSkinIcons.clear();
    FreeSkinImage(sSkinPreview);
    sSkinPreview = NULL;
    sSkinPreviewFor = -2;
}

// A front view of the whole figure, 16x32, the way the grid tiles show it. Classic sheets have one
// arm and one leg; the character's left side is that strip mirrored.
static Aurora::Graphics::Image* MakeBodyIcon(Aurora::Graphics::Image* sheet)
{
    if(sheet == NULL || sheet->Swizzle != 0 || sheet->ImageData == NULL)
        return NULL;
    const int w = 16;
    const int h = 32;
    unsigned int* src = (unsigned int*)sheet->ImageData;
    int stride = sheet->power2Width;
    unsigned int* dst = (unsigned int*)memalign(16, w * h * 4);
    if(dst == NULL)
        return NULL;
    memset(dst, 0, w * h * 4);

    // sx, sy, width, height, dx, dy, mirrored
    const int part[6][7] = {
        {8,  8,  8, 8,  4,  0,  0},
        {20, 20, 8, 12, 4,  8,  0},
        {44, 20, 4, 12, 0,  8,  0},
        {44, 20, 4, 12, 12, 8,  1},
        {4,  20, 4, 12, 4,  20, 0},
        {4,  20, 4, 12, 8,  20, 1}
    };
    for(int p = 0; p < 6; p++)
    {
        for(int y = 0; y < part[p][3]; y++)
        {
            for(int x = 0; x < part[p][2]; x++)
            {
                int sx = part[p][6] ? (part[p][0] + part[p][2] - 1 - x) : (part[p][0] + x);
                dst[(part[p][5] + y) * w + part[p][4] + x] = src[(part[p][1] + y) * stride + sx];
            }
        }
    }
    sceKernelDcacheWritebackInvalidateRange(dst, w * h * 4);
    Aurora::Graphics::Image* icon = new Aurora::Graphics::Image();
    icon->Width = w;
    icon->Height = h;
    icon->power2Width = w;
    icon->power2Height = h;
    icon->vRam = 0;
    icon->ColorMode = GU_PSM_8888;
    icon->Swizzle = 0;
    icon->ImageData = (unsigned short*)dst;
    return icon;
}

static unsigned int SkinShade(float level)
{
    int c = (int)(level * 255.0f);
    return 0xFF000000u | ((unsigned int)c << 16) | ((unsigned int)c << 8) | (unsigned int)c;
}

// Four corners in the order bottom-left, bottom-right, top-right, top-left as seen from outside
// the face, so the texture is never mirrored by the corner order.
static void AddSkinFace(std::vector<SkinMeshVert>& verts, float u, float v, float uw, float vh, unsigned int color,
    float x0, float y0, float z0, float x1, float y1, float z1,
    float x2, float y2, float z2, float x3, float y3, float z3)
{
    float u0 = u / 64.0f;
    float v0 = v / 32.0f;
    float u1 = (u + uw) / 64.0f;
    float v1 = (v + vh) / 32.0f;
    SkinMeshVert corner[4];
    corner[0].u = u0; corner[0].v = v1; corner[0].x = x0; corner[0].y = y0; corner[0].z = z0;
    corner[1].u = u1; corner[1].v = v1; corner[1].x = x1; corner[1].y = y1; corner[1].z = z1;
    corner[2].u = u1; corner[2].v = v0; corner[2].x = x2; corner[2].y = y2; corner[2].z = z2;
    corner[3].u = u0; corner[3].v = v0; corner[3].x = x3; corner[3].y = y3; corner[3].z = z3;
    for(int i = 0; i < 4; i++)
        corner[i].color = color;
    int order[6] = {0, 1, 2, 0, 2, 3};
    for(int i = 0; i < 6; i++)
        verts.push_back(corner[order[i]]);
}

// The figure faces +Z. Its right side is -X. The sheet is laid out right side, front, left side,
// back, so each face's left texture edge is the edge it shares with the face before it.
static void AddSkinBox(std::vector<SkinMeshVert>& verts, float u, float v, float x, float y, float z,
    float w, float h, float d, bool mirrored)
{
    const float s = 1.0f / 16.0f;
    float x0 = x * s, y0 = y * s, z0 = z * s;
    float x1 = (x + w) * s, y1 = (y + h) * s, z1 = (z + d) * s;
    size_t first = verts.size();

    AddSkinFace(verts, u + d, v + d, w, h, SkinShade(0.9f),
        x0, y0, z1,  x1, y0, z1,  x1, y1, z1,  x0, y1, z1);
    AddSkinFace(verts, u + d + w + d, v + d, w, h, SkinShade(0.9f),
        x1, y0, z0,  x0, y0, z0,  x0, y1, z0,  x1, y1, z0);
    AddSkinFace(verts, u, v + d, d, h, SkinShade(0.72f),
        x0, y0, z0,  x0, y0, z1,  x0, y1, z1,  x0, y1, z0);
    AddSkinFace(verts, u + d + w, v + d, d, h, SkinShade(0.72f),
        x1, y0, z1,  x1, y0, z0,  x1, y1, z0,  x1, y1, z1);
    AddSkinFace(verts, u + d, v, w, d, SkinShade(1.0f),
        x0, y1, z1,  x1, y1, z1,  x1, y1, z0,  x0, y1, z0);
    AddSkinFace(verts, u + d + w, v, w, d, SkinShade(0.55f),
        x0, y0, z0,  x1, y0, z0,  x1, y0, z1,  x0, y0, z1);

    if(mirrored)
    {
        float centre = x0 + x1;
        for(size_t i = first; i < verts.size(); i++)
            verts[i].x = centre - verts[i].x;
    }
}

static void BuildPlayerMesh()
{
    if(sPlayerMesh != NULL)
        return;
    std::vector<SkinMeshVert> verts;
    AddSkinBox(verts, 0, 0, -4, 24, -4, 8, 8, 8, false);
    AddSkinBox(verts, 16, 16, -4, 12, -2, 8, 12, 4, false);
    AddSkinBox(verts, 40, 16, -8, 12, -2, 4, 12, 4, false);
    AddSkinBox(verts, 40, 16, 4, 12, -2, 4, 12, 4, true);
    AddSkinBox(verts, 0, 16, -4, 0, -2, 4, 12, 4, false);
    AddSkinBox(verts, 0, 16, 0, 0, -2, 4, 12, 4, true);
    sPlayerMeshCount = (int)verts.size();
    sPlayerMesh = (SkinMeshVert*)memalign(16, verts.size() * sizeof(SkinMeshVert));
    if(sPlayerMesh == NULL)
    {
        sPlayerMeshCount = 0;
        return;
    }
    memcpy(sPlayerMesh, &verts[0], verts.size() * sizeof(SkinMeshVert));
    sceKernelDcacheWritebackInvalidateRange(sPlayerMesh, verts.size() * sizeof(SkinMeshVert));
}

// The GE reads vertices after the call returns, so they come from the display list, never the stack.
static void DrawScreenRect(float x, float y, float w, float h, unsigned int color)
{
    SkinColorVert* v = (SkinColorVert*)sceGuGetMemory(2 * sizeof(SkinColorVert));
    v[0].color = color; v[0].x = x;     v[0].y = y;     v[0].z = 0;
    v[1].color = color; v[1].x = x + w; v[1].y = y + h; v[1].z = 0;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
    sceGuEnable(GU_TEXTURE_2D);
}

static void BindSheet(Aurora::Graphics::Image* sheet)
{
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(sheet->ColorMode, 0, 0, sheet->Swizzle);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexImage(0, sheet->power2Width, sheet->power2Height, sheet->power2Width, sheet->ImageData);
}

static void DrawIconQuad(Aurora::Graphics::Image* icon, float x, float y, float w, float h)
{
    if(icon == NULL || icon->ImageData == NULL)
        return;
    BindSheet(icon);
    SkinVert* v = (SkinVert*)sceGuGetMemory(2 * sizeof(SkinVert));
    v[0].u = 0;                  v[0].v = 0;                   v[0].x = x;     v[0].y = y;     v[0].z = 0;
    v[1].u = (float)icon->Width; v[1].v = (float)icon->Height; v[1].x = x + w; v[1].y = y + h; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
    TextureManager::Instance()->InvalidateCurrentTexture();
}

// One 16x16 cell of a texture-manager sheet, centred on whole pixels (the cells are 1:1 pixel art),
// turned clockwise by quarterTurns.
static void DrawSheetCell(int texture, int cellU, int cellV, float cx, float cy, int quarterTurns)
{
    TextureManager* tm = TextureManager::Instance();
    if(texture < 0 || texture >= (int)tm->Images.size())
        return;
    Aurora::Graphics::Image* sheet = tm->Images[texture];
    if(sheet == NULL || sheet->ImageData == NULL)
        return;
    BindSheet(sheet);

    float x0 = floorf(cx + 0.5f) - 8.0f;
    float y0 = floorf(cy + 0.5f) - 8.0f;
    float sx[4] = {x0, x0 + 16, x0 + 16, x0};
    float sy[4] = {y0, y0, y0 + 16, y0 + 16};
    float su[4] = {(float)cellU, (float)cellU + 16, (float)cellU + 16, (float)cellU};
    float sv[4] = {(float)cellV, (float)cellV, (float)cellV + 16, (float)cellV + 16};
    int order[6] = {0, 1, 2, 0, 2, 3};
    SkinVert* v = (SkinVert*)sceGuGetMemory(6 * sizeof(SkinVert));
    for(int i = 0; i < 6; i++)
    {
        int corner = order[i];
        int from = (corner - quarterTurns + 8) % 4;
        v[i].u = su[from]; v[i].v = sv[from];
        v[i].x = sx[corner]; v[i].y = sy[corner]; v[i].z = 0;
    }
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDisable(GU_CULL_FACE);
    sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 6, 0, v);
    sceGuEnable(GU_CULL_FACE);
    sceGuDisable(GU_BLEND);
    TextureManager::Instance()->InvalidateCurrentTexture();
}

static std::string SkinShortName(const std::string& name, size_t longest, bool upper)
{
    std::string label = name;
    if(label.size() > 4)
    {
        std::string ext = label.substr(label.size() - 4);
        if(ext == ".png" || ext == ".PNG")
            label = label.substr(0, label.size() - 4);
    }
    if(label.size() > longest)
        label = label.substr(0, longest);
    if(upper)
    {
        for(size_t i = 0; i < label.size(); i++)
        {
            if(label[i] >= 'a' && label[i] <= 'z')
                label[i] = (char)(label[i] - 'a' + 'A');
        }
    }
    return label;
}

static void DrawFrame(float x, float y, float w, float h, float t, unsigned int color)
{
    DrawScreenRect(x, y, w, t, color);
    DrawScreenRect(x, y + h - t, w, t, color);
    DrawScreenRect(x, y + t, t, h - 2 * t, color);
    DrawScreenRect(x + w - t, y + t, t, h - 2 * t, color);
}

void StateMenu::BuildSkinIcons()
{
    ClearSkinPreviews();
    for(int i = 0; i < (int)skinList.size(); i++)
    {
        const char* name = (skinList[i] == "Steve") ? "Steve" : skinList[i].c_str();
        Aurora::Graphics::Image* sheet = TextureHelper::Instance()->LoadPlayerSkinImage(name, 0);
        sSkinIcons.push_back(MakeBodyIcon(sheet));
        FreeSkinImage(sheet);
    }
}

void StateMenu::DrawSkinStage()
{
    const int columns = 4;
    const int rows = 3;
    const float gridX = 42.0f;
    const float gridY = 36.0f;
    const float tileW = 52.0f;
    const float tileH = 72.0f;
    const float stepX = 56.0f;
    const float stepY = 76.0f;
    const float viewX = 276.0f;
    const float viewY = 32.0f;
    const float viewW = 196.0f;
    const float viewH = 162.0f;
    const unsigned int accent = GU_RGBA(96, 214, 72, 255);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuColor(GU_COLOR(1, 1, 1, 1.0f));

    for(int x = 0; x < 8; x++)
    {
        for(int y = 0; y < 5; y++)
        {
            backSprite->SetPosition(x * 64, y * 64);
            backSprite->Draw();
        }
    }

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();

    int count = (int)skinList.size();
    const char* equipped = TextureHelper::Instance()->PlayerSkinName();

    // Title bar, body, side tabs, grid well, preview stage, info panel.
    DrawScreenRect(4, 4, 472, 22, GU_RGBA(198, 198, 198, 255));
    DrawScreenRect(4, 24, 472, 2, GU_RGBA(120, 120, 120, 255));
    DrawScreenRect(396, 8, 76, 14, GU_RGBA(70, 70, 74, 255));
    DrawScreenRect(4, 26, 472, 242, GU_RGBA(44, 44, 48, 240));
    DrawScreenRect(8, 32, 28, 232, GU_RGBA(58, 58, 64, 255));
    DrawScreenRect(40, 32, 232, 232, GU_RGBA(26, 26, 30, 255));
    DrawScreenRect(viewX, viewY, viewW, viewH, GU_RGBA(14, 16, 14, 255));
    DrawScreenRect(viewX, 198, viewW, 66, GU_RGBA(30, 30, 34, 255));
    DrawFrame(viewX, 198, viewW, 66, 1, GU_RGBA(80, 80, 86, 255));

    for(int tab = 0; tab < 2; tab++)
    {
        float ty = 38.0f + tab * 30.0f;
        unsigned int fill = (tab == 0) ? GU_RGBA(96, 96, 104, 255) : GU_RGBA(72, 72, 78, 255);
        DrawScreenRect(10, ty, 24, 24, fill);
        if(skinFocus == 2 && skinTab == tab)
            DrawFrame(9, ty - 1, 26, 26, 2, accent);
    }
    int equippedIndex = 0;
    for(int i = 0; i < count; i++)
    {
        if(equipped != NULL && skinList[i] == equipped)
            equippedIndex = i;
    }
    if(equippedIndex < (int)sSkinIcons.size())
        DrawIconQuad(sSkinIcons[equippedIndex], 16, 40, 10, 20);

    for(int shown = 0; shown < columns * rows; shown++)
    {
        int index = skinStart * columns + shown;
        if(index >= count)
            break;
        float x = gridX + (shown % columns) * stepX;
        float y = gridY + (shown / columns) * stepY;
        DrawScreenRect(x, y, tileW, tileH, GU_RGBA(72, 72, 78, 255));
        DrawScreenRect(x + 2, y + 2, tileW - 4, tileH - 4, GU_RGBA(92, 92, 98, 255));
        if(index < (int)sSkinIcons.size())
            DrawIconQuad(sSkinIcons[index], x + 10, y + 4, 32, 64);
        if(index == equippedIndex)
            DrawScreenRect(x + tileW - 9, y + 3, 6, 6, accent);
        if(index == skinPos)
        {
            unsigned int ring = (skinFocus == 0) ? accent : GU_RGBA(60, 130, 50, 255);
            DrawFrame(x - 1, y - 1, tileW + 2, tileH + 2, 3, ring);
        }
    }

    if(skinFocus == 1)
        DrawFrame(viewX, viewY, viewW, viewH, 2, accent);

    unsigned int equipFill = (skinFocus == 1) ? accent : GU_RGBA(70, 70, 76, 255);
    DrawScreenRect(viewX + 4, 238, 94, 22, equipFill);
    DrawScreenRect(viewX + 102, 238, 90, 22, GU_RGBA(200, 200, 200, 255));

    if(skinPos != sSkinPreviewFor)
    {
        FreeSkinImage(sSkinPreview);
        sSkinPreview = NULL;
        sSkinPreviewFor = skinPos;
        if(skinPos >= 0 && skinPos < count)
        {
            const char* name = (skinList[skinPos] == "Steve") ? "Steve" : skinList[skinPos].c_str();
            sSkinPreview = TextureHelper::Instance()->LoadPlayerSkinImage(name, 1);
        }
    }

    if(sSkinPreview != NULL && sSkinPreview->ImageData != NULL)
    {
        BuildPlayerMesh();
        if(sPlayerMesh != NULL)
        {
            // Depth decides which face is in front, so the faces can be submitted in any order and
            // with culling off. Writes have to be on for that; the menu does not promise either.
            sceGuEnable(GU_DEPTH_TEST);
            sceGuDepthFunc(GU_GEQUAL);
            sceGuDepthMask(0);
            sceGuDisable(GU_CULL_FACE);
            sceGuDisable(GU_BLEND);
            sceGuEnable(GU_ALPHA_TEST);
            sceGuAlphaFunc(GU_GREATER, 0x10, 0xff);
            sceGuEnable(GU_TEXTURE_2D);
            sceGuTexMode(sSkinPreview->ColorMode, 0, 0, sSkinPreview->Swizzle);
            sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
            sceGuTexFilter(GU_NEAREST, GU_NEAREST);
            sceGuTexWrap(GU_CLAMP, GU_CLAMP);
            sceGuTexImage(0, sSkinPreview->power2Width, sSkinPreview->power2Height, sSkinPreview->power2Width, sSkinPreview->ImageData);
            sceGuColor(0xFFFFFFFF);
            sceGuScissor((int)viewX + 2, (int)viewY + 2, (int)(viewX + viewW) - 2, (int)(viewY + viewH) - 2);

            // About 125 px tall, raised clear of the hint line, looking slightly down at it.
            sceGumMatrixMode(GU_PROJECTION);
            sceGumLoadIdentity();
            sceGumPerspective(28.0f, 480.0f / 272.0f, 0.5f, 30.0f);
            sceGumMatrixMode(GU_VIEW);
            sceGumLoadIdentity();
            sceGumMatrixMode(GU_MODEL);
            sceGumLoadIdentity();
            ScePspFVector3 place = {2.2f, 0.49f, -9.0f};
            sceGumTranslate(&place);
            sceGumRotateX(0.14f);
            sceGumRotateY(skinSpin);
            ScePspFVector3 centre = {0.0f, -1.0f, 0.0f};
            sceGumTranslate(&centre);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, sPlayerMeshCount, 0, sPlayerMesh);

            sceGuScissor(0, 0, 480, 272);
            sceGuDisable(GU_ALPHA_TEST);
            sceGuEnable(GU_CULL_FACE);
            mRender->SetOrtho(0, 0, 0, 0, 0, 0);
            TextureManager::Instance()->InvalidateCurrentTexture();
            sceGuDisable(GU_DEPTH_TEST);
            sceGuEnable(GU_BLEND);
            sceGuColor(GU_COLOR(1, 1, 1, 1));
        }
    }

    // Text over the stage would fail the depth test against the figure.
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DEPTH_TEST);

    // Each label's y is its box's centre plus 9 (0.5 text) or 7 (0.35 text); see DrawPrompt.
    const int stageX = (int)(viewX + viewW / 2);
    DrawText(64, 24, GU_COLOR(0.2f, 0.2f, 0.2f, 1), default_size, "Skins");
    DrawText(434, 22, GU_COLOR(1, 1, 1, 1), default_small_size, "%d skin%s", count, count == 1 ? "" : "s");
    DrawText(22, 88, GU_COLOR(1, 1, 1, 1), default_size, "+");

    int totalRows = (count + columns - 1) / columns;
    if(totalRows > rows)
        DrawText(240, 22, GU_COLOR(0.25f, 0.25f, 0.25f, 1), default_small_size, "row %d of %d", skinStart + 1, totalRows - rows + 1);

    if(skinPos >= 0 && skinPos < count)
    {
        std::string name = SkinShortName(skinList[skinPos], 16, true);
        DrawText(stageX, 217, GU_COLOR(1, 1, 1, 1), default_size, "%s", name.c_str());
        if(skinMessage.size() > 0)
            DrawText(stageX, 231, GU_COLOR(1, 0.8f, 0.4f, 1), default_small_size, "%s", skinMessage.c_str());
        else if(equipped != NULL && skinList[skinPos] == equipped)
            DrawText(stageX, 231, GU_COLOR(0.45f, 0.9f, 0.35f, 1), default_small_size, "Equipped");
        else if(skinList[skinPos] == "Steve")
            DrawText(stageX, 231, GU_COLOR(0.8f, 0.8f, 0.8f, 1), default_small_size, "Built in");
        else
            DrawText(stageX, 231, GU_COLOR(0.8f, 0.8f, 0.8f, 1), default_small_size, "From your Skins folder");
    }
    DrawPrompt((int)viewX + 51, 258, GU_COLOR(1, 1, 1, 1), default_size, "{X} Equip");
    DrawPrompt((int)viewX + 147, 258, GU_COLOR(0.2f, 0.2f, 0.2f, 1), default_size, "{S} Import");

    if(skinFocus == 1)
        DrawPrompt(stageX, (int)(viewY + viewH) - 3, GU_COLOR(0.85f, 0.85f, 0.85f, 1), default_size, "{<}{>} Turn      {O} Back");
    else if(skinFocus == 0)
        DrawPrompt(stageX, (int)(viewY + viewH) - 3, GU_COLOR(0.7f, 0.7f, 0.7f, 1), default_size, "{X} View");

    sceGuEnable(GU_DEPTH_TEST);
}

void StateMenu::OpenSkinImport()
{
    skinBrowsePath = "ms0:/";
    ScanSkinImport();
    if(skinBrowseNames.empty())
    {
        skinBrowsePath = "ef0:/";
        ScanSkinImport();
        if(skinBrowseNames.empty())
        {
            skinBrowsePath = "ms0:/";
            skinMessage = "Nothing to import on the memory stick";
        }
    }
    skinBrowsePos = 0;
    skinBrowseStart = 0;
    menuState = 14;
}

void StateMenu::ScanSkinImport()
{
    skinBrowseNames.clear();
    skinBrowseDir.clear();
    skinBrowsePos = 0;
    skinBrowseStart = 0;

    SceUID dir = sceIoDopen(skinBrowsePath.c_str());
    if(dir < 0)
        return;

    SceIoDirent ent;
    int kept = 0;
    while(kept < 180)
    {
        memset(&ent, 0, sizeof(ent));
        if(sceIoDread(dir, &ent) <= 0)
            break;
        if(ent.d_name[0] == '.')
            continue;
        std::string child = skinBrowsePath + ent.d_name;
        SceUID sub = sceIoDopen(child.c_str());
        if(sub >= 0)
        {
            sceIoDclose(sub);
            skinBrowseNames.push_back(ent.d_name);
            skinBrowseDir.push_back(1);
            kept++;
            continue;
        }
        if(SkinFileName(ent.d_name))
        {
            skinBrowseNames.push_back(ent.d_name);
            skinBrowseDir.push_back(0);
            kept++;
        }
    }
    sceIoDclose(dir);
}

void StateMenu::ImportHighlightedSkin()
{
    if(skinBrowsePos < 0 || skinBrowsePos >= (int)skinBrowseNames.size())
        return;
    if(skinBrowseDir[skinBrowsePos])
        return;

    const std::string& base = skinBrowseNames[skinBrowsePos];
    std::string src = skinBrowsePath + base;
    std::string dst = std::string("Skins/") + base;

    SceUID in = sceIoOpen(src.c_str(), PSP_O_RDONLY, 0);
    if(in < 0)
    {
        skinMessage = "Could not read that file";
        menuState = 13;
        return;
    }
    SceUID out = sceIoOpen(dst.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if(out < 0)
    {
        sceIoClose(in);
        skinMessage = "Could not copy into Skins";
        menuState = 13;
        return;
    }

    char block[4096];
    int total = 0;
    bool tooBig = false;
    while(1)
    {
        int got = sceIoRead(in, block, sizeof(block));
        if(got < 0)
        {
            tooBig = true;
            break;
        }
        if(got == 0)
            break;
        total += got;
        if(total > 256 * 1024)
        {
            tooBig = true;
            break;
        }
        if(sceIoWrite(out, block, got) != got)
        {
            tooBig = true;
            break;
        }
    }
    sceIoClose(in);
    sceIoClose(out);

    if(tooBig)
    {
        sceIoRemove(dst.c_str());
        skinMessage = "That file is not a skin";
        menuState = 13;
        return;
    }

    if(TextureHelper::Instance()->SetPlayerSkin(base.c_str()) == false)
    {
        sceIoRemove(dst.c_str());
        skinMessage = "That file is not a 64x32 or 64x64 skin";
        menuState = 13;
        return;
    }

    ScanSkins();
    skinPos = 0;
    for(int i = 0; i < (int)skinList.size(); i++)
    {
        if(skinList[i] == base)
            skinPos = i;
    }
    skinMessage = std::string("Imported ") + base;
    menuState = 13;
}

void StateMenu::DrawSkinImport()
{
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuColor(GU_COLOR(1, 1, 1, 1.0f));

    for(int x = 0; x < 8; x++)
    {
        for(int y = 0; y < 5; y++)
        {
            backSprite->SetPosition(x * 64, y * 64);
            backSprite->Draw();
        }
    }

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();

    const unsigned int accent = GU_RGBA(96, 214, 72, 255);
    DrawScreenRect(4, 4, 472, 22, GU_RGBA(198, 198, 198, 255));
    DrawScreenRect(4, 24, 472, 2, GU_RGBA(120, 120, 120, 255));
    DrawScreenRect(4, 26, 472, 242, GU_RGBA(44, 44, 48, 240));
    DrawScreenRect(12, 32, 456, 18, GU_RGBA(26, 26, 30, 255));
    DrawScreenRect(12, 54, 456, 186, GU_RGBA(26, 26, 30, 255));

    int shown = 0;
    for(int i = skinBrowseStart; i < (int)skinBrowseNames.size() && shown < 6; i++, shown++)
    {
        float rowY = 58.0f + shown * 30.0f;
        DrawScreenRect(16, rowY, 448, 26, (i == skinBrowsePos) ? GU_RGBA(92, 92, 98, 255) : GU_RGBA(60, 60, 66, 255));
        if(i == skinBrowsePos)
            DrawFrame(15, rowY - 1, 450, 28, 2, accent);
    }

    sceGuDisable(GU_BLEND);

    DrawText(120, 24, GU_COLOR(0.2f, 0.2f, 0.2f, 1), default_size, "Import from memory stick");
    const char* path = skinBrowsePath.c_str();
    if(skinBrowsePath.size() > 60)
        path = skinBrowsePath.c_str() + skinBrowsePath.size() - 60;
    DrawText(240, 48, GU_COLOR(0.8f, 0.8f, 0.8f, 1), default_small_size, "%s", path);

    shown = 0;
    for(int i = skinBrowseStart; i < (int)skinBrowseNames.size() && shown < 6; i++, shown++)
    {
        int rowY = 80 + shown * 30;
        unsigned int color = (i == skinBrowsePos) ? GU_COLOR(1, 1, 1, 1) : GU_COLOR(0.8f, 0.8f, 0.8f, 1);
        std::string label = skinBrowseNames[i];
        if(skinBrowseDir[i])
            label += "/";
        if(label.size() > 40)
            label = label.substr(0, 40);
        DrawText(240, rowY, color, default_size, "%s", label.c_str());
    }
    if(skinBrowseNames.empty())
        DrawText(240, 140, GU_COLOR(1, 1, 1, 1), default_size, "No skins in this folder");
    DrawPrompt(240, 263, GU_COLOR(1, 1, 1, 1), default_size, "{X} Open or import      {O} Up a folder");

    sceGuEnable(GU_DEPTH_TEST);
}

static bool SkinFileName(const char* name)
{
    int n = 0;
    while(name[n] != 0)
        n++;
    if(n < 5)
        return false;
    const char* e = name + n - 4;
    if(e[0] != '.')
        return false;
    char a = e[1];
    char b = e[2];
    char c = e[3];
    if(a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if(b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if(c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return a == 'p' && b == 'n' && c == 'g';
}

void StateMenu::ScanSkins()
{
    skinList.clear();
    skinList.push_back("Steve");

    DIR* dir = opendir("Skins");
    if(dir == NULL)
    {
        BuildSkinIcons();
        return;
    }

    struct dirent* entry;
    while((entry = readdir(dir)) != NULL)
    {
        if(entry->d_name[0] == '.')
            continue;
        if(SkinFileName(entry->d_name) == false)
            continue;
        std::string full = std::string("Skins/") + entry->d_name;
        struct stat st;
        if(stat(full.c_str(), &st) != 0 || S_ISDIR(st.st_mode))
            continue;
        skinList.push_back(entry->d_name);
    }
    closedir(dir);
    BuildSkinIcons();
}

void StateMenu::ScanTexturePacks(const char* dirName)
{
    if(texturePackList.empty() != false)
    {
        TextureHelper::Instance()->RemoveConstTextures();
        for(int j = 0; j < texturePackList.size(); j++)
        {
            if (texturePackList[j].packSprite != NULL)
            {
                delete texturePackList[j].packSprite;
            }
        }
        texturePackList.clear();
    }
    texturePackList.clear();

    TextureManager::Instance()->LoadConstTexture("Assets/unknown_pack.png");

    int TPcheck = 0;

    DIR *dir = opendir(dirName);
    if (dir == NULL)
        return;

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL)
    {
        std::string plik = entry->d_name;
        std::string fullPath = std::string(dirName) + plik;

        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        {

            size_t found = plik.find(".");
            size_t found2 = plik.find("..");
            size_t found3 = plik.find(".svn");

            if(found==std::string::npos && found2==std::string::npos && found3==std::string::npos)//ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ­ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ¥ ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ­ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ©ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ¤ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ¥ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ­ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ®
            {
                TP newTP;

                std::string plik2 = plik + "/";
                if(plik2 == TextureHelper::Instance()->defaultFolder)
                {
                    tpCurrent = TPcheck;
                }

                newTP.name = plik;

                texturePackList.push_back(newTP);
                TPcheck++;
            }
        }
    }

    closedir(dir);

    for(int j = 0; j < texturePackList.size(); j++)
    {
        std::string packPath = "Assets/Textures/"+texturePackList[j].name+"/pack.png";

        if(fileExists(packPath) == true) // if we have pack sprite
        {
            TextureManager::Instance()->LoadConstTexture(packPath);
            texturePackList[j].packSprite = new Sprite(TextureManager::Instance()->GetConstTextureNumber(packPath),true);
        }
        else
        {
            texturePackList[j].packSprite = new Sprite(TextureManager::Instance()->GetConstTextureNumber("Assets/unknown_pack.png"),true);
        }

        std::string packDescriptionPath = "Assets/Textures/"+texturePackList[j].name+"/pack.txt";

        if(fileExists(packDescriptionPath) == true) // if we have pack description file
        {
            std::string str;
            std::string file_contents;

            std::ifstream file(packDescriptionPath.c_str());

            if (file.is_open())
            {
                while (std::getline(file, str))
                {
                    file_contents += str;
                    file_contents.push_back('\n');
                }
            }

            file.close();
            texturePackList[j].description = file_contents;
        }
    }
}
inline bool StateMenu::fileExists (const std::string& name)
{
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0);
}

unsigned int StateMenu::hash(const char* s, unsigned int seed)
{
    unsigned int hash = seed;
    while (*s)
    {
        hash = hash * 101  +  *s++;
    }
    return hash;
}

int StateMenu::fileSize (const std::string& name)
{
    struct stat stat_buf;
    int rc = stat(name.c_str(), &stat_buf);
    return rc == 0 ? (int)stat_buf.st_size : -1;
}

void StateMenu::DrawPrompt(int cx, int y, unsigned int color, float size, const char* spec)
{
    struct PromptPart
    {
        char icon;
        std::string text;
        float width;
    };
    int buttons = TextureHelper::Instance()->GetTexture(TextureHelper::buttonIconsTexture);
    int dpad = TextureHelper::Instance()->GetTexture(TextureHelper::dpadTexture);

    std::vector<PromptPart> parts;
    PromptPart run;
    run.icon = 0;
    run.width = 0;
    for(const char* c = spec; *c != 0; c++)
    {
        if(c[0] == '{' && c[1] != 0 && c[2] == '}')
        {
            char code = c[1];
            c += 2;
            bool face = (code == 'X' || code == 'O' || code == 'S' || code == 'T');
            bool arrow = (code == '<' || code == '>');
            if((face && buttons >= 0) || (arrow && dpad >= 0))
            {
                if(!run.text.empty())
                    parts.push_back(run);
                run.text.clear();
                if(!parts.empty() && parts.back().icon != 0)
                {
                    PromptPart gap;
                    gap.icon = ' ';
                    gap.width = 4.0f;
                    parts.push_back(gap);
                }
                PromptPart icon;
                icon.icon = code;
                icon.width = face ? 12.0f : 16.0f;
                parts.push_back(icon);
            }
            else if(code == 'S')
                run.text += "[]";
            else if(code == 'T')
                run.text += "/\\";
            else
                run.text += code;
            continue;
        }
        run.text += *c;
    }
    if(!run.text.empty())
        parts.push_back(run);

    // Measured at the size it is drawn; GetTextWidth measures at the current font style.
    mRender->SetFontStyle(size, color, 0, 0x00000000);
    float total = 0;
    for(size_t i = 0; i < parts.size(); i++)
    {
        if(parts[i].icon == 0)
            parts[i].width = (float)mRender->GetTextWidth(parts[i].text.c_str());
        total += parts[i].width;
    }

    // Where a line of this font sits relative to y, measured on the menu: 9 px up at 0.5, 7 at 0.35.
    float iconY = (float)y - ((size >= 0.45f) ? 9.0f : 7.0f);
    float x = (float)cx - total * 0.5f;
    for(size_t i = 0; i < parts.size(); i++)
    {
        const PromptPart& part = parts[i];
        float mid = x + part.width * 0.5f;
        switch(part.icon)
        {
            case 'T': DrawSheetCell(buttons, 0, 0, mid, iconY, 0); break;
            case 'O': DrawSheetCell(buttons, 16, 0, mid, iconY, 0); break;
            case 'X': DrawSheetCell(buttons, 32, 0, mid, iconY, 0); break;
            case 'S': DrawSheetCell(buttons, 48, 0, mid, iconY, 0); break;
            case '>': DrawSheetCell(dpad, 0, 0, mid, iconY, 1); break;
            case '<': DrawSheetCell(dpad, 0, 0, mid, iconY, 3); break;
            case ' ': break;
            default:
                mRender->SetFontStyle(size, GU_COLOR(0.1f, 0.1f, 0.1f, 1), 0, 0x00000000);
                mRender->DebugPrint((int)x + 1, y + 1, "%s", part.text.c_str());
                mRender->SetFontStyle(size, color, 0, 0x00000000);
                mRender->DebugPrint((int)x, y, "%s", part.text.c_str());
                break;
        }
        x += part.width;
    }
}

void StateMenu::DrawText(int x,int y, unsigned int color, float size, const char *message, ...)
{
    va_list argList;
    char cbuffer[512];
    va_start(argList, message);
    vsnprintf(cbuffer, 512, message, argList);
    va_end(argList);

    mRender->SetFontStyle(size,GU_COLOR(0.1f,0.1f,0.1f,1),0,0x00000200|0x00000000);
    mRender->DebugPrint(x+1,y+1,"%s",cbuffer);
    mRender->SetFontStyle(size,color,0,0x00000200|0x00000000);
    mRender->DebugPrint(x,y,"%s",cbuffer);
}