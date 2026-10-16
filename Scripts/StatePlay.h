#ifndef STATEPLAY_H_
#define STATEPLAY_H_

#include <Aurora/Utils/StateManager.h>
#include <Aurora/Utils/GameState.h>

#include <math.h>
#include <psptypes.h>

#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/Utils/Logger.h>
#include <Aurora/Utils/Timer.h>
#include <Aurora/System/SystemManager.h>
#include <Aurora/Graphics/Models/ObjModel.h>
#include <Aurora/Graphics/Camera.h>
#include <Aurora/Graphics/Sprite.h>

#include "DebugConfig.h"

#include <Aurora/Graphics/Effects/InventoryPlayer.h>
#include <Aurora/Graphics/Effects/SkyLight.h>
#include <Aurora/Graphics/Effects/SnowBall2.h>
#include <Aurora/Graphics/Effects/SkyBox.h>
#include "DebugMenu.h"
#include <Aurora/Graphics/Effects/Destroyer.h>
#include <Aurora/Graphics/Effects/StarsBox.h>
#include <Aurora/Graphics/Effects/Drop.h>
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


class StatePlay : public CGameState
{
public:
	StatePlay();
	virtual ~StatePlay();

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

	// Development-only (MC_PERF_PROBE, see DebugConfig.h): times one of each redstone/mesh/light
	// operation in the world that was just generated, so the per-second work counters have a price
	// per unit. Empty in every other build.
	void PerfProbeSelfTest();

	// Development-only (MC_PERF_PROBE): drives the two benchmark torches PerfProbeSelfTest leaves in
	// the world at a plain torch clock's rate, so the per-second work counters describe a *running*
	// clock instead of an idle world. Called once a frame from Update; does nothing once its window
	// is over. Empty in every other build.
	void PerfProbeDriveClock(float frameDt);

#if MC_TEST_ENV
	// Pins the weather (0 clear, 1 rain) for the whole run and, when hour >= 0, the time of day.
	void ApplyTestEnvironment(int weather, int hour);
	// Plants MC_TEST_MOB a few blocks along the look, pose-locked, facing the camera.
	void SpawnTestMobInFront();
#endif

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

	void DrawSlotItem(short& itemId);
	void DrawSlotAmount(short& itemAmount, bool& itemStackable);
	void DrawSlotToolPoints(short& itemAmount, bool& itemStackable);

	void CheckForFurnFuel(Furnace* Fur);
	void CheckForFurnWorking(Furnace* Fur);
	void ReadyFurnSmelting(Furnace* Fur);

    int FindFurnaceId(int x, int y, int z);
	int FindChestId(int x, int y, int z);

	void HungerTime();
	void HealthTime();
	void OxygenTime();
	void HurtPlayer(float damage);
	void HurtPlayerConsiderArmor(float damage);

	// Fires the held bow: builds the shot from the draw that has been accumulated in bowCharge,
	// spends an arrow and plays the release sound. Called from Update on the release edge.
	void FireBowArrow();

private:

    double time12;

	RenderManager *mRender;
	SystemManager *mSystemMgr;
	SoundManager *mSoundMgr;
	InputHelper *mIhelper;
	ChatSystem mChat;
	Camera *fppCam;

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
    InventoryPlayer* invPlayer;

    double temp_total_time_ms;

    bool cycle;
    bool loadReady;
	bool startDt;
	bool tryToDt;
	int pendingCrackId;
	float pendingCrackX, pendingCrackY, pendingCrackZ;
	int pendingBreakStage;
	bool pendingBreakWasLight;
	int pendingBreakChunk;
    float dT;
    float dET;
    char dStd;
    float last_HP;

    float sleepMessageTime;
    int sleepMessageType;
    float sleepTime;

    bool hurt;
    float hurt_time;
    float dtt;

    float furnaceTimes;

    Vector3 testPos1;
	Vector3 cubePos;

	bool showCube;

    short craftSlotId[4];
    short craftSlotAm[4];
    bool craftSlotSt[4];

    short craftSlotId3[9];
    short craftSlotAm3[9];
    bool craftSlotSt3[9];

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
    bool moving;
	bool walkingOnGround;
	bool jumping;
	bool sneaking;
	bool headInWater;
	bool footInWater;
	bool headInLava;
	bool footInLava;
	bool touchingCactus;

	bool invEn;
	bool upEn;
	bool armorEn;
	bool craft3xEn;
	bool chestEn;
	bool furnaceEn;

    // Third-person view ------------------------------------------------------
    // 0 = first person, 1 = behind the player, 2 = in front of them looking back at the face.
    // Toggled by a double tap of the chat button, which cycles the three. A single chat press
    // cannot be acted on immediately or the second tap would land inside the on-screen keyboard,
    // so the press is held for one double-tap window: see chatTapTimer/chatTapPending in Update.
    int thirdPersonView;
    float chatTapTimer;      // > 0 while a single chat press is still waiting to become a double tap
    bool chatTapPending;     // that press has not been turned into a chat open yet

    // The body's yaw for the third-person model, in degrees on the same scale as Camera::horAngle.
    // The head always follows the camera exactly -- Java's rotationYawHead is rotationYaw for the
    // local player -- and the body is dragged after it by updateDistance(), which is what makes the
    // head turn on the spot and the body swing round to catch up. headYawOffset is the turned
    // amount that survives that (clamped), kept here so Draw does not have to redo the clamp.
    float bodyYaw;
    float headYawOffset;

    // Walk cycle. bobWalk is the limb amplitude (ground, air and flight -- no ground gate, see
    // Update), bobCycle is the distance-derived phase both share, and bobCamera is Java's cameraYaw:
    // the camera bob amplitude, in blocks per tick, speed-driven and zeroed whenever the player is not
    // on the ground. Java keeps the two apart for exactly that reason -- the limbs swing through a jump
    // or a flight, the view does not.
    float bobCamera;

