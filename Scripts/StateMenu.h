
#ifndef STATEMENU_H_
#define STATEMENU_H_

#include <stdlib.h>

#include <Aurora/Utils/StateManager.h>
#include <Aurora/Utils/GameState.h>
#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/Utils/Logger.h>
#include <Aurora/Utils/Timer.h>
#include <Aurora/Utils/pgeZip.h>
#include <Aurora/Utils/pgeDir.h>
#include <Aurora/Utils/pge.h>

#include <Aurora/System/SystemManager.h>
#include <Aurora/Graphics/Models/ObjModel.h>
#include <Aurora/Graphics/Camera.h>
#include <Aurora/Graphics/Sprite.h>

#include <pspiofilemgr.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include<fstream>
#include<iostream>

#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __PSP__
	#include <sys/stat.h>
#endif


#include "SoundManager.h"

using namespace Aurora::Graphics;
using namespace Aurora::Utils;
using namespace Aurora::System;
using namespace Aurora;

class SaveFile
{
public:

	int saveVersion;
	bool compression;
	char worldGameMode;
	char worldName[50];
	bool locked;
	std::string fileName;

	int saveSize;
};

class TP
{
public:
    Sprite* packSprite;
	std::string name;
	std::string description;
};

class StateMenu : public CGameState
{
public:
	StateMenu();
	virtual ~StateMenu();

	void Init();
	void Enter();
	void CleanUp();

	void Pause();
	void Resume();

	void HandleEvents(StateManager* sManager);
	void Update(StateManager* sManager);
	void Draw(StateManager* sManager);

private:

	void ScanSaveFiles(const char* dirName);
	void ScanTexturePacks(const char* dirName);
	void ScanSkins();
	void BuildSkinIcons();
	void OpenSkinImport();
	void ScanSkinImport();
	void ImportHighlightedSkin();
	void DrawSkinStage();
	void DrawSkinImport();
	void DrawText(int x,int y, unsigned int color, float size, const char *message, ...);
	// A centred prompt line. {X} {O} {S} {T} draw the launcher's button icons (title/buttons.png),
	// {<} {>} the d-pad with that arm lit (title/dpad.png). A pack without the art gets the words.
	void DrawPrompt(int cx, int y, unsigned int color, float size, const char* spec);
	inline bool fileExists(const std::string& name);
	int fileSize(const std::string& name);

    unsigned int hash(const char* s, unsigned int seed);

private:

    typedef struct Statistics
    {
        unsigned short blockPlaced;
        unsigned short blockDestroyed;
        unsigned short daysInGame;
        unsigned short minutesPlayed;
        unsigned short itemsCrafted;
        unsigned short itemsSmelted;
        unsigned short jumps;
        unsigned short dies;
        unsigned short foodEaten;
        unsigned short badlyFalls;
        unsigned short blocksWalked;
        unsigned short treesGrowned;
        unsigned short cropsGrowned;
        unsigned short soilPlowed;
        unsigned short timeInWater;
        unsigned short timeInAir;
        unsigned short damageRecieved;
    } st;

    typedef struct Options
    {
        bool useMipsTexturing;
        bool detailedSky;
        bool fastRendering;
        bool mipMapTexturing;
        bool freezeDayTime;
        bool sounds;
        bool music;
        bool autoJump;
        bool worldBlockAnimation;
        bool fogRendering;
        bool smoothLighting;
        bool particles;
        bool guiDrawing;

        float fov;
        char horizontalViewDistance;
        char verticalViewDistance;
        int difficult;
    } opt;

	float musicTimeGo;
    float musicTime;

    st mainStatistics;
    opt mainOptions;

    Sprite *backgroundSprite;
    float bx, by;
    bool directionx;
    bool directiony;

    Sprite *buttonSmallSprite;
	Sprite *sbuttonSmallSprite;
	Sprite *nbuttonSmallSprite;

	Sprite *buttonSprite;
	Sprite *sbuttonSprite;

	Sprite *nbuttonSprite;

	Sprite *mbuttonSprite;
	Sprite *smbuttonSprite;

	Sprite *backSprite;
	Sprite *logoSprite;
	Sprite *lamecraftSprite;

	Sprite *rectFilledSprite;
	Sprite *rectEmptySprite;

	Sprite *blackBackground;
	// The d-pad hints in the News & Converter changelog box: the up cell and the down cell of the
	// two-cell dpad.png atlas, in the box's top-right and bottom-right corners.
	Sprite *dpadUpSprite;
	Sprite *dpadDownSprite;
	//Sprite *screen1Sprite;
	//Sprite *screen2Sprite;
	//Sprite *screen3Sprite;
	//Sprite *screen4Sprite;

	RenderManager *mRender;
	SystemManager *mSystemMgr;
	SoundManager *mSoundMgr;

    // converter vars
    bool schematicExists;

	short newW_width;
	short newW_height;
	short newW_length;

	char newW_gameMode;
	bool newW_deleteCaves;
	int conversionStage;
	int errorType;

	std::string lol;

    int converterPos;

    int SplashNumber;
    float splashSize;

    unsigned int seed_1;
	int selectPos;
	int loadSelectPos;
	int loadSavePos;
	int aboutPos;
	// News & Converter focus ladder: 0 = the two buttons, 1 = the changelog box selected,
	// 2 = scrolling inside the box. newsScroll is the first line shown.
	int aboutFocus;
	int newsScroll;
	char worldName[32];

    float size_f;

	float fontcolor;
	bool fontcoloroption;

	bool saveSubmenu;
	int saveSubMenuSelect;

    pgeZip* theZip;

	int loadSaveStart;
	int loadSaveEnd;
	int loadSaveMax;

	short animationscreen;
	short menuState;//0 main,1 load,2 options

	int nextSaveFileNumber;
	std::string nextSaveFileName;

	std::vector<SaveFile> saveFilesList;
	std::vector<TP> texturePackList;
	std::vector<std::string> skinList;
	int skinPos;
	int skinStart;
	int skinReturnState;
	float skinSpin;
	// 0 = the skin grid, 1 = the preview (turns with the stick, Cross equips), 2 = the side tabs.
	int skinFocus;
	int skinTab;
	std::string skinMessage;
	std::string skinBrowsePath;
	std::vector<std::string> skinBrowseNames;
	std::vector<char> skinBrowseDir;
	int skinBrowsePos;
	int skinBrowseStart;
	std::string newWorldName;
	std::string newWorldSeed;
	std::string newWorldNamestr;
	//options for parametric map generation
	bool makeTrees;
	bool makeWater;
	bool makeCaves;
    int terrainBuilder;
	short gameMode;
	int worldType;

    short generateSelectPose;

	//game version
	short currentVersion;

	int tpCurrent;
	int tpMax;
	int tpEnd;
	int tpStart;
	int tpPos;
	int tpSelectPos;
};

#endif