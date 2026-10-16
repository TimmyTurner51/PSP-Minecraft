#ifndef STATEPLAYCREATIVE_H_
#define STATEPLAYCREATIVE_H_

#include <Aurora/Utils/StateManager.h>
#include <Aurora/Utils/GameState.h>

#include <math.h>
#include <psptypes.h>
#include <vector>

#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/Utils/Logger.h>
#include <Aurora/Utils/Timer.h>
#include <Aurora/System/SystemManager.h>
#include <Aurora/Graphics/Models/ObjModel.h>
#include <Aurora/Graphics/Camera.h>
#include <Aurora/Graphics/Sprite.h>

#include <Aurora/Graphics/Effects/SkyLight.h>
#include <Aurora/Graphics/Effects/SnowBall2.h>
#include <Aurora/Graphics/Effects/SkyBox.h>
#include <Aurora/Graphics/Effects/Destroyer.h>
#include <Aurora/Graphics/Effects/StarsBox.h>
#include <Aurora/Graphics/Effects/Drop.h>
#include <Aurora/Graphics/Effects/InventoryPlayer.h>
#include <Aurora/Graphics/Effects/TNT.h>
#include <Aurora/Graphics/Effects/Particle.h>

#include "ChunkDataProvider.h"

#include "Zombie2.h"
#include "Cow2.h"
#include "Creeper2.h"
#include "Pig2.h"
#include "Sheep2.h"
#include "LameMob.h"

#include "CraftWorld2.h"
#include "InputHelper.h"
#include "ChatSystem.h"
#include "DebugMenu.h"
#include "SoundManager.h"
#include "Chest2.h"
#include "Furnace2.h"
#include "NoteBlockEntity.h"
#include "JukeboxEntity.h"
#include "MonsterSpawnerEntity.h"
#include "ItemFrameEntity.h"

#include "LameFunctions.h"


using namespace Aurora::Graphics;
using namespace Aurora::Utils;
using namespace Aurora::System;
using namespace Aurora;


class StatePlayCreative : public CGameState
{
public:
	StatePlayCreative();
	virtual ~StatePlayCreative();

	void Init();
	void InitParametric(bool makeTrees,bool makeWater,bool makeCaves, unsigned int seedIII, int worldType, char gameMode);
	void Enter();
	void CleanUp();

	void Pause();
	void Resume();
	void LoadTextures();

	void HandleEvents(StateManager* sManager);
	void Update(StateManager* sManager);
	void Draw(StateManager* sManager);

	void LoadMap(std::string fileName,bool compressed);
	void SetWorldAndSaveName(std::string worldName,std::string fileName);
	void InitCamera();

	//keys helpers
	bool keyPressed(int currentKey);
	bool keyHold(int currentKey);
	void PutInInventory(int id, int num, bool st);

private:

	void advancedBlit(int sx, int sy, int sw, int sh, int dx, int dy, int slice);
	bool TryToMove(Vector3 moveVector,float dt);
	void SetDayTimeAfterLoad();
	void CraftItem2x2();
	void CraftItem3x3();

	void CheckForFurnFuel(Furnace* Fur);
	void CheckForFurnWorking(Furnace* Fur);
	void ReadyFurnSmelting(Furnace* Fur);

    int FindFurnaceId(int x, int y, int z);

	int FindChestId(int x, int y, int z);

	// Fires the held bow from the accumulated bowCharge. Creative spends no arrow, so this is the
	// same shot as the survival state's with the ammo check removed.
	void FireBowArrow();


private:

	RenderManager *mRender;
	SystemManager *mSystemMgr;
	SoundManager *mSoundMgr;
	InputHelper *mIhelper;
	ChatSystem mChat;
	Camera *fppCam;

	// Third-person view only: the player's own model standing in the world. Survival keeps this as a
	// member already; creative had no use for one until now, so it gets its own.
	InventoryPlayer *invPlayer;

	std::string saveFileName;
	int freeMemory;
	float freeMemoryTimer;

    float cloudsOffset;

	CraftWorld *mWorld;

    SkyLight *skyLight;
    SkyLight *skyMoonLight;
    SkyBox *skyBox;
    Destroyer *destroyer;
    StarsBox *starsBox;

    float starsAngle;
    bool check;
    bool cycle;

