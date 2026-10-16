 #include "StatePlay.h"
#include <Aurora/Graphics/Effects/Arrow2.h>
#include "TextureHelper.h"
#include "DebugConfig.h"
#include "PerfCounters.h"
#include <stdio.h>
#include <stdarg.h>
#include <pspkernel.h>

#include "WorldGenerator.h"
#include "LoadingScreen.h"
#include "LameFunctions.h"

#define PI 3.14159265f
#define DEG_TO_RAD (PI / 180.0f)
#define TEXTURE_CHUNKS 8
#define SKY_MOVE 0.003f

#define WORLD_SIZE 256
#define WORLD_HEIGHT 112
#define CHUNK_SIZE 16

#define default_size 0.5
#define default_big_size 0.687

// How long a single chat-button press waits for a second one. The chat button (D-pad Up by
// default) doubles as the third-person toggle, and the second tap has to be recognised before
// chat opens, because the on-screen keyboard takes every button once it is up. Comfortably long
// enough for a deliberate double tap, and the only cost of a single press is this much delay
// before the keyboard appears.
#define CHAT_DOUBLE_TAP_WINDOW 0.3f

// Third person. The boom length is walked in whole steps from the eye towards the back of the
// player and stops at the last step that is still clear, which is what keeps the camera outside
// walls without a proper swept raycast. NEVER_BELOW is the length used when even the first step is
// blocked -- see the placement call for why it is not simply zero.
#define THIRD_PERSON_DISTANCE 3.5f
#define THIRD_PERSON_NEVER_BELOW 1.1f
#define THIRD_PERSON_STEPS 12

// How far the head may be turned away from the body before the body is dragged round after it, in
// degrees. Java's EntityLivingBase.updateDistance() clamps the head-to-body angle to this, which is
// what gives the pivot-on-the-spot feel: inside the limit only the head moves, at the limit the body
// comes round too.
#define BODY_MAX_HEAD_TURN 75.0f

// Sprinting. Java: walking 0.1 (the speed stat, not the exhaustion figure below), sprinting 0.13, so
// a sprint is 1.3x. The FOV widens 15% while sprinting (EntityRenderer), which on a 480x272 screen is
// most of what makes a sprint feel like one. The window is Java's 7-tick double-tap window; the 0.04
// is the same "skip the first frames so a click bounce is not a second tap" guard the creative
// sprint-fly uses.
#define SPRINT_SPEED_MULT 1.3f
#define SPRINT_FOV_MULT 0.15f
#define SPRINT_TAP_WINDOW 0.35f
#define SPRINT_TAP_MIN_GAP 0.04f

// Camera bob is *not* scaled or suppressed in third person. It was tempting -- the dip is authored at
// the eye, and in third person the nearest thing on screen is the player one boom length away, so the
// same world-space dip reads as the model hopping against the ground more than as scenery sway. But
// Java applies view bobbing identically in every perspective (setupViewBobbing has no thirdPersonView
// test, only the View Bobbing option), so scaling it here would be a deviation, not a correction. The
// correction was in the bob itself: cameraYaw is speed-proportional and ground-gated, so a walk is the
// only thing that bobs at full strength, and the amplitude is Java's.

// Hunger exhaustion. These are Java's own numbers, from EntityPlayer.addMovementStat / jump /
// attackTargetEntityWithCurrentItem, and they are per *metre* wherever a distance is involved -- which
// is the whole point: a sprinted metre costs ten times a walked one, so the bar empties about ten
// times faster for the same distance. Exhaustion accumulates and CraftWorld::AddExhaustion trades
// every full 4.0 in for a hunger point, so 400 walked metres or 40 sprinted metres is one point.
#define EXHAUST_WALK 0.01f      // per metre, also the swimming and sneaking rate in 1.7
#define EXHAUST_SPRINT 0.1f     // per metre
#define EXHAUST_SWIM 0.015f     // per metre, Java's higher water rate
#define EXHAUST_JUMP 0.05f      // per jump
#define EXHAUST_SPRINT_JUMP 0.2f// per jump while sprinting, so bunny-hopping is not free
#define EXHAUST_ATTACK 0.1f     // per swing that lands
#define EXHAUST_DAMAGE 0.1f     // per hit taken

// Java's MathHelper.wrapAngleTo180_float and the 0..360 form, for the head/body follow below.
static float WrapAngle180(float angle)
{
    while(angle >= 180.0f)
        angle -= 360.0f;
    while(angle < -180.0f)
        angle += 360.0f;
    return angle;
}

static float WrapAngle360(float angle)
{
    while(angle >= 360.0f)
        angle -= 360.0f;
    while(angle < 0.0f)
        angle += 360.0f;
    return angle;
}

#define ENGLISH 1

static int PauseOptionsGridMove(int pos, int dcol, int drow)
{
    int col = (pos >= 8) ? 1 : 0;
    int row = pos & 7;
    col = (col + dcol) & 1;
    row = (row + drow + 8) & 7;
    return col * 8 + row;
}

static bool PauseOptionsIsSlider(int pos)
{
    return pos == 0 || pos == 1 || pos == 8 || pos == 9;
}

static bool AdjustPauseOptionsSlider(CraftWorld* world, Camera* cam, SkyLight* sky, SkyLight* moon, int pos, int dir)
{
    if(world == NULL)
        return false;
    if(dir > 0)
        dir = 1;
    else if(dir < 0)
        dir = -1;
    else
        return false;

    if(pos == 0)
    {
        int fov = world->mainOptions.fov + dir * 5;
        if(fov < 40 || fov > 100)
            return false;
        world->mainOptions.fov = fov;
        RenderManager::InstancePtr()->fovv = world->mainOptions.fov;
        RenderManager::InstancePtr()->SetPerspective(0, 480.0f / 272.0f, 0.18f, 1000.f);
        if(sky != NULL)
            sky->UpdateLightSource(world->skyTime);
        if(moon != NULL)
            moon->UpdateLightSource(world->skyTime);
        return true;
    }
    if(pos == 1)
    {
        int v = world->mainOptions.horizontalViewDistance + dir;
        if(v < 1 || v > 3)
            return false;
        world->mainOptions.horizontalViewDistance = v;
        world->playerZoneSize = Vector3(CHUNK_SIZE * v, CHUNK_SIZE * world->mainOptions.verticalViewDistance, CHUNK_SIZE * v);
        if(cam != NULL)
        {
            world->UpdatePlayerZoneBB(cam->m_vPosition);
            cam->needUpdate = true;
        }
        return true;
    }
    if(pos == 8)
    {
        int v = world->mainOptions.verticalViewDistance + dir;
        if(v < 1 || v > 3)
            return false;
        world->mainOptions.verticalViewDistance = v;
        world->playerZoneSize = Vector3(CHUNK_SIZE * world->mainOptions.horizontalViewDistance, CHUNK_SIZE * v, CHUNK_SIZE * world->mainOptions.horizontalViewDistance);
        if(cam != NULL)
        {
            world->UpdatePlayerZoneBB(cam->m_vPosition);
            cam->needUpdate = true;
        }
        return true;
    }
    if(pos == 9)
    {
        int d = world->mainOptions.difficult + dir;
        if(d < 0 || d > 3)
            return false;
        world->mainOptions.difficult = d;
        return true;
    }
    return false;
}

StatePlay::StatePlay()
{
    portalFadeTex = -1;
    time12 = 0.0f;
    mRender = NULL;
    mSystemMgr = NULL;
    fppCam = NULL;
    UseChest = NULL;
    UseFurnace = NULL;

    temp_total_time_ms = 0;

    statisticsPage = 0;

    freeMemory = 30;
    freeMemoryTimer = 0.0f;

    ram1 = 0;
    ram2 = 0;
    dt = 0.0f;

    showCube = true;

    GRAVITY = -6.8f;
    JUMPVELOCITY = 4.6f;
    CLIMBVELOCITY = 2.5f;

    moveForward = false;
    walkingOnGround = false;
    jumping = false;
    sneaking = false;
    moving = false;
    headInWater = false;
    footInWater = false;
    headInLava = false;
    footInLava = false;
    touchingCactus = false;

    invEn = false;
    craft3xEn = false;
    upEn = false;
    chestEn = false;
    furnaceEn = false;
    armorEn = false;

    thirdPersonView = 0;
    chatTapTimer = 0.0f;
    chatTapPending = false;
    bodyYaw = 270.0f;      // the camera's own default heading; InitCamera re-reads it
    headYawOffset = 0.0f;
    bobCamera = 0.0f;
    sprinting = false;
    sprintTapTimer = 0.0f;
    sprintTapArm = false;
    sprintTapPending = false;
    sprintFwdWasHeld = false;
    sprintFovBoost = 0.0f;

    cycle = 0;
    dtt = 0.0f;

    shift_x = 0.0f;
    shift_y = 0.0f;

    musicTimeGo = 0;

    chunkId = 0;
    chunks = 0;

    angleFactor = 0.0f;

    startDt = false;
    dET = 0;
    dT = 0;
    dStd = -1;
    tryToDt = false;
    pendingCrackId = -1;
    pendingCrackX = 0.0f;
    pendingCrackY = 0.0f;
    pendingCrackZ = 0.0f;
    pendingBreakStage = 0;
    pendingBreakWasLight = false;
    pendingBreakChunk = -1;

    testPos1.x = -1;
    testPos1.y = -1;
    testPos1.z = -1;

    barPosition = 0;
    invXPosition = 0;
    invYPosition = 0;

    craftItemId = -1;
    craftItemAm = -1;
    craftItemSt = 0;

    craftItemId3 = -1;
    craftItemAm3 = -1;
    craftItemSt3 = 0;

    chestId = -1;
    furnaceId = -1;

    cameraSpeed = 2.0f / 60.0f;
    cameraMoveSpeed = 0.0f;

    menuState = 0;
    selectPos = 0;

    // The debug stick's menu must never survive a world load. Leaving it up would freeze the new
    // world on its first frame with a menu pointing at a mob from the old one.
    debugMenu.Close();
    debugTarget = NULL;

    cloudsOffset = 0.0f;

    devMode = false;
    makeScreen = false;

	canHeadBob = true;
	invSteveAngle = 0.0f;
	bobCycle = 0.0f;
	bobWalk = 0.0f;
	camFollowY = 0.0f;
	camFollowYInit = false;
	lookDt = 1.0f / 60.0f;
	changeY = 0.0f;
	changeScale = 0.0f;

    slotForChangeScale = -1;

	anim[0] = 0;
	anim[1] = 0;
	anim[2] = 0;

	hpAnim = 0.0f;

	canFly = false;
	flyJumpTimer = 0.0f;
	flyJumpWasHeld = false;

	for(int i = 0; i <= 3; i += 1)
    {
        craftSlotId[i] = -1;
        craftSlotAm[i] = -1;
        craftSlotSt[i] = 0;
    }

    for(int i = 0; i <= 8; i += 1)
    {
        craftSlotId3[i] = -1;
        craftSlotAm3[i] = -1;
        craftSlotSt3[i] = 0;
    }

    tickHunger = 0.8f;
    tickHealth = 0.0f;
    tickCave = 0.0f;
    tickChunk = 0.0f;
    tickOS = 0.4f;
    tickShowSlotName = 0.0f;
    tickCactusDamage = 0.0f;
    tickLavaDamage = 0.6f;
    furnaceTimes = 0.0f;
    tutorialTimer = -1.0f;

    rainSoundFrequency = 1.7f;

    inputDiskNameTimer = 0.0f;
    inputDiskName = "";

    hurt = false;
    hurt_time = 0;
    musicTime = 1;

    dieFactor = true;

    animDest = 0.0f;
    animSpeed = 1.0f;
    animGo = false;

    bowCharging = false;
    bowCharge = 0.0f;

    cubeLight = 0.0f;

    sleepMessageTime = 0.0f;
    sleepMessageType = 0;
    sleepTime = 0.0f;
    loadReady = false;

    pre_fps = 0.0f;
    average_fps = 0;
    tick_fps = 0.0f;
    ticks = 0;

    last_HP = 0;
}

StatePlay::~StatePlay()
{
}

void StatePlay::InitCamera()
{
    fppCam = new Camera();
    fppCam->PositionCamera(playerPosition.x,playerPosition.y,playerPosition.z, playerPosition.x,playerPosition.y,playerPosition.z-5.0f, 0.0f,1.0f,0.0f);
    mRender->SetActiveCamera(fppCam);

    // The third-person body starts aligned with wherever the camera is pointing, so loading a save
    // does not snap the model round on the first frame.
    bodyYaw = fppCam->horAngle;
    headYawOffset = 0.0f;

    mWorld->UpdatePlayerZoneBB(fppCam->m_vPosition);
    mWorld->FetchNearestChunks();
}

void StatePlay::Init()
{
    // Free the title-screen art here rather than in Enter(). The nether runs out of heap inside
    // buildMap() and the first mesh pass -- which happen below, during InitParametric -- and Enter()
    // is not called until the world is pushed, i.e. after all of it. Releasing there would arrive too
    // late to be the reason the mesh build fits. The menu is not drawn between here and the handover
    // (the loading screen is, and it uses Dirt/Load, which are not on the release list), and the
    // menu's own sprites are index-based, so they survive untouched and StateMenu::Resume refills
    // them.
    TextureHelper::Instance()->ReleaseMenuTextures();

    //set render manager instance
    mRender = RenderManager::InstancePtr();
    mSystemMgr = SystemManager::Instance();
    mSoundMgr = SoundManager::Instance();
    mIhelper = InputHelper::Instance();

    WorldGenerator *mGen = new WorldGenerator();

    //then create our perfect world
    mWorld = new CraftWorld();
    mWorld->initWorld(WORLD_SIZE, WORLD_HEIGHT, CHUNK_SIZE);
    delete mGen;
    //mWorld->initRandompMap(128,16);
    mWorld->setTextureSize(256,16,64,16,256,16);
    mWorld->initWorldBlocksLight();
    mWorld->SetWolrdTime(10);
    mWorld->UpdatePlayerZoneBB(playerPosition);
    mWorld->buildMap();

    playerPosition = newPlayerPos = oldPlayerPos = Vector3(64.0f,mWorld->groundHeight(0,0)+0,0);

    // (curchunkTarget removed: computed from getChunkId and never read)

    dt = mTimer.GetDeltaTime();

    bobCycle = 3.14/2;
    cubeLight = 0.0f;

    LoadTextures();
    mWorld->GetTexturesIds();
    mWorld->buildblocksVerts();

    menuOptions = false;
    optionsMenuPos = 0;

    analogLeft = analogRight = analogUp = analogDown = false;

    walkSoundAccu = 0.0f;
    isWalking = false;
    loadReady = false;
}

void StatePlay::InitParametric(bool makeTrees,bool makeWater,bool makeCaves,unsigned int seed_1, int worldType, char gameMode)
{
    // Before the world, not after it. This function builds the map and its meshes itself (see
    // buildMap below), so a release in Init()/Enter() would run after the allocation that runs out
    // of heap -- which is the whole complaint this is addressing. The menu is not drawn between here
    // and the handover; the loading screen is, and it uses Dirt/Load, which are deliberately not on
    // the release list.
    MC_DEBUG_LOG("stateplay: releasing the menu art BEFORE the world build\n");
    TextureHelper::Instance()->ReleaseMenuTextures();
    MC_DEBUG_LOG("stateplay: menu art released, continuing into the world build\n");

    //set render manager instance
    mRender = RenderManager::InstancePtr();
    mSystemMgr = SystemManager::Instance();
    mSoundMgr = SoundManager::Instance();
    mIhelper = InputHelper::Instance();

    //then create our perfect world
    mWorld = new CraftWorld();
    mWorld->initWorld(WORLD_SIZE, WORLD_HEIGHT, CHUNK_SIZE);
    mWorld->gameModeWorld = gameMode;

    mWorld->worldGen = new WorldGenerator();
    mWorld->worldGen->initRandompMap(WORLD_SIZE, WORLD_HEIGHT, CHUNK_SIZE, mWorld, makeTrees, makeWater, makeCaves, seed_1, worldType);
    //mWorld->initRandompMap(128,16,terrainType,makeFlat,makeTrees,makeWater,makeCaves);
    mWorld->setTextureSize(256,16,64,16,256,16);
    mWorld->SetWolrdTime(10);
    playerPosition = newPlayerPos = oldPlayerPos = mWorld->playerSpawnPointPosition;
    mWorld->UpdatePlayerZoneBB(playerPosition);
    mWorld->buildMap();
    MC_DebugHeapCheckpoint("play map built");

    // (curchunkTarget removed: computed from getChunkId and never read)

    dt = mTimer.GetDeltaTime();

    LoadTextures();
    TextureManager::Instance()->LogTextureUsage("at world entry");

    mWorld->GetTexturesIds();
    mWorld->buildblocksVerts();
    mWorld->buildcloudsVerts();
    mWorld->buildmobVerts();
    MC_DebugHeapCheckpoint("play mesh built");

    menuOptions = false;
    optionsMenuPos = 0;

    analogLeft = analogRight = analogUp = analogDown = false;

    walkSoundAccu = 0.0f;
    isWalking = false;

    cubeLight = 0.0f;
    bobCycle = PI/2.0f;
    loadReady = true;

    mWorld->haveCompass = mWorld->HaveItemInBarSlots(Compass::getID());

#if MC_PERF_PROBE
    PerfProbeSelfTest();
#endif
}

#if MC_TEST_ENV
void StatePlay::ApplyTestEnvironment(int weather, int hour)
{
    if(mWorld == NULL || mWorld->worldWeather == NULL)
        return;

    // Longer than any run, so the weather cannot turn on its own.
    mWorld->worldWeather->SetWeatherType(weather);
    mWorld->worldWeather->SetWeatherDuration(1000000.0f);
    if(hour >= 0)
    {
        mWorld->SetWolrdTime((float)(hour % 24));
        mWorld->SetAllChunksToUpdate();
    }
    Aurora::Utils::Logger::Instance()->LogMessage("testenv: weather %d, hour %d\n", weather, hour);
}

void StatePlay::SpawnTestMobInFront()
{
    const char* name = MC_TEST_MOB_STR(MC_TEST_MOB);
    if(name[0] == 0 || mWorld == NULL)
        return;

    // Default camera looks toward -Z. Plant it in that view, on the ground.
    float x = playerPosition.x;
    float z = playerPosition.z - 3.5f;
    int gx = (int)floorf(x);
    int gz = (int)floorf(z);
    int gy = mWorld->groundHeightWater(gx, gz);
    if(gy <= 0)
        gy = (int)playerPosition.y;
    float y = (float)gy + 1.8f;

    LameMob* mob = mWorld->SpawnMobByName(name, x, y, z);
    if(mob == NULL)
    {
        Aurora::Utils::Logger::Instance()->LogMessage("testenv: unknown MC_TEST_MOB '%s'\n", name);
        return;
    }
    mob->poseLock = true;
    mob->mainAngle = ATAN2_fast(playerPosition.x - x, playerPosition.z - z);
    Aurora::Utils::Logger::Instance()->LogMessage("testenv: still %s at %.1f,%.1f,%.1f\n", name, x, y, z);
}
#endif

void StatePlay::SetWorldAndSaveName(std::string worldName,std::string fileName)
{
    if(mWorld != NULL)
    {
        sprintf(mWorld->worldName,"%s",worldName.c_str());
        mWorld->SetWorldSavePath(fileName);
    }
    saveFileName = fileName;
}

void StatePlay::LoadMap(std::string fileName,bool compressed)
{
    // As in InitParametric: this is a second world-build path, and the release has to precede it.
    TextureHelper::Instance()->ReleaseMenuTextures();

    //enter loading screen
    LoadingScreen* loading = new LoadingScreen();

    loading->readiness = 0;
    loading->stateName = 0;

    //set render manager instance
    mRender = RenderManager::InstancePtr();
    mSystemMgr = SystemManager::Instance();
    mSoundMgr = SoundManager::Instance();
    mIhelper = InputHelper::Instance();

    //save name
    saveFileName = fileName;
    //set start position
    playerPosition = newPlayerPos = oldPlayerPos = Vector3(0,0,0);

    //then create our perfect world
    mWorld = new CraftWorld();

    //load data
    if(compressed)
    {
        mWorld->LoadCompressedWorld(saveFileName);
        //set player pos from map
        playerPosition = mWorld->GetPlayerPos();
        newPlayerPos  = mWorld->GetPlayerPos();
        oldPlayerPos = mWorld->GetPlayerPos();
        mWorld->SetWolrdTime(mWorld->worldDayTime);
    }
    else
    {
        mWorld->LoadWorld(saveFileName.c_str());
        mWorld->SetWolrdTime(6);
    }
    //player zone size and bbZone
    mWorld->UpdatePlayerZoneBB(playerPosition);
    //create chunks
    mWorld->setTextureSize(256,16,64,16,256,16);
    mWorld->initWorldBlocksLight();
    mWorld->buildMap();

    // (curchunkTarget removed: computed from getChunkId and never read)
    dt = mTimer.GetDeltaTime();
    // load textures
    LoadTextures();
    mWorld->GetTexturesIds();

    // build custom mesh
    mWorld->buildblocksVerts();
    mWorld->buildcloudsVerts();
    mWorld->buildmobVerts();

    menuOptions = false;
    optionsMenuPos = 0;

    analogLeft = analogRight = analogUp = analogDown = false;

    walkSoundAccu = 0.0f;
    isWalking = false;

    mSoundMgr->playerSounds = mWorld->mainOptions.sounds;
    mWorld->playerZoneSize = Vector3(CHUNK_SIZE*mWorld->mainOptions.horizontalViewDistance,CHUNK_SIZE*mWorld->mainOptions.verticalViewDistance,CHUNK_SIZE*mWorld->mainOptions.horizontalViewDistance);
    mRender->fovv = mWorld->mainOptions.fov;
    RenderManager::InstancePtr()->SetPerspective(0, 480.0f / 272.0f, 0.18f, 1000.f);

    loadReady = true;

    mWorld->haveCompass = mWorld->HaveItemInBarSlots(Compass::getID());

    // cancel loading screen
    loading->KillLoadingScreen();
    delete loading;
}

void StatePlay::LoadTextures()
{
    // The menu equips a skin into the Steve slot. Doing it again here covers a boot that ran
    // before the save folder was the working directory, and a texture pack that reloaded Steve.
    TextureHelper::Instance()->ApplySavedSkin();

    //terrain texure
    texture = TextureHelper::Instance()->GetTexture(TextureHelper::Terrain1);
	Xpbar = TextureHelper::Instance()->GetTexture(TextureHelper::Xpbar);
    texture_mips = TextureHelper::Instance()->GetTexture(TextureHelper::Terrain2);
    waterAnimation = TextureHelper::Instance()->GetTexture(TextureHelper::WaterAnimation);
    Particle::PrecacheTextures(mWorld);

    invPlayerTex = TextureHelper::Instance()->GetTexture(TextureHelper::Steve);
    cloudsTex = TextureHelper::Instance()->GetTexture(TextureHelper::clouds1);
    zombieTex = TextureHelper::Instance()->GetTexture(TextureHelper::zombieTexture);
    cowTex = TextureHelper::Instance()->GetTexture(TextureHelper::cowTexture);

    blue = TextureHelper::Instance()->GetTexture(TextureHelper::Blue);
    red = TextureHelper::Instance()->GetTexture(TextureHelper::Red);
    black = TextureHelper::Instance()->GetTexture(TextureHelper::Black);

    snowBall4 = TextureHelper::Instance()->GetTexture(TextureHelper::SnowBall3);

    suntex = TextureHelper::Instance()->GetTexture(TextureHelper::Sun);
    moontex = TextureHelper::Instance()->GetTexture(TextureHelper::Moon);
    stars = TextureHelper::Instance()->GetTexture(TextureHelper::Stars);
    rainTex  = TextureHelper::Instance()->GetTexture(TextureHelper::rainTexture);

    barItems = TextureHelper::Instance()->GetTexture(TextureHelper::Items1);

    pumpkinMask = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::PumpkinMask));
    pumpkinMask->SetPosition(240,136);
    pumpkinMask->Scale(2.87f,2.84f);

    waterScreen = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Blue));
    waterScreen->SetPosition(240,136);
    waterScreen->Scale(30,17);

    // hud section
    int utilsSize = TextureManager::Instance()->getWidth(TextureHelper::Instance()->GetTexture(TextureHelper::Utils));
    float utilScale = 264.0f/(float)utilsSize;

    barSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),0*utilsSize/182,0*utilsSize/182,182*utilsSize/182,22*utilsSize/182);
    barSprite->SetPosition(240,230);
    barSprite->Scale(utilScale,utilScale);

    selectSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),0*utilsSize/182,22*utilsSize/182,24*utilsSize/182,24*utilsSize/182);
    selectSprite->SetPosition(108.0f + 11.0f * (264.0f / 182.0f),230);
    selectSprite->Scale(utilScale,utilScale);

    crossSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),24*utilsSize/182,22*utilsSize/182,9*utilsSize/182,9*utilsSize/182);
    crossSprite->SetPosition(240,136);
    crossSprite->Scale(utilScale,utilScale);

    hpCellSpriteW = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),60*utilsSize/182,36*utilsSize/182,82*utilsSize/182,10*utilsSize/182);
    hpCellSpriteW->SetPosition(100,29);
    hpCellSpriteW->Scale(utilScale,utilScale);

    hpCellSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),60*utilsSize/182,46*utilsSize/182,82*utilsSize/182,10*utilsSize/182);
    hpCellSprite->SetPosition(100,21);
    hpCellSprite->Scale(utilScale,utilScale);

    hgCellSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),60*utilsSize/182,56*utilsSize/182,82*utilsSize/182,10*utilsSize/182);
    hgCellSprite->SetPosition(100,24);
    hgCellSprite->Scale(utilScale,utilScale);

    arCellSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),60*utilsSize/182,66*utilsSize/182,82*utilsSize/182,10*utilsSize/182);
    arCellSprite->SetPosition(100,27);
    arCellSprite->Scale(utilScale,utilScale);

    hpSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),0*utilsSize/182,66*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hpSprite->SetPosition(100,22);
    hpSprite->Scale(utilScale,utilScale);

    hpSpriteW = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),20*utilsSize/182,66*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hpSpriteW->SetPosition(100,22);
    hpSpriteW->Scale(utilScale,utilScale);

    hpHardSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),0*utilsSize/182,56*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hpHardSprite->SetPosition(100,22);
    hpHardSprite->Scale(utilScale,utilScale);

    hpHardSpriteW = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),20*utilsSize/182,56*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hpHardSpriteW->SetPosition(100,22);
    hpHardSpriteW->Scale(utilScale,utilScale);

    hpHalfSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),10*utilsSize/182,66*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hpHalfSprite->SetPosition(100,22);
    hpHalfSprite->Scale(utilScale,utilScale);

    hpHalfSpriteW = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),30*utilsSize/182,66*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hpHalfSpriteW->SetPosition(100,22);
    hpHalfSpriteW->Scale(utilScale,utilScale);

    hpHardHalfSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),10*utilsSize/182,56*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hpHardHalfSprite->SetPosition(100,22);
    hpHardHalfSprite->Scale(utilScale,utilScale);

    hpHardHalfSpriteW = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),30*utilsSize/182,56*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hpHardHalfSpriteW->SetPosition(100,22);
    hpHardHalfSpriteW->Scale(utilScale,utilScale);

    hgSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),30*utilsSize/182,46*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hgSprite->SetPosition(100,26);
    hgSprite->Scale(utilScale,utilScale);

    hgHalfSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),40*utilsSize/182,46*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    hgHalfSprite->SetPosition(100,26);
    hgHalfSprite->Scale(utilScale,utilScale);

    bubbleSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),0*utilsSize/182,46*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    bubbleSprite->SetPosition(100,27);
    bubbleSprite->Scale(utilScale,utilScale);

    arSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),10*utilsSize/182,46*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    arSprite->SetPosition(100,27);
    arSprite->Scale(utilScale,utilScale);

    arHalfSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),20*utilsSize/182,46*utilsSize/182,10*utilsSize/182,10*utilsSize/182);
    arHalfSprite->SetPosition(100,27);
    arHalfSprite->Scale(utilScale,utilScale);

    for(int j = 13; j >= 0; j--)
    {
        toolPointSprite[j] = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),169*utilsSize/182,(49+(13-j)*2)*utilsSize/182,13*utilsSize/182,2*utilsSize/182,true);//200
        toolPointSprite[j]->SetPosition(240,136);
        toolPointSprite[j]->Scale(utilScale,utilScale);
    }

    //menu section
    buttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,0,95,12); // stand
    buttonSprite->SetPosition(240,150);
    buttonSprite->Scale(2,2);

    sbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,12,95,12); // stand selected
    sbuttonSprite->SetPosition(240,150);
    sbuttonSprite->Scale(2,2);

    nbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,24,95,12); // dark
    nbuttonSprite->SetPosition(240,150);
    nbuttonSprite->Scale(2,2);

	moverSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,60,6,12);
	moverSprite->Scale(2,2);

    // inventory section

    int invTex = TextureHelper::Instance()->GetTexture(TextureHelper::inv);
    guiInvW = TextureManager::Instance()->getWidth(invTex);
    guiInvH = (float)TextureManager::Instance()->getHeight(invTex);
    guiScale = 350.0f / guiInvW;

    int crtTex = TextureHelper::Instance()->GetTexture(TextureHelper::crt);
    guiCrtW = TextureManager::Instance()->getWidth(crtTex);
    guiCrtH = (float)TextureManager::Instance()->getHeight(crtTex);

    int chtTex = TextureHelper::Instance()->GetTexture(TextureHelper::cht);
    guiChtW = TextureManager::Instance()->getWidth(chtTex);
    guiChtH = (float)TextureManager::Instance()->getHeight(chtTex);

    int furTex = TextureHelper::Instance()->GetTexture(TextureHelper::fur);
    guiFurW = TextureManager::Instance()->getWidth(furTex);
    guiFurH = (float)TextureManager::Instance()->getHeight(furTex);

    invSprite = new Sprite(invTex);//200
    invSprite->SetPosition(240,136);
    invSprite->Scale(guiScale,guiScale);

    crtSprite = new Sprite(crtTex);//200
    crtSprite->SetPosition(240,136);
    crtSprite->Scale(guiScale,guiScale);

    chtSprite = new Sprite(chtTex);//200
    chtSprite->SetPosition(240,136);
    chtSprite->Scale(guiScale,guiScale);

    furSprite = new Sprite(furTex);//200
    furSprite->SetPosition(240,136);
    furSprite->Scale(guiScale,guiScale);

    int invArrowSize = TextureManager::Instance()->getWidth(TextureHelper::Instance()->GetTexture(TextureHelper::furArrow));
    float invArrowScale = 44.0f/(float)invArrowSize;

    for(int j = 0; j <= 21; j++)
    {
        furArrowSprite[j] = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::furArrow),0,0,(1*j)*invArrowSize/22.0f,16*invArrowSize/22.0f,true);//200
        furArrowSprite[j]->SetPosition(240,136);
        furArrowSprite[j]->Scale(invArrowScale,invArrowScale);
    }

    int invFireSize = TextureManager::Instance()->getWidth(TextureHelper::Instance()->GetTexture(TextureHelper::furFire));
    float invFireScale = 28.0f/(float)invFireSize;

    for(int j = 0; j <= 13; j++)
    {
        furFireSprite[13-j] = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::furFire),0,j*invFireSize/14.0f,14*invFireSize/14.0f,(14-j)*invFireSize/14.0f,true);//200
        furFireSprite[13-j]->SetPosition(240,136);
        furFireSprite[13-j]->Scale(invFireScale,invFireScale);
    }

    selectInvSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::cursor));//200
    UpdateInvCursorPos();

    invCellSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::invCell));//200
    invCellSprite->SetPosition(240,136);
    invCellSprite->Scale(guiScale,guiScale);

    skyLight = new SkyLight();
    skyMoonLight = new SkyLight();
    skyBox = new SkyBox();
    destroyer = new Destroyer();
    starsBox = new StarsBox();
    invPlayer = new InventoryPlayer();
}



void StatePlay::SetDayTimeAfterLoad()
{

}

void StatePlay::Enter()
{
    RenderManager::InstancePtr()->SetPerspective(55.0f, 480.0f / 272.0f, 0.18f, 1000.0f);//1000

    // No frame drawn from here until StateMenu::Resume samples the title-screen art, and the nether
    // needs the heap it is sitting on. Reloaded on the way back (see TextureHelper).
    TextureHelper::Instance()->ReleaseMenuTextures();
}

void StatePlay::CleanUp()
{
    if(mSnowBalls.empty() == false)
    {
        for(unsigned int i = 0; i < mSnowBalls.size(); i++)
        {
            delete mSnowBalls[i];
        }
        mSnowBalls.clear();
    }

    delete mRender->mCam;
    mRender->mCam = NULL;

    delete invCellSprite;
    delete buttonSprite;
    delete sbuttonSprite;
    delete nbuttonSprite;
    delete moverSprite;
    delete barSprite;
    delete selectSprite;
    delete crossSprite;
    delete invSprite;
    delete crtSprite;
    delete chtSprite;
    delete selectInvSprite;

    delete skyLight;
    delete skyMoonLight;
    delete skyBox;
    delete destroyer;
    delete starsBox;
    delete invPlayer;

    delete furSprite;
    delete waterScreen;
    delete pumpkinMask;

    for(unsigned int i = 0; i <= 13; i++)
    {
        delete furFireSprite[i];
    }

    for(unsigned int i = 0; i <= 21; i++)
    {
        delete furArrowSprite[i];
    }

    for(unsigned int i = 0; i <= 13; i++)
    {
        delete toolPointSprite[i];
    }

    delete hpCellSprite;
    delete hpSprite;
    delete hpHardSprite;
    delete hpHalfSprite;
    delete hpHardHalfSprite;
    delete hpCellSpriteW;
    delete hpSpriteW;
    delete hpHardSpriteW;
    delete hpHalfSpriteW;
    delete hpHardHalfSpriteW;

    delete hgCellSprite;
    delete hgSprite;
    delete hgHalfSprite;

    delete arSprite;
    delete arHalfSprite;
    delete arCellSprite;

    delete bubbleSprite;

    delete mWorld;
}

void StatePlay::Pause()
{

}

void StatePlay::Resume()
{

}

int StatePlay::FindChestId(int x, int y, int z)
{
    int o = -1;
    for(unsigned int i = 0; i < mWorld->mChests.size(); i++)
    {
        Chest* NewChest = mWorld->mChests[i];
        if(NewChest->chestX == x && NewChest->chestY == y && NewChest->chestZ == z)
        {
            o = i;
            break;
        }
    }

    return o;
}

int StatePlay::FindFurnaceId(int x, int y, int z)
{
    int o;
    o = -1;
    for(unsigned int i = 0; i < mWorld->mFurnaces.size(); i++)
    {
        Furnace* NewFurnace = mWorld->mFurnaces[i];
        if(NewFurnace->furnaceX == x && NewFurnace->furnaceY == y && NewFurnace->furnaceZ == z)
        {
            o = i;
            break;
        }
    }

    return o;
}

void StatePlay::CheckForFurnFuel(Furnace* Fur)
{
    int furnItem;

    furnItem = -1;

    if(IsBlockId(Fur->furnaceSlotId[0]) && Fur->furnaceSlotId[0] != -1)
    {
        furnItem = mWorld->blockTypes[Fur->furnaceSlotId[0]].furnItem;
    }

    if(IsItemId(Fur->furnaceSlotId[0]))
    {
        furnItem = mWorld->itemTypes[Fur->furnaceSlotId[0]-250].furnItem;
    }

    if(Fur->fuelTime <= 0 && furnItem != -1)
    {
        if(Fur->furnaceSlotId[1] == -1)
            return;

        bool used = false;

        switch(Fur->furnaceSlotId[1])
        {
        case 8:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 31:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 34:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 296:
            Fur->fuelTime = 5;
            used = true;
        break;
        case 276:
            Fur->fuelTime = 5;
            used = true;
        break;
        case 277:
            Fur->fuelTime = 80;
            used = true;
        break;
        case 292:
            Fur->fuelTime = 1000;
            used = true;
        break;
        case 306:
            Fur->fuelTime = 30;
            used = true;
        break;
        case 59:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 71:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 72:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 100:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 105:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 125:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 155:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 156:
            Fur->fuelTime = 15;
            used = true;
        break;
        case 214:
            Fur->fuelTime = 800;
            used = true;
        break;
        case 250:
            Fur->fuelTime = 10;
            used = true;
        break;
        case 255:
            Fur->fuelTime = 10;
            used = true;
        break;
        case 260:
            Fur->fuelTime = 10;
            used = true;
        break;
        case 265:
            Fur->fuelTime = 10;
            used = true;
        break;
        case 270:
            Fur->fuelTime = 10;
            used = true;
        break;
        }

        if(used == true)
        {
            Fur->furnaceSlotAm[1] -= 1;
            if(Fur->furnaceSlotAm[1] == 0)
            {
                Fur->furnaceSlotAm[1] = -1;
                Fur->furnaceSlotId[1] = -1;
                Fur->furnaceSlotSt[1] = 0;
            }
        }
        else
        {
            Fur->meltingTime = 0.0f;
        }
    }
}

void StatePlay::CheckForFurnWorking(Furnace* Fur)
{
    int furnItem;

    furnItem = -1;

    if(IsBlockId(Fur->furnaceSlotId[0]) && Fur->furnaceSlotId[0] != -1)
    {
        furnItem = mWorld->blockTypes[Fur->furnaceSlotId[0]].furnItem;
    }

    if(IsItemId(Fur->furnaceSlotId[0]))
    {
        furnItem = mWorld->itemTypes[Fur->furnaceSlotId[0]-250].furnItem;
    }

    if(furnItem != -1)
    {
        if(Fur->fuelTime > 0)
        {
            if(Fur->furnaceSlotId[2] == furnItem || Fur->furnaceSlotId[2] == -1)
            {
                if(Fur->furnaceSlotAm[2] <= 63)
                {
                    Fur->working = true;
                }
            }
            if(Fur->furnaceSlotId[2] != furnItem && Fur->furnaceSlotId[2] != -1)
            {
                Fur->working = false;
                Fur->meltingTime = 0;
            }
        }
        else
        {
            Fur->working = false;
            Fur->meltingTime = 0;
        }
        if(Fur->fuelTime == 0)
        {
            Fur->working = false;
            Fur->meltingTime = 0;
        }
    }
    else
    {
        Fur->working = false;
        Fur->meltingTime = 0;
    }
}

void StatePlay::ReadyFurnSmelting(Furnace* Fur)
{
    int furnItem;

    if(IsBlockId(Fur->furnaceSlotId[0]))
    {
        furnItem = mWorld->blockTypes[Fur->furnaceSlotId[0]].furnItem;
    }

    if(IsItemId(Fur->furnaceSlotId[0]))
    {
        furnItem = mWorld->itemTypes[Fur->furnaceSlotId[0]-250].furnItem;
    }

    if(furnItem != -1)
    {
        if(Fur->furnaceSlotId[2] == furnItem)
        {
           Fur->furnaceSlotAm[2] += 1;
        }
        if(Fur->furnaceSlotId[2] == -1)
        {
            Fur->furnaceSlotId[2] = furnItem;
            Fur->furnaceSlotSt[2] = true;
            Fur->furnaceSlotAm[2] = 1;
        }

        Fur->furnaceSlotAm[0] -= 1;
        if(Fur->furnaceSlotAm[0] <= 0)
        {
            Fur->furnaceSlotId[0] = -1;
            Fur->furnaceSlotAm[0] = -1;
            Fur->furnaceSlotSt[0] = 0;
        }
    }
}

void StatePlay::CraftItem2x2()
{
    craftItemId = -1;
    craftItemAm = -1;
    craftItemSt = 0;

    int result = 0;
    bool enableMainCraftBody = true;

    std::vector<int> itemsIDs;
    std::string materialString;

    for(int i = 0; i <= 3; i ++)
    {
        if(craftSlotId[i] != -1)
        {
            result += craftSlotId[i];
            itemsIDs.push_back(craftSlotId[i]);
        }

        if(craftSlotId[i] >= BlackWoolBlock::getID() && craftSlotId[i] <= WhiteWoolBlock::getID())
        {
            materialString += "W";
        }
        else if(craftSlotId[i] == OakPlanks::getID() || craftSlotId[i] == SprucePlanks::getID() || craftSlotId[i] == BirchPlanks::getID())
        {
            materialString += "P";
        }
        else
        {
            materialString += "0";
        }
    }

    // carpets
    if(materialString == "WW00" || materialString == "00WW")
    {
        if(itemsIDs.size() == 2)
        {
            if(itemsIDs[0] == itemsIDs[1])
            {
                craftItemId = itemsIDs[0]+188;
                craftItemSt = 1;
                craftItemAm = 3;

                enableMainCraftBody = false;
            }
        }
    }

    if(itemsIDs.size() == 2) // dyes and colored wool crafting
    {
        if((itemsIDs[0] == RoseRed::getID() && itemsIDs[1] == DandelionYellow::getID()) ||
           (itemsIDs[1] == RoseRed::getID() && itemsIDs[0] == DandelionYellow::getID()))
        {
            craftItemId = OrangeDye::getID();
            craftItemSt = 1;
            craftItemAm = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == RoseRed::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == RoseRed::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId = PinkDye::getID();
            craftItemSt = 1;
            craftItemAm = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == CactusGreen::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == CactusGreen::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId = LimeDye::getID();
            craftItemSt = 1;
            craftItemAm = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == LapisLazuli::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == LapisLazuli::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId = LightBlueDye::getID();
            craftItemSt = 1;
            craftItemAm = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == LapisLazuli::getID() && itemsIDs[1] == RoseRed::getID()) ||
           (itemsIDs[1] == LapisLazuli::getID() && itemsIDs[0] == RoseRed::getID()))
        {
            craftItemId = PurpleDye::getID();
            craftItemSt = 1;
            craftItemAm = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == PurpleDye::getID() && itemsIDs[1] == PinkDye::getID()) ||
           (itemsIDs[1] == PurpleDye::getID() && itemsIDs[0] == PinkDye::getID()))
        {
            craftItemId = MagnetaDye::getID();
            craftItemSt = 1;
            craftItemAm = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == LapisLazuli::getID() && itemsIDs[1] == RoseRed::getID()) ||
           (itemsIDs[1] == LapisLazuli::getID() && itemsIDs[0] == RoseRed::getID()))
        {
            craftItemId = PurpleDye::getID();
            craftItemSt = 1;
            craftItemAm = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == InkSac::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == InkSac::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId = GrayDye::getID();
            craftItemSt = 1;
            craftItemAm = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == GrayDye::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == GrayDye::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId = LightGrayDye::getID();
            craftItemSt = 1;
            craftItemAm = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == InkSac::getID() && itemsIDs[1] == OrangeDye::getID()) ||
           (itemsIDs[1] == InkSac::getID() && itemsIDs[0] == OrangeDye::getID()))
        {
            craftItemId = CocoaBeans::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }


        /// wool


        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == RoseRed::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == RoseRed::getID()))
        {
            craftItemId = RedWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == DandelionYellow::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == DandelionYellow::getID()))
        {
            craftItemId = YellowWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == OrangeDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == OrangeDye::getID()))
        {
            craftItemId = OrangeWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == CactusGreen::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == CactusGreen::getID()))
        {
            craftItemId = DarkGreenWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == LimeDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == LimeDye::getID()))
        {
            craftItemId = GreenWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == LapisLazuli::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == LapisLazuli::getID()))
        {
            craftItemId = BlueWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == LightBlueDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == LightBlueDye::getID()))
        {
            craftItemId = LightBlueWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == PurpleDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == PurpleDye::getID()))
        {
            craftItemId = VioletWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == MagnetaDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == MagnetaDye::getID()))
        {
            craftItemId = PastelWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == InkSac::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == InkSac::getID()))
        {
            craftItemId = BlackWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == GrayDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == GrayDye::getID()))
        {
            craftItemId = GrayWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == LightGrayDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == LightGrayDye::getID()))
        {
            craftItemId = LightGrayWoolBlock::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }
    }

    if(itemsIDs.size() == 3)
    {
        if(result == RoseRed::getID() + PinkDye::getID() + LapisLazuli::getID())
        {
            if(itemsIDs[0] == RoseRed::getID() || itemsIDs[1] == RoseRed::getID() || itemsIDs[2] == RoseRed::getID())
            {
                craftItemId = MagnetaDye::getID();
                craftItemSt = 1;
                craftItemAm = 3;

                enableMainCraftBody = false;
            }
        }
    }

    // redstone torch: a piece of redstone dust above a stick, the same vertical pair the plain torch
    // uses with coal. Ids are 353 (dust) over 276 (stick), and the 2x2 grid is column-major here, so
    // the pair is (0,2) or (1,3).
    if(itemsIDs.size() == 2)
    {
        if((craftSlotId[0] == Redstone::getID() && craftSlotId[2] == Stick::getID()) ||
           (craftSlotId[1] == Redstone::getID() && craftSlotId[3] == Stick::getID()))
        {
            craftItemId = RedstoneTorchItem::getID();
            craftItemSt = 1;
            craftItemAm = 1;

            enableMainCraftBody = false;
        }
    }

    itemsIDs.clear();

    if(enableMainCraftBody)
    {
        switch(result)
        {
        // wood to planks
        case 8:
            if(craftSlotId[0] == 8 || craftSlotId[1] == 8 || craftSlotId[2] == 8 || craftSlotId[3] == 8)
            {
                craftItemId = 34;
                craftItemSt = 1;
                craftItemAm = 4;
            }
        break;

        // birch wood to planks
        case 31:
            if(craftSlotId[0] == 31 || craftSlotId[1] == 31 || craftSlotId[2] == 31 || craftSlotId[3] == 31)
            {
                craftItemId = BirchPlanks::getID();
                craftItemSt = 1;
                craftItemAm = 4;
            }
        break;

        // spruce wood to planks
        case 30 :
            if(craftSlotId[0] == 30 || craftSlotId[1] == 30 || craftSlotId[2] == 30 || craftSlotId[3] == 30)
            {
                craftItemId = SprucePlanks::getID();
                craftItemSt = 1;
                craftItemAm = 4;
            }
        break;

        // bone to bone meals
        case 321:
            if(craftSlotId[0] == 321 || craftSlotId[1] == 321 || craftSlotId[2] == 321 || craftSlotId[3] == 321)
            {
                craftItemId = 322;
                craftItemSt = 1;
                craftItemAm = 3;
            }
        break;

        // snowballs to snow block
        case 1196:
            if(craftSlotId[0] == 299 && craftSlotId[1] == 299 && craftSlotId[2] == 299 && craftSlotId[3] == 299)
            {
                craftItemId = 47;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;

        // melon to seeds
        case 311:
            if(craftSlotId[0] == 311 || craftSlotId[1] == 311 || craftSlotId[2] == 311 || craftSlotId[3] == 311)
            {
                craftItemId = 312;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;

        // melon to seeds
        case 293:
            if(craftSlotId[0] == 293 || craftSlotId[1] == 293 || craftSlotId[2] == 293 || craftSlotId[3] == 293)
            {
                craftItemId = 313;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;

        // diamond block to diamonds
        case 64:
            if(craftSlotId[0] == 64 || craftSlotId[1] == 64 || craftSlotId[2] == 64 || craftSlotId[3] == 64)
            {
                craftItemId = 279;
                craftItemSt = 1;
                craftItemAm = 9;
            }
        break;

        // iron block to bars
        case 66:
            if(craftSlotId[0] == 66 || craftSlotId[1] == 66 || craftSlotId[2] == 66 || craftSlotId[3] == 66)
            {
                craftItemId = 278;
                craftItemSt = 1;
                craftItemAm = 9;
            }
        break;

        // golden block to bars
        case 65:
            if(craftSlotId[0] == 65 || craftSlotId[1] == 65 || craftSlotId[2] == 65 || craftSlotId[3] == 65)
            {
                craftItemId = 280;
                craftItemSt = 1;
                craftItemAm = 9;
            }
        break;

        // wheat block to wheats
        case 213:
            if(craftSlotId[0] == HayBale::getID() || craftSlotId[1] == HayBale::getID() || craftSlotId[2] == HayBale::getID() || craftSlotId[3] == HayBale::getID())
            {
                craftItemId = Wheat::getID();
                craftItemSt = 1;
                craftItemAm = 9;
            }
        break;

        // coal block to coal items
        case 214:
            if(craftSlotId[0] == CoalBlock::getID() || craftSlotId[1] == CoalBlock::getID() || craftSlotId[2] == CoalBlock::getID() || craftSlotId[3] == CoalBlock::getID())
            {
                craftItemId = Coal::getID();
                craftItemSt = 1;
                craftItemAm = 9;
            }
        break;

        // redstone block to redstone items
        case 215:
            if(craftSlotId[0] == RedstoneBlock::getID() || craftSlotId[1] == RedstoneBlock::getID() || craftSlotId[2] == RedstoneBlock::getID() || craftSlotId[3] == RedstoneBlock::getID())
            {
                craftItemId = Redstone::getID();
                craftItemSt = 1;
                craftItemAm = 9;
            }
        break;

        // bricks to brick block
        case 1156:
            if(craftSlotId[0] == 289 && craftSlotId[1] == 289 && craftSlotId[2] == 289 && craftSlotId[3] == 289)
            {
                craftItemId = 27;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;

        // clay pieces to clay block
        case 1132:
            if(craftSlotId[0] == 283 && craftSlotId[1] == 283 && craftSlotId[2] == 283 && craftSlotId[3] == 283)
            {
                craftItemId = 99;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;

        // sand blocks to sandstone
        case 28:
            if(craftSlotId[0] == 7 && craftSlotId[1] == 7 && craftSlotId[2] == 7 && craftSlotId[3] == 7)
            {
                craftItemId = 26;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;

        // stones to stone bricks
        case 12:
            if(craftSlotId[0] == 3 && craftSlotId[1] == 3 && craftSlotId[2] == 3 && craftSlotId[3] == 3)
            {
                craftItemId = 102;
                craftItemSt = 1;
                craftItemAm = 4;
            }
        break;


        // half to carved stone brick
        case 216:
            if((craftSlotId[0] == 108 && craftSlotId[2] == 108) || (craftSlotId[1] == 108 && craftSlotId[3] == 108))
            {
                craftItemId = 104;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;


        // planks to crafting table
        case 136:
            if(craftSlotId[0] == 34)
            {
                craftItemId = 105;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;
        case 620:
            if(craftSlotId[0] == 155)
            {
                craftItemId = 105;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;
        case 624:
            if(craftSlotId[0] == 156)
            {
                craftItemId = 105;
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;

        // planks to sticks
        case 68: // oak
            if((craftSlotId[0] == 34 && craftSlotId[2] == 34)  || (craftSlotId[1] == 34 && craftSlotId[3] == 34))
            {
                craftItemId = 276;
                craftItemSt = 1;
                craftItemAm = 4;
            }
        break;
        case 310: // birch
            if((craftSlotId[0] == 155 && craftSlotId[2] == 155)  || (craftSlotId[1] == 155 && craftSlotId[3] == 155))
            {
                craftItemId = 276;
                craftItemSt = 1;
                craftItemAm = 4;
            }
        break;
        case 312: // spruce
            if((craftSlotId[0] == 156 && craftSlotId[2] == 156)  || (craftSlotId[1] == 156 && craftSlotId[3] == 156))
            {
                craftItemId = 276;
                craftItemSt = 1;
                craftItemAm = 4;
            }
        break;


        // torch
        case 553:
            if((craftSlotId[0] == 277 && craftSlotId[2] == 276) || (craftSlotId[1] == 277 && craftSlotId[3] == 276))
            {
                craftItemId = 308;
                craftItemSt = 1;
                craftItemAm = 4;
            }
        break;

        // jack o'lantern
        case 383:
            if((craftSlotId[0] == Pumpkin3::getID() && craftSlotId[2] == TorchItem::getID()) || (craftSlotId[1] == Pumpkin3::getID() && craftSlotId[3] == TorchItem::getID()))
            {
                craftItemId = JackOLantern3::getID();
                craftItemSt = 1;
                craftItemAm = 1;
            }
        break;

        case 556: //scissors
            if((craftSlotId[2] == 278 && craftSlotId[1] == 278))
            {
                craftItemId = 275;
                craftItemSt = 0;
                craftItemAm = 239;
            }
        break;

        case 629: //flint and steel
            if((craftSlotId[0] == IronBar::getID() && craftSlotId[3] == Flint::getID()))
            {
                craftItemId = FlintAndSteel::getID();
                craftItemSt = 0;
                craftItemAm = 65;
            }
        break;

        // pumpkin to pumpkin seeds
        case 75:
            if(craftSlotId[0] == 75 || craftSlotId[1] == 75 || craftSlotId[2] == 75 || craftSlotId[3] == 75)
            {
                craftItemId = PumpkinSeeds::getID();
                craftItemSt = 1;
                craftItemAm = 4;
            }
        break;

        // rose to dye
        case 294:
            if(craftSlotId[0] == Flower1Item::getID() || craftSlotId[1] == Flower1Item::getID() || craftSlotId[2] == Flower1Item::getID() || craftSlotId[3] == Flower1Item::getID())
            {
                craftItemId = RoseRed::getID();
                craftItemSt = 1;
                craftItemAm = 2;
            }
        break;

        // dandelion to dye
        case 295:
            if(craftSlotId[0] == Flower2Item::getID() || craftSlotId[1] == Flower2Item::getID() || craftSlotId[2] == Flower2Item::getID() || craftSlotId[3] == Flower2Item::getID())
            {
                craftItemId = DandelionYellow::getID();
                craftItemSt = 1;
                craftItemAm = 2;
            }
        break;

        default:
            craftItemId = -1;
            craftItemAm = -1;
            craftItemSt = 0;
        break;

        }
    }
}
/// Need to create new crafting system. Too many recipes cause mistakes and errors due to this system
void StatePlay::CraftItem3x3()
{
    craftItemId3 = -1;
    craftItemAm3 = -1;
    craftItemSt3 = 0;

    int result = 0;
    bool enableMainCraftBody = true;
    std::string materialString;

    std::vector<int> itemsIDs;

    for(int i = 0; i <= 8; i ++)
    {
        if(craftSlotId3[i] != -1)
        {
            result += craftSlotId3[i];
            itemsIDs.push_back(craftSlotId3[i]);
        }

        if(craftSlotId3[i] >= BlackWoolBlock::getID() && craftSlotId3[i] <= WhiteWoolBlock::getID())
        {
            materialString += "W";
        }
        else if(craftSlotId3[i] == OakPlanks::getID() || craftSlotId3[i] == SprucePlanks::getID() || craftSlotId3[i] == BirchPlanks::getID())
        {
            materialString += "P";
        }
        else
        {
            materialString += "0";
        }
    }

    if(materialString == "000WWWPPP")
    {
        craftItemId3 = BedItem::getID();
        craftItemSt3 = 0;
        craftItemAm3 = 1;

        enableMainCraftBody = false;
    }

    if(materialString == "WWWPPP000")
    {
        craftItemId3 = BedItem::getID();
        craftItemSt3 = 0;
        craftItemAm3 = 1;

        enableMainCraftBody = false;
    }

    // carpets
    if(materialString == "WW0000000" || materialString == "0WW000000" ||
       materialString == "000WW0000" || materialString == "0000WW000" ||
       materialString == "000000WW0" || materialString == "0000000WW")
    {
        if(itemsIDs.size() == 2)
        {
            if(itemsIDs[0] == itemsIDs[1])
            {
                craftItemId3 = itemsIDs[0]+188;
                craftItemSt3 = 1;
                craftItemAm3 = 3;

                enableMainCraftBody = false;
            }
        }
    }


    if(itemsIDs.size() == 2) // dyes and colored wool crafting
    {
        if((itemsIDs[0] == RoseRed::getID() && itemsIDs[1] == DandelionYellow::getID()) ||
           (itemsIDs[1] == RoseRed::getID() && itemsIDs[0] == DandelionYellow::getID()))
        {
            craftItemId3 = OrangeDye::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == RoseRed::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == RoseRed::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId3 = PinkDye::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == CactusGreen::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == CactusGreen::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId3 = LimeDye::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == LapisLazuli::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == LapisLazuli::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId3 = LightBlueDye::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == LapisLazuli::getID() && itemsIDs[1] == RoseRed::getID()) ||
           (itemsIDs[1] == LapisLazuli::getID() && itemsIDs[0] == RoseRed::getID()))
        {
            craftItemId3 = PurpleDye::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == PurpleDye::getID() && itemsIDs[1] == PinkDye::getID()) ||
           (itemsIDs[1] == PurpleDye::getID() && itemsIDs[0] == PinkDye::getID()))
        {
            craftItemId3 = MagnetaDye::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == LapisLazuli::getID() && itemsIDs[1] == RoseRed::getID()) ||
           (itemsIDs[1] == LapisLazuli::getID() && itemsIDs[0] == RoseRed::getID()))
        {
            craftItemId3 = PurpleDye::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == InkSac::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == InkSac::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId3 = GrayDye::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == GrayDye::getID() && itemsIDs[1] == BoneMeal::getID()) ||
           (itemsIDs[1] == GrayDye::getID() && itemsIDs[0] == BoneMeal::getID()))
        {
            craftItemId3 = LightGrayDye::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 2;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == InkSac::getID() && itemsIDs[1] == OrangeDye::getID()) ||
           (itemsIDs[1] == InkSac::getID() && itemsIDs[0] == OrangeDye::getID()))
        {
            craftItemId3 = CocoaBeans::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }


        /// wool


        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == RoseRed::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == RoseRed::getID()))
        {
            craftItemId3 = RedWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == DandelionYellow::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == DandelionYellow::getID()))
        {
            craftItemId3 = YellowWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == OrangeDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == OrangeDye::getID()))
        {
            craftItemId3 = OrangeWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == CactusGreen::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == CactusGreen::getID()))
        {
            craftItemId3 = DarkGreenWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == LimeDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == LimeDye::getID()))
        {
            craftItemId3 = GreenWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == LapisLazuli::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == LapisLazuli::getID()))
        {
            craftItemId3 = BlueWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == LightBlueDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == LightBlueDye::getID()))
        {
            craftItemId3 = LightBlueWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == PurpleDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == PurpleDye::getID()))
        {
            craftItemId3 = VioletWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == MagnetaDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == MagnetaDye::getID()))
        {
            craftItemId3 = PastelWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == InkSac::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == InkSac::getID()))
        {
            craftItemId3 = BlackWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == GrayDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == GrayDye::getID()))
        {
            craftItemId3 = GrayWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        if((itemsIDs[0] == WhiteWoolBlock::getID() && itemsIDs[1] == LightGrayDye::getID()) ||
           (itemsIDs[1] == WhiteWoolBlock::getID() && itemsIDs[0] == LightGrayDye::getID()))
        {
            craftItemId3 = LightGrayWoolBlock::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }

        // lever: one cobblestone and one stick, the vanilla shape. It has to live here rather than in
        // the 3x3 switch below, because that switch keys on the *sum* of the grid and cobblestone +
        // stick is 312 -- already the case label of the two-spruce-plank stick recipe. Two different
        // shapes summing to the same number is the one thing that switch cannot express.
        if((itemsIDs[0] == CobbleStone::getID() && itemsIDs[1] == 276) ||
           (itemsIDs[1] == CobbleStone::getID() && itemsIDs[0] == 276))
        {
            craftItemId3 = Lever::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }
    }

    if(itemsIDs.size() == 3)
    {
        if(result == RoseRed::getID() + PinkDye::getID() + LapisLazuli::getID())
        {
            if(itemsIDs[0] == RoseRed::getID() || itemsIDs[1] == RoseRed::getID() || itemsIDs[2] == RoseRed::getID())
            {
                craftItemId3 = MagnetaDye::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 3;

                enableMainCraftBody = false;
            }
        }
    }

    // piston: three planks across the top, cobblestone-iron-cobblestone in the middle and
    // cobblestone-redstone-cobblestone along the bottom -- vanilla's shape, and here the shape is the
    // whole point and not the total. The 3x3 switch below keys on the *sum* of the grid, and a sum
    // cannot tell this arrangement from any other arrangement of the same nine items, so the grid is
    // checked slot by slot.
    //
    // The three planks may be any wood, mixed or not: vanilla's recipe asks the ore dictionary for
    // plankWood, and the bed and carpet recipes above read planks as one category for the same reason.
    // There is deliberately no sticky piston recipe. Vanilla wants a slimeball for it and this build has
    // no slime and no slimeball, so a sticky piston is a creative-tab item -- see the Redstone tab.
    {
        bool planks = true;
        for(int i = 0; i <= 2; i++)
        {
            if(craftSlotId3[i] != OakPlanks::getID() && craftSlotId3[i] != SprucePlanks::getID() &&
               craftSlotId3[i] != BirchPlanks::getID())
            {
                planks = false;
            }
        }

        if(planks &&
           craftSlotId3[3] == CobbleStone::getID() && craftSlotId3[4] == IronBar::getID() &&
           craftSlotId3[5] == CobbleStone::getID() && craftSlotId3[6] == CobbleStone::getID() &&
           craftSlotId3[7] == Redstone::getID() && craftSlotId3[8] == CobbleStone::getID())
        {
            craftItemId3 = PistonOff::getID();
            craftItemSt3 = 1;
            craftItemAm3 = 1;

            enableMainCraftBody = false;
        }
    }

    itemsIDs.clear();

    if(enableMainCraftBody)
    {
        switch(result)
        {
        case 321://bone to bone meals
            if(craftSlotId3[0] == 321 || craftSlotId3[1] == 321 || craftSlotId3[2] == 321 || craftSlotId3[3] == 321 || craftSlotId3[4] == 321 || craftSlotId3[5] == 321 || craftSlotId3[6] == 321 || craftSlotId3[7] == 321 || craftSlotId3[8] == 321)
            {
                craftItemId3 = 322;
                craftItemSt3 = 1;
                craftItemAm3 = 3;
            }
        break;

        // arrow: flint over stick over feather, vanilla's vertical column, and vanilla yields four.
        // The grid is checked slot by slot on purpose -- this switch keys on the *sum* of the grid,
        // so 1038 only proves "something that adds up to flint + stick + feather", not the shape.
        case 1038:
            for(int col = 0; col < 3; col++)
            {
                if(craftSlotId3[col] == Flint::getID() &&
                   craftSlotId3[col+3] == Stick::getID() &&
                   craftSlotId3[col+6] == Feather::getID())
                {
                    craftItemId3 = Arrow::getID();
                    craftItemSt3 = 1;
                    craftItemAm3 = 4;
                }
            }
        break;

        // bow: three sticks and three string in vanilla's shape, string down the right column and
        // sticks curving down the middle and left:
        //     . s #
        //     s . #
        //     . s #
        // 3*string + 3*stick is 2073, which nothing else in this switch claims.
        case 2073:
            if(craftSlotId3[1] == Stick::getID() && craftSlotId3[3] == Stick::getID() &&
               craftSlotId3[7] == Stick::getID() &&
               craftSlotId3[2] == String::getID() && craftSlotId3[5] == String::getID() &&
               craftSlotId3[8] == String::getID())
            {
                craftItemId3 = Bow::getID();
                // A bow is a tool, so it is not stackable and its amount is its durability, not a
                // count of one. This used to read (1, 1), which meant a crafted bow arrived with a
                // single use and an already-empty durability bar -- see CraftWorld::FreshItemAmount.
                craftItemSt3 = 0;
                craftItemAm3 = mWorld->FreshItemAmount(Bow::getID());
            }
        break;

        case 294: //rose to dye
            if(craftSlotId3[0] == Flower1Item::getID() || craftSlotId3[1] == Flower1Item::getID() || craftSlotId3[2] == Flower1Item::getID() ||
               craftSlotId3[3] == Flower1Item::getID() || craftSlotId3[4] == Flower1Item::getID() || craftSlotId3[5] == Flower1Item::getID() ||
               craftSlotId3[6] == Flower1Item::getID() || craftSlotId3[7] == Flower1Item::getID() || craftSlotId3[8] == Flower1Item::getID())
            {
                craftItemId3 = RoseRed::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 2;
            }
        break;

        case 295: //rose to dye
            if(craftSlotId3[0] == Flower2Item::getID() || craftSlotId3[1] == Flower2Item::getID() || craftSlotId3[2] == Flower2Item::getID() ||
               craftSlotId3[3] == Flower2Item::getID() || craftSlotId3[4] == Flower2Item::getID() || craftSlotId3[5] == Flower2Item::getID() ||
               craftSlotId3[6] == Flower2Item::getID() || craftSlotId3[7] == Flower2Item::getID() || craftSlotId3[8] == Flower2Item::getID())
            {
                craftItemId3 = DandelionYellow::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 2;
            }
        break;

        case 625: //noteblock (oak)
            if(craftSlotId3[1] == 34 && craftSlotId3[2] == 34 && craftSlotId3[3] == 34 && craftSlotId3[5] == 34 && craftSlotId3[7] == 34 && craftSlotId3[0] == 34 && craftSlotId3[6] == 34 && craftSlotId3[8] == 34 && craftSlotId3[4] == Redstone::getID())
            {
                craftItemId3 = NoteBlock::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1593: //noteblock (birch)
            if(craftSlotId3[1] == 155 && craftSlotId3[2] == 155 && craftSlotId3[3] == 155 && craftSlotId3[5] == 155 && craftSlotId3[7] == 155 && craftSlotId3[0] == 155 && craftSlotId3[6] == 155 && craftSlotId3[8] == 155 && craftSlotId3[4] == Redstone::getID())
            {
                craftItemId3 = NoteBlock::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1601: //noteblock (spruce)
            if(craftSlotId3[1] == 156 && craftSlotId3[2] == 156 && craftSlotId3[3] == 156 && craftSlotId3[5] == 156 && craftSlotId3[7] == 156 && craftSlotId3[0] == 156 && craftSlotId3[6] == 156 && craftSlotId3[8] == 156 && craftSlotId3[4] == Redstone::getID())
            {
                craftItemId3 = NoteBlock::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 551: //noteblock (oak)
            if(craftSlotId3[1] == 34 && craftSlotId3[2] == 34 && craftSlotId3[3] == 34 && craftSlotId3[5] == 34 && craftSlotId3[7] == 34 && craftSlotId3[0] == 34 && craftSlotId3[6] == 34 && craftSlotId3[8] == 34 && craftSlotId3[4] == DiamondItem::getID())
            {
                craftItemId3 = JukeBox::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1519: //noteblock (birch)
            if(craftSlotId3[1] == 155 && craftSlotId3[2] == 155 && craftSlotId3[3] == 155 && craftSlotId3[5] == 155 && craftSlotId3[7] == 155 && craftSlotId3[0] == 155 && craftSlotId3[6] == 155 && craftSlotId3[8] == 155 && craftSlotId3[4] == DiamondItem::getID())
            {
                craftItemId3 = JukeBox::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1527: //noteblock (spruce)
            if(craftSlotId3[1] == 156 && craftSlotId3[2] == 156 && craftSlotId3[3] == 156 && craftSlotId3[5] == 156 && craftSlotId3[7] == 156 && craftSlotId3[0] == 156 && craftSlotId3[6] == 156 && craftSlotId3[8] == 156 && craftSlotId3[4] == DiamondItem::getID())
            {
                craftItemId3 = JukeBox::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 8: //wood to planks
            if(craftSlotId3[0] == 8 || craftSlotId3[1] == 8 || craftSlotId3[2] == 8 || craftSlotId3[3] == 8 || craftSlotId3[4] == 8 || craftSlotId3[5] == 8 || craftSlotId3[6] == 8 || craftSlotId3[7] == 8 || craftSlotId3[8] == 8)
            {
                craftItemId3 = 34;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;

        case 31://wood to birch planks
            if(craftSlotId3[0] == 31 || craftSlotId3[1] == 31 || craftSlotId3[2] == 31 || craftSlotId3[3] == 31 || craftSlotId3[4] == 31 || craftSlotId3[5] == 31 || craftSlotId3[6] == 31 || craftSlotId3[7] == 31 || craftSlotId3[8] == 31)
            {
                craftItemId3 = 155;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;
        case 30://wood to spurce planks
            if(craftSlotId3[0] == 30 || craftSlotId3[1] == 30 || craftSlotId3[2] == 30 || craftSlotId3[3] == 30 || craftSlotId3[4] == 30 || craftSlotId3[5] == 30 || craftSlotId3[6] == 30 || craftSlotId3[7] == 30 || craftSlotId3[8] == 30)
            {
                craftItemId3 = 156;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;
        case 311://melon to seeds
            if(craftSlotId3[0] == 311 || craftSlotId3[1] == 311 || craftSlotId3[2] == 311 || craftSlotId3[3] == 311 || craftSlotId3[4] == 311 || craftSlotId3[5] == 311 || craftSlotId3[6] == 311 || craftSlotId3[7] == 311 || craftSlotId3[8] == 311)
            {
                craftItemId3 = 312;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 293://cane to sugar
            if(craftSlotId3[0] == 293 || craftSlotId3[1] == 293 || craftSlotId3[2] == 293 || craftSlotId3[3] == 293 || craftSlotId3[4] == 293 || craftSlotId3[5] == 293 || craftSlotId3[6] == 293 || craftSlotId3[7] == 293 || craftSlotId3[8] == 293)
            {
                craftItemId3 = 313;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 68://sticks
            if( (craftSlotId3[0] == 34 && craftSlotId3[3] == 34)  || (craftSlotId3[1] == 34 && craftSlotId3[4] == 34) || (craftSlotId3[2] == 34 && craftSlotId3[5] == 34) || (craftSlotId3[3] == 34 && craftSlotId3[6] == 34) || (craftSlotId3[4] == 34 && craftSlotId3[7] == 34) || (craftSlotId3[5] == 34 && craftSlotId3[8] == 34))
            {
                craftItemId3 = 276;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;
        case 310: //birch
            if( (craftSlotId3[0] == 155 && craftSlotId3[3] == 155)  || (craftSlotId3[1] == 155 && craftSlotId3[4] == 155) || (craftSlotId3[2] == 155 && craftSlotId3[5] == 155) || (craftSlotId3[3] == 155 && craftSlotId3[6] == 155) || (craftSlotId3[4] == 155 && craftSlotId3[7] == 155) || (craftSlotId3[5] == 155 && craftSlotId3[8] == 155))
            {
                craftItemId3 = 276;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;
        case 312://spruce
            if( (craftSlotId3[0] == 156 && craftSlotId3[3] == 156)  || (craftSlotId3[1] == 156 && craftSlotId3[4] == 156) || (craftSlotId3[2] == 156 && craftSlotId3[5] == 156) || (craftSlotId3[3] == 156 && craftSlotId3[6] == 156) || (craftSlotId3[4] == 156 && craftSlotId3[7] == 156) || (craftSlotId3[5] == 156 && craftSlotId3[8] == 156))
            {
                craftItemId3 = 276;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;

        case 688: // oak fence
            if((craftSlotId3[0] == 34 && craftSlotId3[1] == 276 && craftSlotId3[2] == 34 && craftSlotId3[3] == 34 && craftSlotId3[4] == 276 && craftSlotId3[5] == 34) || (craftSlotId3[3] == 34 && craftSlotId3[4] == 276 && craftSlotId3[5] == 34 && craftSlotId3[6] == 34 && craftSlotId3[7] == 276 && craftSlotId3[8] == 34))
            {
                craftItemId3 = WoodenFence::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 2;
            }
        break;
        case 1172: // birch fence
            if((craftSlotId3[0] == 155 && craftSlotId3[1] == 276 && craftSlotId3[2] == 155 && craftSlotId3[3] == 155 && craftSlotId3[4] == 276 && craftSlotId3[5] == 155) || (craftSlotId3[3] == 155 && craftSlotId3[4] == 276 && craftSlotId3[5] == 155 && craftSlotId3[6] == 155 && craftSlotId3[7] == 276 && craftSlotId3[8] == 155))
            {
                craftItemId3 = BirchFence::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 2;
            }
        break;
        case 1176: // spruce fence
            if((craftSlotId3[0] == 156 && craftSlotId3[1] == 276 && craftSlotId3[2] == 156 && craftSlotId3[3] == 156 && craftSlotId3[4] == 276 && craftSlotId3[5] == 156) || (craftSlotId3[3] == 156 && craftSlotId3[4] == 276 && craftSlotId3[5] == 156 && craftSlotId3[6] == 156 && craftSlotId3[7] == 276 && craftSlotId3[8] == 156))
            {
                craftItemId3 = SpruceFence::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 2;
            }
        break;

        case 240: //glass panel
            if((craftSlotId3[0] == 40 && craftSlotId3[1] == 40 && craftSlotId3[2] == 40 && craftSlotId3[3] == 40 && craftSlotId3[4] == 40 && craftSlotId3[5] == 40)
            || (craftSlotId3[3] == 40 && craftSlotId3[4] == 40 && craftSlotId3[5] == 40 && craftSlotId3[6] == 40 && craftSlotId3[7] == 40 && craftSlotId3[8] == 40))
            {
                craftItemId3 = GlassPanelItem::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 16;
            }
        break;

        case 1668: //iron bars
            if((craftSlotId3[0] == 278 && craftSlotId3[1] == 278 && craftSlotId3[2] == 278 && craftSlotId3[3] == 278 && craftSlotId3[4] == 278 && craftSlotId3[5] == 278)
            || (craftSlotId3[3] == 278 && craftSlotId3[4] == 278 && craftSlotId3[5] == 278 && craftSlotId3[6] == 278 && craftSlotId3[7] == 278 && craftSlotId3[8] == 278))
            {
                craftItemId3 = IronBarsItem::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 16;
            }
        break;

        case 204: //door
            if((craftSlotId3[0] == 34 && craftSlotId3[1] == 34 && craftSlotId3[3] == 34 && craftSlotId3[4] == 34 && craftSlotId3[6] == 34 && craftSlotId3[7] == 34) || (craftSlotId3[1] == 34 && craftSlotId3[2] == 34 && craftSlotId3[4] == 34 && craftSlotId3[5] == 34 && craftSlotId3[7] == 34 && craftSlotId3[8] == 34))
            {
                craftItemId3 = 306;
                craftItemSt3 = 0;
                craftItemAm3 = 1;
            }
            //wooden stair
            if((craftSlotId3[0] == 34 && craftSlotId3[3] == 34 && craftSlotId3[4] == 34 && craftSlotId3[6] == 34 && craftSlotId3[7] == 34 && craftSlotId3[8] == 34) ||
               (craftSlotId3[2] == 34 && craftSlotId3[4] == 34 && craftSlotId3[5] == 34 && craftSlotId3[6] == 34 && craftSlotId3[7] == 34 && craftSlotId3[8] == 34))
            {
                craftItemId3 = 125;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
            //trapdoor
            if((craftSlotId3[0] == 34 && craftSlotId3[1] == 34 && craftSlotId3[2] == 34 && craftSlotId3[3] == 34 && craftSlotId3[4] == 34 && craftSlotId3[5] == 34) ||
               (craftSlotId3[3] == 34 && craftSlotId3[4] == 34 && craftSlotId3[5] == 34 && craftSlotId3[6] == 34 && craftSlotId3[7] == 34 && craftSlotId3[8] == 34))
            {
                craftItemId3 = TrapdoorClosed1::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 2;
            }
        break;

        case 930: //door (birch)
            if((craftSlotId3[0] == 155 && craftSlotId3[1] == 155 && craftSlotId3[3] == 155 && craftSlotId3[4] == 155 && craftSlotId3[6] == 155 && craftSlotId3[7] == 155) || (craftSlotId3[1] == 155 && craftSlotId3[2] == 155 && craftSlotId3[4] == 155 && craftSlotId3[5] == 155 && craftSlotId3[7] == 155 && craftSlotId3[8] == 155))
            {
                craftItemId3 = 306;
                craftItemSt3 = 0;
                craftItemAm3 = 1;
            }
            //wooden stair
            if((craftSlotId3[0] == 155 && craftSlotId3[3] == 155 && craftSlotId3[4] == 155 && craftSlotId3[6] == 155 && craftSlotId3[7] == 155 && craftSlotId3[8] == 155) ||
               (craftSlotId3[2] == 155 && craftSlotId3[4] == 155 && craftSlotId3[5] == 155 && craftSlotId3[6] == 155 && craftSlotId3[7] == 155 && craftSlotId3[8] == 155))
            {
                craftItemId3 = BirchStair1::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
            //trapdoor
            if((craftSlotId3[0] == 155 && craftSlotId3[1] == 155 && craftSlotId3[2] == 155 && craftSlotId3[3] == 155 && craftSlotId3[4] == 155 && craftSlotId3[5] == 155) ||
               (craftSlotId3[3] == 155 && craftSlotId3[4] == 155 && craftSlotId3[5] == 155 && craftSlotId3[6] == 155 && craftSlotId3[7] == 155 && craftSlotId3[8] == 155))
            {
                craftItemId3 = TrapdoorClosed1::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 2;
            }
        break;


        case 936: //door (spruce)
            if((craftSlotId3[0] == 156 && craftSlotId3[1] == 156 && craftSlotId3[3] == 156 && craftSlotId3[4] == 156 && craftSlotId3[6] == 156 && craftSlotId3[7] == 156) || (craftSlotId3[1] == 156 && craftSlotId3[2] == 156 && craftSlotId3[4] == 156 && craftSlotId3[5] == 156 && craftSlotId3[7] == 156 && craftSlotId3[8] == 156))
            {
                craftItemId3 = 306;
                craftItemSt3 = 0;
                craftItemAm3 = 1;
            }
            //wooden stair
            if((craftSlotId3[0] == 156 && craftSlotId3[3] == 156 && craftSlotId3[4] == 156 && craftSlotId3[6] == 156 && craftSlotId3[7] == 156 && craftSlotId3[8] == 156) ||
               (craftSlotId3[2] == 156 && craftSlotId3[4] == 156 && craftSlotId3[5] == 156 && craftSlotId3[6] == 156 && craftSlotId3[7] == 156 && craftSlotId3[8] == 156))
            {
                craftItemId3 = SpruceStair1::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
            //trapdoor
            if((craftSlotId3[0] == 156 && craftSlotId3[1] == 156 && craftSlotId3[2] == 156 && craftSlotId3[3] == 156 && craftSlotId3[4] == 156 && craftSlotId3[5] == 156) ||
               (craftSlotId3[3] == 156 && craftSlotId3[4] == 156 && craftSlotId3[5] == 156 && craftSlotId3[6] == 156 && craftSlotId3[7] == 156 && craftSlotId3[8] == 156))
            {
                craftItemId3 = TrapdoorClosed1::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 2;
            }
        break;


        case 216://stone stair
            if((craftSlotId3[0] == 36 && craftSlotId3[3] == 36 && craftSlotId3[4] == 36 && craftSlotId3[6] == 36 && craftSlotId3[7] == 36 && craftSlotId3[8] == 36) ||
               (craftSlotId3[2] == 36 && craftSlotId3[4] == 36 && craftSlotId3[5] == 36 && craftSlotId3[6] == 36 && craftSlotId3[7] == 36 && craftSlotId3[8] == 36))
            {
                craftItemId3 = 60;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
            // decorated stone brick
            if((craftSlotId3[0] == 108 && craftSlotId3[3] == 108) || (craftSlotId3[1] == 108 && craftSlotId3[4] == 108) || (craftSlotId3[2] == 108 && craftSlotId3[5] == 108) || (craftSlotId3[3] == 108 && craftSlotId3[6] == 108) || (craftSlotId3[4] == 108 && craftSlotId3[7] == 108) || (craftSlotId3[5] == 108 && craftSlotId3[8] == 108))
            {
                craftItemId3 = 104;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 162://brick stair
            if((craftSlotId3[0] == 27 && craftSlotId3[3] == 27 && craftSlotId3[4] == 27 && craftSlotId3[6] == 27 && craftSlotId3[7] == 27 && craftSlotId3[8] == 27) ||
               (craftSlotId3[2] == 27 && craftSlotId3[4] == 27 && craftSlotId3[5] == 27 && craftSlotId3[6] == 27 && craftSlotId3[7] == 27 && craftSlotId3[8] == 27))
            {
                craftItemId3 = 67;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;

        case 136://crafting
            if((craftSlotId3[0] == 34 && craftSlotId3[1] == 34 && craftSlotId3[3] == 34 && craftSlotId3[4] == 34) || (craftSlotId3[1] == 34 && craftSlotId3[2] == 34 && craftSlotId3[4] == 34 && craftSlotId3[5] == 34) || (craftSlotId3[3] == 34 && craftSlotId3[4] == 34 && craftSlotId3[6] == 34 && craftSlotId3[7] == 34) || (craftSlotId3[4] == 34 && craftSlotId3[5] == 34 && craftSlotId3[7] == 34 && craftSlotId3[8] == 34))
            {
                craftItemId3 = 105;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1196://snow block
            if((craftSlotId3[0] == 299 && craftSlotId3[1] == 299 && craftSlotId3[3] == 299 && craftSlotId3[4] == 299) || (craftSlotId3[1] == 299 && craftSlotId3[2] == 299 && craftSlotId3[4] == 299 && craftSlotId3[5] == 299) || (craftSlotId3[3] == 299 && craftSlotId3[4] == 299 && craftSlotId3[6] == 299 && craftSlotId3[7] == 299) || (craftSlotId3[4] == 299 && craftSlotId3[5] == 299 && craftSlotId3[7] == 299 && craftSlotId3[8] == 299))
            {
                craftItemId3 = 47;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 28: //stonesand from sand
            if((craftSlotId3[0] == 28 && craftSlotId3[1] == 28 && craftSlotId3[3] == 28 && craftSlotId3[4] == 28) || (craftSlotId3[1] == 28 && craftSlotId3[2] == 28 && craftSlotId3[4] == 28 && craftSlotId3[5] == 28) || (craftSlotId3[3] == 28 && craftSlotId3[4] == 28 && craftSlotId3[6] == 28 && craftSlotId3[7] == 28) || (craftSlotId3[4] == 28 && craftSlotId3[5] == 28 && craftSlotId3[7] == 28 && craftSlotId3[8] == 28))
            {
                craftItemId3 = 26;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1156: //brick block
            if((craftSlotId3[0] == 289 && craftSlotId3[1] == 289 && craftSlotId3[3] == 289 && craftSlotId3[4] == 289) || (craftSlotId3[1] == 289 && craftSlotId3[2] == 289 && craftSlotId3[4] == 289 && craftSlotId3[5] == 289) || (craftSlotId3[3] == 289 && craftSlotId3[4] == 289 && craftSlotId3[6] == 289 && craftSlotId3[7] == 289) || (craftSlotId3[4] == 289 && craftSlotId3[5] == 289 && craftSlotId3[7] == 289 && craftSlotId3[8] == 289))
            {
                craftItemId3 = 27;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 12: //stonebricks
            if((craftSlotId3[0] == 3 && craftSlotId3[1] == 3 && craftSlotId3[3] == 3 && craftSlotId3[4] == 3) || (craftSlotId3[1] == 3 && craftSlotId3[2] == 3 && craftSlotId3[4] == 3 && craftSlotId3[5] == 3) || (craftSlotId3[3] == 3 && craftSlotId3[4] == 3 && craftSlotId3[6] == 3 && craftSlotId3[7] == 3) || (craftSlotId3[4] == 3 && craftSlotId3[5] == 3 && craftSlotId3[7] == 3 && craftSlotId3[8] == 3))
            {
                craftItemId3 = 102;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;

        case 553://torches
            if((craftSlotId3[0] == 277 && craftSlotId3[3] == 276) || (craftSlotId3[1] == 277 && craftSlotId3[4] == 276) || (craftSlotId3[2] == 277 && craftSlotId3[5] == 276) || (craftSlotId3[3] == 277 && craftSlotId3[6] == 276) || (craftSlotId3[4] == 277 && craftSlotId3[7] == 276) || (craftSlotId3[5] == 277 && craftSlotId3[8] == 276))
            {
                craftItemId3 = 308;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;

        case 383://jack o'lantern
            if((craftSlotId3[0] == Pumpkin3::getID() && craftSlotId3[3] == TorchItem::getID()) || (craftSlotId3[1] == Pumpkin3::getID() && craftSlotId3[4] == TorchItem::getID()) || (craftSlotId3[2] == Pumpkin3::getID() && craftSlotId3[5] == TorchItem::getID()) || (craftSlotId3[3] == Pumpkin3::getID() && craftSlotId3[6] == TorchItem::getID()) || (craftSlotId3[4] == Pumpkin3::getID() && craftSlotId3[7] == TorchItem::getID()) || (craftSlotId3[5] == Pumpkin3::getID() && craftSlotId3[8] == TorchItem::getID()))
            {
                craftItemId3 = JackOLantern3::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        // redstone repeater: torch, dust, torch over a full row of stone -- the vanilla shape. The sum
        // is 3*3 + 2*408 + 353, and the grid is row-major, so the two accepted layouts are the torch row
        // on top (0,1,2 over 3,4,5) or on the bottom (3,4,5 over 6,7,8); the flip is allowed because the
        // 3x3 grid is easy to fill upside down and this codebase has no shaped-recipe mirroring.
        case 1178:
            if((craftSlotId3[0] == RedstoneTorchItem::getID() && craftSlotId3[1] == Redstone::getID() && craftSlotId3[2] == RedstoneTorchItem::getID() &&
                craftSlotId3[3] == 3 && craftSlotId3[4] == 3 && craftSlotId3[5] == 3) ||
               (craftSlotId3[3] == RedstoneTorchItem::getID() && craftSlotId3[4] == Redstone::getID() && craftSlotId3[5] == RedstoneTorchItem::getID() &&
                craftSlotId3[6] == 3 && craftSlotId3[7] == 3 && craftSlotId3[8] == 3))
            {
                craftItemId3 = RepeaterItem::getID(); // the item form: it places a dark RepeaterOff
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        // redstone lamp: four dust in a plus around a glowstone block, which is the vanilla recipe
        // verbatim. Unlike every other shape here it needs no second layout: a plus rotated is the same
        // plus. The result is the *dark* id, which is also its own loot, so a lamp broken anywhere in
        // the world comes back as a lamp and not as a lit one.
        case 1635:
            if(craftSlotId3[1] == Redstone::getID() && craftSlotId3[3] == Redstone::getID() &&
               craftSlotId3[4] == GlowstoneBlock::getID() &&
               craftSlotId3[5] == Redstone::getID() && craftSlotId3[7] == Redstone::getID())
            {
                craftItemId3 = RedstoneLampOff::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 556://scissors
            if((craftSlotId3[1] == 278 && craftSlotId3[3] == 278) || (craftSlotId3[2] == 278 && craftSlotId3[4] == 278) || (craftSlotId3[4] == 278 && craftSlotId3[6] == 278) || (craftSlotId3[5] == 278 && craftSlotId3[7] == 278))
            {
                craftItemId3 = 275;
                craftItemSt3 = 0;
                craftItemAm3 = 239;
            }
        break;

        case 1132://clay
            if((craftSlotId3[0] == 283 && craftSlotId3[1] == 283 && craftSlotId3[3] == 283 && craftSlotId3[4] == 283) || (craftSlotId3[1] == 283 && craftSlotId3[2] == 283 && craftSlotId3[4] == 283 && craftSlotId3[5] == 283) || (craftSlotId3[3] == 283 && craftSlotId3[4] == 283 && craftSlotId3[6] == 283 && craftSlotId3[7] == 283) || (craftSlotId3[4] == 283 && craftSlotId3[5] == 283 && craftSlotId3[7] == 283 && craftSlotId3[8] == 283))
            {
                craftItemId3 = 99;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        /// HALF BLOCKS
        case 108://cobblestone
            if((craftSlotId3[0] == 36 && craftSlotId3[1] == 36 && craftSlotId3[2] == 36) || (craftSlotId3[3] == 36 && craftSlotId3[4] == 36 && craftSlotId3[5] == 36) || (craftSlotId3[6] == 36 && craftSlotId3[7] == 36 && craftSlotId3[8] == 36))
            {
                craftItemId3 = 83;
                craftItemSt3 = 1;
                craftItemAm3 = 6;
            }
        break;

        case 102://half planks
            if((craftSlotId3[0] == 34 && craftSlotId3[1] == 34 && craftSlotId3[2] == 34) ||
               (craftSlotId3[3] == 34 && craftSlotId3[4] == 34 && craftSlotId3[5] == 34) ||
               (craftSlotId3[6] == 34 && craftSlotId3[7] == 34 && craftSlotId3[8] == 34))
            {
                craftItemId3 = 82;
                craftItemSt3 = 1;
                craftItemAm3 = 6;
            }
            //4 bowls
            if((craftSlotId3[0] == 34 && craftSlotId3[4] == 34 && craftSlotId3[2] == 34) ||
               (craftSlotId3[3] == 34 && craftSlotId3[7] == 34 && craftSlotId3[5] == 34))
            {
                craftItemId3 = 302;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;

        case 465://half birch planks
            if((craftSlotId3[0] == 155 && craftSlotId3[1] == 155 && craftSlotId3[2] == 155) ||
               (craftSlotId3[3] == 155 && craftSlotId3[4] == 155 && craftSlotId3[5] == 155) ||
               (craftSlotId3[6] == 155 && craftSlotId3[7] == 155 && craftSlotId3[8] == 155))
            {
                craftItemId3 = BirchHalfBlock::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 6;
            }
            //4 bowls
            if((craftSlotId3[0] == 155 && craftSlotId3[4] == 155 && craftSlotId3[2] == 155) ||
               (craftSlotId3[3] == 155 && craftSlotId3[7] == 155 && craftSlotId3[5] == 155))
            {
                craftItemId3 = 302;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;

        case 468://half spruce planks
            if((craftSlotId3[0] == 156 && craftSlotId3[1] == 156 && craftSlotId3[2] == 156) ||
               (craftSlotId3[3] == 156 && craftSlotId3[4] == 156 && craftSlotId3[5] == 156) ||
               (craftSlotId3[6] == 156 && craftSlotId3[7] == 156 && craftSlotId3[8] == 156))
            {
                craftItemId3 = SpruceHalfBlock::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 6;
            }
            //4 bowls
            if((craftSlotId3[0] == 156 && craftSlotId3[4] == 156 && craftSlotId3[2] == 156) ||
               (craftSlotId3[3] == 156 && craftSlotId3[7] == 156 && craftSlotId3[5] == 156))
            {
                craftItemId3 = 302;
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;

        case 9://stone
            if((craftSlotId3[0] == 3 && craftSlotId3[1] == 3 && craftSlotId3[2] == 3) || (craftSlotId3[3] == 3 && craftSlotId3[4] == 3 && craftSlotId3[5] == 3) || (craftSlotId3[6] == 3 && craftSlotId3[7] == 3 && craftSlotId3[8] == 3))
            {
                craftItemId3 = 85;
                craftItemSt3 = 1;
                craftItemAm3 = 6;
            }
        break;

        case 78://stonesand half block
            if((craftSlotId3[0] == 26 && craftSlotId3[1] == 26 && craftSlotId3[2] == 26) || (craftSlotId3[3] == 26 && craftSlotId3[4] == 26 && craftSlotId3[5] == 26) || (craftSlotId3[6] == 26 && craftSlotId3[7] == 26 && craftSlotId3[8] == 26))
            {
                craftItemId3 = 86;
                craftItemSt3 = 1;
                craftItemAm3 = 6;
            }
        break;

        case 306: //stonebrick half block
            if((craftSlotId3[0] == 102 && craftSlotId3[1] == 102 && craftSlotId3[2] == 102) || (craftSlotId3[3] == 102 && craftSlotId3[4] == 102 && craftSlotId3[5] == 102) || (craftSlotId3[6] == 102 && craftSlotId3[7] == 102 && craftSlotId3[8] == 102))
            {
                craftItemId3 = 108;
                craftItemSt3 = 1;
                craftItemAm3 = 6;
            }
        break;


        // end
        case 903://mooshroom bowl
            if((craftSlotId3[1] == 300 && craftSlotId3[4] == 301 && craftSlotId3[7] == 302) || (craftSlotId3[1] == 301 && craftSlotId3[4] == 300 && craftSlotId3[7] == 302))
            {
                craftItemId3 = 303;
                craftItemSt3 = 0;
                craftItemAm3 = 1;
            }
        break;

        /// Tool crafts start
        //wooden tools (oak)
        case 654:
            if(craftSlotId3[1] == 34 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 34 && craftSlotId3[2] == 34) // pick
            {
                craftItemId3 = 250;
                craftItemSt3 = 0;
                craftItemAm3 = 60;

            }
            if(craftSlotId3[0] == 34 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[1] == 34 && craftSlotId3[3] == 34) // axe
            {
                craftItemId3 = 265;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        case 620:
            if(craftSlotId3[1] == 34 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 34) // hoe
            {
                craftItemId3 = 270;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
            // crafting table (birch)
            if((craftSlotId3[0] == 155 && craftSlotId3[1] == 155 && craftSlotId3[3] == 155 && craftSlotId3[4] == 155) ||
               (craftSlotId3[1] == 155 && craftSlotId3[2] == 155 && craftSlotId3[4] == 155 && craftSlotId3[5] == 155) ||
               (craftSlotId3[3] == 155 && craftSlotId3[4] == 155 && craftSlotId3[6] == 155 && craftSlotId3[7] == 155) ||
               (craftSlotId3[4] == 155 && craftSlotId3[5] == 155 && craftSlotId3[7] == 155 && craftSlotId3[8] == 155))
            {
                craftItemId3 = 105;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 586:
            if(craftSlotId3[1] == 34 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276) // shovel
            {
                craftItemId3 = 260;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
            if(craftSlotId3[1] == 155 && craftSlotId3[4] == 155 && craftSlotId3[7] == 276) // sword
            {
                craftItemId3 = 255;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        case 344:
            if(craftSlotId3[1] == 34 && craftSlotId3[4] == 34 && craftSlotId3[7] == 276) // sword
            {
                craftItemId3 = 255;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        //wooden tools (birch)
        case 1017:
            if(craftSlotId3[1] == 155 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 155 && craftSlotId3[2] == 155) // pick
            {
                craftItemId3 = 250;
                craftItemSt3 = 0;
                craftItemAm3 = 60;

            }
            if(craftSlotId3[0] == 155 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[1] == 155 && craftSlotId3[3] == 155) // axe
            {
                craftItemId3 = 265;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        case 862:
            if(craftSlotId3[1] == 155 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 155) // hoe
            {
                craftItemId3 = 270;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        case 707:
            if(craftSlotId3[1] == 155 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276) // shovel
            {
                craftItemId3 = 260;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        //wooden tools (spruce)
        case 1020:
            if(craftSlotId3[1] == 156 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 156 && craftSlotId3[2] == 156) // pick
            {
                craftItemId3 = 250;
                craftItemSt3 = 0;
                craftItemAm3 = 60;

            }
            if(craftSlotId3[0] == 156 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[1] == 156 && craftSlotId3[3] == 156) // axe
            {
                craftItemId3 = 265;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        case 864:
            if(craftSlotId3[1] == 156 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 156) // hoe
            {
                craftItemId3 = 270;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        case 708:
            if(craftSlotId3[1] == 156 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276) // shovel
            {
                craftItemId3 = 260;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        //stone tools
        case 660:
            if(craftSlotId3[1] == 36 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 36 && craftSlotId3[2] == 36)
            {
                craftItemId3 = 251;
                craftItemSt3 = 0;
                craftItemAm3 = 132;

            }
            if(craftSlotId3[0] == 36 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[1] == 36 && craftSlotId3[3] == 36)
            {
                craftItemId3 = 266;
                craftItemSt3 = 0;
                craftItemAm3 = 132;

            }
            break;

        case 624:
            if(craftSlotId3[1] == 36 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 36)
            {
                craftItemId3 = 271;
                craftItemSt3 = 0;
                craftItemAm3 = 132;
            }
            if((craftSlotId3[0] == 156 && craftSlotId3[1] == 156 && craftSlotId3[3] == 156 && craftSlotId3[4] == 156) ||
               (craftSlotId3[1] == 156 && craftSlotId3[2] == 156 && craftSlotId3[4] == 156 && craftSlotId3[5] == 156) ||
               (craftSlotId3[3] == 156 && craftSlotId3[4] == 156 && craftSlotId3[6] == 156 && craftSlotId3[7] == 156) ||
               (craftSlotId3[4] == 156 && craftSlotId3[5] == 156 && craftSlotId3[7] == 156 && craftSlotId3[8] == 156))
            {
                craftItemId3 = 105;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 588:
            if(craftSlotId3[1] == 36 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276)
            {
                craftItemId3 = 261;
                craftItemSt3 = 0;
                craftItemAm3 = 132;
            }
            if(craftSlotId3[1] == 156 && craftSlotId3[4] == 156 && craftSlotId3[7] == 276) // sword
            {
                craftItemId3 = 255;
                craftItemSt3 = 0;
                craftItemAm3 = 60;
            }
        break;

        case 348:
            if(craftSlotId3[1] == 36 && craftSlotId3[4] == 36 && craftSlotId3[7] == 276)
            {
                craftItemId3 = 256;
                craftItemSt3 = 0;
                craftItemAm3 = 132;
            }
        break;

        //iron tools
        case 1386:
            if(craftSlotId3[1] == 278 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 278 && craftSlotId3[2] == 278)
            {
                craftItemId3 = 252;
                craftItemSt3 = 0;
                craftItemAm3 = 251;

            }
            if(craftSlotId3[0] == 278 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[1] == 278 && craftSlotId3[3] == 278)
            {
                craftItemId3 = 267;
                craftItemSt3 = 0;
                craftItemAm3 = 251;

            }
        break;

        case 1108:
            if(craftSlotId3[1] == 278 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 278)
            {
                craftItemId3 = 272;
                craftItemSt3 = 0;
                craftItemAm3 = 251;
            }
        break;

        case 830:
            if(craftSlotId3[1] == 278 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276)
            {
                craftItemId3 = 262;
                craftItemSt3 = 0;
                craftItemAm3 = 251;
            }
        break;

        case 832:
            if(craftSlotId3[1] == 278 && craftSlotId3[4] == 278 && craftSlotId3[7] == 276)
            {
                craftItemId3 = 257;
                craftItemSt3 = 0;
                craftItemAm3 = 251;
            }
            if(craftSlotId3[1] == 280 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276)
            {
                craftItemId3 = 264;
                craftItemSt3 = 0;
                // 33, the golden shovel's own durability -- this read 1562, the diamond shovel's,
                // which gave a golden shovel 47 times the life it should have had. Caught by
                // tools/check-tool-durability.py; see the note at the top of that script.
                craftItemAm3 = 33;
            }
        break;

        //diamond tools
        case 1389:
            if(craftSlotId3[1] == 279 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 279 && craftSlotId3[2] == 279)
            {
                craftItemId3 = 253;
                craftItemSt3 = 0;
                craftItemAm3 = 1562;

            }
            if(craftSlotId3[0] == 279 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[1] == 279 && craftSlotId3[3] == 279)
            {
                craftItemId3 = 268;
                craftItemSt3 = 0;
                craftItemAm3 = 1562;

            }
            break;

        case 1110:
            if(craftSlotId3[1] == 279 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 279)
            {
                craftItemId3 = 273;
                craftItemSt3 = 0;
                craftItemAm3 = 1562;
            }
        break;

        case 831:
            if(craftSlotId3[1] == 279 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276)
            {
                craftItemId3 = 263;
                craftItemSt3 = 0;
                craftItemAm3 = 1562;
            }
        break;

        case 834:
            if(craftSlotId3[1] == 279 && craftSlotId3[4] == 279 && craftSlotId3[7] == 276)
            {
                craftItemId3 = 258;
                craftItemSt3 = 0;
                craftItemAm3 = 1562;
            }

            if((craftSlotId3[0] == 278 && craftSlotId3[4] == 278 && craftSlotId3[2] == 278) || (craftSlotId3[3] == 278 && craftSlotId3[7] == 278 && craftSlotId3[5] == 278))
            {
                craftItemId3 = 290;
                craftItemSt3 = 0;
                craftItemAm3 = 1;
            }
        break;

        //golden tools
        case 1392:
            if(craftSlotId3[1] == 280 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 280 && craftSlotId3[2] == 280)
            {
                craftItemId3 = 254;
                craftItemSt3 = 0;
                craftItemAm3 = 33;

            }
            if(craftSlotId3[0] == 280 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[1] == 280 && craftSlotId3[3] == 280)
            {
                craftItemId3 = 269;
                craftItemSt3 = 0;
                craftItemAm3 = 33;

            }
            break;

        case 1112:
            if(craftSlotId3[1] == 280 && craftSlotId3[4] == 276 && craftSlotId3[7] == 276 && craftSlotId3[0] == 280)
            {
                craftItemId3 = 274;
                craftItemSt3 = 0;
                craftItemAm3 = 33;
            }

            if((craftSlotId3[0] == 278 && craftSlotId3[2] == 278 && craftSlotId3[3] == 278 && craftSlotId3[5] == 278) || (craftSlotId3[3] == 278 && craftSlotId3[5] == 278 && craftSlotId3[6] == 278 && craftSlotId3[8] == 278))
            {
                craftItemId3 = 339;
                craftItemAm3 = 195;
                craftItemSt3 = false;
            }
        break;

        case 836:
            if(craftSlotId3[1] == 280 && craftSlotId3[4] == 280 && craftSlotId3[7] == 276)
            {
                craftItemId3 = 259;
                craftItemSt3 = 0;
                craftItemAm3 = 33;
            }
        break;
        /// Tool crafts end

        /// Armor crafts begin
        // leather

        // helmet
        case 1615:
            if((craftSlotId3[0] == 323 && craftSlotId3[1] == 323 && craftSlotId3[2] == 323 && craftSlotId3[3] == 323 && craftSlotId3[5] == 323) || (craftSlotId3[3] == 323 && craftSlotId3[4] == 323 && craftSlotId3[5] == 323 && craftSlotId3[6] == 323 && craftSlotId3[8] == 323))
            {
                craftItemId3 = 328;
                craftItemAm3 = 55;
                craftItemSt3 = false;
            }
        break;

        // chestplate
        case 2584:
            if(craftSlotId3[0] == 323 && craftSlotId3[2] == 323 && craftSlotId3[3] == 323 && craftSlotId3[4] == 323 && craftSlotId3[5] == 323 && craftSlotId3[6] == 323 && craftSlotId3[7] == 323 && craftSlotId3[8] == 323)
            {
                craftItemId3 = 329;
                craftItemAm3 = 80;
                craftItemSt3 = false;
            }
        break;

        // leggings
        case 2261:
            if(craftSlotId3[0] == 323 && craftSlotId3[1] == 323 && craftSlotId3[2] == 323 && craftSlotId3[3] == 323 && craftSlotId3[5] == 323 && craftSlotId3[6] == 323 && craftSlotId3[8] == 323)
            {
                craftItemId3 = 330;
                craftItemAm3 = 75;
                craftItemSt3 = false;
            }
        break;

        // boots
        case 1292:
            if((craftSlotId3[0] == 323 && craftSlotId3[2] == 323 && craftSlotId3[3] == 323 && craftSlotId3[5] == 323) || (craftSlotId3[3] == 323 && craftSlotId3[5] == 323 && craftSlotId3[6] == 323 && craftSlotId3[8] == 323))
            {
                craftItemId3 = 331;
                craftItemAm3 = 65;
                craftItemSt3 = false;
            }
        break;

        // end

        // iron

        // helmet
        case 1390:
            if((craftSlotId3[0] == 278 && craftSlotId3[1] == 278 && craftSlotId3[2] == 278 && craftSlotId3[3] == 278 && craftSlotId3[5] == 278) || (craftSlotId3[3] == 278 && craftSlotId3[4] == 278 && craftSlotId3[5] == 278 && craftSlotId3[6] == 278 && craftSlotId3[8] == 278))
            {
                craftItemId3 = 336;
                craftItemAm3 = 165;
                craftItemSt3 = false;
            }
        break;

        // chestplate
        case 2224:
            if(craftSlotId3[0] == 278 && craftSlotId3[2] == 278 && craftSlotId3[3] == 278 && craftSlotId3[4] == 278 && craftSlotId3[5] == 278 && craftSlotId3[6] == 278 && craftSlotId3[7] == 278 && craftSlotId3[8] == 278)
            {
                craftItemId3 = 337;
                craftItemAm3 = 240;
                craftItemSt3 = false;
            }
        break;

        // leggings
        case 1946:
            if(craftSlotId3[0] == 278 && craftSlotId3[1] == 278 && craftSlotId3[2] == 278 && craftSlotId3[3] == 278 && craftSlotId3[5] == 278 && craftSlotId3[6] == 278 && craftSlotId3[8] == 278)
            {
                craftItemId3 = 338;
                craftItemAm3 = 225;
                craftItemSt3 = false;
            }
        break;


        // end

        // diamond

        // helmet
        case 1395:
            if((craftSlotId3[0] == 279 && craftSlotId3[1] == 279 && craftSlotId3[2] == 279 && craftSlotId3[3] == 279 && craftSlotId3[5] == 279) || (craftSlotId3[3] == 279 && craftSlotId3[4] == 279 && craftSlotId3[5] == 279 && craftSlotId3[6] == 279 && craftSlotId3[8] == 279))
            {
                craftItemId3 = 340;
                craftItemAm3 = 363;
                craftItemSt3 = false;
            }
        break;

        // chestplate
        case 2232:
            if(craftSlotId3[0] == 279 && craftSlotId3[2] == 279 && craftSlotId3[3] == 279 && craftSlotId3[4] == 279 && craftSlotId3[5] == 279 && craftSlotId3[6] == 279 && craftSlotId3[7] == 279 && craftSlotId3[8] == 279)
            {
                craftItemId3 = 341;
                craftItemAm3 = 528;
                craftItemSt3 = false;
            }
        break;

        // leggings
        case 1953:
            if(craftSlotId3[0] == 279 && craftSlotId3[1] == 279 && craftSlotId3[2] == 279 && craftSlotId3[3] == 279 && craftSlotId3[5] == 279 && craftSlotId3[6] == 279 && craftSlotId3[8] == 279)
            {
                craftItemId3 = 342;
                craftItemAm3 = 495;
                craftItemSt3 = false;
            }
        break;

        // boots
        case 1116:
            if((craftSlotId3[0] == 279 && craftSlotId3[2] == 279 && craftSlotId3[3] == 279 && craftSlotId3[5] == 279) || (craftSlotId3[3] == 279 && craftSlotId3[5] == 279 && craftSlotId3[6] == 279 && craftSlotId3[8] == 279))
            {
                craftItemId3 = 343;
                craftItemAm3 = 429;
                craftItemSt3 = false;
            }
        break;

        // end


        // golden

        // helmet
        case 1400:
            if((craftSlotId3[0] == 280 && craftSlotId3[1] == 280 && craftSlotId3[2] == 280 && craftSlotId3[3] == 280 && craftSlotId3[5] == 280) || (craftSlotId3[3] == 280 && craftSlotId3[4] == 280 && craftSlotId3[5] == 280 && craftSlotId3[6] == 280 && craftSlotId3[8] == 280))
            {
                craftItemId3 = 344;
                craftItemAm3 = 77;
                craftItemSt3 = false;
            }
        break;

        // chestplate
        case 2240:
            if(craftSlotId3[0] == 280 && craftSlotId3[2] == 280 && craftSlotId3[3] == 280 && craftSlotId3[4] == 280 && craftSlotId3[5] == 280 && craftSlotId3[6] == 280 && craftSlotId3[7] == 280 && craftSlotId3[8] == 280)
            {
                craftItemId3 = 345;
                craftItemAm3 = 112;
                craftItemSt3 = false;
            }
        break;

        // leggings
        case 1960:
            if(craftSlotId3[0] == 280 && craftSlotId3[1] == 280 && craftSlotId3[2] == 280 && craftSlotId3[3] == 280 && craftSlotId3[5] == 280 && craftSlotId3[6] == 280 && craftSlotId3[8] == 280)
            {
                craftItemId3 = 346;
                craftItemAm3 = 105;
                craftItemSt3 = false;
            }
        break;

        // boots
        case 1120:
            if((craftSlotId3[0] == 280 && craftSlotId3[2] == 280 && craftSlotId3[3] == 280 && craftSlotId3[5] == 280) || (craftSlotId3[3] == 280 && craftSlotId3[5] == 280 && craftSlotId3[6] == 280 && craftSlotId3[8] == 280))
            {
                craftItemId3 = 347;
                craftItemAm3 = 91;
                craftItemSt3 = false;
            }
        break;

        // end
        /// Armor crafts end

        case 2799: //melon block
            if(craftSlotId3[0] == 311 && craftSlotId3[1] == 311 && craftSlotId3[2] == 311 && craftSlotId3[3] == 311 && craftSlotId3[4] == 311 && craftSlotId3[5] == 311 && craftSlotId3[6] == 311 && craftSlotId3[7] == 311 && craftSlotId3[8] == 311)
            {
                craftItemId3 = 81;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;


        case 2862: //lapis block
            if(craftSlotId3[0] == 318 && craftSlotId3[1] == 318 && craftSlotId3[2] == 318 && craftSlotId3[3] == 318 && craftSlotId3[4] == 318 && craftSlotId3[5] == 318 && craftSlotId3[6] == 318 && craftSlotId3[7] == 318 && craftSlotId3[8] == 318)
            {
                craftItemId3 = 101;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;


        case 101: //lapis block to items
            if(craftSlotId3[0] == 101 || craftSlotId3[1] == 101 || craftSlotId3[2] == 101 || craftSlotId3[3] == 101 || craftSlotId3[4] == 101 || craftSlotId3[5] == 101 || craftSlotId3[6] == 101 || craftSlotId3[7] == 101 || craftSlotId3[8] == 101)
            {
                craftItemId3 = 318;
                craftItemSt3 = 1;
                craftItemAm3 = 9;
            }
        break;

        case 2502: //iron block
            if(craftSlotId3[0] == 278 && craftSlotId3[1] == 278 && craftSlotId3[2] == 278 && craftSlotId3[3] == 278 && craftSlotId3[4] == 278 && craftSlotId3[5] == 278 && craftSlotId3[6] == 278 && craftSlotId3[7] == 278 && craftSlotId3[8] == 278)
            {
                craftItemId3 = 66;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;


        case 66: //iron block to bars
            if(craftSlotId3[0] == 66 || craftSlotId3[1] == 66 || craftSlotId3[2] == 66 || craftSlotId3[3] == 66 || craftSlotId3[4] == 66 || craftSlotId3[5] == 66 || craftSlotId3[6] == 66 || craftSlotId3[7] == 66 || craftSlotId3[8] == 66)
            {
                craftItemId3 = 278;
                craftItemSt3 = 1;
                craftItemAm3 = 9;
            }
        break;

        case 2520: //gold block
            if(craftSlotId3[0] == 280 && craftSlotId3[1] == 280 && craftSlotId3[2] == 280 && craftSlotId3[3] == 280 && craftSlotId3[4] == 280 && craftSlotId3[5] == 280 && craftSlotId3[6] == 280 && craftSlotId3[7] == 280 && craftSlotId3[8] == 280)
            {
                craftItemId3 = 65;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 65: //gold block to bars
            if(craftSlotId3[0] == 65 || craftSlotId3[1] == 65 || craftSlotId3[2] == 65 || craftSlotId3[3] == 65 || craftSlotId3[4] == 65 || craftSlotId3[5] == 65 || craftSlotId3[6] == 65 || craftSlotId3[7] == 65 || craftSlotId3[8] == 65)
            {
                craftItemId3 = 280;
                craftItemSt3 = 1;
                craftItemAm3 = 9;
            }
        break;

        case 2511: //diamond block
            if(craftSlotId3[0] == 279 && craftSlotId3[1] == 279 && craftSlotId3[2] == 279 && craftSlotId3[3] == 279 && craftSlotId3[4] == 279 && craftSlotId3[5] == 279 && craftSlotId3[6] == 279 && craftSlotId3[7] == 279 && craftSlotId3[8] == 279)
            {
                craftItemId3 = 64;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 64: //diamond block to diamonds
            if(craftSlotId3[0] == 64 || craftSlotId3[1] == 64 || craftSlotId3[2] == 64 || craftSlotId3[3] == 64 || craftSlotId3[4] == 64 || craftSlotId3[5] == 64 || craftSlotId3[6] == 64 || craftSlotId3[7] == 64 || craftSlotId3[8] == 64)
            {
                craftItemId3 = 279;
                craftItemSt3 = 1;
                craftItemAm3 = 9;
            }
        break;

        case 2583: //hay bale
            if(craftSlotId3[0] == Wheat::getID() && craftSlotId3[1] == Wheat::getID() && craftSlotId3[2] == Wheat::getID() && craftSlotId3[3] == Wheat::getID() && craftSlotId3[4] == Wheat::getID() &&
               craftSlotId3[5] == Wheat::getID() && craftSlotId3[6] == Wheat::getID() && craftSlotId3[7] == Wheat::getID() && craftSlotId3[8] == Wheat::getID())
            {
                craftItemId3 = HayBale::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 213: //hay bale to wheat
            if(craftSlotId3[0] == HayBale::getID() || craftSlotId3[1] == HayBale::getID() || craftSlotId3[2] == HayBale::getID() || craftSlotId3[3] == HayBale::getID() || craftSlotId3[4] == HayBale::getID() ||
               craftSlotId3[5] == HayBale::getID() || craftSlotId3[6] == HayBale::getID() || craftSlotId3[7] == HayBale::getID() || craftSlotId3[8] == HayBale::getID())
            {
                craftItemId3 = Wheat::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 9;
            }
        break;

        case 2493: //coal to coal block
            if(craftSlotId3[0] == Coal::getID() && craftSlotId3[1] == Coal::getID() && craftSlotId3[2] == Coal::getID() && craftSlotId3[3] == Coal::getID() && craftSlotId3[4] == Coal::getID() &&
               craftSlotId3[5] == Coal::getID() && craftSlotId3[6] == Coal::getID() && craftSlotId3[7] == Coal::getID() && craftSlotId3[8] == Coal::getID())
            {
                craftItemId3 = CoalBlock::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 214: //coal block to coal
            if(craftSlotId3[0] == CoalBlock::getID() || craftSlotId3[1] == CoalBlock::getID() || craftSlotId3[2] == CoalBlock::getID() || craftSlotId3[3] == CoalBlock::getID() || craftSlotId3[4] == CoalBlock::getID() ||
               craftSlotId3[5] == CoalBlock::getID() || craftSlotId3[6] == CoalBlock::getID() || craftSlotId3[7] == CoalBlock::getID() || craftSlotId3[8] == CoalBlock::getID())
            {
                craftItemId3 = Coal::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 9;
            }
        break;

        case 3177: //redstone to redstone block
            if(craftSlotId3[0] == Redstone::getID() && craftSlotId3[1] == Redstone::getID() && craftSlotId3[2] == Redstone::getID() && craftSlotId3[3] == Redstone::getID() && craftSlotId3[4] == Redstone::getID() &&
               craftSlotId3[5] == Redstone::getID() && craftSlotId3[6] == Redstone::getID() && craftSlotId3[7] == Redstone::getID() && craftSlotId3[8] == Redstone::getID())
            {
                craftItemId3 = RedstoneBlock::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 215: //redstonee block to redstone
            if(craftSlotId3[0] == RedstoneBlock::getID() || craftSlotId3[1] == RedstoneBlock::getID() || craftSlotId3[2] == RedstoneBlock::getID() || craftSlotId3[3] == RedstoneBlock::getID() || craftSlotId3[4] == RedstoneBlock::getID() ||
               craftSlotId3[5] == RedstoneBlock::getID() || craftSlotId3[6] == RedstoneBlock::getID() || craftSlotId3[7] == RedstoneBlock::getID() || craftSlotId3[8] == RedstoneBlock::getID())
            {
                craftItemId3 = Redstone::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 9;
            }
        break;

        case 1932: //ladder
            if(craftSlotId3[0] == 276 && craftSlotId3[2] == 276 && craftSlotId3[3] == 276 && craftSlotId3[4] == 276 && craftSlotId3[5] == 276 && craftSlotId3[6] == 276 && craftSlotId3[8] == 276)
            {
                craftItemId3 = 307;
                craftItemSt3 = 1;
                craftItemAm3 = 3;
            }
        break;

        case 272: //chest (oak)
            if(craftSlotId3[1] == 34 && craftSlotId3[2] == 34 && craftSlotId3[3] == 34 && craftSlotId3[5] == 34 && craftSlotId3[7] == 34 && craftSlotId3[0] == 34 && craftSlotId3[6] == 34 && craftSlotId3[8] == 34)
            {
                craftItemId3 = ChestBlock3::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1240: //chest (birch)
            if(craftSlotId3[1] == 155 && craftSlotId3[2] == 155 && craftSlotId3[3] == 155 && craftSlotId3[5] == 155 && craftSlotId3[7] == 155 && craftSlotId3[0] == 155 && craftSlotId3[6] == 155 && craftSlotId3[8] == 155)
            {
                craftItemId3 = ChestBlock3::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1248: //chest (spruce)
            if(craftSlotId3[1] == 156 && craftSlotId3[2] == 156 && craftSlotId3[3] == 156 && craftSlotId3[5] == 156 && craftSlotId3[7] == 156 && craftSlotId3[0] == 156 && craftSlotId3[6] == 156 && craftSlotId3[8] == 156)
            {
                craftItemId3 = ChestBlock3::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 288: //furnace
            if(craftSlotId3[1] == 36 && craftSlotId3[2] == 36 && craftSlotId3[3] == 36 && craftSlotId3[5] == 36 && craftSlotId3[7] == 36 && craftSlotId3[0] == 36 && craftSlotId3[6] == 36 && craftSlotId3[8] == 36)
            {
                craftItemId3 = FurnaceOff::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 861: //bread
            if((craftSlotId3[0] == 287 && craftSlotId3[1] == 287 && craftSlotId3[2] == 287) || (craftSlotId3[3] == 287 && craftSlotId3[4] == 287 && craftSlotId3[5] == 287) || (craftSlotId3[6] == 287 && craftSlotId3[7] == 287 && craftSlotId3[8] == 287))
            {
                craftItemId3 = 288;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 888: //cookies
            if((craftSlotId3[0] == 287 && craftSlotId3[1] == 314 && craftSlotId3[2] == 287) || (craftSlotId3[3] == 287 && craftSlotId3[4] == 314 && craftSlotId3[5] == 287) || (craftSlotId3[6] == 287 && craftSlotId3[7] == 314 && craftSlotId3[8] == 287))
            {
                craftItemId3 = 315;
                craftItemSt3 = 1;
                craftItemAm3 = 8;
            }
        break;

        case 2524: //golden apple
            if(craftSlotId3[0] == 280 && craftSlotId3[1] == 280 && craftSlotId3[2] == 280 && craftSlotId3[3] == 280 && craftSlotId3[4] == 284 && craftSlotId3[5] == 280 && craftSlotId3[6] == 280 && craftSlotId3[7] == 280 && craftSlotId3[8] == 280)
            {
                craftItemId3 = 285;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 879: //paper
            if((craftSlotId3[0] == 293 && craftSlotId3[1] == 293 && craftSlotId3[2] == 293) || (craftSlotId3[3] == 293 && craftSlotId3[4] == 293 && craftSlotId3[5] == 293) || (craftSlotId3[6] == 293 && craftSlotId3[7] == 293 && craftSlotId3[8] == 293))
            {
                craftItemId3 = 297;
                craftItemSt3 = 1;
                craftItemAm3 = 3;
            }
        break;

        case 891: //book
            if((craftSlotId3[0] == 297 && craftSlotId3[3] == 297 && craftSlotId3[6] == 297) || (craftSlotId3[1] == 297 && craftSlotId3[4] == 297 && craftSlotId3[7] == 297) || (craftSlotId3[2] == 297 && craftSlotId3[5] == 297 && craftSlotId3[8] == 297))
            {
                craftItemId3 = 298;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1098: //book shelf
            if(craftSlotId3[0] == 34 && craftSlotId3[1] == 34 && craftSlotId3[2] == 34 && craftSlotId3[3] == 298 && craftSlotId3[4] == 298 && craftSlotId3[5] == 298 && craftSlotId3[6] == 34 && craftSlotId3[7] == 34 && craftSlotId3[8] == 34)
            {
                craftItemId3 = 35;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1734: //book shelf (birch)
            if(craftSlotId3[0] == 155 && craftSlotId3[1] == 155 && craftSlotId3[2] == 155 && craftSlotId3[3] == 298 && craftSlotId3[4] == 298 && craftSlotId3[5] == 298 && craftSlotId3[6] == 155 && craftSlotId3[7] == 155 && craftSlotId3[8] == 155)
            {
                craftItemId3 = 35;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1740: //book shelf (birch)
            if(craftSlotId3[0] == 156 && craftSlotId3[1] == 156 && craftSlotId3[2] == 156 && craftSlotId3[3] == 298 && craftSlotId3[4] == 298 && craftSlotId3[5] == 298 && craftSlotId3[6] == 156 && craftSlotId3[7] == 156 && craftSlotId3[8] == 156)
            {
                craftItemId3 = 35;
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 1473: //clock
            if(craftSlotId3[1] == 280 && craftSlotId3[3] == 280 && craftSlotId3[4] == Redstone::getID() && craftSlotId3[5] == 280 && craftSlotId3[7] == 280)
            {
                craftItemId3 = Clock::getID();
                craftItemSt3 = 0;
                craftItemAm3 = 1;
            }
        break;

        case 1465: //compass
            if(craftSlotId3[1] == IronBar::getID() && craftSlotId3[3] == IronBar::getID() && craftSlotId3[4] == Redstone::getID() && craftSlotId3[5] == IronBar::getID() && craftSlotId3[7] == IronBar::getID())
            {
                craftItemId3 = Compass::getID();
                craftItemSt3 = 0;
                craftItemAm3 = 1;
            }
        break;

        case 846: //watering can
            if(craftSlotId3[8] == 290 && craftSlotId3[4] == 278 && craftSlotId3[0] == 278)
            {
                craftItemId3 = 304;
                craftItemSt3 = 0;
                craftItemAm3 = 0;
            }
        break;

        case 2531: //item frame
            if(craftSlotId3[1] == Stick::getID() && craftSlotId3[2] == Stick::getID() && craftSlotId3[3] == Stick::getID() &&
               craftSlotId3[5] == Stick::getID() && craftSlotId3[7] == Stick::getID() && craftSlotId3[0] == Stick::getID() &&
               craftSlotId3[6] == Stick::getID() && craftSlotId3[8] == Stick::getID() && craftSlotId3[4] == Leather::getID())
            {
                craftItemId3 = ItemFrameItem::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 629: //flint and steel
            if((craftSlotId3[0] == IronBar::getID() && craftSlotId3[4] == Flint::getID()) ||
               (craftSlotId3[1] == IronBar::getID() && craftSlotId3[5] == Flint::getID()) ||
               (craftSlotId3[3] == IronBar::getID() && craftSlotId3[7] == Flint::getID()) ||
               (craftSlotId3[4] == IronBar::getID() && craftSlotId3[8] == Flint::getID()))
            {
                craftItemId3 = FlintAndSteel::getID();
                craftItemSt3 = 0;
                craftItemAm3 = 65;
            }
        break;

        case 1788: //TNT
            if(craftSlotId3[1] == SandBlock::getID() && craftSlotId3[2] == Gunpowder::getID() && craftSlotId3[3] == SandBlock::getID() &&
               craftSlotId3[5] == SandBlock::getID() && craftSlotId3[7] == SandBlock::getID() && craftSlotId3[0] == Gunpowder::getID() &&
               craftSlotId3[6] == Gunpowder::getID() && craftSlotId3[8] == Gunpowder::getID() && craftSlotId3[4] == Gunpowder::getID())
            {
                craftItemId3 = TNTBlock::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 1;
            }
        break;

        case 75: //pumpkin to pumpkin seeds
            // Every slot joined with ||, matching the 2x2 grid's version of this same recipe above. Slots 2
            // and 3 were joined with &&, which made a lone pumpkin in slot 2 match nothing -- the recipe
            // silently failed for that one position.
            if(craftSlotId3[0] == 75 || craftSlotId3[1] == 75 || craftSlotId3[2] == 75 || craftSlotId3[3] == 75 || craftSlotId3[4] == 75 || craftSlotId3[5] == 75 || craftSlotId3[6] == 75 || craftSlotId3[7] == 75 || craftSlotId3[8] == 75)
            {
                craftItemId3 = PumpkinSeeds::getID();
                craftItemSt3 = 1;
                craftItemAm3 = 4;
            }
        break;

        default:
            craftItemId3 = -1;
            craftItemAm3 = -1;
            craftItemSt3 = 0;
        break;

        }
    }
}

void StatePlay::HandleEvents(StateManager* sManager)
{
    //update delta time
    dt = mTimer.GetDeltaTime();
    if(dt > 0.1f)
        dt = 0.1f;

    canFly = false;

    cameraSpeed = 2.0f * dt;
    cameraMoveSpeed = 4.00f * dt;

    //free memory counter
    freeMemoryTimer += dt;
    if(menuState != 0)
    {
        if(freeMemoryTimer > 3.0f)
        {
            freeMemory = mSystemMgr->ramAvailable();
            freeMemoryTimer = 0.0f;
        }
    }
    else
    {
        if(freeMemoryTimer > 15.0f)
        {
            freeMemory = mSystemMgr->ramAvailable();
            freeMemoryTimer = 0.0f;
        }
    }

    //update input
    mSystemMgr->InputUpdate();

    // While the debug menu is up it owns the input. The world is held frozen and the only keys that
    // mean anything are the menu's own, so returning here keeps a press of the use button from
    // reaching the block and item paths, and keeps the stick from re-opening the menu on top of
    // itself. HandleEvents is safe to leave early: the chat and the sounds are ticked in Update.
    if(debugMenu.IsOpen() == true)
    {
        fppCam->CancelLookEase();
        return;
    }

    // A menu, the inventory, or a dead player skips the look block below. Drop the coast so
    // closing the menu does not finish the turn that was in progress when it opened.
    if(mWorld->HP <= 0 || sleepTime > 0.0f || menuState != 0 || invEn || craft3xEn || chestEn || furnaceEn)
        fppCam->CancelLookEase();

    if(mWorld->HP > 0 && sleepTime <= 0.0f)
    {
        sneaking = false;
        if(menuState == 0 && invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false)
        {
            bool wantSneak = keyHold(InputHelper::Instance()->getButtonToAction(InputHelper::ACTION_SNEAK));
            if(wantSneak)
                sneaking = true;
            else if(mWorld->playerGrowth < 1.60f)
            {
                Vector3 standHead = playerPosition + Vector3(0.0f, 0.35f, 0.0f);
                sneaking = mWorld->SolidAtPointForPlayer(standHead);
            }
        }
        else if(mWorld->playerGrowth < 1.60f)
        {
            Vector3 standHead = playerPosition + Vector3(0.0f, 0.35f, 0.0f);
            sneaking = mWorld->SolidAtPointForPlayer(standHead);
        }
        mWorld->playerGrowth = sneaking ? 1.50f : 1.65f;
        if(WorldRunning())//game state
        {
            if(invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false)
            {
                if(tickShowSlotName > -0.1f)
                {
                    tickShowSlotName -= dt;
                }

                // L+R in either order opens inventory and cancels a break.
                if(sleepTime == 0.0f &&
                   (keyHold(InputHelper::Instance()->getButtonToAction(13)) || keyPressed(InputHelper::Instance()->getButtonToAction(13))) &&
                   (keyHold(InputHelper::Instance()->getButtonToAction(14)) || keyPressed(InputHelper::Instance()->getButtonToAction(14))) &&
                   (keyPressed(InputHelper::Instance()->getButtonToAction(13)) || keyPressed(InputHelper::Instance()->getButtonToAction(14))))
                {
                    invEn = true;
                    dT = 0;
                    dET = 0;
                    chestId = -1;
                    dStd = -1;
                    startDt = false;
                    return;
                }

                if(sleepTime == 0.0f && keyPressed(InputHelper::Instance()->getButtonToAction(InputHelper::ACTION_CHAT)))
                {
                    int chatBtn = InputHelper::Instance()->getButtonToAction(InputHelper::ACTION_CHAT);
                    int placeBtn = InputHelper::Instance()->getButtonToAction(14);
                    if(!(chatBtn == 0 && (keyHold(placeBtn) || keyPressed(placeBtn))))
                    {
                        if(chatTapTimer > 0.0f)
                        {
                            // Second tap inside the window: that was a double tap, so the held-back
                            // chat open is dropped and the view cycles instead.
                            chatTapTimer = 0.0f;
                            chatTapPending = false;
                            thirdPersonView = (thirdPersonView + 1) % 3;
                        }
                        else
                        {
                            // First tap. Do not open chat yet -- it would take the second tap.
                            chatTapTimer = CHAT_DOUBLE_TAP_WINDOW;
                            chatTapPending = true;
                        }
                    }
                }

                //switch right
                if(keyPressed(InputHelper::Instance()->getButtonToAction(9)))
                {
                    if (mWorld->invId[27+barPosition] != -1)
                    {
                        changeY = -0.16f;
                        anim[0] = 1;
                    }
                    barPosition != 8 ? barPosition ++ : barPosition = 0;

                    selectSprite->SetPosition(108.0f + (11.0f + barPosition * 20.0f) * (264.0f / 182.0f),230);

                    tickShowSlotName = 1.25f;

                    startDt = 0;
                    dT = 0;
                    dET = 0;
                    dStd = -1;
                }

                //switch left
                if(keyPressed(InputHelper::Instance()->getButtonToAction(8)))
                {
                    if (mWorld->invId[27+barPosition] != -1)
                    {
                        changeY = -0.16f;
                        anim[0] = 1;
                    }
                    barPosition != 0 ? barPosition -- : barPosition = 8;

                    selectSprite->SetPosition(108.0f + (11.0f + barPosition * 20.0f) * (264.0f / 182.0f),230);

                    tickShowSlotName = 1.25f;

                    startDt = 0;
                    dT = 0;
                    dET = 0;
                    dStd = -1;
                }

                // drop 1 item
                if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
                {
                    if(keyPressed(InputHelper::Instance()->getButtonToAction(14)))
                    {
                        if(mWorld->invId[27+barPosition] > 0)
                        {
                            Vector3 dropVelocity = Vector3(0,0,0);

                            dropVelocity.x = (cosf((fppCam->horAngle/180.0f)*PI))*4.0f;
                            dropVelocity.y = 2.0f;
                            dropVelocity.z = (sinf((fppCam->horAngle/180.0f)*PI))*4.0f;

                            if(mWorld->invSt[27+barPosition] == true)
                            {
                                if(mWorld->invAm[27+barPosition] > 0)
                                {
                                    mWorld->invAm[27+barPosition] -= 1;
                                    mWorld->DropThisNoAlign(mWorld->invId[27+barPosition],1,mWorld->invSt[27+barPosition],Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y-0.25f,fppCam->m_vPosition.z),dropVelocity);

                                    if(mWorld->invAm[27+barPosition] == 0)
                                    {
                                        mWorld->invAm[27+barPosition] = -1;
                                        mWorld->invSt[27+barPosition] = 0;
                                        mWorld->invId[27+barPosition] = -1;
                                    }
                                }
                            }
                            else
                            {
                                mWorld->DropThisNoAlign(mWorld->invId[27+barPosition],mWorld->invAm[27+barPosition],mWorld->invSt[27+barPosition],Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y-0.25f,fppCam->m_vPosition.z),dropVelocity);

                                mWorld->invAm[27+barPosition] = -1;
                                mWorld->invSt[27+barPosition] = 0;
                                mWorld->invId[27+barPosition] = -1;
                            }
                        }
                        return;
                    }
                }

                //open menu
                if(sleepTime == 0.0f)
                {
                    if(keyPressed(InputHelper::Instance()->getButtonToAction(15)))
                    {
                        menuState = 1;
                        menuOptions = false;
                        optionsMenuPos = 0;
                    }
                }

                //camera look — yaw/pitch from angles so turning can't drag pitch
                {
                    int bUp = InputHelper::Instance()->getButtonToAction(4);
                    int bDown = InputHelper::Instance()->getButtonToAction(5);
                    int bLeft = InputHelper::Instance()->getButtonToAction(6);
                    int bRight = InputHelper::Instance()->getButtonToAction(7);
                    bool analogLook = (bUp >= 11 && bUp <= 14) || (bDown >= 11 && bDown <= 14) || (bLeft >= 11 && bLeft <= 14) || (bRight >= 11 && bRight <= 14);
                    float lookYaw = 0.0f;
                    float lookPitch = 0.0f;
                    if(analogLook)
                    {
                        float ax = mSystemMgr->GetAnalogX();
                        float ay = mSystemMgr->GetAnalogY();
                        float mag = sqrtf(ax * ax + ay * ay);
                        float dead = 0.18f;
                        if(mag > dead)
                        {
                            float t = (mag - dead) / (1.0f - dead);
                            if(t > 1.0f)
                                t = 1.0f;
                            ax = ax / mag * t;
                            ay = ay / mag * t;
                            lookYaw = ax * 2.0f;
                            lookPitch = ay * 2.0f;
                        }
                    }
                    else
                    {
                        if(keyHold(bUp))
                            lookPitch += 2.0f;
                        if(keyHold(bDown))
                            lookPitch -= 2.0f;
                        if(keyHold(bRight))
                            lookYaw += 2.0f;
                        if(keyHold(bLeft))
                            lookYaw -= 2.0f;
                    }
                    // Same dt as movement, with no tighter clamp. The rate eases toward the stick
                    // inside ApplyLook; the angle still advances by rate * real elapsed time, which is
                    // what keeps a long frame from turning a fixed number of degrees.
                    lookDt = dt;
                    fppCam->ApplyLook(lookYaw, lookPitch, dt);
                    if(fppCam->lookRatePitch != 0.0f || fppCam->lookRateYaw != 0.0f)
                    {
                        if(startDt == true)
                            dStd = -1;

                        // Diagnostic: the turn rate as the console actually produces it, reported once a
                        // second. A held button settles near 114 deg/s. The first fraction of a second
                        // is the ease-in, so a tap averages lower.
                        static int sTurnFrames = 0;
                        static float sTurnYawDeg = 0.0f;
                        static float sTurnWorstDt = 0.0f;
                        static float sTurnAccum = 0.0f;
                        if(fppCam->lookRateYaw != 0.0f)
                            sTurnYawDeg += fabsf(fppCam->lookRateYaw * lookDt) * 180.0f / 3.14159265f;
                        sTurnFrames++;
                        if(dt > sTurnWorstDt)
                            sTurnWorstDt = dt;
                        sTurnAccum += dt;
                        if(sTurnAccum >= 1.0f)
                        {
                            // MC_DEBUG_LOG, not Logger directly: this is a once-a-second card write, and
                            // Logger flushes the memory stick on every line. In a release build that
                            // hitch lands on every second the camera is turning.
                            if(sTurnYawDeg > 0.0f)
                                MC_DEBUG_LOG(
                                    "turn: frames=%d yawDegPerSec=%d worstFrameMs=%d\n",
                                    sTurnFrames, (int)(sTurnYawDeg / sTurnAccum), (int)(sTurnWorstDt * 1000.0f));
                            sTurnFrames = 0;
                            sTurnYawDeg = 0.0f;
                            sTurnWorstDt = 0.0f;
                            sTurnAccum = 0.0f;
                        }
                    }
                    if(lookPitch > 0.0f)
                        shift_y > -0.06f ? shift_y -= 0.24f*dt : shift_y = -0.06f;
                    else if(lookPitch < 0.0f)
                        shift_y < 0.06f ? shift_y += 0.24f*dt : shift_y = 0.06f;
                    else
                    {
                        if(shift_y < 0.0f)
                            shift_y += 0.24f*dt;
                        else if(shift_y > 0.0f)
                            shift_y -= 0.24f*dt;
                        if(fabsf(shift_y) <= 0.001f)
                            shift_y = 0.0f;
                    }
                    if(lookYaw < 0.0f)
                        shift_x > -0.06f ? shift_x -= 0.24f*dt : shift_x = -0.06f;
                    else if(lookYaw > 0.0f)
                        shift_x < 0.06f ? shift_x += 0.24f*dt : shift_x = 0.06f;
                    else
                    {
                        if(shift_x < 0.0f)
                            shift_x += 0.24f*dt;
                        else if(shift_x > 0.0f)
                            shift_x -= 0.24f*dt;
                        if(fabsf(shift_x) <= 0.001f)
                            shift_x = 0.0f;
                    }
                }

                //camera moving
                moving = false;
                moveForward = false;
                bool diagonleMoving = false;
                if(sneaking)
                    cameraMoveSpeed *= 0.3f;

                // Sprint, on a double tap of Forward -- Java's EntityPlayerSP rule, and the same tap
                // pattern the creative sprint-fly uses (see StatePlayCreative). The taps have to be
                // told apart inside a 7-tick window with no button to spare, and they can only be
                // told apart after the fact, so the first tap arms the window and the second inside
                // it starts the sprint. Releasing Forward ends it, which is also Java's rule, and
                // that is why the release cancels unconditionally rather than remembering a state.
                //
                // Java's other cancellation conditions are honoured where this port can see them:
                // sneaking, and a food level of 6 or less. It also cancels on hitting a wall
                // (isCollidedHorizontally); this port has no such flag, and the practical effect
                // there is that you keep the sprint animation while pressed against a block, which
                // costs nothing because you are not moving and so are not paying exhaustion for it.
                if(sleepTime == 0.0f)
                {
                    bool sprintFwdHeld = keyHold(InputHelper::Instance()->getButtonToAction(0));
                    if(sprintTapTimer > 0.0f)
                        sprintTapTimer -= dt;

                    if(sprintFwdHeld && !sprintFwdWasHeld)
                    {
                        if(sprintTapArm && sprintTapTimer > SPRINT_TAP_MIN_GAP)
                        {
                            sprinting = true;
                            sprintTapArm = false;
                            sprintTapTimer = 0.0f;
                        }
                        else
                        {
                            sprintTapPending = true;
                            sprintTapArm = false;
                            sprintTapTimer = SPRINT_TAP_WINDOW;
                        }
                    }
                    if(!sprintFwdHeld && sprintFwdWasHeld)
                    {
                        if(sprintTapPending && sprintTapTimer > SPRINT_TAP_MIN_GAP)
                            sprintTapArm = true;
                        sprintTapPending = false;
                        sprinting = false;
                    }
                    sprintFwdWasHeld = sprintFwdHeld;
                    if(!sprintFwdHeld && sprintTapTimer <= 0.0f)
                        sprintTapArm = false;

                    // Java will not start or keep a sprint on an empty bar. Checked after the taps so
                    // a sprint that runs the bar dry stops at that moment rather than at the next tap.
                    if(sneaking || mWorld->HG <= 6.0f)
                        sprinting = false;

                    if(sprinting)
                        cameraMoveSpeed *= SPRINT_SPEED_MULT;
                }
                else
                {
                    sprinting = false;
                    sprintTapTimer = 0.0f;
                    sprintTapArm = false;
                    sprintTapPending = false;
                    sprintFwdWasHeld = false;
                }

                if(sleepTime == 0.0f)
                {
                    //move forward
                    if(keyHold(InputHelper::Instance()->getButtonToAction(0)))
                    {
                        float slowdown = 0.8f;
                        float slowdown2 = 1.0f;
                        if (startDt == true)
                        {
                            //startDt = false;
                            dStd = -1;
                        }
                        moving = true;
                        moveForward = true;

                        if(walkingOnGround == false)
                        {
                            slowdown -= 0.2f;
                            slowdown2 -= 0.2f;
                        }
                        if(slowdown < 0.72)
                        {
                            slowdown = 0.72f;
                        }
						/*/Sprinting forward
						if(keyPressed(InputHelper::Instance()->getButtonToAction(0)) || keyHold(InputHelper::Instance()->getButtonToAction(0)))
						{
							moving = true;
							moveForward = true;

							if(walkingOnGround == false)
							{
								slowdown -= -1.0f;
								slowdown2 -= -1.0f;
							}
							if(slowdown < -1.25f)
							{
								slowdown = -1.25f;
							}
							if(walkingOnGround == true)
							{
								slowdown -= -0.4f;
								slowdown2 -= -0.4f;
							}
							if(slowdown < -0.9f)
							{
								slowdown = -0.9f;
							}
						}*/
                        //move right at the same time
                        if(keyHold(InputHelper::Instance()->getButtonToAction(3)))
                        {
                            diagonleMoving = true;
                            fppCam->StrafePhysic(cameraMoveSpeed*slowdown);

                            if(canFly)
                                fppCam->MovePhysicNoY(cameraMoveSpeed*slowdown);
                            else
                                fppCam->MovePhysicNoY(cameraMoveSpeed*slowdown);
                        }

                        //move left at the same time
                        if(keyHold(InputHelper::Instance()->getButtonToAction(2)))
                        {
                            diagonleMoving = true;
                            fppCam->StrafePhysic(-cameraMoveSpeed*slowdown);

                            if(canFly)
                                fppCam->MovePhysicNoY(cameraMoveSpeed*slowdown);
                            else
                                fppCam->MovePhysicNoY(cameraMoveSpeed*slowdown);
                        }

                        if(!diagonleMoving)
                        {
                            if(canFly)
                                fppCam->MovePhysicNoY(cameraMoveSpeed*slowdown2);
                            else
                                fppCam->MovePhysicNoY(cameraMoveSpeed*slowdown2);
                        }

                        // The walking cost is no longer subtracted here. Hunger now runs on Java's
                        // exhaustion accumulator, fed once per frame from the distance actually
                        // moved in Update (see EXHAUST_WALK / EXHAUST_SPRINT), which is the only
                        // place that knows whether the player was sprinting, wading or on the ground.
                    }

                    //move back
                    if(keyHold(InputHelper::Instance()->getButtonToAction(1)))
                    {
                        float slowdown = 0.8f;
                        float slowdown2 = 1.0f;
                        if (startDt == true)
                        {
                            //startDt = false;
                            dStd = -1;
                        }
                        moving = true;
                        moveForward = false;

                        if(walkingOnGround == false)
                        {
                            slowdown -= 0.2f;
                            slowdown2 -= 0.2f;
                        }
                        if(slowdown < 0.72)
                        {
                            slowdown = 0.72f;
                        }
                        //move right at the same time
                        if(keyHold(InputHelper::Instance()->getButtonToAction(3)))
                        {
                            diagonleMoving = true;
                            fppCam->StrafePhysic(cameraMoveSpeed*slowdown);

                            if(canFly)
                                fppCam->MovePhysicNoY(-cameraMoveSpeed*slowdown);
                            else
                                fppCam->MovePhysicNoY(-cameraMoveSpeed*slowdown);
                        }

                        //move left at the same time
                        if(keyHold(InputHelper::Instance()->getButtonToAction(2)))
                        {
                            diagonleMoving = true;
                            fppCam->StrafePhysic(-cameraMoveSpeed*slowdown);

                            if(canFly)
                                fppCam->MovePhysicNoY(-cameraMoveSpeed*slowdown);
                            else
                                fppCam->MovePhysicNoY(-cameraMoveSpeed*slowdown);
                        }

                        if(!diagonleMoving)
                        {
                            if(canFly)
                                fppCam->MovePhysicNoY(-cameraMoveSpeed*slowdown2);
                            else
                                fppCam->MovePhysicNoY(-cameraMoveSpeed*slowdown2);
                        }
                    }

                    //move right
                    if(keyHold(InputHelper::Instance()->getButtonToAction(3)))
                    {
                        float slowdown = 1.0f;

                        if(walkingOnGround == false)
                        {
                            slowdown -= 0.2f;
                        }

                        if (startDt == true)
                        {
                            //startDt = false;
                            dStd = -1;
                        }
                        if(!diagonleMoving)
                        {
                            fppCam->StrafePhysic(cameraMoveSpeed*slowdown);
                            moving = true;
                        }
                    }

                    //move left
                    if(keyHold(InputHelper::Instance()->getButtonToAction(2)))
                    {
                        float slowdown = 1.0f;

                        if(walkingOnGround == false)
                        {
                            slowdown -= 0.2f;
                        }

                        if (startDt == true)
                        {
                            //startDt = false;
                            dStd = -1;
                        }
                        if(!diagonleMoving)
                        {
                            fppCam->StrafePhysic(-cameraMoveSpeed*slowdown);
                            moving = true;
                        }
                    }

                    // jumping/swiming (survival — no fly toggle)
                    canFly = false;
                    if(keyHold(InputHelper::Instance()->getButtonToAction(12)))
                    {
                        dStd = -1;
                        if (headInWater || headInLava || mWorld->BlockAtPoint(Vector3(playerPosition.x,playerPosition.y-0.65f,playerPosition.z)) == WaterBlock::getID() || mWorld->BlockAtPoint(Vector3(playerPosition.x,playerPosition.y-0.65f,playerPosition.z)) == Lava::getID())
                        {
                            if(walkingOnGround == true)
                            {
                                jumping = true;
                            }
                            else
                            {
                                playerVelocity.y = 0.525 * JUMPVELOCITY;
                            }
                        }
                        if (headInWater == false && headInLava == false && walkingOnGround == false && (footInWater == true || footInLava == true))	//Your above he water, so Jump out
                        {
                            playerVelocity.y = 1.2 * JUMPVELOCITY;
                            walkingOnGround = false;
                            if(mWorld->HG > 0.0001)
                            {
                                mWorld->HG -= 0.00001;
                            }
                        }
                        if(walkingOnGround == true)
                        {
                            if(headInWater == false && headInLava == false)
                            {
                                jumping = true;
                            }
                        }
                        else
                        {
                            jumping = false;
                        }
                    }
                }
            //add cube
            if(keyPressed(InputHelper::Instance()->getButtonToAction(14)))
            {
                // The debug stick, before anything else in this block. It is a held item with no
                // other use, so letting a block's own action see it first would mean a block could
                // swallow the click; handled here, holding the stick can never place, break or open
                // anything by accident.
                if(mWorld->invId[27+barPosition] == DebugStick::getID())
                {
                    CheckDebugStickUse();
                    return;
                }

                if(keyHold(InputHelper::Instance()->getButtonToAction(14)))
                {
                    if(sleepTime == 0.0f)
					{
						if(keyPressed(InputHelper::Instance()->getButtonToAction(13))) //open inventory
						{
							if(craft3xEn == false && chestEn == false && furnaceEn == false)
							{
								invEn = true;
								dT = 0;
								dET = 0;
								chestId = -1;
								dStd = -1;
								return;
							}
                        }
                    }
                }
                if (startDt == true)
                {
                    startDt = false; //stop destroying
                }


                if (mWorld->invId[27+barPosition] == Bow::getID()) ///BOW: start the draw
                {
                    // Nothing happens on the press itself. Vanilla starts drawing here and fires when
                    // the button comes back up, which is what Update watches for; see bowCharging.
                    // Survival spends an arrow out of the pack, so with none this is a dry draw and
                    // the press does nothing. Creative *in this state* needs no arrow either: a world
                    // switched to creative in-game keeps running StatePlay, and `gameModeWorld` is
                    // what says so -- the separate StatePlayCreative never had a gate to begin with.
                    //
                    // Only a *new* draw is started. `SystemManager::KeyPressed` is not a true press
                    // edge: it answers yes whenever the whole button mask changed and the key asked
                    // about happens to be down, so it reports the place button again on any frame in
                    // which some other button moved -- and the default rotation keys are face buttons,
                    // so turning while holding the trigger fired this block repeatedly. Restarting a
                    // draw that is already running reset bowCharge to 0, which is what made the bow's
                    // draw snap back to its first frame while the camera was turning and fill up again
                    // the moment it stopped. A draw already in progress is left alone.
                    if(bowCharging == false &&
                       (mWorld->gameModeWorld == 1 || mWorld->HasItemInInventory(Arrow::getID()) == true))
                    {
                        bowCharging = true;
                        bowCharge = 0.0f;
                    }
                    return;
                }

                if (mWorld->invId[27+barPosition] == 299) ///SNOWBALL THROWING
                {
                    changeY = -0.20f;
                    anim[0] = 1;

                    mSoundMgr->PlayBowSound();

                    SnowBall2* NewSB = new SnowBall2(playerPosition.x,playerPosition.y,playerPosition.z);
                    NewSB->SetVeloc(fppCam->upDownAngle,(fppCam->horAngle/(float)180)*PI);
                    mSnowBalls.push_back(NewSB);

                    mWorld->invAm[27+barPosition] -= 1;
                    if (mWorld->invAm[27+barPosition] == 0) // [27+barPosition] - selected item/block
                    {
                        mWorld->invAm[27+barPosition] = -1;
                        mWorld->invId[27+barPosition] = -1;
                        mWorld->invSt[27+barPosition] = 0;
                    }
                    return;
                }

                if(mWorld->invId[27+barPosition] == FlintAndSteel::getID()) // light a mob (8 s, like a fire block)
                {
                    Vector3 igniteDir = fppCam->m_vView - fppCam->m_vPosition;
                    igniteDir.normalize();
                    float mobT = 6.0f, mobPower = 0.0f;
                    LameMob* igniteMob = FindMobAlongRay(mWorld, fppCam->m_vPosition, igniteDir, 5.0f, &mobT, &mobPower);
                    if(igniteMob != NULL)
                    {
                        bool blocked = false;
                        for(float i = 0; i < mobT; i += 0.15f)
                        {
                            Vector3 p = fppCam->m_vPosition + (igniteDir * i);
                            BoundingBox colliderP = mWorld->GetBlockCollideBox(p.x,p.y,p.z, mWorld->GetBlock(p.x,p.y,p.z));
                            BoundingBox testBoxP = BoundingBox(floorf(p.x)+0.5f+colliderP.min.x, floorf(p.y)+0.5f+colliderP.min.y, floorf(p.z)+0.5f+colliderP.min.z, floorf(p.x)+0.5f+colliderP.max.x, floorf(p.y)+0.5f+colliderP.max.y, floorf(p.z)+0.5f+colliderP.max.z);
                            if(mWorld->BlockEditable(p.x,p.y,p.z) && testBoxP.contains(p))
                            {
                                blocked = true;
                                break;
                            }
                        }
                        if(blocked == false)
                        {
                            igniteMob->SetOnFire(8.0f);
                            mSoundMgr->PlayFlintSound();
                            changeY = -0.20f;
                            anim[0] = 1;

                            mWorld->invAm[27+barPosition] -= 1;
                            if(mWorld->invAm[27+barPosition] <= 0)
                            {
                                mWorld->invAm[27+barPosition] = -1;
                                mWorld->invSt[27+barPosition] = 0;
                                mWorld->invId[27+barPosition] = -1;
                            }
                            return;
                        }
                    }
                }

                if(mWorld->invId[27+barPosition] == Wheat::getID() || mWorld->invId[27+barPosition] == Carrot::getID() || mWorld->invId[27+barPosition] == WheatSeeds::getID()) // feed: love mode / grow a baby
                {
                    Vector3 feedDir = fppCam->m_vView - fppCam->m_vPosition;
                    feedDir.normalize();
                    LameMob* feedMob = FindMobForUse(mWorld, fppCam->m_vPosition, feedDir, 5.0f);
                    // LameMob::Feed is the authority on what feeds what -- it rejects any item that is
                    // not the mob's own breedItemId -- so this list just has to cover every mob food in
                    // the game. WheatSeeds was missing, which is why chickens happily followed seeds but
                    // could never be fed: the ray never ran for them.
                    if(feedMob != NULL && feedMob->Feed(mWorld->invId[27+barPosition]))
                    {
                        changeY = -0.20f;
                        anim[0] = 1;
                        mWorld->invAm[27+barPosition] -= 1;
                        if(mWorld->invAm[27+barPosition] <= 0)
                        {
                            mWorld->invAm[27+barPosition] = -1;
                            mWorld->invSt[27+barPosition] = 0;
                            mWorld->invId[27+barPosition] = -1;
                        }
                        return;
                    }
                }

                //add cube
                Vector3 rayDir = fppCam->m_vView - fppCam->m_vPosition;
                rayDir.normalize();

                //get position and view vector
                Vector3 testPos;

                BoundingBox collider = mWorld->GetBlockCollideBox(testPos.x,testPos.y,testPos.z, mWorld->GetBlock(testPos.x,testPos.y,testPos.z));
                BoundingBox testBox = BoundingBox(testPos.x+collider.min.x, testPos.y+collider.min.y, testPos.z+collider.min.z, testPos.x+collider.max.x, testPos.y+collider.max.y, testPos.z+collider.max.z);


                if(mWorld->invId[27+barPosition] == Scissors::getID() || (mWorld->ItemType(mWorld->invId[27+barPosition]) == 'Y' && mWorld->invId[27+barPosition] != BoneMeal::getID())) // shear sheep or dye it
                {
                    if(mWorld->mSheeps.empty() == false)
                    {
                        for(float i = 0; i < 5.25f; i+=0.075f)
                        {
                            testPos = fppCam->m_vPosition + (rayDir * i);
                            for(int f = 0; f < mWorld->mSheeps.size(); f++)
                            {
                                Sheep *TestSheep = mWorld->mSheeps[f];
                                if(TestSheep->DistanceToPlayer() < 2.85 && TestSheep->kicked == false && TestSheep->damaged == false)
                                {
                                    if(TestSheep->bBox.contains(testPos))
                                    {
                                        if(mWorld->ItemType(mWorld->invId[27+barPosition]) == 'Y') // dye item
                                        {
                                            int newColor = 14;
                                            switch(mWorld->invId[27+barPosition])
                                            {
                                                case 314: // brown
                                                newColor = 6;
                                                break;

                                                case 318: // blue
                                                newColor = 8;
                                                break;

                                                case 322: // white
                                                newColor = 14;
                                                break;

                                                case 365: // lt gray
                                                newColor = 13;
                                                break;

                                                case 366: // gray
                                                newColor = 1;
                                                break;

                                                case 367: // black
                                                newColor = 0;
                                                break;

                                                case 368: // pink
                                                newColor = 3;
                                                break;

                                                case 369: // red
                                                newColor = 2;
                                                break;

                                                case 370: // orange
                                                newColor = 12;
                                                break;

                                                case 371: // yellow
                                                newColor = 7;
                                                break;

                                                case 372: // lime
                                                newColor = 5;
                                                break;

                                                case 373: // lime
                                                newColor = 4;
                                                break;

                                                case 374: // lt blue
                                                newColor = 9;
                                                break;

                                                case 375: // purple
                                                newColor = 10;
                                                break;

                                                case 376: // magneta
                                                newColor = 11;
                                                break;
                                            }

                                            if(TestSheep->GetFurColor() != newColor)
                                            {
                                                TestSheep->SetFurColor(newColor);

                                                mWorld->invAm[27+barPosition] -= 1;
                                                if (mWorld->invAm[27+barPosition] == 0)
                                                {
                                                    mWorld->invAm[27+barPosition] = -1;
                                                    mWorld->invId[27+barPosition] = -1;
                                                    mWorld->invSt[27+barPosition] = 0;
                                                }

                                                animGo = true;
                                                animDest = 0.0f;
                                            }
                                            return;
                                        }

                                        if(mWorld->invId[27+barPosition] == Scissors::getID())
                                        {
                                            if(TestSheep->sheared == false && TestSheep->IsBaby() == false)
                                            {
                                                int woolNum = 1+rand()%3;
                                                for(int i = 0; i < woolNum; i++)
                                                {
                                                    mWorld->DropThis(BlackWoolBlock::getID()+TestSheep->GetFurColor(),1,true,Vector3(TestSheep->position.x,TestSheep->position.y+0.3f,TestSheep->position.z));
                                                }

                                                TestSheep->sheared = true;
                                                mSoundMgr->PlayShearSound();

                                                mWorld->invAm[27+barPosition] -= 1;
                                                if (mWorld->invAm[27+barPosition] == 0)
                                                {
                                                    mWorld->invAm[27+barPosition] = -1;
                                                    mWorld->invId[27+barPosition] = -1;
                                                    mWorld->invSt[27+barPosition] = 0;
                                                }

                                                animGo = true;
                                                animDest = 0.0f;
                                                return;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    return;
                }

                // The look, and the two things that make the aim exact. The loop steps from cell wall to
                // cell wall instead of marching a fixed step, so every cell the ray crosses is visited:
                // a fixed step can pass a whole cell it crosses in less than its stride, and the cell it
                // costs is exactly the thin one -- redstone wire's 1 px selection plate (see
                // GetBlockSelectionBox and CraftWorld::NextCellDistance). The box test is then a clip,
                // `hasIntersected(box, distance)` -- the same call the placement face normal below uses --
                // rather than "did this point land inside the box", which on a plate the look only grazes
                // is the other half of the same coin toss.
                Ray pickRay(fppCam->m_vPosition, rayDir);

                for(float i = 0.0f; i < 5.25f; i = CraftWorld::NextCellDistance(fppCam->m_vPosition, rayDir, i))
                {
                    testPos = fppCam->m_vPosition + (rayDir * i);

                    collider = mWorld->GetBlockSelectionBox(testPos.x,testPos.y,testPos.z, mWorld->GetBlock(testPos.x,testPos.y,testPos.z));
                    testBox = BoundingBox(floorf(testPos.x)+0.5f+collider.min.x, floorf(testPos.y)+0.5f+collider.min.y, floorf(testPos.z)+0.5f+collider.min.z, floorf(testPos.x)+0.5f+collider.max.x, floorf(testPos.y)+0.5f+collider.max.y, floorf(testPos.z)+0.5f+collider.max.z);

                    int selectedBlock = mWorld->GetBlock(testPos.x, testPos.y, testPos.z);

                    if(mWorld->invId[27+barPosition] == WaterBusket::getID())
                    {
                        if(selectedBlock == Lava::getID())
                        {
                            int x = (int)testPos.x;
                            int y = (int)testPos.y;
                            int z = (int)testPos.z;
                            BoundingBox blockBox = BoundingBox(Vector3(x,y,z),Vector3(x + 1,y + 1,z + 1));
                            BoundingBox playerBox;
                            if(walkingOnGround == false)
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            else
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            if(!blockBox.intersect(playerBox))
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = Obsidian::getID();

                                mSoundMgr->PlayFissSound();

                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                mWorld->RebuildFullMeshChunk(curchunkTarget);
                                fppCam->needUpdate = true;

                                mWorld->invId[27+barPosition] = Busket::getID();

                                changeY = -0.16f;
                                anim[0] = 1;
                                return;
                            }
                        }
                    }

                    if(mWorld->invId[27+barPosition] == LavaBusket::getID())
                    {
                        if(selectedBlock == WaterBlock::getID())
                        {
                            int x = (int)testPos.x;
                            int y = (int)testPos.y;
                            int z = (int)testPos.z;
                            BoundingBox blockBox = BoundingBox(Vector3(x,y,z),Vector3(x + 1,y + 1,z + 1));
                            BoundingBox playerBox;
                            if(walkingOnGround == false)
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            else
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            if(!blockBox.intersect(playerBox))
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = CobbleStone::getID();

                                mSoundMgr->PlayFissSound();

                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                mWorld->RebuildFullMeshChunk(curchunkTarget);
                                fppCam->needUpdate = true;

                                mWorld->invId[27+barPosition] = Busket::getID();

                                changeY = -0.16f;
                                anim[0] = 1;
                                return;
                            }
                        }
                    }

                    if(mWorld->invId[27+barPosition] == 290) //busket
                    {
                        if (selectedBlock == 4) //if it is WATER
                        {
                            // well simulation
                            if((mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z) == 4 && mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z-1) == 4 && mWorld->GetBlock(testPos.x, testPos.y, testPos.z-1) == 4) ||
                               (mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z) == 4 && mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z-1) == 4 && mWorld->GetBlock(testPos.x, testPos.y, testPos.z-1) == 4) ||
                               (mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z) == 4 && mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z+1) == 4 && mWorld->GetBlock(testPos.x, testPos.y, testPos.z+1) == 4) ||
                               (mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z) == 4 && mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z+1) == 4 && mWorld->GetBlock(testPos.x, testPos.y, testPos.z+1) == 4))
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = 4;
                            }
                            else
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = 0;

                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                mWorld->RebuildTransparentMeshChunk(curchunkTarget);
                                fppCam->needUpdate = true;
                            }

                            mWorld->invId[27+barPosition] = 291;

                            changeY = -0.16f;
                            anim[0] = 1;
                            return;
                        }

                        if (selectedBlock == 112) //if it is lava
                        {
                            // well simulation
                            if((mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z) == Lava::getID() && mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z-1) == Lava::getID() && mWorld->GetBlock(testPos.x, testPos.y, testPos.z-1) == Lava::getID()) ||
                               (mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z) == Lava::getID() && mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z-1) == Lava::getID() && mWorld->GetBlock(testPos.x, testPos.y, testPos.z-1) == Lava::getID()) ||
                               (mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z) == Lava::getID() && mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z+1) == Lava::getID() && mWorld->GetBlock(testPos.x, testPos.y, testPos.z+1) == Lava::getID()) ||
                               (mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z) == Lava::getID() && mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z+1) == Lava::getID() && mWorld->GetBlock(testPos.x, testPos.y, testPos.z+1) == Lava::getID()))
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = Lava::getID();
                            }
                            else
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = 0;

                                mWorld->RemoveLigtSourceAtPosition(testPos.x,testPos.y,testPos.z,112);
                                mWorld->UpdateLightAreaIn(testPos);

                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                mWorld->RebuildFullMeshChunk(curchunkTarget);
                                fppCam->needUpdate = true;
                            }
                            mWorld->invId[27+barPosition] = 292;

                            changeY = -0.16f;
                            anim[0] = 1;
                            return;
                        }
                    }

                    //check if we are touch something
                    float pickDist = -1.0f;
                    pickRay.hasIntersected(testBox, pickDist);

                    if(mWorld->BlockEditable(testPos.x,testPos.y,testPos.z) && pickDist != -1.0f)
                    {
                        // we use hoes
                        if(mWorld->invId[27+barPosition] >= 270 && mWorld->invId[27+barPosition] <= 274)
                        {
                            if (selectedBlock == 1) //if it is grass
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = 79;
                                if(rand() % 11 == 0)
                                {
                                    //mWorld->DropThis(286,1,true,testPos); //put seed in inventory
                                }

                                mWorld->invAm[27+barPosition] -= 1; // bit crashing to our hoe
                                if(mWorld->invAm[27+barPosition] == 0) //destroy tool if it has 0 durability points
                                {
                                    mWorld->invAm[27+barPosition] = -1;
                                    mWorld->invId[27+barPosition] = -1;
                                    mWorld->invSt[27+barPosition] = 0;

                                    mSoundMgr->PlayBreakSound();
                                }
                                int	curchunkTarget = mWorld->getChunkId(testPos);

                                //Rebuild nearby world
                                mWorld->RebuildOpaqueMeshChunk(curchunkTarget);
                                fppCam->needUpdate = true;

                                mSoundMgr->PlayWalkSound(1);
                                changeY = -0.16f;
                                anim[0] = 1;
                                return;
                            }

                            if (selectedBlock == 2) //if it is dirt
                            {
                                mWorld->mainStatistics.soilPlowed += 1;
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = 79;

                                mWorld->invAm[27+barPosition] -= 1;
                                if(mWorld->invAm[27+barPosition] == 0) //destroy tool if it has 0 durability points
                                {
                                    mWorld->invAm[27+barPosition] = -1;
                                    mWorld->invId[27+barPosition] = -1;
                                    mWorld->invSt[27+barPosition] = 0;

                                    mSoundMgr->PlayBreakSound();
                                }

                                int	curchunkTarget = mWorld->getChunkId(testPos);

                                //Rebuild nearby world
                                mWorld->RebuildOpaqueMeshChunk(curchunkTarget);
                                fppCam->needUpdate = true;

                                mSoundMgr->PlayWalkSound(1); // play dirt sound
                                changeY = -0.16f;
                                anim[0] = 1;
                                return;
                            }
                        }

                        if (selectedBlock == NoteBlock::getID())
                        {
                            int noteBlockId = mWorld->FindNoteBlockID(testPos);

                            if(noteBlockId != -1)
                            {
                                if(mWorld->GetBlock(testPos.x,testPos.y+1,testPos.z) == 0)
                                {
                                    NoteBlockEntity* TestNoteBlock = mWorld->mNoteBlocks[noteBlockId];

                                    TestNoteBlock->IncreaseNote();
                                    mSoundMgr->PlayNoteSound(!(mWorld->BlockMaterial(testPos.x,testPos.y-1,testPos.z) == 1 || mWorld->BlockMaterial(testPos.x,testPos.y-1,testPos.z) == 8),TestNoteBlock->GetPitch());

                                    float red, blue, green;
                                    if(TestNoteBlock->GetNote() <= 12)
                                    {
                                        blue = 1.0 - (TestNoteBlock->GetNote()/12.0f);
                                        green = (TestNoteBlock->GetNote()/12.0f);
                                        red = 0.0f;
                                    }
                                    else
                                    {
                                        blue = 0.0f;
                                        green = 1.0 - ((TestNoteBlock->GetNote()-12)/12.0f);
                                        red = ((TestNoteBlock->GetNote()-12)/12.0f);
                                    }
                                    Particle* Note = Particle::Spawn(mWorld,"note",Vector3((int)testPos.x+0.5f,(int)testPos.y+1.1f,(int)testPos.z+0.5f));
                                    Note->SetColor(Vector3(red,green,blue));
                                }

                                animGo = true;
                                animDest = 0.0f;

                                changeY = -0.1f;
                                anim[0] = 1;

                                return;
                            }
                        }

                        if(selectedBlock >= ItemFrame1::getID() && selectedBlock <= ItemFrame4::getID())
                        {
                            int itemFrameId = mWorld->FindItemFrameID(testPos);

                            if(itemFrameId != -1)  // set item in item frame
                            {
                                ItemFrameEntity* TestItemFrame = mWorld->mItemFrames[itemFrameId];

                                if(mWorld->invId[27+barPosition] != -1)
                                {
                                    if(TestItemFrame->GetItem() == -1)
                                    {
                                        TestItemFrame->SetItem(mWorld->invId[27+barPosition]);

                                        if(mWorld->invSt[27+barPosition] == true)
                                        {
                                            mWorld->invAm[27+barPosition] -= 1;
                                            if(mWorld->invAm[27+barPosition] == 0)
                                            {
                                                mWorld->invAm[27+barPosition] = -1;
                                                mWorld->invSt[27+barPosition] = 0;
                                                mWorld->invId[27+barPosition] = -1;
                                            }
                                        }
                                        else
                                        {
                                            TestItemFrame->SetAmount(mWorld->invAm[27+barPosition]);

                                            mWorld->invAm[27+barPosition] = -1;
                                            mWorld->invSt[27+barPosition] = 0;
                                            mWorld->invId[27+barPosition] = -1;
                                        }

                                        animGo = true;
                                        animDest = 0.0f;

                                        changeY = -0.1f;
                                        anim[0] = 1;

                                        return;
                                    }
                                }

                                if(TestItemFrame->GetItem() != -1)  // remove item in item frame
                                {
                                    if(IsBlockId(TestItemFrame->GetItem()))
                                    {
                                        mWorld->DropThis(TestItemFrame->GetItem(), 1, true, Vector3((int)testPos.x+0.5f,(int)testPos.y+0.5f,(int)testPos.z+0.5f));
                                    }
                                    else
                                    {
                                        if(TestItemFrame->GetAmount() != -1)
                                        {
                                            mWorld->DropThis(TestItemFrame->GetItem(), TestItemFrame->GetAmount(), mWorld->StackableItem(TestItemFrame->GetItem()), Vector3((int)testPos.x+0.5f,(int)testPos.y+0.5f,(int)testPos.z+0.5f));
                                        }
                                        else
                                        {
                                            if(IsItemId(TestItemFrame->GetItem()))
                                            {
                                                mWorld->DropThis(TestItemFrame->GetItem(), 1, mWorld->StackableItem(TestItemFrame->GetItem()), Vector3((int)testPos.x+0.5f,(int)testPos.y+0.5f,(int)testPos.z+0.5f));
                                            }
                                            else
                                            {
                                                mWorld->DropThis(TestItemFrame->GetItem(), 1, true, Vector3((int)testPos.x+0.5f,(int)testPos.y+0.5f,(int)testPos.z+0.5f));
                                            }
                                        }
                                    }

                                    TestItemFrame->ResetItem();
                                    TestItemFrame->SetAmount(-1);

                                    return;
                                }
                            }
                        }

                        if (selectedBlock == JukeBox::getID())
                        {
                            int jukeboxId = mWorld->FindJukeboxID(testPos);

                            if(jukeboxId != -1)
                            {
                                if(mWorld->GetBlock(testPos.x,testPos.y+1,testPos.z) == 0)
                                {
                                    JukeboxEntity* TestJukebox = mWorld->mJukeboxes[jukeboxId];

                                    if(TestJukebox->GetRecord() == -1)
                                    {
                                        if((mWorld->invId[27+barPosition] >= Disk1::getID() && mWorld->invId[27+barPosition] <= Disk3::getID()) || (mWorld->invId[27+barPosition] >= Disk4::getID() && mWorld->invId[27+barPosition] <= Disk12::getID()))
                                        {
                                            TestJukebox->SetRecord(mWorld->invId[27+barPosition]);

                                            int music_number = 0;
                                            if(mWorld->invId[27+barPosition] >= Disk1::getID() && mWorld->invId[27+barPosition] <= Disk3::getID())
                                            {
                                                music_number = mWorld->invId[27+barPosition]-Disk1::getID();
                                            }
                                            if(mWorld->invId[27+barPosition] >= Disk4::getID() && mWorld->invId[27+barPosition] <= Disk12::getID())
                                            {
                                                music_number = mWorld->invId[27+barPosition]-Disk4::getID()+3;
                                            }

                                            mSoundMgr->StopAmbient();
                                            mSoundMgr->StopDiskTrack();
                                            mSoundMgr->PlayDiskTrack(music_number);

                                            switch(music_number)
                                            {
                                                case 0: inputDiskName = "C418 - 13"; break;
                                                case 1: inputDiskName = "C418 - cat"; break;
                                                case 2: inputDiskName = "C418 - blocks"; break;
                                                case 3: inputDiskName = "C418 - chirp"; break;
                                                case 4: inputDiskName = "C418 - far"; break;
                                                case 5: inputDiskName = "C418 - mall"; break;
                                                case 6: inputDiskName = "C418 - mellohi"; break;
                                                case 7: inputDiskName = "C418 - stal"; break;
                                                case 8: inputDiskName = "C418 - strad"; break;
                                                case 9: inputDiskName = "C418 - ward"; break;
                                                case 10: inputDiskName = "C418 - 11"; break;
                                                case 11: inputDiskName = "C418 - wait"; break;
                                            }

                                            inputDiskNameTimer = 2.4f;

                                            mWorld->invId[27+barPosition] = -1;
                                            mWorld->invAm[27+barPosition] = -1;
                                            mWorld->invSt[27+barPosition] = 0;

                                            musicTime = 360.0f;
                                        }
                                    }
                                    else
                                    {
                                        inputDiskName = "";
                                        inputDiskNameTimer = 0.0f;

                                        mWorld->DropThis(TestJukebox->GetRecord(), 1, false, Vector3((int)testPos.x+0.5f,(int)testPos.y+1.0f,(int)testPos.z+0.5f) );
                                        TestJukebox->SetRecord(-1);

                                        mSoundMgr->StopAmbient();
                                        mSoundMgr->StopDiskTrack();

                                        musicTime = 60.0f+rand()%60;
                                    }

                                    animGo = true;
                                    animDest = 0.0f;

                                    changeY = -0.1f;
                                    anim[0] = 1;
                                }
                                return;
                            }
                        }

                        if (selectedBlock == CraftingTable::getID()) // open crafting table menu
                        {
                            craft3xEn = true;
                            return;
                        }

                        if(mWorld->invId[27+barPosition] == BoneMeal::getID())
                        {
                            bool used = false;

                            if (selectedBlock == GrassBlock::getID())
                            {
                                for(int xx = testPos.x-2; xx <= testPos.x+2; xx ++)
                                {
                                    for(int zz = testPos.z-2; zz <= testPos.z+2; zz ++)
                                    {
                                        for(int yy = testPos.y; yy > 0; yy--)
                                        {
                                            if(mWorld->GetBlock(xx,yy,zz) != 0)
                                            {
                                                if(mWorld->GetBlock(xx,yy,zz) == GrassBlock::getID())
                                                {
                                                    if(mWorld->GetBlock(xx,yy+1,zz) == 0)
                                                    {
                                                        if(rand()%10<=5)
                                                        {
                                                            rand() % 2 == 0 ? mWorld->GetBlock(xx,yy+1,zz) = Flower1::getID() : mWorld->GetBlock(xx,yy+1,zz) = Flower2::getID();
                                                            if(rand()%2 == 0)
                                                            {
                                                                mWorld->GetBlock(xx,yy+1,zz) = TallGrass::getID();
                                                            }
                                                        }
                                                        break;
                                                    }
                                                    else
                                                    {
                                                        break;
                                                    }
                                                }
                                                else
                                                {
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                                used = true;
                            }
                            if (selectedBlock >= WheatBlock1::getID() && selectedBlock <= WheatBlock5::getID())
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = WheatBlock6::getID();
                                used = true;
                            }
                            if (selectedBlock >= MelonPlant1::getID() && selectedBlock <= MelonPlant3::getID())
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = MelonPlant4::getID();
                                used = true;
                            }
                            if (selectedBlock >= PumpkinPlant1::getID() && selectedBlock <= PumpkinPlant3::getID())
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = PumpkinPlant4::getID();
                                used = true;
                            }
                            if (selectedBlock >= PotatoPlant1::getID() && selectedBlock <= PotatoPlant3::getID())
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = PotatoPlant4::getID();
                                used = true;
                            }
                            if (selectedBlock >= CarrotPlant1::getID() && selectedBlock <= CarrotPlant3::getID())
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = CarrotPlant4::getID();
                                used = true;
                            }

                            if(used)
                            {
                                animGo = true;
                                animDest = 0.0f;

                                changeY = -0.1f;
                                anim[0] = 1;

                                mWorld->invAm[27+barPosition] -= 1;
                                if(mWorld->invAm[27+barPosition] == 0)
                                {
                                    mWorld->invAm[27+barPosition] = -1;
                                    mWorld->invSt[27+barPosition] = 0;
                                    mWorld->invId[27+barPosition] = -1;
                                }
                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                mWorld->RebuildTransparentMeshChunk(curchunkTarget);

                                fppCam->needUpdate = true;
                                return;
                            }
                        }

                        // half block + half block = full block
                        if (mWorld->blockTypes[mWorld->GetBlock(testPos.x, testPos.y, testPos.z)].blockModel == 1 &&
                            mWorld->blockTypes[mWorld->invId[27+barPosition]].blockModel == 1  &&
                            mWorld->GetBlock(testPos.x, testPos.y, testPos.z) == mWorld->invId[27+barPosition]
                           )
                        {
                            int x = (int)testPos.x;
                            int y = (int)testPos.y;
                            int z = (int)testPos.z;

                            if(testPos.y - y > 0.4f)
                            {
                                BoundingBox blockBox = BoundingBox(Vector3(x,y,z),Vector3(x + 1,y + 1,z + 1));
                                BoundingBox playerBox;
                                if(walkingOnGround == false)
                                {
                                    playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                                }
                                else
                                {
                                    playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                                }

                                if(!blockBox.intersect(playerBox))
                                {
                                    switch(mWorld->invId[27+barPosition])
                                    {
                                        case 82 : mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = OakPlanks::getID(); break;
                                        case 83 : mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = CobbleStone::getID(); break;
                                        case 84 : mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = BrickBlock::getID(); break;
                                        case 85 : mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = DoubleSlab::getID(); break;
                                        case 86 : mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = SandStone::getID(); break;
                                        case 108 : mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = StoneBrick::getID(); break;
                                        case 157 : mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = BirchPlanks::getID(); break;
                                        case 158 : mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = SprucePlanks::getID(); break;
                                    }

                                    mSoundMgr->PlayWalkSound(mWorld->blockTypes[mWorld->invId[27+barPosition]].soundType);

                                    mWorld->invAm[27+barPosition] -= 1;
                                    if(mWorld->invAm[27+barPosition] == 0)
                                    {
                                        mWorld->invAm[27+barPosition] = -1;
                                        mWorld->invId[27+barPosition] = -1;
                                        mWorld->invSt[27+barPosition] = 0;
                                    }

                                    animGo = true;
                                    animDest = 0.0f;

                                    int	curchunkTarget = mWorld->getChunkId(testPos);
                                    fppCam->needUpdate = true;
                                    mWorld->RebuildOpaqueMeshChunk(curchunkTarget);
                                    mWorld->rebuildNearestChunks(curchunkTarget,testPos);
                                    return;
                                }
                            }
                        }


                        if (selectedBlock >= TrapdoorClosed1::getID() && selectedBlock <= TrapdoorClosed4::getID()) //door
                        {
                            //check if player dont stand inside the door block
                            int x = (int)testPos.x;
                            int y = (int)testPos.y;
                            int z = (int)testPos.z;
                            BoundingBox doorBox = BoundingBox(Vector3(x,y,z),Vector3(x + 1,y + 1,z + 1));
                            BoundingBox playerBox;
                            if(walkingOnGround == false)
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            else
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            if(!doorBox.intersect(playerBox))
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = mWorld->GetBlock(testPos.x, testPos.y, testPos.z) - 4;
                                mSoundMgr->PlayDoorSound(true);

                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                fppCam->needUpdate = true;
                                mWorld->RebuildTransparentMeshChunk(curchunkTarget);
                                return;
                            }
                        }

                        if (selectedBlock >= TrapdoorOpen1::getID() && selectedBlock <= TrapdoorOpen4::getID()) //door
                        {
                            //check if player dont stand inside the door block
                            int x = (int)testPos.x;
                            int y = (int)testPos.y;
                            int z = (int)testPos.z;
                            BoundingBox doorBox = BoundingBox(Vector3(x,y,z),Vector3(x + 1,y + 1,z + 1));
                            BoundingBox playerBox;
                            if(walkingOnGround == false)
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            else
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }

                            if(!doorBox.intersect(playerBox))
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = mWorld->GetBlock(testPos.x, testPos.y, testPos.z) + 4;
                                mSoundMgr->PlayDoorSound(false);

                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                fppCam->needUpdate = true;
                                mWorld->RebuildTransparentMeshChunk(curchunkTarget);
                                return;
                            }
                        }

                        if (selectedBlock >= 49 && selectedBlock <= 52) //door
                        {
                            //check if player dont stand inside the door block
                            int x = (int)testPos.x;
                            int y = (int)testPos.y;
                            int z = (int)testPos.z;
                            BoundingBox doorBox = BoundingBox(Vector3(x,y,z),Vector3(x + 1,y + 2,z + 1));
                            BoundingBox playerBox;
                            if(walkingOnGround == false)
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            else
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            if(!doorBox.intersect(playerBox))
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = mWorld->GetBlock(testPos.x, testPos.y, testPos.z) + 4;
                                mWorld->GetBlock(testPos.x, testPos.y+1, testPos.z) = 58;
                                mSoundMgr->PlayDoorSound(true);

                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                fppCam->needUpdate = true;
                                mWorld->RebuildTransparentMeshChunk(curchunkTarget);
                                return;
                            }
                        }

                        if (selectedBlock >= 53 && selectedBlock <= 56) //door
                        {
                            //check if player dont stand inside the door block
                            int x = (int)testPos.x;
                            int y = (int)testPos.y;
                            int z = (int)testPos.z;
                            BoundingBox doorBox = BoundingBox(Vector3(x,y,z),Vector3(x + 1,y + 2,z + 1));
                            BoundingBox playerBox;
                            if(walkingOnGround == false)
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            else
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            if(!doorBox.intersect(playerBox))
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = mWorld->GetBlock(testPos.x, testPos.y, testPos.z) - 4;
                                mWorld->GetBlock(testPos.x, testPos.y+1, testPos.z) = 57;
                                mSoundMgr->PlayDoorSound(false);

                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                fppCam->needUpdate = true;
                                mWorld->RebuildTransparentMeshChunk(curchunkTarget);
                                return;
                            }
                        }

                        if (selectedBlock == 57) //door
                        {
                            //check if player dont stand inside the door block
                            int x = (int)testPos.x;
                            int y = (int)testPos.y;
                            int z = (int)testPos.z;
                            BoundingBox doorBox = BoundingBox(Vector3(x, y-2, z),Vector3(x + 1,y,z + 1));
                            BoundingBox playerBox;
                            if(walkingOnGround == false)
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            else
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            if(!doorBox.intersect(playerBox))
                            {
                                if (mWorld->GetBlock(testPos.x, testPos.y-1, testPos.z) >= 49 && mWorld->GetBlock(testPos.x, testPos.y-1, testPos.z) <= 52) //door
                                {
                                    mWorld->GetBlock(testPos.x, testPos.y-1, testPos.z) = mWorld->GetBlock(testPos.x, testPos.y-1, testPos.z) + 4;
                                    mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = 58;
                                    mSoundMgr->PlayDoorSound(true);

                                    int	curchunkTarget = mWorld->getChunkId(Vector3(testPos.x,testPos.y-1,testPos.z));
                                    fppCam->needUpdate = true;
                                    mWorld->RebuildTransparentMeshChunk(curchunkTarget);
                                    return;
                                }
                            }
                        }

                        if (selectedBlock == 58) //door
                        {
                            //check if player dont stand inside the door block
                            int x = (int)testPos.x;
                            int y = (int)testPos.y;
                            int z = (int)testPos.z;
                            BoundingBox doorBox = BoundingBox(Vector3(x, y-2, z),Vector3(x + 1,y,z + 1));
                            BoundingBox playerBox;
                            if(walkingOnGround == false)
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            else
                            {
                                playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                            }
                            if(!doorBox.intersect(playerBox))
                            {
                                if (mWorld->GetBlock(testPos.x, testPos.y-1, testPos.z) >= 53 && mWorld->GetBlock(testPos.x, testPos.y-1, testPos.z) <= 56) //door
                                {
                                    mWorld->GetBlock(testPos.x, testPos.y-1, testPos.z) = mWorld->GetBlock(testPos.x, testPos.y-1, testPos.z) - 4;
                                    mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = 57;
                                    mSoundMgr->PlayDoorSound(false);

                                    int	curchunkTarget = mWorld->getChunkId(Vector3(testPos.x,testPos.y-1,testPos.z));
                                    fppCam->needUpdate = true;
                                    mWorld->RebuildTransparentMeshChunk(curchunkTarget);
                                    return;
                                }
                            }
                        }

                        if (mWorld->IsButtonBlock((block_t)selectedBlock)) // pressing a button
                        {
                            mWorld->PressButton((int)testPos.x,(int)testPos.y,(int)testPos.z);
                            fppCam->needUpdate = true;
                            return;
                        }

                        if (mWorld->IsLever((block_t)selectedBlock)) // flipping a lever
                        {
                            // No press timer and no mPressedButtons entry: a lever latches, so the whole
                            // interaction is the state flip and the click it plays.
                            mWorld->UseLever((int)testPos.x,(int)testPos.y,(int)testPos.z);
                            fppCam->needUpdate = true;
                            return;
                        }

                        if (mWorld->IsRepeaterBlock((block_t)selectedBlock)) // retiming a repeater
                        {
                            mWorld->UseRepeater((int)testPos.x,(int)testPos.y,(int)testPos.z);
                            fppCam->needUpdate = true;
                            return;
                        }

                        if (selectedBlock == TNTBlock::getID() && mWorld->invId[27+barPosition] == FlintAndSteel::getID())//TNT explosion
                        {
                            mWorld->PrimeTNTBlock((int)testPos.x,(int)testPos.y,(int)testPos.z);
                            fppCam->needUpdate = true;

                            mWorld->invAm[27+barPosition] -= 1;
                            if(mWorld->invAm[27+barPosition] <= 0)
                            {
                                mWorld->invId[27+barPosition] = -1;
                                mWorld->invAm[27+barPosition] = -1;
                                mWorld->invSt[27+barPosition] = 0;
                            }

                            animGo = true;
                            animDest = 0.0f;
                            return;
                        }

                        if (selectedBlock == ChestBlock1::getID() || selectedBlock == ChestBlock2::getID() || selectedBlock == ChestBlock3::getID() || selectedBlock == ChestBlock4::getID())
                        {
                            chestId = FindChestId(testPos.x,testPos.y,testPos.z);
                            if(chestId != -1)
                            {
                                craft3xEn = false;
                                chestEn = true;
                                UseChest = mWorld->mChests[chestId];
                                return;
                            }
                        }

                        if (selectedBlock == FurnaceOff::getID() || selectedBlock == FurnaceOn::getID())
                        {
                            furnaceId = FindFurnaceId(testPos.x,testPos.y,testPos.z);
                            if(furnaceId != -1)
                            {
                                craft3xEn = false;
                                chestEn = false;
                                furnaceEn = true;
                                UseFurnace = mWorld->mFurnaces[furnaceId];
                                return;
                            }
                        }

                        if(selectedBlock >= BedBlock1::getID() && selectedBlock <= BedBlock8::getID() && sleepMessageTime <= 0.0f)
                        {
                            if(mWorld->worldDayTime >= 6 && mWorld->worldDayTime <= 21)
                            {
                                sleepMessageTime = 3.0f;
                                sleepMessageType = 1;
                                return;
                            }
                            if(mWorld->mZombies.empty() == false)
                            {
                                for(int f = 0; f < mWorld->mZombies.size(); f++)
                                {
                                    Zombie *TestZombie = mWorld->mZombies[f];
                                    if(abs(playerPosition.x-TestZombie->position.x) <= 7 && abs(playerPosition.z-TestZombie->position.z) <= 7 && abs(playerPosition.y-TestZombie->position.y) <= 4)
                                    {
                                        sleepMessageTime = 3.0f;
                                        sleepMessageType = 2;
                                        break;
                                    }
                                }
                                if(sleepMessageTime == 3.0f)
                                {
                                    return;
                                }
                            }

                            mWorld->playerSpawnPointPosition = Vector3((int)mWorld->playerPos.x+0.5f,mWorld->playerPos.y,(int)mWorld->playerPos.z+0.5f);
                            sleepTime = 4.6f;

                            if(selectedBlock == BedBlock1::getID() || selectedBlock == BedBlock5::getID())
                            {
                                Vector3 pillowPosition;
                                if(selectedBlock == BedBlock5::getID())
                                {
                                    pillowPosition.x = (int)testPos.x+0.75f;
                                    pillowPosition.y = (int)testPos.y+0.8f;
                                    pillowPosition.z = (int)testPos.z+0.5f;
                                }
                                if(selectedBlock == BedBlock1::getID())
                                {
                                    pillowPosition.x = (int)testPos.x+1.75f;
                                    pillowPosition.y = (int)testPos.y+0.8f;
                                    pillowPosition.z = (int)testPos.z+0.5f;
                                }

                                playerPosition = pillowPosition;

                                fppCam->PositionCamera(playerPosition.x,playerPosition.y,playerPosition.z, playerPosition.x-5.0f,playerPosition.y,playerPosition.z, 0.0f,1.0f,0.0f);
                                fppCam->RotateView(0,0,0,0);
                                fppCam->upDownAngle = 0;
                                fppCam->horAngle = 180;
                                fppCam->RebuildView();
                                return;
                            }
                            if(selectedBlock == BedBlock2::getID() || selectedBlock == BedBlock6::getID())
                            {
                                Vector3 pillowPosition;
                                if(selectedBlock == BedBlock6::getID())
                                {
                                    pillowPosition.x = (int)testPos.x+0.25f;
                                    pillowPosition.y = (int)testPos.y+0.8f;
                                    pillowPosition.z = (int)testPos.z+0.5f;
                                }
                                if(selectedBlock == BedBlock2::getID())
                                {
                                    pillowPosition.x = (int)testPos.x-0.75f;
                                    pillowPosition.y = (int)testPos.y+0.8f;
                                    pillowPosition.z = (int)testPos.z+0.5f;
                                }

                                playerPosition = pillowPosition;

                                fppCam->PositionCamera(playerPosition.x,playerPosition.y,playerPosition.z, playerPosition.x+5.0f,playerPosition.y,playerPosition.z, 0.0f,1.0f,0.0f);
                                fppCam->RotateView(0,0,180,0);
                                fppCam->upDownAngle = 0;
                                fppCam->horAngle = 0;
                                fppCam->RebuildView();
                                return;
                            }
                            if(selectedBlock == BedBlock3::getID() || selectedBlock == BedBlock7::getID())
                            {
                                Vector3 pillowPosition;
                                if(selectedBlock == BedBlock7::getID())
                                {
                                    pillowPosition.x = (int)testPos.x+0.5f;
                                    pillowPosition.y = (int)testPos.y+0.8f;
                                    pillowPosition.z = (int)testPos.z+0.75f;
                                }
                                if(selectedBlock == BedBlock3::getID())
                                {
                                    pillowPosition.x = (int)testPos.x+0.5f;
                                    pillowPosition.y = (int)testPos.y+0.8f;
                                    pillowPosition.z = (int)testPos.z+1.75f;
                                }

                                playerPosition = pillowPosition;

                                fppCam->PositionCamera(playerPosition.x,playerPosition.y,playerPosition.z, playerPosition.x,playerPosition.y,playerPosition.z-5.0f, 0.0f,1.0f,0.0f);
                                fppCam->RotateView(0,0,90,0);
                                fppCam->upDownAngle = 0;
                                fppCam->horAngle = 270;
                fppCam->RebuildView();
                                return;
                            }
                            if(selectedBlock == BedBlock4::getID() || selectedBlock == BedBlock8::getID())
                            {
                                Vector3 pillowPosition;
                                if(selectedBlock == BedBlock8::getID())
                                {
                                    pillowPosition.x = (int)testPos.x+0.5f;
                                    pillowPosition.y = (int)testPos.y+0.8f;
                                    pillowPosition.z = (int)testPos.z+0.25f;
                                }
                                if(selectedBlock == BedBlock4::getID())
                                {
                                    pillowPosition.x = (int)testPos.x+0.5f;
                                    pillowPosition.y = (int)testPos.y+0.8f;
                                    pillowPosition.z = (int)testPos.z-0.75f;
                                }

                                playerPosition = pillowPosition;

                                fppCam->PositionCamera(playerPosition.x,playerPosition.y,playerPosition.z, playerPosition.x,playerPosition.y,playerPosition.z+5.0f, 0.0f,1.0f,0.0f);
                                fppCam->RotateView(0,0,270,0);
                                fppCam->upDownAngle = 0;
                                fppCam->horAngle = 90;
                                fppCam->RebuildView();
                                return;
                            }
                        }

                        if(ceilf(mWorld->HG) < 20.0f && selectedBlock != Soil::getID() && selectedBlock != WateredSoil::getID())
                        {
                            bool foodEaten = false;

                            if (mWorld->invId[27+barPosition] == 288) ///BREAD
                            {
                                foodEaten = true;
                                mWorld->HG += 5;
                            }

                            if (mWorld->invId[27+barPosition] == 315) ///COOKIE
                            {
                                foodEaten = true;
                                mWorld->HG += 1;
                                mWorld->HP += 1;
                            }

                            if (mWorld->invId[27+barPosition] == RottenFlesh::getID()) ///ROTTEN FLESH
                            {
                                foodEaten = true;
                                mWorld->HG += 1;

                                if(rand() % 100 < 45)
                                {
                                    HurtPlayer(1);
                                }
                            }

                            if (mWorld->invId[27+barPosition] == Potato::getID()) ///POTATO
                            {
                                foodEaten = true;
                                mWorld->HG += 1;
                            }

                            if (mWorld->invId[27+barPosition] == PoisonedPotato::getID()) ///POSIONED POTATO
                            {
                                foodEaten = true;
                                mWorld->HG += 1;

                                HurtPlayer(4);
                            }

                            if (mWorld->invId[27+barPosition] == BakedPotato::getID()) ///BAKED POTATO
                            {
                                foodEaten = true;
                                mWorld->HG += 5;
                            }

                            if (mWorld->invId[27+barPosition] == Carrot::getID()) ///CARROT
                            {
                                foodEaten = true;
                                mWorld->HG += 3;
                            }

                            if (mWorld->invId[27+barPosition] == 311) ///MELON
                            {
                                foodEaten = true;
                                mWorld->HG += 2;
                            }

							if (mWorld->invId[27+barPosition] == RawBeef::getID()) ///RAW BEEF
                            {
                                foodEaten = true;
                                mWorld->HG += 1;
                            }


                            if (mWorld->invId[27+barPosition] == 317) ///STEAK
                            {
                                foodEaten = true;
                                mWorld->HG += 8;
                            }

                            if (mWorld->invId[27+barPosition] == RawChicken::getID()) ///RAW CHICKEN
                            {
                                foodEaten = true;
                                mWorld->HG += 1;
                            }

                            if (mWorld->invId[27+barPosition] == CookedChicken::getID()) ///COOKED CHICKEN
                            {
                                foodEaten = true;
                                mWorld->HG += 8;
                            }

							if (mWorld->invId[27+barPosition] == RawMutton::getID()) ///RAW Mutton
                            {
                                foodEaten = true;
                                mWorld->HG += 1;
                            }

							if (mWorld->invId[27+barPosition] == 393) ///Cooked Mutton
                            {
                                foodEaten = true;
                                mWorld->HG += 8;
                            }

                            if (mWorld->invId[27+barPosition] == 284) ///APPLE
                            {
                                foodEaten = true;
                                mWorld->HG += 4;
                            }

                            if (mWorld->invId[27+barPosition] == 285) ///GOLDEN APPLE
                            {
                                foodEaten = true;
                                mWorld->HG = 20;
                                mWorld->HP = 20;
                            }

                            if (mWorld->invId[27+barPosition] == 303) ///MOOSHROOM BOWL
                            {
                                foodEaten = true;
                                mWorld->HG += 6;
                            }

                            if(foodEaten)
                            {
                                mSoundMgr->PlayEatSound();
                                mWorld->mainStatistics.foodEaten += 1;

                                changeY = -0.20f;
                                anim[0] = 1;

                                Vector3 rayParticleDir = fppCam->m_vView - fppCam->m_vPosition;
                                rayParticleDir.normalize();

                                Vector3 particlePos = fppCam->m_vPosition + (rayParticleDir * 0.38f);

                                if(Particle::ActiveCount() <= 52)
                                {
                                    for(int k = 0; k <= 5; k++) // 5 particles
                                    {
                                        Particle* FoodPart = Particle::Spawn(mWorld,"item crack",Vector3(particlePos.x,particlePos.y-0.18f,particlePos.z));
                                        FoodPart->var1 = mWorld->invId[27+barPosition]-250;
                                    }
                                }

                                if(mWorld->HG >= 20)
                                {
                                    mWorld->HG = 19.999f;
                                }

                                if(mWorld->invId[27+barPosition] == MooshroomBowl::getID())
                                {
                                    mWorld->invId[27+barPosition] = 302; // turn it into simple stackable bowl
                                    mWorld->invSt[27+barPosition] = 1;
                                }
                                else
                                {
                                    mWorld->invAm[27+barPosition] -= 1;
                                    if (mWorld->invAm[27+barPosition] == 0)
                                    {
                                        mWorld->invAm[27+barPosition] = -1;
                                        mWorld->invId[27+barPosition] = -1;
                                        mWorld->invSt[27+barPosition] = 0;
                                    }
                                }
                                return;
                            }
                        }

                        BoundingBox testBox2 = BoundingBox(Vector3(cubePos.x - 0.5f,cubePos.y - 0.5f,cubePos.z - 0.5f),Vector3(cubePos.x + 0.5f,cubePos.y + 0.5f,cubePos.z + 0.5f));
                        Ray tesRay = Ray(fppCam->m_vPosition,rayDir);

                        float distance = -1.0f;
                        tesRay.hasIntersected(testBox2,distance);

                        bool wasUpdated = false;

                        if(distance != -1.0f)
                        {
                            Vector3 hitPos = fppCam->m_vPosition + (rayDir * distance);
                            // Which face of the block the look entered. The cell a placement targets is
                            // the block the pick landed on, stepped one cell out along that face -- and
                            // that means exactly *one* axis, so the dominant axis of the hit normal is
                            // what decides it. The per-axis test this replaces asked each axis on its own
                            // against a 0.66 threshold, and a hit near a block edge splits the normal
                            // across two axes -- (0.68, 0.68, 0.27) at a corner -- so it pushed two of
                            // them at once and produced a *diagonal* target cell. A torch reads its
                            // mounting straight off this offset, so that diagonal cell is precisely what
                            // put the torch on the block diagonally beside the one being aimed at.
                            Vector3 normal = (hitPos - cubePos);

                            Vector3 testPos2;

                            if(fabsf(normal.x) >= fabsf(normal.y) && fabsf(normal.x) >= fabsf(normal.z))
                                testPos2.x = (normal.x > 0.0f) ? 1.0f : -1.0f;
                            else
                                testPos2.x = 0.0f;

                            if(fabsf(normal.y) > fabsf(normal.x) && fabsf(normal.y) >= fabsf(normal.z))
                                testPos2.y = (normal.y > 0.0f) ? 1.0f : -1.0f;
                            else
                                testPos2.y = 0.0f;

                            if(fabsf(normal.z) > fabsf(normal.x) && fabsf(normal.z) > fabsf(normal.y))
                                testPos2.z = (normal.z > 0.0f) ? 1.0f : -1.0f;
                            else
                                testPos2.z = 0.0f;

                            testPos2 += testPos;

                            // Aiming at a wire means "put this on the wire". The wire's own cell can
                            // never be the target -- the wire is in it -- and the face-derived cell is
                            // not what a player means either: a floor plate has no meaningful side face,
                            // so a level look at a wire put the block *beside* it. Wire only ever lies
                            // flat (there is no wall-mounted dust), so a block aimed at one belongs in
                            // the cell directly above it, whatever face the ray reports.
                            if(mWorld->GetBlock(testPos.x,testPos.y,testPos.z) == Redstoneoff::getID())
                            {
                                testPos2.x = (float)((int)testPos.x);
                                testPos2.y = (float)((int)testPos.y + 1);
                                testPos2.z = (float)((int)testPos.z);
                            }

                            //check if this block is empty
                            if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0 ||
                               mWorld->BlockIsWater(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z)) ||
                               mWorld->BlockIsLava(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z)) ||
                               mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 93)
                            {
                                //now check if player is not there ;) we don't want to place cubes where we stand
                                int boxX = (int)testPos2.x;
                                int boxY = (int)testPos2.y;
                                int boxZ = (int)testPos2.z;

                                BoundingBox blockBox = BoundingBox(Vector3(boxX,boxY,boxZ),Vector3(boxX + 1,boxY + 1,boxZ + 1));
                                BoundingBox playerBox;
                                if(walkingOnGround == false)
                                {
                                    playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.02f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                                }
                                else
                                {
                                    playerBox = BoundingBox(Vector3(playerPosition.x - (mWorld->playerCollisionSize-0.03f),playerPosition.y - (mWorld->playerGrowth - 0.06f),playerPosition.z - (mWorld->playerCollisionSize-0.03)),Vector3(playerPosition.x + (mWorld->playerCollisionSize-0.03),playerPosition.y + 0.2f,playerPosition.z + (mWorld->playerCollisionSize-0.03)));
                                }

                                if(!blockBox.intersect(playerBox) ||
                                   mWorld->invId[27+barPosition] == TorchItem::getID()    ||
                                   mWorld->invId[27+barPosition] == LadderItem::getID()   ||
                                   mWorld->invId[27+barPosition] == WheatSeeds::getID()   ||
                                   mWorld->invId[27+barPosition] == MelonSeeds::getID()   ||
                                   mWorld->invId[27+barPosition] == PumpkinSeeds::getID() ||
                                   mWorld->invId[27+barPosition] == Carrot::getID()       ||
                                   mWorld->invId[27+barPosition] == Potato::getID()        ||
                                   mWorld->invId[27+barPosition] == Redstone::getID()      ||
                                   mWorld->IsSpawnEgg(mWorld->invId[27+barPosition]))
                                {
                                    //check if you want put light source or normal block
                                    if(mWorld->CanPutBlockHere(testPos2.x,testPos2.y,testPos2.z,mWorld->invId[27+barPosition]))
                                    {
                                        bool wasLight = false;
                                        int oldBlock = mWorld->invId[27+barPosition];

                                        if(mWorld->LightSourceBlock(mWorld->invId[27+barPosition]))
                                        {
                                            wasLight = true;
                                            mWorld->SetLigtSourcePosition(testPos2.x,testPos2.y,testPos2.z,mWorld->invId[27+barPosition]);
                                        }

                                        if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == Lava::getID())
                                        {
                                            wasLight = true;
                                            mWorld->RemoveLigtSourceAtPosition(testPos2.x,testPos2.y,testPos2.z,112);
                                            mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 0;
                                        }

                                        if (IsBlockId(mWorld->invId[27+barPosition])) // if block in our hands
                                        {
                                            if (mWorld->invId[27+barPosition] >= BlackWoolCarpet::getID() && mWorld->invId[27+barPosition] <= WhiteWoolCarpet::getID()) // if carpet in our hands
                                            {
                                                if(mWorld->BlockSpecial(testPos2.x,testPos2.y-1,testPos2.z) != 2)
                                                {
                                                    if(mWorld->BlockSpecial(testPos2.x,testPos2.y-1,testPos2.z) != 0 || mWorld->BlockSolid(testPos2.x,testPos2.y-1,testPos2.z) == false)
                                                    {
                                                        return;
                                                    }
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == NoteBlock::getID()) // if we are putting note block
                                            {
                                                NoteBlockEntity* NewNoteBlockEntity = new NoteBlockEntity(testPos2.x,testPos2.y,testPos2.z,12);
                                                mWorld->mNoteBlocks.push_back(NewNoteBlockEntity);
                                            }

                                            if(mWorld->invId[27+barPosition] == JukeBox::getID()) // if we are putting jukebox
                                            {
                                                JukeboxEntity* NewJukeboxEntity = new JukeboxEntity(testPos2.x,testPos2.y,testPos2.z,-1);
                                                mWorld->mJukeboxes.push_back(NewJukeboxEntity);
                                            }

                                            if(mWorld->invId[27+barPosition] == ChestBlock3::getID()) // if we are putting chest
                                            {
                                                Chest* NewChest =  new Chest(testPos2.x,testPos2.y,testPos2.z);
                                                mWorld->mChests.push_back(NewChest);
                                            }

                                            if(mWorld->invId[27+barPosition] == FurnaceOff::getID()) // if we are putting chest
                                            {
                                                Furnace* NewFurnace =  new Furnace(testPos2.x,testPos2.y,testPos2.z);

                                                NewFurnace->SetSide(0);
                                                if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                {
                                                    NewFurnace->SetSide(3);
                                                }
                                                if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                {
                                                    NewFurnace->SetSide(0);
                                                }
                                                if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                {
                                                    NewFurnace->SetSide(2);
                                                }
                                                if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                {
                                                    NewFurnace->SetSide(1);
                                                }

                                                mWorld->mFurnaces.push_back(NewFurnace);
                                            }

                                            if(mWorld->invId[27+barPosition]!=-1) //multi-texturing blocks
                                            {
                                                bool placed = true;

                                                switch(mWorld->invId[27+barPosition])
                                                {
                                                    case 75:
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 75;
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 76;
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 73;
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 74;
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 73;
                                                    }
                                                    break;

                                                    case 145:
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 145;
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 146;
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 143;
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 144;
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 143;
                                                    }
                                                    break;

                                                    case 96:
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 96; // 3
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 97; // 4
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 94; // 1
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 95; // 2
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 94;
                                                    }
                                                    break;

                                                    case 125:
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 125; // 3
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 126; // 4
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 127; // 1
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 128; // 2
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 125;
                                                    }
                                                    break;

                                                    case 161: // birch stair
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 161; // 3
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 162; // 4
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 163; // 1
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 164; // 2
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 161;
                                                    }
                                                    break;

                                                    case 165: // spruce stair
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 165; // 3
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 166; // 4
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 167; // 1
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 168; // 2
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 165;
                                                    }
                                                    break;

                                                    case 60:
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 60; // 3
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 61; // 4
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 62; // 1
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 63; // 2
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 60;
                                                    }
                                                    break;

                                                    case 67:
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 67; // 3
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 68; // 4
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 69; // 1
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 70; // 2
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 67;
                                                    }
                                                    break;

                                                    case 151:
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        if(mWorld->BlockSolid(testPos2.x,testPos2.y,testPos2.z+1))
                                                        {
                                                            mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 154; // 3
                                                        }
                                                        else
                                                        {
                                                            placed = false;
                                                        }
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        if(mWorld->BlockSolid(testPos2.x,testPos2.y,testPos2.z-1))
                                                        {
                                                            mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 153; // 4
                                                        }
                                                        else
                                                        {
                                                            placed = false;
                                                        }
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        if(mWorld->BlockSolid(testPos2.x+1,testPos2.y,testPos2.z))
                                                        {
                                                            mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 152; // 1
                                                        }
                                                        else
                                                        {
                                                            placed = false;
                                                        }
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        if(mWorld->BlockSolid(testPos2.x-1,testPos2.y,testPos2.z))
                                                        {
                                                            mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 151; // 2
                                                        }
                                                        else
                                                        {
                                                            placed = false;
                                                        }
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        if(mWorld->BlockSolid(testPos2.x-1,testPos2.y,testPos2.z))
                                                        {
                                                            mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 151;
                                                        }
                                                        else
                                                        {
                                                            placed = false;
                                                        }
                                                    }
                                                    break;


                                                    case kExtendedBlockBase + 3: // Stone button
                                                    case kExtendedBlockBase + 4: // Wooden button
                                                    case kExtendedBlockBase + 17: // Lever
                                                    {
                                                        // Buttons mount onto the clicked face; the
                                                        // pressed flag is added later, as block state.
                                                        // A lever mounts the same way and stores its
                                                        // facing in the same nibble, so placement is
                                                        // identical -- the only difference is what the
                                                        // fourth bit means once it is there.
                                                        float odx = testPos2.x - testPos.x;
                                                        float ody = testPos2.y - testPos.y;
                                                        float odz = testPos2.z - testPos.z;
                                                        int buttonFacing = -1;
                                                        if(ody > 0.5f) buttonFacing = 0;
                                                        else if(ody < -0.5f) buttonFacing = 1;
                                                        else if(odx > 0.5f) buttonFacing = 2;
                                                        else if(odx < -0.5f) buttonFacing = 3;
                                                        else if(odz > 0.5f) buttonFacing = 4;
                                                        else if(odz < -0.5f) buttonFacing = 5;

                                                        if(buttonFacing >= 0 && mWorld->BlockSolid(testPos.x,testPos.y,testPos.z))
                                                        {
                                                            mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = mWorld->invId[27+barPosition];
                                                            mWorld->SetButtonState((int)testPos2.x,(int)testPos2.y,(int)testPos2.z, buttonFacing);
                                                        }
                                                        else
                                                        {
                                                            placed = false;
                                                        }
                                                        break;
                                                    }

                                                    case kExtendedBlockBase + 5: // Redstone repeater
                                                    {
                                                        // Java BlockRedstoneRepeater: it only ever sits on the
                                                        // floor, and its output points away from the player who
                                                        // placed it, so it drives the block straight ahead.
                                                        float ody = testPos2.y - testPos.y;
                                                        if(ody > 0.5f && mWorld->BlockSolid(testPos.x,testPos.y,testPos.z))
                                                        {
                                                            Vector3 look = fppCam->m_vView - fppCam->m_vPosition;
                                                            float lookX = look.x < 0.0f ? -look.x : look.x;
                                                            float lookZ = look.z < 0.0f ? -look.z : look.z;
                                                            int repeaterFacing;
                                                            if(lookX >= lookZ)
                                                                repeaterFacing = (look.x > 0.0f) ? 1 : 3;
                                                            else
                                                                repeaterFacing = (look.z > 0.0f) ? 2 : 0;

                                                            mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = mWorld->invId[27+barPosition];
                                                            mWorld->SetRepeaterState((int)testPos2.x,(int)testPos2.y,(int)testPos2.z,
                                                                mWorld->RepeaterStateFrom(repeaterFacing, 1));
                                                            mWorld->UpdateRedstonePower((int)testPos2.x,(int)testPos2.y,(int)testPos2.z);
                                                        }
                                                        else
                                                        {
                                                            placed = false;
                                                        }
                                                        break;
                                                    }

                                                    case kExtendedBlockBase + 20: // Piston
                                                    case kExtendedBlockBase + 22: // Sticky piston
                                                    {
                                                        // Java BlockPistonBase.determineOrientation: the head
                                                        // comes out *towards* whoever placed the piston, and the
                                                        // placer's pitch is only consulted when they are standing
                                                        // almost directly over or under the cell being filled.
                                                        // Reading the reticle instead would make a piston placed
                                                        // on the floor at your feet push *down* into the ground,
                                                        // where vanilla pushes it up -- the cell is under the eye
                                                        // and the block below is not the direction being asked
                                                        // for.
                                                        //
                                                        // "Towards the placer" is the part that is easy to get
                                                        // backwards, and reading the look vector directly is how:
                                                        // that vector points *away* from the player, so the
                                                        // direction to store is the one the piston pushes back
                                                        // along, not the one the vector names. Stored the other
                                                        // way round, a piston placed on a wall has its arm in the
                                                        // wall and its plate facing away, and it disagrees with
                                                        // the vertical cases below -- which were always right,
                                                        // and are the ones that show the piston on the floor
                                                        // pushing up at you.
                                                        Vector3 look = fppCam->m_vView - fppCam->m_vPosition;
                                                        float lookX = look.x < 0.0f ? -look.x : look.x;
                                                        float lookZ = look.z < 0.0f ? -look.z : look.z;
                                                        int pistonFacing;
                                                        if(lookX >= lookZ)
                                                            pistonFacing = (look.x > 0.0f) ? 2 : 3;   // -X / +X
                                                        else
                                                            pistonFacing = (look.z > 0.0f) ? 4 : 5;   // -Z / +Z

                                                        if(fabsf(playerPosition.x - testPos2.x) < 2.0f &&
                                                           fabsf(playerPosition.z - testPos2.z) < 2.0f)
                                                        {
                                                            if(playerPosition.y - testPos2.y > 2.0f)
                                                                pistonFacing = 1;   // the placer is well above it
                                                            else if(testPos2.y - playerPosition.y > 0.0f)
                                                                pistonFacing = 0;   // and here, well below it
                                                        }

                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = mWorld->invId[27+barPosition];
                                                        // The idle piston *is* the item, so there is no other
                                                        // id to write -- but the state nibble is written by
                                                        // hand, as for a button: the cell has just been air
                                                        // (or water), and whatever was in the nibble before
                                                        // that is not this piston's direction.
                                                        mWorld->SetPistonState((int)testPos2.x,(int)testPos2.y,(int)testPos2.z, pistonFacing);
                                                        break;
                                                    }

                                                    default:
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = mWorld->invId[27+barPosition];
                                                    break;
                                                }

                                                if(placed)
                                                {
                                                    if(mWorld->invAm[27+barPosition]==1)
                                                    {
                                                        mWorld->invAm[27+barPosition]= -1;
                                                        mWorld->invId[27+barPosition]= -1;
                                                        mWorld->invSt[27+barPosition]= 0;
                                                    }
                                                    else
                                                    {
                                                        mWorld->invAm[27+barPosition]-= 1;
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == TNTBlock::getID())
                                                    {
                                                        mWorld->TryIgniteTNT((int)testPos2.x,(int)testPos2.y,(int)testPos2.z);
                                                    }
                                                    else
                                                    {
                                                        // Java's World.setBlock notifies every neighbour on *any*
                                                        // placement, and the redstone half of that is a reseed: dust
                                                        // recomputes from its sources, a lamp in range re-decides, and a
                                                        // switch in range is re-asked. Covered by the old
                                                        // `== RedstoneBlock` test only, which is why a lamp placed
                                                        // against live wiring stayed dark until some unrelated redstone
                                                        // event happened inside its 5x5x5 window.
                                                        mWorld->UpdateRedstonePower((int)testPos2.x,(int)testPos2.y,(int)testPos2.z);
                                                    }
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }
                                        }
                                        else //if it is item in our hand
                                        {
                                            if(mWorld->IsSpawnEgg(mWorld->invId[27+barPosition]))
                                            {
                                                if(mWorld->SpawnFromEgg(mWorld->invId[27+barPosition], testPos2.x+0.5f, testPos2.y, testPos2.z+0.5f))
                                                {
                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                        mWorld->invAm[27+barPosition] = -1;
                                                        mWorld->invSt[27+barPosition] = 0;
                                                        mWorld->invId[27+barPosition] = -1;
                                                    }
                                                    animGo = true;
                                                    animDest = 0.0f;
                                                }
                                                return;
                                            }
                                            if(mWorld->invId[27+barPosition] == TorchItem::getID()) // if it is torch
                                            {
                                                // Face from clicked support: 0=floor, 1=-X, 2=+X, 3=-Z, 4=+Z
                                                float odx = testPos2.x - testPos.x;
                                                float ody = testPos2.y - testPos.y;
                                                float odz = testPos2.z - testPos.z;
                                                int torchFace = -1;
                                                if(ody > 0.5f)
                                                    torchFace = 0;
                                                else if(ody < -0.5f)
                                                    torchFace = -1; // no ceiling torches
                                                else if(odx > 0.5f)
                                                    torchFace = 1;
                                                else if(odx < -0.5f)
                                                    torchFace = 2;
                                                else if(odz > 0.5f)
                                                    torchFace = 3;
                                                else if(odz < -0.5f)
                                                    torchFace = 4;

                                                if(torchFace >= 0 && mWorld->BlockSolid(testPos.x,testPos.y,testPos.z))
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = CraftWorld::TorchBlockFromFace(torchFace);
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                    wasLight = true;
                                                    mWorld->SetLigtSourcePosition(testPos2.x,testPos2.y,testPos2.z,Torch::getID());
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == RedstoneTorchItem::getID()) // redstone torch
                                            {
                                                // Same mount rule as the plain torch, but the block id has
                                                // to carry the lit/unlit family as well, so the id is built
                                                // from the face and the torch then decides its own state.
                                                float odx = testPos2.x - testPos.x;
                                                float ody = testPos2.y - testPos.y;
                                                float odz = testPos2.z - testPos.z;
                                                int torchFace = -1;
                                                if(ody > 0.5f)
                                                    torchFace = 0;
                                                else if(ody < -0.5f)
                                                    torchFace = -1; // no ceiling torches
                                                else if(odx > 0.5f)
                                                    torchFace = 1;
                                                else if(odx < -0.5f)
                                                    torchFace = 2;
                                                else if(odz > 0.5f)
                                                    torchFace = 3;
                                                else if(odz < -0.5f)
                                                    torchFace = 4;

                                                if(torchFace >= 0 && mWorld->BlockSolid(testPos.x,testPos.y,testPos.z))
                                                {
                                                    // Java places the lit torch and lets updateTick darken it if
                                                    // the mount is already powered. Evaluating immediately gives
                                                    // the same result without the visible 0.1 s of wrong light.
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) =
                                                        CraftWorld::RedstoneTorchFromFace(torchFace, true);
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                    mWorld->UpdateRedstonePower((int)testPos2.x,(int)testPos2.y,(int)testPos2.z);
                                                    mWorld->EvaluateRedstoneTorch((int)testPos2.x,(int)testPos2.y,(int)testPos2.z);
                                                    wasLight = true;
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == RepeaterItem::getID()) // redstone repeater
                                            {
                                                // Java BlockRedstoneRepeater: it only ever sits on the floor, and
                                                // its output points away from the player who placed it, so it
                                                // drives the block straight ahead. The block-id path below keeps
                                                // the same rule for inventories saved before the repeater had an
                                                // item form; this is the path a repeater picked up now takes.
                                                float ody = testPos2.y - testPos.y;
                                                if(ody > 0.5f && mWorld->BlockSolid(testPos.x,testPos.y,testPos.z))
                                                {
                                                    Vector3 look = fppCam->m_vView - fppCam->m_vPosition;
                                                    float lookX = look.x < 0.0f ? -look.x : look.x;
                                                    float lookZ = look.z < 0.0f ? -look.z : look.z;
                                                    int repeaterFacing;
                                                    if(lookX >= lookZ)
                                                        repeaterFacing = (look.x > 0.0f) ? 1 : 3;
                                                    else
                                                        repeaterFacing = (look.z > 0.0f) ? 2 : 0;

                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = RepeaterOff::getID();
                                                    mWorld->SetRepeaterState((int)testPos2.x,(int)testPos2.y,(int)testPos2.z,
                                                        mWorld->RepeaterStateFrom(repeaterFacing, 1));
                                                    mWorld->UpdateRedstonePower((int)testPos2.x,(int)testPos2.y,(int)testPos2.z);
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                    wasLight = true;
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == Redstone::getID()) // if it is Redstone item
                                            {
                                                bool canPlaceDust = (mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0);
                                                if(canPlaceDust && mWorld->BlockSolid(testPos2.x,testPos2.y-1,testPos2.z) == false)
                                                {
                                                    canPlaceDust = false;
                                                }
                                                if(canPlaceDust)
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = Redstoneoff::getID();
                                                    fppCam->needUpdate = true;
                                                    int chunkTarget = mWorld->getChunkId(testPos2);
                                                    mWorld->RebuildTransparentMeshChunk(chunkTarget);
                                                    mWorld->rebuildNearestChunks(chunkTarget,testPos2);
                                                    mWorld->UpdateRedstonePower((int)testPos2.x,(int)testPos2.y,(int)testPos2.z);

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                    wasUpdated = true;
                                                    animGo = true;
                                                    animDest = 0.0f;
                                                    return;
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == WheatSeeds::getID()) // if it is seeds
                                            {
                                                if( mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 79 || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 80) // planting seeds
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 134;
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == MelonSeeds::getID()) // if it is melon seeds
                                            {
                                                if( mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == Soil::getID() || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == WateredSoil::getID()) // planting seeds
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = MelonPlant1::getID();
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == PumpkinSeeds::getID()) // if it is pumpkin seeds
                                            {
                                                if( mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == Soil::getID() || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == WateredSoil::getID()) // planting seeds
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = PumpkinPlant1::getID();
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == Potato::getID())
                                            {
                                                if( mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == Soil::getID() || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == WateredSoil::getID()) // planting seeds
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = PotatoPlant1::getID();
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                    mWorld->RebuildTransparentMeshChunk(mWorld->getChunkId(testPos2));

                                                    animGo = true;
                                                    animDest = 0.0f;

                                                    changeY = -0.1f;
                                                    anim[0] = 1;
                                                }
                                                return;
                                            }

                                            if(mWorld->invId[27+barPosition] == Carrot::getID())
                                            {
                                                if( mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == Soil::getID() || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == WateredSoil::getID()) // planting seeds
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = CarrotPlant1::getID();
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                    mWorld->RebuildTransparentMeshChunk(mWorld->getChunkId(testPos2));

                                                    animGo = true;
                                                    animDest = 0.0f;

                                                    changeY = -0.1f;
                                                    anim[0] = 1;
                                                }
                                                return;
                                            }
											if (mWorld->invId[27 + barPosition] == FlintAndSteel::getID()) // if it is FlintAndSteel
											{
												mSoundMgr->PlayFlintSound();

												// A flint and steel on a valid obsidian ring lights the portal rather
												// than starting a fire. TryLightPortal measures the ring, so any legal
												// frame size works; only when there is no frame does the old fire
												// behaviour below run. The clicked cell is tried first, then the aimed
												// block itself, because the player may aim at either the opening or the
												// obsidian.
												bool litPortal = mWorld->TryLightPortal(testPos2.x, testPos2.y, testPos2.z);
												if(litPortal == false)
													litPortal = mWorld->TryLightPortal(testPos.x, testPos.y, testPos.z);
												Aurora::Utils::Logger::Instance()->LogMessage("portal: flint clicked=(%d,%d,%d) place=(%d,%d,%d) lit=%d\n",
													(int)testPos.x,(int)testPos.y,(int)testPos.z,
													(int)testPos2.x,(int)testPos2.y,(int)testPos2.z,(int)litPortal);

												fppCam->needUpdate = true;
												int chunkTarget = mWorld->getChunkId(testPos2);
												if(litPortal == true)
												{
													mWorld->RebuildTransparentMeshChunk(chunkTarget);
													mWorld->rebuildNearestChunks(chunkTarget, testPos2);
													mWorld->UpdateLightAreaIn(testPos2);
													mWorld->RebuildFullMeshChunk(chunkTarget);
												}
												else
												{
													mWorld->GetBlock(testPos2.x, testPos2.y, testPos2.z) = Fire::getID();
													mWorld->RebuildTransparentMeshChunk(chunkTarget);
													mWorld->rebuildNearestChunks(chunkTarget, testPos2);
													mWorld->SetLigtSourcePosition(testPos2.x, testPos2.y, testPos2.z, Fire::getID());
													mWorld->UpdateLightAreaIn(testPos2);
													mWorld->RebuildFullMeshChunk(chunkTarget);
												}

												mWorld->invAm[27 + barPosition] -= 1;
												if (mWorld->invAm[27 + barPosition] == 0)
												{
													mWorld->invAm[27 + barPosition] = -1;
													mWorld->invSt[27 + barPosition] = 0;
													mWorld->invId[27 + barPosition] = -1;
												}
												wasLight = true;
												wasUpdated = true;
											}
                                            if(mWorld->invId[27+barPosition] == GlassPanelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = GlassPanel::getID();

                                                fppCam->needUpdate = true;
                                                int chunkTarget = mWorld->getChunkId(testPos2);
                                                mWorld->RebuildTransparentMeshChunk(chunkTarget);
                                                mWorld->rebuildNearestChunks(chunkTarget,testPos2);

                                                mWorld->invAm[27+barPosition] -= 1;
                                                if(mWorld->invAm[27+barPosition] == 0)
                                                {
                                                   mWorld->invAm[27+barPosition] = -1;
                                                   mWorld->invSt[27+barPosition] = 0;
                                                   mWorld->invId[27+barPosition] = -1;
                                                }

                                                wasUpdated = true;
                                            }

                                            if(mWorld->invId[27+barPosition] == IronBarsItem::getID()) // if it is iron bars,kw
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = IronBars::getID();

                                                fppCam->needUpdate = true;
                                                int chunkTarget = mWorld->getChunkId(testPos2);
                                                mWorld->RebuildTransparentMeshChunk(chunkTarget);
                                                mWorld->rebuildNearestChunks(chunkTarget,testPos2);

                                                mWorld->invAm[27+barPosition] -= 1;
                                                if(mWorld->invAm[27+barPosition] == 0)
                                                {
                                                   mWorld->invAm[27+barPosition] = -1;
                                                   mWorld->invSt[27+barPosition] = 0;
                                                   mWorld->invId[27+barPosition] = -1;
                                                }

                                                wasUpdated = true;
                                            }

                                            if(mWorld->invId[27+barPosition] >= 300 && mWorld->invId[27+barPosition] <= 301) // if it is moshrooms
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 77 + (mWorld->invId[27+barPosition] - 300);
                                                fppCam->needUpdate = true;

                                                mWorld->invAm[27+barPosition] -= 1;
                                                if(mWorld->invAm[27+barPosition] == 0)
                                                {
                                                    mWorld->invAm[27+barPosition] = -1;
                                                    mWorld->invSt[27+barPosition] = 0;
                                                    mWorld->invId[27+barPosition] = -1;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] >= 294 && mWorld->invId[27+barPosition] <= 296) // if it is flowers or saplings
                                            {
                                                if(mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 1 || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 2) // if under is dirt or grass
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 119 + (mWorld->invId[27+barPosition] - 294); //very difficult expression >:
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == 309 || mWorld->invId[27+barPosition] == 310) // if it is another sapling
                                            {
                                                if(mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 1 || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 2 || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 29) // if under is dirt or grass
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 122 + (mWorld->invId[27+barPosition] - 309); //very difficult expression >:
                                                    fppCam->needUpdate = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                       mWorld->invAm[27+barPosition] = -1;
                                                       mWorld->invSt[27+barPosition] = 0;
                                                       mWorld->invId[27+barPosition] = -1;
                                                    }
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == 306) // if it is door item
                                            {
                                                if(mWorld->GetBlock(testPos2.x,testPos2.y+1,testPos2.z) == 0)
                                                {
                                                    if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 49; // 3
                                                        mWorld->GetBlock(testPos2.x,testPos2.y+1,testPos2.z) = 57;
                                                    }
                                                    if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 50; // 4
                                                        mWorld->GetBlock(testPos2.x,testPos2.y+1,testPos2.z) = 57;
                                                    }
                                                    if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 51; // 1
                                                        mWorld->GetBlock(testPos2.x,testPos2.y+1,testPos2.z) = 57;
                                                    }
                                                    if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 52; // 2
                                                        mWorld->GetBlock(testPos2.x,testPos2.y+1,testPos2.z) = 57;
                                                    }
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 49;
                                                        mWorld->GetBlock(testPos2.x,testPos2.y+1,testPos2.z) = 57;
                                                    }

                                                    fppCam->needUpdate = true;
                                                    int chunkTarget = mWorld->getChunkId(testPos2);
                                                    mWorld->RebuildTransparentMeshChunk(chunkTarget);
                                                    mWorld->rebuildNearestChunks(chunkTarget,testPos2);

                                                    wasUpdated = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                        mWorld->invAm[27+barPosition] = -1;
                                                        mWorld->invSt[27+barPosition] = 0;
                                                        mWorld->invId[27+barPosition] = -1;
                                                    }

                                                    animGo = true;
                                                    animDest = 0.0f;
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }


                                            if(mWorld->invId[27+barPosition] == BedItem::getID()) // if it is door item
                                            {
                                                bool placed = false;
                                                if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                {

                                                    if(mWorld->GetBlock(testPos2.x+1,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = BedBlock1::getID(); // 3
                                                        mWorld->GetBlock(testPos2.x+1,testPos2.y,testPos2.z) = BedBlock5::getID();

                                                        placed = true;
                                                    }
                                                }
                                                if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                {

                                                    if(mWorld->GetBlock(testPos2.x-1,testPos2.y,testPos2.z) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = BedBlock2::getID(); // 3
                                                        mWorld->GetBlock(testPos2.x-1,testPos2.y,testPos2.z) = BedBlock6::getID();

                                                        placed = true;
                                                    }
                                                }
                                                if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                {
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z+1) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = BedBlock3::getID(); // 3
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z+1) = BedBlock7::getID();

                                                        placed = true;
                                                    }
                                                }
                                                if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                {
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z-1) == 0)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = BedBlock4::getID(); // 3
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z-1) = BedBlock8::getID();

                                                        placed = true;
                                                    }
                                                }


                                                if(placed)
                                                {
                                                    fppCam->needUpdate = true;
                                                    int chunkTarget = mWorld->getChunkId(testPos2);
                                                    mWorld->RebuildTransparentMeshChunk(chunkTarget);
                                                    mWorld->rebuildNearestChunks(chunkTarget,testPos2);

                                                    wasUpdated = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                        mWorld->invAm[27+barPosition] = -1;
                                                        mWorld->invSt[27+barPosition] = 0;
                                                        mWorld->invId[27+barPosition] = -1;
                                                    }

                                                    animGo = true;
                                                    animDest = 0.0f;

                                                    return;
                                                }
                                            }


                                            if(mWorld->invId[27+barPosition] == ItemFrameItem::getID()) // if it is door item
                                            {
                                                bool placed = false;
                                                unsigned int facing = 0;

                                                if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                {
                                                    if(mWorld->BlockSolid(testPos2.x,testPos2.y,testPos2.z+1))
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = ItemFrame4::getID();

                                                        facing = 4;
                                                        placed = true;
                                                    }
                                                }
                                                if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                {
                                                    if(mWorld->BlockSolid(testPos2.x,testPos2.y,testPos2.z-1))
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = ItemFrame3::getID();

                                                        facing = 3;
                                                        placed = true;
                                                    }
                                                }
                                                if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                {
                                                    if(mWorld->BlockSolid(testPos2.x+1,testPos2.y,testPos2.z))
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = ItemFrame2::getID();

                                                        facing = 2;
                                                        placed = true;
                                                    }
                                                }
                                                if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                {
                                                    if(mWorld->BlockSolid(testPos2.x-1,testPos2.y,testPos2.z))
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = ItemFrame1::getID();

                                                        facing = 1;
                                                        placed = true;
                                                    }
                                                }

                                                if(placed)
                                                {
                                                    ItemFrameEntity* NewItemFrameEntity = new ItemFrameEntity(testPos2.x,testPos2.y,testPos2.z,facing);
                                                    mWorld->mItemFrames.push_back(NewItemFrameEntity);

                                                    fppCam->needUpdate = true;
                                                    int chunkTarget = mWorld->getChunkId(testPos2);

                                                    mWorld->RebuildTransparentMeshChunk(chunkTarget);

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                        mWorld->invAm[27+barPosition] = -1;
                                                        mWorld->invSt[27+barPosition] = 0;
                                                        mWorld->invId[27+barPosition] = -1;
                                                    }

                                                    wasUpdated = true;
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }


                                            if(mWorld->invId[27+barPosition] == 307) // if it is ladder item
                                            {
                                                bool placed = false;
                                                if(fppCam->horAngle > 45 && fppCam->horAngle < 135)
                                                {
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z+1) != 0 && mWorld->BlockSolid(testPos2.x,testPos2.y,testPos2.z+1) == true)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 90; // 3
                                                        placed = true;
                                                    }
                                                }
                                                if(fppCam->horAngle > 225 && fppCam->horAngle < 315)
                                                {
                                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z-1) != 0 && mWorld->BlockSolid(testPos2.x,testPos2.y,testPos2.z-1) == true)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 89;
                                                        placed = true;
                                                    }
                                                }
                                                if(fppCam->horAngle < 45 || fppCam->horAngle > 315)
                                                {
                                                    if(mWorld->GetBlock(testPos2.x+1,testPos2.y,testPos2.z) != 0 && mWorld->BlockSolid(testPos2.x+1,testPos2.y,testPos2.z) == true)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 88;
                                                        placed = true;
                                                    }
                                                }
                                                if(fppCam->horAngle > 135 && fppCam->horAngle < 225)
                                                {
                                                    if(mWorld->GetBlock(testPos2.x-1,testPos2.y,testPos2.z) != 0 && mWorld->BlockSolid(testPos2.x-1,testPos2.y,testPos2.z) == true)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 87;
                                                        placed = true;
                                                    }
                                                }

                                                if(placed == true)
                                                {
                                                    fppCam->needUpdate = true;
                                                    int chunkTarget = mWorld->getChunkId(testPos2);

                                                    mWorld->RebuildTransparentMeshChunk(chunkTarget);
                                                    mWorld->rebuildNearestChunks(chunkTarget,testPos2);

                                                    wasUpdated = true;

                                                    mWorld->invAm[27+barPosition] -= 1;
                                                    if(mWorld->invAm[27+barPosition] == 0)
                                                    {
                                                        mWorld->invAm[27+barPosition] = -1;
                                                        mWorld->invSt[27+barPosition] = 0;
                                                        mWorld->invId[27+barPosition] = -1;
                                                    }
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }


                                            if(mWorld->invId[27+barPosition] == 293) // if it is cane item
                                            {
                                                if(mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 7) //check if under is sand
                                                {
                                                    if(mWorld->GetBlock(testPos2.x-1,testPos2.y-1,testPos2.z) == 4 || mWorld->GetBlock(testPos2.x+1,testPos2.y-1,testPos2.z) == 4 || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z+1) == 4 || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z-1) == 4)
                                                    {
                                                        mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 33;
                                                        fppCam->needUpdate = true;

                                                        mWorld->invAm[27+barPosition] -= 1;
                                                        if(mWorld->invAm[27+barPosition] == 0)
                                                        {
                                                            mWorld->invAm[27+barPosition] = -1;
                                                            mWorld->invSt[27+barPosition] = 0;
                                                            mWorld->invId[27+barPosition] = -1;
                                                        }
                                                    }
                                                }
                                                else
                                                {
                                                    return;
                                                }
                                            }

                                            if(mWorld->invId[27+barPosition] == 291) // if it is water busket
                                            {
                                                int chunkTarget = mWorld->getChunkId(testPos2);
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 4;
                                                mWorld->invId[27+barPosition] = 290;
                                                mWorld->RebuildTransparentMeshChunk(chunkTarget);
                                                mWorld->rebuildNearestChunks(chunkTarget,testPos2);
                                                fppCam->needUpdate = true;
                                            }
                                            if(mWorld->invId[27+barPosition] == 292) // if it is lava busket
                                            {
                                                int chunkTarget = mWorld->getChunkId(testPos2);

                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 112;
                                                mWorld->invId[27+barPosition] = 290;

                                                if(mWorld->mChunks[chunkTarget]->periodicallyUpadted == false)
                                                {
                                                    mWorld->updatingChunks.push_back(chunkTarget);
                                                    mWorld->mChunks[chunkTarget]->periodicallyUpadted = true;
                                                }

                                                wasLight = true;
                                                mWorld->SetLigtSourcePosition(testPos2.x,testPos2.y,testPos2.z,Lava::getID());
                                                fppCam->needUpdate = true;
                                            }
                                        }

                                        // some animation stuff
                                        animGo = true;
                                        animDest = 0.0f;

                                        changeY = -0.1f;
                                        anim[0] = 1;

                                        mWorld->mainStatistics.blockPlaced += 1;

                                        int chunkTarget = mWorld->getChunkId(testPos2);

                                        if(IsBlockId(oldBlock) && oldBlock != -1)
                                        {
                                            mSoundMgr->PlayWalkSound(mWorld->blockTypes[oldBlock].soundType);
                                        }

                                        //rebuild
                                        if(wasLight == true)
                                        {
                                            mWorld->UpdateLightAreaIn(testPos2);
                                            mWorld->RebuildFullMeshChunk(chunkTarget);
                                            mWorld->rebuildNearestChunksForLight(chunkTarget,testPos2);
                                        }
                                        else
                                        {
                                            mWorld->initPutBlocksLight(testPos2.x, testPos2.y+2, testPos2.z);

                                            if(!wasUpdated)
                                            {
                                                if(mWorld->GetLightLevel(testPos2.x,testPos2.y,testPos2.z) != 0)
                                                {
                                                    mWorld->UpdateLightAreaIn(testPos2);
                                                }

                                                mWorld->RebuildFullMeshChunk(chunkTarget);

                                                if(mWorld->BlockTransparent(testPos2.x,testPos2.y,testPos2.z) || mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == WoodenFence::getID())
                                                {
                                                    mWorld->rebuildNearestChunks(chunkTarget,testPos2);
                                                }
                                            }
                                        }
                                    }
                                    fppCam->needUpdate = true;
                                }
                            }
                        }
                        break;
                    }
                }

                if(ceilf(mWorld->HG) < 20.0f)
                {
                    bool foodEaten = false;

                    if (mWorld->invId[27+barPosition] == 288) ///BREAD
                    {
                        foodEaten = true;
                        mWorld->HG += 5;
                    }

                    if (mWorld->invId[27+barPosition] == 315) ///COOKIE
                    {
                        foodEaten = true;
                        mWorld->HG += 1;
                        mWorld->HP += 1;
                    }

                    if (mWorld->invId[27+barPosition] == RottenFlesh::getID()) ///ROTTEN FLESH
                    {
                        foodEaten = true;
                        mWorld->HG += 1;

                        if(rand() % 100 < 45)
                        {
                            HurtPlayer(1);
                        }
                    }

                    if (mWorld->invId[27+barPosition] == Potato::getID()) ///POTATO
                    {
                        foodEaten = true;
                        mWorld->HG += 1;
                    }

                    if (mWorld->invId[27+barPosition] == PoisonedPotato::getID()) ///POSIONED POTATO
                    {
                        foodEaten = true;
                        mWorld->HG += 1;

                        HurtPlayer(4);
                    }

                    if (mWorld->invId[27+barPosition] == BakedPotato::getID()) ///BAKED POTATO
                    {
                        foodEaten = true;
                        mWorld->HG += 5;
                    }

                    if (mWorld->invId[27+barPosition] == Carrot::getID()) ///CARROT
                    {
                        foodEaten = true;
                        mWorld->HG += 3;
                    }

                    if (mWorld->invId[27+barPosition] == 311) ///MELON
                    {
                        foodEaten = true;
                        mWorld->HG += 2;
                    }

                    if (mWorld->invId[27+barPosition] == 317) ///STEAK
                    {
                        foodEaten = true;
                        mWorld->HG += 8;
                    }

                    if (mWorld->invId[27+barPosition] == 284) ///APPLE
                    {
                        foodEaten = true;
                        mWorld->HG += 4;
                    }

                    if (mWorld->invId[27+barPosition] == 285) ///GOLDEN APPLE
                    {
                        foodEaten = true;
                        mWorld->HG = 20;
                        mWorld->HP = 20;
                    }

                    if (mWorld->invId[27+barPosition] == 303) ///MOOSHROOM BOWL
                    {
                        foodEaten = true;
                        mWorld->HG += 6;
                    }

                    if(foodEaten)
                    {
                        mSoundMgr->PlayEatSound();
                        mWorld->mainStatistics.foodEaten += 1;

                        changeY = -0.20f;
                        anim[0] = 1;

                        Vector3 rayParticleDir = fppCam->m_vView - fppCam->m_vPosition;
                        rayParticleDir.normalize();

                        Vector3 particlePos = fppCam->m_vPosition + (rayParticleDir * 0.38f);

                        if(Particle::ActiveCount() <= 52)
                        {
                            for(int k = 0; k <= 5; k++) // 5 particles
                            {
                                Particle* FoodPart = Particle::Spawn(mWorld,"item crack",Vector3(particlePos.x,particlePos.y-0.18f,particlePos.z));
                                FoodPart->var1 = mWorld->invId[27+barPosition]-250;
                            }
                        }

                        if(mWorld->HG >= 20)
                        {
                            mWorld->HG = 19.999f;
                        }

                        if(mWorld->invId[27+barPosition] == MooshroomBowl::getID())
                        {
                            mWorld->invId[27+barPosition] = 302; // turn it into simple stackable bowl
                            mWorld->invSt[27+barPosition] = 1;
                        }
                        else
                        {
                            mWorld->invAm[27+barPosition] -= 1;
                            if (mWorld->invAm[27+barPosition] == 0)
                            {
                                mWorld->invAm[27+barPosition] = -1;
                                mWorld->invId[27+barPosition] = -1;
                                mWorld->invSt[27+barPosition] = 0;
                            }
                        }
                        return;
                    }
                }
            }


            if(keyPressed(InputHelper::Instance()->getButtonToAction(13)) || tryToDt == true)
            {
                tryToDt = false;

                //remove cube
                Vector3 rayDir = fppCam->m_vView - fppCam->m_vPosition;
                rayDir.normalize();
                Vector3 rayOrigin = fppCam->m_vPosition;
                float punchReach = 3.5f;
                float nearestMobT = punchReach + 1.0f;
                float punchPower = 0.7f;
                LameMob* punchMob = FindMobAlongRay(mWorld, rayOrigin, rayDir, punchReach, &nearestMobT, &punchPower);

                //we are takin our positiona and view vector
                Vector3 testPos2;
                testPos2 = Vector3(-1,-1,-1);

                BoundingBox collider = mWorld->GetBlockCollideBox(testPos2.x,testPos2.y,testPos2.z, mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z));
                BoundingBox testBox = BoundingBox(testPos2.x+collider.min.x, testPos2.y+collider.min.y, testPos2.z+collider.min.z, testPos2.x+collider.max.x, testPos2.y+collider.max.y, testPos2.z+collider.max.z);

                //we are moving slowly to the target +=0.5
                // One iteration per cell the look crosses (see CraftWorld::NextCellDistance), clipped
                // against that cell's selection box: the punch has to hit the same shape the crosshair
                // outlines, or a 1 px plate can be aimed at and not broken.
                Ray punchRay(fppCam->m_vPosition, rayDir);

                for(float i = 0.0f; i < 5.25f; i = CraftWorld::NextCellDistance(fppCam->m_vPosition, rayDir, i))
                {
                    testPos2 = fppCam->m_vPosition + (rayDir * i);

                    collider = mWorld->GetBlockSelectionBox(testPos2.x,testPos2.y,testPos2.z, mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z));
                    testBox = BoundingBox(floorf(testPos2.x)+0.5f+collider.min.x, floorf(testPos2.y)+0.5f+collider.min.y, floorf(testPos2.z)+0.5f+collider.min.z, floorf(testPos2.x)+0.5f+collider.max.x, floorf(testPos2.y)+0.5f+collider.max.y, floorf(testPos2.z)+0.5f+collider.max.z);

                    //check if we touch something
                    float punchDist = -1.0f;
                    punchRay.hasIntersected(testBox, punchDist);

                    if(mWorld->BlockEditable(testPos2.x,testPos2.y,testPos2.z) && punchDist != -1.0f && i <= nearestMobT)
                    {
                        if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == NoteBlock::getID())
                        {
                            unsigned int noteBlockId = mWorld->FindNoteBlockID(testPos2);

                            if(noteBlockId != -1)
                            {
                                if(mWorld->GetBlock(testPos2.x,testPos2.y+1,testPos2.z) == 0)
                                {
                                    NoteBlockEntity* TestNoteBlock = mWorld->mNoteBlocks[noteBlockId];
                                    mSoundMgr->PlayNoteSound(!(mWorld->BlockMaterial(testPos2.x,testPos2.y-1,testPos2.z) == 1 || mWorld->BlockMaterial(testPos2.x,testPos2.y-1,testPos2.z) == 8),TestNoteBlock->GetPitch());

                                    float red, blue, green;
                                    if(TestNoteBlock->GetNote() <= 12)
                                    {
                                        blue = 1.0 - (TestNoteBlock->GetNote()/12.0f);
                                        green = (TestNoteBlock->GetNote()/12.0f);
                                        red = 0.0f;
                                    }
                                    else
                                    {
                                        blue = 0.0f;
                                        green = 1.0 - ((TestNoteBlock->GetNote()-12)/12.0f);
                                        red = ((TestNoteBlock->GetNote()-12)/12.0f);
                                    }
                                    Particle* Note = Particle::Spawn(mWorld,"note",Vector3((int)testPos2.x+0.5f,(int)testPos2.y+1.1f,(int)testPos2.z+0.5f));
                                    Note->SetColor(Vector3(red,green,blue));
                                }
                            }
                        }

                        if (startDt == false)
                        {
                            testPos1.x  = testPos2.x;
                            testPos1.y  = testPos2.y;
                            testPos1.z  = testPos2.z;
                            dET = 1.0f;
                            dT = 0.0f;
                            switch(mWorld->BlockMaterial(testPos1.x,testPos1.y,testPos1.z)) // fetch time of block breaking
                            {
                                case 1: // wood
                                if(mWorld->invId[27+barPosition] == WoodenAxe::getID()) // wooden axe
                                {
                                    dET = 1.5f; // 1.5 СЃРµРєСѓРЅРґС‹
                                }
                                else if(mWorld->invId[27+barPosition] == StoneAxe::getID()) // stone axe
                                {
                                    dET = 0.75f;
                                }
                                else if(mWorld->invId[27+barPosition] == IronAxe::getID()) // iron axe
                                {
                                    dET = 0.5f;
                                }
                                else if(mWorld->invId[27+barPosition] == DiamondAxe::getID()) // diamond axe
                                {
                                    dET = 0.4f;
                                }
                                else if(mWorld->invId[27+barPosition] == GoldenAxe::getID()) // golden axe
                                {
                                    dET = 0.25f;
                                }
                                else
                                {
                                    dET = 3.0f;
                                }
                                break;

                                case 2: // stone
                                if(mWorld->invId[27+barPosition] == WoodenPickaxe::getID())
                                {
                                    dET = 1.5f;
                                }
                                else if(mWorld->invId[27+barPosition] == StonePickaxe::getID())
                                {
                                    dET = 0.75f;
                                }
                                else if(mWorld->invId[27+barPosition] == IronPickaxe::getID())
                                {
                                    dET = 0.5f;
                                }
                                else if(mWorld->invId[27+barPosition] == DiamondPickaxe::getID())
                                {
                                    dET = 0.4f;
                                }
                                else if(mWorld->invId[27+barPosition] == GoldenPickaxe::getID())
                                {
                                    dET = 0.25f;
                                }
                                else
                                {
                                    dET = 10.0f;
                                }
                                break;

                                case 3: // soil
                                if(mWorld->invId[27+barPosition] == 260)
                                {
                                    dET = 0.45f;
                                }
                                else if(mWorld->invId[27+barPosition] == 261)
                                {
                                    dET = 0.25f;
                                }
                                else if(mWorld->invId[27+barPosition] == 262)
                                {
                                    dET = 0.15f;
                                }
                                else if(mWorld->invId[27+barPosition] == 263)
                                {
                                    dET = 0.15f;
                                }
                                else if(mWorld->invId[27+barPosition] == 264)
                                {
                                    dET = 0.1f;
                                }
                                else
                                {
                                    dET = 0.9f;
                                }
                                break;

                                case 4: // leaves
                                if(mWorld->invId[27+barPosition] == 275)
                                {
                                    dET = 0.1f;
                                }
                                else
                                {
                                    dET = 0.4f;
                                }
                                break;

                                case 5: // obsidian
                                if(mWorld->invId[27+barPosition] == 253)
                                {
                                    dET = 15.0f;
                                }
                                else
                                {
                                    dET = 250.0f;
                                }
                                break;

                                case 6: // bedrock
                                dET = 36000.0f;
                                break;

                                break;

                                case 7: // ores
                                if(mWorld->invId[27+barPosition] == 250)
                                {
                                    dET = 2.25f;
                                }
                                else if(mWorld->invId[27+barPosition] == 251)
                                {
                                    dET = 1.15f;
                                }
                                else if(mWorld->invId[27+barPosition] == 252)
                                {
                                    dET = 0.75f;
                                }
                                else if(mWorld->invId[27+barPosition] == 253)
                                {
                                    dET = 0.6f;
                                }
                                else if(mWorld->invId[27+barPosition] == 254)
                                {
                                    dET = 0.4f;
                                }
                                else
                                {
                                    dET = 15.0f;
                                }
                                break;

                                case 8: // strong wood
                                if(mWorld->invId[27+barPosition] == 265) // wooden axe
                                {
                                    dET = 1.9f; //
                                }
                                else if(mWorld->invId[27+barPosition] == 266) // stone axe
                                {
                                    dET = 0.95f;
                                }
                                else if(mWorld->invId[27+barPosition] == 267) // iron axe
                                {
                                    dET = 0.65f;
                                }
                                else if(mWorld->invId[27+barPosition] == 268) // diamond axe
                                {
                                    dET = 0.5f;
                                }
                                else if(mWorld->invId[27+barPosition] == 269) // golden axe
                                {
                                    dET = 0.35f;
                                }
                                else
                                {
                                    dET = 3.75f;
                                }
                                break;

                                case 9: // strong stone
                                if(mWorld->invId[27+barPosition] == 250)
                                {
                                    dET = 2.65f;
                                }
                                else if(mWorld->invId[27+barPosition] == 251)
                                {
                                    dET = 1.35f;
                                }
                                else if(mWorld->invId[27+barPosition] == 252)
                                {
                                    dET = 0.9f;
                                }
                                else if(mWorld->invId[27+barPosition] == 253)
                                {
                                    dET = 0.7f;
                                }
                                else if(mWorld->invId[27+barPosition] == 254)
                                {
                                    dET = 0.45f;
                                }
                                else
                                {
                                    dET = 17.5f;
                                }
                                break;

                                case 10: // metal
                                if(mWorld->invId[27+barPosition] == 250)
                                {
                                    dET = 25.0f;
                                }
                                else if(mWorld->invId[27+barPosition] == 251)
                                {
                                    dET = 10.0f;
                                }
                                else if(mWorld->invId[27+barPosition] == 252)
                                {
                                    dET = 5.0f;
                                }
                                else if(mWorld->invId[27+barPosition] == 253)
                                {
                                    dET = 2.5f;
                                }
                                else if(mWorld->invId[27+barPosition] == 254)
                                {
                                    dET = 25.0f;
                                }
                                else
                                {
                                    dET = 25.0f;
                                }
                                break;

                                default:
                                dET = 0.3f;
                                break;

								case 11: // Plants like Tall Grass
                                if(mWorld->invId[27+barPosition] == 275)
                                {
                                    dET = 0.001f;
                                }
                                else
                                {
                                    dET = 0.001f;
                                }
                                break;

                            }
                            dT = 0;
                            startDt = true;
                        }
                        break;
                    }
                }
                if(punchMob != NULL && startDt == false)
                {
                    ApplyMeleeHit(mWorld, mSoundMgr, barPosition, punchMob, punchPower, dt, true);
                    // Java charges exhaustion for a swing that lands (attackTargetEntityWithCurrentItem).
                    mWorld->AddExhaustion(EXHAUST_ATTACK);
                    animGo = true;
                    animDest = 0.0f;
                    changeY = -0.1f;
                    anim[0] = 1;
                    return;
                }
            }

            //remove cube
            if(keyHold(InputHelper::Instance()->getButtonToAction(13)))
            {
                if (startDt == true)
                {
                    if(animGo == false)
                    {
                        mSoundMgr->PlayDigSound(mWorld->BlockSoundAtPos(Vector3(testPos1.x,testPos1.y,testPos1.z)));

                        animGo = true;
                        animDest = 0.0f;
                    }

                    if(headInWater) // slow removing under water
                    {
                        dT < dET ? dT += dt*0.2 : dT = 0;
                    }
                    else
                    {
                        dT < dET ? dT += dt : dT = 0;
                    }

                    dStd = ceilf(dT*(100/(float)dET))/10.0f;
                    //remove block
                    if (dT >= dET)
                    {
                        bool wasLight = false;
                        int oldBlock = 0;
                        oldBlock = mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z);
                        if(oldBlock != -1)//if it's light block
                        {
                            if(mWorld->LightSourceBlock(oldBlock))
                            {
                                wasLight = true;
                            }
                        }

                        if(oldBlock >= ChestBlock1::getID() && oldBlock <= ChestBlock4::getID()) // chest destroying
                        {
                            chestId = FindChestId(testPos1.x,testPos1.y,testPos1.z);
                            if(chestId != -1)
                            {
                                // drop all items in chest
                                Chest* TestChest = mWorld->mChests[chestId];
                                for(int i = 0; i < 27; i++) //Chest has 27 slots (0..26)
                                {
                                    if(TestChest->chestSlotId[i] > 0)
                                    {
                                        Vector3 dropVelocity = Vector3(0,0,0);

                                        dropVelocity.x = (-15+rand()%30)*0.1f;
                                        dropVelocity.y = 3.0f;
                                        dropVelocity.z = (-15+rand()%30)*0.1f;

                                        if(TestChest->chestSlotAm[i] > 0)
                                        {
                                            mWorld->DropThisNoAlign(TestChest->chestSlotId[i],TestChest->chestSlotAm[i],TestChest->chestSlotSt[i],Vector3(testPos1.x+0.5f,testPos1.y+0.25f,testPos1.z+0.5f),dropVelocity);

                                            TestChest->chestSlotAm[i] = -1;
                                            TestChest->chestSlotSt[i] = 0;
                                            TestChest->chestSlotId[i] = -1;
                                        }
                                    }
                                }

                                delete mWorld->mChests[chestId];
                                mWorld->mChests.erase(mWorld->mChests.begin()+chestId);
                            }
                        }

                        if(oldBlock == NoteBlock::getID())
                        {
                            int noteBlockId = mWorld->FindNoteBlockID(testPos1);

                            if(noteBlockId != -1)
                            {
                                delete mWorld->mNoteBlocks[noteBlockId];
                                mWorld->mNoteBlocks.erase(mWorld->mNoteBlocks.begin()+noteBlockId);
                            }
                        }

                        if(oldBlock == JukeBox::getID())
                        {
                            int jukeboxId = mWorld->FindJukeboxID(testPos1);

                            if(jukeboxId != -1)
                            {
                                if(mWorld->mJukeboxes[jukeboxId]->GetRecord() != -1)
                                {
                                    mWorld->DropThis(mWorld->mJukeboxes[jukeboxId]->GetRecord(), 1, false, Vector3((int)testPos1.x+0.5f,(int)testPos1.y+0.5f,(int)testPos1.z+0.5f) );
                                }

                                delete mWorld->mJukeboxes[jukeboxId];
                                mWorld->mJukeboxes.erase(mWorld->mJukeboxes.begin()+jukeboxId);
                                mSoundMgr->StopDiskTrack();
                            }
                        }

                        if(oldBlock == Spawner::getID())
                        {
                            int MonsterSpawnerId = mWorld->FindMonsterSpawnerID(testPos1);

                            if(MonsterSpawnerId != -1)
                            {
                                delete mWorld->mMonsterSpawners[MonsterSpawnerId];
                                mWorld->mMonsterSpawners.erase(mWorld->mMonsterSpawners.begin()+MonsterSpawnerId);
                            }
                        }

                        if(oldBlock >= ItemFrame1::getID() && oldBlock <= ItemFrame4::getID())
                        {
                            mWorld->DestroyItemFrameEntity(testPos1);
                        }

                        if(oldBlock == FurnaceOff::getID() || oldBlock == FurnaceOn::getID()) // furnace destroying
                        {
                            furnaceId = FindFurnaceId(testPos1.x,testPos1.y,testPos1.z);
                            if(furnaceId != -1)
                            {
                                // drop all items in furnace
                                Furnace* TestFurnace = mWorld->mFurnaces[furnaceId];
                                for(int i = 0; i <= 2; i++)
                                {
                                    if(TestFurnace->furnaceSlotId[i] > 0)
                                    {
                                        Vector3 dropVelocity = Vector3(0,0,0);

                                        dropVelocity.x = (-15+rand()%30)*0.1f;
                                        dropVelocity.y = 3.0f;
                                        dropVelocity.z = (-15+rand()%30)*0.1f;

                                        if(TestFurnace->furnaceSlotAm[i] > 0)
                                        {
                                            mWorld->DropThisNoAlign(TestFurnace->furnaceSlotId[i],TestFurnace->furnaceSlotAm[i],TestFurnace->furnaceSlotSt[i],Vector3(testPos1.x+0.5f,testPos1.y+0.25f,testPos1.z+0.5f),dropVelocity);

                                            TestFurnace->furnaceSlotAm[i] = -1;
                                            TestFurnace->furnaceSlotSt[i] = 0;
                                            TestFurnace->furnaceSlotId[i] = -1;
                                        }
                                    }
                                }
                                delete mWorld->mFurnaces[furnaceId];
                                mWorld->mFurnaces.erase(mWorld->mFurnaces.begin()+furnaceId);
                            }
                        }

                        // Breaking half a piston takes the other half with it: an extended base would
                        // otherwise leave its head standing in the next cell with nothing to retract
                        // it, and a broken head would leave a base stuck open. This runs *before* the
                        // cell is emptied below, because the direction it needs lives in the state
                        // nibble that ClearStateCellNibble is about to zero.
                        mWorld->BreakPistonPartner((int)testPos1.x,(int)testPos1.y,(int)testPos1.z);

                        if(oldBlock >= 57 && oldBlock <= 58)
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x,testPos1.y-1,testPos1.z) = 0;
                        }

                        if(oldBlock >= 49 && oldBlock <= 56)
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x,testPos1.y+1,testPos1.z) = 0;
                        }

                        if(oldBlock == BedBlock1::getID())
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x+1,testPos1.y,testPos1.z) = 0;
                        }
                        if(oldBlock == BedBlock2::getID())
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x-1,testPos1.y,testPos1.z) = 0;
                        }
                        if(oldBlock == BedBlock3::getID())
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z+1) = 0;
                        }
                        if(oldBlock == BedBlock4::getID())
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z-1) = 0;
                        }

                        if(oldBlock == BedBlock5::getID())
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x-1,testPos1.y,testPos1.z) = 0;
                        }
                        if(oldBlock == BedBlock6::getID())
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x+1,testPos1.y,testPos1.z) = 0;
                        }
                        if(oldBlock == BedBlock7::getID())
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z-1) = 0;
                        }
                        if(oldBlock == BedBlock8::getID())
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z+1) = 0;
                        }

                        if(oldBlock == IceBlock::getID())
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = WaterBlock::getID();
                        }
                        else
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            // The cell is air now, so its settings nibble is no longer state as far as the
                            // light readers are concerned -- see ClearStateCellNibble. Zero it before
                            // anything reads the cell, or a high delay setting is read back as a light
                            // level and shadows the spot it was broken on.
                            if(CraftWorld::IsStateCellBlock(oldBlock))
                            {
                                mWorld->ClearStateCellNibble((int)testPos1.x,(int)testPos1.y,(int)testPos1.z);
                            }

                            // A nether portal hangs off its obsidian ring, so pulling a frame block out
                            // takes the portal with it. Java gets this from a scheduled tick that
                            // re-measures the frame; with no block tick here, the break does it.
                            if(oldBlock == Obsidian::getID())
                            {
                                mWorld->BreakPortalNear((int)testPos1.x,(int)testPos1.y,(int)testPos1.z);
                            }
                            // Removing any redstone source or one-way part changes what everything around
                            // it reads, so reseed: dust, a block of redstone, a redstone torch, a
                            // repeater and now a switch or a lamp all have to be announced. The torch case
                            // also covers the torch being picked up while it was the mount's only reason to
                            // be powered. A switch (button or lever) earns its place because while it is on
                            // it weak-powers all six neighbours, so removing it leaves exactly the wire it was
                            // driving reading stale; a lamp earns its place as a *conductor* -- a block of
                            // redstone behind a lamp strongly powers the lamp, and dust pointing into the lamp
                            // reads that back, so pulling the lamp out has to reseed even though a lamp never
                            // emits power in either state.
                            if(oldBlock == Redstoneoff::getID() || oldBlock == RedstoneBlock::getID()
                               || CraftWorld::IsRedstoneTorchBlock(oldBlock) || CraftWorld::IsRepeaterBlock(oldBlock)
                               || CraftWorld::IsSwitchBlock(oldBlock) || CraftWorld::IsRedstoneLampBlock(oldBlock))
                            {
                                mWorld->UpdateRedstonePower((int)testPos1.x,(int)testPos1.y,(int)testPos1.z);
                            }

                            int upper_block = mWorld->GetBlock(testPos1.x,testPos1.y+1,testPos1.z);

                            if(upper_block != 0)
                            {
                                if(mWorld->BlockSolid(testPos1.x,testPos1.y+1,testPos1.z) == false &&
                                   !(upper_block >= Door1::getID() && upper_block <= Door8::getID()) &&
                                   !(upper_block >= ItemFrame1::getID() && upper_block <= ItemFrame4::getID()) &&
                                   !CraftWorld::IsTorchBlock(upper_block) &&
                                   upper_block != WaterBlock::getID() &&
                                   upper_block != Lava::getID() &&
                                   upper_block != CaneBlock::getID() &&
                                   !(upper_block >= Ladder1::getID() && upper_block <= Ladder4::getID()) &&
                                   !(upper_block >= TrapdoorOpen1::getID() && upper_block <= TrapdoorClosed4::getID()))
                                {
                                    Vector3 testPos2 = Vector3(testPos1.x,testPos1.y+1,testPos1.z);

                                    mWorld->DropThis(mWorld->BlockLoot(testPos2.x,testPos2.y,testPos2.z),testPos2);
                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 0;

                                    if(mWorld->ChunksEqual(testPos1,testPos2) == false)
                                    {
                                        int chunkTarget = mWorld->getChunkId(testPos2);
                                        mWorld->AddChunkToUpdate(chunkTarget);
                                    }
                                }

                                for(int checkY = testPos1.y+1; checkY <= testPos1.y+5; checkY++)
                                {
                                    Vector3 testPos2 = Vector3(testPos1.x,checkY,testPos1.z);
                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) != CaneBlock::getID() && mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) != CactusBlock::getID())
                                    {
                                        if(mWorld->ChunksEqual(testPos1, testPos2) == false)
                                        {
                                            int chunkTarget = mWorld->getChunkId(testPos2);
                                            mWorld->AddChunkToUpdate(chunkTarget);
                                        }
                                        break;
                                    }

                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == CaneBlock::getID())
                                    {
                                        mWorld->DropThis(CaneItem::getID(),testPos2);
                                    }
                                    if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) == CactusBlock::getID())
                                    {
                                        mWorld->DropThis(CactusBlock::getID(),testPos2);
                                    }
                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 0;
                                }
                            }
                        }

                        if(IsItemId(mWorld->invId[27+barPosition]) && mWorld->invId[27+barPosition] <= 275) // tool breaking
                        {
                            mWorld->invAm[27+barPosition] -= 1;
                            if(mWorld->invAm[27+barPosition] <= 0)
                            {
                                Vector3 rayParticleDir = fppCam->m_vView - fppCam->m_vPosition;
                                rayParticleDir.normalize();

                                Vector3 particlePos = fppCam->m_vPosition + (rayParticleDir * 0.38f);

                                if(Particle::ActiveCount() <= 52)
                                {
                                    for(int k = 0; k <= 5; k++) // 5 particles
                                    {
                                        Particle* ToolPart = Particle::Spawn(mWorld,"item crack",Vector3(particlePos.x,particlePos.y-0.18f,particlePos.z));
                                        if(ToolPart)
                                        {
                                            ToolPart->var1 = mWorld->invId[27+barPosition]-250;
                                        }
                                    }
                                }

                                mWorld->invId[27+barPosition] = -1;
                                mWorld->invAm[27+barPosition] = -1;
                                mWorld->invSt[27+barPosition] = 0;
                                mSoundMgr->PlayBreakSound();
                            }
                        }

                        switch(oldBlock)
                        {
                        case 9: // oak leaves
                            if(mWorld->invId[27+barPosition] != 275)
                            {
                                if ((rand() % 50) == 0)
                                {
                                    mWorld->DropThis(284,testPos1);
                                }

                                if ((rand() % 15) == 0)
                                {
                                    mWorld->DropThis(296,testPos1);
                                }
                            }
                            else
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            }
                        break;

                        case 37: // spruce leaves
                            if(mWorld->invId[27+barPosition] != 275)
                            {
                                if (rand() % 10 == 1)
                                {
                                    mWorld->DropThis(309,testPos1);
                                }
                            }
                            else
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            }
                        break;

                        case 38: // birch leaves
                            if(mWorld->invId[27+barPosition] != 275)
                            {
                                if (rand() % 16 == 1)
                                {
                                    mWorld->DropThis(310,testPos1);
                                }
                            }
                            else
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            }
                        break;

                        case 139: // wheat
                            mWorld->DropThis(Wheat::getID(),testPos1);
                            mWorld->DropThis(WheatSeeds::getID(),1+rand()%2,testPos1);
                        break;

                        case 93: // snow cover
                            if(mWorld->invId[27+barPosition] >= 260 && mWorld->invId[27+barPosition] <= 264)
                            {
                                mWorld->DropThis(299,4,testPos1);
                            }
                        break;

                        // ore digging with special tools
                        case 3: // stone
                            if(mWorld->invId[27+barPosition] >= WoodenPickaxe::getID() && mWorld->invId[27+barPosition] <= GoldenPickaxe::getID())
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            }
                        break;

                        case 36: // cobblestone
                            if(mWorld->invId[27+barPosition] >= WoodenPickaxe::getID() && mWorld->invId[27+barPosition] <= GoldenPickaxe::getID())
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            }
                        break;

                        case 41: // iron ore
                            if(mWorld->invId[27+barPosition] >= StonePickaxe::getID() && mWorld->invId[27+barPosition] <= GoldenPickaxe::getID())
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            }
                        break;

                        case 42: // coal ore
                            if(mWorld->invId[27+barPosition] >= WoodenPickaxe::getID() && mWorld->invId[27+barPosition] <= GoldenPickaxe::getID())
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            }
                        break;

                        case 43: // diamond ore
                            if(mWorld->invId[27+barPosition] >= IronPickaxe::getID() && mWorld->invId[27+barPosition] <= GoldenPickaxe::getID())
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            }
                        break;

                        case 44: // redstone ore
                            if(mWorld->invId[27+barPosition] >= IronPickaxe::getID() && mWorld->invId[27+barPosition] <= GoldenPickaxe::getID())
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),3,testPos1);
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),1+rand()%2,testPos1);
                            }
                        break;

                        case 45: // lapis ore
                            if(mWorld->invId[27+barPosition] >= StonePickaxe::getID() && mWorld->invId[27+barPosition] <= GoldenPickaxe::getID())
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),3,testPos1);
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),1+rand()%5,testPos1);
                            }
                        break;

                        case 46: // obsidian
                            if(mWorld->invId[27+barPosition] == DiamondPickaxe::getID() )
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),3,testPos1);
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),1+rand()%5,testPos1);
                            }
                        break;

                        case 5: // golden ore
                            if(mWorld->invId[27+barPosition] >= IronPickaxe::getID() && mWorld->invId[27+barPosition] <= GoldenPickaxe::getID())
                            {
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            }
                        break;

                        case 99: // clay block
                            mWorld->DropThis(mWorld->LootBlock(oldBlock),2,testPos1);
                            mWorld->DropThis(mWorld->LootBlock(oldBlock),2,testPos1);
                        break;

                        case 81: // melon block
                            mWorld->DropThis(mWorld->LootBlock(oldBlock),2,testPos1);

                            mWorld->DropThis(mWorld->LootBlock(oldBlock),1+rand()%5,testPos1);
                        break;

                        case 133: // final melon plant
                            mWorld->DropThis(MelonSeeds::getID(),1+rand()%3,testPos1);
                        break;

                        case 181: // final pumpkin plant
                            mWorld->DropThis(PumpkinSeeds::getID(),1+rand()%3,testPos1);
                        break;

                        case 185: // matrue potato
                            mWorld->DropThis(Potato::getID(),1,testPos1);
                            mWorld->DropThis(Potato::getID(),rand() % 4,testPos1);

                            if(rand() % 100 <= 1)
                            {
                                mWorld->DropThis(PoisonedPotato::getID(),1,testPos1);
                            }
                        break;

                        case 189: // matrue carrot
                            mWorld->DropThis(Carrot::getID(),1,testPos1);
                            mWorld->DropThis(Carrot::getID(),rand() % 4,testPos1);
                        break;

                        case 33: // cane
                            mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                            for(int checkY = testPos1.y+1; checkY <= testPos1.y+5; checkY++)
                            {
                                Vector3 testPos2 = Vector3(testPos1.x,checkY,testPos1.z);
                                if(mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) != 33)
                                {
                                    if(mWorld->ChunksEqual(testPos1, testPos2) == false)
                                    {
                                        int chunkTarget = mWorld->getChunkId(testPos2);
                                        mWorld->AddChunkToUpdate(chunkTarget);
                                    }
                                    break;
                                }

                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 0;
                                mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos2);
                            }
                        break;

                        case 116: // tall grass
                        if (rand() % 2 == 0)
                        {
                            mWorld->DropThis(WheatSeeds::getID(),testPos1);
                        }
                        break;

                        case 113: // gravel
                        if (rand() % 12 == 0)
                        {
                            mWorld->DropThis(Flint::getID(),testPos1);
                        }
                        else
                        {
                            mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                        }
                        break;

                        default: // all other blocks
                            mWorld->DropThis(mWorld->LootBlock(oldBlock),testPos1);
                        break;
                        }

                        mWorld->mainStatistics.blockDestroyed += 1;
                        if(mWorld->mainStatistics.blockDestroyed == 1)
                        {
                            tutorialTimer = 10.0f;
                        }

                        if(oldBlock == GlassBlock::getID() || oldBlock == IceBlock::getID()|| oldBlock == GlassPanel::getID())
                        {
                            mSoundMgr->PlayGlassSound();
                        }
                        else
                        {
                            if(oldBlock >= 0 && oldBlock < mWorld->blockTypes.size())
                            {
                                mSoundMgr->PlayEndDigSound(mWorld->blockTypes[oldBlock].soundType);
                            }
                        }

                        if(oldBlock >= 0)
                        {
                            pendingCrackId = oldBlock;
                            pendingCrackX = testPos1.x;
                            pendingCrackY = testPos1.y;
                            pendingCrackZ = testPos1.z;
                            pendingBreakWasLight = wasLight;
                            pendingBreakChunk = mWorld->getChunkId(testPos1);
                            pendingBreakStage = 1;
                        }

                        if(mWorld->HG > 0.01)
                        {
                            mWorld->HG -= (1.0f)/64.0f;
                        }

                        fppCam->needUpdate = true;
                        testPos1 = Vector3(-1,-1,-1);
                        startDt = false;
                        tryToDt = true;
                        dT = 0;
                        dET = 0;
                        chestId = -1;
                        dStd = -1;
                    }
                }
            }
            else
            {
                if (startDt != false)
                {
                    dT = 0;
                    dStd = -1;
                    startDt = false;
                    chestId = -1;
                }

            }
            }

            /// INVENTORY

            if(invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true)
            {
                //Craft item
                if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
                {
                    if(invEn == true)
                    {
                        for(int i = 0; i <= 3; i++)
                        {
                            if(craftSlotId[i] > 0)
                            {
                                Vector3 dropVelocity = Vector3(0,0,0);

                                dropVelocity.x = (cosf((fppCam->horAngle/180.0f)*PI))*4.0f;
                                dropVelocity.y = 2.0f;
                                dropVelocity.z = (sinf((fppCam->horAngle/180.0f)*PI))*4.0f;

                                if(craftSlotAm[i] > 0)
                                {
                                    mWorld->DropThisNoAlign(craftSlotId[i],craftSlotAm[i],craftSlotSt[i],Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y-0.25f,fppCam->m_vPosition.z),dropVelocity);

                                    craftSlotAm[i] = -1;
                                    craftSlotSt[i] = 0;
                                    craftSlotId[i] = -1;
                                }
                            }
                        }
                        craftItemId = -1;
                        craftItemAm = -1;
                        craftItemSt = false;
                    }

                    if(craft3xEn == true)
                    {
                        for(int i = 0; i <= 8; i++)
                        {
                            if(craftSlotId3[i] > 0)
                            {
                                Vector3 dropVelocity = Vector3(0,0,0);

                                dropVelocity.x = (cosf((fppCam->horAngle/180.0f)*PI))*4.0f;
                                dropVelocity.y = 2.0f;
                                dropVelocity.z = (sinf((fppCam->horAngle/180.0f)*PI))*4.0f;

                                if(craftSlotAm3[i] > 0)
                                {
                                    mWorld->DropThisNoAlign(craftSlotId3[i],craftSlotAm3[i],craftSlotSt3[i],Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y-0.25f,fppCam->m_vPosition.z),dropVelocity);

                                    craftSlotAm3[i] = -1;
                                    craftSlotSt3[i] = 0;
                                    craftSlotId3[i] = -1;
                                }
                            }
                        }
                        craftItemId3 = -1;
                        craftItemAm3 = -1;
                        craftItemSt3 = false;
                    }

                    if(upEn == true)
                    {
                        invXPosition = 0;
                        invYPosition = 0;
                    }
                    UpdateInvCursorPos();

                    invEn = false;
                    craft3xEn = false;
                    furnaceEn = false;
                    chestEn = false;
                    upEn = false;
                    furnaceEn = false;
                    chestId = -1;
                    armorEn = false;

                    UseChest = 0;
                    UseFurnace = 0;

                    mWorld->haveCompass = mWorld->HaveItemInBarSlots(Compass::getID());
                    return;
                }

                if(keyPressed(InputHelper::Instance()->getButtonToAction(14))) // if you press R
                {
                    if(invEn == true) // if 2x craft menu is open
                    {
                        if(craftItemId != -1) // if craft cell is not empty
                        {
                            if(mWorld->mId == craftItemId) // if item in craft cell = item in your mouse
                            {
                                if(craftItemSt == 1) // if item in craft cell is stackble
                                {
                                    if(mWorld->mAm+craftItemAm <= 64) //if sum of item in you mouse and items in craft cell is lower or equal than 64
                                    {
                                        for(int i = 0; i <= 3; i++) // check items in craft menu
                                        {
                                            craftSlotAm[i] -= 1;
                                            if(craftSlotAm[i] == 0) // delete if 0
                                            {
                                                craftSlotId[i] = -1;
                                                craftSlotAm[i] = -1;
                                                craftSlotSt[i] = 0;
                                            }
                                        }

                                        mWorld->mAm += craftItemAm; // item in mouse ++
                                        mWorld->mainStatistics.itemsCrafted += craftItemAm;

                                        CraftItem2x2(); // repeat function because in craft menu we can lost items
                                    }
                                }
                            }

                            if(mWorld->mId == -1) //if you haven't mouse item
                            {
                                for(int i = 0; i <= 3; i++)
                                {
                                    craftSlotAm[i] -= 1;
                                    if(craftSlotAm[i] == 0)
                                    {
                                        craftSlotId[i] = -1;
                                        craftSlotAm[i] = -1;
                                        craftSlotSt[i] = 0;
                                    }
                                }
                                mWorld->mId = craftItemId; // mouse item id = id of craft cell item
                                mWorld->mAm = craftItemAm;
                                mWorld->mSt = craftItemSt;
                                mWorld->mainStatistics.itemsCrafted += craftItemAm;

                                CraftItem2x2();

                            }
                        }
                    }


                    if(furnaceEn == true)
                    {
                        if(UseFurnace->furnaceSlotId[2] != -1)
                        {
                            if(mWorld->mId == UseFurnace->furnaceSlotId[2])
                            {
                                if(UseFurnace->furnaceSlotSt[2] == 1)
                                {
                                    if(mWorld->mAm+UseFurnace->furnaceSlotAm[2] <= 64)
                                    {
                                        mWorld->mAm += UseFurnace->furnaceSlotAm[2];

                                        UseFurnace->furnaceSlotAm[2] = -1;
                                        UseFurnace->furnaceSlotId[2] = -1;
                                        UseFurnace->furnaceSlotSt[2] = 0;
                                    }
                                }
                            }

                            if(mWorld->mId == -1) //if you haven't mouse item
                            {
                                mWorld->mId = UseFurnace->furnaceSlotId[2];
                                mWorld->mAm = UseFurnace->furnaceSlotAm[2];
                                mWorld->mSt = UseFurnace->furnaceSlotSt[2];

                                UseFurnace->furnaceSlotId[2] = -1;
                                UseFurnace->furnaceSlotAm[2] = -1;
                                UseFurnace->furnaceSlotSt[2] = 0;
                            }
                        }
                    }


                    if(craft3xEn == true)
                    {
                        if(craftItemId3 != -1)
                        {

                            if(mWorld->mId == craftItemId3)
                            {
                                if(craftItemSt3 == 1)
                                {
                                    if(mWorld->mAm+craftItemAm3 <= 64)
                                    {
                                        for(int i = 0; i <= 8; i++)
                                        {
                                            craftSlotAm3[i] -= 1;
                                            if(craftSlotAm3[i] == 0)
                                            {
                                                craftSlotId3[i] = -1;
                                                craftSlotAm3[i] = -1;
                                                craftSlotSt3[i] = 0;
                                            }
                                        }

                                        mWorld->mAm += craftItemAm3;
                                        mWorld->mainStatistics.itemsCrafted += craftItemAm3;

                                        CraftItem3x3();
                                    }
                                }
                            }

                            if(mWorld->mId == -1)
                            {
                                for(int i = 0; i <= 8; i++)
                                {
                                    craftSlotAm3[i] -= 1;
                                    if(craftSlotAm3[i] == 0)
                                    {
                                        craftSlotId3[i] = -1;
                                        craftSlotAm3[i] = -1;
                                        craftSlotSt3[i] = 0;
                                    }
                                }
                                mWorld->mId = craftItemId3;
                                mWorld->mAm = craftItemAm3;
                                mWorld->mSt = craftItemSt3;
                                mWorld->mainStatistics.itemsCrafted += craftItemAm3;

                                CraftItem3x3();
                            }
                        }
                    }
                }

                //switch right
                if(keyPressed(InputHelper::Instance()->getButtonToAction(9)))
                {
                    if (upEn == 0) // if your mouse in neither of craft menus
                    {
                        if(armorEn == true)
                        {
                            invXPosition != 1 ? invXPosition ++ : invXPosition = 1;
                            UpdateInvCursorPos();
                        }
                        else
                        {
                            invXPosition != 8 ? invXPosition ++ : invXPosition = 8;
                            UpdateInvCursorPos();
                        }
                    }
                    else
                    {
                        if(invEn == 1)
                        {
                            invXPosition != 1 ? invXPosition ++ : invXPosition = 1;
                            UpdateInvCursorPos();
                        }
                        if(craft3xEn == 1)
                        {
                            invXPosition != 2 ? invXPosition ++ : invXPosition = 2;
                            UpdateInvCursorPos();
                        }
                        if(chestEn == 1) // if it is in chest
                        {
                            invXPosition != 8 ? invXPosition ++ : invXPosition = 8;
                            UpdateInvCursorPos();
                        }
                    }
                }

                //switch left
                if(keyPressed(InputHelper::Instance()->getButtonToAction(8)))
                {
                    if (upEn == 0) // not only craft but chest too
                    {
                        invXPosition != 0 ? invXPosition -- : invXPosition = 0;

                        UpdateInvCursorPos();
                    }
                    else
                    {
                        invXPosition != 0 ? invXPosition -- : invXPosition = 0;
                        if(invEn == 1)
                        {
                            UpdateInvCursorPos();
                        }
                        if(craft3xEn == 1)
                        {
                            UpdateInvCursorPos();
                        }
                        if(chestEn == 1)
                        {
                            UpdateInvCursorPos();
                        }
                    }
                }

                //switch down
                if(keyPressed(InputHelper::Instance()->getButtonToAction(11)))
                {
                    if (upEn == 0)
                    {
                        if(armorEn == true)
                        {
                            if(invYPosition < 1)
                            {
                                invYPosition ++;
                            }
                            else
                            {
                                // Leave the armour grid downward and land on the inventory row
                                // directly below the column we were in (0 stays 0, 1 becomes 3).
                                if(invXPosition == 1)
                                {
                                    invXPosition = 3;
                                }
                                invYPosition = 0;
                                armorEn = false;
                            }
                            UpdateInvCursorPos();
                        }
                        else
                        {
                            invYPosition != 3 ? invYPosition ++ : invYPosition = 3;
                            UpdateInvCursorPos();
                        }
                    }
                    else
                    {
                        if (invEn == 1) //if mouse in the down of menu
                        {
                            if (invYPosition == 1)
                            {
                                upEn = 0; // we go to standart inv menu
                                invYPosition = 0;
                                invXPosition = 5 + invXPosition;
                                UpdateInvCursorPos();
                                return;
                            }
                            invYPosition != 1 ? invYPosition ++ : invYPosition = 1;
                            UpdateInvCursorPos();
                        }

                        if (craft3xEn == 1)
                        {
                            if (invYPosition == 2)
                            {
                                upEn = 0;
                                invYPosition = 0;
                                invXPosition = 3 + invXPosition;
                                UpdateInvCursorPos();
                                return;
                            }
                            invYPosition != 2 ? invYPosition ++ : invYPosition = 2;
                            UpdateInvCursorPos();
                        }

                        if (chestEn == 1)
                        {
                            if (invYPosition == 2)
                            {
                                upEn = 0;
                                invYPosition = 0;
                                UpdateInvCursorPos();
                                return;
                            }
                            invYPosition != 1 ? invYPosition ++ : invYPosition = 2;
                            UpdateInvCursorPos();
                        }

                        if (furnaceEn == 1)
                        {
                            if (invYPosition == 1)
                            {
                                upEn = 0;
                                invYPosition = 0;
                                invXPosition = 3;
                                UpdateInvCursorPos();
                                return;
                            }
                            invYPosition != 1 ? invYPosition ++ : invYPosition = 1;
                            UpdateInvCursorPos();
                        }
                    }
                }

                //switch up
                if(keyPressed(InputHelper::Instance()->getButtonToAction(10)))
                {
                    if (upEn == 0)
                    {
                        if(armorEn)
                        {
                            if(invYPosition > 0)
                            {
                                invYPosition --;
                            }
                            UpdateInvCursorPos();
                            return;
                        }
                        if (invYPosition == 0)
                        {
                            if(invEn == 1)
                            {
                                if((invXPosition == 5 || invXPosition == 6) && armorEn == false)
                                {
                                    invXPosition -= 5;
                                    invYPosition = 1;
                                    upEn = true;
                                    UpdateInvCursorPos();
                                }
                                if((invXPosition == 0 || invXPosition == 3) && armorEn == false && upEn == false)
                                {
                                    // Enter the armour grid on its bottom row, in the column the
                                    // inventory cursor was over (0 -> 0, 3 -> 1).
                                    invXPosition = (invXPosition == 3) ? 1 : 0;
                                    invYPosition = 1;
                                    armorEn = true;
                                    UpdateInvCursorPos();
                                }
                            }
                            if(craft3xEn == 1)
                            {
                                if(invXPosition >= 3 && invXPosition <= 5)
                                {
                                    invXPosition -= 3;
                                    invYPosition = 2;
                                    upEn = 1;
                                    UpdateInvCursorPos();
                                }
                            }
                            if(chestEn == 1)
                            {
                                invYPosition = 2;
                                upEn = 1;
                                UpdateInvCursorPos();
                            }
                            if(furnaceEn == 1)
                            {
                                if(invXPosition == 3)
                                {
                                    invYPosition = 1;
                                    upEn = 1;
                                    UpdateInvCursorPos();
                                }
                            }
                            return;
                        }
                        invYPosition != 0 ? invYPosition -- : invYPosition = 0;
                        UpdateInvCursorPos();

                        if(armorEn == true)
                        {
                            UpdateInvCursorPos();
                        }
                    }
                    else
                    {
                        invYPosition != 0 ? invYPosition -- : invYPosition = 0;

                        if(invEn == 1)
                        {
                            UpdateInvCursorPos();
                        }
                        if(craft3xEn == 1)
                        {
                            UpdateInvCursorPos();
                        }
                        if(chestEn == 1)
                        {
                            UpdateInvCursorPos();
                        }
                        if(furnaceEn == 1)
                        {
                            UpdateInvCursorPos();
                        }
                    }
                }


                if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
                {
                    if (upEn == 0)
                    {
                        if(armorEn == false)
                        {
                            if (mWorld->mId == -1)
                            {
                                if (mWorld->invId[invYPosition*9 + invXPosition] != -1)
                                {
                                    mWorld->mId = mWorld->invId[invYPosition*9 + invXPosition];
                                    mWorld->mAm = mWorld->invAm[invYPosition*9 + invXPosition];
                                    mWorld->mSt = mWorld->invSt[invYPosition*9 + invXPosition];
                                    mWorld->invId[invYPosition*9 + invXPosition] = -1;
                                    mWorld->invAm[invYPosition*9 + invXPosition] = -1;
                                    mWorld->invSt[invYPosition*9 + invXPosition] = 0;
                                }
                            }
                            else
                            {
                                int idx = invYPosition*9 + invXPosition;
                                if (mWorld->invId[idx] == -1)
                                {
                                    mWorld->invId[idx]=mWorld->mId;
                                    mWorld->invAm[idx]=mWorld->mAm;
                                    mWorld->invSt[idx]=mWorld->mSt;
                                    mWorld->mId = -1;
                                    mWorld->mAm = -1;
                                    mWorld->mSt = 0;
                                }
                                else if (mWorld->invId[idx] == mWorld->mId && mWorld->invSt[idx] == 1)
                                {
                                    if (mWorld->invAm[idx]+mWorld->mAm < 64)
                                    {
                                        mWorld->invAm[idx]+=mWorld->mAm;
                                        mWorld->mId = -1;
                                        mWorld->mAm = -1;
                                        mWorld->mSt = 0;
                                    }
                                    else
                                    {
                                        mWorld->mAm = (mWorld->invAm[idx] + mWorld->mAm) - 64;
                                        mWorld->invAm[idx]=64;

                                        if(mWorld->mAm == 0)
                                        {
                                            mWorld->mAm = -1;
                                            mWorld->mId = -1;
                                            mWorld->mSt = 0;
                                        }
                                    }
                                }
                                else
                                {
                                    short tId = mWorld->invId[idx];
                                    short tAm = mWorld->invAm[idx];
                                    bool  tSt = mWorld->invSt[idx];
                                    mWorld->invId[idx] = mWorld->mId;
                                    mWorld->invAm[idx] = mWorld->mAm;
                                    mWorld->invSt[idx] = mWorld->mSt;
                                    mWorld->mId = tId;
                                    mWorld->mAm = tAm;
                                    mWorld->mSt = tSt;
                                }
                            }
                        }
                        else
                        {
                            int armorIdx = invYPosition * 2 + invXPosition;

                            if (mWorld->mId == -1)
                            {
                                if (mWorld->armorId[armorIdx] != 324+armorIdx) // 324+armorIdx is an id of sample armor item
                                {
                                    mWorld->mId = mWorld->armorId[armorIdx];
                                    mWorld->mAm = mWorld->armorAm[armorIdx];
                                    mWorld->mSt = mWorld->armorSt[armorIdx];
                                    mWorld->armorId[armorIdx] = 324+armorIdx;
                                    mWorld->armorAm[armorIdx] = -1;
                                    mWorld->armorSt[armorIdx] = false;
                                }
                            }
                            else
                            {
                                if (mWorld->armorId[armorIdx] == 324+armorIdx)
                                {
                                    if(mWorld->mId == 324+armorIdx+4 || /* all leather armor */
                                       mWorld->mId == 324+armorIdx+8 || /* all chain armor */
                                       mWorld->mId == 324+armorIdx+12|| /* all iron armor */
                                       mWorld->mId == 324+armorIdx+16|| /* all diamond armor */
                                       mWorld->mId == 324+armorIdx+20|| /* all golde armor */
                                       (armorIdx == 0 && mWorld->mId == Pumpkin3::getID()) /** it is pumpkin and cursor on helmet slot **/
                                      ) // crazy code monkey skill
                                    {
                                        mWorld->armorId[armorIdx]=mWorld->mId;
                                        mWorld->armorAm[armorIdx]=mWorld->mAm;
                                        mWorld->armorSt[armorIdx]=mWorld->mSt;
                                        mWorld->mId = -1;
                                        mWorld->mAm = -1;
                                        mWorld->mSt = false;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if (invEn == 1)
                        {
                            if (mWorld->mId == -1)
                            {
                                if (craftSlotId[invYPosition*2 + invXPosition] != -1)
                                {
                                    mWorld->mId = craftSlotId[invYPosition*2 + invXPosition];
                                    mWorld->mAm = craftSlotAm[invYPosition*2 + invXPosition];
                                    mWorld->mSt = craftSlotSt[invYPosition*2 + invXPosition];
                                    craftSlotId[invYPosition*2 + invXPosition] = -1;
                                    craftSlotAm[invYPosition*2 + invXPosition] = -1;
                                    craftSlotSt[invYPosition*2 + invXPosition] = 0;
                                }
                            }
                            else
                            {
                                if (craftSlotId[invYPosition*2 + invXPosition] == -1)
                                {
                                    craftSlotId[invYPosition*2 + invXPosition]=mWorld->mId;
                                    craftSlotAm[invYPosition*2 + invXPosition]=mWorld->mAm;
                                    craftSlotSt[invYPosition*2 + invXPosition]=mWorld->mSt;
                                    mWorld->mId = -1;
                                    mWorld->mAm = -1;
                                    mWorld->mSt = 0;
                                }

                                if (craftSlotId[invYPosition*2 + invXPosition] == mWorld->mId)
                                {
                                    if(craftSlotSt[invYPosition*2 + invXPosition] == 1)
                                    {
                                    if (craftSlotAm[invYPosition*2 + invXPosition]+mWorld->mAm < 64)
                                    {
                                        craftSlotAm[invYPosition*2 + invXPosition]+=mWorld->mAm;
                                        mWorld->mId = -1;
                                        mWorld->mAm = -1;
                                        mWorld->mSt = 0;
                                    }
                                    else
                                    {
                                        mWorld->mAm = (craftSlotAm[invYPosition*2 + invXPosition] + mWorld->mAm) - 64;
                                        craftSlotAm[invYPosition*2 + invXPosition]=64;

                                        if(mWorld->mAm == 0)
                                        {
                                            mWorld->mAm = -1;
                                            mWorld->mId = -1;
                                            mWorld->mSt = 0;
                                        }
                                    }
                                    }
                                }
                            }
                            CraftItem2x2();
                        }

                        if (craft3xEn == 1)
                        {
                            if (mWorld->mId == -1)
                            {
                                if (craftSlotId3[invYPosition*3 + invXPosition] != -1)
                                {
                                    mWorld->mId = craftSlotId3[invYPosition*3 + invXPosition];
                                    mWorld->mAm = craftSlotAm3[invYPosition*3 + invXPosition];
                                    mWorld->mSt = craftSlotSt3[invYPosition*3 + invXPosition];
                                    craftSlotId3[invYPosition*3 + invXPosition] = -1;
                                    craftSlotAm3[invYPosition*3 + invXPosition] = -1;
                                    craftSlotSt3[invYPosition*3 + invXPosition] = 0;
                                }
                            }
                            else
                            {
                                if (craftSlotId3[invYPosition*3 + invXPosition] == -1)
                                {
                                    craftSlotId3[invYPosition*3 + invXPosition]=mWorld->mId;
                                    craftSlotAm3[invYPosition*3 + invXPosition]=mWorld->mAm;
                                    craftSlotSt3[invYPosition*3 + invXPosition]=mWorld->mSt;
                                    mWorld->mId = -1;
                                    mWorld->mAm = -1;
                                    mWorld->mSt = 0;
                                }

                                if (craftSlotId3[invYPosition*3 + invXPosition] == mWorld->mId)
                                {
                                    if(craftSlotSt3[invYPosition*3 + invXPosition] == 1)
                                    {
                                        if (craftSlotAm3[invYPosition*3 + invXPosition]+mWorld->mAm < 64)
                                        {
                                            craftSlotAm3[invYPosition*3 + invXPosition]+=mWorld->mAm;
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                            mWorld->mSt = 0;
                                        }
                                        else
                                        {
                                            mWorld->mAm = (craftSlotAm3[invYPosition*3 + invXPosition] + mWorld->mAm) - 64;
                                            craftSlotAm3[invYPosition*3 + invXPosition]=64;

                                            if(mWorld->mAm == 0)
                                            {
                                                mWorld->mAm = -1;
                                                mWorld->mId = -1;
                                                mWorld->mSt = 0;
                                            }
                                        }
                                    }
                                }
                            }
                            CraftItem3x3();
                        }
                        //Chest
                        if (chestEn == 1)
                        {
                            if (mWorld->mId == -1)
                            {
                                if (UseChest->chestSlotId[invYPosition*9 + invXPosition] != -1)
                                {
                                    mWorld->mId = UseChest->chestSlotId[invYPosition*9 + invXPosition];
                                    mWorld->mAm = UseChest->chestSlotAm[invYPosition*9 + invXPosition];
                                    mWorld->mSt = UseChest->chestSlotSt[invYPosition*9 + invXPosition];
                                    UseChest->chestSlotId[invYPosition*9 + invXPosition] = -1;
                                    UseChest->chestSlotAm[invYPosition*9 + invXPosition] = -1;
                                    UseChest->chestSlotSt[invYPosition*9 + invXPosition] = 0;
                                }
                            }
                            else
                            {
                                if (UseChest->chestSlotId[invYPosition*9 + invXPosition] == -1)
                                {
                                    UseChest->chestSlotId[invYPosition*9 + invXPosition]=mWorld->mId;
                                    UseChest->chestSlotAm[invYPosition*9 + invXPosition]=mWorld->mAm;
                                    UseChest->chestSlotSt[invYPosition*9 + invXPosition]=mWorld->mSt;
                                    mWorld->mId = -1;
                                    mWorld->mAm = -1;
                                    mWorld->mSt = 0;
                                }

                                if (UseChest->chestSlotId[invYPosition*9 + invXPosition] == mWorld->mId)
                                {
                                    if(UseChest->chestSlotSt[invYPosition*9 + invXPosition] == 1)
                                    {
                                        if (UseChest->chestSlotAm[+invYPosition*9 + invXPosition]+mWorld->mAm < 64)
                                        {
                                            UseChest->chestSlotAm[invYPosition*9 + invXPosition]+=mWorld->mAm;
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                            mWorld->mSt = 0;
                                        }
                                        else
                                        {
                                            mWorld->mAm = (UseChest->chestSlotAm[invYPosition*9 + invXPosition] + mWorld->mAm) - 64;
                                            UseChest->chestSlotAm[invYPosition*9 + invXPosition]=64;

                                            if(mWorld->mAm == 0)
                                            {
                                                mWorld->mAm = -1;
                                                mWorld->mId = -1;
                                                mWorld->mSt = 0;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        //furnace
                        if(furnaceEn == true)
                        {
                            if (mWorld->mId == -1)
                            {
                                if (UseFurnace->furnaceSlotId[invYPosition] != -1)
                                {
                                    mWorld->mId = UseFurnace->furnaceSlotId[invYPosition];
                                    mWorld->mAm = UseFurnace->furnaceSlotAm[invYPosition];
                                    mWorld->mSt = UseFurnace->furnaceSlotSt[invYPosition];
                                    UseFurnace->furnaceSlotId[invYPosition] = -1;
                                    UseFurnace->furnaceSlotAm[invYPosition] = -1;
                                    UseFurnace->furnaceSlotSt[invYPosition] = 0;
                                }
                            }
                            else
                            {
                                if (UseFurnace->furnaceSlotId[invYPosition] == -1)
                                {
                                    UseFurnace->furnaceSlotId[invYPosition]=mWorld->mId;
                                    UseFurnace->furnaceSlotAm[invYPosition]=mWorld->mAm;
                                    UseFurnace->furnaceSlotSt[invYPosition]=mWorld->mSt;
                                    mWorld->mId = -1;
                                    mWorld->mAm = -1;
                                    mWorld->mSt = 0;
                                }

                                if (UseFurnace->furnaceSlotId[invYPosition] == mWorld->mId)
                                {
                                    if(UseFurnace->furnaceSlotSt[invYPosition] == 1)
                                    {
                                        if (UseFurnace->furnaceSlotAm[invYPosition]+mWorld->mAm < 64)
                                        {
                                            UseFurnace->furnaceSlotAm[invYPosition]+=mWorld->mAm;
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                            mWorld->mSt = 0;
                                        }
                                        else
                                        {
                                            mWorld->mAm = (UseFurnace->furnaceSlotAm[invYPosition] + mWorld->mAm) - 64;
                                            UseFurnace->furnaceSlotAm[invYPosition]=64;

                                            if(mWorld->mAm == 0)
                                            {
                                                mWorld->mAm = -1;
                                                mWorld->mId = -1;
                                                mWorld->mSt = 0;
                                            }
                                        }
                                    }
                                }
                            }
                            CheckForFurnFuel(UseFurnace);
                            CheckForFurnWorking(UseFurnace);
                        }
                    }
                }

                if(mSystemMgr->KeyPressed(PSP_CTRL_SQUARE))
                {
                    if (upEn == 0)
                    {
                        if (mWorld->invId[invYPosition*9 + invXPosition] == mWorld->mId)
                        {
                            if(mWorld->mSt == true)
                            {
                                if (mWorld->invAm[invYPosition*9 + invXPosition] != 64)
                                {
                                    mWorld->invAm[invYPosition*9 + invXPosition] += 1;

                                    mWorld->mAm -= 1;

                                    if(mWorld->mAm == 0)
                                    {
                                        mWorld->mId = -1;
                                        mWorld->mAm = -1;
                                    }
                                    return;
                                }
                            }
                        }

                        if (mWorld->mId != -1)
                        {
                            if(mWorld->mSt == true)
                            {
                               if (mWorld->invId[invYPosition*9 + invXPosition] == -1)
                               {
                                    mWorld->invId[invYPosition*9 + invXPosition] = mWorld->mId;
                                    mWorld->invAm[invYPosition*9 + invXPosition] = 1;
                                    mWorld->invSt[invYPosition*9 + invXPosition] = 1;

                                    mWorld->mAm -= 1;

                                    if(mWorld->mAm == 0)
                                    {
                                        mWorld->mId = -1;
                                        mWorld->mAm = -1;
                                        mWorld->mSt = 0;
                                    }
                                    return;
                                }
                            }
                        }
                        else
                        {
                           if (mWorld->invId[invYPosition*9 + invXPosition] != -1 && mWorld->invSt[invYPosition*9 + invXPosition] == 1)
                           {
                               if(mWorld->invAm[invYPosition*9 + invXPosition] == 1)
                               {
                                   mWorld->mId = mWorld->invId[invYPosition*9 + invXPosition];
                                   mWorld->mAm = 1;
                                   mWorld->mSt = mWorld->invSt[invYPosition*9 + invXPosition];

                                   mWorld->invId[invYPosition*9 + invXPosition] = -1;
                                   mWorld->invAm[invYPosition*9 + invXPosition] = -1;
                                   mWorld->invSt[invYPosition*9 + invXPosition] = 0;
                               }
                               if(mWorld->invAm[invYPosition*9 + invXPosition] > 1)
                               {
                                   mWorld->mId = mWorld->invId[invYPosition*9 + invXPosition];
                                   mWorld->mAm = ceilf((float)(mWorld->invAm[invYPosition*9 + invXPosition]/2.0f));
                                   mWorld->mSt = mWorld->invSt[invYPosition*9 + invXPosition];

                                   mWorld->invAm[invYPosition*9 + invXPosition] = mWorld->invAm[invYPosition*9 + invXPosition]-mWorld->mAm;
                               }
                            }
                        }
                    }
                    else
                    {
                        ///2x CRAFT
                        if (invEn == 1)
                        {
                            if (craftSlotId[invYPosition*2 + invXPosition] == mWorld->mId)
                            {
                                if(mWorld->mSt == true)
                                {
                                    if (craftSlotAm[invYPosition*2 + invXPosition] != 64)
                                    {
                                        craftSlotAm[invYPosition*2 + invXPosition] += 1;

                                        mWorld->mAm -= 1;

                                        if(mWorld->mAm == 0)
                                        {
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                        }
                                        CraftItem2x2();
                                        return;
                                    }
                                }
                            }

                            if (mWorld->mId != -1)
                            {
                                if(mWorld->mSt == true)
                                {
                                   if (craftSlotId[invYPosition*2 + invXPosition] == -1)
                                   {
                                       craftSlotId[invYPosition*2 + invXPosition] = mWorld->mId;
                                       craftSlotAm[invYPosition*2 + invXPosition] = 1;
                                       craftSlotSt[invYPosition*2 + invXPosition] = 1;

                                       mWorld->mAm -= 1;

                                       if(mWorld->mAm == 0)
                                       {
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                            mWorld->mSt = 0;
                                        }
                                        CraftItem2x2();
                                        return;
                                    }
                                }
                            }
                            else
                            {
                               if (craftSlotId[invYPosition*2 + invXPosition] != -1 && craftSlotSt[invYPosition*2 + invXPosition] == 1)
                               {
                                   if(craftSlotAm[invYPosition*2 + invXPosition] == 1)
                                   {
                                       mWorld->mId = craftSlotId[invYPosition*2 + invXPosition];
                                       mWorld->mAm = 1;
                                       mWorld->mSt = craftSlotSt[invYPosition*2 + invXPosition];

                                       craftSlotId[invYPosition*2 + invXPosition] = -1;
                                       craftSlotAm[invYPosition*2 + invXPosition] = -1;
                                       craftSlotSt[invYPosition*2 + invXPosition] = 0;
                                   }
                                   if(craftSlotAm[invYPosition*2 + invXPosition] > 1)
                                   {
                                       mWorld->mId = craftSlotId[invYPosition*2 + invXPosition];
                                       mWorld->mAm = ceilf((float)(craftSlotAm[invYPosition*2 + invXPosition]/2.0f));
                                       mWorld->mSt = craftSlotSt[invYPosition*2 + invXPosition];

                                       craftSlotAm[invYPosition*2 + invXPosition] = craftSlotAm[invYPosition*2 + invXPosition]-mWorld->mAm;
                                   }
                                }
                            }
                            CraftItem2x2();
                        }

                        ///CRAFT TABLE
                        if (craft3xEn == 1)
                        {
                            if (craftSlotId3[invYPosition*3 + invXPosition] == mWorld->mId)
                            {
                                if(mWorld->mSt == true)
                                {
                                    if (craftSlotAm3[invYPosition*3 + invXPosition] != 64)
                                    {
                                        craftSlotAm3[invYPosition*3 + invXPosition] += 1;

                                        mWorld->mAm -= 1;

                                        if(mWorld->mAm == 0)
                                        {
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                        }
                                        CraftItem3x3();
                                        return;
                                    }
                                }
                            }

                            if (mWorld->mId != -1)
                            {
                                if(mWorld->mSt == true)
                                {
                                   if (craftSlotId3[invYPosition*3 + invXPosition] == -1)
                                   {
                                       craftSlotId3[invYPosition*3 + invXPosition] = mWorld->mId;
                                       craftSlotAm3[invYPosition*3 + invXPosition] = 1;
                                       craftSlotSt3[invYPosition*3 + invXPosition] = 1;

                                       mWorld->mAm -= 1;

                                       if(mWorld->mAm == 0)
                                       {
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                            mWorld->mSt = 0;
                                       }
                                       CraftItem3x3();
                                       return;
                                   }
                                }
                           }
                            else
                            {
                               if (craftSlotId3[invYPosition*3 + invXPosition] != -1 && craftSlotSt3[invYPosition*3 + invXPosition] == 1)
                               {
                                   if(craftSlotAm3[invYPosition*3 + invXPosition] == 1)
                                   {
                                       mWorld->mId = craftSlotId3[invYPosition*3 + invXPosition];
                                       mWorld->mAm = 1;
                                       mWorld->mSt = craftSlotSt3[invYPosition*3 + invXPosition];

                                       craftSlotId3[invYPosition*3 + invXPosition] = -1;
                                       craftSlotAm3[invYPosition*3 + invXPosition] = -1;
                                       craftSlotSt3[invYPosition*3 + invXPosition] = 0;
                                   }
                                   if(craftSlotAm3[invYPosition*3 + invXPosition] > 1)
                                   {
                                       mWorld->mId = craftSlotId3[invYPosition*3 + invXPosition];
                                       mWorld->mAm = ceilf((float)(craftSlotAm3[invYPosition*3 + invXPosition]/2.0f));
                                       mWorld->mSt = craftSlotSt3[invYPosition*3 + invXPosition];

                                       craftSlotAm3[invYPosition*3 + invXPosition] = craftSlotAm3[invYPosition*3 + invXPosition]-mWorld->mAm;
                                   }
                                }
                            }
                           CraftItem3x3();
                        }

                        ///CHEST
                        if (chestEn == 1)
                        {
                            if (UseChest->chestSlotId[invYPosition*9 + invXPosition] == mWorld->mId)
                            {
                                if(mWorld->mSt == true)
                                {
                                    if (UseChest->chestSlotAm[invYPosition*9 + invXPosition] != 64)
                                    {
                                        UseChest->chestSlotAm[invYPosition*9 + invXPosition] += 1;

                                        mWorld->mAm -= 1;
                                        if(mWorld->mAm == 0)
                                        {
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                        }
                                        return;
                                    }
                                }
                            }

                            if (mWorld->mId != -1)
                            {
                                if(mWorld->mSt == true)
                                {
                                    if (UseChest->chestSlotId[invYPosition*9 + invXPosition] == -1)
                                    {
                                        UseChest->chestSlotId[invYPosition*9 + invXPosition] = mWorld->mId;
                                        UseChest->chestSlotAm[invYPosition*9 + invXPosition] = 1;
                                        UseChest->chestSlotSt[invYPosition*9 + invXPosition] = 1;

                                        mWorld->mAm -= 1;

                                        if(mWorld->mAm == 0)
                                        {
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                            mWorld->mSt = 0;
                                        }
                                        return;
                                    }
                                }
                            }
                            else
                            {
                               if (UseChest->chestSlotId[invYPosition*9 + invXPosition] != -1 && UseChest->chestSlotSt[invYPosition*9 + invXPosition] == 1)
                               {
                                   if(UseChest->chestSlotAm[invYPosition*9 + invXPosition] == 1)
                                   {
                                       mWorld->mId = UseChest->chestSlotId[invYPosition*9 + invXPosition];
                                       mWorld->mAm = 1;
                                       mWorld->mSt = UseChest->chestSlotSt[invYPosition*9 + invXPosition];

                                       UseChest->chestSlotId[invYPosition*9 + invXPosition] = -1;
                                       UseChest->chestSlotAm[invYPosition*9 + invXPosition] = -1;
                                       UseChest->chestSlotSt[invYPosition*9 + invXPosition] = 0;
                                   }
                                   if(UseChest->chestSlotAm[invYPosition*9 + invXPosition] > 1)
                                   {
                                       mWorld->mId = UseChest->chestSlotId[invYPosition*9 + invXPosition];
                                       mWorld->mAm = ceilf((float)(UseChest->chestSlotAm[invYPosition*9 + invXPosition]/2.0f));
                                       mWorld->mSt = UseChest->chestSlotSt[invYPosition*9 + invXPosition];

                                       UseChest->chestSlotAm[invYPosition*9 + invXPosition] = UseChest->chestSlotAm[invYPosition*9 + invXPosition]-mWorld->mAm;
                                   }
                                }
                            }
                        }
                        ///FURNACE
                        if (furnaceEn == 1)
                        {
                            if (UseFurnace->furnaceSlotId[invYPosition] == mWorld->mId)
                            {
                                if(mWorld->mSt == true)
                                {
                                    if (UseFurnace->furnaceSlotAm[invYPosition] != 64)
                                    {
                                        UseFurnace->furnaceSlotAm[invYPosition] += 1;

                                        mWorld->mAm -= 1;
                                        if(mWorld->mAm == 0)
                                        {
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                        }
                                        CheckForFurnFuel(UseFurnace);
                                        CheckForFurnWorking(UseFurnace);
                                        return;
                                    }
                                }
                            }

                            if (mWorld->mId != -1)
                            {
                                if(mWorld->mSt == true)
                                {
                                    if (UseFurnace->furnaceSlotId[invYPosition] == -1)
                                    {
                                        UseFurnace->furnaceSlotId[invYPosition] = mWorld->mId;
                                        UseFurnace->furnaceSlotAm[invYPosition] = 1;
                                        UseFurnace->furnaceSlotSt[invYPosition] = 1;

                                        mWorld->mAm -= 1;

                                        if(mWorld->mAm == 0)
                                        {
                                            mWorld->mId = -1;
                                            mWorld->mAm = -1;
                                            mWorld->mSt = 0;
                                        }
                                        CheckForFurnFuel(UseFurnace);
                                        CheckForFurnWorking(UseFurnace);
                                        return;
                                    }
                                }
                            }
                            else
                            {
                               if (UseFurnace->furnaceSlotId[invYPosition] != -1 && UseFurnace->furnaceSlotSt[invYPosition] == 1)
                               {
                                   if(UseFurnace->furnaceSlotAm[invYPosition] == 1)
                                   {
                                       mWorld->mId = UseFurnace->furnaceSlotId[invYPosition];
                                       mWorld->mAm = 1;
                                       mWorld->mSt = UseFurnace->furnaceSlotSt[invYPosition];

                                       UseFurnace->furnaceSlotId[invYPosition] = -1;
                                       UseFurnace->furnaceSlotAm[invYPosition] = -1;
                                       UseFurnace->furnaceSlotSt[invYPosition] = 0;
                                   }
                                   if(UseFurnace->furnaceSlotAm[invYPosition] > 1)
                                   {
                                       mWorld->mId = UseFurnace->furnaceSlotId[invYPosition];
                                       mWorld->mAm = ceilf((float)(UseFurnace->furnaceSlotAm[invYPosition]/2.0f));
                                       mWorld->mSt = UseFurnace->furnaceSlotSt[invYPosition];

                                       UseFurnace->furnaceSlotAm[invYPosition] = UseFurnace->furnaceSlotAm[invYPosition]-mWorld->mAm;
                                   }
                                }
                            }
                            CheckForFurnFuel(UseFurnace);
                            CheckForFurnWorking(UseFurnace);
                        }
                    }
                }
            }
        }
        else if(menuState == 1) //menu state
        {
            static float analogMenuWait = 0.0f;
            analogMenuWait -= dt;
            if(analogMenuWait < 0.0f)
                analogMenuWait = 0.0f;
            if(analogMenuWait <= 0.0f)
            {
                float ay = mSystemMgr->GetAnalogY();
                float ax = mSystemMgr->GetAnalogX();
                if(ay < -0.6f)
                {
                    analogMenuWait = 0.18f;
                    if(menuOptions)
                    {
                        optionsMenuPos = PauseOptionsGridMove(optionsMenuPos, 0, -1);
                        mSoundMgr->PlayMenuSound();
                    }
                    else
                    {
                        selectPos--;
                        if(selectPos < 0)
                            selectPos = 5;
                        mSoundMgr->PlayMenuSound();
                    }
                }
                else if(ay > 0.6f)
                {
                    analogMenuWait = 0.18f;
                    if(menuOptions)
                    {
                        optionsMenuPos = PauseOptionsGridMove(optionsMenuPos, 0, 1);
                        mSoundMgr->PlayMenuSound();
                    }
                    else
                    {
                        selectPos++;
                        if(selectPos > 5)
                            selectPos = 0;
                        mSoundMgr->PlayMenuSound();
                    }
                }
                else if(menuOptions && PauseOptionsIsSlider(optionsMenuPos) && ax < -0.6f)
                {
                    analogMenuWait = 0.18f;
                    if(AdjustPauseOptionsSlider(mWorld, fppCam, skyLight, skyMoonLight, optionsMenuPos, -1))
                        mSoundMgr->PlayMenuSound();
                }
                else if(menuOptions && PauseOptionsIsSlider(optionsMenuPos) && ax > 0.6f)
                {
                    analogMenuWait = 0.18f;
                    if(AdjustPauseOptionsSlider(mWorld, fppCam, skyLight, skyMoonLight, optionsMenuPos, 1))
                        mSoundMgr->PlayMenuSound();
                }
            }

            //turn off menu with the same key
            if(keyPressed(InputHelper::Instance()->getButtonToAction(15)))
            {
                menuState = 0;
                menuOptions = false;
                optionsMenuPos = 0;
                selectPos = 0;
            }

            //menu keys
        if(menuOptions)
        {
            //up, down, left, right move between the two columns
            if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
            {
                optionsMenuPos = PauseOptionsGridMove(optionsMenuPos, 0, -1);
                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
            {
                optionsMenuPos = PauseOptionsGridMove(optionsMenuPos, 0, 1);
                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_LEFT))
            {
                optionsMenuPos = PauseOptionsGridMove(optionsMenuPos, -1, 0);
                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_RIGHT))
            {
                optionsMenuPos = PauseOptionsGridMove(optionsMenuPos, 1, 0);
                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_LTRIGGER))
            {
                if(PauseOptionsIsSlider(optionsMenuPos))
                {
                    if(AdjustPauseOptionsSlider(mWorld, fppCam, skyLight, skyMoonLight, optionsMenuPos, -1))
                        mSoundMgr->PlayMenuSound();
                }
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_RTRIGGER))
            {
                if(PauseOptionsIsSlider(optionsMenuPos))
                {
                    if(AdjustPauseOptionsSlider(mWorld, fppCam, skyLight, skyMoonLight, optionsMenuPos, 1))
                        mSoundMgr->PlayMenuSound();
                }
            }

            //back
            if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                if (mWorld->mainOptions.difficult == 0)
                {
                    mWorld->DestroyAllZombies();
					mSoundMgr->PlayMenuSound3();
                }
                selectPos = 1;
                menuOptions = false;
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                //fog rendering
                if(optionsMenuPos == 2)
                {
                     mWorld->mainOptions.fogRendering = !mWorld->mainOptions.fogRendering;
                     mSoundMgr->PlayMenuSound2();
                }

                //clouds rendering
                if(optionsMenuPos == 3)
                {
                     mWorld->mainOptions.detailedSky = !mWorld->mainOptions.detailedSky;
                     mSoundMgr->PlayMenuSound2();
                }

                //fast rendering
                if(optionsMenuPos == 4)
                {
                       mWorld->mainOptions.fastRendering = !mWorld->mainOptions.fastRendering;
                       mSoundMgr->PlayMenuSound2();

                       mWorld->RebuildVisibleChunks();
                }

                //smooth lighting
                if(optionsMenuPos == 5)
                {
                    mWorld->mainOptions.smoothLighting = !mWorld->mainOptions.smoothLighting;
                    mSoundMgr->PlayMenuSound2();

                    mWorld->RebuildVisibleChunks();
                }

                //block animation
                if(optionsMenuPos == 6)
                {
                    mWorld->mainOptions.worldBlockAnimation = !mWorld->mainOptions.worldBlockAnimation;
                    mSoundMgr->PlayMenuSound2();
                }

                //auto jump
                if(optionsMenuPos == 7)
                {
                    mWorld->mainOptions.autoJump = !mWorld->mainOptions.autoJump;
                    bobCycle = 0.0f;
                    mSoundMgr->PlayMenuSound2();
                }

                if(optionsMenuPos == 10)
                {
                     mWorld->mainOptions.freezeDayTime = !mWorld->mainOptions.freezeDayTime;
                     mSoundMgr->PlayMenuSound2();
                }

                if(optionsMenuPos == 11)
                {
                     mWorld->mainOptions.guiDrawing = !mWorld->mainOptions.guiDrawing;
                     mSoundMgr->PlayMenuSound2();
                }

                if(optionsMenuPos == 12)
                {
                     mWorld->mainOptions.sounds = !mWorld->mainOptions.sounds;
                     mSoundMgr->PlayMenuSound2();
                     mSoundMgr->playerSounds = mWorld->mainOptions.sounds;
                }

                //music
                if(optionsMenuPos == 13)
                {
                    mWorld->mainOptions.music = !mWorld->mainOptions.music;
                    mSoundMgr->PlayMenuSound2();
                    mSoundMgr->StopAmbient();
                }


                //take screen
                if(optionsMenuPos == 14)
                {
                    makeScreen = true;
                }

                if(optionsMenuPos == 15)
                {
                    devMode = !devMode;
                }
            }
        }
        else
        {
            //up, down
            if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
            {
                selectPos--;
                if(selectPos < 0)
                    selectPos = 5;

                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
            {
                selectPos++;
                if(selectPos > 5)
                    selectPos = 0;

                mSoundMgr->PlayMenuSound();
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                menuState = 0;
                selectPos = 0;
                optionsMenuPos = 0;
                menuOptions = false;
            }

            if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
            {
                if(selectPos == 0)//resume
                {
                    menuState = 0;
					mSoundMgr->PlayMenuSound2();
                }
                if(selectPos == 1)//options
                {
                    optionsMenuPos = 0;
                    menuOptions = true;
					mSoundMgr->PlayMenuSound2();
                }
                if(selectPos == 2)//options
                {
                    menuState = 3;
                    selectPos = 0;
					mSoundMgr->PlayMenuSound2();
                }
                if(selectPos == 3)//save
                {
                    //mWorld->sunTime = sunTime;
                    mWorld->SaveCompressedWorld(saveFileName.c_str());
                    menuState = 0;
					mSoundMgr->PlayMenuSound2();
                }
                if(selectPos == 4)//save end exit
                {
                    mSoundMgr->StopAmbient();
                    mSoundMgr->StopDiskTrack();
                    mWorld->SaveCompressedWorld(saveFileName.c_str());
                    sManager->PopState();
					mSoundMgr->PlayMenuSound2();
                }
                if(selectPos == 5)//exit
                {
                    mSoundMgr->StopAmbient();
                    mSoundMgr->StopDiskTrack();
                    sManager->PopState();
					mSoundMgr->PlayMenuSound2();
                }
            }
        }
    }
    if (menuState == 3)
    {
        if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
        {
            selectPos = 2;
            menuState = 1;
			mSoundMgr->PlayMenuSound3();
        }
        if(mSystemMgr->KeyPressed(PSP_CTRL_RTRIGGER))
        {
            statisticsPage += 1;
            if(statisticsPage == 2)
            {
                statisticsPage = 0;
				mSoundMgr->PlayMenuSound2();
            }
        }
        if(mSystemMgr->KeyPressed(PSP_CTRL_LTRIGGER))
        {
            statisticsPage -= 1;
            if(statisticsPage == -1)
            {
                statisticsPage = 1;
				mSoundMgr->PlayMenuSound2();
            }
        }
    }
}
    if(mWorld->HP <= 0)
    {
        mWorld->playerGrowth = 0.65f;
        if(dieFactor == 1) // drop your loot
        {
            for(int i = 0; i < 36; i++)
            {
                if(i <= 3) // drop armor items
                {
                    if(mWorld->armorId[i] != 324 + i)
                    {
                        Vector3 dropVelocity = Vector3(0,0,0);

                        dropVelocity.x = (-15+rand()%30)*0.1f;
                        dropVelocity.y = 3.0f;
                        dropVelocity.z = (-15+rand()%30)*0.1f;

                        mWorld->DropThisNoAlign(mWorld->armorId[i],mWorld->armorAm[i],mWorld->armorSt[i],Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y-0.25f,fppCam->m_vPosition.z),dropVelocity);

                        mWorld->armorAm[i] = -1;
                        mWorld->armorSt[i] = 0;
                        mWorld->armorId[i] = -1;
                    }
                }
                if(mWorld->invId[i] > 0)
                {
                    Vector3 dropVelocity = Vector3(0,0,0);

                    dropVelocity.x = (-15+rand()%30)*0.1f;
                    dropVelocity.y = 3.0f;
                    dropVelocity.z = (-15+rand()%30)*0.1f;

                    if(mWorld->invAm[i] > 0)
                    {
                        mWorld->DropThisNoAlign(mWorld->invId[i],mWorld->invAm[i],mWorld->invSt[i],Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y-0.25f,fppCam->m_vPosition.z),dropVelocity);

                        mWorld->invAm[i] = -1;
                        mWorld->invSt[i] = 0;
                        mWorld->invId[i] = -1;
                    }
                }
            }

            mWorld->mainStatistics.dies += 1;
            dieFactor = 0;
        }
        if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
        {
            if(mWorld->gameModeWorld == 2)
            {
                mWorld->gameLocked = true;
                mWorld->SaveCompressedWorld(saveFileName.c_str());

                std::string fileName = saveFileName;
                remove(fileName.c_str());
                fileName = fileName + "c";
                remove(fileName.c_str());

                sManager->PopState();
            }
            else
            {
                mWorld->playerGrowth = 1.65f;

                Vector3 newPosition = mWorld->playerSpawnPointPosition;
                if(mWorld->BlockSolid(newPosition.x,newPosition.y-1.2f,newPosition.z) || mWorld->BlockSolid(newPosition.x,newPosition.y,newPosition.z))
                {
                    newPosition.y = mWorld->groundHeight(newPosition.x,newPosition.z)+mWorld->playerGrowth-0.05f+1;
                }

                playerPosition.x = newPosition.x;
                playerPosition.y = newPosition.y;
                playerPosition.z = newPosition.z;

                mWorld->UpdatePlayerZoneBB(playerPosition);	//Move player back to original spawn point
                fppCam->PositionCamera(playerPosition.x,playerPosition.y,playerPosition.z, playerPosition.x,playerPosition.y,playerPosition.z-5.0f, 0.0f,1.0f,0.0f);
                fppCam->RotateView(0,0,90,0);
                fppCam->upDownAngle = 0;
                fppCam->horAngle = 270;
                fppCam->RebuildView();
                mWorld->HP = 20;
                mWorld->HG = 20;
                mWorld->OS = 10;

                bodyYaw = 270.0f;      // the respawn teleport moved the camera; snap the model with it
                headYawOffset = 0.0f;

                invEn = false;
                craft3xEn = false;
                furnaceEn = false;
                chestEn = false;
                upEn = false;
                furnaceEn = false;
                chestId = -1;
                armorEn = false;

                UseChest = NULL;
                UseFurnace = NULL;

                int chunkTarget = mWorld->getChunkId(playerPosition-Vector3(0, -1.75, 0));

                //Rebuild the world around
                mWorld->RebuildFullMeshChunk(chunkTarget);
                mWorld->rebuildNearestChunks(chunkTarget,playerPosition);
                //mWorld->rebuildVisibleChunks();
               // mWorld->rebuildVisibleTransperentChunks();

                dieFactor = 1;
            }
        }
    }
}


#if MC_AUTO_WALK
// Development-only auto-walk. Defined further down, beside StatePlay::keyHold -- the one place the
// game turns the analog into a direction -- but ticked from Update, which is above it in this file.
static void AutoWalkTick(float dt);
// Stuck check, likewise: it needs the player's feet, which Update has and keyHold does not.
static void AutoWalkProgress(float dt, float x, float y, float z, float vx, float vz, float hp);
#endif

#if MC_AUTO_FLY
// Development-only flight path -- see MC_AUTO_FLY in DebugConfig.h.
namespace autofly
{
    const float kSettleSecs = 8.0f;    // let the spawn area finish meshing first
    const float kFlySecs    = 10.0f;   // 112 blocks, seven chunks, per leg at sprint-fly speed
    // Then stop and hover. A hole that is still there after this is the kind the player reports: the
    // one that does not fill when they stand still. The hole audit (CraftWorld::AuditHoles) fires
    // 2.5 s into the hover and names it.
    const float kHoverSecs  = 8.0f;
    const float kLegSecs    = kFlySecs + kHoverSecs;
    const float kHeight     = 72.0f;   // a few blocks over typical ground (~60), so the hole is on screen
    const float kFlyPitch   = -0.55f;  // look at the ground ahead, not the sky
    const float kHoverPitch = -0.90f;  // look down at the hole when they stop
    float t = 0.0f;
    float logTimer = 0.0f;
}

static void AutoFlyPlace(float dt, Vector3& pos, Camera* cam)
{
    autofly::t += dt;
    if(autofly::t < autofly::kSettleSecs)
        return;
    const float legT = autofly::t - autofly::kSettleSecs;
    const int leg = (int)(legT / autofly::kLegSecs);
    const bool hovering = (legT - leg * autofly::kLegSecs) >= autofly::kFlySecs;
    // horAngle 0 looks along +x, 270 along -z (Camera::RebuildView).
    const bool east = (leg & 1) == 0;
    const float step = hovering ? 0.0f : MC_AUTO_FLY_SPEED * dt;
    if(east)
        pos.x += step;
    else
        pos.z -= step;
    pos.y = autofly::kHeight;
    cam->horAngle = east ? 0.0f : 270.0f;
    cam->upDownAngle = hovering ? autofly::kHoverPitch : autofly::kFlyPitch;

    autofly::logTimer += dt;
    if(autofly::logTimer >= 1.0f)
    {
        autofly::logTimer -= 1.0f;
        Aurora::Utils::Logger::Instance()->LogMessage("fly t=%.1f pos=%.1f,%.1f,%.1f leg=%d %s\n",
            autofly::t, pos.x, pos.y, pos.z, leg, hovering ? "hover" : "fly");
    }
}
#endif

void StatePlay::Update(StateManager* sManager)
{
    canFly = false;
    mSoundMgr->AudioSync();
    mChat.Update(dt);

#if MC_NETHER_PROBE
    // TEMPORARY diagnostic: log what the pad actually delivers, so a control problem can be told
    // apart from a binding one. Logs only while something is being pressed.
    {
        static float inProbeT = 0.0f;
        inProbeT += dt;
        if(inProbeT > 0.4f)
        {
            inProbeT = 0.0f;
            float ax = mSystemMgr->GetAnalogX();
            float ay = mSystemMgr->GetAnalogY();
            bool up = mSystemMgr->KeyHold(PSP_CTRL_UP);
            bool dn = mSystemMgr->KeyHold(PSP_CTRL_DOWN);
            bool lf = mSystemMgr->KeyHold(PSP_CTRL_LEFT);
            bool rt = mSystemMgr->KeyHold(PSP_CTRL_RIGHT);
            if(ax > 0.05f || ax < -0.05f || ay > 0.05f || ay < -0.05f || up || dn || lf || rt)
                Aurora::Utils::Logger::Instance()->LogMessage("[inputprobe] ax=%.3f ay=%.3f dpad=%d%d%d%d\n", ax, ay, up, dn, lf, rt);
        }
    }
#endif

    // The debug stick's menu. It takes the whole update while it is up: the world must not advance
    // a single tick behind an open debug menu, or the mob being tested walks away while you are
    // deciding what to do to it. This is a hold of its own rather than a menuState value, because
    // the pause menu and the inventory panels are one big else-if chain and sliding in between them
    // would mean touching every one of their conditions.
    if(debugMenu.IsOpen() == true)
        debugMenu.Update(dt, mSystemMgr, mSoundMgr);

    ApplyDebugMenuResult();

    if(debugMenu.IsOpen() == true)
        return;

#if MC_PERF_PROBE
    // Probe build only: keep the benchmark clock PerfProbeSelfTest left in the world flipping, so the
    // per-second perf lines below describe a world with a running clock rather than an idle one.
    PerfProbeDriveClock(dt);
#endif

#if MC_AUTO_WALK
    // Development-only auto-walk clock and stuck check. Once a frame, here rather than in keyHold --
    // see AutoWalkTick. This is also the only place holding both the clock and the player's feet, which
    // is what the stuck check needs.
    AutoWalkTick(dt);
    AutoWalkProgress(dt, playerPosition.x, playerPosition.y, playerPosition.z,
                     fppCam ? fppCam->m_vVelocity.x : 0.0f, fppCam ? fppCam->m_vVelocity.z : 0.0f,
                     mWorld ? mWorld->HP : 0.0f);
    // The input tables, because nothing else in a probe build ever sets them.
    //
    // InputHelper::Init() -- the only thing that fills connections[] with the real bindings -- has
    // exactly one caller, StateMenu. MC_PERF_PROBE and MC_NETHER_PROBE both boot straight into a world
    // and never pass through the menu, so connections[] keeps its default-constructed button = 0 for
    // every action, getButtonToAction() returns 0, and the movement code asks keyHold(0) -- the d-pad --
    // instead of the analog entries 11-14 this probe injects into. The probe was therefore never being
    // asked for a direction at all, and the player never moved no matter what it returned.
    //
    // Measured on the overworld probe before this: `asked=0/0/0/0` and `vel=0.000,0.000` for 95
    // seconds, with the position frozen to the digit at spawn. A stationary player produces no meshing
    // after the world-entry fill, which is why every earlier "walking" run looked like a short
    // streaming window followed by idle -- that window was the fill, not the walk.
    //
    // Init() only writes the default tables, so calling it here is the same state StateMenu would have
    // left behind before Load() applied the player's own bindings. Load() is deliberately not called:
    // the probe wants the defaults, and it avoids depending on a Config/input.cf that may not exist.
    static bool sInputTablesReady = false;
    if(!sInputTablesReady)
    {
        sInputTablesReady = true;
        InputHelper::Instance()->Init();
    }
    // The game's own step-up assist, so walking into a one-block ledge is not read as a blocked path.
    // A player option, set rather than saved, because this is a probe build.
    if(mWorld)
        mWorld->mainOptions.autoJump = true;
    // Keep the probe alive. The movement block in HandleEvents is guarded by `mWorld->HP > 0`, so a
    // death ends the walk silently: the clock keeps ticking and the log keeps printing, but nothing is
    // asked for any more and the player stands still for the rest of the session. Measured on the run
    // before this one: the walk travelled 224.7 blocks in 72 s and then froze for the remaining 48 with
    // asked=0/0/0/0 -- a death, not a blocked path. The death is logged once, because it is a fact
    // about the run, and then undone so the remaining seconds still measure walking.
    if(mWorld && mWorld->HP <= 0.0f)
    {
        static bool sLoggedWalkDeath = false;
        if(!sLoggedWalkDeath)
        {
            sLoggedWalkDeath = true;
            Aurora::Utils::Logger::Instance()->LogMessage(
                "walk: player died -- reviving so the walk can cover the full session\n");
        }
        mWorld->HP = 20.0f;
    }
#endif

    // Chat-button double-tap bookkeeping. A press that never became a double tap opens chat here,
    // one window after it was pressed; the menu checks are repeated because the player has had that
    // window to open something, and a chat open behind an inventory would strand the keyboard.
    if(chatTapTimer > 0.0f)
    {
        chatTapTimer -= dt;
        if(chatTapTimer <= 0.0f)
        {
            chatTapTimer = 0.0f;
            if(chatTapPending)
            {
                chatTapPending = false;
                if(menuState == 0 && invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && sleepTime == 0.0f)
                {
                    mChat.TryOpen(mSystemMgr, mWorld);
                }
            }
        }
    }

    // Bow draw and release. HandleEvents started the draw on the press; here it is accumulated while
    // the button is still held and the shot goes off the frame it is not. The panels and the sleep
    // fade both take the button for other things, so a draw is abandoned rather than fired if one of
    // them came up mid-draw.
    if(bowCharging == true)
    {
        bool canFire = (mWorld != NULL && fppCam != NULL && menuState == 0 && sleepTime == 0.0f &&
                        invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false);
        if(canFire == false)
        {
            bowCharging = false;
            bowCharge = 0.0f;
        }
        else if(keyHold(InputHelper::Instance()->getButtonToAction(14)) == true)
        {
            bowCharge += dt;
            if(bowCharge > 1.0f)
                bowCharge = 1.0f;
        }
        else
        {
            bowCharging = false;
            FireBowArrow();
            bowCharge = 0.0f;
        }
    }

    // Head and body. The head is wherever the camera points -- Java's rotationYawHead is rotationYaw
    // for the local player -- and the body is dragged after it: 0.3 of the remaining angle per 20 Hz
    // game tick, which is what EntityLivingBase.updateDistance() applies. Written as a decay rather
    // than a fixed step so it does not depend on the frame rate, and then the head is clamped to
    // BODY_MAX_HEAD_TURN away from the body, so a big flick takes the body round with it instead of
    // leaving the head cranked right round on a stationary torso.
    if(mWorld != NULL && fppCam != NULL)
    {
        float headYaw = fppCam->horAngle;
        float diff = WrapAngle180(headYaw - bodyYaw);
        bodyYaw += diff * (1.0f - powf(0.7f, dt * 20.0f));

        headYawOffset = WrapAngle180(headYaw - bodyYaw);
        if(headYawOffset > BODY_MAX_HEAD_TURN)
            headYawOffset = BODY_MAX_HEAD_TURN;
        if(headYawOffset < -BODY_MAX_HEAD_TURN)
            headYawOffset = -BODY_MAX_HEAD_TURN;

        bodyYaw = WrapAngle360(headYaw - headYawOffset);
    }

    if(menuState != 0)
        return;

    if(pendingBreakStage == 1 && pendingCrackId < 0 && mWorld != NULL)
    {
        int x = (int)pendingCrackX;
        int y = (int)pendingCrackY;
        int z = (int)pendingCrackZ;
        mWorld->WakeBuriedAround(x, y, z);
        if(pendingBreakChunk != -1)
        {
            if(pendingBreakWasLight)
                mWorld->RemoveLigtSourceAtPosition(x, y, z, Torch::getID());
            mWorld->RebuildFullMeshChunk(pendingBreakChunk);
            mWorld->rebuildNearestChunks(pendingBreakChunk, Vector3((float)x, (float)y, (float)z));
        }
        pendingBreakStage = 2;
    }
    else if(pendingBreakStage == 2 && mWorld != NULL)
    {
        pendingBreakStage = 0;
        int x = (int)pendingCrackX;
        int y = (int)pendingCrackY;
        int z = (int)pendingCrackZ;
        mWorld->CheckForTorchSupport(x, y, z, mWorld->GetBlock(x, y, z));
        mWorld->CheckForLadderSupport(x, y, z);
        mWorld->DestroyAroundTrapdoors(x, y, z);
        mWorld->DestroyAroundItemFrames(x, y, z);
    }

    if(mWorld->armorAm[0] <= 0)
    {
        mWorld->armorId[0] = 324;
        mWorld->armorAm[0] = -1;
        mWorld->armorSt[0] = false;
    }
    if(mWorld->armorAm[1] <= 0)
    {
        mWorld->armorId[1] = 325;
        mWorld->armorAm[1] = -1;
        mWorld->armorSt[1] = false;
    }
    if(mWorld->armorAm[2] <= 0)
    {
        mWorld->armorId[2] = 326;
        mWorld->armorAm[2] = -1;
        mWorld->armorSt[2] = false;
    }
    if(mWorld->armorAm[3] <= 0)
    {
        mWorld->armorId[3] = 327;
        mWorld->armorAm[3] = -1;
        mWorld->armorSt[3] = false;
    }
    mWorld->AP = mWorld->FetchArmorPoints(mWorld->armorId[3],mWorld->armorId[2],mWorld->armorId[1],mWorld->armorId[0]);

    if(WorldRunning())//game state
    {
        cubeLight = mWorld->BlockFinalLight(playerPosition.x,playerPosition.y-1,playerPosition.z) * 1.2f;
        if(cubeLight > 1.0f)
        {
            cubeLight = 1.0f;
        }

        furnaceTimes += dt;
        if(furnaceTimes >= 0.5f)
        {
            if(mWorld->mFurnaces.size() > 0)
            {
                vector<Furnace*>::iterator it;
                for(it = mWorld->mFurnaces.begin(); it != mWorld->mFurnaces.end(); ++it)
                {
                    if((*it)->fuelTime > 0)
                    {
                        (*it)->fuelTime -= 0.5f;
                        if(mWorld->GetBlock((*it)->furnaceX, (*it)->furnaceY, (*it)->furnaceZ) == FurnaceOff::getID())
                        {
                            Vector3 testPos5 = Vector3(-1,-1,-1);
                            int chunkTarget2 = -1;

                            testPos5.x = (*it)->furnaceX;
                            testPos5.y = (*it)->furnaceY;
                            testPos5.z = (*it)->furnaceZ;
                            chunkTarget2 = mWorld->getChunkId(testPos5);

                            mWorld->GetBlock((*it)->furnaceX, (*it)->furnaceY, (*it)->furnaceZ) = FurnaceOn::getID();
                            mWorld->UpdateLightAreaIn(testPos5);

                            mWorld->RebuildFullMeshChunk(chunkTarget2);
                            mWorld->rebuildNearestChunksForLight(chunkTarget2,testPos5);

                            fppCam->needUpdate = true;
                        }
                    }

                    if((*it)->fuelTime <= 0)
                    {
                        (*it)->fuelTime = 0.0f;
                        CheckForFurnFuel((*it));
                    }

                    if((*it)->fuelTime <= 0)
                    {
                        if(mWorld->GetBlock((*it)->furnaceX,(*it)->furnaceY,(*it)->furnaceZ) == 107)
                        {
                            Vector3 testPos5 = Vector3(-1,-1,-1);
                            int chunkTarget2 = -1;

                            testPos5.x = (*it)->furnaceX;
                            testPos5.y = (*it)->furnaceY;
                            testPos5.z = (*it)->furnaceZ;
                            chunkTarget2 = mWorld->getChunkId(testPos5);

                            mWorld->GetBlock((*it)->furnaceX,(*it)->furnaceY,(*it)->furnaceZ) = 106;

                            mWorld->RemoveLigtSourceAtPosition((*it)->furnaceX,(*it)->furnaceY,(*it)->furnaceZ,107);

                            mWorld->UpdateLightAreaIn(testPos5);

                            mWorld->RebuildFullMeshChunk(chunkTarget2);
                            mWorld->rebuildNearestChunksForLight(chunkTarget2,testPos5);

                            fppCam->needUpdate = true;
                        }
                    }

                    if((*it)->fuelTime > 0 && (*it)->working == 1)
                    {
                        (*it)->meltingTime += 0.5f;
                        if((*it)->meltingTime >= 10)
                        {
                            (*it)->meltingTime = 0;
                            (*it)->working = 0;

                            ReadyFurnSmelting((*it));
                            CheckForFurnWorking((*it));
                        }
                    }
                }
            }
            furnaceTimes -= 0.5f;
        }

        if(mWorld->mMonsterSpawners.size() > 0)
        {
            BoundingBox playerBox = BoundingBox(Vector3(playerPosition.x - 0.15f,playerPosition.y - 1.60f,playerPosition.z - 0.15f),Vector3(playerPosition.x + 0.15f,playerPosition.y + 0.2f,playerPosition.z + 0.15f));

            vector<MonsterSpawnerEntity*>::iterator it;
            for(it = mWorld->mMonsterSpawners.begin(); it != mWorld->mMonsterSpawners.end(); ++it)
            {
                if(playerBox.intersect((*it)->actArea))
                {
                    (*it)->Update(dt);

                    if((*it)->particleDelay1 <= 0)
                    {
                         (*it)->particleDelay1 = 0.5f+(rand() % 500)/1000.0f;

                         for(int i = 0; i < 4; i++)
                         {
                            float particle_x = (*it)->GetX()+(rand()%(11)/10.0f);
                            float particle_y = (*it)->GetY()+(rand()%(11)/10.0f);
                            float particle_z = (*it)->GetZ()+(rand()%(11)/10.0f);

                            Particle::Spawn(mWorld, "black smoke",Vector3(particle_x,particle_y,particle_z));
                            Particle::Spawn(mWorld, "flame",Vector3(particle_x,particle_y,particle_z));
                         }
                    }
                    if((*it)->particleDelay2 <= 0)
                    {
                         (*it)->particleDelay2 = 0.5f+(rand() % 500)/1000.0f;

                         for(int i = 0; i < 6; i++)
                         {
                            float particle_x = (*it)->GetX()+(rand()%(11)/10.0f);
                            float particle_y = (*it)->GetY()+(rand()%(11)/10.0f);
                            float particle_z = (*it)->GetZ()+(rand()%(11)/10.0f);

                            Particle::Spawn(mWorld, "black smoke",Vector3(particle_x,particle_y,particle_z));
                            Particle::Spawn(mWorld, "flame",Vector3(particle_x,particle_y,particle_z));
                         }
                    }

                    if((*it)->ItIsTimeToSpawn() && mWorld->BlockFinalLight((*it)->GetX(),(*it)->GetY()+1,(*it)->GetZ()) < 0.6f && mWorld->mainOptions.difficult != 0)
                    {
                        (*it)->forceSpawn = false;
                        int nearbyEntityAmount = 0;

                        if((*it)->MobToSpawn() == "zombie")
                        {
                            if(mWorld->mZombies.empty() == false)
                            {
                                for(int i = 0; i < mWorld->mZombies.size(); i++)
                                {
                                    if(nearbyEntityAmount == 4)
                                    {
                                        break;
                                    }
                                    Zombie* TestMob = mWorld->mZombies[i];
                                    if(TestMob->bBox.intersect((*it)->actArea))
                                    {
                                        nearbyEntityAmount ++;
                                    }
                                }
                            }

                            if(nearbyEntityAmount < 4)
                            {
                                int count0 = (*it)->MobCountToSpawn();
                                for(int i = 0; i < count0; i++)
                                {
                                    float spawn_x = (*it)->GetX()-2+(rand()%2)*4;
                                    float spawn_z = (*it)->GetZ()-2+(rand()%2)*4;
                                    spawn_x += 0.5f;
                                    spawn_z += 0.5f;

                                    float spawn_y = mWorld->groundHeight(spawn_x+0.5f,(*it)->GetY()+1,spawn_z+0.5f)+1.2f;

                                    if(mWorld->CanSpawnHostile())
                                        mWorld->SpawnZombie((*it)->GetX()-1+rand()%3,(*it)->GetY()+1,(*it)->GetZ()-1+rand()%3);
                                }
                            }
                        }
                    }
                }
            }
        }

        if(sleepTime > 0.0f)
        {
            sleepTime -= dt;
        }
        if(sleepTime < 0.1f && sleepTime > 0.0f)
        {
            Vector3 previousPosition = mWorld->playerSpawnPointPosition;

            playerPosition.x = previousPosition.x;
            playerPosition.y = previousPosition.y;
            playerPosition.z = previousPosition.z;

            mWorld->UpdatePlayerZoneBB(playerPosition);
            fppCam->PositionCamera(playerPosition.x,playerPosition.y,playerPosition.z, playerPosition.x,playerPosition.y,playerPosition.z-5.0f, 0.0f,1.0f,0.0f);
            fppCam->RotateView(0,0,90,0);
            fppCam->upDownAngle = 0;
            fppCam->horAngle = 270;
                fppCam->RebuildView();
            bodyYaw = 270.0f;      // snap the model with it: this is a teleport, not a turn
            headYawOffset = 0.0f;

            sleepTime = 0.0f;
            sleepMessageType = 0;
            mWorld->SetWolrdTime(6);
            mWorld->brightFactor = 0.0f;
            mWorld->mainStatistics.daysInGame++;
            mWorld->RebuildVisibleChunks();
        }

        if (mWorld->skyTime >= 100 && mWorld->skyTime <= 150) // morning
        {
            mWorld->brightFactor = (mWorld->skyTime - 100.0f)/50.0f;
        }
        if (mWorld->skyTime >= 146 && mWorld->skyTime <= 249) // noon
        {
            mWorld->brightFactor = 1.0f;
        }
        if (mWorld->skyTime >= 250 && mWorld->skyTime <= 300) // sunset
        {
            mWorld->brightFactor = (300.0f - mWorld->skyTime)/50.0f;
        }
        if (mWorld->skyTime >= 300 && mWorld->skyTime <= 460) // night
        {
            mWorld->brightFactor = 0.0f;
        }

        if (mWorld->skyTime >= 101 && mWorld->skyTime <= 116)
        {
            mWorld->dawnSunsetFactor = (mWorld->skyTime - 101)/18.0f;
        }
        if (mWorld->skyTime >= 125 && mWorld->skyTime <= 139)
        {
            mWorld->dawnSunsetFactor = (139 - mWorld->skyTime)/18.0f;
        }
        if (mWorld->skyTime > 134 && mWorld->skyTime < 240)
        {
            mWorld->dawnSunsetFactor = 0.0f;
        }

        if (mWorld->skyTime >= 265 && mWorld->skyTime <= 288)
        {
            mWorld->dawnSunsetFactor = (mWorld->skyTime - 265)/18.0f;
        }
        if (mWorld->skyTime >= 291 && mWorld->skyTime <= 306)
        {
            mWorld->dawnSunsetFactor = (306 - mWorld->skyTime)/18.0f;
        }
        if (mWorld->skyTime > 306)
        {
            mWorld->dawnSunsetFactor = 0.0f;
        }

        if(mWorld->dawnSunsetFactor > 1.0f)
        {
            mWorld->dawnSunsetFactor = 1.0f;
        }

        if (anim[0] == 1)
        {
            if (changeY < 0.2f)
            {
                changeY += dt*0.9f;
            }
            if (changeY >= 0.0f)
            {
                anim[0] = 0;
                changeY = 0.0f;
            }
        }

        animSpeed = dt*2.86*PI;
        if(animGo == true)
        {
            if(animDest < PI)
            {
                animDest += animSpeed;
            }
            if(animDest >= PI)
            {
                animGo = false;
                animDest = 0;
            }
        }

        if(playerVelocity.y != 0.0f)
        {
            fppCam->needUpdate = true;
        }

        if(fppCam->needUpdate)
        {
            showCube = false;

            //show pick cube
            Vector3 rayDir = fppCam->m_vView - fppCam->m_vPosition;
            rayDir.normalize();

            //we take position and view vector
            Vector3 testPos ;

            // collider
            block_t aimBlock = mWorld->GetBlock(testPos.x,testPos.y,testPos.z);
            BoundingBox collider = mWorld->GetBlockSelectionBox(testPos.x,testPos.y,testPos.z,aimBlock);
            BoundingBox testBox = BoundingBox(testPos.x+collider.min.x, testPos.y+collider.min.y, testPos.z+collider.min.z, testPos.x+collider.max.x, testPos.y+collider.max.y, testPos.z+collider.max.z);

            //moving slowly to the target
            // The aim itself. This loop is the only place the pick is decided -- cubePos feeds the
            // outline drawn here *and* the placement site at the top of this file -- so it is exact in
            // both halves: the loop visits every cell the look crosses, one step per cell wall
            // (CraftWorld::NextCellDistance), and the test is a clip against that cell's selection box
            // rather than "is this point inside it". The shape that needs both is a wire's 1 px plate:
            // thinner than a fixed step, so a march can step over the cell it lies in, and thin enough
            // that the look can cross it between two samples. The box is the *selection* shape and not
            // the collision shape, so the outline is drawn on the surface the player can see.
            Ray aimRay(fppCam->m_vPosition, rayDir);

            for(float i = 0.0f; i < 5.25f; i = CraftWorld::NextCellDistance(fppCam->m_vPosition, rayDir, i))
            {
                testPos = fppCam->m_vPosition + (rayDir * i);

                aimBlock = mWorld->GetBlock(testPos.x,testPos.y,testPos.z);
                collider = mWorld->GetBlockSelectionBox(testPos.x,testPos.y,testPos.z,aimBlock);
                testBox = BoundingBox(floorf(testPos.x)+0.5f+collider.min.x, floorf(testPos.y)+0.5f+collider.min.y, floorf(testPos.z)+0.5f+collider.min.z, floorf(testPos.x)+0.5f+collider.max.x, floorf(testPos.y)+0.5f+collider.max.y, floorf(testPos.z)+0.5f+collider.max.z);

                //check if we touching something
                float aimDist = -1.0f;
                aimRay.hasIntersected(testBox, aimDist);

                if(mWorld->BlockEditable(testPos.x,testPos.y,testPos.z) && aimDist != -1.0f)
                {
                    //if yes then show pick cube
                    cubePos.x = (int)testPos.x + 0.5f;
                    cubePos.y = (int)testPos.y + 0.5f;
                    cubePos.z = (int)testPos.z + 0.5f;

                    showCube = true;
                    break;
                }
            }
        }

        int soundBlockType = -1;

        //RenderManager::InstancePtr()

        //update player position
        if(dt <= 0.1f)
        {
            /// player timers
            musicTime -= dt;
            if (musicTime < 0.0f)
            {
                musicTime = 300.0f;
                if(mWorld->mainOptions.music == true)
                {
                    musicTime = mSoundMgr->PlayRandomAmbient();
                }
            }

            cloudsOffset += dt/1500.0f;
            if(cloudsOffset >= 2.0f)
            {
                cloudsOffset -= 2.0f;
            }

            { // nearest chunk update
                tickChunk += dt;
                if (tickChunk >= 10.0f)
                {
                    chunkId = mWorld->getChunkId(playerPosition);
                    if(chunkId >= 0 && chunkId < (int)mWorld->mChunks.size() && mWorld->mChunks[chunkId])
                    {
                        unsigned int beforeHash = mWorld->HashSectionBlocks(mWorld->mChunks[chunkId]->chunkStartX, mWorld->mChunks[chunkId]->chunkStartY, mWorld->mChunks[chunkId]->chunkStartZ);
                        mWorld->UpdateChunkBlocks(chunkId);
                        unsigned int afterHash = mWorld->HashSectionBlocks(mWorld->mChunks[chunkId]->chunkStartX, mWorld->mChunks[chunkId]->chunkStartY, mWorld->mChunks[chunkId]->chunkStartZ);
                        if(beforeHash != afterHash)
                            mWorld->mChunks[chunkId]->needUpdate = true;
                    }
                    tickChunk -= 10.0f;
                }
				//Block Physics
				if (tickChunk >= 0.5f)
                {
                    if(CHUNK_SIZE > 0)
                    {
                        int sx = ChunkFloorDiv((int)playerPosition.x, CHUNK_SIZE) * CHUNK_SIZE;
                        int sz = ChunkFloorDiv((int)playerPosition.z, CHUNK_SIZE) * CHUNK_SIZE;
                        int sy = ((int)playerPosition.y / CHUNK_SIZE) * CHUNK_SIZE;
                        mWorld->UpdateBlockPhysicsBox(sx, sy, sz);
                    }
                    tickChunk -= 0.5f;
                }
            }

            {
                // The passive hunger tick. This is NOT Java's model -- Java has no timed hunger drain
                // at all; its only passive cost is healing (6 exhaustion per hit point, i.e. 1.5
                // hunger, at food 18 or more) and this port does not charge for healing. The tick
                // stands in for that and for the fact that a session is long, which is why it is left
                // alone here: 25% chance of a point every 75 s while the bar is above 16, else a
                // certain one. Movement, jumping, hits and blows no longer touch HG directly -- they
                // go through CraftWorld::AddExhaustion, where the per-metre rates live.
                tickHunger += dt;
                if(tickHunger >= 75)
                {
                    HungerTime();
                    tickHunger -= 75;
                }

                tickHealth += dt;
                if(tickHealth >= 5.0f)
                {
                    HealthTime();
                    tickHealth -= 5.0f;
                }

                tickLavaDamage += dt;
                if(tickLavaDamage >= 0.5f)
                {
                    if(headInLava || footInLava)
                    {
                        HurtPlayer(4.0f);
                        if(mWorld->armorId[0] != 324)
                        {
                            mWorld->armorAm[0] -= 8+rand()%8;
                        }
                        if(mWorld->armorId[1] != 325)
                        {
                            mWorld->armorAm[1] -= 8+rand()%8;
                        }
                        if(mWorld->armorId[2] != 326)
                        {
                            mWorld->armorAm[2] -= 8+rand()%8;
                        }
                        if(mWorld->armorId[3] != 327)
                        {
                            mWorld->armorAm[3] -= 8+rand()%8;
                        }
                    }
                    tickLavaDamage -= 0.5f;
                }

                if(touchingCactus)
                {
                    tickCactusDamage += dt;
                }
                else
                {
                    tickCactusDamage = 0.4f;
                }
                if(tickCactusDamage >= 0.5f)
                {
                    HurtPlayer(1);

                    tickCactusDamage -= 0.5f;
                }

                if (hurt_time > 0)
                {
                    hurt_time -= dt;
                }
                if (hurt_time <= 0)
                {
                    hurt = false;
                }

                if(tickOS >= 1.0f && headInWater == 1)
                {
                    OxygenTime();
                    tickOS = 0;
                }

                if (headInWater == 0)
                {
                    if (mWorld->OS != 10)
                    {
                        mWorld->OS = 10;
                    }
                    tickOS = 0;
                }
                else
                {
                    tickOS += dt;
                }
            }

            /// player physics
            playerPosition = fppCam->m_vPosition;

            //gravity
            Vector3 footPosition = playerPosition + Vector3(0.0f, -mWorld->playerGrowth, 0.0f);
            Vector3 legsPosition = playerPosition + Vector3(0.0f, -mWorld->playerGrowth+0.65f, 0.0f);
            Vector3 headPosition = playerPosition + Vector3(0.0f, 0.2f, 0.0f);

            if(sleepTime <= 0.0f)
            {
                //foot in water
                if(mWorld->PlayerInWater(legsPosition))
                {
                    if(footInWater == false)
                    {
                        mSoundMgr->PlaySplashSound();
                        footInWater = true;
                    }

                }
                else
                {
                    footInWater = false;
                }

                //check if head is is in the water
                if(mWorld->PlayerInWater(headPosition))
                {
                    //change gravity
                    if(!canFly)
                    {
                        if (playerVelocity.y > -3)
                        {
                            playerVelocity.y += (GRAVITY/3.0f) * dt;
                        }
                        else
                        {
                            playerVelocity.y = -3;
                        }
                    }
                    /*if(headInWater == false)
                    {
                        mSoundMgr->PlaySplashSound();
                        headInWater = true;
                    }*/
                    headInWater = true;
                }
                else
                {
                    headInWater = false;
                }

                if(mWorld->PlayerInLava(legsPosition))
                {
                    footInLava = true;
                }
                else
                {
                    footInLava = false;
                }

                //check if head is is in the Lava
                if(mWorld->PlayerInLava(headPosition))
                {
                    //change gravity
                    if(!canFly)
                    {
                        playerVelocity.y += (GRAVITY/3.0f) * dt;
                    }
                    headInLava = true;
                }
                else
                {
                    headInLava = false;
                }

                if(!canFly && !headInLava && !headInWater)
                {
                    playerVelocity.y += GRAVITY * 3.4f * dt;
                }

                if(!canFly && mWorld->PlayerCollisionWithLadder(playerPosition) && moving == false)
                {
                    playerVelocity.y = -CLIMBVELOCITY;
                }

                touchingCactus = false;
                if(mWorld->PlayerCollisionWithCactus(playerPosition) || mWorld->PlayerCollisionWithCactus(Vector3(playerPosition.x,playerPosition.y-1.5f,playerPosition.z)))
                {
                    touchingCactus = true;
                }

                if(!canFly)
                {
                //check if we will stand on ground
                Vector3 futureFootPosition = Vector3(footPosition.x,footPosition.y+playerVelocity.y*dt,footPosition.z);
                if(mWorld->SolidAtPointForPlayer(futureFootPosition))
                {
                    walkingOnGround = true;
                    soundBlockType = mWorld->BlockSoundAtPos(futureFootPosition);

                    // If the player is stuck in the ground, bring them out.
                    // This happens because we're standing on a block at -1.5, but stuck in it at -1.4, so -1.45 is the sweet spot.
                    if (mWorld->SolidAtPointForPlayer(futureFootPosition))
                    {
                        int blockOn = (int)(futureFootPosition.y);

                        if(mWorld->GetBlock(futureFootPosition.x,futureFootPosition.y,futureFootPosition.z) == CactusBlock::getID())
                        {
                            touchingCactus = true;
                        }

                        if(mWorld->PlayerOnHalfBlock(futureFootPosition))
                        {
                            playerPosition.y = (float)(blockOn + 1 + mWorld->playerGrowth-0.55f);
                        }
                        else if(mWorld->PlayerOnStairsBlock(futureFootPosition))
                        {
                            if(mWorld->PlayerOnHighStair(futureFootPosition))
                            {
                                playerPosition.y = (float)(blockOn + 1 + mWorld->playerGrowth-0.05f);
                            }
                            else
                            {
                                playerPosition.y = (float)(blockOn + 1 + mWorld->playerGrowth-0.55f);
                            }
                        }
                        else
                        {
                            playerPosition.y = (float)(blockOn + 1 + mWorld->playerGrowth-0.05f);
                        }
                    }

                    if(playerVelocity.y != 0.0f) /// FALL DAMAGE
                    {
                        if (!mWorld->PlayerInWater(Vector3(futureFootPosition.x,futureFootPosition.y+0.45f,futureFootPosition.z)) &&
                            !(mWorld->GetBlock(playerPosition.x,playerPosition.y-1.3f,playerPosition.z) >= Ladder1::getID() && mWorld->GetBlock(playerPosition.x,playerPosition.y-1.3f,playerPosition.z) <= Ladder4::getID()))
                        {
                            if(playerVelocity.y < -6 && rand() % 10 >= 7) // destroy soil
                            {
                                if(mWorld->GetBlock(futureFootPosition.x, futureFootPosition.y-0.25f, futureFootPosition.z) == Soil::getID() || mWorld->GetBlock(futureFootPosition.x, futureFootPosition.y-0.25f, futureFootPosition.z) == WateredSoil::getID())
                                {
                                    mWorld->GetBlock(futureFootPosition.x, futureFootPosition.y-0.25f, futureFootPosition.z) = DirtBlock::getID();
                                    if(mWorld->GetBlock(futureFootPosition.x, futureFootPosition.y+0.25f, futureFootPosition.z) != 0)
                                    {
                                        mWorld->DropThis(mWorld->BlockLoot(futureFootPosition.x, futureFootPosition.y+0.25f, futureFootPosition.z),1,true,Vector3(futureFootPosition.x, futureFootPosition.y+0.25f, futureFootPosition.z));
                                        mWorld->GetBlock(futureFootPosition.x, futureFootPosition.y+0.25f, futureFootPosition.z) = 0;
                                    }
                                    if(!mWorld->ChunksEqual(Vector3(futureFootPosition.x, futureFootPosition.y-0.25f, futureFootPosition.z), Vector3(futureFootPosition.x, futureFootPosition.y+0.25f, futureFootPosition.z)))
                                    {
                                        mWorld->RebuildFullMeshChunk(mWorld->getChunkId(Vector3(futureFootPosition.x, futureFootPosition.y-0.25f, futureFootPosition.z)));
                                        mWorld->RebuildFullMeshChunk(mWorld->getChunkId(Vector3(futureFootPosition.x, futureFootPosition.y+0.25f, futureFootPosition.z)));
                                    }
                                    else
                                    {
                                        mWorld->RebuildFullMeshChunk(mWorld->getChunkId(Vector3(futureFootPosition.x, futureFootPosition.y-0.25f, futureFootPosition.z)));
                                    }
                                }
                            }
                            if(playerVelocity.y < -12.8 && playerVelocity.y > -19)
                            {
                                mSoundMgr->PlayFallSound(playerVelocity.y);
                                if(mWorld->GetBlock(futureFootPosition.x, futureFootPosition.y-0.25f, futureFootPosition.z) == HayBale::getID())
                                {
                                    HurtPlayer((playerVelocity.y*-1 - 11) / 1.4 * 0.3f);
                                }
                                else
                                {
                                    HurtPlayer((playerVelocity.y*-1 - 11) / 1.4);
                                }

                                if(mWorld->HG > 0.2)
                                {
                                    mWorld->HG -= 0.2;
                                }
                                mWorld->mainStatistics.badlyFalls += 1;

                                if(mWorld->armorId[3] != 327) // if we have boots
                                {
                                    mWorld->armorAm[3] -= 2;
                                }
                            }
                            if(playerVelocity.y < -19)
                            {
                                mSoundMgr->PlayFallSound(playerVelocity.y);
                                if(mWorld->GetBlock(futureFootPosition.x, futureFootPosition.y-0.25f, futureFootPosition.z) == HayBale::getID())
                                {
                                    HurtPlayer((playerVelocity.y*-1 - 11) * 1.3f * 0.3f);
                                }
                                else
                                {
                                    HurtPlayer((playerVelocity.y*-1 - 11) * 1.3f);
                                }

                                if(mWorld->HG > 0.3)
                                {
                                    mWorld->HG -= 0.3;
                                }
                                mWorld->mainStatistics.badlyFalls += 1;

                                if(mWorld->armorId[3] != 327) // if we have boots
                                {
                                    mWorld->armorAm[3] -= 6;
                                }
                            }
                        }
                    }
                    playerVelocity.y = 0.0f;

                    //dodatkowa logika podczas stania na klocku
                    //jump
                    if(jumping)
                    {
                        if(!mWorld->SolidAtPointForPlayerHead(headPosition))
                        {
                            playerVelocity.y = 1.575f * JUMPVELOCITY;
                            walkingOnGround = false;

                            mWorld->mainStatistics.jumps += 1;
                            // Java's EntityPlayer.jump(): a sprint-jump costs four times a standing
                            // one, which is what stops bunny-hopping from being free travel.
                            mWorld->AddExhaustion(sprinting ? EXHAUST_SPRINT_JUMP : EXHAUST_JUMP);
                        }
                        jumping = false;
                    }
                }
                else
                {
                    walkingOnGround = false;
                }

                // If the player has their head stuck in a block, push them down.
                Vector3 futureHeadPosition = Vector3(headPosition.x,headPosition.y+playerVelocity.y*dt,headPosition.z);
                if(mWorld->SolidAtPointForPlayerHead(futureHeadPosition))
                {
                    int blockIn = (int)(futureHeadPosition.y);
                    playerPosition.y = (float)(blockIn-0.2f); //0.15
                    playerVelocity.y = 0.0f;
                }
                }
                else
                {
                    walkingOnGround = mWorld->SolidAtPointForPlayer(footPosition);
                    playerVelocity.y = 0.0f;
                }
            }

            if(mWorld->kickedStart)
            {
                if(mWorld->kickedBy == 1) // if you damaged by zombie
                {
                    walkingOnGround = false;
                    playerVelocity.y = 0.9f * JUMPVELOCITY;
                    mWorld->kickedTimer = 0.5f;
                    fppCam->needUpdate = true;

                    switch(mWorld->mainOptions.difficult)
                    {
                    case 0: // peaceful
                        HurtPlayerConsiderArmor(0);
                    break;
                    case 1: // easy
                        HurtPlayerConsiderArmor(mWorld->kickedDamage-1);
                    break;
                    case 2: // normal
                        HurtPlayerConsiderArmor(mWorld->kickedDamage);
                    break;
                    case 3: // hard
                        HurtPlayerConsiderArmor(mWorld->kickedDamage+1);
                    break;
                    }
                    if(mWorld->armorId[0] != 324)
                    {
                        mWorld->armorAm[0] -= 1+rand()%3;
                    }
                    if(mWorld->armorId[1] != 325)
                    {
                        mWorld->armorAm[1] -= 1+rand()%4;
                    }
                    if(mWorld->armorId[2] != 326)
                    {
                        mWorld->armorAm[2] -= 1+rand()%3;
                    }
                    if(mWorld->armorId[3] != 327)
                    {
                        mWorld->armorAm[3] -= rand()%3;
                    }
                }
                if(mWorld->kickedBy == 2) // creeper or TNT
                {
                    walkingOnGround = false;
                    // A blast supplies its own distance-scaled launch; a creeper kick does not and keeps
                    // the original flat hop.
                    playerVelocity.y = (mWorld->kickedVelY > 0.0f) ? mWorld->kickedVelY : 1.2f * JUMPVELOCITY;
                    mWorld->kickedTimer = 0.6f;
                    fppCam->needUpdate = true;

                    switch(mWorld->mainOptions.difficult)
                    {
                    case 0: // peaceful
                        HurtPlayerConsiderArmor(mWorld->kickedDamage*0.5f);
                    break;
                    case 1: // easy
                        HurtPlayerConsiderArmor(mWorld->kickedDamage*0.66f);
                    break;
                    case 2: // normal
                        HurtPlayerConsiderArmor(mWorld->kickedDamage);
                    break;
                    case 3: // hard
                        HurtPlayerConsiderArmor(mWorld->kickedDamage*1.33f);
                    break;
                    }

                    if(mWorld->armorId[0] != 324)
                    {
                        mWorld->armorAm[0] -= (mWorld->kickedDamage/2.0f)+rand()%6;
                    }
                    if(mWorld->armorId[1] != 325)
                    {
                        mWorld->armorAm[1] -= (mWorld->kickedDamage/1.5f)+rand()%6;
                    }
                    if(mWorld->armorId[2] != 326)
                    {
                        mWorld->armorAm[2] -= (mWorld->kickedDamage/2.0f)+rand()%4;
                    }
                    if(mWorld->armorId[3] != 327)
                    {
                        mWorld->armorAm[3] -= (mWorld->kickedDamage/2.0f)+rand()%3;
                    }
                }
                mWorld->kickedStart = false;
                mWorld->kickedBy = 0;
                mWorld->kickedDamage = 0;
                // kickedVelY is consumed here; kickedPush has to survive until the slide below, so it
                // is cleared with kickedAngle once the timer runs out.
                mWorld->kickedVelY = 0.0f;
            }

            //update position
            if(sleepTime <= 0.0f)
            {
                if(!canFly)
                {
                    playerPosition += playerVelocity * dt;
                }
                else
                {
                    playerVelocity.y = 0.0f;
                }
            }

            //collision with walls
            isWalking = false;
            Vector3 moveVector = fppCam->m_vVelocity;

            jumping = false;
            if(moveVector.x != 0.0f || moveVector.z != 0.0f)
            {
                if(TryToMove(Vector3(0,0,moveVector.z),dt))
                {
                    isWalking = true;
                }
                else
                {
                    if(mWorld->mainOptions.autoJump && !sneaking)
                    {
                        if(jumping == false)
                        {
                            jumping = true;
                        }
                    }
                }
                if(TryToMove(Vector3(moveVector.x,0,0),dt))
                {
                    isWalking = true;
                }
                else
                {
                    if(mWorld->mainOptions.autoJump && !sneaking)
                    {
                        if(jumping == false)
                        {
                            jumping = true;
                        }
                    }
                }
            }

            if(mWorld->kickedTimer > 0.0f)
            {
                mWorld->kickedTimer -= dt;
                // A blast's sideways throw scales with how close it was; mob kicks keep the flat 0.1.
                float kickPush = (mWorld->kickedPush > 0.0f) ? mWorld->kickedPush : 0.1f;
                TryToMove(Vector3(sinf(mWorld->kickedAngle+PI)*kickPush,0,cosf(mWorld->kickedAngle+PI)*kickPush),dt);
            }
            else
            {
                mWorld->kickedTimer = 0.0f;
                mWorld->kickedAngle = 0.0f;
                mWorld->kickedPush = 0.0f;

                fppCam->needUpdate = true;
            }

            // Portal travel runs before the camera is placed: a swap moves the player, and the camera
            // has to follow it in the same frame rather than lag a frame behind.
            mWorld->UpdatePortalTravel(dt);
            if(mWorld->travelledThisFrame)
            {
                playerPosition = newPlayerPos = oldPlayerPos = mWorld->playerPos;
                mWorld->travelledThisFrame = false;
            }

#if MC_AUTO_FLY
            AutoFlyPlace(dt, playerPosition, fppCam);
            mWorld->HP = 20;
            // The menu's default (StateMenu); a probe never passes through it and the world's own
            // default of 1 leaves the ground outside the mesh zone at flying height.
            mWorld->mainOptions.verticalViewDistance = 2;
#endif
            //update camera
            fppCam->m_vPosition = playerPosition;
            // The portal's wobble. Applied here rather than inside the camera update because
            // RebuildView rebuilds the up vector from scratch every frame and would drop the roll.
            fppCam->rollAngle = mWorld->GetPortalWobbleAngle();
            fppCam->RebuildView();
            fppCam->m_vVelocity = Vector3(0,0,0);
            fppCam->needUpdate = true;

            mWorld->UpdateWorldTime(dt);
            skyLight->UpdateLightSource(mWorld->skyTime);
        }

        mWorld->UpdatePlayerZoneBB(playerPosition);
        mWorld->UpdateWorldProcesses(dt > 0.1f ? 0.1f : dt);

        //walking sound & walking animation
        if(isWalking && walkingOnGround && !canFly && (!footInWater || !footInLava))
        {
            float walkSoundSpeed = dt;
            // Footsteps are paced by the same clock as the legs, so a sprint takes steps 1.3x sooner.
            // Java's step sounds are distance-triggered for exactly this reason.
            if(sprinting)
            {
                walkSoundSpeed *= SPRINT_SPEED_MULT;
            }
            if(footInLava || footInWater)
            {
                walkSoundSpeed *= 0.7f;
            }
			if(headInLava || headInWater)
            {
                walkSoundSpeed *= 0.5f;
            }
		    walkSoundAccu += walkSoundSpeed;

			if(walkSoundAccu > 0.428f || walkSoundAccu == 0.0f)
			{
				mSoundMgr->PlayWalkSound(soundBlockType);
				walkSoundAccu = 0.0f;
			}
		}
		else
		{
			walkSoundAccu = 0.45f;
		}
        {
            float dx = playerPosition.x - oldPlayerPos.x;
            float dz = playerPosition.z - oldPlayerPos.z;
            float step = sqrtf(dx * dx + dz * dz);
            oldPlayerPos = playerPosition;

            // Hunger's movement cost, Java's EntityPlayer.addMovementStat: exhaustion is charged per
            // *metre* actually travelled, so a sprinted metre (0.1) is ten walked ones (0.01) and the
            // bar empties about ten times faster for the same distance -- which is the difference the
            // old per-second subtraction could not express. Charged here, once per frame, because this
            // is the only place that knows the distance that survived collision and whether the player
            // was sprinting, wading or airborne. Airborne horizontal travel is deliberately free, as
            // in Java, unless the player is in water, which is checked first for the same reason.
            if(step > 0.0001f && !canFly)
            {
                if(footInWater || headInWater)
                    mWorld->AddExhaustion(step * EXHAUST_SWIM);
                else if(walkingOnGround)
                    mWorld->AddExhaustion(step * (sprinting ? EXHAUST_SPRINT : EXHAUST_WALK));
            }

            // The limb animation is shared between ground and air on purpose: it is driven by
            // horizontal distance, not by contact, so a jump, a fall or creative flight keeps the
            // limbs moving exactly as they would on the ground. Java does the same -- limbSwing
            // advances with horizontal speed in EntityLivingBase, and it never consults the ground --
            // and it is what makes a jump read as part of a run instead of a pause in it, and what
            // animates a flying player at all. Only the footstep sound stays ground-gated. There is
            // no canFly test here in either play state: survival cannot fly, and creative wants the
            // animation precisely while it is flying.
            float target = isWalking ? (sneaking ? 0.35f : 1.0f) : 0.0f;
            float k = 8.0f * dt;
            if(k > 1.0f)
                k = 1.0f;
            bobWalk += (target - bobWalk) * k;

            // The camera bob is a *separate* quantity in Java, and a fussier one. EntityPlayerSP
            // .onLivingUpdate builds cameraYaw from the *per-tick horizontal distance travelled*,
            // capped at 0.1 and forced to zero whenever the player is not on the ground:
            //
            //     float f2 = sqrt(dx*dx + dz*dz);
            //     if (f2 > 0.1F) f2 = 0.1F;
            //     if (!this.onGround || this.getHealth() <= 0.0F) f2 = 0.0F;
            //     this.cameraYaw += (f2 - this.cameraYaw) * 0.4F;
            //
            // Reproduced with Java's numbers rather than reused from bobWalk, because all three of its
            // properties matter and none of them survive aliasing it to the limb amplitude: it is
            // proportional to speed (so sneaking bobs about 60% as far as walking), it saturates at
            // walking pace (so sprinting does not bob harder, as in Java), and it stops dead in the
            // air. ApplyWalkBob works in tenths of a block, so the 10x is just the unit change.
            // Java's 0.4-per-tick ease is 8 per second, which is the k above.
            float camSpeed = (dt > 0.0f) ? (step / (20.0f * dt)) : 0.0f;
            if(camSpeed > 0.1f)
                camSpeed = 0.1f;
            if(!(isWalking && walkingOnGround && !canFly))
                camSpeed = 0.0f;
            bobCamera += (camSpeed - bobCamera) * k;

            if(step > 0.0001f)
            {
                // Distance-derived, so the cycle already runs 1.3x faster while sprinting because the
                // player covers 1.3x the ground. The extra factor on top is for readability: at this
                // screen size 1.3x alone reads as a slightly brisk walk, and a run should read as a
                // run. It is the one deliberate departure from Java's numbers in the walk cycle.
                float cycleScale = sprinting ? 1.15f : 1.0f;
                bobCycle += step * 0.6f * PI * cycleScale;
            }

            float camBob = bobCamera * 10.0f;
            fppCam->ApplyWalkBob(camBob, bobCycle);
        }

        if(invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true)
        {
            mWorld->UpdateCompassTexture((fppCam->horAngle/180.0f)*PI);
        }
        else
        {
            if(mWorld->haveCompass) // only if you have compass in bar slots, compass texture should be updated
            {
                mWorld->UpdateCompassTexture((fppCam->horAngle/180.0f)*PI);
            }
        }
    }
}


void StatePlay::Draw(StateManager* sManager)
{    // Sprint FOV. Java's EntityRenderer widens the FOV by 15% while sprinting, and eases it there
    // rather than snapping, so the zoom reads as acceleration. Rebuilt before StartFrame because
    // SetupCameraMatrices loads the projection, and only touched when it actually changes: SetPerspective
    // rebuilds the projection matrix, which is not free.
    if(mWorld != NULL && mRender != NULL)
    {
        float targetBoost = sprinting ? ((float)mWorld->mainOptions.fov * SPRINT_FOV_MULT) : 0.0f;
        float lerp = (dt > 0.0f) ? (12.0f * dt) : 1.0f;
        if(lerp > 1.0f)
            lerp = 1.0f;
        sprintFovBoost += (targetBoost - sprintFovBoost) * lerp;
        float wantFov = (float)mWorld->mainOptions.fov + sprintFovBoost;
        if(wantFov < 40.0f)
            wantFov = 40.0f;
        if(wantFov > 120.0f)
            wantFov = 120.0f;
        if(fabsf(mRender->fovv - wantFov) > 0.01f)
        {
            mRender->fovv = wantFov;
            RenderManager::InstancePtr()->SetPerspective(0, 480.0f / 272.0f, 0.18f, 1000.f);
        }
    }

    if(menuState == 0 && mWorld != NULL && fppCam != NULL)
    {
        Vector3 fwd = fppCam->m_vView - fppCam->m_vPosition;
        fwd.normalize();

        // Third person. The view matrix is built from m_vPosition + m_vBoom, so this is a render-only
        // offset: the eye stays on the player, and every pick ray, particle, sky and sound that reads
        // the camera keeps working from the eyes exactly as before. The boom is walked backwards from
        // the eye and cut at the last clear step, which is what stops the camera ending up inside a
        // wall. Rotating or moving already marks the camera for update, so needUpdate is only forced
        // when the boom itself changes -- that is what refreshes the frustum on the frame the view is
        // toggled, without asking for one every frame.
        //
        // The front view is the same walk in the other direction plus the mirror flag, which turns the
        // view matrix round to look back at the player (see RenderManager::SetupCameraMatrices). Both
        // third-person views are therefore mirrors of each other through the eye, so the eye stays the
        // pivot and the crosshair keeps aiming where the player is looking.
        Vector3 wantBoom = Vector3(0.0f,0.0f,0.0f);
        bool wantMirror = false;
        if(thirdPersonView > 0)
        {
            float boomSide = (thirdPersonView == 2) ? 1.0f : -1.0f;
            float boom = THIRD_PERSON_DISTANCE;
            for(int i = 1; i <= THIRD_PERSON_STEPS; i++)
            {
                float d = THIRD_PERSON_DISTANCE * (float)i / (float)THIRD_PERSON_STEPS;
                Vector3 sample = fppCam->m_vPosition + (fwd * (d * boomSide));
                if(mWorld->BlockSolid((int)floorf(sample.x),(int)floorf(sample.y),(int)floorf(sample.z)))
                {
                    boom = THIRD_PERSON_DISTANCE * (float)(i-1) / (float)THIRD_PERSON_STEPS;
                    break;
                }
            }
            if(boom < THIRD_PERSON_NEVER_BELOW)
                boom = THIRD_PERSON_NEVER_BELOW;

            wantBoom = fwd * (boom * boomSide);
            wantMirror = (thirdPersonView == 2);
        }

        if(wantBoom.x != fppCam->m_vBoom.x || wantBoom.y != fppCam->m_vBoom.y || wantBoom.z != fppCam->m_vBoom.z
           || wantMirror != fppCam->m_vMirror)
        {
            fppCam->m_vBoom = wantBoom;
            fppCam->m_vMirror = wantMirror;
            fppCam->needUpdate = true;
        }

        Vector3 camUp = fppCam->m_vUpVector;
        Vector3 camRight = Vector3::cross(fwd, camUp);
        if(camRight.magnitudeSq() < 0.0001f)
            camRight = Vector3(1.0f, 0.0f, 0.0f);
        else
            camRight.normalize();
        camUp = Vector3::cross(camRight, fwd);
        camUp.normalize();
        if(pendingCrackId >= 0)
        {
            Particle::QueueBlockCrack(mWorld, Vector3(pendingCrackX, pendingCrackY, pendingCrackZ), pendingCrackId);
            pendingCrackId = -1;
        }
        Particle::UpdateAll(mWorld, dt, camRight, camUp);
    }

    mWorld->startSkyColor = mWorld->startClearSkyColor*(1.0-mWorld->rainyColorAlpha)+mWorld->startRainySkyColor*mWorld->rainyColorAlpha;
    mWorld->endSkyColor = mWorld->endClearSkyColor*(1.0-mWorld->rainyColorAlpha)+mWorld->endRainySkyColor*mWorld->rainyColorAlpha;
    mWorld->dawnSunsetSkyColor = mWorld->dawnSunsetClearSkyColor*(1.0-mWorld->rainyColorAlpha)+mWorld->dawnSunsetRainySkyColor*mWorld->rainyColorAlpha;
    (mWorld->startSkyColor).saturate();
    (mWorld->endSkyColor).saturate();
    (mWorld->dawnSunsetSkyColor).saturate();

    mRender->SetFontStyle(0.345f,0xFFFFFFFF,0,0x00000200);

    if (mWorld->skyTime < 180)
    {
        if(fppCam->horAngle < 360 && fppCam->horAngle >= 239)
        {
            angleFactor = (fppCam->horAngle - 239)/120.0f;
        }
        else if(fppCam->horAngle >= 0 && fppCam->horAngle <= 120)
        {
            angleFactor = (120 - fppCam->horAngle)/120.0f;
        }
        else
        {
            angleFactor = 0;
        }
    }
    else
    {
        if(fppCam->horAngle <= 180 && fppCam->horAngle >= 60)
        {
            angleFactor = (fppCam->horAngle - 60)/120.0f;
        }
        else if(fppCam->horAngle > 180 && fppCam->horAngle <= 300)
        {
            angleFactor = (300 - fppCam->horAngle)/120.0f;
        }
        else
        {
            angleFactor = 0;
        }
    }
    if(angleFactor < 0.0f)
    {
        angleFactor = 0.0f;
    }
    if(angleFactor > 1.0f)
    {
        angleFactor = 1.0f;
    }

	// The nether has no sky of its own. The overworld's is what washed the whole dimension grey, so
	// instead it gets a fixed dark red for both the clear colour and the fog, and everything that
	// draws a sky -- skybox, stars, sun, moon, clouds -- is skipped below.
	const bool nether = (mWorld->dimension == 1);
	const Vector3 netherSky(0.28f, 0.05f, 0.05f);

	bool needUpdate = fppCam->needUpdate;
	float nullFactor = (1.0f-mWorld->dawnSunsetFactor)+(1.0f-angleFactor);
	if(nullFactor > 1.0f)
    {
        nullFactor = 1.0f;
    }

    if(nether)
    {
        mRender->StartFrame(netherSky.x,netherSky.y,netherSky.z);
    }
    else if(playerPosition.y <= 48)
    {
        Vector3 color = ((mWorld->startSkyColor*mWorld->brightFactor)*nullFactor+(mWorld->dawnSunsetSkyColor*mWorld->dawnSunsetFactor*angleFactor))*(playerPosition.y/48.0f)*(playerPosition.y/48.0f);
        mRender->StartFrame(color.x,color.y,color.z);
    }
    else
    {
        Vector3 color = (mWorld->startSkyColor*mWorld->brightFactor)*nullFactor+(mWorld->dawnSunsetSkyColor*mWorld->dawnSunsetFactor*angleFactor);
        mRender->StartFrame(color.x,color.y,color.z);
    }
    // StartFrame has waited for the previous frame's GE work; the atlases are free to change.
    mWorld->ApplyTextureAnimation();

    if(mWorld->mainOptions.smoothLighting == 1)
    {
        sceGuShadeModel(GU_SMOOTH);
    }
    else
    {
        sceGuShadeModel(GU_FLAT);
    }

    if(mWorld->mainOptions.detailedSky == 1 && !nether)
    {
        if(playerPosition.y <= 48)
        {
            Vector3 color = ((mWorld->startSkyColor*mWorld->brightFactor)*nullFactor+(mWorld->dawnSunsetSkyColor*mWorld->dawnSunsetFactor*angleFactor))*(playerPosition.y/48.0f)*(playerPosition.y/48.0f);
            sceGuFog(mWorld->mainOptions.horizontalViewDistance * 8, mWorld->mainOptions.horizontalViewDistance * 16 + 168.0f, GU_COLOR(color.x,color.y,color.z,1.0));
        }
        else
        {
            Vector3 color = (mWorld->startSkyColor*mWorld->brightFactor)*nullFactor+(mWorld->dawnSunsetSkyColor*mWorld->dawnSunsetFactor*angleFactor);
            sceGuFog(mWorld->mainOptions.horizontalViewDistance * 8, mWorld->mainOptions.horizontalViewDistance * 16 + 168.0f, GU_COLOR(color.x,color.y,color.z,1.0));
        }
        sceGuEnable(GU_FOG);

        /// Skybox rendering
        {
            MatrixPush();
            MatrixTranslation(Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y+44,fppCam->m_vPosition.z));
            MatrixScale(Vector3(1.0f,1.0f,1.0f));

            if(playerPosition.y <= 45)
            {
                skyBox->Render(mWorld->endSkyColor*mWorld->brightFactor*(playerPosition.y/45.0f),playerPosition,fppCam->horAngle/180.0f*PI);
            }
            else
            {
                skyBox->Render(mWorld->endSkyColor*mWorld->brightFactor,playerPosition,fppCam->horAngle/180.0f*PI);
            }
            MatrixPop();
        }
        sceGuDisable(GU_FOG);

		/// StarsBox rendering
        if(mWorld->rainyColorAlpha != 1.0f)
        {
            if(mWorld->worldDayTime < 10 || mWorld->worldDayTime >= 21)
            {
                TextureManager::Instance()->SetTextureModeulate(stars);

                MatrixPush();
                MatrixTranslation(Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y+fppCam->m_vOffset.y,fppCam->m_vPosition.z));
                MatrixScale(Vector3(1.0f,1.0f,1.0f));

                if(mWorld->brightFactor <= 0.05f)
                {
                    if(mWorld->starsFactor < 0.3f)
                    {
                        mWorld->starsFactor += dt*0.03f;
                    }
                    if(mWorld->starsFactor > 0.3f)
                    {
                        mWorld->starsFactor = 0.3f;
                    }
                }
                else
                {
                    if(mWorld->starsFactor > 0.0f)
                    {
                        mWorld->starsFactor -= dt*0.03f;
                    }
                    if(mWorld->starsFactor < 0.0f)
                    {
                        mWorld->starsFactor = 0.0f;
                    }
                }

                starsBox->Render((1.0f-mWorld->brightFactor-0.4+mWorld->starsFactor)*(1-mWorld->rainyColorAlpha));


                MatrixPop();
            }
        }

        /// Sun/moon rendering
        if(mWorld->rainyColorAlpha != 1)
        {
            MatrixColor(GU_COLOR(1.0f-mWorld->rainyColorAlpha,1.0f-mWorld->rainyColorAlpha,1.0f-mWorld->rainyColorAlpha,1.0f-mWorld->rainyColorAlpha));

            // sun
            MatrixPush();
            MatrixTranslation(Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y,fppCam->m_vPosition.z));

            TextureManager::Instance()->SetTextureModeulate(suntex);

            skyLight->Render();
            MatrixPop();

            // moon
            MatrixPush();
            MatrixTranslation(Vector3(fppCam->m_vPosition.x,fppCam->m_vPosition.y,fppCam->m_vPosition.z));

            TextureManager::Instance()->SetTextureModeulate(moontex);

            skyMoonLight->UpdateLightSource(mWorld->skyTime+180.0f);
            skyMoonLight->Render();
            MatrixPop();
        }

        if(mWorld->rainyColorAlpha != 1.0f)
        {
            if(mWorld->mainOptions.fogRendering == 1) /// FOG FOR CLOUDS
            {
                if(headInWater == true)
                {
                    sceGuFog(0.0f, 15.0f, GU_COLOR(74.0f/255.0f*mWorld->brightFactor*0.6,130.0f/255.0f*mWorld->brightFactor*0.6,158.0f/255.0f*mWorld->brightFactor*0.6, 1.0f));
                }
                else
                {
                    if(playerPosition.y <= 48)
                    {
                        Vector3 color = (( mWorld->startSkyColor*mWorld->brightFactor)*nullFactor+(mWorld->dawnSunsetSkyColor*mWorld->dawnSunsetFactor*angleFactor))*(playerPosition.y/48.0f)*(playerPosition.y/48.0f);
                        sceGuFog(mWorld->mainOptions.horizontalViewDistance * 8 + 48.0f, mWorld->mainOptions.horizontalViewDistance * 16 + 122.0f, GU_COLOR(color.x,color.y,color.z,1.0));
                    }
                    else
                    {
                        Vector3 color = (mWorld->startSkyColor*mWorld->brightFactor)*nullFactor+(mWorld->dawnSunsetSkyColor*mWorld->dawnSunsetFactor*angleFactor);
                        sceGuFog(mWorld->mainOptions.horizontalViewDistance * 8 + 48.0f, mWorld->mainOptions.horizontalViewDistance * 16 + 122.0f, GU_COLOR(color.x,color.y,color.z,1.0));
                    }
                }
                sceGuEnable(GU_FOG);
            }

            /// Clouds rendering

            MatrixPush();

            if(playerPosition.y <= 48)
            {
                MatrixColor(GU_COLOR(mWorld->brightFactor*(playerPosition.y/48.0f),mWorld->brightFactor*(playerPosition.y/48.0f),mWorld->brightFactor*(playerPosition.y/48.0f),1-mWorld->rainyColorAlpha));
            }
            else
            {
                MatrixColor(GU_COLOR(mWorld->brightFactor,mWorld->brightFactor,mWorld->brightFactor,1-mWorld->rainyColorAlpha));
            }

            MatrixTranslation(Vector3(-114,140,-114));

            TextureManager::Instance()->SetTextureModeulate(cloudsTex);

            sceGuTexWrap(GU_REPEAT,GU_REPEAT);
            sceGuTexOffset(cloudsOffset,0.0f); // fake movement
            mWorld->drawClouds();
            sceGuTexWrap(GU_CLAMP,GU_CLAMP);

            MatrixPop();
            MatrixColor(0xFFFFFFFF);

            if(mWorld->mainOptions.fogRendering == 1)
            {
                sceGuDisable(GU_FOG);
            }
			/// Layer 2

            MatrixPush();

            if(playerPosition.y <= 48)
            {
                MatrixColor(GU_COLOR(mWorld->brightFactor*(playerPosition.y/28.0f),mWorld->brightFactor*(playerPosition.y/28.0f),mWorld->brightFactor*(playerPosition.y/28.0f),1-mWorld->rainyColorAlpha));
            }
            else
            {
                MatrixColor(GU_COLOR(mWorld->brightFactor,mWorld->brightFactor,mWorld->brightFactor,1-mWorld->rainyColorAlpha));
            }

            MatrixTranslation(Vector3(-114,141,-114));

            TextureManager::Instance()->SetTextureModeulate(cloudsTex);

            sceGuTexWrap(GU_REPEAT,GU_REPEAT);
            sceGuTexOffset(cloudsOffset,0.0f); // fake movement
            mWorld->drawClouds();
            sceGuTexWrap(GU_CLAMP,GU_CLAMP);

            MatrixPop();
            MatrixColor(0xFFFFFFFF);

            if(mWorld->mainOptions.fogRendering == 1)
            {
                sceGuDisable(GU_FOG);
            }
        }
    }

    if(mWorld->mainOptions.fogRendering == 1) /// FOG FOR WORLD
    {
        if(headInWater == true)
        {
            sceGuFog(0.0f, 15.0f, GU_COLOR(74.0f/255.0f*mWorld->brightFactor*0.6,130.0f/255.0f*mWorld->brightFactor*0.6,158.0f/255.0f*mWorld->brightFactor*0.6, 1.0f));
        }
        else
        {
            float fog_start = 20.0f;
            float fog_end = mWorld->EffectiveViewDistance() * 4 + 40.00f;
            if(fog_start < 0.0f)
            {
                fog_start = 0.0f;
            }
            if(nether)
            {
                // Pulled in closer than the overworld's: the nether should feel enclosed, and the fog
                // is what stops the far wall of a cavern reading as flat untextured rock. The 0.6 also
                // keeps the fog inside the mesh zone now that the nether's effective view is one chunk
                // shorter -- see CraftWorld::EffectiveViewDistance -- so the shorter view cannot show
                // an edge of unmeshed terrain past the fog.
                float nether_end = fog_end * 0.6f;
                float mesh_edge = (float)(mWorld->EffectiveViewDistance() * CHUNK_SIZE);
                if(nether_end > mesh_edge)
                    nether_end = mesh_edge;
                sceGuFog(fog_start, nether_end, GU_COLOR(netherSky.x,netherSky.y,netherSky.z,1.0));
            }
            else if(playerPosition.y <= 48)
            {
                Vector3 color = ((Vector3(0.8f-(mWorld->rainyColorAlpha/10.0f)*3,0.8f-(mWorld->rainyColorAlpha/10.0f)*3,0.8f-(mWorld->rainyColorAlpha/10.0f)*3)*mWorld->brightFactor)*nullFactor+(mWorld->dawnSunsetSkyColor*mWorld->dawnSunsetFactor*angleFactor))*(playerPosition.y/48.0f)*(playerPosition.y/48.0f);
                sceGuFog(fog_start, fog_end, GU_COLOR(color.x,color.y,color.z,1.0));
            }
            else
            {
                Vector3 color = (Vector3(0.8f-(mWorld->rainyColorAlpha/10.0f)*3,0.8f-(mWorld->rainyColorAlpha/10.0f)*3,0.8f-(mWorld->rainyColorAlpha/10.0f)*3)*mWorld->brightFactor)*nullFactor+(mWorld->dawnSunsetSkyColor*mWorld->dawnSunsetFactor*angleFactor);
                sceGuFog(fog_start, fog_end, GU_COLOR(color.x,color.y,color.z,1.0));
            }
        }
        sceGuEnable(GU_FOG);	// Enable fog
    }

    DrawSetDepthTest(true);
    DrawSetAlphaTest(true);
    DrawSetBlend(true);
    DrawSetDepthMask(false);

    /// DRAW WORLD
    {
        sceGuEnable(GU_TEXTURE_2D);
        if(mWorld->mainOptions.mipMapTexturing)
        {
            TextureManager::Instance()->SetMipMaps2Textures(texture, texture_mips, mWorld->mainOptions.mipMapBias);
        }
        else
        {
            TextureManager::Instance()->SetTextureModeulate(texture);
        }

        sceGumPushMatrix();
        MC_PERF_PHASE_BEGIN(kPhDrawWorld);
        mWorld->drawWorld(fppCam->mFrustum,needUpdate);
        MC_PERF_PHASE_END(kPhDrawWorld);
        sceGumPopMatrix();
        sceGuDisable(GU_TEXTURE_2D);
    }

    /// Drop rendering
    if(mWorld->mDrops.empty() == false)
    {
        for(int d = 0; d < mWorld->mDrops.size(); d++)
        {
            if(d < mWorld->mDrops.size())
            {
                Drop* TestDrop = mWorld->mDrops[d];

                if(WorldRunning())//game state
                {
                    TestDrop->Update(dt);
                }

                if(IsBlockId(TestDrop->GetId()))
                {
                    TextureManager::Instance()->SetTextureModeulate(texture);
                }
                else
                {
                    mWorld->ItemHaveTerrainTexture(TestDrop->GetId()) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);
                }

                TestDrop->Render(fppCam->mFrustum, playerPosition, fppCam->horAngle/180.0f*PI);

                if(TestDrop->toDestroy == true)
                {
                    mWorld->DestroyDrop(d);
                    d -= 1;
                    continue;
                }

                if(TestDrop->getMe == true)
                {
                    mWorld->PutInInventory(TestDrop->GetId(),TestDrop->GetAmount(),TestDrop->GetStackable());
                    mSoundMgr->PlayPlopSound();
                    mWorld->DestroyDrop(d);
                    d -= 1;
                }
            }
        }
    }

    if(mWorld->mTNTs.empty() == false)
    {
        TextureManager::Instance()->SetTextureModeulate(texture);
        for(int k = mWorld->mTNTs.size()-1; k >= 0; k--)
        {
            if(mWorld->mTNTs.empty() == true)
            {
                return;
            }

            TNT* TestTNT = mWorld->mTNTs[k];

            if(TestTNT != NULL) // if this TNT exists
            {
                if(TestTNT->toDestroy == false)
                {
                    if(WorldRunning())//game state
                    {
                        TestTNT->Update(dt);
                        int time = TestTNT->timeToExplode*100;
                        if(time % 4 == 0 && rand()%10 <= 6 && time > 40 && TestTNT->ignitedByPlayer)
                        {

                            Particle* Smoke = Particle::Spawn(mWorld,"smoke",Vector3(TestTNT->position.x+(-0.1f+(rand()%10)/100.0f),TestTNT->position.y+0.60f,TestTNT->position.z+(-0.1f+(rand()%10)/100.0f)));
                            Smoke->SetVelocity(Vector3(0,1.5f+(rand()%10)/10.0f,0));
                            Smoke->SetScale(0.1f+(rand()%10)/100.0f);
                            Smoke->SetMaxLiveTime(1000);
                            Smoke->SetLiveTime(500,1250);
                            Smoke->SetBrightness(0.15,0.25);
                            Smoke->friction = 4.0f;

                        }
                    }

                    TestTNT->Render();
                }

                if(TestTNT->toDestroy == true)
                {
                    for(float verAngle = 0.0f; verAngle <= PI; verAngle += PI/4.0f)
                    {
                        for(float horAngle = 0.0f; horAngle <= PI*2; horAngle += PI/2.0f)
                        {
                            if(Particle::ActiveCount() > 52)
                            {
                                continue;
                            }
                            float radius = 4.5f + (rand() % 100)/10.0f;

                            Particle* Smoke = Particle::Spawn(mWorld,"smoke",Vector3(TestTNT->position.x,TestTNT->position.y,TestTNT->position.z));
                            Smoke->SetVelocity(Vector3(mWorld->LengthDirX(horAngle,verAngle)*radius,mWorld->LengthDirY(horAngle,verAngle)*radius,mWorld->LengthDirZ(horAngle,verAngle)*radius));
                            Smoke->SetScale(0.3f+(rand()%25)/100.0f);
                            Smoke->friction = 4.25f;

                        }
                    }
                    if(Particle::ActiveCount() < 64)
                    {
                        Particle::Spawn(mWorld,"explosion",Vector3(TestTNT->position.x-1.2f,TestTNT->position.y-1.2f,TestTNT->position.z));
                        Particle::Spawn(mWorld,"explosion",Vector3(TestTNT->position.x+1.2f,TestTNT->position.y+1.2f,TestTNT->position.z));
                        Particle::Spawn(mWorld,"explosion",Vector3(TestTNT->position.x,TestTNT->position.y-1.2f,TestTNT->position.z-1.2f));
                        Particle::Spawn(mWorld,"explosion",Vector3(TestTNT->position.x,TestTNT->position.y+1.2f,TestTNT->position.z+1.2f));
                        Particle::Spawn(mWorld,"explosion",Vector3(TestTNT->position.x,TestTNT->position.y,TestTNT->position.z));
                    }

                    mWorld->DestroyTNTentity(k);
                    if(mWorld->mTNTs.size() <= 16)
                    {
                        mSoundMgr->TNTSound();
                    }
                }
            }
        }
    }

    /// THIRD-PERSON PLAYER MODEL
    // Drawn where the mobs are drawn, so it sits in the world pass with the same lighting and its own
    // depth. Only in third person: in first person the player is the camera and the hand/model pass
    // further down covers it.
    if(thirdPersonView > 0 && mWorld->HP > 0 && sleepTime <= 0.0f)
    {
        // Swing comes from the walk bob the camera already computes: bobCycle advances with distance
        // travelled rather than with time, so the legs stop moving the moment the player does, and
        // bobWalk fades the whole swing out so a standing player is not left frozen mid-stride.
        // Arms swing against the leg on the same side, as in Java.
        float walkSwing = sinf(bobCycle) * 0.85f * bobWalk;

        // Soles, from the game's own standing relation: the eye is playerGrowth - 0.05 above them.
        Vector3 feet = Vector3(playerPosition.x,
                               playerPosition.y - (mWorld->playerGrowth - 0.05f),
                               playerPosition.z);

        // The mobs' angle convention: their forward is (sin, 0, cos) while the camera's is
        // (cos, 0, sin), so the quarter turn is the conversion. This is the *body's* heading, which
        // lags the camera; the head is turned past it by headYawOffset and nods with the camera's own
        // pitch, so the player keeps looking where the camera looks while the body swings round.
        float faceAngle = PI/2.0f - (bodyYaw/180.0f)*PI;
        float headTurn = -(headYawOffset/180.0f)*PI;

        float playerLight = mWorld->BlockFinalLight((int)floorf(feet.x),(int)floorf(feet.y + 1.0f),(int)floorf(feet.z));

        // Punch and place. animDest runs 0..PI over a swing, so sin() of it is the 0..1..0 arc, which
        // is Java's swingProgress*PI exactly -- the same curve the first-person hand uses. 1.3 rad at
        // the peak puts the arm a little past horizontal, less than the first-person hand's 1.5
        // because this arm cannot also turn across the body without a second axis.
        float hitSwing = sinf(animDest) * 1.3f;

        TextureManager::Instance()->SetTextureModeulate(invPlayerTex);
        invPlayer->RenderInWorld(mWorld, feet, faceAngle, walkSwing, -walkSwing, playerLight,
                                 invPlayerTex, mWorld->invId[27+barPosition],
                                 headTurn, fppCam->upDownAngle, hitSwing,
                                 // -1 = a bow in hand but no draw, so the arm keeps its walk pose;
                                 // 0..3 = the string is being pulled, which lifts the arm. See
                                 // InventoryPlayer::RenderInWorld and kBowArmRaise.
                                 bowCharging == true ? BowDrawStage(bowCharge) : -1);
    }

    /// ZOMBIE
    if(mWorld->mZombies.empty() == false)
    {
        for(int d = 0; d < mWorld->mZombies.size(); d++)
        {
            if(d < mWorld->mZombies.size() && mWorld->mZombies.empty() == false)
            {
                Zombie* TestZombie = mWorld->mZombies[d];

                if(WorldRunning())//game state
                {
                    TestZombie->Update(mWorld, playerPosition, dt);
                }
                TestZombie->Render(fppCam->mFrustum, dt);

                if(TestZombie->playHurtSound)
                {
                    mSoundMgr->PlayZombieHurtSound(TestZombie->FastDistanceToPlayer());
                    TestZombie->playHurtSound = false;
                }

                if(TestZombie->playSaySound)
                {
                    mSoundMgr->PlayZombieSaySound(TestZombie->FastDistanceToPlayer());
                    TestZombie->playSaySound = false;
                }

                if(TestZombie->killed == true)
                {
                    if(TestZombie->playDieSound)
                    {
                        mSoundMgr->PlayZombieDieSound(TestZombie->FastDistanceToPlayer());
                        TestZombie->playDieSound = false;
                    }
                }

                if(TestZombie->toDestroy == true || (TestZombie->killed == true && TestZombie->deathTimer <= 0.0f))
                {
                    if(TestZombie->killed == true)
                    {
                        if(TestZombie->lastKickType != 2) // kicked by player or other death
                        {
                            int rfAmount = rand() % 3;
                            if(rfAmount != 0)
                            {
                                mWorld->DropThis(RottenFlesh::getID(),rfAmount,true,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            }

                            // rare drop
                            if(rand() % 100 <= 2)
                            {
                                mWorld->DropThis(IronBar::getID(),1,true,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            }
                            if(rand() % 100 <= 3)
                            {
                                mWorld->DropThis(Potato::getID(),1,true,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            }
                            if(rand() % 100 <= 3)
                            {
                                mWorld->DropThis(Carrot::getID(),1,true,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            }
                            // end

                            if(TestZombie->helmetId != -1 && rand() % 4 <= 2)
                            {
                                mWorld->DropThis(TestZombie->helmetId,30+rand()%20,false,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            }
                            if(TestZombie->chestplateId != -1 && rand() % 4 <= 2)
                            {
                                mWorld->DropThis(TestZombie->chestplateId,30+rand()%20,false,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            }
                            if(TestZombie->leggingsId != -1 && rand() % 4 <= 2)
                            {
                                mWorld->DropThis(TestZombie->leggingsId,30+rand()%20,false,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            }
                            if(TestZombie->bootsId != -1 && rand() % 4 <= 2)
                            {
                                mWorld->DropThis(TestZombie->bootsId,30+rand()%20,false,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            }
                        }
                        else // killed by explosion
                        {
                            mWorld->DropThis(Bone::getID(),1,true,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            mWorld->DropThis(Bone::getID(),1,true,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            if(rand()%3 == 0)
                            {
                                mWorld->DropThis(Bone::getID(),1,true,Vector3(TestZombie->position.x,TestZombie->position.y+0.3f,TestZombie->position.z));
                            }
                        }
                    }

                    for(int k = 0; k <= 7; k++)
                    {
                        Particle::Spawn(mWorld, "smoke",Vector3(TestZombie->position.x-0.4+(rand()%9/10.0f),TestZombie->position.y+0.2f+(rand()%7/5.0f),TestZombie->position.z-0.4+(rand()%9/10.0f)));
                    }
                    mWorld->DestroyZombie(d);
                    d --;
                }
            }
        }
    }

    /// COWS
    if(mWorld->mCows.empty() == false)
    {
        for(int d = 0; d < mWorld->mCows.size(); d++)
        {
            if(d < mWorld->mCows.size() && mWorld->mCows.empty() == false)
            {
                Cow* TestCow = mWorld->mCows[d];

                if(WorldRunning())//game state
                {
                    TestCow->handItemId = mWorld->invId[27+barPosition];
                    TestCow->Update(mWorld, playerPosition, dt);
                }
                TestCow->Render(fppCam->mFrustum, dt);

                if(TestCow->playHurtSound)
                {
                    mSoundMgr->PlayCowHurtSound(TestCow->FastDistanceToPlayer());
                    TestCow->playHurtSound = false;
                }

                if(TestCow->playSaySound)
                {
                    mSoundMgr->PlayCowSaySound(TestCow->FastDistanceToPlayer());
                    TestCow->playSaySound = false;
                }

                if(TestCow->toDestroy == true || (TestCow->killed == true && TestCow->deathTimer < 0.0f))
                {
                    if(TestCow->killed == true && TestCow->IsBaby() == false)
                    {
                        mWorld->DropThis(RawBeef::getID(),1+rand()%2,true,Vector3(TestCow->position.x,TestCow->position.y+0.3f,TestCow->position.z));
                        mWorld->DropThis(Leather::getID(),1+rand()%2,true,Vector3(TestCow->position.x,TestCow->position.y+0.3f,TestCow->position.z));
                    }

                    for(int k = 0; k <= 7; k++)
                    {
                        Particle* Smoke = Particle::Spawn(mWorld, "smoke",Vector3(TestCow->position.x-0.5+(rand()%11/10.0f),TestCow->position.y+0.2+(rand()%4/5.0f),TestCow->position.z-0.5+(rand()%11/10.0f)));
                        k % 2 == 0 ? Smoke->SetVelocity(Vector3(-3*rand()%2,0,-3*rand()%2)) : Smoke->SetVelocity(Vector3(3*rand()%2,0,3*rand()%2));
                    }

                    mWorld->DestroyCow(d);
                    d --;
                }
            }
        }
    }

    /// SHEEP
    if(mWorld->mSheeps.empty() == false)
    {
        for(int d = 0; d < mWorld->mSheeps.size(); d++)
        {
            if(d < mWorld->mSheeps.size() && mWorld->mSheeps.empty() == false)
            {
                Sheep* TestSheep = mWorld->mSheeps[d];

                if(WorldRunning())//game state
                {
                    TestSheep->handItemId = mWorld->invId[27+barPosition];
                    TestSheep->Update(mWorld, playerPosition, dt);
                }
                TestSheep->Render(fppCam->mFrustum, dt);

                if(TestSheep->playHurtSound)
                {
                    mSoundMgr->PlaySheepHurtSound(TestSheep->FastDistanceToPlayer());
                    TestSheep->playHurtSound = false;
                }

                if(TestSheep->playSaySound)
                {
                    mSoundMgr->PlaySheepHurtSound(TestSheep->FastDistanceToPlayer());
                    TestSheep->playSaySound = false;
                }

                if(TestSheep->toDestroy == true || (TestSheep->killed == true && TestSheep->deathTimer < 0.0f))
                {
                    if(TestSheep->killed == true && TestSheep->sheared == false && TestSheep->IsBaby() == false)
                    {
                        mWorld->DropThis(BlackWoolBlock::getID()+TestSheep->GetFurColor(),1,true,Vector3(TestSheep->position.x,TestSheep->position.y+0.3f,TestSheep->position.z));
					mWorld->DropThis(RawMutton::getID(),1+rand()%2,true,Vector3(TestSheep->position.x,TestSheep->position.y+0.3f,TestSheep->position.z));
                    }

                    for(int k = 0; k <= 7; k++)
                    {
                        Particle* Smoke = Particle::Spawn(mWorld, "smoke",Vector3(TestSheep->position.x-0.5+(rand()%11/10.0f),TestSheep->position.y+0.2+(rand()%4/5.0f),TestSheep->position.z-0.5+(rand()%11/10.0f)));
                        k % 2 == 0 ? Smoke->SetVelocity(Vector3(-3*rand()%2,0,-3*rand()%2)) : Smoke->SetVelocity(Vector3(3*rand()%2,0,3*rand()%2));
                    }

                    mWorld->DestroySheep(d);
                    d --;
                }
            }
        }
    }

    /// CREEPER
    if(mWorld->mCreepers.empty() == false)
    {
        for(int d = 0; d < mWorld->mCreepers.size(); d++)
        {
            if(d < mWorld->mCreepers.size() && mWorld->mCreepers.empty() == false)
            {
                Creeper* TestCreeper = mWorld->mCreepers[d];

                if(WorldRunning())//game state
                {
                    TestCreeper->Update(mWorld, playerPosition, dt);
                }
                TestCreeper->Render(fppCam->mFrustum, dt);

                if(TestCreeper->playHurtSound)
                {
                    mSoundMgr->PlayCreeperHurtSound(TestCreeper->DistanceToPlayer());
                    TestCreeper->playHurtSound = false;
                }

                if(TestCreeper->playFuseSound)
                {
                    mSoundMgr->PlayFuseSound();
                    TestCreeper->playFuseSound = false;
                }

                if(TestCreeper->toDestroy == true || (TestCreeper->killed == true && TestCreeper->deathTimer < 0.0f))
                {
                    if(TestCreeper->killed == true)
                    {
                        if(TestCreeper->lastKickType != 2) // killed by player or other death
                        {
                            mWorld->DropThis(Gunpowder::getID(),1,true,Vector3(TestCreeper->position.x,TestCreeper->position.y+0.3f,TestCreeper->position.z));
                            mWorld->DropThis(Gunpowder::getID(),rand()%3,true,Vector3(TestCreeper->position.x,TestCreeper->position.y+0.3f,TestCreeper->position.z));
                        }
                        else
                        {
                            int drop_id = 1;
                            int disk_number = rand()%12;
                            if(disk_number <= 2)
                            {
                                drop_id = Disk1::getID() + disk_number;
                            }
                            else
                            {
                                drop_id = Disk4::getID() + disk_number-3;
                            }
                            mWorld->DropThis(drop_id,1,false,Vector3(TestCreeper->position.x,TestCreeper->position.y+0.3f,TestCreeper->position.z));
                        }
                    }

                    for(int k = 0; k <= 7; k++)
                    {
                        Particle* Smoke = Particle::Spawn(mWorld, "smoke",Vector3(TestCreeper->position.x-0.5+(rand()%11/10.0f),TestCreeper->position.y+0.2+(rand()%4/5.0f),TestCreeper->position.z-0.5+(rand()%11/10.0f)));
                        k % 2 == 0 ? Smoke->SetVelocity(Vector3(-3*rand()%2,0,-3*rand()%2)) : Smoke->SetVelocity(Vector3(3*rand()%2,0,3*rand()%2));
                    }

                    mWorld->DestroyCreeper(d);
                    d --;
                }
                if(TestCreeper->exploded == true)
                {
                    for(float verAngle = 0.0f; verAngle <= PI; verAngle += PI/4.0f)
                    {
                        for(float horAngle = 0.0f; horAngle <= PI*2; horAngle += PI/2.0f)
                        {
                            if(Particle::ActiveCount() > 52)
                            {
                                continue;
                            }
                            float radius = 4.5f + (rand() % 100)/10.0f;

                            Particle* Smoke = Particle::Spawn(mWorld,"smoke",Vector3(TestCreeper->position.x,TestCreeper->position.y,TestCreeper->position.z));
                            Smoke->SetVelocity(Vector3(mWorld->LengthDirX(horAngle,verAngle)*radius,mWorld->LengthDirY(horAngle,verAngle)*radius,mWorld->LengthDirZ(horAngle,verAngle)*radius));
                            Smoke->SetScale(0.3f+(rand()%25)/100.0f);
                            Smoke->friction = 4.25f;

                        }
                    }
                    if(Particle::ActiveCount() < 64)
                    {
                        Particle::Spawn(mWorld,"explosion",Vector3(TestCreeper->position.x-1.2f,TestCreeper->position.y-1.2f,TestCreeper->position.z));
                        Particle::Spawn(mWorld,"explosion",Vector3(TestCreeper->position.x+1.2f,TestCreeper->position.y+1.2f,TestCreeper->position.z));
                        Particle::Spawn(mWorld,"explosion",Vector3(TestCreeper->position.x,TestCreeper->position.y-1.2f,TestCreeper->position.z-1.2f));
                        Particle::Spawn(mWorld,"explosion",Vector3(TestCreeper->position.x,TestCreeper->position.y+1.2f,TestCreeper->position.z+1.2f));
                        Particle::Spawn(mWorld,"explosion",Vector3(TestCreeper->position.x,TestCreeper->position.y,TestCreeper->position.z));
                    }

                    mSoundMgr->TNTSound();

                    mWorld->DestroyCreeper(d);
                    d --;
                }
            }
        }
    }

    /// PIG
    if(mWorld->mPigs.empty() == false)
    {
        for(int d = 0; d < mWorld->mPigs.size(); d++)
        {
            if(d < mWorld->mPigs.size() && mWorld->mPigs.empty() == false)
            {
                Pig* TestPig = mWorld->mPigs[d];

                if(WorldRunning())//game state
                {
                    TestPig->handItemId = mWorld->invId[27+barPosition];
                    TestPig->Update(mWorld, playerPosition, dt);
                }
                TestPig->Render(fppCam->mFrustum, dt);

                if(TestPig->playHurtSound)
                {
                    mSoundMgr->PlayPigHurtSound(TestPig->FastDistanceToPlayer());
                    TestPig->playHurtSound = false;
                }

                if(TestPig->playSaySound)
                {
                    mSoundMgr->PlayPigSaySound(TestPig->FastDistanceToPlayer());
                    TestPig->playSaySound = false;
                }

                if(TestPig->toDestroy == true || (TestPig->killed == true && TestPig->deathTimer < 0.0f))
                {
                    if(TestPig->killed == true && TestPig->IsBaby() == false)
                    {
                        mWorld->DropThis(RawPorkchop::getID(),1+rand()%2,true,Vector3(TestPig->position.x,TestPig->position.y+0.3f,TestPig->position.z));
                    }

                    for(int k = 0; k <= 7; k++)
                    {
                        Particle* Smoke = Particle::Spawn(mWorld, "smoke",Vector3(TestPig->position.x-0.5+(rand()%11/10.0f),TestPig->position.y+0.2+(rand()%4/5.0f),TestPig->position.z-0.5+(rand()%11/10.0f)));
                        k % 2 == 0 ? Smoke->SetVelocity(Vector3(-3*rand()%2,0,-3*rand()%2)) : Smoke->SetVelocity(Vector3(3*rand()%2,0,3*rand()%2));
                    }

                    mWorld->DestroyPig(d);
                    d --;
                }
            }
        }
    }

    /// chickens. The mesh is smaller than the pig's. Drops feathers and raw chicken when killed, and
    /// lays an egg on a timer (see Chicken::Update).
    if (mWorld->mChickens.empty() == false)
    {
        for(int d = 0; d < mWorld->mChickens.size(); d++)
        {
            if(d < mWorld->mChickens.size() && mWorld->mChickens.empty() == false)
            {
                Chicken* TestChicken = mWorld->mChickens[d];

                if(WorldRunning())//game state
                {
                    TestChicken->handItemId = mWorld->invId[27+barPosition];
                    TestChicken->Update(mWorld, playerPosition, dt);
                }
                TestChicken->Render(fppCam->mFrustum, dt);

                // Chicken audio, converted from the vanilla 1.7.3 assets by
                // tools/make-vanilla-mob-sounds.py (mob/chicken hurt1-2, say1-3, step1-2, plop).
                if(TestChicken->playHurtSound)
                {
                    mSoundMgr->PlayChickenHurtSound(TestChicken->FastDistanceToPlayer());
                }

                if(TestChicken->playSaySound)
                {
                    mSoundMgr->PlayChickenSaySound(TestChicken->FastDistanceToPlayer());
                }

                if(TestChicken->playPlopSound)
                {
                    mSoundMgr->PlayChickenPlopSound(TestChicken->FastDistanceToPlayer());
                }

                TestChicken->playHurtSound = false;
                TestChicken->playSaySound = false;
                TestChicken->playPlopSound = false;

                if(TestChicken->toDestroy == true || (TestChicken->killed == true && TestChicken->deathTimer < 0.0f))
                {
                    if(TestChicken->killed == true && TestChicken->IsBaby() == false)
                    {
                        mWorld->DropThis(Feather::getID(),1+rand()%2,true,Vector3(TestChicken->position.x,TestChicken->position.y+0.3f,TestChicken->position.z));
                        mWorld->DropThis(RawChicken::getID(),1,true,Vector3(TestChicken->position.x,TestChicken->position.y+0.3f,TestChicken->position.z));
                    }

                    for(int k = 0; k <= 7; k++)
                    {
                        Particle* Smoke = Particle::Spawn(mWorld, "smoke",Vector3(TestChicken->position.x-0.5+(rand()%11/10.0f),TestChicken->position.y+0.2+(rand()%4/5.0f),TestChicken->position.z-0.5+(rand()%11/10.0f)));
                        k % 2 == 0 ? Smoke->SetVelocity(Vector3(-3*rand()%2,0,-3*rand()%2)) : Smoke->SetVelocity(Vector3(3*rand()%2,0,3*rand()%2));
                    }

                    mWorld->DestroyChicken(d);
                    d --;
                }
            }
        }
    }


    /// SPIDERS. Four-footed like the pig on the API side, but hostile and with its own walk cycle:
    /// the class drives all eight legs off one phase, so the loop only has to feed it the player and
    /// the frame time. Drops string, and a spider eye one time in three (Java: 0-2 string always,
    /// one eye at a 1/3 chance).
    if (mWorld->mSpiders.empty() == false)
    {
        for(int d = 0; d < mWorld->mSpiders.size(); d++)
        {
            if(d < mWorld->mSpiders.size() && mWorld->mSpiders.empty() == false)
            {
                Spider* TestSpider = mWorld->mSpiders[d];

                if(WorldRunning())//game state
                {
                    TestSpider->Update(mWorld, playerPosition, dt);
                }
                TestSpider->Render(fppCam->mFrustum, dt);

                // Spider audio, converted from the vanilla 1.7.3 assets by
                // tools/make-vanilla-mob-sounds.py (mob/spider say1-4, step1-4, death).
                if(TestSpider->playHurtSound)
                {
                    mSoundMgr->PlaySpiderHurtSound(TestSpider->FastDistanceToPlayer());
                }

                if(TestSpider->playSaySound)
                {
                    mSoundMgr->PlaySpiderSaySound(TestSpider->FastDistanceToPlayer());
                }

                if(TestSpider->playStepSound)
                {
                    mSoundMgr->PlaySpiderStepSound(TestSpider->FastDistanceToPlayer());
                }

                if(TestSpider->killed == true)
                {
                    if(TestSpider->playDieSound)
                    {
                        mSoundMgr->PlaySpiderDeathSound(TestSpider->FastDistanceToPlayer());
                        TestSpider->playDieSound = false;
                    }
                }

                TestSpider->playHurtSound = false;
                TestSpider->playSaySound = false;
                TestSpider->playStepSound = false;

                if(TestSpider->toDestroy == true || (TestSpider->killed == true && TestSpider->deathTimer < 0.0f))
                {
                    if(TestSpider->killed == true && TestSpider->IsBaby() == false)
                    {
                        mWorld->DropThis(String::getID(),1+rand()%2,true,Vector3(TestSpider->position.x,TestSpider->position.y+0.3f,TestSpider->position.z));
                        if(rand() % 3 == 0)
                        {
                            mWorld->DropThis(SpiderEye::getID(),1,true,Vector3(TestSpider->position.x,TestSpider->position.y+0.3f,TestSpider->position.z));
                        }
                    }

                    for(int k = 0; k <= 7; k++)
                    {
                        Particle* Smoke = Particle::Spawn(mWorld, "smoke",Vector3(TestSpider->position.x-0.5+(rand()%11/10.0f),TestSpider->position.y+0.2+(rand()%4/5.0f),TestSpider->position.z-0.5+(rand()%11/10.0f)));
                        k % 2 == 0 ? Smoke->SetVelocity(Vector3(-3*rand()%2,0,-3*rand()%2)) : Smoke->SetVelocity(Vector3(3*rand()%2,0,3*rand()%2));
                    }

                    mWorld->DestroySpider(d);
                    d --;
                }
            }
        }
    }

    /// skeletons
    if (mWorld->mSkeletons.empty() == false)
    {
        for(int d = 0; d < mWorld->mSkeletons.size(); d++)
        {
            if(d < mWorld->mSkeletons.size() && mWorld->mSkeletons.empty() == false)
            {
                Skeleton* TestSkeleton = mWorld->mSkeletons[d];

                if(WorldRunning())//game state
                {
                    TestSkeleton->Update(mWorld, playerPosition, dt);
                }
                TestSkeleton->Render(fppCam->mFrustum, dt);

                // Skeleton audio, converted from the vanilla 1.7.3 assets by
                // tools/make-vanilla-mob-sounds.py (mob/skeleton say1-3, hurt1-4, step1-4, death).
                if(TestSkeleton->playHurtSound)
                {
                    mSoundMgr->PlaySkeletonHurtSound(TestSkeleton->FastDistanceToPlayer());
                }

                if(TestSkeleton->playSaySound)
                {
                    mSoundMgr->PlaySkeletonSaySound(TestSkeleton->FastDistanceToPlayer());
                }

                if(TestSkeleton->playStepSound)
                {
                    mSoundMgr->PlaySkeletonStepSound(TestSkeleton->FastDistanceToPlayer());
                }

                if(TestSkeleton->killed == true)
                {
                    if(TestSkeleton->playDieSound)
                    {
                        mSoundMgr->PlaySkeletonDeathSound(TestSkeleton->FastDistanceToPlayer());
                        TestSkeleton->playDieSound = false;
                    }
                }

                TestSkeleton->playHurtSound = false;
                TestSkeleton->playSaySound = false;
                TestSkeleton->playStepSound = false;

                if(TestSkeleton->toDestroy == true || (TestSkeleton->killed == true && TestSkeleton->deathTimer < 0.0f))
                {
                    if(TestSkeleton->killed == true)
                    {
                        // Vanilla EntitySkeleton.dropFewItems: 0-2 bones, then 0-2 arrows, and a bow
                        // on roughly one death in twelve.
                        int bones = rand() % 3;
                        for(int b = 0; b < bones; b++)
                        {
                            mWorld->DropThis(Bone::getID(),1,true,Vector3(TestSkeleton->position.x,TestSkeleton->position.y+0.3f,TestSkeleton->position.z));
                        }
                        int arrows = rand() % 3;
                        for(int a = 0; a < arrows; a++)
                        {
                            mWorld->DropThis(Arrow::getID(),1,true,Vector3(TestSkeleton->position.x,TestSkeleton->position.y+0.3f,TestSkeleton->position.z));
                        }
                        if(rand() % 4 == 0)
                        {
                            // Dropped as a usable bow, not a one-use one: a non-stacking item carries
                            // its durability in the drop's amount field, the same way a worn tool
                            // dropped from the hotbar does. See CraftWorld::FreshItemAmount.
                            mWorld->DropThis(Bow::getID(),mWorld->FreshItemAmount(Bow::getID()),false,Vector3(TestSkeleton->position.x,TestSkeleton->position.y+0.3f,TestSkeleton->position.z));
                        }
                    }

                    for(int k = 0; k <= 7; k++)
                    {
                        Particle* Smoke = Particle::Spawn(mWorld, "smoke",Vector3(TestSkeleton->position.x-0.5+(rand()%11/10.0f),TestSkeleton->position.y+0.2+(rand()%4/5.0f),TestSkeleton->position.z-0.5+(rand()%11/10.0f)));
                        k % 2 == 0 ? Smoke->SetVelocity(Vector3(-3*rand()%2,0,-3*rand()%2)) : Smoke->SetVelocity(Vector3(3*rand()%2,0,3*rand()%2));
                    }

                    mWorld->DestroySkeleton(d);
                    d --;
                }
            }
        }
    }

    /// Arrows: fly them, resolve what they hit, land them, collect them, cap them. All of it is one
    /// call into the world (CraftWorld::UpdateArrows) so that this state and the creative one cannot
    /// disagree about what a shot does -- which is the point of moving it out, because these two
    /// loops used to hold two copies of the whole thing and only one of the copies ever gets edited.
    if(WorldRunning())//game state
    {
        mWorld->UpdateArrows(dt);
    }

    /// Draw the shots that are left: the ones in the air, and the ones that have landed and are
    /// waiting to be walked over. A landed arrow is not simulated any more, so its own geometry is
    /// rebuilt here instead of in Update -- it has stopped, so the heading it arrived on is the one it
    /// keeps, and it still has to take the light where it fell.
    if (mWorld->mArrows.empty() == false)
    {
        for(unsigned int i = 0; i < mWorld->mArrows.size(); i++)
        {
            if(i < mWorld->mArrows.size() && mWorld->mArrows.empty() == false)
            {
                Arrow2* UseArrow = mWorld->mArrows[i];

                UseArrow->BuildVertices(mWorld);

                MatrixPush();
                MatrixTranslation(Vector3(0,0,0));
                DrawSetDepthTest(true);
                DrawSetAlphaTest(true);

                UseArrow->Render();

                DrawSetAlphaTest(false);
                DrawSetDepthTest(false);
                MatrixPop();
            }
        }
    }

    /// render snowballs
    if (mSnowBalls.empty() == false)
    {
        for(unsigned int i = 0; i < mSnowBalls.size(); i++)
        {
            if(i < mSnowBalls.size() && mSnowBalls.empty() == false)
            {
                TextureManager::Instance()->SetTextureModeulate(snowBall4);
                SnowBall2* UseSnowBall = mSnowBalls[i];

                if(WorldRunning())//game state
                {
                    UseSnowBall->Update(mWorld, dt);
                }

                MatrixPush();
                MatrixTranslation(Vector3(0,0,0));
                DrawSetDepthTest(true);
                DrawSetAlphaTest(true);

                UseSnowBall->Render();

                DrawSetAlphaTest(false);
                DrawSetDepthTest(false);
                MatrixPop();

                if(UseSnowBall->CheckCollision(mWorld) == true)
                {
                    if(Particle::ActiveCount() <= 52)
                    {
                        for(int k = 0; k <= 3; k++) // 4 particles
                        {
                            Particle* SnowballPart = Particle::Spawn(mWorld,"item crack",Vector3(UseSnowBall->position.x,UseSnowBall->position.y+0.1f,UseSnowBall->position.z));
                            SnowballPart->var1 = SnowBallItem::getID()-250;
                            SnowballPart->SetScale(0.06f, 0.12f);
                        }
                    }

                    delete mSnowBalls[i];
                    mSnowBalls.erase(mSnowBalls.begin()+i);
                    i --;
                }
            }
        }
    }

    /// render particles
    if(fppCam != NULL)
        Particle::RenderAll(fppCam->mFrustum);

    /// render items in item frames
    if (mWorld->mItemFrames.empty() == false)
    {
        for(std::vector<ItemFrameEntity*>::size_type i = 0; i < mWorld->mItemFrames.size(); i++)
        {
            ItemFrameEntity* TestItemFrame = mWorld->mItemFrames[i];

            if(TestItemFrame->GetItem() != -1)
            {
                TestItemFrame->Render(mWorld,fppCam->mFrustum,dt);
            }
        }
    }

    if(mWorld->mainOptions.fogRendering == 1)
    {
        sceGuDisable(GU_FOG); // disable fog
    }

    /// weather effects rendering
    if(WorldRunning() || makeScreen == true)
    {
        if(mWorld->worldWeather->GetWeatherType() == 1) // if it is rain
        {
            if(mWorld->rainyColorAlpha < 1.0f)
            {
                mWorld->rainyColorAlpha += dt/3.0f;
            }
            if(mWorld->rainyColorAlpha > 1.0f)
            {
                mWorld->rainyColorAlpha = 1.0f;
            }

            rainSoundFrequency -= dt;
            if(!mSoundMgr->RainSoundPlaying())
            {
                mSoundMgr->PlayRainSound(20);
                rainSoundFrequency = 0.55f;
            }
            else if(rainSoundFrequency <= 0.0f)
            {
                mSoundMgr->PlayRainSound(20);
                rainSoundFrequency = 0.55f;
            }
        }
        else // clear weather
        {
            mSoundMgr->StopRainSound();
            rainSoundFrequency = 0.0f;
            if(mWorld->rainyColorAlpha > 0.0f)
            {
                mWorld->rainyColorAlpha -= dt/8.0f;
            }
            if(mWorld->rainyColorAlpha < 0.0f)
            {
                mWorld->rainyColorAlpha = 0.0f;
            }
        }
        mWorld->worldWeather->Update(mWorld, dt);
        mWorld->UpdateButtons(dt);
        mWorld->UpdateRepeaters(dt);
        mWorld->worldWeather->Render(mWorld, fppCam->mFrustum, fppCam->upDownAngle, playerPosition, dt);
    }

    /// render destroy model
    if (((startDt == 1 && dStd >= 0 && dStd <= 9) || showCube == true) && makeScreen == false && sleepTime <= 0.0f)
	{
		MatrixPush();

        DrawSetDepthTest(true);
        DrawSetAlphaTest(true);
		DrawSetBlend(true);

		sceGuDepthMask(1);

		MatrixTranslation(Vector3(cubePos.x,cubePos.y,cubePos.z));
		MatrixColor(0xFFFFFFFF);

        sceGuTexFilter(GU_NEAREST,GU_NEAREST);
        TextureManager::Instance()->SetTextureModeulate(texture);

		destroyer->Update(dStd, mWorld, mWorld->GetBlock(cubePos.x,cubePos.y,cubePos.z), (int)cubePos.x, (int)cubePos.y, (int)cubePos.z);
		destroyer->Render(dStd);

		sceGuDepthMask(0);

		DrawSetBlend(false);
        DrawSetAlphaTest(false);
        DrawSetDepthTest(false);

		MatrixPop();
	}

    // First-person hand and held item. Skipped in third person: the model standing in the world is
    // already holding the item, so drawing this too would put it inside the camera as well.
	if(mWorld->mainOptions.guiDrawing == 1 && mWorld->HP > 0 && thirdPersonView == 0)
	{
        float cubeBob = 0.0f;
        // No hand bob while flying; see StatePlayCreative's copy for the reasoning. canFly is always
        // false in survival today, so this is a no-op here -- it is written the same way so the two
        // hand blocks cannot drift apart if that ever changes.
        float cubeBob2 = canFly ? 0.0f : (-fabsf(cosf(bobCycle)) * bobWalk / 18);

        sceGuColor(GU_COLOR(cubeLight,cubeLight,cubeLight,1)); // set color to drawing model
        if(sleepTime <= 0.0f)
        {
            if(mWorld->invId[27+barPosition] == -1) // slot is empty so we need to draw steve's hand
            {
                TextureManager::Instance()->SetTextureModeulate(invPlayerTex);
                sceGumPushMatrix();
                sceGumLoadIdentity(); // hand is drawn in view space: drop the world-origin shift

                //set view matrix to identity
                sceGumMatrixMode(GU_VIEW);
                sceGumLoadIdentity();
                //translate
                ScePspFVector3 move = {0.53f+cubeBob+shift_x+(mWorld->mainOptions.fov-70)/200.0f+sinf(animDest)*-0.35,-0.42f+shift_y+cubeBob2+changeY,-0.5f+(mWorld->mainOptions.fov-70)/130.0f}; //446
                sceGumTranslate(&move);
                //rotate
                sceGumRotateX(-0.72f+sinf(animDest)*-1.5);//0.1
                sceGumRotateY(3.4839f+sinf(animDest)*-0.3); //0.9
                sceGumRotateZ(0.57f); //0.9
                //scale
                ScePspFVector3 scale = {0.49f,0.49f,0.49f};
                sceGumScale(&scale);
                //draw hand
                sceGuDisable(GU_DEPTH_TEST);
                sceGuDepthMask(1);
                mWorld->drawHand();
                sceGuEnable(GU_DEPTH_TEST);
                sceGuDepthMask(0);

                sceGumMatrixMode(GU_MODEL);
                sceGumPopMatrix();
            }

            if(IsBlockId(mWorld->invId[27+barPosition]) && mWorld->invId[27+barPosition] != -1)
            {
                TextureManager::Instance()->SetTextureModeulate(texture);
                sceGumPushMatrix();
                sceGumLoadIdentity(); // hand is drawn in view space: drop the world-origin shift

                //set view matrix to identity
                sceGumMatrixMode(GU_VIEW);
                sceGumLoadIdentity();
                //translate
                ScePspFVector3 move = {0.62f+cubeBob+shift_x+(mWorld->mainOptions.fov-70)/200.0f+sinf(animDest)*-0.35,-0.35f+shift_y+cubeBob2+changeY+sinf(animDest)*-0.1,-0.7f+(mWorld->mainOptions.fov-70)/130.0f}; //446
                sceGumTranslate(&move);
                //rotate
                sceGumRotateX(0.169+sinf(animDest)*-1.5);
                sceGumRotateY(0.934+sinf(animDest)*-0.3); //3
                //scale
                ScePspFVector3 scale = {0.32f,0.32f,0.32f};
                sceGumScale(&scale);
                //draw cube
                sceGuDisable(GU_DEPTH_TEST);
                sceGuDepthMask(1);
                mWorld->drawCubes(mWorld->invId[27+barPosition],cubeLight);
                sceGuEnable(GU_DEPTH_TEST);
                sceGuDepthMask(0);

                sceGumMatrixMode(GU_MODEL);
                sceGumPopMatrix();
            }

            if(IsItemId(mWorld->invId[27+barPosition]))
            {
                // A held bow is drawn as a weapon, and while the string is pulled it is drawn as one of
                // its three pulling frames. Those frames are item types of their own, because an item's
                // atlas cell is baked into its mesh at load and this is the only way to get a different
                // cell into the hand. Both facts are shared with the skeleton and the third-person
                // model, so a bow looks the same in every hand that holds one.
                const int heldId = mWorld->invId[27+barPosition];
                const bool holdingBow = IsBowItem(heldId);
                const int drawId = holdingBow ? BowHeldMeshId(bowCharging == true ? BowDrawStage(bowCharge) : 0) : heldId;

                mWorld->ItemHaveTerrainTexture(mWorld->invId[27+barPosition]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);
                sceGumPushMatrix();
                sceGumLoadIdentity(); // hand is drawn in view space: drop the world-origin shift

                //set view matrix to identity
                sceGumMatrixMode(GU_VIEW);
                sceGumLoadIdentity();

                //translate
                ScePspFVector3 move = {0.523f+cubeBob+shift_x+(mWorld->mainOptions.fov-70)/250.0f+sinf(animDest)*-0.25,-0.24f+cubeBob2+shift_y+changeY,-0.6f+(mWorld->mainOptions.fov-70)/180.0f};//-0.17
                sceGumTranslate(&move); //-0.22
                //rotate
                sceGumRotateX(-0.33f);
                sceGumRotateY(-1.47+sinf(animDest)*-0.3+PI); //-1.47
                // ~-PI stands the sprite up (GetItemVerts authors it upside down); the tilt is this
                // hand's own solved angle, which puts the artwork's arrow in the plane the shot leaves
                // along. The third person and the skeleton need different numbers -- their chains are
                // different chains. See kHeldBowFirstPersonTilt.
                sceGumRotateZ(-3.13-sinf(animDest)*1.4+(holdingBow ? kHeldBowFirstPersonTilt : 0.0f)); //-3.8
                //scale. A bow is drawn at weapon size; every other item keeps the size its 16x16 face
                // was tuned for. See kHeldBowScale.
                float heldScale = (holdingBow == true) ? kHeldBowScale : 0.435f;
                ScePspFVector3 scale = {heldScale,heldScale,heldScale};
                sceGumScale(&scale);

                //draw item
                sceGuDisable(GU_DEPTH_TEST);
                sceGuDepthMask(1);
                mWorld->drawHandItems(drawId,cubeLight);
                sceGuEnable(GU_DEPTH_TEST);
                sceGuDepthMask(0);

                sceGumMatrixMode(GU_MODEL);
                sceGumPopMatrix();
            }
        }
        MatrixColor(0xFFFFFFFF);
	}
	MatrixColor(0xFFFFFFFF);

	/// GUI
	mRender->SetOrtho(0,0,0,0,0,0);

	DrawSetDepthTest(false);
    DrawSetDepthMask(true);
    DrawSetBlend(true);
    DrawPlaceTexture(true);

    TextureManager::Instance()->SetTextureModeulate(Xpbar);
	advancedBlit(0,0,SCR_WIDTH,SCR_HEIGHT,0,0,32);

    DrawPlaceTexture(false);
    DrawSetBlend(false);
    DrawSetDepthMask(false);
    DrawSetDepthTest(true);

    if(mWorld->HP <= 0)
	{
        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);
        DrawPlaceTexture(true);

		MatrixColor(GU_COLOR(1,1,1,0.5f));

		TextureManager::Instance()->SetTextureModeulate(red);
		advancedBlit(0,0,SCR_WIDTH,SCR_HEIGHT,0,0,32);

        DrawPlaceTexture(false);
        DrawSetBlend(false);
        DrawSetDepthMask(false);
        DrawSetDepthTest(true);
	}

    if(hurt == true)
	{
        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);
        DrawPlaceTexture(true);

        MatrixColor(GU_COLOR(1,1,1,hurt_time));

		TextureManager::Instance()->SetTextureModeulate(red);
		advancedBlit(0,0,SCR_WIDTH,SCR_HEIGHT,0,0,32);

        DrawPlaceTexture(false);
        DrawSetBlend(false);
        DrawSetDepthMask(false);
        DrawSetDepthTest(true);
	}

	if(headInLava == 1 || footInLava == 1)
	{
        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);
        DrawPlaceTexture(true);

        MatrixColor(GU_COLOR(1,1,1,0.7));

		TextureManager::Instance()->SetTextureModeulate(red);
		advancedBlit(0,0,SCR_WIDTH,SCR_HEIGHT,0,0,32);

        DrawPlaceTexture(false);
        DrawSetBlend(false);
        DrawSetDepthMask(false);
        DrawSetDepthTest(true);
	}
	// draw dark mask
    if((invEn == 1 || craft3xEn == 1 || chestEn == 1 || menuState != 0 || furnaceEn == 1) && makeScreen == false)
    {
        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);
        DrawPlaceTexture(true);

        MatrixColor(GU_COLOR(1,1,1,0.7));

        TextureManager::Instance()->SetTextureModeulate(black);
        advancedBlit(0,0,SCR_WIDTH,SCR_HEIGHT,0,0,32);

        DrawPlaceTexture(false);
        DrawSetBlend(false);
        DrawSetDepthMask(false);
        DrawSetDepthTest(true);
    }


    if(sleepTime > 0.0f) // dark mask again
    {
        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);
        DrawPlaceTexture(true);

        MatrixColor(GU_COLOR(1,1,1,(4.7-sleepTime)/4.8f));

        TextureManager::Instance()->SetTextureModeulate(black);
        advancedBlit(0,0,SCR_WIDTH,SCR_HEIGHT,0,0,32);

        DrawPlaceTexture(false);
        DrawSetBlend(false);
        DrawSetDepthMask(false);
        DrawSetDepthTest(true);
    }

    // draw water mask
	if(headInWater)
	{
        if (GRAVITY != -4.0f)
        {
            GRAVITY = -4.0f;
        }

        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);
        DrawPlaceTexture(true);

        MatrixColor(GU_COLOR(1,1,1,0.8));

		waterScreen->Draw();

        DrawPlaceTexture(false);
        DrawSetBlend(false);
        DrawSetDepthMask(false);
        DrawSetDepthTest(true);
	}
	else
    {
        if (GRAVITY != -6.8f)
        {
            GRAVITY = -6.8f;
        }
    }
    // draw pumpkin mask
    if(mWorld->armorId[0] == Pumpkin3::getID())
    {
        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);
        DrawPlaceTexture(true);
     //   sceGuTexFilter( GU_LINEAR, GU_LINEAR );

        MatrixColor(GU_COLOR(1,1,1,1));

		pumpkinMask->Draw();

       // sceGuTexFilter( GU_NEAREST, GU_NEAREST );
        DrawPlaceTexture(false);
        DrawSetBlend(false);
        DrawSetDepthMask(false);
        DrawSetDepthTest(true);
    }

	/// GUI
	DrawSetDepthTest(false);
	DrawSetDepthMask(true);

	DrawSetBlend(true);
	MatrixColor(GU_COLOR(1,1,1,1.0f));
	

	if ((invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && menuState == 0 && mWorld->mainOptions.guiDrawing == 1) || (makeScreen == true && mWorld->mainOptions.guiDrawing == 1))
    {
        hpAnim += dt * 5.9 * PI;
        if(hpAnim >= 10 * PI)
        {
            hpAnim = 0.0f;
        }
        barSprite->Draw();
        if (menuState != 1 || makeScreen == true)
        {
            /// HP and HG RENDERING
            // draw cells
			
            hpCellSpriteW->SetPosition(170, 200);
            hpCellSprite->SetPosition(170, 200);
            hgCellSprite->SetPosition(310, 200);
            arCellSprite->SetPosition(170,185);
            if(hurt == true && sinf(hpAnim) > 0)
            {
                hpCellSpriteW->Draw();
            }
            if(hurt == false || sinf(hpAnim) <= 0)
            {
                hpCellSprite->Draw();
            }
            hgCellSprite->Draw();
            if(mWorld->AP > 0)
            {
                arCellSprite->Draw();
            }

            for(int i = 1; i <= 10; i++)
            {
                hpSprite->SetPosition(106.1f +i* 11.65f,200);
                hpSpriteW->SetPosition(106.1f+i* 11.65f,200);
                hpHalfSprite->SetPosition(106.1f+i* 11.65f,200);
                hpHalfSpriteW->SetPosition(106.1f+i* 11.65f,200);

                hpHardSprite->SetPosition(106.1f+i* 11.65f,200);
                hpHardHalfSprite->SetPosition(106.1f+i* 11.65f,200);
                hpHardSpriteW->SetPosition(106.1f +i* 11.65f,200);
                hpHardHalfSpriteW->SetPosition(106.1f +i* 11.65f,200);

                if(hurt == true && sinf(hpAnim) > 0)
                {
                    if(last_HP > mWorld->HP)
                    {
                        if(i*2 >= floorf(mWorld->HP/2.0f)*2)
                        {
                            if(i * 2 <= ceilf(last_HP))
                            {
                                if(mWorld->gameModeWorld == 2) // hardcore mode
                                {
                                    hpHardSpriteW->Draw();
                                }
                                else
                                {
                                    hpSpriteW->Draw();
                                }
                            }
                            if(i * 2 - ceilf(last_HP) == 1)
                            {
                                if(mWorld->gameModeWorld == 2) // hardcore mode
                                {
                                    hpHardHalfSpriteW->Draw();
                                }
                                else
                                {
                                    hpHalfSpriteW->Draw();
                                }
                            }
                        }
                    }
                }

                if(i * 2 <= ceilf(mWorld->HP) && mWorld->HP >= 0)
                {
                    if(mWorld->gameModeWorld == 2) // hardcore mode
                    {
                        hpHardSprite->Draw();
                    }
                    else
                    {
                        hpSprite->Draw();
                    }
                }
                if(i * 2 - ceilf(mWorld->HP) == 1 && mWorld->HP >= 0)
                {
                    if(mWorld->gameModeWorld == 2) // hardcore mode
                    {
                        hpHardHalfSprite->Draw();
                    }
                    else
                    {
                        hpHalfSprite->Draw();
                    }
                }
                hgSprite->SetPosition(374.1f -i* 11.65f,200);
                hgHalfSprite->SetPosition(374.1f-i*11.65f,200);

                if(i * 2 <= ceilf(mWorld->HG))
                {
                    hgSprite->Draw();
                }
                if(i * 2 - ceilf(mWorld->HG) == 1)
                {
                    hgHalfSprite->Draw();
                }

                arSprite->SetPosition(106.1f+i* 11.65f,185);
                arHalfSprite->SetPosition(106.1f+i* 11.65f,185);

                if(mWorld->AP > 0)
                {
                    if(i * 2 <= ceilf(mWorld->AP))
                    {
                        arSprite->Draw();
                    }
                    if(i * 2 - ceilf(mWorld->AP) == 1)
                    {
                        arHalfSprite->Draw();
                    }
                }

                if(headInWater)
                {
                    bubbleSprite->SetPosition(374.1f-i* 11.5f,185);
                    if(i <= mWorld->OS)
                    {
                        bubbleSprite->Draw();
                    }
				}
            }

            if(mWorld->HP > 0)
            {
                DrawSetInverseColors();
                crossSprite->Draw();
                DrawSetDefaultColors();
            }

            selectSprite->Draw();
        }
    }
    else
    {
        if (invEn == true)
        {
            invSprite->Draw();
        }
        if (craft3xEn == true)
        {
            crtSprite->Draw();
        }
        if (chestEn == true)
        {
            chtSprite->Draw();
        }
        if (furnaceEn == true)
        {
            furSprite->Draw();

            int furFireStage = 0;

            if(UseFurnace->fuelTime >= 10.0f)
            {
                furFireStage = 13;
            }
            if(UseFurnace->fuelTime < 10.0f)
            {
                furFireStage = ((int)(UseFurnace->fuelTime/10.0f*14.0f));
            }

            if(UseFurnace->fuelTime != 0.0f)
            {
                if(furFireStage >= 0 && furFireStage <= 13)
                {
                    furFireSprite[furFireStage]->SetPosition(190,50-((furFireStage-13)*2));
                    furFireSprite[furFireStage]->Draw();
                }
                if(furFireStage == 14)
                {
                    furFireSprite[13]->SetPosition(190,50-((13-4)*6));
                    furFireSprite[13]->Draw();
                }
            }

            if(UseFurnace->meltingTime != 0.0f)
            {
                int furArrowStage = 0;
                furArrowStage = ((int)(UseFurnace->meltingTime/10.0f*22.0f));

                if(furArrowStage >= 0 && furArrowStage <= 21)
                {
                    furArrowSprite[furArrowStage]->SetPosition(236,46);
                    furArrowSprite[furArrowStage]->Draw();
                }
                if(furArrowStage == 22)
                {
                    furArrowSprite[21]->SetPosition(236,46);
                    furArrowSprite[21]->Draw();
                }
            }
        }
    }
	DrawSetBlend(false);

    /// 3D BLOCKS AND ITEMS ON 2D PANEL
    if ((invEn == false && craft3xEn == false && chestEn == false && menuState == 0 && mWorld->mainOptions.guiDrawing == 1 && furnaceEn == false) || (makeScreen == true && mWorld->mainOptions.guiDrawing == 1))
    {
        for(int k = 0; k <= 8; k++)
        {
            if(mWorld->invId[27+k] != -1)
            {
                MatrixPush();

                if(IsBlockId(mWorld->invId[27+k]))
                {
					MatrixTranslation(Vector3(108.0f + (11.0f + k * 20.0f) * (264.0f / 182.0f), 230, 0));
                    TextureManager::Instance()->SetTextureModeulate(texture);

                    sceGuDisable(GU_DEPTH_TEST);
                    DrawSetDepthMask(true);

                    if(k == slotForChangeScale)
                    {
                        MatrixScale(Vector3(18-changeScale*5,18+changeScale*10,18-changeScale*5));

                        changeScale -= dt*3.4f;
                        if(changeScale < 0.0f)
                        {
                            changeScale = 0.0f;
                            slotForChangeScale = -1;
                        }
                    }
                    else
                    {
                        MatrixScale(Vector3(14,14,14));
                    }

                    MatrixAngle(Vector3(150,44.6f,0));

                    DrawSetCulling(true);
                    mWorld->drawCubes(mWorld->invId[27+k],1.0f);
                    DrawSetCulling(false);

                    DrawSetDepthMask(false);
                    sceGuEnable(GU_DEPTH_TEST);
                }

                if(IsItemId(mWorld->invId[27+k]))
                {
                    MatrixTranslation(Vector3(108.0f + (11.0f + k * 20.0f) * (264.0f / 182.0f), 230, 0));
                    mWorld->ItemHaveTerrainTexture(mWorld->invId[27+k]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                    MatrixAngle(Vector3(0,0,0));
                    if(k == slotForChangeScale)
                    {
                        MatrixScale(Vector3(32-changeScale*5,32+changeScale*10,32-changeScale*5));

                        changeScale -= dt*3.4f;
                        if(changeScale < 0.0f)
                        {
                            changeScale = 0.0f;
                            slotForChangeScale = -1;
                        }
                    }
                    else
                    {
                        MatrixScale(Vector3(25,25,25));
                    }

                    DrawSetCulling(true);
                    mWorld->drawItems(mWorld->invId[27+k]);
                    DrawSetCulling(false);
                }
                MatrixPop();
            }
        }
    }

    /// INVENTORY CRAFT MENU
    if(invEn == true)
    {
        for(int i = 0; i <= 1; i++)
        {
            for(int j = 0; j <= 1; j++)
            {
                if(craftSlotId[i*2+j] != -1)
                {
                    MatrixPush();

                    MatrixTranslation(Vector3(GuiSlotX(guiInvW, guiScale, 97.0f + j * 18.0f), GuiSlotY(guiInvH, guiScale, 14.0f + i * 18.0f), 0.0f));

                    if(IsBlockId(craftSlotId[i*2+j]))
                    {
                        TextureManager::Instance()->SetTextureModeulate(texture);

                        mWorld->drawHudCubes(craftSlotId[i*2+j]);
                    }

                    if(IsItemId(craftSlotId[i*2+j]))
                    {
                        mWorld->ItemHaveTerrainTexture(craftSlotId[i*2+j]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                        mWorld->drawHudItems(craftSlotId[i*2+j]);
                    }
                    MatrixPop();
                }
            }
        }

        // The armour slots are the four boxes that flank the player preview in inventory1.png,
        // a 2x2 grid: (7,4) (61,4) / (7,40) (61,40) in texture units. Slot index runs along the
        // rows, so helmet, chestplate on top and leggings, boots underneath.
        for(int aRow = 0; aRow <= 1; aRow++)
        {
            for(int aCol = 0; aCol <= 1; aCol++)
            {
                int a = aRow * 2 + aCol;

                if(mWorld->armorId[a] != -1)
                {
                    MatrixPush();

                    MatrixTranslation(Vector3(GuiSlotX(guiInvW, guiScale, 7.0f + aCol * 54.0f), GuiSlotY(guiInvH, guiScale, 4.0f + aRow * 36.0f), 0.0f));

                    if(IsBlockId(mWorld->armorId[a]))
                    {
                        TextureManager::Instance()->SetTextureModeulate(texture);

                        mWorld->drawHudCubes(mWorld->armorId[a]);
                    }

                    if(IsItemId(mWorld->armorId[a]))
                    {
                        mWorld->ItemHaveTerrainTexture(mWorld->armorId[a]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                        mWorld->drawHudItems(mWorld->armorId[a]);
                    }
                    MatrixPop();
                }
            }
        }


        //Item which we are crafting
        if (craftItemId != -1)
        {
            MatrixPush();

            MatrixTranslation(Vector3(GuiSlotX(guiInvW, guiScale, 151.0f), GuiSlotY(guiInvH, guiScale, 24.0f), 0.0f));

            if(IsBlockId(craftItemId))
            {
                TextureManager::Instance()->SetTextureModeulate(texture);

                mWorld->drawHudCubes(craftItemId);
            }

            if(IsItemId(craftItemId))
            {
                mWorld->ItemHaveTerrainTexture(craftItemId) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                mWorld->drawHudItems(craftItemId);
            }
            MatrixPop();
        }

        if(upEn == false)
        {
            if(invSteveAngle < invXPosition/8.0f*48)
            {
                invSteveAngle += (invXPosition/8.0f*48-invSteveAngle)*0.05;
            }
            if(invSteveAngle > invXPosition/8.0f*48)
            {
                invSteveAngle -= (invSteveAngle-invXPosition/8.0f*48)*0.05;
            }
        }

        MatrixPush();

        MatrixTranslation(Vector3(151,56,-14));
        MatrixScale(Vector3(45,45,45));
        MatrixAngle(Vector3(-14,invSteveAngle,180));// 24

        TextureManager::Instance()->SetTextureModeulate(invPlayerTex);

        invPlayer->mainAngle = 0.45f;

        DrawSetDepthMask(false);
        DrawSetCulling(true);
        invPlayer->Render(mWorld, dt, invSteveAngle, invPlayerTex, mWorld->invId[27+barPosition]);
        DrawSetCulling(false);
        DrawSetDepthMask(true);

        MatrixPop();
    }


    ///CRAFTING TABLE
    if(craft3xEn == true)
    {
        for(int i = 0; i <= 2; i++)
        {
            for(int j = 0; j <= 2; j++)
            {
                if(craftSlotId3[i*3+j] != -1)
                {
                    MatrixPush();

                    MatrixTranslation(Vector3(GuiSlotX(guiCrtW, guiScale, 61.0f + j * 18.0f), GuiSlotY(guiCrtH, guiScale, 4.0f + i * 18.0f), 0.0f));

                    if(IsBlockId(craftSlotId3[i*3+j]))
                    {
                        TextureManager::Instance()->SetTextureModeulate(texture);

                        mWorld->drawHudCubes(craftSlotId3[i*3+j]);
                    }

                    if(IsItemId(craftSlotId3[i*3+j]))
                    {
                        mWorld->ItemHaveTerrainTexture(craftSlotId3[i*3+j] ) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                        mWorld->drawHudItems(craftSlotId3[i*3+j]);
                    }
                    MatrixPop();
                }
            }
        }
        //End

        //Item which we are crafting 3x
        if (craftItemId3 != -1)
        {
            MatrixPush();

            MatrixTranslation(Vector3(GuiSlotX(guiCrtW, guiScale, 133.0f), GuiSlotY(guiCrtH, guiScale, 22.0f), 0.0f));

            if(IsBlockId(craftItemId3))
            {
                TextureManager::Instance()->SetTextureModeulate(texture);

                mWorld->drawHudCubes(craftItemId3);
            }

            if(IsItemId(craftItemId3))
            {
                mWorld->ItemHaveTerrainTexture(craftItemId3) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                mWorld->drawHudItems(craftItemId3);
            }
            MatrixPop();
        }
    }

    ///CHEST
    if(chestEn == true)
    {
        for(int i = 0; i <= 2; i++)
        {
            for(int j = 0; j <= 8; j++)
            {
                if(UseChest->chestSlotId[i*9+j] != -1)
                {
                    MatrixPush();

                    MatrixTranslation(Vector3(GuiSlotX(guiChtW, guiScale, 7.0f + j * 18.0f), GuiSlotY(guiChtH, guiScale, 4.0f + i * 18.0f), 0.0f));

                    if(IsBlockId(UseChest->chestSlotId[i*9+j]))
                    {
                        TextureManager::Instance()->SetTextureModeulate(texture);

                        mWorld->drawHudCubes(UseChest->chestSlotId[i*9+j]);
                    }

                    if(IsItemId(UseChest->chestSlotId[i*9+j]))
                    {
                        mWorld->ItemHaveTerrainTexture(UseChest->chestSlotId[i*9+j]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                        mWorld->drawHudItems(UseChest->chestSlotId[i*9+j]);
                    }
                    MatrixPop();
                }
            }
        }
    }

    ///FURNACE
    if(furnaceEn == true)
    {
        if(UseFurnace->furnaceSlotId[0] != -1)
        {
            sceGumPushMatrix();
            ScePspFVector3 loc = {GuiSlotX(guiFurW, guiScale, 61.0f), GuiSlotY(guiFurH, guiScale, 4.0f), 0.0f};
            sceGumTranslate(&loc);

            if(IsBlockId(UseFurnace->furnaceSlotId[0]))
            {
                TextureManager::Instance()->SetTextureModeulate(texture);

                mWorld->drawHudCubes(UseFurnace->furnaceSlotId[0]);
            }

            if(IsItemId(UseFurnace->furnaceSlotId[0]))
            {
                mWorld->ItemHaveTerrainTexture(UseFurnace->furnaceSlotId[0]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                mWorld->drawHudItems(UseFurnace->furnaceSlotId[0]);
            }
            sceGumPopMatrix();
        }

        if(UseFurnace->furnaceSlotId[1] != -1)
        {
            sceGumPushMatrix();
            ScePspFVector3 loc = {GuiSlotX(guiFurW, guiScale, 61.0f), GuiSlotY(guiFurH, guiScale, 40.0f), 0.0f};
            sceGumTranslate(&loc);

            if(IsBlockId(UseFurnace->furnaceSlotId[1]))
            {
                TextureManager::Instance()->SetTextureModeulate(texture);

                mWorld->drawHudCubes(UseFurnace->furnaceSlotId[1]);
            }

            if(IsItemId(UseFurnace->furnaceSlotId[1]))
            {
                mWorld->ItemHaveTerrainTexture(UseFurnace->furnaceSlotId[1]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                mWorld->drawHudItems(UseFurnace->furnaceSlotId[1]);
            }
            sceGumPopMatrix();
        }

        if(UseFurnace->furnaceSlotId[2] != -1)
        {
            sceGumPushMatrix();
            ScePspFVector3 loc = {GuiSlotX(guiFurW, guiScale, 115.0f), GuiSlotY(guiFurH, guiScale, 22.0f), 0.0f};
            sceGumTranslate(&loc);

            if(IsBlockId(UseFurnace->furnaceSlotId[2]))
            {
                TextureManager::Instance()->SetTextureModeulate(texture);

                mWorld->drawHudCubes(UseFurnace->furnaceSlotId[2]);
            }

            if(IsItemId(UseFurnace->furnaceSlotId[2]))
            {
                mWorld->ItemHaveTerrainTexture(UseFurnace->furnaceSlotId[2]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                mWorld->drawHudItems(UseFurnace->furnaceSlotId[2]);
            }
            sceGumPopMatrix();
        }
    }

    if (invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true)
    {
        for(int i = 0; i <= 3; i++)
        {
            for(int j = 0; j <= 8; j++)
            {
                if(mWorld->invId[i*9+j] != -1)
                {
                    MatrixPush();
                    float panelW = guiInvW;
                    float panelH = guiInvH;
                    if(craft3xEn)
                    {
                        panelW = guiCrtW;
                        panelH = guiCrtH;
                    }
                    else if(chestEn)
                    {
                        panelW = guiChtW;
                        panelH = guiChtH;
                    }
                    else if(furnaceEn)
                    {
                        panelW = guiFurW;
                        panelH = guiFurH;
                    }
                    MatrixTranslation(Vector3(GuiSlotX(panelW, guiScale, 7.0f + j * 18.0f), GuiPlayerInvItemY(panelH, guiScale, i), 0.0f));

                    if(IsBlockId(mWorld->invId[i*9+j]))
                    {
                        TextureManager::Instance()->SetTextureModeulate(texture);

                        mWorld->drawHudCubes(mWorld->invId[i*9+j]);
                    }

                    if(IsItemId(mWorld->invId[i*9+j]))
                    {
                        mWorld->ItemHaveTerrainTexture(mWorld->invId[i*9+j]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                        mWorld->drawHudItems(mWorld->invId[i*9+j]);
                    }
                    MatrixPop();
                }
            }
        }

        DrawSetBlend(true);

        invCellSprite->SetPosition(selectInvSprite->GetPositionX(),selectInvSprite->GetPositionY());
        invCellSprite->Draw();

        if (mWorld->mId != -1) //MOUSE ITEM
        {
            MatrixPush();
            if (upEn == 1)
            {
                if (invEn == 1)
                {
                    MatrixTranslation(Vector3(GuiSlotX(guiInvW, guiScale, 97.0f + invXPosition * 18.0f), GuiSlotY(guiInvH, guiScale, 14.0f + invYPosition * 18.0f) - 8.0f, 0.0f));
                }
                if (craft3xEn == 1)
                {
                    MatrixTranslation(Vector3(GuiSlotX(guiCrtW, guiScale, 61.0f + invXPosition * 18.0f), GuiSlotY(guiCrtH, guiScale, 4.0f + invYPosition * 18.0f) - 8.0f, 0.0f));
                }
                if (chestEn == 1)
                {
                    MatrixTranslation(Vector3(GuiSlotX(guiChtW, guiScale, 7.0f + invXPosition * 18.0f), GuiSlotY(guiChtH, guiScale, 4.0f + invYPosition * 18.0f) - 8.0f, 0.0f));
                }
                if (furnaceEn == 1)
                {
                    MatrixTranslation(Vector3(GuiSlotX(guiFurW, guiScale, 61.0f), GuiSlotY(guiFurH, guiScale, 4.0f + invYPosition * 36.0f) - 8.0f, 0.0f));
                }
            }
            else
            {
                if(armorEn == false)
                {
                    float panelW = guiInvW;
                    float panelH = guiInvH;
                    if(craft3xEn)
                    {
                        panelW = guiCrtW;
                        panelH = guiCrtH;
                    }
                    else if(chestEn)
                    {
                        panelW = guiChtW;
                        panelH = guiChtH;
                    }
                    else if(furnaceEn)
                    {
                        panelW = guiFurW;
                        panelH = guiFurH;
                    }
                    MatrixTranslation(Vector3(GuiSlotX(panelW, guiScale, 7.0f + invXPosition * 18.0f), GuiPlayerInvSlotY(panelH, guiScale, invYPosition) - 8.0f, 0.0f));
                }
                else
                {
                    // The item held on the cursor while it is over the armour grid. This is the same
                    // 2x2 the armour icons and the cursor use -- it was still on the old 1-D column
                    // (x pinned at 7, y = 8 + row*18), so a carried item followed a place the grid no
                    // longer is. The -8.0f matches the inventory branch above: the HUD item draw
                    // centres half a slot low, so every caller pulls it up by the same amount.
                    MatrixTranslation(Vector3(GuiSlotX(guiInvW, guiScale, 7.0f + invXPosition * 54.0f), GuiSlotY(guiInvH, guiScale, 4.0f + invYPosition * 36.0f) - 8.0f, 0.0f));
                }
            }

            if (IsBlockId(mWorld->mId))
            {
                TextureManager::Instance()->SetTextureModeulate(texture);

                mWorld->drawHudCubes(mWorld->mId);
            }

            if (IsItemId(mWorld->mId))
            {
                mWorld->ItemHaveTerrainTexture(mWorld->mId) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                mWorld->drawHudItems(mWorld->mId);
            }
            MatrixPop();
        }
    }

	DrawSetDepthTest(false);
	DrawSetDepthMask(true);
	DrawSetBlend(true);
	MatrixColor(GU_COLOR(1,1,1,1.0f));
    // durability strip drawing
    if ((invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && menuState == 0 && mWorld->mainOptions.guiDrawing == 1) || (makeScreen == true && mWorld->mainOptions.guiDrawing == 1))
    {
        for(int k = 0; k <= 8; k++)
        {
            if(IsItemId(mWorld->invId[27+k]))
            {
                if(mWorld->DurabilityPointsItem(mWorld->invId[27+k]) != -1) // if it is tool
                {
                    if(mWorld->invAm[27+k] != mWorld->DurabilityPointsItem(mWorld->invId[27+k])) // checking durability
                    {
                        unsigned int toolPointStd = roundf((float)(mWorld->invAm[27+k]) / (float)(mWorld->DurabilityPointsItem(mWorld->invId[27+k])) * 13);

                        toolPointSprite[toolPointStd]->SetPosition(115+k*29,240);
                        toolPointSprite[toolPointStd]->Draw();
                    }
                }
            }
        }
    }

    if(invEn == true)
    {
        for(int i = 0; i <= 1; i++)
        {
            for(int j = 0; j <= 1; j++)
            {
                if(IsItemId(craftSlotId[i*2+j]) && mWorld->DurabilityPointsItem(craftSlotId[i*2+j]) != -1)
                {
                    if(craftSlotAm[i*2+j] != mWorld->DurabilityPointsItem(craftSlotId[i*2+j])) // checking durability
                    {
                        unsigned int toolPointStd = roundf((float)(craftSlotAm[i*2+j]) / (float)(mWorld->DurabilityPointsItem(craftSlotId[i*2+j])) * 13);

                        toolPointSprite[toolPointStd]->SetPosition(GuiDurX(GuiSlotX(guiInvW, guiScale, 97.0f + j * 18.0f)), GuiDurY(GuiSlotY(guiInvH, guiScale, 14.0f + i * 18.0f)));
                        toolPointSprite[toolPointStd]->Draw();
                    }
                }
            }
        }

        for(int aRow = 0; aRow <= 1; aRow++)
        {
            for(int aCol = 0; aCol <= 1; aCol++)
            {
                int a = aRow * 2 + aCol;

                if(IsItemId(mWorld->armorId[a]) && mWorld->DurabilityPointsItem(mWorld->armorId[a]) != -1)
                {
                    if(mWorld->armorAm[a] != mWorld->DurabilityPointsItem(mWorld->armorId[a])) // checking durability
                    {
                        unsigned int toolPointStd = roundf((float)(mWorld->armorAm[a]) / (float)(mWorld->DurabilityPointsItem(mWorld->armorId[a])) * 13);

                        toolPointSprite[toolPointStd]->SetPosition(GuiDurX(GuiSlotX(guiInvW, guiScale, 7.0f + aCol * 54.0f)), GuiDurY(GuiSlotY(guiInvH, guiScale, 4.0f + aRow * 36.0f)));
                        toolPointSprite[toolPointStd]->Draw();
                    }
                }
            }
        }
    }

    if(craft3xEn == true)
    {
        for(int i = 0; i <= 2; i++)
        {
            for(int j = 0; j <= 2; j++)
            {
                if(IsItemId(craftSlotId3[i*3+j]) && mWorld->DurabilityPointsItem(craftSlotId3[i*3+j]) != -1)
                {
                    if(craftSlotAm3[i*3+j] != mWorld->DurabilityPointsItem(craftSlotId3[i*3+j])) // checking durability
                    {
                        unsigned int toolPointStd = roundf((float)(craftSlotAm3[i*3+j]) / (float)(mWorld->DurabilityPointsItem(craftSlotId3[i*3+j])) * 13);

                        toolPointSprite[toolPointStd]->SetPosition(GuiDurX(GuiSlotX(guiCrtW, guiScale, 61.0f + j * 18.0f)), GuiDurY(GuiSlotY(guiCrtH, guiScale, 4.0f + i * 18.0f)));
                        toolPointSprite[toolPointStd]->Draw();
                    }
                }
            }
        }
    }

    if(chestEn == true)
    {
        for(int i = 0; i <= 2; i++)
        {
            for(int j = 0; j <= 8; j++)
            {
                if(IsItemId(UseChest->chestSlotId[i*9+j]) && mWorld->DurabilityPointsItem(UseChest->chestSlotId[i*9+j]) != -1)
                {
                    if(UseChest->chestSlotAm[i*9+j] != mWorld->DurabilityPointsItem(UseChest->chestSlotId[i*9+j])) // checking durability
                    {
                        unsigned int toolPointStd = roundf((float)(UseChest->chestSlotAm[i*9+j]) / (float)(mWorld->DurabilityPointsItem(UseChest->chestSlotId[i*9+j])) * 13);

                        toolPointSprite[toolPointStd]->SetPosition(GuiDurX(GuiSlotX(guiChtW, guiScale, 7.0f + j * 18.0f)), GuiDurY(GuiSlotY(guiChtH, guiScale, 4.0f + i * 18.0f)));
                        toolPointSprite[toolPointStd]->Draw();
                    }
                }
            }
        }
    }

    if(furnaceEn == true)
    {
        if(IsItemId(UseFurnace->furnaceSlotId[0]) && mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[0]) != -1) // it is item and it have durability points
        {
            // Only when the tool is actually damaged. The trailing `!= -1` that used to be here made this
            // `((am != max) != -1)`, which is true for a full tool as well, so the bar was drawn for an
            // undamaged one. The outer if has already established the item has durability, so the
            // comparison on its own is the whole test -- the same shape as the inventory, crafting and
            // chest slots above.
            if(UseFurnace->furnaceSlotAm[0] != mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[0])) // checking durability
            {
                unsigned int toolPointStd = roundf((float)(UseFurnace->furnaceSlotAm[0]) / (float)(mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[0])) * 13);

                toolPointSprite[toolPointStd]->SetPosition(GuiDurX(GuiSlotX(guiFurW, guiScale, 61.0f)), GuiDurY(GuiSlotY(guiFurH, guiScale, 4.0f)));
                toolPointSprite[toolPointStd]->Draw();
            }
        }

        if(IsItemId(UseFurnace->furnaceSlotId[1]) && mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[1]) != -1) // it is item and it have durability points
        {
            // See the slot 0 case above: the trailing `!= -1` made this guard always true.
            if(UseFurnace->furnaceSlotAm[1] != mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[1])) // checking durability
            {
                unsigned int toolPointStd = roundf((float)(UseFurnace->furnaceSlotAm[1]) / (float)(mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[1])) * 13);

                toolPointSprite[toolPointStd]->SetPosition(GuiDurX(GuiSlotX(guiFurW, guiScale, 61.0f)), GuiDurY(GuiSlotY(guiFurH, guiScale, 40.0f)));
                toolPointSprite[toolPointStd]->Draw();
            }
        }
    }

    if (invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true)
    {
        for(int i = 0; i <= 3; i++)
        {
            for(int j = 0; j <= 8; j++)
            {
                if(IsItemId(mWorld->invId[i*9+j]))
                {
                    if(mWorld->DurabilityPointsItem(mWorld->invId[i*9+j]) != -1) // if it is tool
                    {
                        if(mWorld->invAm[i*9+j] != mWorld->DurabilityPointsItem(mWorld->invId[i*9+j])) // checking durability
                        {
                            unsigned int toolPointStd = roundf((float)(mWorld->invAm[i*9+j]) / (float)(mWorld->DurabilityPointsItem(mWorld->invId[i*9+j])) * 13);

                            float panelW = guiInvW;
                            float panelH = guiInvH;
                            if(craft3xEn)
                            {
                                panelW = guiCrtW;
                                panelH = guiCrtH;
                            }
                            else if(chestEn)
                            {
                                panelW = guiChtW;
                                panelH = guiChtH;
                            }
                            else if(furnaceEn)
                            {
                                panelW = guiFurW;
                                panelH = guiFurH;
                            }
                            toolPointSprite[toolPointStd]->SetPosition(GuiDurX(GuiSlotX(panelW, guiScale, 7.0f + j * 18.0f)), GuiDurY(GuiPlayerInvItemY(panelH, guiScale, i)));
                            toolPointSprite[toolPointStd]->Draw();
                        }
                    }
                }
            }
        }

        if (IsItemId(mWorld->mId) && mWorld->DurabilityPointsItem(mWorld->mId) != -1)
        {
            if(mWorld->mAm != mWorld->DurabilityPointsItem(mWorld->mId))
            {
                unsigned int toolPointStd = roundf((float)(mWorld->mAm) / (float)(mWorld->DurabilityPointsItem(mWorld->mId)) * 13);
                if (upEn == 1)
                {
                    if (invEn == 1)
                    {
                        toolPointSprite[toolPointStd]->SetPosition(264+invXPosition*36,48+invYPosition*36);
                    }
                    if (craft3xEn == 1)
                    {
                        toolPointSprite[toolPointStd]->SetPosition(192+invXPosition*36,28+invYPosition*36);
                    }
                    if (chestEn == 1)
                    {
                        toolPointSprite[toolPointStd]->SetPosition(84+invXPosition*36,28+invYPosition*36);

                    }
                    if (furnaceEn == 1)
                    {
                        toolPointSprite[toolPointStd]->SetPosition(192,28+invYPosition*72);
                    }
                }
                else
                {
                    if(armorEn == false)
                    {
                        toolPointSprite[toolPointStd]->SetPosition(84+invXPosition*36,140+invYPosition*36);
                    }
                    else
                    {
                        toolPointSprite[toolPointStd]->SetPosition(84+invXPosition*108,28+invYPosition*72);
                    }
                }
                toolPointSprite[toolPointStd]->Draw();
            }
        }
    }

    DrawSetBlend(false);


	sceGuEnable(GU_DEPTH_TEST);
	sceGuDepthMask(0);

    //text
    if(mWorld->HP <= 0)
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        if(mRender->GetFontLanguage() == ENGLISH)
        {
            if(mWorld->gameModeWorld == 2)
            {
                mRender->SetFontStyle(default_big_size,GU_COLOR(0.25f,0.25f,0.25f,1),0,0x00000200);

                mRender->DebugPrint(242,52,"Game Over!");
                mRender->DebugPrint(242,152,"Press X to delete your world!");

                mRender->SetFontStyle(default_big_size,GU_COLOR(1,0,0,1),0,0x00000200);

                mRender->DebugPrint(240,50,"Game Over!");
                mRender->DebugPrint(240,150,"Press X to delete your world!");
            }
            else
            {
                mRender->SetFontStyle(default_big_size,GU_COLOR(0.25f,0.25f,0.25f,1),0,0x00000200);

                mRender->DebugPrint(242,52,"You Died!");
                mRender->DebugPrint(242,152,"Press X to respawn!");

                mRender->SetFontStyle(default_big_size,GU_COLOR(1,0,0,1),0,0x00000200);

                mRender->DebugPrint(240,50,"You Died!");
                mRender->DebugPrint(240,150,"Press X to respawn!");
            }
        }
    }

    if(sleepMessageTime > 0.0f)
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        if(sleepMessageType == 1)
        {
            if(mRender->GetFontLanguage() == ENGLISH)
            {
                mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(242,158,"You can only sleep at night");
            }
        }
        else
        {
            if(mRender->GetFontLanguage() == ENGLISH)
            {
                mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(242,158,"You may not rest now, there are monsters nearby");
            }
        }
    }

    sleepMessageTime -= dt;
    if(sleepMessageTime < 0.1f && sleepMessageTime > 0.0f)
    {
        sleepMessageTime = 0.0f;
    }

    mRender->SetFontStyle(default_size,0xFFFFFFFF,0,0x00000200);

    if((freeMemory/1024)/1024 <= 3 && mWorld->mainStatistics.minutesPlayed > 2)
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));
        mRender->SetFontStyle(default_size,GU_COLOR(1,0,0,1),0,0x00000000);
        //mRender->DebugPrint(5,15,"You have few free ram! Please, reduce the settings or restart the game!");
        mRender->SetFontStyle(default_size,0xFFFFFFFF,0,0x00000200);
    }

    mRender->SetFontStyle(default_size,0xFFFFFFFF,0xFF000000,0x00000200);

    if(invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true)
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int i = 0; i <= 3; i++)
        {
            for(int j = 0; j <= 8; j++)
            {
                if(mWorld->invId[i*9+j] != -1 && mWorld->invAm[i*9+j] > 1)
                {
                    if(IsItemId(mWorld->invId[i*9+j]))
                    {
                        if(mWorld->DurabilityPointsItem(mWorld->invId[i*9+j]) == -1)
                        {
                            DrawAmount((int)GuiAmtX(GuiSlotX(guiInvW, guiScale, 7.0f + j * 18.0f)), (int)GuiAmtY(GuiPlayerInvSlotY(guiInvH, guiScale, i)), mWorld->invAm[i*9+j]);
                        }
                    }
                    else
                    {
                        DrawAmount((int)GuiAmtX(GuiSlotX(guiInvW, guiScale, 7.0f + j * 18.0f)), (int)GuiAmtY(GuiPlayerInvSlotY(guiInvH, guiScale, i)), mWorld->invAm[i*9+j]);
                    }
                }
            }
        }

        if (mWorld->mId != -1)
        {
            bool canDraw = false;

            if(IsItemId(mWorld->mId))
            {
                if(mWorld->DurabilityPointsItem(mWorld->mId) == -1)
                {
                    canDraw = true;
                }
            }
            else
            {
                canDraw = true;
            }

            if(canDraw == true)
            {
                if (upEn == 0)
                {
                    if(armorEn == false)
                    {
                        DrawAmount(106+(invXPosition * 36),156+(invYPosition * 36),mWorld->mAm);
                    }
                    else
                    {
                        DrawAmount(106+(invXPosition * 108),44+(invYPosition * 72),mWorld->mAm);
                    }
                }
                else
                {
                    if(invEn == 1)
                    {
                        DrawAmount(286+(invXPosition * 36),64+(invYPosition * 36),mWorld->mAm);
                    }
                    if(craft3xEn == 1)
                    {
                        DrawAmount(214+(invXPosition * 36),44+(invYPosition * 36),mWorld->mAm);
                    }
                    if(chestEn == 1)
                    {
                        DrawAmount(106+(invXPosition * 36),44+(invYPosition * 36),mWorld->mAm);
                    }
                    if(furnaceEn == 1)
                    {
                        DrawAmount(214,44+(invYPosition * 72),mWorld->mAm);
                    }
                }
            }
        }

        sceGuEnable(GU_BLEND);
        selectInvSprite->Draw();
    }

    if((invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && menuState == 0 && mWorld->mainOptions.guiDrawing == 1) || (makeScreen == true && mWorld->mainOptions.guiDrawing == 1))
    {
        for(int k = 0; k <= 8; k++)
        {
            if (mWorld->invAm[27+k] > 1)
            {
                if(IsItemId(mWorld->invId[27+k])) // if it is item but not a tool
                {
                    if(mWorld->DurabilityPointsItem(mWorld->invId[27+k]) == -1)
                    {
                        DrawAmount(128+k*29,251,mWorld->invAm[27+k]);
                    }
                }
                else
                {
                    DrawAmount(128+k*29,251,mWorld->invAm[27+k]);
                }
            }
        }
    }

    if(invEn == true)
    {
        for(int i = 0; i <= 1; i++)
        {
            for(int j = 0; j <= 1; j++)
            {
                if(craftSlotId[i*2+j] != -1 && craftSlotAm[i*2+j] > 1)
                {
                    if(IsItemId(craftSlotId[i*2+j]))
                    {
                        if(mWorld->DurabilityPointsItem(craftSlotId[i*2+j]) == -1)
                        {
                            DrawAmount(286+j*36,72+i*36,craftSlotAm[i*2+j]);
                        }
                    }
                    else
                    {
                        DrawAmount(286+j*36,72+i*36,craftSlotAm[i*2+j]);
                    }
                }
            }
        }

        if(craftItemId != -1)
        {
            if(IsItemId(craftItemId))
            {
                if(mWorld->DurabilityPointsItem(craftItemId) == -1)
                {
                    DrawAmount(394,92,craftItemAm);
                }
            }
            else
            {
                DrawAmount(394,92,craftItemAm);
            }
        }
    }

    if(craft3xEn == true)
    {
        for(int i = 0; i <= 2; i++)
        {
            for(int j = 0; j <= 2; j++)
            {
                if(craftSlotId3[i*3+j] != -1 && craftSlotAm3[i*3+j] > 1)
                {
                    if(IsItemId(craftSlotId3[i*3+j]))
                    {
                        if(mWorld->DurabilityPointsItem(craftSlotId3[i*3+j]) == -1)
                        {
                            DrawAmount(214+j*36,52+i*36,craftSlotAm3[i*3+j]);
                        }
                    }
                    else
                    {
                        DrawAmount(214+j*36,52+i*36,craftSlotAm3[i*3+j]);
                    }
                }
            }
        }

        if(craftItemId3 != -1)
        {
            if(IsItemId(craftItemId3))
            {
                if(mWorld->DurabilityPointsItem(craftItemId3) == -1)
                {
                    DrawAmount(358,88,craftItemAm3);
                }
            }
            else
            {
                DrawAmount(358,88,craftItemAm3);
            }
        }
    }

    if(chestEn == true)
    {
        for(int i = 0; i <= 2; i++)
        {
            for(int j = 0; j <= 8; j++)
            {
                if(UseChest->chestSlotId[i*9+j] != -1 && UseChest->chestSlotAm[i*9+j] > 1)
                {
                    if(IsItemId(UseChest->chestSlotId[i*9+j]))
                    {
                        if(mWorld->DurabilityPointsItem(UseChest->chestSlotId[i*9+j]) == -1)
                        {
                            DrawAmount(106+j*36,52+i*36,UseChest->chestSlotAm[i*9+j]);
                        }
                    }
                    else
                    {
                        DrawAmount(106+j*36,52+i*36,UseChest->chestSlotAm[i*9+j]);
                    }
                }
            }
        }
    }

    if(furnaceEn == true)
    {
        if(UseFurnace->furnaceSlotId[0] != -1 && UseFurnace->furnaceSlotAm[0] > 1)
        {
            if(IsItemId(UseFurnace->furnaceSlotId[0]))
            {
                if(mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[0]) == -1)
                {
                    DrawAmount(214,52,UseFurnace->furnaceSlotAm[0]);
                }
            }
            else
            {
                 DrawAmount(214,52,UseFurnace->furnaceSlotAm[0]);
            }
        }
        if(UseFurnace->furnaceSlotId[1] != -1 && UseFurnace->furnaceSlotAm[1] > 1)
        {
            if(IsItemId(UseFurnace->furnaceSlotId[1]))
            {
                if(mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[1]) == -1)
                {
                    DrawAmount(214,124,UseFurnace->furnaceSlotAm[1]);
                }
            }
            else
            {
                DrawAmount(214,124,UseFurnace->furnaceSlotAm[1]);
            }
        }
        if(UseFurnace->furnaceSlotId[2] != -1 && UseFurnace->furnaceSlotAm[2] > 1)
        {
            DrawAmount(322,88,UseFurnace->furnaceSlotAm[2]);
        }
    }
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_DEPTH_TEST);

    if(makeScreen)
	{
		//end frame now to update frame buffer
		mRender->EndFrame();
		//make screenshot
		mRender->TakeNextScreenshot();
		makeScreen = false;
	}

    int language = mRender->GetFontLanguage();
    mRender->SetFontStyle(0.345f,0xFFFFFFFF,0,0x00000200);

    // Name of the item under the inventory cursor. The panels had no equivalent of the hotbar's name
    // label below, which made the spawn eggs a guessing game: they are sixteen near-identical 16x16
    // sprites, so telling one from another meant picking it up and putting it on the bar first.
    //
    // The cursor -> slot mapping is taken from the take/put code in HandleEvents, which is the only
    // place it is actually defined. upEn picks the upper panel (2x2 craft grid, 3x3 craft grid, chest
    // or furnace) and armorEn replaces the player grid with the 2x2 armour grid, and each of those
    // uses its own index formula -- *9 for the inventory and chest, *2 and *3 for the craft grids,
    // *2 + invXPosition for armour, and a bare invYPosition for the furnace. The two crafting OUTPUTS
    // are deliberately absent: they sit at x 151 while the cursor stops at 115, so it can never rest
    // on one (they are taken with a button press instead, whatever the cursor is over).
    if(invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true || armorEn == true)
    {
        int hoverId = -1;
        if(armorEn == true)
        {
            // The armour grid is 2x2, indexed down its rows (armorId[invYPosition*2 + invXPosition]).
            // An "empty" armour slot does not hold -1: it holds the id of the sample armour item for
            // that slot, which is what HandleEvents tests against (armorId[i] != 324+i). Translating
            // it here is the difference between the label naming the armour you are wearing and the
            // label naming a piece that is not there at all.
            int armorIdx = invYPosition * 2 + invXPosition;
            if(mWorld->armorId[armorIdx] != 324 + armorIdx)
            {
                hoverId = mWorld->armorId[armorIdx];
            }
        }
        else if(upEn == 1)
        {
            if(invEn == true)          hoverId = craftSlotId[invYPosition*2 + invXPosition];
            else if(craft3xEn == true) hoverId = craftSlotId3[invYPosition*3 + invXPosition];
            else if(chestEn == true)   hoverId = UseChest->chestSlotId[invYPosition*9 + invXPosition];
            else if(furnaceEn == true) hoverId = UseFurnace->furnaceSlotId[invYPosition];
        }
        else
        {
            hoverId = mWorld->invId[invYPosition*9 + invXPosition];
        }

        // The label describes the slot under the cursor; when that slot is empty it describes what
        // you are carrying instead, so "what did I just pick up?" is answerable in place too.
        if(hoverId == -1)
        {
            hoverId = mWorld->mId;
        }

        if(hoverId != -1)
        {
            std::string hoverName = mWorld->NameBlock(hoverId);
            if(hoverName != "null")   // NameBlock's answer for an id it does not know
            {
                mRender->SetFontStyle(0.5f,GU_COLOR(1,1,1,1),GU_COLOR(0.15f,0.15f,0.15f,1),0x00000200);
                float hoverNameW = (float)mRender->GetTextWidth(hoverName.c_str());
                float hoverX = GuiHoverLabelX(selectInvSprite->GetPositionX(),hoverNameW);
                float hoverY = GuiHoverLabelY(selectInvSprite->GetPositionY());
                mRender->DebugPrint((int)hoverX,(int)hoverY,"%s",hoverName.c_str());
                mRender->SetFontStyle(0.345f,0xFFFFFFFF,0,0x00000200);
            }
        }
    }

    if(invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && menuState == 0)
    {
        if(tickShowSlotName > 0.0f)
        {
            if(tickShowSlotName > 1.0f)
            {
                mRender->SetFontStyle(0.5f,GU_COLOR(1,1,1,1),GU_COLOR(0.15f,0.15f,0.15f,1),0x00000200);
            }
            else
            {
                mRender->SetFontStyle(0.5f,GU_COLOR(1,1,1,tickShowSlotName),GU_COLOR(0.15f,0.15f,0.15f,tickShowSlotName),0x00000200);
            }
            if(mWorld->invId[27+barPosition] != -1)
            {
                std::string slotName = mWorld->NameBlock(mWorld->invId[27+barPosition]);
                mRender->DebugPrint(240,185,"%s",slotName.c_str());
            }
        }

        inputDiskNameTimer -= dt;
        if(inputDiskNameTimer > 0.0f)
        {
            float red, blue, green, alpha;
            if(inputDiskNameTimer > 1.8f && inputDiskNameTimer <= 2.4f)
            {
                red = 1.0f;
                green = 1.0f - (2.4f-inputDiskNameTimer)/0.6f;
                blue = 0.0f;
                alpha = 1.0f;
            }
            if(inputDiskNameTimer > 1.2f && inputDiskNameTimer <= 1.8f)
            {
                red = 1.0f - (1.8f-inputDiskNameTimer)/0.6f;
                green = 0.0f;
                blue = (1.8f-inputDiskNameTimer)/0.6f;
                alpha = 1.0f;
            }
            if(inputDiskNameTimer > 0.6f && inputDiskNameTimer <= 1.2f)
            {
                red = 0.0f;
                green = (1.2f-inputDiskNameTimer)/0.6f;
                blue = 1.0f - (1.2f-inputDiskNameTimer)/0.6f;
                alpha = 1.0f;
            }
            if(inputDiskNameTimer > 0.0f && inputDiskNameTimer <= 0.6f)
            {
                red = (0.6f-inputDiskNameTimer)/0.6f;
                green = 1.0f;
                blue = 0.0f;
                alpha = 1.0f-(0.6f-inputDiskNameTimer)/0.6f;
            }
            if(alpha < 0.0f)
            {
                alpha = 0.0f;
            }

            mRender->SetFontStyle(0.5f,GU_COLOR(red,green,blue,alpha),999,0x00000200);

            mRender->SetFont(ENGLISH);
            mRender->DebugPrint(240,50,"%s",inputDiskName.c_str());
            mRender->SetDefaultFont();
        }

        if(tutorialTimer > -1.0f)
        {
            tutorialTimer -= dt;
        }
        if(tutorialTimer > 0.0f)
        {
            if(tutorialTimer > 9.0f && tutorialTimer <= 10.0f)
            {
                mRender->SetFontStyle(0.5f,GU_COLOR(1,1,1,10.0f-tutorialTimer),GU_COLOR(1,1,1,10.0f-tutorialTimer),0x00000000);
            }
            if(tutorialTimer > 1.0f && tutorialTimer <= 9.0f)
            {
                mRender->SetFontStyle(0.5f,GU_COLOR(1,1,1,1),GU_COLOR(1,1,1,1),0x00000000);
            }
            if(tutorialTimer <= 1.0f)
            {
                mRender->SetFontStyle(0.5f,GU_COLOR(1,1,1,tutorialTimer),GU_COLOR(1,1,1,tutorialTimer),0x00000000);
            }

            if(language == ENGLISH)
            {
                mRender->DebugPrint(5,40,"Tutorial");
                mRender->DebugPrint(5,57,"Press L+R to open inventory");
                mRender->DebugPrint(5,74,"Press L + Up to drop item");
            }
            else
            {
                mRender->DebugPrint(5,40,"Tutorial");
                mRender->DebugPrint(5,57,"Najmite L+R ytob@ otkr@t$ inventar$");
                mRender->DebugPrint(5,74,"Najmite L+vverh ytob@ v@brosit$ predmet");
            }
        }
        mRender->SetFontStyle(0.345f,0xFFFFFFFF,0,0x00000000);
    }

    //menu buttons
    if(menuState == 1)
    {
        if(menuOptions)
        {
            sceGuDisable(GU_DEPTH_TEST);
            sceGuEnable(GU_BLEND);
            sceGuColor(GU_COLOR(1,1,1,1.0f));

            nbuttonSprite->SetPosition(110,40);
            nbuttonSprite->Draw();

            nbuttonSprite->SetPosition(370,40);
            nbuttonSprite->Draw();

            nbuttonSprite->SetPosition(110,70);
            nbuttonSprite->Draw();

            nbuttonSprite->SetPosition(370,70);
            nbuttonSprite->Draw();

            buttonSprite->SetPosition(110,100);
            buttonSprite->Draw();

            buttonSprite->SetPosition(370,100);
            buttonSprite->Draw();

            buttonSprite->SetPosition(110,130);
            buttonSprite->Draw();

            buttonSprite->SetPosition(370,130);
            buttonSprite->Draw();

            buttonSprite->SetPosition(110,160);
            buttonSprite->Draw();

            buttonSprite->SetPosition(370,160);
            buttonSprite->Draw();

            buttonSprite->SetPosition(110,190);
            buttonSprite->Draw();

            buttonSprite->SetPosition(370,190);
            buttonSprite->Draw();

            buttonSprite->SetPosition(110,220);
            buttonSprite->Draw();

            buttonSprite->SetPosition(370,220);
            buttonSprite->Draw();

            buttonSprite->SetPosition(110,250);
            buttonSprite->Draw();

            buttonSprite->SetPosition(370,250);
            buttonSprite->Draw();

            if (optionsMenuPos < 8)
            {
                sbuttonSprite->SetPosition(110,(optionsMenuPos * 30) + 40);
            }
            if(optionsMenuPos >= 8)
            {
                sbuttonSprite->SetPosition(370,((optionsMenuPos-8) * 30) + 40);
            }
            sbuttonSprite->Draw();


            if(mWorld->mainOptions.fov == 40)
            {
                moverSprite->SetPosition(21,40);
            }
            else
            {
                moverSprite->SetPosition((mWorld->mainOptions.fov - 40)*3.0f+19,40);
            }
            moverSprite->Draw();

            if(mWorld->mainOptions.difficult != 3)
            {
                moverSprite->SetPosition((mWorld->mainOptions.difficult)*(190.0f/3.0f)+265+16,70);
            }
            else
            {
                moverSprite->SetPosition((mWorld->mainOptions.difficult)*(190.0f/3.0f)+269,70);
            }
            moverSprite->Draw();

            sceGuDisable(GU_BLEND);
            sceGuEnable(GU_DEPTH_TEST);

            //draw subtitles on buttons
            if(language == ENGLISH)
            {
                if(mWorld->mainOptions.fov == 65)
                {
                    optionsMenuPos == 0 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(110,49,"FOV: Normal");
                }
                else
                {
                    optionsMenuPos == 0 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(110,49,"FOV: %f",mWorld->mainOptions.fov);
                }

                if(mWorld->mainOptions.horizontalViewDistance == 1)
                {
                    optionsMenuPos == 1 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(110,79,"Render Distance: %i",mWorld->mainOptions.horizontalViewDistance);
                }
                else
                {
                    optionsMenuPos == 1 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(110,79,"Render Distance: %i",mWorld->mainOptions.horizontalViewDistance);
                }

                if(mWorld->mainOptions.verticalViewDistance == 1)
                {
                    optionsMenuPos == 8 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(370,49,"Verticle Render: %i ch",mWorld->mainOptions.verticalViewDistance);
                }
                else
                {
                    optionsMenuPos == 8 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(370,49,"Verticle Render: %i chs",mWorld->mainOptions.verticalViewDistance);
                }


                switch(mWorld->mainOptions.difficult)
                {
                case 0:
                    optionsMenuPos == 9 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(370,79,"Difficult: Peaceful");
                break;
                case 1:
                    optionsMenuPos == 9 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(370,79,"Difficult: Easy");
                break;
                case 2:
                    optionsMenuPos == 9 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(370,79,"Difficult: Normal");
                break;
                case 3:
                    optionsMenuPos == 9 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                    mRender->DebugPrint(370,79,"Difficult: Hard");
                break;
                }


				optionsMenuPos == 2 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.fogRendering == true ? mRender->DebugPrint(110,109,"Fog Rendering: ON"): mRender->DebugPrint(110,109,"Fog Rendering: OFF");

                optionsMenuPos == 3 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.detailedSky == true ? mRender->DebugPrint(110,139,"Beautiful Skies: ON"): mRender->DebugPrint(110,139,"Beautiful Skies: OFF");

                optionsMenuPos == 4 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.mipMapTexturing == true ? mRender->DebugPrint(110,169,"Fancy Graphics"): mRender->DebugPrint(110,169,"Fancy Graphics");

                optionsMenuPos == 5 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.smoothLighting == true ? mRender->DebugPrint(110,199,"Smooth Lighting: ON"): mRender->DebugPrint(110,199,"Smooth Lighting: OFF");

                optionsMenuPos == 6 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.worldBlockAnimation == true ? mRender->DebugPrint(110,229,"Animated Blocks: ON"): mRender->DebugPrint(110,229,"Animated Blocks: OFF");

                optionsMenuPos == 7 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.autoJump == true ? mRender->DebugPrint(110,259,"Auto jump: ON"): mRender->DebugPrint(110,259,"Auto jump: OFF");

                optionsMenuPos == 10 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.freezeDayTime == true ? mRender->DebugPrint(370,109,"Do Day Cycle: OFF"): mRender->DebugPrint(370,109,"Do Day Cycle: ON");

                optionsMenuPos == 11 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.guiDrawing == true ? mRender->DebugPrint(370,139,"Hide Hud: OFF"): mRender->DebugPrint(370,139,"Hide Hud: ON");

                optionsMenuPos == 12 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.sounds == true ? mRender->DebugPrint(370,169,"Sound: ON"): mRender->DebugPrint(370,169,"Sound: OFF");

                optionsMenuPos == 13 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mWorld->mainOptions.music == true ? mRender->DebugPrint(370,199,"Music : ON") : mRender->DebugPrint(370,199,"Music : OFF");

                optionsMenuPos == 14 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                mRender->DebugPrint(370,229,"Take Screenshot");

                optionsMenuPos == 15 ? RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,0,1),0,0x00004000|0x00000200) : RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
                devMode == true ? mRender->DebugPrint(370,259,"Show Coordinates: ON") : mRender->DebugPrint(370,259,"Show Coordinates: OFF");

                DrawText(240,24,GU_COLOR(1,1,1,1),default_size,"Options");

                if(PauseOptionsIsSlider(optionsMenuPos))
                {
                    int sliderX = (optionsMenuPos < 8) ? 110 : 370;
                    int sliderY = ((optionsMenuPos < 8) ? optionsMenuPos : (optionsMenuPos - 8)) * 30 + 49;
                    DrawText(sliderX - 98, sliderY, GU_COLOR(1,1,0,1), default_size, "L");
                    DrawText(sliderX + 98, sliderY, GU_COLOR(1,1,0,1), default_size, "R");
                }
            }
        }
        else
        {
            sceGuDisable(GU_DEPTH_TEST);
            sceGuEnable(GU_BLEND);
            sceGuColor(GU_COLOR(1,1,1,1.0f));

			//Resume Game
            buttonSprite->SetPosition(240,70);
            buttonSprite->Draw();

            //Help & Options
            buttonSprite->SetPosition(240,100);
            buttonSprite->Draw();

            //Leaderboards
            buttonSprite->SetPosition(240,130);
            buttonSprite->Draw();

            //Exit Game
            buttonSprite->SetPosition(240,160);
            buttonSprite->Draw();
			
			//Save & Exit Game
			buttonSprite->SetPosition(240,190);
            buttonSprite->Draw();
			
			//Exit Game
			buttonSprite->SetPosition(240,220);
            buttonSprite->Draw();

			//selected button
			sbuttonSprite->SetPosition(240,(selectPos * 30) + 70);
            sbuttonSprite->Draw();

            sceGuDisable(GU_BLEND);
            sceGuEnable(GU_DEPTH_TEST);

            mRender->SetFontStyle(default_size ,GU_COLOR(1,1,1,1),0,0x00000000|0x00004000);
            if(language == ENGLISH)
            {
                selectPos == 0 ? DrawText(240,79,GU_COLOR(1,1,0,1),default_size,"Resume Game") : DrawText(240,79,GU_COLOR(1,1,1,1),default_size,"Resume Game");
                selectPos == 1 ? DrawText(240,109,GU_COLOR(1,1,0,1),default_size,"Help & Options") : DrawText(240,109,GU_COLOR(1,1,1,1),default_size,"Help & Options");
                selectPos == 2 ? DrawText(240,139,GU_COLOR(1,1,0,1),default_size,"Leaderboards") : DrawText(240,139,GU_COLOR(1,1,1,1),default_size,"Leaderboards");
                selectPos == 3 ? DrawText(240,169,GU_COLOR(1,1,0,1),default_size,"Save Game") : DrawText(240,169,GU_COLOR(1,1,1,1),default_size,"Save Game");
				selectPos == 4 ? DrawText(240,199,GU_COLOR(1,1,0,1),default_size,"Save & Exit Game") : DrawText(240,199,GU_COLOR(1,1,1,1),default_size,"Save & Exit Game");
				selectPos == 5 ? DrawText(240,229,GU_COLOR(1,1,0,1),default_size,"Exit Game") : DrawText(240,229,GU_COLOR(1,1,1,1),default_size,"Exit Game");
            }
        }
    }
    if (menuState == 3)
    {
        if(language == ENGLISH)
        {
            if(statisticsPage == 0)
            {
                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,54,"Blocks Placed: %i",mWorld->mainStatistics.blockPlaced);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(0.8,0.8,0.8,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,74,"Blocks Destroyed: %i",mWorld->mainStatistics.blockDestroyed);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,94,"Age Of The World: %i d",mWorld->mainStatistics.daysInGame);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(0.8,0.8,0.8,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,114,"Minutes Played: %i m",mWorld->mainStatistics.minutesPlayed);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,134,"Items Crafted: %i",mWorld->mainStatistics.itemsCrafted);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(0.8,0.8,0.8,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,154,"Items Smelted: %i",mWorld->mainStatistics.itemsSmelted);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,174,"Soil Plowed: %i",mWorld->mainStatistics.soilPlowed);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(0.8,0.8,0.8,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,194,"Crops Grown: %i",mWorld->mainStatistics.cropsGrowned);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,214,"Trees Grown: %i",mWorld->mainStatistics.treesGrowned);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(0.8,0.8,0.8,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,234,"Damage Dealt: %i",mWorld->mainStatistics.damageRecieved);
            }
            else
            {
                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,54,"Bad falls: %i",mWorld->mainStatistics.badlyFalls);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(0.8,0.8,0.8,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,74,"Food eaten: %i",mWorld->mainStatistics.foodEaten);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,94,"Jumps: %i",mWorld->mainStatistics.jumps);

                RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(0.8,0.8,0.8,1),0,0x00004000|0x00000000);
                RenderManager::InstancePtr()->DebugPrint(40,114,"Deaths: %i",mWorld->mainStatistics.dies);
            }
            DrawText(240,24,GU_COLOR(1,1,1,1),default_size,"Leaderboards");

            RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000200);
            RenderManager::InstancePtr()->DebugPrint(240,254,"Page: %i / 2",statisticsPage+1);
        }
    }

    //debug info
    if(devMode)
    {
        mRender->SetFontStyle(default_size,0xFFFFFFFF,0,0x00000000);

        if(dt > 0.0f)
        {
            tick_fps += dt;
            if(tick_fps > dt/500.0f) // each 2 ms
            {
                pre_fps += (1.0f/dt);
                tick_fps -= dt/500.0f;
                ticks ++;
            }

            if(ticks == 50)
            {
                average_fps = pre_fps/ticks;
                tick_fps = 0;
                pre_fps = 0;
                ticks = 0;
            }
        }
        //mRender->DebugPrint(20,24,"fps: %d",average_fps+1);
        //mRender->DebugPrint(20,34,"cpu: %d%%",mRender->GetCpuUsage());
        //mRender->DebugPrint(20,44,"gpu: %d%%",mRender->GetGpuUsage());
        //mRender->DebugPrint(20,54,"world time: %f",mWorld->worldDayTime);

        mRender->DebugPrint(15,20,"Position: %f,%f,%f",playerPosition.x,playerPosition.y,playerPosition.z);
    }

    if(menuState == 0)
        mChat.Draw(mRender);

    // The purple wash while a portal pulls the player through.
    //
    // Drawn as a screen-space quad rather than with Sprite. Sprite works in GU_TRANSFORM_3D, so it goes
    // through the model matrix, while everything else on this screen -- the hotbar, the xp bar, the
    // chat, the debug dim -- is 2D and bypasses it. A 2D quad also lets the 16x16 texture be stretched
    // over the whole screen by giving the vertices the full screen rectangle and the texels only 0..16.
    // advancedBlit cannot express that (it maps texels onto pixels one to one, which is why the xp bar
    // has to be a native 480x272 image), and a Sprite would need a 30x17 scale that the model matrix
    // then displaces.
    //
    // The blend function is set here rather than trusting whatever the world pass left behind: the
    // transparent chunk pass and parts of the HUD switch between additive, inverted and straight alpha,
    // and depth test is turned off here so the wash covers the world instead of being culled by it.
    const float portalOverlay = mWorld->GetPortalOverlayAlpha();
    if(portalOverlay > 0.0f)
    {
        // Diagnostic, so a run can prove the wash was both reached and drawn instead of the question
        // being settled from a screenshot. Logged on the crossing of half, and re-armed below a fifth so
        // a second portal logs again -- otherwise this is 120 lines per transition.
        static bool sLoggedPortalWash = false;
        if(portalOverlay > 0.5f && sLoggedPortalWash == false)
        {
            MC_DEBUG_LOG("portal wash reached alpha=%.2f\n", portalOverlay);
            sLoggedPortalWash = true;
        }
        else if(portalOverlay < 0.2f)
        {
            sLoggedPortalWash = false;
        }

        if(portalFadeTex < 0)
            portalFadeTex = TextureHelper::Instance()->GetTexture(TextureHelper::PortalFade);

        if(portalFadeTex >= 0)
        {
            // Texturing is not switched on by SetTextureModeulate, only bound, and the HUD leaves it off
            // in places -- the xp bar's full-screen blit turns it back off immediately after itself.
            // Without this the quad modulates by nothing and reads as a white wash instead of purple.
            DrawPlaceTexture(true);
            TextureManager::Instance()->SetTextureModeulate(portalFadeTex);

            sceGuDisable(GU_DEPTH_TEST);
            sceGuEnable(GU_BLEND);
            sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
            sceGuColor(GU_COLOR(1, 1, 1, portalOverlay));

            // GU_SPRITES with two vertices, which is the call advancedBlit uses for every 2D blit in the
            // HUD and the one the xp bar fills the screen with, rather than a four-vertex triangle strip.
            // The sprite is the screen rectangle and its corners carry texels 0 and 16, which is how the
            // whole 16x16 texture is stretched across 480x272.
            TexturesPSPVertex16* quad = (TexturesPSPVertex16*)sceGuGetMemory(2 * sizeof(TexturesPSPVertex16));
            quad[0].u = 0;  quad[0].v = 0;  quad[0].x = 0;         quad[0].y = 0;         quad[0].z = 0;
            quad[1].u = 16; quad[1].v = 16; quad[1].x = SCR_WIDTH; quad[1].y = SCR_HEIGHT; quad[1].z = 0;
            sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, quad);
        }
    }

    // The debug stick's menu, last so it sits over the world, the HUD and the chat. It handles its
    // own depth-test and blend state, the same way the pause menu's button block does.
    debugMenu.Render(mRender);

    //end frame
    mRender->EndFrame();
}

//additional functions
void StatePlay::advancedBlit(int sx, int sy, int sw, int sh, int dx, int dy, int slice)
{
    int start, end;

    // blit maximizing the use of the texture-cache

    for (start = sx, end = sx+sw; start < end; start += slice, dx += slice)
    {
        TexturesPSPVertex16* vertices = (TexturesPSPVertex16*)sceGuGetMemory(2 * sizeof(TexturesPSPVertex16));
        int width = (start + slice) < end ? slice : end-start;

        vertices[0].u = start;
        vertices[0].v = sy;
        vertices[0].x = dx;
        vertices[0].y = dy;
        vertices[0].z = 0;

        vertices[1].u = start + width;
        vertices[1].v = sy + sh;
        vertices[1].x = dx + width;
        vertices[1].y = dy + sh;
        vertices[1].z = 0;

        sceGuDrawArray(GU_SPRITES,GU_TEXTURE_16BIT|GU_VERTEX_16BIT|GU_TRANSFORM_2D,2,0,vertices);
    }
}

bool StatePlay::TryToMove(Vector3 moveVector,float dt)
{
    // Build a "test vector" that is a little longer than the move vector.
    float moveLength = moveVector.magnitude();
    Vector3 testVector = moveVector;
    testVector.normalize();
    testVector = testVector * (moveLength + 0.2f);

    // Apply this test vector.
    Vector3 movePosition = playerPosition + testVector;
    Vector3 midBodyPoint = movePosition + Vector3(0, -mWorld->playerGrowth+1.1f, 0);
    Vector3 midLowBodyPoint = movePosition + Vector3(0, -mWorld->playerGrowth+0.6f, 0);
    Vector3 lowerBodyPoint = movePosition + Vector3(0, -mWorld->playerGrowth+0.1f, 0);

    float slowDown = 1.0f;
    if(footInLava || footInWater)
    {
        slowDown = 0.89f;
    }
    if(headInLava || headInWater)
    {
        slowDown = 0.66f;
    }

    if (!mWorld->SolidAtPointForPlayer(movePosition) && !mWorld->SolidAtPointForPlayer(lowerBodyPoint) && !mWorld->SolidAtPointForPlayer(midBodyPoint))
    {
        Vector3 dest = playerPosition + moveVector*slowDown;
        if(sneaking && walkingOnGround && (moveVector.x != 0.0f || moveVector.z != 0.0f))
        {
            Vector3 destFoot = dest + Vector3(0.0f, -mWorld->playerGrowth - 0.05f, 0.0f);
            Vector3 curFoot = playerPosition + Vector3(0.0f, -mWorld->playerGrowth - 0.05f, 0.0f);
            if(mWorld->SolidAtPointForPlayer(curFoot) && !mWorld->SolidAtPointForPlayer(destFoot))
                return false;
        }
        playerPosition = dest;

        return true;
    }

    //teraz mo¿na robiæ ciekawe rzeczy

    testVector = moveVector;
    testVector.normalize();
    testVector = testVector * (moveLength + 0.42f);
    movePosition = playerPosition + testVector;
    midBodyPoint = movePosition + Vector3(0, -mWorld->playerGrowth+1.1f, 0);
    midLowBodyPoint = movePosition + Vector3(0, -mWorld->playerGrowth+0.6f, 0);
    lowerBodyPoint = movePosition + Vector3(0, -mWorld->playerGrowth+0.1f, 0);

    Vector3 footPosition = playerPosition + Vector3(0.0f, -mWorld->playerGrowth, 0.0f);

    block_t lowerBlock = mWorld->BlockAtPoint(lowerBodyPoint);
    block_t midLowBlock = mWorld->BlockAtPoint(midLowBodyPoint);
    block_t midBlock = mWorld->BlockAtPoint(midBodyPoint);
    block_t upperBlock = mWorld->BlockAtPoint(movePosition);


    if ((mWorld->blockTypes[lowerBlock].blockModel == 1 || (mWorld->blockTypes[lowerBlock].blockModel >= 6 && mWorld->blockTypes[lowerBlock].blockModel <= 9)) &&
        mWorld->blockTypes[midLowBlock].solid == false &&
        mWorld->blockTypes[midBlock].solid == false &&
        mWorld->blockTypes[upperBlock].solid == false)
    {
        playerVelocity.y = CLIMBVELOCITY*1.29f;
        if (mWorld->SolidAtPointForPlayer(footPosition))
            playerPosition.y += 0.13f;
        return true;
    }

    block_t footBlock = mWorld->GetBlock(footPosition.x,footPosition.y,footPosition.z);
    if (mWorld->blockTypes[footBlock].blockModel == 1 &&
        mWorld->blockTypes[lowerBlock].solid == true &&
        mWorld->blockTypes[midLowBlock].solid == false &&
        mWorld->blockTypes[midBlock].solid == false &&
        mWorld->blockTypes[upperBlock].solid == false)
    {
        playerVelocity.y = CLIMBVELOCITY*1.29f;
        if(mWorld->SolidAtPointForPlayer(footPosition))
            playerPosition.y += 0.13f;
        return true;
    }

    int cameraAngle = (int)fppCam->horAngle;
    if(moveForward)
    {
        if(mWorld->GetBlock(playerPosition.x,playerPosition.y-1.55f,playerPosition.z) == Ladder1::getID())
        {
            if(cameraAngle > 125 && cameraAngle < 235)
            {
                if (upperBlock != 0 || lowerBlock != 0 || midBlock != 0)
                {
                    playerVelocity.y = CLIMBVELOCITY;
                    if (mWorld->SolidAtPointForPlayer(footPosition))
                        playerPosition.y += 0.1f;
                    return true;
                }
            }
        }

        if(mWorld->GetBlock(playerPosition.x,playerPosition.y-1.55f,playerPosition.z) == Ladder2::getID())
        {
            if(cameraAngle < 55 || cameraAngle > 305)
            {
                if (upperBlock != 0 || lowerBlock != 0 || midBlock != 0)
                {
                    playerVelocity.y = CLIMBVELOCITY;
                    if (mWorld->SolidAtPointForPlayer(footPosition))
                        playerPosition.y += 0.1f;
                    return true;
                }
            }
        }

        if(mWorld->GetBlock(playerPosition.x,playerPosition.y-1.55f,playerPosition.z) == Ladder3::getID())
        {
            if(cameraAngle > 215 && cameraAngle < 325)
            {
                if (upperBlock != 0 || lowerBlock != 0 || midBlock != 0)
                {
                    playerVelocity.y = CLIMBVELOCITY;
                    if (mWorld->SolidAtPointForPlayer(footPosition))
                        playerPosition.y += 0.1f;
                    return true;
                }
            }
        }

        if(mWorld->GetBlock(playerPosition.x,playerPosition.y-1.55f,playerPosition.z) == Ladder4::getID())
        {
            if(cameraAngle > 35 && cameraAngle < 145)
            {
                if (upperBlock != 0 || lowerBlock != 0 || midBlock != 0)
                {
                    playerVelocity.y = CLIMBVELOCITY;
                    if (mWorld->SolidAtPointForPlayer(footPosition))
                        playerPosition.y += 0.1f;
                    return true;
                }
            }
        }
    }

    return false;
}

bool StatePlay::keyPressed(int currentKey)
{
    //analog reset
    if(analogUp)
    {
        if(mSystemMgr->GetAnalogY() < InputHelper::Instance()->analogYup)
            analogUp = false;
    }
    if(analogDown)
    {
        if(mSystemMgr->GetAnalogY() > InputHelper::Instance()->analogYdown)
            analogDown = false;
    }
    if(analogLeft)
    {
        if(mSystemMgr->GetAnalogX() > InputHelper::Instance()->analogXleft)
            analogLeft = false;
    }
    if(analogRight)
    {
        if(mSystemMgr->GetAnalogX() < InputHelper::Instance()->analogXright)
            analogRight = false;
    }

    //keys
    if(currentKey == 0)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_UP);
    if(currentKey == 1)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_DOWN);
    if(currentKey == 2)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_LEFT);
    if(currentKey == 3)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_RIGHT);

    if(currentKey == 4)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_TRIANGLE);
    if(currentKey == 5)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_CROSS);
    if(currentKey == 6)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_SQUARE);
    if(currentKey == 7)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE);

    if(currentKey == 8)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_LTRIGGER);
    if(currentKey == 9)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_RTRIGGER);
    if(currentKey == 10)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_SELECT);
    if(currentKey == 15)//cross
        return mSystemMgr->KeyPressed(PSP_CTRL_START);

    //analog stick....
    if(currentKey == 12)//cross
    {
        if(!analogUp)
        {
            if(mSystemMgr->GetAnalogY() > InputHelper::Instance()->analogYup)
            {
                analogUp = true;
                return true;//analog up
            }
        }
    }
    if(currentKey == 11)//cross
    {
        if(!analogDown)
        {
            if(mSystemMgr->GetAnalogY() < InputHelper::Instance()->analogYdown)
            {
                analogDown = true;
                return true;//analog down
            }
        }
    }
    if(currentKey == 13)//cross
    {
        if(!analogLeft)
        {
            if(mSystemMgr->GetAnalogX() < InputHelper::Instance()->analogXleft)
            {
                analogLeft = true;
                return true;//analog left
            }
        }
    }
    if(currentKey == 14)//cross
    {
        if(!analogRight)
        {
            if(mSystemMgr->GetAnalogX() > InputHelper::Instance()->analogXright)
            {
                analogRight = true;
                return true;//analog right
            }
        }
    }

    return false;
}

#if MC_AUTO_WALK
// ---------------------------------------------------------------------------------------------
// Development-only auto-walk -- see MC_AUTO_WALK in DebugConfig.h for why it exists.
//
// StatePlay::keyHold turns the analog into the four direction booleans the movement code consumes
// (keys 11-14; the connection table in InputHelper.cpp binds Forward/Back/Left/Right to analog
// up/down/left/right). Returning the probe's direction from that ONE place means the walk is driven by
// exactly the code a human drives. It deliberately does not write the player's position: bypassing the
// movement path would skip collision, the camera-relative direction and the speed ramp, i.e. most of
// what is being measured.
//
// The path is a patrol, one leg per direction, so the player keeps crossing ground it has not loaded,
// which is the case the streamer and the mesher are measured on. A straight line runs into the first
// hill and stops; a circle comes back round to ground already resident.
//
// Eight headings rather than four, and a stuck check, because the overworld's square does not survive
// the nether. Measured on the nether probe: 174 of 178 seconds were spent standing still. A cave there
// is a corridor of netherrack, so a leg pointing at a wall is a leg that never happens, and whichever
// direction the square asked for next was the same wall twice more often than not. Turning alone does
// not fix it either -- the legs were long enough that one blocked leg was most of the run.
//
// So: shorter legs, and the heading moves on by one when the player stops making progress. Nothing here
// teleports the player or reads the terrain; a blocked path is detected only by the fact that the
// player's own collision did not let them move, which is the same signal a human would act on.
namespace autowalk
{
    const float kSettleSecs = 8.0f;    // let the world finish its first mesh before walking into it
    const float kLegSecs    = 12.0f;   // one leg of the patrol
    const float kStuckSecs  = 2.5f;    // this long without progress means the way ahead is blocked
    const float kStuckDist  = 0.75f;   // horizontal blocks that count as real progress over that window

    float t = 0.0f;
    int   heading = 0;                 // added to the leg index, so a turn takes effect immediately
    float sinceCheck = 0.0f;
    float lastX = 0.0f;
    float lastZ = 0.0f;
    bool  haveLast = false;
    float logTimer = 0.0f;
    int   stuckCount = 0;
    // Per-second counts of how often keyHold was actually ASKED for each direction by the movement
    // code. This is what separates the two ways a walk can stand still: the request never reaches the
    // movement code (nothing asks), or it reaches it and the world refuses (something asks, the player
    // does not move). The camera velocity in the log tells the same two apart from the other side.
    int   ask[4] = { 0, 0, 0, 0 };      // forward, back, left, right

    // Heading 0..7 clockwise from forward. The movement code reads forward/back and left/right as
    // separate booleans and combines them, so a diagonal is simply two of them set -- which is also
    // why the diagonals are reachable here at all (see the analog block in keyHold).
    const bool kDir[8][4] = {
        // fwd    back   left   right
        { true,  false, false, false },   // 0  forward
        { true,  false, false, true  },   // 1  forward-right
        { false, false, false, true  },   // 2  right
        { false, true,  false, true  },   // 3  back-right
        { false, true,  false, false },   // 4  back
        { false, true,  true,  false },   // 5  back-left
        { false, false, true,  false },   // 6  left
        { true,  false, true,  false },   // 7  forward-left
    };
}

// Which directions the probe is asking for right now.
static void AutoWalkDirections(bool &fwd, bool &back, bool &left, bool &right)
{
    fwd = back = left = right = false;
    if(autowalk::t < autowalk::kSettleSecs)
        return;
    const float leg = autowalk::t - autowalk::kSettleSecs;
    const int legIndex = (int)(leg / autowalk::kLegSecs);
    const int h = (legIndex + autowalk::heading) & 7;
    fwd   = autowalk::kDir[h][0];
    back  = autowalk::kDir[h][1];
    left  = autowalk::kDir[h][2];
    right = autowalk::kDir[h][3];
}

// Called once a frame from Update, NOT from keyHold: keyHold is called once per action per frame, so a
// clock advanced in there would run several times faster than real time.
static void AutoWalkTick(float dt)
{
    autowalk::t += dt;
}

// Called once a frame from Update with the player's feet, which keyHold has no way to supply. Bumps the
// heading when a whole window passes without the player actually going anywhere -- see the namespace
// comment for why the nether needs it.
static void AutoWalkProgress(float dt, float x, float y, float z, float vx, float vz, float hp)
{
    if(autowalk::t < autowalk::kSettleSecs)
        return;
    if(!autowalk::haveLast)
    {
        autowalk::haveLast = true;
        autowalk::lastX = x;
        autowalk::lastZ = z;
        return;
    }
    autowalk::sinceCheck += dt;

    // Once a second, where the player actually is. This is here because `mesh/s` cannot answer "did the
    // walk move": a cave pocket is fully meshed within a few seconds, so a player walking around inside
    // one produces no meshing at all and the perf line reads as idle while the walk is working fine.
    // Only the position settles whether a leg is blocked.
    autowalk::logTimer += dt;
    if(autowalk::logTimer >= 1.0f)
    {
        autowalk::logTimer -= 1.0f;
        // asked = keyHold was consulted that many times this second for fwd/back/left/right; vel = the
        // camera's own horizontal velocity, so a non-zero component with a static position means the
        // input arrived and collision is refusing the move.
        Aurora::Utils::Logger::Instance()->LogMessage(
            "walk t=%.1f pos=%.2f,%.2f,%.2f hp=%.1f heading=%d turns=%d asked=%d/%d/%d/%d vel=%.3f,%.3f\n",
            autowalk::t, x, y, z, hp, autowalk::heading, autowalk::stuckCount,
            autowalk::ask[0], autowalk::ask[1], autowalk::ask[2], autowalk::ask[3], vx, vz);
        autowalk::ask[0] = autowalk::ask[1] = autowalk::ask[2] = autowalk::ask[3] = 0;
    }

    if(autowalk::sinceCheck < autowalk::kStuckSecs)
        return;

    const float dx = x - autowalk::lastX;
    const float dz = z - autowalk::lastZ;
    const float dist = sqrtf(dx * dx + dz * dz);
    autowalk::sinceCheck = 0.0f;
    autowalk::lastX = x;
    autowalk::lastZ = z;
    if(dist < autowalk::kStuckDist)
    {
        autowalk::heading = (autowalk::heading + 1) & 7;
        autowalk::stuckCount++;
    }
}
#endif

bool StatePlay::keyHold(int currentKey)
{
    if(currentKey == 0)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_UP);
    if(currentKey == 1)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_DOWN);
    if(currentKey == 2)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_LEFT);
    if(currentKey == 3)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_RIGHT);

    if(currentKey == 4)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_TRIANGLE);
    if(currentKey == 5)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_CROSS);
    if(currentKey == 6)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_SQUARE);
    if(currentKey == 7)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_CIRCLE);

    if(currentKey == 8)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_LTRIGGER);
    if(currentKey == 9)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_RTRIGGER);
    if(currentKey == 10)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_SELECT);
    if(currentKey == 15)//cross
        return mSystemMgr->KeyHold(PSP_CTRL_START);

    //analog stick....
    //
    // One radial deadzone for the whole stick, then an 8-way test on the components, rather than
    // four independent per-axis thresholds. A per-axis test cannot express a diagonal: it made a
    // 45-degree push clear BOTH axis thresholds at once, which needs sqrt(2) more throw than a
    // straight push, so once the sensitivity setting was high enough a diagonal fell between the
    // two and registered nothing -- which is exactly how it presented (forward/back/left/right
    // fine, every diagonal dead). Here the stick clears the same distance in every direction, and
    // a second direction counts when its component is at least kAnalogDiagonalShare of the larger
    // one, so a straight push claims one way and a diagonal claims two. The movement code reads
    // forward/back and left/right as separate booleans and combines them, so that is all it needs.
    if(currentKey >= 11 && currentKey <= 14)
    {
#if MC_AUTO_WALK
        // Development-only auto-walk. Falls through to the real stick while the probe is not walking
        // yet, so the pad still works in a probe build and the settle window is honest.
        {
            bool fwd, back, left, right;
            AutoWalkDirections(fwd, back, left, right);
            if(fwd || back || left || right)
            {
                if(currentKey == 11) { autowalk::ask[0]++; return fwd; }      // analog up    -- Forward
                if(currentKey == 12) { autowalk::ask[1]++; return back; }     // analog down  -- Back
                if(currentKey == 13) { autowalk::ask[2]++; return left; }     // analog left  -- Left
                autowalk::ask[3]++; return right;                             // analog right -- Right
            }
        }
#endif
        float ax = mSystemMgr->GetAnalogX();
        float ay = mSystemMgr->GetAnalogY();
        float mag = sqrtf(ax * ax + ay * ay);

        // The four per-axis settings (0.1 .. 0.9, from Settings) average to the single radius a
        // radial deadzone is defined by; they are kept symmetric in practice, and a radial gate is
        // symmetric by construction. Two are stored negative, hence the sum rather than a min.
        float dead = (InputHelper::Instance()->analogYup - InputHelper::Instance()->analogYdown
                    + InputHelper::Instance()->analogXright - InputHelper::Instance()->analogXleft) * 0.25f;
        if(dead < 0.0f)
            dead = -dead;
        if(mag < dead)
            return false;

        const float kAnalogDiagonalShare = 0.5f;   // a component this much of the larger axis is a second way
        float axAbs = fabsf(ax);
        float ayAbs = fabsf(ay);
        float big = (axAbs > ayAbs) ? axAbs : ayAbs;

        if(currentKey == 11)   //analog up (action Forward is bound here on this build)
            return (ay < 0.0f && ayAbs >= big * kAnalogDiagonalShare);
        if(currentKey == 12)   //analog down (action Back)
            return (ay > 0.0f && ayAbs >= big * kAnalogDiagonalShare);
        if(currentKey == 13)   //analog left (action Left)
            return (ax < 0.0f && axAbs >= big * kAnalogDiagonalShare);
        return (ax > 0.0f && axAbs >= big * kAnalogDiagonalShare);   //analog right (action Right)
    }

    return false;
}


void StatePlay::HungerTime()
{
    if(mWorld->HG >= 16)
    {
        if (rand() % 4 == 0)
        {
            mWorld->HG -= 1;
        }
    }
    else
    {
        mWorld->HG -= 1;
    }
    if(mWorld->HG < 0)
    {
        mWorld->HG = 0;
    }
}

void StatePlay::HealthTime()
{
    if(mWorld->HG <= 0 && mWorld->HP > 0)
    {
        if(mWorld->mainOptions.difficult == 0)
        {
            if(mWorld->HP > 1)
            {
                if(mWorld->HP == 2)
                {
                    HurtPlayer(1);
                }
                else
                {
                    HurtPlayer(2);
                }
            }
        }
        else
        {
            HurtPlayer(2);
        }
    }

    if(mWorld->HG >= 18)
    {
        if(mWorld->HP > 0)
        {
            mWorld->HP += 1;
        }
    }

    if(mWorld->HP > 20)
    {
        mWorld->HP = 20;
    }
}

void StatePlay::OxygenTime()
{
    if(mWorld->HP > 0)
    {
        if(mWorld->OS <= 0)
        {
            HurtPlayer(2);
        }
        if(mWorld->OS > 0)
        {
            mWorld->OS -= 1;
        }
    }
}

void StatePlay::HurtPlayer(float damage)
{
    mSoundMgr->PlayHitSound();
    last_HP = mWorld->HP;
    mWorld->HP -= damage;
    mWorld->mainStatistics.damageRecieved += damage;
    // Java: EntityPlayer.damageEntity -> addExhaustion(0.1F). Taking a beating is hungry work.
    mWorld->AddExhaustion(EXHAUST_DAMAGE);

    if(mWorld->HP > 0)
    {
        hurt = true;
        hurt_time = 1.0f;
        hpAnim = 0.0f;
    }
}

void StatePlay::HurtPlayerConsiderArmor(float damage)
{
    mSoundMgr->PlayHitSound();
    last_HP = mWorld->HP;
    mWorld->HP -= damage*(1.0f-(mWorld->AP/25.0f));
    mWorld->mainStatistics.damageRecieved += damage*(1.0f-(mWorld->AP/25.0f));

    if(mWorld->HP > 0)
    {
        hurt = true;
        hurt_time = 1.0f;
        hpAnim = 0.0f;
    }
}

// The bow's release. bowCharge is 0..1 (one second of draw) and vanilla bends it, not uses it
// straight: f = (c^2 + 2c) / 3, so half a second gives 0.42 of full speed and the last fifth of the
// draw is worth almost nothing. SetVeloc already applies the same power to the arrow's drop, which
// is what makes a tapped shot fall short of a held one. Vanilla also refuses a shot drawn for less
// than two ticks; 0.1 on the curve is that same floor.
void StatePlay::FireBowArrow()
{
    float power = (bowCharge*bowCharge + 2.0f*bowCharge) / 3.0f;
    if(power < 0.1f)
        return;
    if(power > 1.0f)
        power = 1.0f;

    // Creative looses without the pack: a world switched to creative in-game still runs this state,
    // and the draw above already let the player pull the string with no arrows, so spending one here
    // would be a dry release on a draw that was allowed to happen. `gameModeWorld` is the mode the
    // world itself carries, which is what StatePlay goes by everywhere else; see HasItemInInventory.
    if(mWorld->gameModeWorld != 1 && mWorld->ConsumeOneItemFromInventory(Arrow::getID()) == false)
        return;

    Arrow2* arrow = new Arrow2(fppCam->m_vPosition.x, fppCam->m_vPosition.y, fppCam->m_vPosition.z);
    arrow->SetTexture(TextureHelper::Instance()->GetTexture(TextureHelper::arrowProjectileTexture));
    arrow->SetVeloc(fppCam->upDownAngle, (fppCam->horAngle/(float)180)*PI, power);
    // Banded the way the skeleton's shot is weighted (it fires a flat 2.0): a tap does 2, a full draw
    // does 8. Same units as every other hit in the game -- 2.0 is one heart.
    arrow->damage = 2.0f + 6.0f*power;
    arrow->fromMob = false;
    mWorld->mArrows.push_back(arrow);

    // The shot is away, so the bow has been used: one point of its 384, and it breaks if that was
    // the last one. After the push_back rather than before it, so a bow on its final use still
    // looses the arrow it was drawn for -- vanilla fires the shot and then breaks the bow.
    WearBowOnShot(mWorld, mSoundMgr, barPosition);

    mSoundMgr->PlayBowSound();

    animGo = true;
    animDest = 0.0f;
}

void StatePlay::PutInInventory(int id, int num, bool st)
{
    bool ok;
    ok = false;
    for(int j = 27; j <= 35; j++)
    {
        if (mWorld->invSt[j]==1)
        {
            if (mWorld->invId[j]== id)
            {
                if(mWorld->invAm[j] + num <= 64)
                {
                    mWorld->invAm[j] += num;
                    chestId = -1;
                    ok = true;
                    break;
                }
                else
                {
                    continue;
                }
            }
        }

        if (mWorld->invId[j]== -1)
        {
            mWorld->invSt[j] = st;
            mWorld->invId[j] = id;
            mWorld->invAm[j] = num;
            ok = true;
            break;
        }
    }

    if(ok == false)
    {
        for(int j = 0; j <= 26; j++)
        {
            if (mWorld->invSt[j]==1)
            {
                if (mWorld->invId[j]== id)
                {
                    if(mWorld->invAm[j] + num <= 64)
                    {
                        mWorld->invAm[j] += num;
                        chestId = -1;
                        break;
                        return;
                    }
                    else
                    {
                        continue;
                    }
                }
            }

            if (mWorld->invId[j]== -1)
            {
                mWorld->invSt[j] = st;
                mWorld->invId[j] = id;
                mWorld->invAm[j] = num;
                break;
                return;
            }
        }
    }
}

// Right-click with the debug stick in hand. Finds whatever is being aimed at and asks it to describe
// its own debug menu; nothing is assumed about the mob here, which is what makes this work for a
// block later without touching this function.
void StatePlay::CheckDebugStickUse()
{
    if(mWorld == NULL || fppCam == NULL)
        return;

    Vector3 dir = fppCam->m_vView - fppCam->m_vPosition;
    dir.normalize();

    // The same 5-block reach the feeding and ignition rays use. A testing tool needs to be able to
    // touch a mob without standing inside it, and standing inside it is exactly what a mob that is
    // being debugged tends to do.
    LameMob* mob = FindMobForUse(mWorld, fppCam->m_vPosition, dir, 5.0f);
    if(mob == NULL)
    {
        mChat.AddLine("Debug stick: nothing to point it at", 1.0f, 0.6f, 0.3f);
        return;
    }

    std::vector<std::string> options;
    if(mob->DebugOptions(options) <= 0)
    {
        mChat.AddLine(std::string("Debug stick: ") + mob->debugName + " has no debug actions", 1.0f, 0.6f, 0.3f);
        return;
    }

    // Held, not passed: the menu closes the moment the player picks something, and the action then
    // has to be applied to the same mob on a later frame.
    debugTarget = mob;
    debugMenu.Open(mob->debugName, options);
    mSoundMgr->PlayMenuSound2();
}

// Applies the debug menu's answer, if there is one. Reads and clears, so a choice is acted on once.
void StatePlay::ApplyDebugMenuResult()
{
    int picked = debugMenu.ConsumeResult();
    if(picked == DebugMenu::RESULT_NONE)
        return;

    if(picked == DebugMenu::RESULT_CANCELLED)
    {
        debugTarget = NULL;
        return;
    }

    if(debugTarget != NULL)
    {
        // The mob may have been destroyed between opening the menu and choosing. The world is frozen
        // while the menu is up so this should be impossible, but dereferencing a stale pointer is a
        // crash rather than a wrong-looking menu, so it is worth the four lines.
        bool live = false;
        for(unsigned int i = 0; i < mWorld->mSkeletons.size(); i++)
            if((LameMob*)mWorld->mSkeletons[i] == debugTarget) live = true;
        for(unsigned int i = 0; i < mWorld->mZombies.size(); i++)
            if((LameMob*)mWorld->mZombies[i] == debugTarget) live = true;
        for(unsigned int i = 0; i < mWorld->mCows.size(); i++)
            if((LameMob*)mWorld->mCows[i] == debugTarget) live = true;
        for(unsigned int i = 0; i < mWorld->mPigs.size(); i++)
            if((LameMob*)mWorld->mPigs[i] == debugTarget) live = true;
        for(unsigned int i = 0; i < mWorld->mSheeps.size(); i++)
            if((LameMob*)mWorld->mSheeps[i] == debugTarget) live = true;
        for(unsigned int i = 0; i < mWorld->mChickens.size(); i++)
            if((LameMob*)mWorld->mChickens[i] == debugTarget) live = true;
        for(unsigned int i = 0; i < mWorld->mSpiders.size(); i++)
            if((LameMob*)mWorld->mSpiders[i] == debugTarget) live = true;
        for(unsigned int i = 0; i < mWorld->mCreepers.size(); i++)
            if((LameMob*)mWorld->mCreepers[i] == debugTarget) live = true;

        if(live == true)
            debugTarget->DebugAction(picked);
    }

    debugTarget = NULL;
}

bool StatePlay::WorldRunning()
{
    // The world is ticked from Draw, not Update: StatePlay::Draw carries the mob, drop, TNT, arrow,
    // snowball and weather updates, each individually guarded on `menuState == 0`. So the pause menu
    // stops the world by leaving menuState at 1, and an early return from Update -- which is where the
    // debug menu's hold started life -- stops nothing at all, because Update is not where the world
    // advances. That is why a skeleton burned to death behind an open menu: the menu held Update, the
    // mobs kept ticking in Draw. Every one of those guards now asks this instead.
    //
    // menuState stays 0 while the debug menu is up, so the world still renders; only its simulation
    // is held.
    return menuState == 0 && debugMenu.IsOpen() == false;
}

#if MC_PERF_PROBE
// ---------------------------------------------------------------------------------------------
// Development-only cost probe -- see MC_PERF_PROBE in DebugConfig.h.
//
// The per-second counters in Scripts/PerfCounters.h say how *much* work a frame did; they cannot
// say what a unit of it costs. This closes that loop by timing one of each operation a running
// circuit performs, in the world that was just generated. The world is a throwaway: this writes a
// wire line and a torch into it and never saves, which is why the probe is a command-line switch
// and not a build anyone plays.
//
// One caveat worth stating where the numbers are produced: these are microseconds on whatever is
// running the EBOOT. In PPSSPP that is an x86 core at GHz speeds and the absolute figures are
// meaningless -- the *ratios* between the seven lines are not, and those are what the probe is
// for. A PSP run gives real microseconds.
static unsigned int PerfProbeNow()
{
    return sceKernelGetSystemTimeLow();
}

#if MC_PERF_PROBE
// ---------------------------------------------------------------------------------------------
// The other half of the probe: a *running* clock. See StatePlay::PerfProbeDriveClock.
//
// The micro-benchmarks price one operation each, in isolation, with nothing else in flight -- which is
// the right way to price a unit but not the shape of the thing being reported. A redstone clock is a
// producer that runs faster than a frame, and what that costs is only visible in what the frame does
// per second: how deep the mesh and light queues stand and whether the frame budget is spent working
// or waiting. So the two torches the benchmarks leave in the world are kept flipping from the update
// loop at a plain torch clock's rate for a bounded window, and the ordinary per-second `perf` lines
// report the steady state.
namespace probeclock
{
    const int   kTorches    = 2;
    const float kPeriod     = 0.2f;   // 5 Hz per torch, i.e. the 10 flips/s of a plain torch clock
    const float kRunSeconds = 20.0f;

    int   px[kTorches];
    int   py[kTorches];
    int   pz[kTorches];
    bool  lit[kTorches];
    int   live     = 0;
    float left     = 0.0f;
    float acc      = 0.0f;
}
#endif

void StatePlay::PerfProbeSelfTest()
{
    if(mWorld == NULL)
        return;

    const int bx = (int)playerPosition.x + 4;
    const int bz = (int)playerPosition.z + 4;
    const int by = mWorld->groundHeight(bx, bz) + 1;
    if(by <= 1 || by + 4 >= WORLD_HEIGHT)
    {
        MC_DEBUG_LOG("perfprobe: no room at y=%d\n", by);
        return;
    }

    // Clear a pad and lay the wire on it. The pad is bigger than the wire so the "empty window"
    // case below is measuring an empty window and not the terrain that happened to be there.
    for(int k = 0; k <= 3; k++)
        for(int j = -1; j <= 4; j++)
            for(int i = -1; i <= 40; i++)
                mWorld->GetBlock(bx + i, by + k, bz + j) = 0;

    const block_t wire = Redstoneoff::getID();
    const int wireLen = 20;
    for(int i = 0; i < wireLen; i++)
        mWorld->GetBlock(bx + i, by, bz) = wire;

    // Build the section that holds the wire first, so the mesh timings below are a rebuild and not
    // a first build (which allocates the vertex buffers and is a different cost entirely).
    const int cid = mWorld->getChunkId(Vector3((float)bx, (float)by, (float)bz));
    if(cid != -1)
    {
        mWorld->RebuildOpaqueMeshChunk(cid);
        mWorld->RebuildTransparentMeshChunk(cid);
    }

    unsigned int t0, t1;

    const int N_DUST = 500;
    t0 = PerfProbeNow();
    for(int i = 0; i < N_DUST; i++)
        mWorld->GetRedstoneDustPower(bx, by, bz);
    t1 = PerfProbeNow();
    const unsigned int usDust = (t1 - t0) / (unsigned int)N_DUST;

    const int N_RS = 500;
    t0 = PerfProbeNow();
    for(int i = 0; i < N_RS; i++)
        mWorld->UpdateRedstonePower(bx, by, bz);
    t1 = PerfProbeNow();
    const unsigned int usRsWire = (t1 - t0) / (unsigned int)N_RS;

    t0 = PerfProbeNow();
    for(int i = 0; i < N_RS; i++)
        mWorld->UpdateRedstonePower(bx + 30, by, bz + 3);
    t1 = PerfProbeNow();
    const unsigned int usRsEmpty = (t1 - t0) / (unsigned int)N_RS;

    // A torch flip, one direction at a time, alternating so each call actually flips. `torchZ` sits
    // in the wire just laid, so both directions pay a dust recompute; `awayX` does not, which is what
    // separates "the flip" from "the wire it is wired into".
    const int torchX = bx + 2, torchY = by + 1, torchZ = bz;
    const int awayX = bx + 30, awayY = by + 1, awayZ = bz;
    mWorld->GetBlock(torchX, torchY, torchZ) = RedstoneTorch::getID();
    mWorld->GetBlock(awayX, awayY, awayZ) = RedstoneTorch::getID();

    const int N_FLIP = 40;
    unsigned int accDark = 0, accLit = 0, accAway = 0;
    for(int i = 0; i < N_FLIP; i++)
    {
        t0 = PerfProbeNow();
        mWorld->SetRedstoneTorchLit(torchX, torchY, torchZ, false);
        t1 = PerfProbeNow();
        accDark += t1 - t0;

        t0 = PerfProbeNow();
        mWorld->SetRedstoneTorchLit(torchX, torchY, torchZ, true);
        t1 = PerfProbeNow();
        accLit += t1 - t0;

        t0 = PerfProbeNow();
        mWorld->SetRedstoneTorchLit(awayX, awayY, awayZ, false);
        t1 = PerfProbeNow();
        mWorld->SetRedstoneTorchLit(awayX, awayY, awayZ, true);
        accAway += PerfProbeNow() - t0;
    }
    const unsigned int usTorchDark = accDark / (unsigned int)N_FLIP;
    const unsigned int usTorchLit = accLit / (unsigned int)N_FLIP;
    const unsigned int usTorchAway = accAway / (unsigned int)N_FLIP;

    // The 19^3 rescan a flip queues, through the queue so the drain is what is timed and not the
    // enqueue. QueueLightAreaIn keeps at most 8, which is why this fills 8 and drains 8. Best of
    // them, not the average: the first pass through a cold section is not what a running clock pays.
    for(int i = 0; i < 8; i++)
        mWorld->QueueLightAreaIn(Vector3((float)torchX, (float)torchY, (float)torchZ));
    unsigned int usLightScan = 0xFFFFFFFFu;
    for(int i = 0; i < 8; i++)
    {
        t0 = PerfProbeNow();
        mWorld->ProcessQueuedLightArea();
        t1 = PerfProbeNow();
        if(t1 - t0 < usLightScan)
            usLightScan = t1 - t0;
    }

    unsigned int usMeshTransparent = 0, usMeshOpaque = 0, usMeshFull = 0;
    unsigned int usMeshCache = 0;
    if(cid != -1)
    {
        const int N_T = 10;
        t0 = PerfProbeNow();
        for(int i = 0; i < N_T; i++)
            mWorld->RebuildTransparentMeshChunk(cid);
        t1 = PerfProbeNow();
        usMeshTransparent = (t1 - t0) / (unsigned int)N_T;

        t0 = PerfProbeNow();
        for(int i = 0; i < N_T; i++)
            mWorld->RebuildOpaqueMeshChunk(cid);
        t1 = PerfProbeNow();
        usMeshOpaque = (t1 - t0) / (unsigned int)N_T;

        const int N_F = 5;
        t0 = PerfProbeNow();
        for(int i = 0; i < N_F; i++)
            mWorld->RebuildFullMeshChunk(cid);
        t1 = PerfProbeNow();
        usMeshFull = (t1 - t0) / (unsigned int)N_F;

        // The fixed half of a remesh: the 18^3 light/block cache every rebuild fills, measured with no
        // walk and no geometry around it. Everything else a rebuild costs scales with what is in the
        // section; this does not, and it is the number to look at before optimising the meshers -- a
        // section with almost nothing transparent in it still pays it in full.
        const int N_C = 10;
        t0 = PerfProbeNow();
        mWorld->PerfProbeMeshCache(cid, N_C);
        t1 = PerfProbeNow();
        usMeshCache = (t1 - t0) / (unsigned int)N_C;
    }

    // Hand the torches to the driven clock. Both were left lit by the flip loop above, and they are the
    // same two cells the numbers above were taken in, so the steady state that follows is a clock over
    // the geometry that was just priced.
    probeclock::px[0] = torchX; probeclock::py[0] = torchY; probeclock::pz[0] = torchZ;
    probeclock::px[1] = awayX;  probeclock::py[1] = awayY;  probeclock::pz[1] = awayZ;
    probeclock::lit[0] = true;  probeclock::lit[1] = true;
    probeclock::live = probeclock::kTorches;
    probeclock::left = probeclock::kRunSeconds;
    probeclock::acc  = 0.0f;
    MC_DEBUG_LOG("perfprobe: driving %d torches at %.0f Hz for %.0fs\n",
                 probeclock::kTorches, 1.0f / probeclock::kPeriod, probeclock::kRunSeconds);

    MC_DEBUG_LOG("perfprobe us/op wire=%d at (%d,%d,%d): dustPower=%u rsWire=%u rsEmpty=%u | torch dark=%u lit=%u away=%u | lightScan=%u | mesh transp=%u opaque=%u full=%u cache=%u\n",
                 wireLen, bx, by, bz,
                 usDust, usRsWire, usRsEmpty,
                 usTorchDark, usTorchLit, usTorchAway,
                 usLightScan, usMeshTransparent, usMeshOpaque, usMeshFull, usMeshCache);
}
#else
void StatePlay::PerfProbeSelfTest()
{
    // MC_PERF_PROBE off: nothing to time. The declaration exists in the header so the two builds
    // have the same class layout, and this keeps the symbol from being undefined.
}
#endif

#if MC_PERF_PROBE
// Called once a frame from Update. While the window PerfProbeSelfTest opened is open, this flips each
// benchmark torch once every kPeriod and leaves the frame's other work to the engine -- the point is
// the steady state the per-second counters report, not anything this function does itself.
void StatePlay::PerfProbeDriveClock(float frameDt)
{
    if(probeclock::live <= 0)
        return;

    if(probeclock::left <= 0.0f)
    {
        MC_DEBUG_LOG("perfprobe: clock stopped\n");
        probeclock::live = 0;
        return;
    }
    probeclock::left -= frameDt;

    // Accumulated rather than a timer per torch: Update can be called with any dt, and a period that
    // does not divide the frame rate must still average the same rate as one that does.
    probeclock::acc += frameDt;
    while(probeclock::acc >= probeclock::kPeriod)
    {
        probeclock::acc -= probeclock::kPeriod;
        for(int i = 0; i < probeclock::live; i++)
        {
            probeclock::lit[i] = !probeclock::lit[i];
            mWorld->SetRedstoneTorchLit(probeclock::px[i], probeclock::py[i], probeclock::pz[i], probeclock::lit[i]);
        }
    }
}
#else
void StatePlay::PerfProbeDriveClock(float frameDt)
{
    (void)frameDt;
}
#endif

void StatePlay::DrawText(int x,int y, unsigned int color, float size, const char *message, ...)
{
    va_list argList;
    char cbuffer[512];
    va_start(argList, message);
    vsnprintf(cbuffer, 512, message, argList);
    va_end(argList);

    RenderManager::InstancePtr()->SetFontStyle(size,GU_COLOR(0.1f,0.1f,0.1f,1),0,0x00000200|0x00000000);
    RenderManager::InstancePtr()->DebugPrint(x+1,y+1,"%s",cbuffer);
    RenderManager::InstancePtr()->SetFontStyle(size,color,0,0x00000200|0x00000000);
    RenderManager::InstancePtr()->DebugPrint(x,y,"%s",cbuffer);
}

void StatePlay::DrawText2(int x,int y, unsigned int color, float size, const char *message, ...)
{
    va_list argList;
    char cbuffer[512];
    va_start(argList, message);
    vsnprintf(cbuffer, 512, message, argList);
    va_end(argList);

    RenderManager::InstancePtr()->SetFontStyle(size,GU_COLOR(0.1f,0.1f,0.1f,1),0,0x00004000|0x00000000);
    RenderManager::InstancePtr()->DebugPrint(x+1,y+1,"%s",cbuffer);
    RenderManager::InstancePtr()->SetFontStyle(size,color,0,0x00004000|0x00000000);
    RenderManager::InstancePtr()->DebugPrint(x,y,"%s",cbuffer);
}

void StatePlay::UpdateInvCursorPos()
{
    if(armorEn)
    {
        selectInvSprite->SetPosition(GuiSlotX(guiInvW, guiScale, 7.0f + invXPosition * 54.0f), GuiSlotY(guiInvH, guiScale, 4.0f + invYPosition * 36.0f));
        return;
    }
    if(upEn)
    {
        if(invEn)
        {
            selectInvSprite->SetPosition(GuiSlotX(guiInvW, guiScale, 97.0f + invXPosition * 18.0f), GuiSlotY(guiInvH, guiScale, 14.0f + invYPosition * 18.0f));
            return;
        }
        if(craft3xEn)
        {
            selectInvSprite->SetPosition(GuiSlotX(guiCrtW, guiScale, 61.0f + invXPosition * 18.0f), GuiSlotY(guiCrtH, guiScale, 4.0f + invYPosition * 18.0f));
            return;
        }
        if(chestEn)
        {
            selectInvSprite->SetPosition(GuiSlotX(guiChtW, guiScale, 7.0f + invXPosition * 18.0f), GuiSlotY(guiChtH, guiScale, 4.0f + invYPosition * 18.0f));
            return;
        }
        if(furnaceEn)
        {
            selectInvSprite->SetPosition(GuiSlotX(guiFurW, guiScale, 61.0f), GuiSlotY(guiFurH, guiScale, 4.0f + invYPosition * 36.0f));
            return;
        }
    }

    float w = guiInvW;
    float h = guiInvH;
    if(craft3xEn)
    {
        w = guiCrtW;
        h = guiCrtH;
    }
    else if(chestEn)
    {
        w = guiChtW;
        h = guiChtH;
    }
    else if(furnaceEn)
    {
        w = guiFurW;
        h = guiFurH;
    }
    selectInvSprite->SetPosition(GuiSlotX(w, guiScale, 7.0f + invXPosition * 18.0f), GuiPlayerInvSlotY(h, guiScale, invYPosition));
}

void StatePlay::DrawAmount(int x,int y, int amount)
{
	int dozens, units;

	dozens = floorf(amount / 10.0f);
	units = amount % 10;

	RenderManager::InstancePtr()->SetFont(0);
	if (dozens != 0)
	{
		RenderManager::InstancePtr()->SetFontStyle(default_size, GU_COLOR(0.25f, 0.25f, 0.25f, 1.0f), 0, 0x00000400 | 0x00004000);
		RenderManager::InstancePtr()->DebugPrint(x + 2, y + 2, "%i", dozens);
		RenderManager::InstancePtr()->SetFontStyle(default_size, GU_COLOR(1.0f, 1.0f, 1.0f, 1.0f), 0, 0x00000400 | 0x00004000);
		RenderManager::InstancePtr()->DebugPrint(x, y, "%i", dozens);
	}

	RenderManager::InstancePtr()->SetFontStyle(default_size, GU_COLOR(0.25f, 0.25f, 0.25f, 1.0f), 0, 0x00000400 | 0x00004000);
	RenderManager::InstancePtr()->DebugPrint(x + 14, y + 2, "%i", units);
	RenderManager::InstancePtr()->SetFontStyle(default_size, GU_COLOR(1.0f, 1.0f, 1.0f, 1.0f), 0, 0x00000400 | 0x00004000);
	RenderManager::InstancePtr()->DebugPrint(x + 12, y, "%i", units);
	RenderManager::InstancePtr()->SetDefaultFont();
}