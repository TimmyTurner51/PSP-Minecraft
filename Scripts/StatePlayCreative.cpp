#include "StatePlayCreative.h"
#include <Aurora/Graphics/Effects/Arrow2.h>
#include "TextureHelper.h"
#include "DebugConfig.h"
#include "PerfCounters.h"
#include <stdio.h>
#include <stdarg.h>

#include "WorldGenerator.h"
#include "LoadingScreen.h"

#define PI 3.14159265f
#define DEG_TO_RAD (PI / 180.0f)
#define TEXTURE_CHUNKS 8
#define SKY_MOVE 0.003f

#define WORLD_SIZE 256
#define WORLD_HEIGHT 112
#define CHUNK_SIZE 16

#define no_size 0.0
#define default_size 0.5
#define default_big_size 0.687

// How long a single chat-button press waits for a second one, so that the double tap which toggles
// the third-person view can be told apart from the single tap that opens chat. See StatePlay.cpp
// for the full reasoning; the two states share these values on purpose.
#define CHAT_DOUBLE_TAP_WINDOW 0.3f

// Third-person view: boom length, the floor it is never pulled in past, the number of whole steps
// the walk is sampled in, and how far the head may turn away from the body before the body is dragged
// round after it (Java's updateDistance clamp). See StatePlay.cpp for the reasoning; the two states
// deliberately share these values.
#define THIRD_PERSON_DISTANCE 3.5f
#define THIRD_PERSON_NEVER_BELOW 1.1f
#define THIRD_PERSON_STEPS 12
#define BODY_MAX_HEAD_TURN 75.0f

// Ground sprint, which creative needs as well as survival (Java lets you sprint in creative; the fly
// sprint above is a separate thing and only applies while canFly). Same values as StatePlay.cpp --
// see there for where each comes from.
#define SPRINT_SPEED_MULT 1.3f
#define SPRINT_FOV_MULT 0.15f
#define SPRINT_TAP_WINDOW 0.35f
#define SPRINT_TAP_MIN_GAP 0.04f

// Camera bob is not scaled or suppressed in third person -- Java applies view bobbing identically in
// every perspective. See StatePlay.cpp for why, and for what the real correction was.

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

static void CreativeCopyItemToCursor(CraftWorld* world, int itemId)
{
    if(world == NULL || itemId == -1)
        return;

    world->mId = itemId;
    if(IsItemId(itemId))
    {
        int dura = world->DurabilityPointsItem(itemId);
        if(dura != -1)
        {
            world->mAm = dura;
            world->mSt = 0;
        }
        else
        {
            // Stackable items (eggs, food, etc.) — not tools. mSt must be true
            // or the hotbar treats them like damaged gear.
            world->mAm = 999;
            world->mSt = world->StackableItem(itemId) ? 1 : 0;
        }
    }
    else
    {
        world->mAm = 999;
        world->mSt = 999;
    }
}

static void CreativeClearCursor(CraftWorld* world)
{
    if(world == NULL)
        return;

    world->mId = -1;
    world->mAm = -1;
    world->mSt = 0;
}

static void CreativeHandleCatalogClick(CraftWorld* world, int catalogId, bool* heldFromHotbar)
{
    if(world == NULL || heldFromHotbar == NULL)
        return;

    if(world->mId != -1 && *heldFromHotbar)
    {
        CreativeClearCursor(world);
        *heldFromHotbar = false;
        return;
    }

    if(catalogId != -1)
    {
        CreativeCopyItemToCursor(world, catalogId);
        *heldFromHotbar = false;
        return;
    }

    CreativeClearCursor(world);
    *heldFromHotbar = false;
}

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

StatePlayCreative::StatePlayCreative()
{
    portalFadeTex = -1;
    mRender = NULL;
    mSystemMgr = NULL;
    fppCam = NULL;
    invPlayer = NULL;
    UseChest = NULL;
    UseFurnace = NULL;

    saveFileName = "";

    sleepMessageTime = 0.0f;
    sleepTime = 0.0f;
    sleepMessageType = 0;

    check = false;
    statisticsPage = 0;

    creativePage = 0;
    creativePageMax = 0;
    creativeTab = 0;
    //utils
    freeMemory = 0;
    freeMemoryTimer = 0.0f;
    showCube = true;
    pendingCrackId = -1;
    pendingCrackX = 0.0f;
    pendingCrackY = 0.0f;
    pendingCrackZ = 0.0f;
    pendingBreakStage = 0;
    pendingBreakWasLight = false;
    pendingBreakChunk = -1;

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

    invEn = false;
    craft3xEn = false;
    upEn = false;
    chestEn = false;
    furnaceEn = false;

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
    furnaceTimes = 0.0f;

    musicTimeGo = 0;

    timeTexture = 0;
    currentTexture = 0;

    chunkId = 0;
    chunks = 0;

    angleFactor = 0.0f;

    startDt = false;
    dET = 0;
    dT = 0;
    dStd = -1;

    bowCharging = false;
    bowCharge = 0.0f;

    testPos1 = Vector3(-1,-1,-1);
    cubePos = Vector3(0,0,0);

    playerVelocity = Vector3(0,0,0);
    playerPosition = Vector3(0,0,0);
    newPlayerPos = Vector3(0,0,0);
    oldPlayerPos = Vector3(0,0,0);

    barPosition = 0;
    invXPosition = 0;
    invYPosition = 0;

    tempXFurnace = 0;
    tempYFurnace = 0;
    tempUpFurnace = false;

    tempXInv = 0;
    tempYInv = 0;
    tempUpInv = false;

    tempXCT = 0;
    tempYCT = 0;
    tempUpCT = false;

    tempXChest = 0;
    tempYChest = 0;
    tempUpChest = false;

    shift_x = 0.0f;
    shift_y = 0.0f;
    changeY = 0.0f;

    craftItemId = -1;
    craftItemAm = -1;
    craftItemSt = 0;

    craftItemId3 = -1;
    craftItemAm3 = -1;
    craftItemSt3 = 0;

    chestId = -1;
    furnaceId = -1;

    ram1 = 0;
    ram2 = 0;
    dt = 0.0f;

    cameraSpeed = 2.0f / 60.0f;
    cameraMoveSpeed = 4.0f/ 60.0f;

    menuState = 0;
    selectPos = 0;

    // The debug menu must never survive a world load -- see the note in StatePlay::Init.
    debugMenu.Close();
    debugTarget = NULL;

    cloudsOffset = 0.0f;

    devMode = false;
    makeScreen = false;

	canHeadBob = true;
	bobCycle = 0.0f;
	bobWalk = 0.0f;
	camFollowY = 0.0f;
	camFollowYInit = false;
	lookDt = 1.0f / 60.0f;
    bobType = false;

    cubeLight = 0.0f;

	anim[0] = 0;
	anim[1] = 0;
	anim[2] = 0;

	canFly = false;
	flyJumpTimer = 0.0f;
	flyJumpWasHeld = false;
	flyBoost = 1.0f;
	flyForwardTimer = 0.0f;
	flyForwardWasHeld = false;
	flyForwardPending = false;
	flySprintArm = false;
	flySprint = false;
	flyFovBoost = 0.0f;
	flyGlideVelocity = Vector3(0,0,0);

	for(int i = 0; i <= 3; i += 1)
    {
        craftSlotId[i] = -1;
        craftSlotAm[i] = -1;
        craftSlotSt[i] = 0;
    }

    creativeSlotId[26] = -1;

    for(int i = 0; i <= 8; i += 1)
    {
        craftSlotId3[i] = -1;
        craftSlotAm3[i] = -1;
        craftSlotSt3[i] = 0;
    }

    tickHunger = 0.0f;
    tickHealth = 0.0f;
    tickCave = 0.0f;
    tickChunk = 0.0f;
    tickShowFlymodeMessage = 0.0f;
    tickShowSlotName = 0.0f;
    tickOS = 0;

    rainSoundFrequency = 1.7f;

    inputDiskNameTimer = 0.0f;
    inputDiskName = "";

    hurt = false;
    hurt_time = 0;
    musicTime = 1;

    dieFactor = true;
    starsAngle = 0.0f;

    animDest = 0.0f;
    animSpeed = 17.0f;
    animGo = false;

    showSlotName = false;
    slotId = -1;
    showSlotTimer = 0.0f;
    showSlotTimerMax = 1.0f; // 1 sec

    usingSlider = false;
    onDestroySlot = false;
    creativeHeldFromHotbar = false;
    creativeTab = 0;
    creativeMouseX = 240.0f;
    creativeMouseY = 136.0f;
    creativeMouseFree = false;
    for(int t = 0; t < 5; t++)
        creativeTabSelSprite[t] = 0;

    pre_fps = 0.0f;
    average_fps = 0;
    tick_fps = 0.0f;
    ticks = 0;
}

StatePlayCreative::~StatePlayCreative()
{
    delete invPlayer;
    invPlayer = NULL;
}

void StatePlayCreative::InitCamera()
{
    fppCam = new Camera();
    fppCam->PositionCamera(playerPosition.x,playerPosition.y,playerPosition.z, playerPosition.x,playerPosition.y,playerPosition.z-5.0f, 0.0f,1.0f,0.0f);
    mRender->SetActiveCamera(fppCam);

    // The third-person body starts aligned with wherever the camera is pointing, so loading a save
    // does not snap the model round on the first frame.
    bodyYaw = fppCam->horAngle;
    headYawOffset = 0.0f;

    // One model, used only by the third-person view. InitCamera can run again on a load, so it is
    // only built when missing.
    if(invPlayer == NULL)
        invPlayer = new InventoryPlayer();


    mWorld->UpdatePlayerZoneBB(fppCam->m_vPosition);
    mWorld->FetchNearestChunks();
}

void StatePlayCreative::Init()
{
    // See StatePlay::Init: the release has to precede the world build, not the state push.
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
    mWorld->SetWolrdTime(6);
    mWorld->UpdatePlayerZoneBB(playerPosition);
    mWorld->buildMap();
    mWorld->buildblocksVerts();
    mWorld->buildcloudsVerts();
    mWorld->buildmobVerts();

    playerPosition = newPlayerPos = oldPlayerPos = Vector3(64.0f,mWorld->groundHeight(0,0)+0,0);

    // (curchunkTarget removed: computed from getChunkId and never read)

    dt = mTimer.GetDeltaTime();

    bobCycle = PI/2;

    LoadTextures();
    mWorld->GetTexturesIds();

    menuOptions = false;
    optionsMenuPos = 0;

    analogLeft = analogRight = analogUp = analogDown = false;

    walkSoundAccu = 0.0f;
    isWalking = false;
}

void StatePlayCreative::InitParametric(bool makeTrees, bool makeWater,bool makeCaves,unsigned int seed_1,int worldType, char gameMode)
{
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
    bobCycle = PI/2.0f;
    cubeLight = 0.0f;

    InitCreativeInventory();

    mWorld->haveCompass = mWorld->HaveItemInBarSlots(Compass::getID());
}

void StatePlayCreative::SetWorldAndSaveName(std::string worldName,std::string fileName)
{
    if(mWorld != NULL)
    {
        sprintf(mWorld->worldName,"%s",worldName.c_str());
        mWorld->SetWorldSavePath(fileName);
    }
    saveFileName = fileName;
}

void StatePlayCreative::LoadMap(std::string fileName,bool compressed)
{
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
    playerPosition = newPlayerPos = oldPlayerPos = Vector3(64.0f,64,64);

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

    InitCreativeInventory();

    mWorld->haveCompass = mWorld->HaveItemInBarSlots(Compass::getID());

    // cancel loading screen
    loading->KillLoadingScreen();
    delete loading;
}

void StatePlayCreative::LoadTextures()
{
    TextureHelper::Instance()->ApplySavedSkin();

    //terrain texure
    barItems = TextureHelper::Instance()->GetTexture(TextureHelper::Items1);
    texture = TextureHelper::Instance()->GetTexture(TextureHelper::Terrain1);
    texture_mips = TextureHelper::Instance()->GetTexture(TextureHelper::Terrain2);
    waterAnimation = TextureHelper::Instance()->GetTexture(TextureHelper::WaterAnimation);
    Particle::PrecacheTextures(mWorld);
    cloudsTex = TextureHelper::Instance()->GetTexture(TextureHelper::clouds1);
    blue = TextureHelper::Instance()->GetTexture(TextureHelper::Blue);
    red = TextureHelper::Instance()->GetTexture(TextureHelper::Red);
    black = TextureHelper::Instance()->GetTexture(TextureHelper::Black);
    stars = TextureHelper::Instance()->GetTexture(TextureHelper::Stars);
    snowBall4 = TextureHelper::Instance()->GetTexture(TextureHelper::SnowBall3);
    suntex = TextureHelper::Instance()->GetTexture(TextureHelper::Sun);
    moontex = TextureHelper::Instance()->GetTexture(TextureHelper::Moon);
    grassColorTex = TextureHelper::Instance()->GetTexture(TextureHelper::vegetationColorMap);

    // sprites
    waterScreen = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Blue));
    waterScreen->SetPosition(240,136);
    waterScreen->Scale(30,17);

    // hud section
	int utilsSize = TextureManager::Instance()->getWidth(TextureHelper::Instance()->GetTexture(TextureHelper::Utils));
	float utilScale = 264.0f / (float)utilsSize;

	barSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils), 0 * utilsSize / 182, 0 * utilsSize / 182, 182 * utilsSize / 182, 22 * utilsSize / 182);
	barSprite->SetPosition(240, 230);
	barSprite->Scale(utilScale, utilScale);

	selectSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils), 0 * utilsSize / 182, 22 * utilsSize / 182, 24 * utilsSize / 182, 24 * utilsSize / 182);
	selectSprite->SetPosition(108.0f + 11.0f * (264.0f / 182.0f), 230);
	selectSprite->Scale(utilScale, utilScale);

	crossSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils), 24 * utilsSize / 182, 22 * utilsSize / 182, 9 * utilsSize / 182, 9 * utilsSize / 182);
	crossSprite->SetPosition(240, 136);
	crossSprite->Scale(utilScale, utilScale);

    // menu section
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

    int creativeTex = TextureHelper::Instance()->GetTexture(TextureHelper::invCreative);
    guiCreativeW = kCreativeGuiW;
    guiCreativeH = kCreativeGuiH;
    guiCreativeScale = kCreativeGuiScale;

    int crtTex = TextureHelper::Instance()->GetTexture(TextureHelper::crt);
    guiCrtW = TextureManager::Instance()->getWidth(crtTex);
    guiCrtH = (float)TextureManager::Instance()->getHeight(crtTex);

    int chtTex = TextureHelper::Instance()->GetTexture(TextureHelper::cht);
    guiChtW = TextureManager::Instance()->getWidth(chtTex);
    guiChtH = (float)TextureManager::Instance()->getHeight(chtTex);

    int furTex = TextureHelper::Instance()->GetTexture(TextureHelper::fur);
    guiFurW = TextureManager::Instance()->getWidth(furTex);
    guiFurH = (float)TextureManager::Instance()->getHeight(furTex);

    invSprite = new Sprite(creativeTex, 0, 0, (int)kCreativeGuiW, (int)kCreativeGuiH);
    invSprite->SetPosition(240,136);
    invSprite->Scale(guiCreativeScale,guiCreativeScale);

    for(int t = 0; t < kCreativeTabCount; t++)
    {
        creativeTabSelSprite[t] = new Sprite(creativeTex, t * (int)kCreativeTabW, (int)kCreativeGuiH, (int)kCreativeTabW, (int)kCreativeTabH);
        creativeTabSelSprite[t]->Scale(guiCreativeScale, guiCreativeScale);
    }

    crtSprite = new Sprite(crtTex);//200
    crtSprite->SetPosition(240,136);
    crtSprite->Scale(guiScale,guiScale);

    chtSprite = new Sprite(chtTex);//200
    chtSprite->SetPosition(240,136);
    chtSprite->Scale(guiScale,guiScale);

    furSprite = new Sprite(furTex);//200
    furSprite->SetPosition(240,136);
    furSprite->Scale(guiScale,guiScale);

    sliderSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::invSlider),0,0,12,15);//200
    sliderSprite->SetPosition(240,136);
    sliderSprite->Scale(guiCreativeScale,guiCreativeScale);

    sliderLightedSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::invSlider),12,0,12,15);//200
    sliderLightedSprite->SetPosition(240,136);
    sliderLightedSprite->Scale(guiCreativeScale,guiCreativeScale);

    float invArrowSize = TextureManager::Instance()->getWidth(TextureHelper::Instance()->GetTexture(TextureHelper::furArrow));
    float invArrowScale = 44.0f/invArrowSize;

    for(int j = 13; j >= 0; j--)
    {
        toolPointSprite[j] = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Utils),169*utilsSize/182,(49+(13-j)*2)*utilsSize/182,13*utilsSize/182,2*utilsSize/182,true);//200
        toolPointSprite[j]->SetPosition(240,136);
        toolPointSprite[j]->Scale(utilScale,utilScale);
    }

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
}