	bool startDt;
	int pendingCrackId;
	float pendingCrackX, pendingCrackY, pendingCrackZ;
	int pendingBreakStage;
	bool pendingBreakWasLight;
	int pendingBreakChunk;
    float dT;
    float dET;
    char dStd;

    bool hurt;
    float hurt_time;
    float dtt;

    float furnaceTimes;

    Vector3 testPos1;

	Vector3 cubePos;
	bool showCube;

    float sleepMessageTime;
    int sleepMessageType;
    float sleepTime;

    short craftSlotId[4];
    short craftSlotAm[4];
    bool craftSlotSt[4];

    short craftSlotId3[9];
    short craftSlotAm3[9];
    bool craftSlotSt3[9];

    short creativeSlotId[27];

    short craftItemId;
    short craftItemAm;
    bool craftItemSt;

    short craftItemId3;
    short craftItemAm3;
    bool craftItemSt3;

    float musicTimeGo;
    float musicTime;

    char chestId;
    char furnaceId;

    Chest* UseChest;
    Furnace* UseFurnace;
	//zmienne do poruszania
	float GRAVITY;
	float JUMPVELOCITY;
	float CLIMBVELOCITY;

	Vector3 playerVelocity;
	Vector3 playerPosition;
	Vector3 newPlayerPos;
	Vector3 oldPlayerPos;

    bool moveForward;
	bool walkingOnGround;
	bool moving;
	bool jumping;
	bool sneaking;
	bool headInWater;
	bool footInWater;
	bool headInLava;
	bool footInLava;

	bool invEn;
	bool upEn;
	bool craft3xEn;
	bool chestEn;
	bool furnaceEn;

    // Third-person view ------------------------------------------------------
    // 0 = first person, 1 = behind the player, 2 = in front of them looking back at the face; a
    // double tap of the chat button cycles the three. A single press is held for one double-tap
    // window so the second tap can be recognised before the on-screen keyboard takes the input.
    int thirdPersonView;
    float chatTapTimer;
    bool chatTapPending;

    // Body yaw for the third-person model (degrees, same scale as Camera::horAngle) and the amount
    // the head is turned past it. See StatePlay.h for why the two are separate.
    float bodyYaw;
    float headYawOffset;

    // bobWalk is the limb amplitude (ground, air and flight), bobCycle the phase both share, and
    // bobCamera Java's cameraYaw -- the camera bob amplitude in blocks per tick, speed-driven and zero
    // whenever the player is not on the ground. See StatePlay.h.
    float bobCamera;

    // Sprint, on a double tap of Forward as in Java -- and on the ground it is a different thing from
    // the fly sprint above (flySprint), which only applies while canFly. See StatePlay.h.
    bool sprinting;
    float sprintTapTimer;
    bool sprintTapArm;
    bool sprintTapPending;
    bool sprintFwdWasHeld;
    float sprintFovBoost;

    float guiScale;
    float guiInvW, guiInvH;
    float guiCrtW, guiCrtH;
    float guiChtW, guiChtH;
    float guiFurW, guiFurH;
    float guiCreativeW, guiCreativeH;
    float guiCreativeScale;

	bool usingSlider; // unused; analog mouse + d-pad paging
	bool onDestroySlot;
	bool creativeHeldFromHotbar;
    int creativeTab;
    float creativeMouseX;
    float creativeMouseY;
    bool creativeMouseFree;

	bool showSlotName;
	float showSlotTimer;
	float showSlotTimerMax;
	int slotId;

    char barPosition;
    int invXPosition;
    int invYPosition;

    char tempXFurnace;
    char tempYFurnace;
    bool tempUpFurnace;

    char tempXInv;
    char tempYInv;
    bool tempUpInv;

    char tempXCT;
    char tempYCT;
    bool tempUpCT;

    char tempXChest;
    char tempYChest;
    bool tempUpChest;

	int ram1;
	int ram2;

    float tickCave; //cave sounds timer
	float tickHunger;
	float tickHealth;
	float tickOS;
    float tickChunk;
    float tickShowFlymodeMessage;
    float tickShowSlotName;
    float rainSoundFrequency;

    float inputDiskNameTimer;
	std::string inputDiskName;

	short chunks; // 0 - 7;

    int chunkId;

	Timer mTimer;
	float dt;