    // Sprint, on a double tap of Forward as in Java. The full set of tap bookkeeping the creative
    // sprint-fly already uses, because the two taps have to be told apart inside Java's 7-tick window
    // without a dedicated button. See the sprint block in HandleEvents.
    bool sprinting;
    float sprintTapTimer;
    bool sprintTapArm;
    bool sprintTapPending;
    bool sprintFwdWasHeld;
    float sprintFovBoost;   // eased toward the sprint FOV widening, in degrees, for the view zoom

    float guiScale;
    float guiInvW, guiInvH;
    float guiCrtW, guiCrtH;
    float guiChtW, guiChtH;
    float guiFurW, guiFurH;

    char barPosition;
    int invXPosition;
    int invYPosition;
    float invSteveAngle;

	int ram1;
	int ram2;

    float tickCave; //cave sounds timer
	float tickHunger;
	float tickHealth;
	float tickChunk;
	float tickLavaDamage;
	float tickCactusDamage;
	float tickOS;
	float tickShowSlotName;
	short chunks; // 0 - 7;

	float tutorialTimer;

	float rainSoundFrequency;

	float inputDiskNameTimer;
	std::string inputDiskName;

    int chunkId;

	Timer mTimer;
	float dt;

	//int texture[16];
	int barItems;
	int texture;
	int texture_mips;
	int waterAnimation;

	unsigned char Xpbar;
	unsigned char blue;
	unsigned char red;
	unsigned char black;
	unsigned char stars;

    int invPlayerTex;
    int zombieTex;
    int cowTex;
    int snowBall4;
	int cloudsTex;
	int suntex;
	int moontex;
	int rainTex;

	unsigned char currentTexture;
	unsigned char timeTexture;

	float pre_fps;
	int average_fps;
	float tick_fps;
	int ticks;

	Sprite *waterScreen;
	Sprite *pumpkinMask;

    Sprite *invCellSprite;
	Sprite *invSprite;
    Sprite *crtSprite;
    Sprite *chtSprite;
    Sprite *selectInvSprite;
	Sprite *barSprite;
	Sprite *crossSprite;
	Sprite *selectSprite;

	Sprite *furArrowSprite[22];
	Sprite *furFireSprite[14];
    Sprite *furSprite;

	Sprite *hpCellSprite;
	Sprite *hpCellSpriteW;
	Sprite *hpSprite;
	Sprite *hpSpriteW;
	Sprite *hpHardSprite;
	Sprite *hpHardSpriteW;
	Sprite *hpHalfSprite;
	Sprite *hpHalfSpriteW;
	Sprite *hpHardHalfSprite;
	Sprite *hpHardHalfSpriteW;

	Sprite *hgCellSprite;
	Sprite *hgSprite;
	Sprite *hgHalfSprite;
	Sprite *bubbleSprite;

    Sprite *arCellSprite;
	Sprite *arSprite;
	Sprite *arHalfSprite;

	Sprite *toolPointSprite[14];

	float hpAnim;

	float cameraSpeed;
	float cameraMoveSpeed;

    //menu sprites
	Sprite *buttonSprite;
	Sprite *sbuttonSprite;
	Sprite *nbuttonSprite;
	Sprite *moverSprite;

	bool menuOptions;
    char menuState;//0 game,1 menu
	char optionsMenuPos;
	char selectPos;

	// The debug stick. `debugMenu` owns its own open/closed state and drawing, and the play state
	// only has to pause the world while it is up and apply whatever comes back. `debugTarget` is the
	// mob the stick was used on -- kept, not passed, because the menu closes the instant the player
	// picks something and the action has to be applied to the same mob afterwards.
    DebugMenu debugMenu;
	LameMob* debugTarget;
	void CheckDebugStickUse();
	void ApplyDebugMenuResult();

	// The purple wash drawn over the screen while a portal pulls the player through. Built on first use
	// rather than in the constructor, for the same reason the debug menu builds its sprites lazily: the
	// play states are constructed before the texture pack has been opened, so GetTexture would be -1.
	int portalFadeTex;

	// Whether the world simulation should advance this frame. Everything in Draw that ticks the
	// world -- mobs, drops, TNT, arrows, snowballs, weather -- is guarded on this rather than on
	// `menuState == 0` directly, so the debug menu freezes the world exactly as the pause menu does.
	// The world still *renders* while it is frozen, which is the point: a testing menu you cannot see
	// the subject through is not much of a testing menu.
	bool WorldRunning();

    char statisticsPage;

	//some settings
	bool canFly;
	float flyJumpTimer;
	bool flyJumpWasHeld;
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
    float changeScale;
    int slotForChangeScale;

    float animDest;
    float animSpeed;
    bool animGo;

    bool anim[3];

    // Bow draw. HandleEvents sets bowCharging on the frame the use button goes down -- a bow has
    // nothing to do on the press itself, vanilla just starts drawing -- and Update accumulates the
    // draw and fires it the frame the button stops being held. Split across two functions because
    // the engine's input layer exposes keyPressed and keyHold but no keyReleased, and the release is
    // the one edge a bow needs. bowCharge is 0..1, one second of draw.
    bool bowCharging;
    float bowCharge;
	bool makeScreen;
	bool dieFactor;

	float angleFactor;

    void DrawText(int x,int y, unsigned int color, float size, const char *message, ...);
    void DrawText2(int x,int y, unsigned int color, float size, const char *message, ...);

    void DrawAmount(int x,int y, int amount);
    void UpdateInvCursorPos();

	std::vector<SnowBall2*> mSnowBalls;
};

#endif