void StatePlayCreative::SetCreativePage(char page)
{
    creativePage = page;
    RefreshCreativePageMax();
}

std::vector<int>& StatePlayCreative::CreativeItems()
{
    if(creativeTab < 0 || creativeTab >= kCreativeTabCount)
        creativeTab = 0;
    return creativeTabItems[creativeTab];
}

void StatePlayCreative::RefreshCreativePageMax()
{
    int n = (int)CreativeItems().size();
    int rows = (n + 8) / 9;
    creativePageMax = rows - kCreativeCatRows;
    if(creativePageMax < 0)
        creativePageMax = 0;
    if(creativePage > creativePageMax)
        creativePage = creativePageMax;
    if(creativePage < 0)
        creativePage = 0;
}

void StatePlayCreative::SetCreativeTab(int tab)
{
    while(tab < 0)
        tab += kCreativeTabCount;
    while(tab >= kCreativeTabCount)
        tab -= kCreativeTabCount;
    creativeTab = tab;
    creativePage = 0;
    creativeMouseFree = false;
    RefreshCreativePageMax();
    if(invYPosition > kCreativeHotbarRow)
        invYPosition = kCreativeHotbarRow;
    UpdateInvCursorPos();
}

void StatePlayCreative::InitCreativeInventory()
{
    for(int t = 0; t < kCreativeTabCount; t++)
        creativeTabItems[t].clear();

    std::vector<int>& building = creativeTabItems[0];
    std::vector<int>& deco = creativeTabItems[1];
    std::vector<int>& redstone = creativeTabItems[2];
    std::vector<int>& items = creativeTabItems[3];
    std::vector<int>& mats = creativeTabItems[4];

    building.push_back(CobbleStone::getID());
    building.push_back(RockBlock::getID());
    building.push_back(DiamondOre::getID());
    building.push_back(GoldBlock::getID());
    building.push_back(IronOre::getID());
    building.push_back(CoalOre::getID());
    building.push_back(LapisOre::getID());
    building.push_back(RedstoneOre::getID());
    building.push_back(StoneBrick::getID());
    building.push_back(CrackedStoneBrick::getID());
    building.push_back(CarvedStoneBrick::getID());
    building.push_back(ClayBlock::getID());
    building.push_back(Diamond::getID());
    building.push_back(Gold::getID());
    building.push_back(Iron::getID());
    building.push_back(bedrock::getID());
    building.push_back(LapisBlock::getID());
    building.push_back(BrickBlock::getID());
    building.push_back(MossyCobblestone::getID());
    building.push_back(Slab::getID());
    building.push_back(StoneSandHalfBlock::getID());
    building.push_back(OakHalfBlock::getID());
    building.push_back(BirchHalfBlock::getID());
    building.push_back(SpruceHalfBlock::getID());
    building.push_back(CobbleHalfBlock::getID());
    building.push_back(BrickHalfBlock::getID());
    building.push_back(HalfStoneBrick::getID());
    building.push_back(Obsidian::getID());
    building.push_back(WoodBlock::getID());
    building.push_back(WhiteWoodBlock::getID());
    building.push_back(DarkWoodBlock::getID());
    building.push_back(LeavesBlock::getID());
    building.push_back(BirchLeaves::getID());
    building.push_back(SpruceLeaves::getID());
    building.push_back(DirtBlock::getID());
    building.push_back(GrassBlock::getID());
    building.push_back(GravitySandBlock::getID());
    building.push_back(SandStone::getID());
    building.push_back(Gravel::getID());
    building.push_back(OakPlanks::getID());
    building.push_back(BirchPlanks::getID());
    building.push_back(SprucePlanks::getID());
    building.push_back(IceBlock::getID());
    building.push_back(WaterBusket::getID());
    building.push_back(LavaBusket::getID());
    building.push_back(SnowBlock::getID());
    building.push_back(SnowSoil::getID());
    building.push_back(GlowstoneBlock::getID());
    building.push_back(Soulsand::getID());
    building.push_back(Netherrack::getID());
    building.push_back(MagmaBlock::getID());
    building.push_back(BlockofQuartz::getID());
    building.push_back(ChiseledQuartzBlock::getID());
    building.push_back(PillarQuartzBlock::getID());
    building.push_back(HayBale::getID());
    building.push_back(CoalBlock::getID());
    building.push_back(white_concrete_powder::getID());
    building.push_back(yellow_concrete_powder::getID());
    building.push_back(pink_concrete_powder::getID());
    building.push_back(gray_concrete_powder::getID());
    building.push_back(purple_concrete_powder::getID());
    building.push_back(blue_concrete_powder::getID());
    building.push_back(green_concrete_powder::getID());
    building.push_back(red_concrete_powder::getID());
    building.push_back(black_concrete_powder::getID());
    building.push_back(WoodenStair1::getID());
    building.push_back(BirchStair1::getID());
    building.push_back(SpruceStair1::getID());
    building.push_back(CobbleStair1::getID());
    building.push_back(BrickStair1::getID());

    deco.push_back(OakSaplingItem::getID());
    deco.push_back(BirchSaplingItem::getID());
    deco.push_back(SpruceSaplingItem::getID());
    deco.push_back(Flower1Item::getID());
    deco.push_back(Flower2Item::getID());
    deco.push_back(MooshroomItem1::getID());
    deco.push_back(MooshroomItem2::getID());
    deco.push_back(CactusBlock::getID());
    deco.push_back(MelonBlock::getID());
    deco.push_back(Pumpkin3::getID());
    deco.push_back(JackOLantern3::getID());
    deco.push_back(IronBarsItem::getID());
    deco.push_back(GlassBlock::getID());
    deco.push_back(YellowGlassBlock::getID());
    deco.push_back(GreenGlassBlock::getID());
    deco.push_back(PinkGlassBlock::getID());
    deco.push_back(PurpleGlassBlock::getID());
    deco.push_back(RedGlassBlock::getID());
    deco.push_back(BlackGlassBlock::getID());
    deco.push_back(BlueGlassBlock::getID());
    deco.push_back(GlassPanelItem::getID());
    deco.push_back(YellowGlassBlockPannelItem::getID());
    deco.push_back(GreenGlassBlockPannelItem::getID());
    deco.push_back(PinkGlassBlockPannelItem::getID());
    deco.push_back(PurpleGlassBlockPannelItem::getID());
    deco.push_back(RedGlassBlockPannelItem::getID());
    deco.push_back(BlackGlassBlockPannelItem::getID());
    deco.push_back(BlueGlassBlockPannelItem::getID());
    deco.push_back(ChestBlock3::getID());
    deco.push_back(CraftingTable::getID());
    deco.push_back(FurnaceOff::getID());
    deco.push_back(ShelfBlock::getID());
    deco.push_back(JukeBox::getID());
    deco.push_back(Snow2::getID());
    for (int i = BlackWoolBlock::getID(); i <= WhiteWoolBlock::getID(); i++)
        deco.push_back(i);
    deco.push_back(WoodenFence::getID());
    deco.push_back(BirchFence::getID());
    deco.push_back(SpruceFence::getID());
    deco.push_back(LadderItem::getID());
    deco.push_back(TorchItem::getID());
    deco.push_back(TrapdoorClosed1::getID());
    for (int i = BlackWoolCarpet::getID(); i <= WhiteWoolCarpet::getID(); i++)
        deco.push_back(i);
    deco.push_back(ItemFrameItem::getID());
    deco.push_back(DoorItem::getID());
    deco.push_back(BedItem::getID());
    deco.push_back(Sign::getID());
    deco.push_back(FlowerPotItem::getID());
    deco.push_back(PaintningItem::getID());

    redstone.push_back(Redstone::getID());
    redstone.push_back(RedstoneBlock::getID());
    redstone.push_back(NoteBlock::getID());
    redstone.push_back(ButtonStone::getID());
    redstone.push_back(ButtonWood::getID());
    redstone.push_back(Lever::getID());
    redstone.push_back(RedstoneLampOff::getID());
    redstone.push_back(RepeaterItem::getID());
    redstone.push_back(RedstoneTorchItem::getID());
    redstone.push_back(PistonOff::getID());
    redstone.push_back(StickyPistonOff::getID());
    redstone.push_back(TNTBlock::getID());

    items.push_back(Apple::getID());
    items.push_back(GoldenApple::getID());
    for (int i = WoodenPickaxe::getID(); i <= Scissors::getID(); i++)
        items.push_back(i);
    items.push_back(FlintAndSteel::getID());
    // The bow is a tool, but it cannot live in the pickaxe..scissors id range above: ids are
    // handed out by push order, so inserting one mid-range would renumber every item after it and
    // silently invalidate saved inventories. It gets the next free id instead and is listed here.
    items.push_back(Bow::getID());
    for (int i = LeatherHelmet::getID(); i <= GoldenBoots::getID(); i++)
        items.push_back(i);
    items.push_back(Clock::getID());
    items.push_back(Compass::getID());
    items.push_back(Saddle::getID());
    items.push_back(MooshroomBowl::getID());
    items.push_back(WhiteBread::getID());
    items.push_back(RawBeef::getID());
    items.push_back(Steak::getID());
    items.push_back(RawPorkchop::getID());
    items.push_back(CookedPorkchop::getID());
    items.push_back(RawMutton::getID());
    items.push_back(CookedMutton::getID());
    items.push_back(RawChicken::getID());
    items.push_back(CookedChicken::getID());
    items.push_back(Busket::getID());
    items.push_back(MilkBusket::getID());
    items.push_back(SnowBallItem::getID());
    items.push_back(Cookie::getID());
    items.push_back(CowSpawnEgg::getID());
    items.push_back(PigSpawnEgg::getID());
    items.push_back(SheepSpawnEgg::getID());
    items.push_back(ZombieSpawnEgg::getID());
    items.push_back(CreeperSpawnEgg::getID());
    items.push_back(ChickenSpawnEgg::getID());
    items.push_back(SpiderSpawnEgg::getID());
    items.push_back(SkeletonSpawnEgg::getID());
    // Last, so the debug stick sits at the end of the Items tab and is easy to page down to.
    items.push_back(DebugStick::getID());

    mats.push_back(Coal::getID());
    mats.push_back(DiamondItem::getID());
    mats.push_back(IronBar::getID());
    mats.push_back(GoldenBar::getID());
    mats.push_back(Stick::getID());
    mats.push_back(Bowl::getID());
    mats.push_back(WheatSeeds::getID());
    mats.push_back(Wheat::getID());
    mats.push_back(Leather::getID());
    mats.push_back(Brick::getID());
    mats.push_back(Clay::getID());
    mats.push_back(CaneItem::getID());
    mats.push_back(Paper::getID());
    mats.push_back(Book::getID());
    mats.push_back(Bone::getID());
    mats.push_back(BoneMeal::getID());
    mats.push_back(MelonSeeds::getID());
    mats.push_back(PumpkinSeeds::getID());
    mats.push_back(Feather::getID());
    mats.push_back(Egg::getID());
    mats.push_back(String::getID());
    mats.push_back(SpiderEye::getID());
    mats.push_back(Arrow::getID());
    mats.push_back(MelonItem::getID());
    mats.push_back(Potato::getID());
    mats.push_back(BakedPotato::getID());
    mats.push_back(PoisonedPotato::getID());
    mats.push_back(Carrot::getID());
    mats.push_back(Disk1::getID());
    mats.push_back(Disk2::getID());
    mats.push_back(Disk3::getID());
    mats.push_back(Disk4::getID());
    mats.push_back(Disk5::getID());
    mats.push_back(Disk6::getID());
    mats.push_back(Disk7::getID());
    mats.push_back(Disk8::getID());
    mats.push_back(Disk9::getID());
    mats.push_back(Disk10::getID());
    mats.push_back(Disk11::getID());
    mats.push_back(Disk12::getID());

    creativeTab = 0;
    creativePage = 0;
    RefreshCreativePageMax();
}


void StatePlayCreative::SetDayTimeAfterLoad()
{

}

void StatePlayCreative::Enter()
{
    RenderManager::InstancePtr()->SetPerspective(55.0f, 480.0f / 272.0f, 0.18f, 1000.0f);

    // Same as StatePlay::Enter: the title-screen art is dead weight while a world runs.
    TextureHelper::Instance()->ReleaseMenuTextures();
}

void StatePlayCreative::CleanUp()
{
    if(mSnowBalls.empty() == false)
    {
        for(unsigned int i = 0; i < mSnowBalls.size(); i++)
        {
            delete mSnowBalls[i];
        }
        mSnowBalls.clear();
    }

    for(int t = 0; t < kCreativeTabCount; t++)
        creativeTabItems[t].clear();

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
    for(int t = 0; t < kCreativeTabCount; t++)
        delete creativeTabSelSprite[t];
    delete crtSprite;
    delete chtSprite;
    delete selectInvSprite;
    delete sliderSprite;
    delete sliderLightedSprite;

    delete skyLight;
    delete skyMoonLight;
    delete skyBox;
    delete destroyer;
    delete starsBox;

    delete furSprite;
    delete waterScreen;

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

    delete mWorld;
}

void StatePlayCreative::Pause()
{

}

void StatePlayCreative::Resume()
{

}

int StatePlayCreative::FindChestId(int x, int y, int z)
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

int StatePlayCreative::FindFurnaceId(int x, int y, int z)
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

void StatePlayCreative::CheckForFurnFuel(Furnace* Fur)
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
		case 275:
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

void StatePlayCreative::CheckForFurnWorking(Furnace* Fur)
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

void StatePlayCreative::ReadyFurnSmelting(Furnace* Fur)
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