	//int texture[16];
	int barItems;
	int texture;
	int texture_mips;
	int waterAnimation;

	unsigned char blue;
	unsigned char red;
	unsigned char black;
	unsigned char stars;
	
	int zombieTex;
    int cowTex;
    int snowBall4;
	int cloudsTex;
	int suntex;
	int moontex;
	int grassColorTex;

	unsigned char currentTexture;
	unsigned char timeTexture;

    float pre_fps;
	int average_fps;
	float tick_fps;
	int ticks;

    Sprite *waterScreen;

    Sprite *invCellSprite;
	Sprite *invSprite;
	Sprite *creativeTabSelSprite[5];
	Sprite *sliderSprite;
	Sprite *sliderLightedSprite;
    Sprite *crtSprite;
    Sprite *chtSprite;
    Sprite *selectInvSprite;
	Sprite *barSprite;
	Sprite *crossSprite;
	Sprite *selectSprite;

	Sprite *furArrowSprite[22];
	Sprite *furFireSprite[14];
    Sprite *furSprite;
    Sprite *toolPointSprite[14];

	float cameraSpeed;
	float cameraMoveSpeed;

	//menu sprites
	Sprite *buttonSprite;
	Sprite *sbuttonSprite;
	Sprite *nbuttonSprite;
	Sprite *moverSprite;

	int menuState;//0 game,1 menu
	bool menuOptions;
	int optionsMenuPos;
	int selectPos;

	// The debug stick. Same members and same contract as StatePlay's -- see the comment there. Kept
	// per state rather than shared because each state owns its own mob lists through mWorld, and a
	// menu that outlived a state change would be pointing at a dead world.
	DebugMenu debugMenu;
	LameMob* debugTarget;
	void CheckDebugStickUse();
	void ApplyDebugMenuResult();

	// The purple wash drawn over the screen while a portal pulls the player through. Built on first use
	// rather than in the constructor, for the same reason the debug menu builds its sprites lazily: the
	// play states are constructed before the texture pack has been opened, so GetTexture would be -1.
	int portalFadeTex;

	// Whether the world simulation should advance this frame. Same contract as StatePlay's -- see the
	// comment there.
	bool WorldRunning();

    char statisticsPage;
	//some settings
	bool canFly;
	float flyJumpTimer;
	bool flyJumpWasHeld;
	float flyBoost;
	float flyForwardTimer;
	bool flyForwardWasHeld;
	bool flyForwardPending;
	bool flySprintArm;
	bool flySprint;
	float flyFovBoost;
	// Fly carries momentum: releasing the stick decelerates instead of stopping dead, which is what
	// real Minecraft does. This is the velocity the glide decays from; walking never uses it.
	Vector3 flyGlideVelocity;
	bool devMode;
	bool analogLeft,analogRight,analogUp,analogDown;

	//for sound of walkinng
	float walkSoundAccu;
	bool  isWalking;

	//Headbob
	bool canHeadBob;
	float bobCycle;
	bool bobType;
	float bobWalk;
	float camFollowY;
	bool camFollowYInit;
	float lookDt;
	float cubeLight;

    float shift_x;
    float shift_y;
    float changeY;

    float animDest;
    float animSpeed;
    bool animGo;

    bool anim[3];

    // Bow draw, same split as the survival state and for the same reason: HandleEvents starts it on
    // the use button going down, Update accumulates it and fires on the release edge (the engine has
    // keyPressed/keyHold, no keyReleased). Creative needs no arrow, so the draw never comes up dry.
    bool bowCharging;
    float bowCharge;
	bool makeScreen;
	bool dieFactor;

	float angleFactor;

    void SetCreativePage(char page);
    void InitCreativeInventory();
    void SetCreativeTab(int tab);
    void RefreshCreativePageMax();
    std::vector<int>& CreativeItems();

    void DrawText(int x,int y, unsigned int color, float size, const char *message, ...);
    void DrawText2(int x,int y, unsigned int color, float size, const char *message, ...);
    void DrawAmount(int x,int y, int amount);
    void UpdateInvCursorPos();
    void CreativeUpdateAnalogMouse();
    void CreativePickSlotFromMouse();

	std::vector<SnowBall2*> mSnowBalls;
	std::vector<int> creativeTabItems[5];

    int creativePage;
    int creativePageMax;
};

#endif