void StatePlayCreative::CraftItem2x2()
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

    // redstone torch: redstone dust above a stick (353 over 276). The 2x2 grid is column-major, so the
    // vertical pair is (0,2) or (1,3) -- the same pair the plain torch recipe uses with coal.
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
void StatePlayCreative::CraftItem3x3()
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

        // arrow: flint over stick over feather, vanilla's vertical column, four per craft. Checked
        // slot by slot because this switch keys on the sum of the grid, not the shape. Kept identical
        // to StatePlay::CraftItem3x3 -- crafting is forked across the two play states.
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

        // bow: three sticks and three string, vanilla's shape (string down the right column, sticks
        // curving down the middle and left). See StatePlay::CraftItem3x3 for the shape.
        case 2073:
            if(craftSlotId3[1] == Stick::getID() && craftSlotId3[3] == Stick::getID() &&
               craftSlotId3[7] == Stick::getID() &&
               craftSlotId3[2] == String::getID() && craftSlotId3[5] == String::getID() &&
               craftSlotId3[8] == String::getID())
            {
                craftItemId3 = Bow::getID();
                // A bow is a tool: not stackable, and its amount is its durability rather than a
                // count of one. Matches StatePlay::CraftItem3x3; see CraftWorld::FreshItemAmount.
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

        // redstone repeater: torch, dust, torch over a full row of stone -- the vanilla shape. Sum is
        // 3*3 + 2*408 + 353; the grid is row-major, so the torch row is either 0,1,2 over 3,4,5 or
        // 3,4,5 over 6,7,8. The flip is accepted because this codebase has no shaped-recipe mirroring.
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
                // The golden shovel's own durability, not the diamond shovel's 1562. Same fix as
                // StatePlay; see tools/check-tool-durability.py.
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
            // See the 2x2 grid's version of this recipe: slots 2 and 3 were joined with && instead of ||.
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

void StatePlayCreative::HandleEvents(StateManager* sManager)
{
    //update delta time
    dt = mTimer.GetDeltaTime();
    if(dt > 0.1f)
        dt = 0.1f;

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

    // While the debug menu is up it owns the input, exactly as in the survival state. The world is
    // held frozen, so nothing below this line has anything to do.
    if(debugMenu.IsOpen() == true)
    {
        fppCam->CancelLookEase();
        return;
    }
    if(menuState != 0 || invEn || craft3xEn || chestEn || furnaceEn)
        fppCam->CancelLookEase();

    sneaking = false;
    if(menuState == 0 && invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && sleepTime == 0.0f && !canFly)
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
            if((keyHold(InputHelper::Instance()->getButtonToAction(13)) || keyPressed(InputHelper::Instance()->getButtonToAction(13))) &&
               (keyHold(InputHelper::Instance()->getButtonToAction(14)) || keyPressed(InputHelper::Instance()->getButtonToAction(14))) &&
               (keyPressed(InputHelper::Instance()->getButtonToAction(13)) || keyPressed(InputHelper::Instance()->getButtonToAction(14))))
            {
                invEn = true;
                startDt = false;
                invXPosition = tempXInv;
                invYPosition = tempYInv;
                if(invYPosition > kCreativeHotbarRow)
                    invYPosition = kCreativeHotbarRow;
                creativeMouseFree = false;
                upEn = tempUpInv;
                UpdateInvCursorPos();
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
                        // Second tap inside the window: a double tap, so the held-back chat open is
                        // dropped and the view cycles instead.
                        chatTapTimer = 0.0f;
                        chatTapPending = false;
                        thirdPersonView = (thirdPersonView + 1) % 3;
                    }
                    else
                    {
                        // First tap. Chat cannot open yet -- the keyboard would eat the second tap.
                        chatTapTimer = CHAT_DOUBLE_TAP_WINDOW;
                        chatTapPending = true;
                    }
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
                // Same dt as movement, and the same ease as survival. The 0.05s cap here was the
                // console wobble: a long frame turned a fixed amount while walking kept going.
                lookDt = dt;
                fppCam->ApplyLook(lookYaw, lookPitch, dt);
                if(fppCam->lookRatePitch != 0.0f || fppCam->lookRateYaw != 0.0f)
                {
                    if(startDt == true)
                        dStd = -1;
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
            if(sneaking && !canFly)
                cameraMoveSpeed *= 0.3f;
            if(canFly)
            {
                // Double-tap Forward (pad Up) while flying = sprint.
                // Must release between taps. Base fly is deliberately slow so
                // sprint (+ FOV) is obvious.
                bool fwdHeld = keyHold(InputHelper::Instance()->getButtonToAction(0));
                if(flyForwardTimer > 0.0f)
                    flyForwardTimer -= dt;

                if(fwdHeld && !flyForwardWasHeld)
                {
                    if(flySprintArm && flyForwardTimer > 0.04f)
                    {
                        flySprint = true;
                        flySprintArm = false;
                        flyForwardTimer = 0.0f;
                    }
                    else
                    {
                        flyForwardPending = true;
                        flySprintArm = false;
                        flyForwardTimer = 0.35f;
                    }
                }
                if(!fwdHeld && flyForwardWasHeld)
                {
                    if(flyForwardPending && flyForwardTimer > 0.04f)
                        flySprintArm = true;
                    flyForwardPending = false;
                    flySprint = false;
                }
                flyForwardWasHeld = fwdHeld;
                if(!fwdHeld && flyForwardTimer <= 0.0f)
                    flySprintArm = false;

                // Java: fly ~2.5x walk, sprint-fly ~2x that. Scaled down a bit for PSP streaming.
                if(flySprint)
                    cameraMoveSpeed *= 2.8f;
                else
                    cameraMoveSpeed *= 1.4f;
            }
            else
            {
                flySprint = false;
                flyForwardTimer = 0.0f;
                flyForwardWasHeld = false;
                flyForwardPending = false;
                flySprintArm = false;
                // Not flying: drop any glide, so re-entering fly later cannot launch the player with a
                // stale velocity from the last time they were airborne.
                flyGlideVelocity = Vector3(0,0,0);

                // Ground sprint, on the same double tap of Forward. Creative has no hunger, so Java's
                // "food level above 6" gate does not apply and nothing is charged for it; the tap
                // pattern is identical to the fly sprint above and to survival's, deliberately. Wall
                // collision is the one Java condition this port cannot see, and it does not matter
                // here: pressed against a block you are not moving, so the animation just continues.
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

                if(sneaking)
                    sprinting = false;

                if(sprinting)
                    cameraMoveSpeed *= SPRINT_SPEED_MULT;
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

                // jumping/swiming / fly toggle
                    if(flyJumpTimer > 0.0f)
                    {
                        flyJumpTimer -= dt;
                    }
                    // Rising edge of Jump only. KeyPressed() is not per-button: any other
                    // pad change while Jump is held would look like a second jump tap.
                    bool jumpHeld = keyHold(InputHelper::Instance()->getButtonToAction(12));
                    if(jumpHeld && !flyJumpWasHeld)
                    {
                        // Java Minecraft: 7 ticks = 0.35s. Must be airborne so a
                        // normal land-and-jump does not toggle. Skip the first
                        // ~2 frames so a click bounce is not a second tap.
                        const float flyWindow = 0.35f;
                        if(flyJumpTimer > 0.0f && flyJumpTimer < (flyWindow - 0.04f) && walkingOnGround == false)
                        {
                            canFly = !canFly;
                            tickShowFlymodeMessage = 1.2f;
                            flyJumpTimer = 0.0f;
                            flyBoost = 1.0f;
                            flySprint = false;
                            flyForwardTimer = 0.0f;
                            flyForwardWasHeld = false;
                            flyForwardPending = false;
                            flySprintArm = false;
                            flyFovBoost = 0.0f;
                            playerVelocity.y = 0.0f;
                            jumping = false;
                        }
                        else
                        {
                            flyJumpTimer = flyWindow;
                        }
                    }
                    flyJumpWasHeld = jumpHeld;
                    if(keyHold(InputHelper::Instance()->getButtonToAction(12)) && !canFly)
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


            // switch left
            if(tickShowSlotName > -0.1f)
            {
                tickShowSlotName -= dt;
            }
            if(keyPressed(InputHelper::Instance()->getButtonToAction(9)))
            {
                if (mWorld->invId[27+barPosition] != -1)
                {
                    changeY = -0.16f;
                    anim[0] = 1;
                }
                barPosition != 8 ? barPosition ++ : barPosition = 0;

				selectSprite->SetPosition(108.0f + (11.0f + barPosition * 20.0f) * (264.0f / 182.0f), 230);

                tickShowSlotName = 1.25f;

                // reset destroying vars
                startDt = 0;
                dT = 0;
                dET = 0;
                dStd = -1;
            }

            //switch right
            if(keyPressed(InputHelper::Instance()->getButtonToAction(8)))
            {
                if (mWorld->invId[27+barPosition] != -1)
                {
                    changeY = -0.16f;
                    anim[0] = 1;
                }
                barPosition != 0 ? barPosition -- : barPosition = 8;

				selectSprite->SetPosition(108.0f + (11.0f + barPosition * 20.0f) * (264.0f / 182.0f), 230);

                tickShowSlotName = 1.25f;

                // reset destroying vars
                startDt = 0;
                dT = 0;
                dET = 0;
                dStd = -1;
            }

            //menu
            if(sleepTime == 0.0f)
            {
                if(keyPressed(InputHelper::Instance()->getButtonToAction(15)))
                {
                    menuState = 1;
                    menuOptions = false;
                    optionsMenuPos = 0;
                }
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

            // enable/disable fly mode (double-tap jump, like Minecraft creative)
            if(tickShowFlymodeMessage > -0.1f)
            {
                tickShowFlymodeMessage -= dt;
            }

            //add cube
            if(keyPressed(InputHelper::Instance()->getButtonToAction(14)))
            {
                // The debug stick, before anything else in this block -- creative is where testing
                // actually happens, so this is the state it matters most in. See
                // StatePlay::CheckDebugStickUse for why it is handled ahead of the block actions.
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
							invEn = true;

							invXPosition = tempXInv;
							invYPosition = tempYInv;
							if(invYPosition > kCreativeHotbarRow)
								invYPosition = kCreativeHotbarRow;
							creativeMouseFree = false;
							upEn = tempUpInv;
							UpdateInvCursorPos();

							return;
						}
                    }
                }
                if (startDt == true) //stop destroying
                {
                    startDt = false;
                }

                if (mWorld->invId[27+barPosition] == Bow::getID()) ///BOW: start the draw
                {
                    // Same split as the survival state: the press only starts the draw, and Update
                    // fires it when the button is let go (no keyReleased in this engine). Creative
                    // never needs an arrow, so this can always start.
                    //
                    // Only a *new* draw, as in the survival state, for the same reason: KeyPressed
                    // also reports a key that is merely still down whenever any other button moved,
                    // and turning uses face buttons, so this ran on every frame of a turn and reset
                    // the half-filled draw. See StatePlay.cpp.
                    if(bowCharging == false)
                    {
                        bowCharging = true;
                        bowCharge = 0.0f;
                    }
                    return;
                }

                if (mWorld->invId[27+barPosition] == 299) ///SNOWBALL THROWING
                {
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

                //move to the target +=0.5
                // One iteration per cell the look crosses, each one clipped against its selection box:
                // see the long note at this loop in StatePlay.cpp. The box is the selection shape
                // because that is what the crosshair outlines, and for redstone wire it is a 1 px plate
                // -- too thin for a fixed march step or a point-in-box test to be reliable.
                Ray pickRay(fppCam->m_vPosition, rayDir);

                for(float i = 0.0f; i < 5.25f; i = CraftWorld::NextCellDistance(fppCam->m_vPosition, rayDir, i))
                {
                    testPos = fppCam->m_vPosition + (rayDir * i);

                    collider = mWorld->GetBlockSelectionBox(testPos.x,testPos.y,testPos.z, mWorld->GetBlock(testPos.x,testPos.y,testPos.z));
                    testBox = BoundingBox(floorf(testPos.x)+0.5f+collider.min.x, floorf(testPos.y)+0.5f+collider.min.y, floorf(testPos.z)+0.5f+collider.min.z, floorf(testPos.x)+0.5f+collider.max.x, floorf(testPos.y)+0.5f+collider.max.y, floorf(testPos.z)+0.5f+collider.max.z);

                    int selectedBlock = mWorld->GetBlock(testPos.x, testPos.y, testPos.z);

                    if(mWorld->invId[27+barPosition] == WaterBusket::getID())
                    {
                        if(mWorld->BlockIsLava(selectedBlock))
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
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = (selectedBlock == Lava::getID()) ? Obsidian::getID() : CobbleStone::getID();

                                mSoundMgr->PlayFissSound();

                                int	curchunkTarget = mWorld->getChunkId(testPos);
                                mWorld->RebuildFullMeshChunk(curchunkTarget);
                                fppCam->needUpdate = true;

                                changeY = -0.16f;
                                anim[0] = 1;
                                return;
                            }
                        }
                    }

                    if(mWorld->invId[27+barPosition] == LavaBusket::getID())
                    {
                        if(mWorld->BlockIsWater(selectedBlock))
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
                            if((mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z) == 4 && mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z-1) == 4 && mWorld->GetBlock(testPos.x, testPos.y, testPos.z-1) == 4) || (mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z) == 4 && mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z-1) == 4 && mWorld->GetBlock(testPos.x, testPos.y, testPos.z-1) == 4) || (mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z) == 4 && mWorld->GetBlock(testPos.x-1, testPos.y, testPos.z+1) == 4 && mWorld->GetBlock(testPos.x, testPos.y, testPos.z+1) == 4) || (mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z) == 4 && mWorld->GetBlock(testPos.x+1, testPos.y, testPos.z+1) == 4 && mWorld->GetBlock(testPos.x, testPos.y, testPos.z+1) == 4))
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
                        if(mWorld->invId[27+barPosition] >= 270 && mWorld->invId[27+barPosition] <= 274) //hoes
                        {
                            if (selectedBlock == 1) //if it is grass
                            {
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = 79;
                                if(rand() % 10 == 1)
                                {
                                    //mWorld->DropThis(286,1,true,testPos); //put seed in inventory
                                }

                                mWorld->invAm[27+barPosition] -= 1; //bit break to our hoe
                                if(mWorld->invAm[27+barPosition] == 0) //if it has 0 of solid we jut destroy it
                                {
                                    mWorld->invAm[27+barPosition] = -1;
                                    mWorld->invId[27+barPosition] = -1;
                                    mWorld->invSt[27+barPosition] = 0;

                                    mSoundMgr->PlayBreakSound();
                                }
                                int	curchunkTarget = mWorld->getChunkId(testPos);

                                fppCam->needUpdate = true;
                                //Rebuild nearby world
                                mWorld->RebuildOpaqueMeshChunk(curchunkTarget);
                                mSoundMgr->PlayWalkSound(1);
                                changeY = -0.16f;
                                anim[0] = 1;
                                //mWorld->rebuildTransparentChunk(curchunkTarget);
                                return;
                            }

                            if (selectedBlock == 2) //if it is dirt
                            {
                                mWorld->mainStatistics.soilPlowed += 1;
                                mWorld->GetBlock(testPos.x, testPos.y, testPos.z) = 79;

                                mWorld->invAm[27+barPosition] -= 1;
                                if(mWorld->invAm[27+barPosition] == 0) //if it has 0 of solid we jut destroy it
                                {
                                    mWorld->invAm[27+barPosition] = -1;
                                    mWorld->invId[27+barPosition] = -1;
                                    mWorld->invSt[27+barPosition] = 0;

                                    mSoundMgr->PlayBreakSound();
                                }

                                int	curchunkTarget = mWorld->getChunkId(testPos);

                                fppCam->needUpdate = true;
                                //Rebuild nearby world
                                mWorld->RebuildOpaqueMeshChunk(curchunkTarget);
                                mSoundMgr->PlayWalkSound(1);
                                changeY = -0.16f;
                                anim[0] = 1;
                                //mWorld->rebuildTransparentChunk(curchunkTarget);
                                return;
                            }
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
                                fppCam->needUpdate = true;
                                mWorld->RebuildTransparentMeshChunk(curchunkTarget);
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

                            invXPosition = tempXCT;
                            invYPosition = tempYCT;
                            upEn = tempUpCT;

UpdateInvCursorPos();

                            return;
                        }

                        // half block + half block = full block
                        if (mWorld->blockTypes[selectedBlock].blockModel == 1 &&
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

                                invXPosition = tempXFurnace;
                                invYPosition = tempYFurnace;
                                upEn = tempUpFurnace;

UpdateInvCursorPos();

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

                        BoundingBox testBox2 = BoundingBox(Vector3(cubePos.x - 0.5f,cubePos.y - 0.5f,cubePos.z - 0.5f),Vector3(cubePos.x + 0.5f,cubePos.y + 0.5f,cubePos.z + 0.5f));
                        Ray tesRay = Ray(fppCam->m_vPosition,rayDir);

                        bool wasUpdated = false;

                        float distance = -1.0f;
                        tesRay.hasIntersected(testBox2,distance);

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

                            int newPosBlockId = mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z);

                            //check if this block is empty
                            if(mWorld->BlockIsLava(newPosBlockId) || mWorld->BlockIsWater(newPosBlockId) || newPosBlockId == 0 || newPosBlockId == 93)
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

                                        if(newPosBlockId == Lava::getID())
                                        {
                                            wasLight = true;
                                            mWorld->RemoveLigtSourceAtPosition(testPos2.x,testPos2.y,testPos2.z,112);
                                            mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 0;
                                        }

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

                                        if (IsBlockId(mWorld->invId[27+barPosition])) // if block in our hands
                                        {

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

                                                    case 125: // oak stair
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
                                        else //if item in our hand
                                        {
                                            if(mWorld->IsSpawnEgg(mWorld->invId[27+barPosition]))
                                            {
                                                mWorld->SpawnFromEgg(mWorld->invId[27+barPosition], testPos2.x+0.5f, testPos2.y, testPos2.z+0.5f);
                                                animGo = true;
                                                animDest = 0.0f;
                                                return;
                                            }
                                            if(mWorld->invId[27+barPosition] == TorchItem::getID()) // if it is torch
                                            {
                                                float odx = testPos2.x - testPos.x;
                                                float ody = testPos2.y - testPos.y;
                                                float odz = testPos2.z - testPos.z;
                                                int torchFace = -1;
                                                if(ody > 0.5f)
                                                    torchFace = 0;
                                                else if(ody < -0.5f)
                                                    torchFace = -1;
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
                                                // Same mount rule as the plain torch; the id also carries the
                                                // lit/unlit family, so build it lit and let the torch decide.
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

                                            if(mWorld->invId[27+barPosition] == 286) // if it is seeds
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
                                                }
                                                else
                                                {
                                                    return;
                                                }
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
                                                }
                                                else
                                                {
                                                    return;
                                                }
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

											/*if(mWorld->invId[27+barPosition] == DPurpleGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = GlassPanel::getID();//DPurpleGlassBlockPannel::getID();

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
                                            }*/

											if(mWorld->invId[27+barPosition] == BlueGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = BlueGlassBlockPannel::getID();//BlueGlassBlockPannel::getID();

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

											/*if(mWorld->invId[27+barPosition] == LBlueGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = GlassPanel::getID();//BlueGlassBlockPannel::getID();

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
                                            }*/

											/*if(mWorld->invId[27+barPosition] == DBlueGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = GlassPanel::getID();//DBlueGlassBlockPannel::getID();

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

											if(mWorld->invId[27+barPosition] == BrownGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = BrownGlassBlockPannel::getID();//BrownGlassBlockPannel::getID();

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

											if(mWorld->invId[27+barPosition] == WhiteGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = WhiteGlassBlockPannel::getID();//WhiteGlassBlockPannel::getID();

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

											if(mWorld->invId[27+barPosition] == GrayGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = GrayGlassBlockPannel::getID();//GrayGlassBlockPannel::getID();

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

											if(mWorld->invId[27+barPosition] == LGrayGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = GlassPanel::getID();//LGrayGlassBlockPannel::getID();

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
                                            }*/

											if(mWorld->invId[27+barPosition] == BlackGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = BlackGlassBlockPannel::getID();//BlackGlassBlockPannel::getID();

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

											/*if(mWorld->invId[27+barPosition] == DGreenGlassBlockPannelItem::getID()) // if it is glass panel
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
                                            }*/

											if(mWorld->invId[27+barPosition] == RedGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = RedGlassBlockPannel::getID();

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

											/*if(mWorld->invId[27+barPosition] == OrangeGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = OrangeGlassBlockPannel::getID();

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
                                            }*/
											
											if(mWorld->invId[27+barPosition] == PurpleGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = PurpleGlassBlockPannel::getID();//PurpleGlassBlockPannel::getID();

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

											if(mWorld->invId[27+barPosition] == PinkGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = PinkGlassBlockPannel::getID();//PinkGlassBlockPannel::getID();

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

											if(mWorld->invId[27+barPosition] == GreenGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = GreenGlassBlockPannel::getID();//GreenGlassBlockPannel::getID();

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

											if(mWorld->invId[27+barPosition] == YellowGlassBlockPannelItem::getID()) // if it is glass panel
                                            {
                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = YellowGlassBlockPannel::getID();//YellowGlassBlockPannel::getID();

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

                                            if(mWorld->invId[27+barPosition] == IronBarsItem::getID()) // if it is iron bars
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

                                            if(mWorld->invId[27+barPosition] >= 294 && mWorld->invId[27+barPosition] <= 296) // if it is flowers or saplings
                                            {
                                                if(mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 1 || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 2 || mWorld->GetBlock(testPos2.x,testPos2.y-1,testPos2.z) == 29) // if under is dirt or grass
                                                {
                                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 119 + (mWorld->invId[27+barPosition] - 294);
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

                                            if(mWorld->invId[27+barPosition] == ItemFrameItem::getID()) // if it is item frame
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
                                                mWorld->RebuildTransparentMeshChunk(chunkTarget);
                                                mWorld->rebuildNearestChunks(chunkTarget,testPos2);
                                                if(chunkTarget >= 0 && chunkTarget < (int)mWorld->mChunks.size())
                                                {
                                                    if(mWorld->mChunks[chunkTarget]->periodicallyUpadted == false)
                                                    {
                                                        mWorld->updatingChunks.push_back(chunkTarget);
                                                        mWorld->mChunks[chunkTarget]->periodicallyUpadted = true;
                                                    }
                                                }
                                                fppCam->needUpdate = true;
                                            }
                                            if(mWorld->invId[27+barPosition] == 292) // if it is lava busket
                                            {
                                                int chunkTarget = mWorld->getChunkId(testPos2);

                                                mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 112;
                                                wasLight = true;
                                                mWorld->SetLigtSourcePosition(testPos2.x,testPos2.y,testPos2.z,Lava::getID());

                                                fppCam->needUpdate = true;

                                                if(chunkTarget >= 0 && chunkTarget < (int)mWorld->mChunks.size() && mWorld->mChunks[chunkTarget]->periodicallyUpadted == false)
                                                {
                                                    mWorld->updatingChunks.push_back(chunkTarget);
                                                    mWorld->mChunks[chunkTarget]->periodicallyUpadted = true;
                                                }
                                            }
                                        }

                                        animGo = true;
                                        animDest = 0.0f;

                                        mWorld->mainStatistics.blockPlaced += 1;

                                        if(IsBlockId(oldBlock) && oldBlock != -1)
                                        {
                                            mSoundMgr->PlayWalkSound(mWorld->blockTypes[oldBlock].soundType);
                                        }

                                        int chunkTarget = mWorld->getChunkId(testPos2);

                                        //rebuild
                                        if(wasLight == true)
                                        {
                                            if(!wasUpdated)
                                            {
                                                mWorld->UpdateLightAreaIn(testPos2);

                                                mWorld->RebuildFullMeshChunk(chunkTarget);
                                                mWorld->rebuildNearestChunksForLight(chunkTarget,testPos2);
                                            }
                                        }
                                        else
                                        {
                                            mWorld->initPutBlocksLight(testPos2.x, testPos2.y+1, testPos2.z);

                                            if(!wasUpdated)
                                            {
                                                if(mWorld->GetLightLevel(testPos2.x,testPos2.y,testPos2.z) > 0)
                                                {
                                                    mWorld->UpdateLightAreaIn(testPos2);
                                                }

                                                mWorld->RebuildFullMeshChunk(chunkTarget);
                                                if(mWorld->BlockTransparent(testPos2.x,testPos2.y,testPos2.z) || mWorld->blockTypes[mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z)].blockModel == 2)
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
            }

			/*if(keyPressed(InputHelper::Instance()->getButtonToAction(13)) || tryToDt == true)
            {
                tryToDt = false;

                //remove cube
                Vector3 rayDir = fppCam->m_vView - fppCam->m_vPosition;
                rayDir.normalize();

                //we are takin our positiona and view vector
                Vector3 testPos2;
                testPos2 = Vector3(-1,-1,-1);

                BoundingBox collider = mWorld->GetBlockCollideBox(testPos2.x,testPos2.y,testPos2.z, mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z));
                BoundingBox testBox = BoundingBox(testPos2.x+collider.min.x, testPos2.y+collider.min.y, testPos2.z+collider.min.z, testPos2.x+collider.max.x, testPos2.y+collider.max.y, testPos2.z+collider.max.z);

                //we are moving slowly to the target +=0.5
                // The punch reads the same box the crosshair does -- the *selection* shape, so a 1 px
                // plate is hit when it is outlined -- clipped as the look crosses each cell, one step
                // per cell wall. See StatePlay.cpp's placement loop.
                Ray punchRay(fppCam->m_vPosition, rayDir);

                for(float i = 0.0f; i < 5.25f; i = CraftWorld::NextCellDistance(fppCam->m_vPosition, rayDir, i))
                {
                    testPos2 = fppCam->m_vPosition + (rayDir * i);

                    collider = mWorld->GetBlockSelectionBox(testPos2.x,testPos2.y,testPos2.z, mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z));
                    testBox = BoundingBox(floorf(testPos2.x)+0.5f+collider.min.x, floorf(testPos2.y)+0.5f+collider.min.y, floorf(testPos2.z)+0.5f+collider.min.z, floorf(testPos2.x)+0.5f+collider.max.x, floorf(testPos2.y)+0.5f+collider.max.y, floorf(testPos2.z)+0.5f+collider.max.z);

                    bool kick = false;
                    if(mWorld->mZombies.empty() == false)
                    {
                        for(int f = 0; f < mWorld->mZombies.size(); f++)
                        {
                            Zombie *TestZombie = mWorld->mZombies[f];
                            if(TestZombie->DistanceToPlayer() < 3.16f && TestZombie->kicked == false && TestZombie->damaged == false)
                            {
                                if(TestZombie->bBox.contains(testPos2))
                                {
                                    float damage = 1.0f;
                                    bool weapon = false;

                                    damage = mWorld->FetchDamagePoints(mWorld->invId[27+barPosition], weapon);

                                    if(weapon == true)
                                    {
                                        mWorld->invAm[27+barPosition] -= 1+rand()%2;

                                        if(mWorld->invAm[27+barPosition] <= 0)
                                        {
                                            mWorld->invId[27+barPosition] = -1;
                                            mWorld->invAm[27+barPosition] = -1;
                                            mWorld->invSt[27+barPosition] = false;

                                            mSoundMgr->PlayBreakSound();
                                        }
                                    }

                                    TestZombie->TakeDamage(damage,0.75f,dt);
                                    kick = true;

                                    animGo = true;
                                    animDest = 0.0f;

                                    changeY = -0.1f;
                                    anim[0] = 1;
                                    break;
                                }
                            }
                        }
                        if(kick == true)
                        {
                            return;
                        }
                    }


                    if(mWorld->mCows.empty() == false)
                    {
                        for(int f = 0; f < mWorld->mCows.size(); f++)
                        {
                            Cow *TestCow = mWorld->mCows[f];
                            if(TestCow->DistanceToPlayer() < 2.75 && TestCow->kicked == false && TestCow->damaged == false)
                            {
                                if(TestCow->bBox.contains(testPos2))
                                {
                                    float damage = 1.0f;
                                    bool weapon = false;

                                    damage = mWorld->FetchDamagePoints(mWorld->invId[27+barPosition], weapon);

                                    if(weapon == true)
                                    {
                                        mWorld->invAm[27+barPosition] -= 1+rand()%2;

                                        if(mWorld->invAm[27+barPosition] <= 0)
                                        {
                                            mWorld->invId[27+barPosition] = -1;
                                            mWorld->invAm[27+barPosition] = -1;
                                            mWorld->invSt[27+barPosition] = false;

                                            mSoundMgr->PlayBreakSound();
                                        }
                                    }

                                    TestCow->TakeDamage(damage,0.7f,dt);
                                    kick = true;

                                    animGo = true;
                                    animDest = 0.0f;
                                    break;
                                }
                            }
                        }
                        if(kick == true)
                        {
                            return;
                        }
                    }


                    if(mWorld->mSheeps.empty() == false)
                    {
                        for(int f = 0; f < mWorld->mSheeps.size(); f++)
                        {
                            Sheep *TestSheep = mWorld->mSheeps[f];
                            if(TestSheep->DistanceToPlayer() < 2.75 && TestSheep->kicked == false && TestSheep->damaged == false)
                            {
                                if(TestSheep->bBox.contains(testPos2))
                                {
                                    float damage = 1.0f;
                                    bool weapon = false;

                                    damage = mWorld->FetchDamagePoints(mWorld->invId[27+barPosition], weapon);

                                    if(weapon == true)
                                    {
                                        mWorld->invAm[27+barPosition] -= 1+rand()%2;

                                        if(mWorld->invAm[27+barPosition] <= 0)
                                        {
                                            mWorld->invId[27+barPosition] = -1;
                                            mWorld->invAm[27+barPosition] = -1;
                                            mWorld->invSt[27+barPosition] = false;

                                            mSoundMgr->PlayBreakSound();
                                        }
                                    }

                                    TestSheep->TakeDamage(damage,0.7f,dt);
                                    kick = true;

                                    animGo = true;
                                    animDest = 0.0f;
                                    break;
                                }
                            }
                        }
                        if(kick == true)
                        {
                            return;
                        }
                    }


                    if(mWorld->mCreepers.empty() == false)
                    {
                        for(int f = 0; f < mWorld->mCreepers.size(); f++)
                        {
                            Creeper *TestCreeper = mWorld->mCreepers[f];
                            if(mWorld->FastDistance2d(abs(TestCreeper->position.x-playerPosition.x)*10,abs(TestCreeper->position.z-playerPosition.z)*10)/10.0f + abs(TestCreeper->position.y-(playerPosition.y-0.7))/2.0f < 2.18 && TestCreeper->kicked == false && TestCreeper->damaged == false)
                            {
                                if(TestCreeper->bBox.contains(testPos2))
                                {
                                    float damage = 1.0f;
                                    bool weapon = false;

                                    damage = mWorld->FetchDamagePoints(mWorld->invId[27+barPosition], weapon);

                                    if(weapon == true)
                                    {
                                        mWorld->invAm[27+barPosition] -= 1+rand()%2;

                                        if(mWorld->invAm[27+barPosition] <= 0)
                                        {
                                            mWorld->invId[27+barPosition] = -1;
                                            mWorld->invAm[27+barPosition] = -1;
                                            mWorld->invSt[27+barPosition] = false;

                                            mSoundMgr->PlayBreakSound();
                                        }
                                    }

                                    TestCreeper->TakeDamage(damage,0.7f,dt);
                                    kick = true;

                                    animGo = true;
                                    animDest = 0.0f;
                                    break;
                                }
                            }
                        }
                        if(kick == true)
                        {
                            return;
                        }
                    }
					//check if we touch something
                    float punchDist = -1.0f;
                    punchRay.hasIntersected(testBox, punchDist);

                    if(mWorld->BlockEditable(testPos2.x,testPos2.y,testPos2.z) && punchDist != -1.0f)
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
                            
                        }
                    }
                }
            }*/
            if(keyPressed(InputHelper::Instance()->getButtonToAction(13))) //remove cube
			{
				if(keyHold(InputHelper::Instance()->getButtonToAction(13)))
				{
                    Vector3 rayDirPunch = fppCam->m_vView - fppCam->m_vPosition;
                    rayDirPunch.normalize();
                    Vector3 rayOrigin = fppCam->m_vPosition;
                    float punchReach = 5.0f;
                    float nearestMobT = punchReach + 1.0f;
                    float punchPower = 0.7f;
                    LameMob* punchMob = FindMobAlongRay(mWorld, rayOrigin, rayDirPunch, punchReach, &nearestMobT, &punchPower);
                    if(punchMob != NULL)
                    {
                        float nearestBlockT = punchReach + 1.0f;
                        for(float i = 0; i < punchReach; i+=0.15f)
                        {
                            Vector3 p = rayOrigin + (rayDirPunch * i);
                            BoundingBox colliderP = mWorld->GetBlockCollideBox(p.x,p.y,p.z, mWorld->GetBlock(p.x,p.y,p.z));
                            BoundingBox testBoxP = BoundingBox(floorf(p.x)+0.5f+colliderP.min.x, floorf(p.y)+0.5f+colliderP.min.y, floorf(p.z)+0.5f+colliderP.min.z, floorf(p.x)+0.5f+colliderP.max.x, floorf(p.y)+0.5f+colliderP.max.y, floorf(p.z)+0.5f+colliderP.max.z);
                            if(mWorld->BlockEditable(p.x,p.y,p.z) && testBoxP.contains(p))
                            {
                                nearestBlockT = i;
                                break;
                            }
                        }
                        if(nearestMobT <= nearestBlockT)
                        {
                            ApplyMeleeHit(mWorld, mSoundMgr, barPosition, punchMob, punchPower, dt, false);
                            animGo = true;
                            animDest = 0.0f;
                            return;
                        }
                    }

					mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
					// (chunkTarget removed: computed and never read)

					Vector3 rayDir = fppCam->m_vView - fppCam->m_vPosition;
					rayDir.normalize();

					//we are takin our position and view vector
					Vector3 testPos2;
					testPos2 = Vector3(-1,-1,-1);

					BoundingBox collider = mWorld->GetBlockCollideBox(testPos2.x,testPos2.y,testPos2.z, mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z));
					BoundingBox testBox = BoundingBox(testPos2.x+collider.min.x, testPos2.y+collider.min.y, testPos2.z+collider.min.z, testPos2.x+collider.max.x, testPos2.y+collider.max.y, testPos2.z+collider.max.z);

					//we are moving slowly to the target +=0.5
					// One clip per cell the look crosses, the same pick the other loops take.
					Ray breakRay(fppCam->m_vPosition, rayDir);

					for(float i = 0.0f; i < 5.25f; i = CraftWorld::NextCellDistance(fppCam->m_vPosition, rayDir, i))
					{
						testPos2 = fppCam->m_vPosition + (rayDir * i);

						collider = mWorld->GetBlockSelectionBox(testPos2.x,testPos2.y,testPos2.z, mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z));
						testBox = BoundingBox(floorf(testPos2.x)+0.5f+collider.min.x, floorf(testPos2.y)+0.5f+collider.min.y, floorf(testPos2.z)+0.5f+collider.min.z, floorf(testPos2.x)+0.5f+collider.max.x, floorf(testPos2.y)+0.5f+collider.max.y, floorf(testPos2.z)+0.5f+collider.max.z);

						float breakDist = -1.0f;
						breakRay.hasIntersected(testBox, breakDist);

						if(mWorld->BlockEditable(testPos2.x,testPos2.y,testPos2.z) && breakDist != -1.0f)
						{
							testPos1.x  = testPos2.x;
							testPos1.y  = testPos2.y;
							testPos1.z  = testPos2.z;

							dT = 0;
							startDt = true;
							bool wasLight = false;
							int oldBlock = 0;
							oldBlock = mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z);

							if(mWorld->LightSourceBlock(oldBlock))//if it's light block
							{
								wasLight = true;
							}

							if(oldBlock >= ChestBlock1::getID() && oldBlock <= ChestBlock4::getID()) // if it is chest
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

							if(oldBlock == FurnaceOff::getID() || oldBlock == FurnaceOn::getID())
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

                        if(IsItemId(mWorld->invId[27+barPosition]) && mWorld->invId[27+barPosition] <= 275)
                        {
                            mWorld->invAm[27+barPosition] -= 1;

                            if(mWorld->invAm[27+barPosition] == 0)
                            {
                                Vector3 rayParticleDir = fppCam->m_vView - fppCam->m_vPosition;
                                rayParticleDir.normalize();

                                Vector3 particlePos = fppCam->m_vPosition + (rayParticleDir * 0.38f);

                                if(Particle::ActiveCount() <= 52)
                                {
                                    for(int k = 0; k <= 5; k++) // 5 particles
                                    {
                                        Particle* ToolPart = Particle::Spawn(mWorld,"item crack",Vector3(particlePos.x,particlePos.y-0.18f,particlePos.z));
                                        ToolPart->var1 = mWorld->invId[27+barPosition]-250;
                                    }
                                }

                                mWorld->invId[27+barPosition] = -1;
                                mWorld->invAm[27+barPosition] = -1;
                                mWorld->invSt[27+barPosition] = 0;
                                mSoundMgr->PlayBreakSound();
                            }
                        }

                        // Breaking half a piston takes the other half with it: an extended base would
                        // otherwise leave its head standing in the next cell with nothing to retract
                        // it, and a broken head would leave a base stuck open. This runs *before* the
                        // cell is emptied below, because the direction it needs lives in the state
                        // nibble that ClearStateCellNibble is about to zero.
                        mWorld->BreakPistonPartner((int)testPos1.x,(int)testPos1.y,(int)testPos1.z);

                        if(oldBlock >= 57 && oldBlock <= 58) // if it is door
                        {
                            mWorld->GetBlock(testPos1.x,testPos1.y,testPos1.z) = 0;
                            mWorld->GetBlock(testPos1.x,testPos1.y-1,testPos1.z) = 0;
                        }

                        if(oldBlock >= 49 && oldBlock <= 56) // if it is door
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

                                    mWorld->GetBlock(testPos2.x,testPos2.y,testPos2.z) = 0;
                                }
                            }
                        }

                        mWorld->mainStatistics.blockDestroyed += 1;

                        if(oldBlock == GlassBlock::getID() || oldBlock == IceBlock::getID() || oldBlock == GlowstoneBlock::getID() || oldBlock == GlassPanel::getID() || oldBlock == YellowGlassBlock::getID() || oldBlock == GreenGlassBlock::getID() || oldBlock == PinkGlassBlock::getID())
                        {
                            mSoundMgr->PlayGlassSound();
                        }
                        else
                        {
                            mSoundMgr->PlayEndDigSound(mWorld->blockTypes[oldBlock].soundType);
                        }

						if (oldBlock == BlueGlassBlockPannel::getID() || oldBlock == BlackGlassBlockPannel::getID() || oldBlock == RedGlassBlockPannel::getID() || oldBlock == PurpleGlassBlockPannel::getID() || oldBlock == PinkGlassBlockPannel::getID() || oldBlock == GreenGlassBlockPannel::getID() || oldBlock == YellowGlassBlockPannel::getID())
						{
							mSoundMgr->PlayGlassSound();
						}
						else
						{
							mSoundMgr->PlayEndDigSound(mWorld->blockTypes[oldBlock].soundType);
						}

						if(oldBlock == PurpleGlassBlock::getID() /*|| oldBlock == OrangeGlassBlock::getID()*/ || oldBlock == RedGlassBlock::getID() /*|| oldBlock == DGreenGlassBlock::getID() */|| oldBlock == BlackGlassBlock::getID() /*|| oldBlock == LGrayGlassBlock::getID()*/)
                        {
                            mSoundMgr->PlayGlassSound();
                        }
                        else
                        {
                            mSoundMgr->PlayEndDigSound(mWorld->blockTypes[oldBlock].soundType);
                        }

						if(/*oldBlock == WhiteGlassBlock::getID() || oldBlock == BrownGlassBlock::getID() /*|| oldBlock == DBlueGlassBlock::getID() || oldBlock == LBlueGlassBlock::getID() ||*/ oldBlock == BlueGlassBlock::getID() /*|| oldBlock == DPurpleGlassBlock::getID()*/)
                        {
                            mSoundMgr->PlayGlassSound();
                        }
                        else
                        {
                            mSoundMgr->PlayEndDigSound(mWorld->blockTypes[oldBlock].soundType);
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

                        animGo = true;
                        animDest = 0.0f;

                        fppCam->needUpdate = true;
                        testPos1 = Vector3(-1,-1,-1);
                        startDt = false;
                        chestId = -1;
                        break;
                    }
                }
            }
        }
	}
        /// INVENTORY SECTOR
        if(invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true)
        {
            if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
            {
                if(invEn == true)
                {
                    tempXInv = invXPosition;
                    tempYInv = invYPosition;
                    tempUpInv = upEn;
                }

                if(furnaceEn == true)
                {
                    tempXFurnace = invXPosition;
                    tempYFurnace = invYPosition;
                    tempUpFurnace = upEn;
                }

                if(chestEn == true)
                {
                    tempXChest = invXPosition;
                    tempYChest = invYPosition;
                    tempUpChest = upEn;
                }

                if(craft3xEn == true)
                {
                    tempXCT = invXPosition;
                    tempYCT = invYPosition;
                    tempUpCT = upEn;

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
                }

                invXPosition = 0;
                invYPosition = 0;

                upEn = false;

                invEn = false;
                craft3xEn = false;
                furnaceEn = false;
                chestEn = false;

                furnaceEn = false;

                chestId = -1;

                UseChest = 0;
                UseFurnace = 0;

                mWorld->haveCompass = mWorld->HaveItemInBarSlots(Compass::getID());

                return;
            }

        if(invEn == true)
        {
            usingSlider = false;
            if(mSystemMgr->KeyPressed(PSP_CTRL_LTRIGGER))
            {
                SetCreativeTab(creativeTab - 1);
                return;
            }
            if(mSystemMgr->KeyPressed(PSP_CTRL_RTRIGGER))
            {
                SetCreativeTab(creativeTab + 1);
                return;
            }
            CreativeUpdateAnalogMouse();
        }

        if(keyPressed(InputHelper::Instance()->getButtonToAction(14))) // if you press R
        {
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
                if(invEn)
                    creativeMouseFree = false;
                invXPosition != 8 ? invXPosition ++ : invXPosition = 8;
                UpdateInvCursorPos();
            }
            else
            {
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
                if(invEn)
                    creativeMouseFree = false;
                invXPosition != 0 ? invXPosition -- : invXPosition = 0;
                UpdateInvCursorPos();
            }
            else
            {
                invXPosition != 0 ? invXPosition -- : invXPosition = 0;
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
                if(invEn == false)
                {
                    invYPosition != 3 ? invYPosition ++ : invYPosition = 3;
                    UpdateInvCursorPos();
                }
                else
                {
                    creativeMouseFree = false;
                    if(invYPosition < kCreativeCatRows - 1)
                        invYPosition++;
                    else if(invYPosition == kCreativeCatRows - 1)
                    {
                        if(creativePage < creativePageMax)
                            creativePage++;
                        else
                            invYPosition = kCreativeHotbarRow;
                    }
                    UpdateInvCursorPos();
                }
            }
            else
            {
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
                if(invEn == true)
                {
                    creativeMouseFree = false;
                    if(invYPosition == kCreativeHotbarRow)
                        invYPosition = kCreativeCatRows - 1;
                    else if(invYPosition == 0)
                    {
                        if(creativePage > 0)
                            creativePage--;
                    }
                    else
                        invYPosition--;
                    UpdateInvCursorPos();
                    return;
                }
                if (invYPosition == 0 && invEn == false)
                {
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

                if(invEn == false)
                {
                    UpdateInvCursorPos();
                }
                else
                {
                    UpdateInvCursorPos();
                }
            }
            else
            {
                invYPosition != 0 ? invYPosition -- : invYPosition = 0;

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
                if(invEn == false)
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
                    if (invYPosition == kCreativeHotbarRow)
                    {
                        int slot = 27 + invXPosition;
                        if (mWorld->mId == -1)
                        {
                            if (mWorld->invId[slot] != -1)
                            {
                                mWorld->mId = mWorld->invId[slot];
                                mWorld->mAm = mWorld->invAm[slot];
                                mWorld->mSt = mWorld->invSt[slot];
                                mWorld->invId[slot] = -1;
                                mWorld->invAm[slot] = -1;
                                mWorld->invSt[slot] = 0;
                                creativeHeldFromHotbar = true;
                            }
                        }
                        else
                        {
                            if (mWorld->invId[slot] == -1)
                            {
                                mWorld->invId[slot] = mWorld->mId;
                                mWorld->invAm[slot] = mWorld->mAm;
                                mWorld->invSt[slot] = mWorld->mSt;
                                CreativeClearCursor(mWorld);
                                creativeHeldFromHotbar = false;
                            }
                            else if (mWorld->invId[slot] == mWorld->mId && mWorld->invSt[slot] == 1)
                            {
                                if (mWorld->invAm[slot]+mWorld->mAm < 64)
                                {
                                    mWorld->invAm[slot]+=mWorld->mAm;
                                    CreativeClearCursor(mWorld);
                                    creativeHeldFromHotbar = false;
                                }
                                else
                                {
                                    mWorld->mAm = (mWorld->invAm[slot] + mWorld->mAm) - 64;
                                    mWorld->invAm[slot]=64;

                                    if(mWorld->mAm == 0)
                                    {
                                        CreativeClearCursor(mWorld);
                                        creativeHeldFromHotbar = false;
                                    }
                                }
                            }
                            else
                            {
                                short tId = mWorld->invId[slot];
                                short tAm = mWorld->invAm[slot];
                                bool  tSt = mWorld->invSt[slot];
                                mWorld->invId[slot] = mWorld->mId;
                                mWorld->invAm[slot] = mWorld->mAm;
                                mWorld->invSt[slot] = mWorld->mSt;
                                mWorld->mId = tId;
                                mWorld->mAm = tAm;
                                mWorld->mSt = tSt;
                                creativeHeldFromHotbar = (mWorld->mId != -1);
                            }
                        }
                    }
                    else
                    {
                        unsigned int catalogIndex = (unsigned int)((creativePage + invYPosition) * 9 + invXPosition);
                        int catalogId = -1;
                        if (catalogIndex < CreativeItems().size())
                            catalogId = CreativeItems()[catalogIndex];

                        CreativeHandleCatalogClick(mWorld, catalogId, &creativeHeldFromHotbar);
                    }
                }
            }
            else
            {
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
                if(invEn == false)
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
                    if(invYPosition == kCreativeHotbarRow)
                    {
                        int slot = 27 + invXPosition;
                        if (mWorld->invId[slot] == mWorld->mId)
                        {
                            if(mWorld->mSt == true)
                            {
                                if (mWorld->invAm[slot] != 64)
                                {
                                    mWorld->invAm[slot] += 1;

                                    mWorld->mAm -= 1;

                                    if(mWorld->mAm == 0)
                                    {
                                        CreativeClearCursor(mWorld);
                                        creativeHeldFromHotbar = false;
                                    }
                                    return;
                                }
                            }
                        }

                        if (mWorld->mId != -1)
                        {
                            if(mWorld->mSt == true)
                            {
                               if (mWorld->invId[slot] == -1)
                               {
                                    mWorld->invId[slot] = mWorld->mId;
                                    mWorld->invAm[slot] = 1;
                                    mWorld->invSt[slot] = 1;

                                    mWorld->mAm -= 1;

                                    if(mWorld->mAm == 0)
                                    {
                                        CreativeClearCursor(mWorld);
                                        creativeHeldFromHotbar = false;
                                    }
                                    return;
                               }
                            }
                        }
                        else
                        {
                           if (mWorld->invId[slot] != -1 && mWorld->invSt[slot] == 1)
                           {
                               if(mWorld->invAm[slot] == 1)
                               {
                                   mWorld->mId = mWorld->invId[slot];
                                   mWorld->mAm = 1;
                                   mWorld->mSt = mWorld->invSt[slot];

                                   mWorld->invId[slot] = -1;
                                   mWorld->invAm[slot] = -1;
                                   mWorld->invSt[slot] = 0;
                                   creativeHeldFromHotbar = true;
                               }
                               if(mWorld->invAm[slot] > 1)
                               {
                                   mWorld->mId = mWorld->invId[slot];
                                   mWorld->mAm = ceilf((float)(mWorld->invAm[slot]/2.0f));
                                   mWorld->mSt = mWorld->invSt[slot];

                                   mWorld->invAm[slot] = mWorld->invAm[slot]-mWorld->mAm;
                                   creativeHeldFromHotbar = true;
                               }
                            }
                        }
                    }
                    else
                    {
                        unsigned int catalogIndex = (unsigned int)((creativePage + invYPosition) * 9 + invXPosition);
                        int catalogId = -1;
                        if (catalogIndex < CreativeItems().size())
                            catalogId = CreativeItems()[catalogIndex];

                        CreativeHandleCatalogClick(mWorld, catalogId, &creativeHeldFromHotbar);
                    }
                }
            }
            else
            {

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


void StatePlayCreative::Update(StateManager* sManager)
{
    mSoundMgr->AudioSync();
    mChat.Update(dt);

    // The debug stick's menu. Same contract as the survival state: it takes the whole update while it
    // is up, so the world does not advance behind it. See StatePlay::Update for the reasoning.
    if(debugMenu.IsOpen() == true)
        debugMenu.Update(dt, mSystemMgr, mSoundMgr);

    ApplyDebugMenuResult();

    if(debugMenu.IsOpen() == true)
        return;

    // Chat-button double-tap bookkeeping: a press that never became a double tap opens chat here,
    // one window later. The menu checks are repeated because that window is long enough for the
    // player to have opened something, and a keyboard stranded behind an inventory is worse than a
    // dropped tap.
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

    // Bow draw and release, same as the survival state. Creative keeps the button free while the
    // bow is drawn and needs no arrow to fire, so there is no ammo gate here.
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

    // Head and body, exactly as in StatePlay: the head follows the camera, the body is dragged after
    // it by 0.3 of the remaining angle per game tick and is clamped to BODY_MAX_HEAD_TURN behind it,
    // so the head can turn on the spot and the body swings round to catch up. See StatePlay.cpp.
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

    if(WorldRunning())//game state
    {
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
            mWorld->CheckForTorchSupport(x, y, z, 0);
            mWorld->CheckForLadderSupport(x, y, z);
            mWorld->DestroyAroundTrapdoors(x, y, z);
            mWorld->DestroyAroundItemFrames(x, y, z);
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

                            //mWorld->SetLigtSourcePosition(WorkingFurance->furnaceX,WorkingFurance->furnaceY,WorkingFurance->furnaceZ,107);
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
        }

        if(mWorld->mainOptions.freezeDayTime == false)
        {
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
            // The pick: one step per cell wall, clipped against the selection box. See the long note at
            // the same loop in StatePlay.cpp -- a wire's selection box is a 1 px plate, and both the
            // stride and the test matter for it.
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


        //update player position
        if(dt <= 0.1f)
        {
            musicTime -= dt;
            if (musicTime < 0.0f)
            {
                musicTime = 300.0f;
                if(mWorld->mainOptions.music)
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
                            mWorld->HG -= (1.0f)/55.0f;
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

            // A TNT blast launches the player here too -- the water player cannon is a build, and
            // creative is where you would try it. Only a supplied launch (kickedVelY) does anything:
            // mob kicks do not set it, which keeps creative's deliberate "no mob knockback" rule.
            if(mWorld->kickedStart)
            {
                if(mWorld->kickedVelY > 0.0f)
                {
                    walkingOnGround = false;
                    playerVelocity.y = mWorld->kickedVelY;
                    mWorld->kickedTimer = 0.6f;
                    fppCam->needUpdate = true;
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
            if(invEn || craft3xEn || chestEn || furnaceEn)
                fppCam->m_vVelocity = Vector3(0,0,0);

            // Fly carries momentum, so releasing the stick glides to a stop instead of stopping dead.
            // Written into m_vVelocity here because the move vector is read on the next line, so this is
            // the last place the glide can be applied before the collision actually uses it. Only in fly
            // (walking keeps its instant stop), and never while a panel is open, where the velocity was
            // just cleared above and a glide would drive the player away from the container.
            if(canFly && !invEn && !craft3xEn && !chestEn && !furnaceEn && sleepTime <= 0.0f)
            {
                bool moveInput = keyHold(InputHelper::Instance()->getButtonToAction(0))
                              || keyHold(InputHelper::Instance()->getButtonToAction(1))
                              || keyHold(InputHelper::Instance()->getButtonToAction(2))
                              || keyHold(InputHelper::Instance()->getButtonToAction(3));
                if(moveInput)
                {
                    // Held: the glide follows the input exactly, so there is no lag while steering.
                    flyGlideVelocity = fppCam->m_vVelocity;
                }
                else
                {
                    // Released: decay towards a stop. powf makes it frame-rate independent, and ~0.4s to
                    // a standstill is Java's feel rather than a fixed per-frame factor that would brake
                    // twice as hard at 60 fps as at 30.
                    float decay = powf(0.0006f, dt);
                    flyGlideVelocity.x *= decay;
                    flyGlideVelocity.z *= decay;
                    flyGlideVelocity.y = 0.0f;
                    if(flyGlideVelocity.magnitudeSq() < 0.0001f)
                        flyGlideVelocity = Vector3(0,0,0);
                    fppCam->m_vVelocity = flyGlideVelocity;
                }
            }

            Vector3 moveVector = fppCam->m_vVelocity;

            jumping = false;
            if(moveVector.x != 0.0f || moveVector.z != 0.0f)
            {
                if(!canFly)
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
                else
                {
                    if(TryToMove(Vector3(0,moveVector.y,moveVector.z),dt))
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
                    if(TryToMove(Vector3(moveVector.x,moveVector.y,0),dt))
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
            }

            if(canFly && sleepTime <= 0.0f && invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false)
            {
                float flyStep = cameraMoveSpeed * 1.35f;
                if(keyHold(InputHelper::Instance()->getButtonToAction(12)))
                {
                    TryToMove(Vector3(0, flyStep, 0), dt);
                }
                if(mSystemMgr->KeyHold(PSP_CTRL_DOWN))
                {
                    TryToMove(Vector3(0, -flyStep, 0), dt);
                }
            }

            if(mWorld->kickedTimer > 0.0f)
            {
                mWorld->kickedTimer -= dt;
                float kickPush = (mWorld->kickedPush > 0.0f) ? mWorld->kickedPush : 0.1f;
                TryToMove(Vector3(sinf(mWorld->kickedAngle+PI)*kickPush,0,cosf(mWorld->kickedAngle+PI)*kickPush),dt);
            }
            else
            {
                mWorld->kickedTimer = 0.0f;
                mWorld->kickedAngle = 0.0f;
                mWorld->kickedPush = 0.0f;
            }

            // Portal travel runs before the camera is placed: a swap moves the player, and the camera
            // has to follow it in the same frame rather than lag a frame behind.
            mWorld->UpdatePortalTravel(dt);
            if(mWorld->travelledThisFrame)
            {
                playerPosition = newPlayerPos = oldPlayerPos = mWorld->playerPos;
                mWorld->travelledThisFrame = false;
            }

            //update camera
            fppCam->m_vPosition = playerPosition;
            // The portal's wobble. Applied here rather than inside the camera update because
            // RebuildView rebuilds the up vector from scratch every frame and would drop the roll.
            fppCam->rollAngle = mWorld->GetPortalWobbleAngle();
            fppCam->RebuildView();
            fppCam->m_vVelocity = Vector3(0,0,0);
            fppCam->needUpdate = true;

            // world timers
            if(mWorld->mainOptions.freezeDayTime == false)
            {
                mWorld->UpdateWorldTime(dt);
                skyLight->UpdateLightSource(mWorld->skyTime);
            }

            cubeLight = mWorld->BlockFinalLight(playerPosition.x,playerPosition.y-1,playerPosition.z) * 1.2f;
            if(cubeLight > 1.0f)
            {
                cubeLight = 1.0f;
            }
        }

        mWorld->UpdatePlayerZoneBB(playerPosition);
        mWorld->UpdateWorldProcesses(dt > 0.1f ? 0.1f : dt);

        //walking sound & walking animation
        if(isWalking && walkingOnGround && !canFly && (!footInWater || !footInLava))
        {
            float walkSoundSpeed = dt;
            // Footsteps paced by the same clock as the legs, so a sprint takes steps 1.3x sooner.
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

            // The limb animation is shared between ground, air and flight: it tracks horizontal
            // distance, not ground contact, so a jump, a fall or creative flight keeps the limbs
            // moving -- which is the whole of what a flying player's walk cycle is. Java's limbSwing
            // never consults the ground. See StatePlay.cpp.
            float target = isWalking ? (sneaking ? 0.35f : 1.0f) : 0.0f;
            float k = 8.0f * dt;
            if(k > 1.0f)
                k = 1.0f;
            bobWalk += (target - bobWalk) * k;

            // Camera bob, Java's cameraYaw: built from the per-tick horizontal distance, capped at
            // 0.1, and forced to zero whenever the player is not on the ground -- so it holds steady
            // through a jump and through flight while the limbs keep going. See StatePlay.cpp for the
            // Java lines this mirrors and for why it is not aliased to bobWalk.
            float camSpeed = (dt > 0.0f) ? (step / (20.0f * dt)) : 0.0f;
            if(camSpeed > 0.1f)
                camSpeed = 0.1f;
            if(!(isWalking && walkingOnGround && !canFly))
                camSpeed = 0.0f;
            bobCamera += (camSpeed - bobCamera) * k;

            if(step > 0.0001f)
            {
                // 1.3x comes free from the sprint speed; the extra 1.15 is for readability on this
                // screen, as in StatePlay.cpp. The fly sprint counts too, since it is the same speed
                // increase and Java's cycle follows speed.
                float cycleScale = (sprinting || (canFly && flySprint)) ? 1.15f : 1.0f;
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
            if(mWorld->haveCompass) // only if you have compass in bar slots, compass twxture should update
            {
                mWorld->UpdateCompassTexture((fppCam->horAngle/180.0f)*PI);
            }
        }

        {
            Vector3 fwd = fppCam->m_vView - fppCam->m_vPosition;
            fwd.normalize();
            Vector3 camUp = fppCam->m_vUpVector;
            Vector3 camRight = Vector3::cross(fwd, camUp);
            if(camRight.magnitudeSq() < 0.0001f)
                camRight = Vector3(1.0f, 0.0f, 0.0f);
            else
                camRight.normalize();
            camUp = Vector3::cross(camRight, fwd);
            camUp.normalize();
        }
    }
}


void StatePlayCreative::Draw(StateManager* sManager)
{
    // Sprint FOV must rebuild proj before StartFrame (SetupCameraMatrices loads proj).
    if(mWorld != NULL && mRender != NULL)
    {
        // Java sprint FOV = fov * 1.15. Both kinds of sprint widen it: the fly sprint above, and the
        // ground sprint when not flying. See StatePlay.cpp.
        bool sprintingNow = (canFly && flySprint) || (!canFly && sprinting);
        float targetBoost = sprintingNow ? ((float)mWorld->mainOptions.fov * SPRINT_FOV_MULT) : 0.0f;
        float lerp = (dt > 0.0f) ? (12.0f * dt) : 1.0f;
        if(lerp > 1.0f) lerp = 1.0f;
        flyFovBoost += (targetBoost - flyFovBoost) * lerp;
        float wantFov = (float)mWorld->mainOptions.fov + flyFovBoost;
        if(wantFov < 40.0f) wantFov = 40.0f;
        if(wantFov > 120.0f) wantFov = 120.0f;
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

        // Third person. Render-only offset: the eye stays on the player, so picks, particles and the
        // sky are untouched, and the boom is walked back until a solid block stops it. needUpdate is
        // forced only when the boom itself changes, which refreshes the frustum on the toggle frame
        // without asking for one every frame. The front view is the same walk in the other direction
        // plus the mirror flag; see StatePlay.cpp for the full reasoning.
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
	// draws a sky -- skybox, stars, sun, moon, clouds -- is skipped below. Mirrors StatePlay.
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
            float fog_end = mWorld->mainOptions.horizontalViewDistance * 4 + 40.00f;
            if(fog_start < 0.0f)
            {
                fog_start = 0.0f;
            }
            if(nether)
            {
                // Pulled in closer than the overworld's: the nether should feel enclosed, and the fog
                // is what stops the far wall of a cavern reading as flat untextured rock.
                sceGuFog(fog_start, fog_end * 0.6f, GU_COLOR(netherSky.x,netherSky.y,netherSky.z,1.0));
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

	/// TNT entity rendering
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
    // In the world pass with the mobs. Only in third person; in first person the player is the camera
    // and the hand pass further down draws the held item.
    if(thirdPersonView > 0 && sleepTime <= 0.0f)
    {
        // Swing from the same walk bob the camera uses -- it advances with distance travelled, so the
        // legs stop when the player does, and bobWalk fades it out so nobody freezes mid-stride.
        float walkSwing = sinf(bobCycle) * 0.85f * bobWalk;

        // Soles, from the game's own standing relation: the eye is playerGrowth - 0.05 above them.
        Vector3 feet = Vector3(playerPosition.x,
                               playerPosition.y - (mWorld->playerGrowth - 0.05f),
                               playerPosition.z);

        // Mobs measure facing as (sin, 0, cos) and the camera as (cos, 0, sin): the quarter turn.
        // bodyYaw lags the camera and the head is turned past it, so the model's head follows the aim
        // while the body comes round after it. See StatePlay.cpp.
        float faceAngle = PI/2.0f - (bodyYaw/180.0f)*PI;
        float headTurn = -(headYawOffset/180.0f)*PI;

        float playerLight = mWorld->BlockFinalLight((int)floorf(feet.x),(int)floorf(feet.y + 1.0f),(int)floorf(feet.z));

        if(invPlayer != NULL)
        {
            int steveTex = TextureHelper::Instance()->GetTexture(TextureHelper::Steve);
            TextureManager::Instance()->SetTextureModeulate(steveTex);
            // Punch and place, from the same 0..PI swing the first-person hand uses; sin() of it is
            // the 0..1..0 arc. See StatePlay.cpp.
            float hitSwing = sinf(animDest) * 1.3f;
            invPlayer->RenderInWorld(mWorld, feet, faceAngle, walkSwing, -walkSwing, playerLight,
                                     steveTex, mWorld->invId[27+barPosition],
                                     headTurn, fppCam->upDownAngle, hitSwing,
                                     // -1 = a bow in hand but no draw; 0..3 = the string is pulled,
                                     // which lifts the bow arm. See StatePlay.cpp and kBowArmRaise.
                                     bowCharging == true ? BowDrawStage(bowCharge) : -1);
        }
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

    /// CHICKEN. No drop: vanilla chickens yield feathers and chicken meat, and neither item exists
    /// in this build.
    if(mWorld->mChickens.empty() == false)
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

    /// SPIDERS. The survival block, copied: the class owns the eight-legged walk, the wall climb and
    /// the melee kick, so the loop only feeds it the player and the frame time. The two states fork
    /// gameplay and their mob blocks are duplicated by design.
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
                            // Same as StatePlay: the drop carries the bow's full durability in its
                            // amount field, because a bow is a non-stacking tool.
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
    /// call into the world (CraftWorld::UpdateArrows) so that this state and the survival one cannot
    /// disagree about what a shot does -- which is the point of moving it out, because these two
    /// loops used to hold two copies of the whole thing and only one of the copies ever gets edited.
    /// The one difference that used to be here, "creative does not take knockback", is now a
    /// gameModeWorld test inside that one call.
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


    /// render particles
    if(fppCam != NULL)
        Particle::RenderAll(fppCam->mFrustum);

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
        sceGuDisable(GU_FOG );	// disable fog
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

    TextureManager::Instance()->SetTextureModeulate(texture);

    /// RENDER DESTROY MODEL
    if (((startDt == 1 && dStd >= 0 && dStd <= 9) || showCube == true) && makeScreen == false)
	{
		MatrixPush();

        DrawSetDepthTest(true);
        DrawSetAlphaTest(true);
		DrawSetBlend(true);

		sceGuDepthMask(1);

		MatrixTranslation(Vector3(cubePos.x,cubePos.y,cubePos.z));
		MatrixColor(0xFFFFFFFF);

        TextureManager::Instance()->SetTextureModeulate(texture);

		destroyer->Update(dStd, mWorld, mWorld->GetBlock(cubePos.x,cubePos.y,cubePos.z), (int)cubePos.x, (int)cubePos.y, (int)cubePos.z);
		destroyer->Render(dStd);

		sceGuDepthMask(0);

		DrawSetBlend(false);
        DrawSetAlphaTest(false);
        DrawSetDepthTest(false);

		MatrixPop();
	}
    ///

    /// RENDER STEVE'S HAND
    // First person only: in third person the model in the world holds the item instead.
    if(mWorld->mainOptions.guiDrawing == 1 && thirdPersonView == 0)
	{
	    //if(mWorld->invId[27+barPosition] != -1)
        //{
            float cubeBob = 0.0f;
            // No hand bob while flying. The walk cycle itself keeps running -- the third-person model
            // and the legs still swing, which is Java's limbSwing and never consults the ground -- but
            // the arm on the camera must not sway with a walk the player is not doing. The camera bob
            // is already forced to zero in flight by the same rule (camSpeed above), so suppressing
            // only that half left the hand walking through the air on its own.
            float cubeBob2 = canFly ? 0.0f : (-fabsf(cosf(bobCycle)) * bobWalk / 18);

            sceGuColor(GU_COLOR(cubeLight,cubeLight,cubeLight,1));

            if(mWorld->invId[27+barPosition] == -1)
            {
                TextureManager::Instance()->SetTextureModeulate(TextureHelper::Instance()->GetTexture(TextureHelper::Steve));
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
                // See StatePlay's copy of this: a held bow is drawn as a weapon, and mid-draw as one of
                // its three pulling frames, which exist as item types of their own.
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
                //scale
                //scale. A bow is drawn at weapon size; see kHeldBowScale.
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
            sceGuColor(0xFFFFFFFF);
	}
	sceGuColor(0xFFFFFFFF);

	/// DRAW MASKS
	mRender->SetOrtho(0,0,0,0,0,0);
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

        MatrixColor(GU_COLOR(1,1,1,(3.1-sleepTime)/3.2f));

        TextureManager::Instance()->SetTextureModeulate(black);
        advancedBlit(0,0,SCR_WIDTH,SCR_HEIGHT,0,0,32);

        DrawPlaceTexture(false);
        DrawSetBlend(false);
        DrawSetDepthMask(false);
        DrawSetDepthTest(true);
    }


    if(mWorld->BlockSolid(playerPosition.x,playerPosition.y,playerPosition.z))
    {
        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);
        DrawPlaceTexture(true);

        MatrixColor(GU_COLOR(1,1,1,0.9));

        TextureManager::Instance()->SetTextureModeulate(black);
        advancedBlit(0,0,SCR_WIDTH,SCR_HEIGHT,0,0,32);

        DrawPlaceTexture(false);
        DrawSetBlend(false);
        DrawSetDepthMask(false);
        DrawSetDepthTest(true);
    }

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
	/// GUI
	sceGuDisable(GU_DEPTH_TEST);
	sceGuDepthMask(1);
	sceGuEnable(GU_BLEND);
	sceGuColor(GU_COLOR(1,1,1,1.0f));

	if ((invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && menuState == 0 && mWorld->mainOptions.guiDrawing == 1) || makeScreen == true)
    {
        barSprite->Draw();
        if (menuState != 1)
        {
            sceGuBlendFunc(GU_ABS, GU_ONE_MINUS_SRC_ALPHA, GU_SRC_ALPHA, 0, 0);
            crossSprite->Draw();
            sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
            selectSprite->Draw();
        }
    }
    else
    {
        if (invEn == true)
        {
            invSprite->Draw();
            if(creativeTab >= 0 && creativeTab < kCreativeTabCount && creativeTabSelSprite[creativeTab] != NULL)
            {
                float tabX = GuiSpriteX(kCreativeGuiW, kCreativeGuiScale, creativeTab * kCreativeTabW, kCreativeTabW);
                float tabY = GuiSpriteY(kCreativeGuiH, kCreativeGuiScale, 0.0f, kCreativeTabH);
                creativeTabSelSprite[creativeTab]->SetPosition(tabX, tabY);
                creativeTabSelSprite[creativeTab]->Draw();
            }
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
	sceGuDisable(GU_BLEND);

    /// 3D BLOCKS AND ITEMS ON 2D PANEL
    if ((invEn == false && craft3xEn == false && chestEn == false && menuState == 0 && mWorld->mainOptions.guiDrawing == 1 && furnaceEn == false) || makeScreen == true)
    {
        for(int k = 0; k <= 8; k++)
        {
            if(mWorld->invId[27+k] != -1)
            {
                MatrixPush();
				float slotX = 108.0f + (11.0f + k * 20.0f) * (264.0f / 182.0f);
				MatrixTranslation(Vector3(slotX, 230, 0));
				MatrixScale(Vector3(0.8f, 0.8f, 0.8f));

                if(IsBlockId(mWorld->invId[27+k]))
                {
                    TextureManager::Instance()->SetTextureModeulate(texture);

                    mWorld->drawHudCubes(mWorld->invId[27+k]);
                }

                if(IsItemId(mWorld->invId[27+k]))
                {
                    mWorld->ItemHaveTerrainTexture(mWorld->invId[27+k]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                    mWorld->drawHudItems(mWorld->invId[27+k]);
                }
                MatrixPop();
            }
        }
    }

    /// INVENTORY CRAFT MENU
    if(invEn == true)
    {
        int tabIcon[5];
        tabIcon[0] = GrassBlock::getID();
        tabIcon[1] = Flower1Item::getID();
        tabIcon[2] = Redstone::getID();
        tabIcon[3] = WoodenPickaxe::getID();
        tabIcon[4] = IronBar::getID();
        for(int t = 0; t < kCreativeTabCount; t++)
        {
            float iconX = GuiTexX(kCreativeGuiW, kCreativeGuiScale, t * kCreativeTabW + 13.0f);
            float iconY = GuiTexY(kCreativeGuiH, kCreativeGuiScale, (t == creativeTab) ? 15.0f : 18.0f);
            sceGumPushMatrix();
            ScePspFVector3 tabLoc = {iconX, iconY, 0.0f};
            sceGumTranslate(&tabLoc);
            MatrixScale(Vector3(0.7f, 0.7f, 0.7f));
            if(IsBlockId(tabIcon[t]))
            {
                TextureManager::Instance()->SetTextureModeulate(texture);
                mWorld->drawHudCubes(tabIcon[t]);
            }
            else
            {
                mWorld->ItemHaveTerrainTexture(tabIcon[t]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);
                mWorld->drawHudItems(tabIcon[t]);
            }
            sceGumPopMatrix();
        }

        std::vector<int>& cat = CreativeItems();
        for(int i = 0; i < kCreativeCatRows; i++)
        {
            for(int j = 0; j <= 8; j++)
            {
                unsigned int catalogIndex = (unsigned int)((creativePage + i) * 9 + j);
                if(catalogIndex < cat.size() && cat[catalogIndex] != -1)
                {
                    sceGumPushMatrix();

                    ScePspFVector3 loc = {GuiCreativeSlotX(j), GuiCreativeSlotY(i), 0.0f};
                    sceGumTranslate(&loc);

                    if(IsBlockId(cat[catalogIndex]))
                    {
                        TextureManager::Instance()->SetTextureModeulate(texture);

                        mWorld->drawHudCubes(cat[catalogIndex]);
                    }

                    if(IsItemId(cat[catalogIndex]))
                    {
                        mWorld->ItemHaveTerrainTexture(cat[catalogIndex]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                        mWorld->drawHudItems(cat[catalogIndex]);
                    }
                    sceGumPopMatrix();
                }
            }
        }
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
                        mWorld->ItemHaveTerrainTexture(craftSlotId3[i*3+j]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                        mWorld->drawHudItems(craftSlotId3[i*3+j]);
                    }
                    MatrixPop();
                }
            }
        }

        ///Item which we are crafting 3x
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
                TextureManager::Instance()->SetTextureModeulate(barItems);

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
        for(int k = 0; k <= 8; k++)
        {
            if(mWorld->invId[27+k] != -1)
            {
                sceGumPushMatrix();

                if(invEn == true)
                {
                    ScePspFVector3 loc = {GuiCreativeSlotX(k), GuiCreativeItemY(kCreativeHotbarRow), 0.0f};
                    sceGumTranslate(&loc);
                }
                else
                {
                    ScePspFVector3 loc = {GuiSlotX(guiInvW, guiScale, 7.0f + k * 18.0f), GuiPlayerInvItemY(guiInvH, guiScale, 3), 0.0f};
                    sceGumTranslate(&loc);
                }


                if(IsBlockId(mWorld->invId[27+k]))
                {
                    TextureManager::Instance()->SetTextureModeulate(texture);

                    mWorld->drawHudCubes(mWorld->invId[27+k]);
                }

                if(IsItemId(mWorld->invId[27+k]))
                {
                    mWorld->ItemHaveTerrainTexture(mWorld->invId[27+k]) ? TextureManager::Instance()->SetTextureModeulate(texture) : TextureManager::Instance()->SetTextureModeulate(barItems);

                    mWorld->drawHudItems(mWorld->invId[27+k]);
                }

                sceGumPopMatrix();
            }
        }

        if(invEn == false)
        {
            for(int i = 0; i <= 2; i++)
            {
                for(int j = 0; j <= 8; j++)
                {
                    if(mWorld->invId[i*9+j] != -1)
                    {
                        sceGumPushMatrix();

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
                        ScePspFVector3 loc = {GuiSlotX(panelW, guiScale, 7.0f + j * 18.0f), GuiPlayerInvSlotY(panelH, guiScale, i), 0.0f};
                        sceGumTranslate(&loc);

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
                        sceGumPopMatrix();
                    }
                }
            }
        }

        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);

        if(invEn)
        {
            invCellSprite->SetPosition(GuiCreativeSlotX(invXPosition), GuiCreativeSlotY(invYPosition));
            invCellSprite->Draw();
        }
        else
        {
            invCellSprite->SetPosition(selectInvSprite->GetPositionX(),selectInvSprite->GetPositionY());
            invCellSprite->Draw();
        }

        if (mWorld->mId != -1) //MOUSE ITEM
        {
            sceGumPushMatrix();
            if (upEn == 1)
            {
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
                    ScePspFVector3 loc = {GuiSlotX(guiFurW, guiScale, 61.0f), GuiSlotY(guiFurH, guiScale, 4.0f + invYPosition * 36.0f) - 8.0f, 0.0f};
                    sceGumTranslate(&loc);
                }
            }
            else
            {
                if(invEn == false)
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
                    ScePspFVector3 loc = {GuiSlotX(panelW, guiScale, 7.0f + invXPosition * 18.0f), GuiPlayerInvSlotY(panelH, guiScale, invYPosition) - 8.0f, 0.0f};
                    sceGumTranslate(&loc);
                }
                else
                {
                    float hx = creativeMouseFree ? creativeMouseX : GuiCreativeSlotX(invXPosition);
                    float hy = (creativeMouseFree ? creativeMouseY : GuiCreativeSlotY(invYPosition)) - 8.0f;
                    ScePspFVector3 loc = {hx, hy, 0.0f};
                    sceGumTranslate(&loc);
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
            sceGumPopMatrix();
        }
    }

	DrawSetDepthTest(false);
	DrawSetDepthMask(true);
	DrawSetBlend(true);
	MatrixColor(GU_COLOR(1,1,1,1.0f));
    // durability strip drawing
    if ((invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && menuState == 0 && mWorld->mainOptions.guiDrawing == 1) || makeScreen == true)
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

                        toolPointSprite[toolPointStd]->SetPosition(192+j*36,36+i*36);
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

                        toolPointSprite[toolPointStd]->SetPosition(84+j*36,36+i*36);
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

                toolPointSprite[toolPointStd]->SetPosition(192,36);
                toolPointSprite[toolPointStd]->Draw();
            }
        }

        if(IsItemId(UseFurnace->furnaceSlotId[1]) && mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[1]) != -1) // it is item and it have durability points
        {
            // See the slot 0 case above: the trailing `!= -1` made this guard always true.
            if(UseFurnace->furnaceSlotAm[1] != mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[1])) // checking durability
            {
                unsigned int toolPointStd = roundf((float)(UseFurnace->furnaceSlotAm[1]) / (float)(mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[1])) * 13);

                toolPointSprite[toolPointStd]->SetPosition(192,108);
                toolPointSprite[toolPointStd]->Draw();
            }
        }
    }

    if (invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true)
    {
        if(invEn == false)
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

                                toolPointSprite[toolPointStd]->SetPosition(84+j*36,148+i*36);
                                toolPointSprite[toolPointStd]->Draw();
                            }
                        }
                    }
                }
            }
        }
        else
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

                            toolPointSprite[toolPointStd]->SetPosition(68+k*36,254);
                            toolPointSprite[toolPointStd]->Draw();
                        }
                    }
                }
            }
            /*
            mRender->SetFont(ENGLISH);
            RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00004000|0x00000000);
            RenderManager::InstancePtr()->DebugPrint(20,20,"%i/3",creativePage+1);
            RenderManager::InstancePtr()->DebugPrint(420,20,"R");
            RenderManager::InstancePtr()->DebugPrint(420,40,"L");
            mRender->SetDefaultFont();
            */
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
                    if(invEn == false)
                    {
                        toolPointSprite[toolPointStd]->SetPosition(84+invXPosition*36,140+invYPosition*36);
                    }
                    else
                    {
                        toolPointSprite[toolPointStd]->SetPosition(GuiDurX(GuiCreativeSlotX(invXPosition)), GuiDurY(GuiCreativeSlotY(invYPosition)));
                    }
                }
                toolPointSprite[toolPointStd]->Draw();
            }
        }
    }

    DrawSetBlend(false);


    if(invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true)
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        if(invEn == false)
        {
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
                                //DrawAmount(106+j*36,164+i*36,mWorld->invAm[i*9+j]);
                            }
                        }
                        else
                        {
                            //DrawAmount(106+j*36,164+i*36,mWorld->invAm[i*9+j]);
                        }
                    }
                }
            }
        }
        else
        {
            for(int k = 0; k <= 8; k++)
            {
                if (mWorld->invId[27+k] != -1 && mWorld->invAm[27+k] > 1)
                {
                    if(IsItemId(mWorld->invId[27+k]))
                    {
                        if(mWorld->DurabilityPointsItem(mWorld->invId[27+k]) == -1)
                        {
                           // DrawAmount(90+k*36,270,mWorld->invAm[27+k]);
                        }
                    }
                    else
                    {
                        //DrawAmount(90+k*36,270,mWorld->invAm[27+k]);
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
                    if(invEn == 1)
                    {
                        if(onDestroySlot)
                        {
                           // DrawAmount(92+(9 * 36),46+(6 * 36),mWorld->mAm);
                        }
                        else
                        {
                           // DrawAmount(90+(invXPosition * 36),46+(invYPosition * 36),mWorld->mAm);
                        }
                    }
                    else
                    {
                        //DrawAmount(106+(invXPosition * 36),156+(invYPosition * 36),mWorld->mAm);
                    }
                }
                else
                {
                    if(craft3xEn == 1)
                    {
                       // DrawAmount(214+(invXPosition * 36),44+(invYPosition * 36),mWorld->mAm);
                    }
                    if(chestEn == 1)
                    {
                       // DrawAmount(106+(invXPosition * 36),44+(invYPosition * 36),mWorld->mAm);
                    }
                    if(furnaceEn == 1)
                    {
                       // DrawAmount(214,44+(invYPosition * 72),mWorld->mAm);
                    }
                }
            }
        }

        DrawSetDepthTest(false);
        DrawSetDepthMask(true);
        DrawSetBlend(true);

        const float sliderNubW = kCreativeSliderNubW;
        const float sliderNubH = kCreativeSliderNubH;
        float sliderX = GuiSpriteX(kCreativeGuiW, kCreativeGuiScale, kCreativeSliderX, sliderNubW);
        float sliderTravel = kCreativeSliderH - sliderNubH;
        float slideTexY = kCreativeSliderY;
        if(creativePageMax > 0)
            slideTexY = kCreativeSliderY + ((float)creativePage / (float)creativePageMax) * sliderTravel;
        float slide_y = GuiSpriteY(kCreativeGuiH, kCreativeGuiScale, slideTexY, sliderNubH);
        sliderSprite->SetPosition(sliderX,slide_y);
        sliderLightedSprite->SetPosition(sliderX,slide_y);

        if(invEn)
        {
            if(creativePageMax > 0)
                sliderSprite->Draw();
            else
                sliderLightedSprite->Draw();
            selectInvSprite->Draw();
        }
        else
        {
            selectInvSprite->Draw();
        }

        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));
    }

    if((invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && menuState == 0 && mWorld->mainOptions.guiDrawing == 1) || makeScreen == true)
    {
        for(int k = 0; k <= 8; k++)
        {
			if (mWorld->invAm[27 + k] > 1)
			{
				if (IsItemId(mWorld->invId[27 + k])) // if it is item but not a tool
				{
					if (mWorld->DurabilityPointsItem(mWorld->invId[27 + k]) == -1)
					{
						//DrawAmount(128 + k * 29, 251, mWorld->invAm[27 + k]);
					}
				}
				else
				{
					//DrawAmount(128 + k * 29, 251, mWorld->invAm[27 + k]);
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
                if(craftSlotId3[i*3+j] != -1 && craftSlotAm3[i*3+j] > 1)
                {
                    if(IsItemId(craftSlotId3[i*3+j]))
                    {
                        if(mWorld->DurabilityPointsItem(craftSlotId3[i*3+j]) == -1)
                        {
                            //DrawAmount(214+j*36,52+i*36,craftSlotAm3[i*3+j]);
                        }
                    }
                    else
                    {
                        //DrawAmount(214+j*36,52+i*36,craftSlotAm3[i*3+j]);
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
                   // DrawAmount(358,88,craftItemAm3);
                }
            }
            else
            {
                //DrawAmount(358,88,craftItemAm3);
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
                            //DrawAmount(106+j*36,52+i*36,UseChest->chestSlotAm[i*9+j]);
                        }
                    }
                    else
                    {
                       // DrawAmount(106+j*36,52+i*36,UseChest->chestSlotAm[i*9+j]);
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
                   // DrawAmount(214,52,UseFurnace->furnaceSlotAm[0]);
                }
            }
            else
            {
                 //DrawAmount(214,52,UseFurnace->furnaceSlotAm[0]);
            }
        }
        if(UseFurnace->furnaceSlotId[1] != -1 && UseFurnace->furnaceSlotAm[1] > 1)
        {
            if(IsItemId(UseFurnace->furnaceSlotId[1]))
            {
                if(mWorld->DurabilityPointsItem(UseFurnace->furnaceSlotId[1]) == -1)
                {
                   // DrawAmount(214,124,UseFurnace->furnaceSlotAm[1]);
                }
            }
            else
            {
               // DrawAmount(214,124,UseFurnace->furnaceSlotAm[1]);
            }
        }
        if(UseFurnace->furnaceSlotId[2] != -1 && UseFurnace->furnaceSlotAm[2] > 1)
        {
            //DrawAmount(322,88,UseFurnace->furnaceSlotAm[2]);
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
    mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),GU_COLOR(0.0f,0.0f,0.0f,1),0x00000200);

    // Name of the item under the cursor, the same hover label the survival panels draw. It matters
    // more here than it does in survival: this is the screen where every spawn egg is on display at
    // once, and sixteen 16x16 eggs that differ only in colour are impossible to tell apart without
    // picking each one up. The branches mirror StatePlayCreative::UpdateInvCursorPos, which is what
    // decides where the cursor actually is: invEn means the catalog (four scrollable rows plus the
    // hotbar row), upEn means a craft grid / chest / furnace, and otherwise it is the player grid.
    if(invEn == true || craft3xEn == true || chestEn == true || furnaceEn == true)
    {
        int hoverId = -1;
        if(invEn == true)
        {
            if(invYPosition == kCreativeHotbarRow)
            {
                hoverId = mWorld->invId[27 + invXPosition];
            }
            else
            {
                unsigned int catalogIndex = (unsigned int)((creativePage + invYPosition) * 9 + invXPosition);
                if(catalogIndex < CreativeItems().size())
                {
                    hoverId = CreativeItems()[catalogIndex];
                }
            }
        }
        else if(upEn == 1)
        {
            if(craft3xEn == true)      hoverId = craftSlotId3[invYPosition*3 + invXPosition];
            else if(chestEn == true)   hoverId = UseChest->chestSlotId[invYPosition*9 + invXPosition];
            else if(furnaceEn == true) hoverId = UseFurnace->furnaceSlotId[invYPosition];
        }
        else
        {
            hoverId = mWorld->invId[invYPosition*9 + invXPosition];
        }

        if(hoverId == -1)
        {
            hoverId = mWorld->mId;
        }

        if(hoverId != -1)
        {
            std::string hoverName = mWorld->NameBlock(hoverId);
            if(hoverName != "null")
            {
                mRender->SetFontStyle(0.5f,GU_COLOR(1,1,1,1),GU_COLOR(0.15f,0.15f,0.15f,1),0x00000200);
                float hoverNameW = (float)mRender->GetTextWidth(hoverName.c_str());
                float hoverX = GuiHoverLabelX(selectInvSprite->GetPositionX(),hoverNameW);
                float hoverY = GuiHoverLabelY(selectInvSprite->GetPositionY());
                mRender->DebugPrint((int)hoverX,(int)hoverY,"%s",hoverName.c_str());
                mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),GU_COLOR(0.0f,0.0f,0.0f,1),0x00000200);
            }
        }
    }

    if(invEn == false && craft3xEn == false && chestEn == false && furnaceEn == false && menuState == 0)
    {
        if(tickShowFlymodeMessage > 0.0f)
        {
            if(tickShowFlymodeMessage > 1.0f)
            {
                mRender->SetFontStyle(0.5f,GU_COLOR(1,1,1,1),GU_COLOR(0.15f,0.15f,0.15f,1),0x00000200);
            }
            else
            {
                mRender->SetFontStyle(0.5f,GU_COLOR(1,1,1,tickShowFlymodeMessage),GU_COLOR(0.15f,0.15f,0.15f,tickShowSlotName),0x00000200);
            }
            if(language == ENGLISH)
            {
                canFly == true ? mRender->DebugPrint(240,30,"Fly mode : on") : mRender->DebugPrint(240,30,"Fly mode : off");
            }
        }

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
    }

    if(sleepMessageTime > 0.0f)
    {
        sceGuDisable(GU_DEPTH_TEST);
        sceGuEnable(GU_BLEND);
        sceGuColor(GU_COLOR(1,1,1,1.0f));

        if(mRender->GetFontLanguage() == ENGLISH)
        {
            mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
            mRender->DebugPrint(242,158,"You can only sleep at night");
        }
    }

    sleepMessageTime -= dt;
    if(sleepMessageTime < 0.1f && sleepMessageTime > 0.0f)
    {
        sleepMessageTime = 0.0f;
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
        //mRender->DebugPrint(20,34,"cpu: %d%%",mRender->GetCpuUsage());
        //mRender->DebugPrint(20,44,"gpu: %d%%",mRender->GetGpuUsage());
        //mRender->DebugPrint(20,54,"world time: %f",mWorld->worldDayTime);

        mRender->DebugPrint(15,20,"Position: %f,%f,%f",playerPosition.x,playerPosition.y,playerPosition.z);
    }

    if(menuState == 0)
        mChat.Draw(mRender);

    // The purple wash while a portal pulls the player through. Same contract and same reason for the
    // screen-space quad as StatePlay's -- see the long note there.
    const float portalOverlay = mWorld->GetPortalOverlayAlpha();
    if(portalOverlay > 0.0f)
    {
        // Diagnostic, same contract as StatePlay's -- see the note there.
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

            // GU_SPRITES with two vertices, the same call advancedBlit fills the screen with for the xp
            // bar; the corners carry texels 0 and 16, which stretches the whole 16x16 over 480x272.
            TexturesPSPVertex16* quad = (TexturesPSPVertex16*)sceGuGetMemory(2 * sizeof(TexturesPSPVertex16));
            quad[0].u = 0;  quad[0].v = 0;  quad[0].x = 0;         quad[0].y = 0;         quad[0].z = 0;
            quad[1].u = 16; quad[1].v = 16; quad[1].x = SCR_WIDTH; quad[1].y = SCR_HEIGHT; quad[1].z = 0;
            sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, quad);
        }
    }

    // The debug stick's menu, last so it sits over the world, the HUD and the chat.
    debugMenu.Render(mRender);

    //end frame
    mRender->EndFrame();
}
//additional functions
void StatePlayCreative::advancedBlit(int sx, int sy, int sw, int sh, int dx, int dy, int slice)
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

bool StatePlayCreative::TryToMove(Vector3 moveVector,float dt)
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
        if(sneaking && walkingOnGround && !canFly && (moveVector.x != 0.0f || moveVector.z != 0.0f))
        {
            Vector3 destFoot = dest + Vector3(0.0f, -mWorld->playerGrowth - 0.05f, 0.0f);
            Vector3 curFoot = playerPosition + Vector3(0.0f, -mWorld->playerGrowth - 0.05f, 0.0f);
            if(mWorld->SolidAtPointForPlayer(curFoot) && !mWorld->SolidAtPointForPlayer(destFoot))
                return false;
        }
        playerPosition = dest;

        return true;
    }

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

bool StatePlayCreative::keyPressed(int currentKey)
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

bool StatePlayCreative::keyHold(int currentKey)
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
    // One radial deadzone and an 8-way test, so a diagonal is a direction like any other. See the
    // long note in StatePlay::keyHold -- this is the same change, kept identical on purpose.
    if(currentKey >= 11 && currentKey <= 14)
    {
        float ax = mSystemMgr->GetAnalogX();
        float ay = mSystemMgr->GetAnalogY();
        float mag = sqrtf(ax * ax + ay * ay);

        float dead = (InputHelper::Instance()->analogYup - InputHelper::Instance()->analogYdown
                    + InputHelper::Instance()->analogXright - InputHelper::Instance()->analogXleft) * 0.25f;
        if(dead < 0.0f)
            dead = -dead;
        if(mag < dead)
            return false;

        const float kAnalogDiagonalShare = 0.5f;
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

void StatePlayCreative::FireBowArrow()
{
    // Identical to the survival shot minus the arrow spend: bowCharge is 0..1 of draw and vanilla
    // bends it, f = (c^2 + 2c) / 3, so half a second is 0.42 of full speed. Under 0.1 the shot is
    // refused, which is Java's "at least two ticks drawn" floor.
    float power = (bowCharge*bowCharge + 2.0f*bowCharge) / 3.0f;
    if(power < 0.1f)
        return;
    if(power > 1.0f)
        power = 1.0f;

    Arrow2* arrow = new Arrow2(fppCam->m_vPosition.x, fppCam->m_vPosition.y, fppCam->m_vPosition.z);
    arrow->SetTexture(TextureHelper::Instance()->GetTexture(TextureHelper::arrowProjectileTexture));
    arrow->SetVeloc(fppCam->upDownAngle, (fppCam->horAngle/(float)180)*PI, power);
    arrow->damage = 2.0f + 6.0f*power;
    arrow->fromMob = false;
    mWorld->mArrows.push_back(arrow);

    // A creative-created world switched to survival still runs this state, so the wear call is made
    // here too and the survival/creative decision is made inside it from gameModeWorld -- the same
    // thing StatePlay does with the arrow spend above.
    WearBowOnShot(mWorld, mSoundMgr, barPosition);

    mSoundMgr->PlayBowSound();

    animGo = true;
    animDest = 0.0f;
}

void StatePlayCreative::PutInInventory(int id, int num, bool st)
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

// Right-click with the debug stick in hand. Identical in intent to StatePlay::CheckDebugStickUse --
// see that one for the reach and the reasoning; the two are separate because the states do not share
// a base class and each owns its own mob vectors through its own mWorld.
void StatePlayCreative::CheckDebugStickUse()
{
    if(mWorld == NULL || fppCam == NULL)
        return;

    Vector3 dir = fppCam->m_vView - fppCam->m_vPosition;
    dir.normalize();

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

    debugTarget = mob;
    debugMenu.Open(mob->debugName, options);
    mSoundMgr->PlayMenuSound2();
}

// Applies the debug menu's answer, if there is one.
void StatePlayCreative::ApplyDebugMenuResult()
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
        // The world is frozen while the menu is up, so the mob should still be alive and in its list.
        // Checked anyway: a stale pointer here is a crash, not a wrong-looking menu.
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

bool StatePlayCreative::WorldRunning()
{
    // See StatePlay::WorldRunning: the world is ticked from Draw under `menuState == 0` guards, so the
    // debug menu has to join that condition rather than hold Update.
    return menuState == 0 && debugMenu.IsOpen() == false;
}

void StatePlayCreative::DrawText(int x,int y, unsigned int color, float size, const char *message, ...)
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

void StatePlayCreative::DrawText2(int x,int y, unsigned int color, float size, const char *message, ...)
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

void StatePlayCreative::CreativePickSlotFromMouse()
{
    float best = 1.0e9f;
    int bestX = invXPosition;
    int bestY = invYPosition;
    for(int row = 0; row <= kCreativeHotbarRow; row++)
    {
        if(row > kCreativeCatRows - 1 && row != kCreativeHotbarRow)
            continue;
        for(int col = 0; col <= 8; col++)
        {
            float dx = creativeMouseX - GuiCreativeSlotX(col);
            float dy = creativeMouseY - GuiCreativeSlotY(row);
            float d = dx * dx + dy * dy;
            if(d < best)
            {
                best = d;
                bestX = col;
                bestY = row;
            }
        }
    }
    invXPosition = (char)bestX;
    invYPosition = (char)bestY;
}

void StatePlayCreative::CreativeUpdateAnalogMouse()
{
    // Wider than the look input's deadzone on purpose. The camera only has to survive a twitch until
    // the next frame, but this is a free cursor: any residual bias walks it across the grid and changes
    // the selected slot behind the player's back. A worn stick rests well off centre, and a per-axis
    // 0.18 was not enough of a floor to hold it still -- the residue is a slow drift rather than a
    // nudge, which is exactly what makes it look like the cursor has a mind of its own.
    const float kDeadzone = 0.28f;
    float ax = 0.0f;
    float ay = 0.0f;
    mSystemMgr->GetAnalogDeadzone(kDeadzone, ax, ay);
    if(ax == 0.0f && ay == 0.0f)
    {
        if(creativeMouseFree)
        {
            creativeMouseFree = false;
            UpdateInvCursorPos();
        }
        return;
    }

    creativeMouseFree = true;
    const float speed = 360.0f;
    creativeMouseX += ax * speed * dt;
    creativeMouseY += ay * speed * dt;

    float minX = GuiCreativeSlotX(0);
    float maxX = GuiCreativeSlotX(8);
    float minY = GuiCreativeSlotY(0);
    float maxY = GuiCreativeSlotY(kCreativeHotbarRow);
    if(creativeMouseX < minX) creativeMouseX = minX;
    if(creativeMouseX > maxX) creativeMouseX = maxX;
    if(creativeMouseY < minY) creativeMouseY = minY;
    if(creativeMouseY > maxY) creativeMouseY = maxY;

    CreativePickSlotFromMouse();
    UpdateInvCursorPos();
}

void StatePlayCreative::UpdateInvCursorPos()
{
    if(invEn)
    {
        float slotX = GuiCreativeSlotX(invXPosition);
        float slotY = GuiCreativeSlotY(invYPosition);
        if(!creativeMouseFree)
        {
            creativeMouseX = slotX;
            creativeMouseY = slotY;
            selectInvSprite->SetPosition(slotX, slotY);
        }
        else
            selectInvSprite->SetPosition(creativeMouseX, creativeMouseY);
        return;
    }
    if(upEn)
    {
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

void StatePlayCreative::DrawAmount(int x,int y, int amount)
{
	int dozens, units;

	dozens = floorf(amount / 10.0f);
	units = amount % 10;

	RenderManager::InstancePtr()->SetFont(0);
	if (dozens != 0)
	{
		RenderManager::InstancePtr()->SetFontStyle(no_size, GU_COLOR(0.25f, 0.25f, 0.25f, 1.0f), 0, 0x00000400 | 0x00004000);
		RenderManager::InstancePtr()->DebugPrint(x + 2, y + 2, "%i", dozens);
		RenderManager::InstancePtr()->SetFontStyle(no_size, GU_COLOR(1.0f, 1.0f, 1.0f, 1.0f), 0, 0x00000400 | 0x00004000);
		RenderManager::InstancePtr()->DebugPrint(x, y, "%i", dozens);
	}

	RenderManager::InstancePtr()->SetFontStyle(no_size, GU_COLOR(0.25f, 0.25f, 0.25f, 1.0f), 0, 0x00000400 | 0x00004000);
	RenderManager::InstancePtr()->DebugPrint(x + 14, y + 2, "%i", units);
	RenderManager::InstancePtr()->SetFontStyle(no_size, GU_COLOR(1.0f, 1.0f, 1.0f, 1.0f), 0, 0x00000400 | 0x00004000);
	RenderManager::InstancePtr()->DebugPrint(x + 12, y, "%i", units);
	RenderManager::InstancePtr()->SetDefaultFont();
}