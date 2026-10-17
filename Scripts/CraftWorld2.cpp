#include "CraftWorld2.h"
#include <Aurora/Graphics/Effects/Arrow2.h>
#include <Aurora/Graphics/Effects/Particle.h>
#include "WorldGenerator.h"
#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/Utils/Logger.h>
#include "TextureHelper.h"
#include "SoundManager.h"
#include "DebugConfig.h"
#include "PerfCounters.h"
#include "ColumnSaver.h"

#include <string.h>
#include <stdio.h>
#include <zlib.h>
#include <pspiofilemgr.h>
#include <pspsysmem.h>
#include <psprtc.h>
#include <pspthreadman.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>

// The whole user partition is grabbed for the malloc heap at startup, less the reserve named by
// PSP_HEAP_THRESHOLD_SIZE_KB (2048) in Scripts.cpp, so sceKernelTotalFreeMemSize() reports that
// reserve and says nothing about what the game can still allocate. Ask the
// allocator instead. mallinfo().fordblks only counts free blocks inside the part
// of the heap sbrk has already handed out, so the untouched tail of the heap block
// (heapTop - sbrk(0)) is added; heapTop is found once by asking the kernel for the
// lowest free address above the heap block.
static int gHeapTopAddr = 0;
static void ProbeHeapTop()
{
    if(gHeapTopAddr != 0)
        return;
    SceUID probe = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "heapprobe", PSP_SMEM_Low, 256, NULL);
    if(probe >= 0)
    {
        gHeapTopAddr = (int)(intptr_t)sceKernelGetBlockHeadAddr(probe);
        sceKernelFreePartitionMemory(probe);
    }
    else
        gHeapTopAddr = -1;
}

// mallinfo() walks every free chunk in the heap. Once streaming has fragmented the
// heap (thousands of freed vertex buffers) that walk costs milliseconds and never
// gets cheaper, and the streamer used to call it 3-5 times per 0.12s tick — a
// permanent stutter that started the moment chunks began streaming. Cache it.
static int gHeapFreeKBCached = -1;
static unsigned int gHeapFreeKBStamp = 0;
int CraftWorld::HeapFreeKB()
{
    unsigned int now = sceKernelGetSystemTimeLow();
    if(gHeapFreeKBCached >= 0 && (now - gHeapFreeKBStamp) < 1000000u)
        return gHeapFreeKBCached;
    struct mallinfo mi = mallinfo();
    long freeBytes = (long)mi.fordblks;
    ProbeHeapTop();
    if(gHeapTopAddr > 0)
    {
        int cur = (int)(intptr_t)sbrk(0);
        if(cur > 0 && gHeapTopAddr > cur)
            freeBytes += gHeapTopAddr - cur;
    }
    gHeapFreeKBCached = (int)(freeBytes / 1024);
    gHeapFreeKBStamp = now;
    return gHeapFreeKBCached;
}

int CraftWorld::HeapUsedKB()
{
    struct mallinfo mi = mallinfo();
    return (int)(mi.uordblks / 1024);
}
#define PI 3.14159f

#define ENGLISH 1

CraftWorld::CraftWorld()
{
    pendingLightCount = 0;

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
    mainOptions.mipMapTexturing = true;
    mainOptions.mipMapBias = 0.0f;
    mainOptions.autoJump = 0;
    mainOptions.fov = 60;
    mainOptions.freezeDayTime = 0;
    mainOptions.worldBlockAnimation = 1;
    mainOptions.particles = 0;
    mainOptions.difficult = 2; // normal
    mainOptions.horizontalViewDistance = 2;
    mainOptions.verticalViewDistance = 1;
    mainOptions.guiDrawing = 1;
    // end
    mainWorldVariables.worldCoalOre = 0;
    mainWorldVariables.worldDiamondOre = 0;
    mainWorldVariables.worldFlowers = 0;
    mainWorldVariables.worldGoldenOre = 0;
    mainWorldVariables.worldIronOre = 0;
    mainWorldVariables.worldLapisOre = 0;
    mainWorldVariables.worldRedstoneOre = 0;
    mainWorldVariables.worldTrees = 0;
    mainWorldVariables.worldClay = 0;
    // end

    blockXConstLight = 0.65f;
    blockZConstLight = 0.85f;

    worldWeather = new WeatherSystem();

    snowBiomePosition = Vector3(0,0,0);
    snowBiomeRadius = -1;
    desertBiomePosition = Vector3(0,0,0);
    desertBiomeRadius = -1;

    worldTime = 0.0f;
    skyTime = 112.0f;

    playerZoneSize = Vector3(16*mainOptions.horizontalViewDistance,16*mainOptions.verticalViewDistance,16*mainOptions.horizontalViewDistance);
    playerSpawnPointPosition = Vector3(0.0f,64,0.0f);
    playerContactLadder = false;

    updateChunksSwitch = false;
    chunksWasSetToUpdate = false;

    updateChunkTimer = 0.0f;
    updateTransparentChunkTimer = 0.12f;
    updateFastChunkTimer = 0.0f;
    fetchNearestChunksTimer = 2.0f;

    chunksCreatedInFrameCount = 0;
    transparentchunksCreatedInFrameCount = 0;
    chunksCreatedInFrameCount2 = 0;
    transparentchunksCreatedInFrameCount2 = 0;

    mId = -1;
    mAm = -1;
    mSt = 0;

    worldSeed = 0;
    worldStamp = 0;
    lowMemoryMode = false;
    // Heap free at world creation: ~38 MB on a 64 MB PSP, under 12 MB on a PSP-1000.
    // What memory this process was actually granted, stated plainly, and what the hardware is not.
    //
    // There is no user-mode API for the hardware model. sceKernelGetModel is kernel-only in this SDK --
    // it is declared in pspsysmem_kernel.h and there is no user-mode import stub for it anywhere in the
    // SDK, so a user-mode PRX cannot link it -- and kubridge's kuKernelGetModel needs a kernel addon
    // installed by the firmware. Calling an unavailable import is not a safe failure here: an
    // unresolved import stops the EBOOT loading at all, which this project has already been bitten by.
    //
    // So the game identifies the memory partition it is actually running in, which is the thing its
    // decisions depend on, and does not claim a model it cannot verify.
    //
    // Recording this is not a downgrade from naming a model. The earlier "PSP-1000" verdict was drawn
    // from this same measurement: it was right about the memory and misleading about the console, and
    // those are two different questions. The log now answers them separately.
    //
    // The threshold is free heap at this point, ~9800 KB on the base partition and ~44000 KB with the
    // expansion granted (both measured), so 20 MB separates them with a wide margin. DebugConfig's
    // MC_FORCE_MEMORY_MODE overrides it, so a console whose firmware does grant the expansion does not
    // depend on a threshold being crossed.
    bigMemory = HeapFreeKB() > 20 * 1024;
#if defined(MC_FORCE_MEMORY_MODE) && MC_FORCE_MEMORY_MODE >= 0
    bigMemory = (MC_FORCE_MEMORY_MODE != 0);
#endif

    Aurora::Utils::Logger::Instance()->LogMessage("memory: freeKB=%d => %s\n", HeapFreeKB(),
        bigMemory ? "EXPANDED (64MB partition granted)" : "BASE (32MB partition)");
    if(!bigMemory)
    {
        Aurora::Utils::Logger::Instance()->LogMessage(
            "memory: running in the base partition. The 64MB partition needs MEMSIZE=1 in PARAM.SFO, "
            "which this EBOOT has, and then the firmware to honour it. On a PSP-2000/3000 where only "
            "the base partition arrives, the firmware is not applying the expansion: check the custom "
            "firmware's own memory-expansion setting, and whether a UMD/ISO is holding the extra RAM "
            "(the PSP-2000/3000 UMD cache does exactly that).\n");
    }
    Aurora::Utils::Logger::Instance()->LogMessage("mob caps bigMemory=%d passive=%d hostile=%d\n",
        bigMemory ? 1 : 0, PassiveMobCap(), HostileMobCap());

    HP = 20;
    HG = 20;
    OS = 10;
    AP = 0;
    foodExhaustion = 0.0f;

    playerGrowth = 1.65f;
    playerCollisionSize = 0.2f;

    worldTime = 0.0f;
    worldDayTime = 12.0f;
    worldHour = 0.0f;
    allChunksUpdateTimer = 0.0f;

    westLight = 0.0f;
    eastLight = 0.0f;

    worldVersion = 151;
    worldLimit = 4096;
    worldType = 0;
    dimension = 0;
    // No dimension has been visited yet, so every slot is empty and the first trip to each one builds
    // its arrival portal rather than restoring a position.
    for(int i = 0; i < 3; i++)
    {
        dimState[i].valid = false;
        dimState[i].playerPos = Vector3(0.0f, 0.0f, 0.0f);
        dimState[i].dayTime = 0.0f;
        dimState[i].portalPos = Vector3(0.0f, 0.0f, 0.0f);
    }
    portalTimer = 0.0f;
    portalCooldown = 0.0f;
    portalWobble = 0.0f;
    portalFade = 0.0f;
    portalLeaveFirst = false;
    travelledThisFrame = false;
    waterLevel = 58;
    makeTrees = true;
    makeWater = true;
    makeCaves = true;
    worldGen = 0;
    worldSavePath = "";
    worldGenerating = false;
    spawnGenClip = false;
    genMinX = 0;
    genMaxX = 4096;
    genMinZ = 0;
    genMaxZ = 4096;
    lastCol = 0;
    lastCx = 0x7fffffff;
    lastCz = 0x7fffffff;
    dummyBlock = 0;
    dummySettings = 0;
    writeSink = 0;
    streamOpsThisFrame = 0;
    pauseAfterTerrain = false;
    colFileSide = 0;
    colFileDim = -1;
    animPendingDt = 0.0f;
    auditLastPos = Vector3(0.0f, 0.0f, 0.0f);
    auditStillFor = 0.0f;
    auditTimer = 0.0f;
    auditShots = 0;
    perfInStreaming = false;
    lastProcessUs = 0;
    streamTickTimer = 0.0f;
    simColCursor = 0;
    meshColCursor = 0;
    simTickTimer = 0.0f;
    meshBuildsThisFrame = 0;
    physicsBoxesThisFrame = 0;
    meshJobKind = kMeshJobNone;
    meshJobId = -1;
    meshJobCursor = 0;
    meshJobFillCursor = 0;
    meshJobCachesReady = false;
    meshJobScanPlants = false;
    meshJobSkin = false;
    meshScanSealed = 0;
    meshScanCol = -1;
    meshGridActive = false;
    meshLightActive = false;
    meshGridCx = 0;
    meshGridCz = 0;
    meshLightOriginX = 0;
    meshLightOriginY = 0;
    meshLightOriginZ = 0;
    meshLightDimX = 0;
    meshLightDimY = 0;
    meshLightDimZ = 0;
    meshPending = true;
    meshIdleTimer = 0.0f;
    viewFrustumValid = false;
    viewArmedValid = false;
    viewArmedDir = Vector3(0.0f, 0.0f, 0.0f);
    meshWatchCx = 0x7fffffff;
    meshWatchCz = 0x7fffffff;
    meshWatchCy = 0x7fffffff;
    meshPendPosX = 1.0e9f;
    meshPendPosY = 1.0e9f;
    meshPendPosZ = 1.0e9f;
    for(int gz = 0; gz < 3; gz++)
    {
        for(int gx = 0; gx < 3; gx++)
            meshColGrid[gx][gz] = 0;
    }
    m_Blocks = 0;
    m_BlockSettings = 0;
    for(int i = 0; i < kColHashSize; i++)
        colHash[i] = 0;

    mChestsize = 0;
    mFurnacesize = 0;
    mZombiesize = 0;
    mDirectsize = 0;
    mCowsize = 0;
    parkedPassiveCount = 0;
    passiveHerdTimer = 0.0f;
    mCreepersize = 0;
	mPigsize = 0;
    mSheepsize = 0;
    mTNTsize = 0;
    mNoteBlocksize = 0;
    mJukeboxsize = 0;
    mMonsterSpawnersize = 0;
    mItemFramesize = 0;

    zombieNum = 0;//14
    dropNum = 0;
    TNTnum = 0;

    spawnCowsTimer = 0.0f;
    spawnZombiesTimer = 0.0f;
    spawnCreeperTimer = 0.0f;
    spawnSheepTimer = 0.0f;
    spawnChickenTimer = 0.0f;
    spawnSpiderTimer = 0.0f;
    spawnSkeletonTimer = 0.0f;

    updatingChunksTimer = 0.0f;
    updatingChunksTimerMax = 1.0f;
    updatingChunksPosition = 0;

    gameModeWorld = 0;
    gameLocked = false;

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

    minuteTimer = 0.0f;
    lightShadowFactor = 0.29f;
    lightFactor = Vector3(1.0f,1.0f,1.0f);

	dawnSunsetClearSkyColor = Vector3(0,0,0);
	startClearSkyColor = Vector3(0,0,0);
	endClearSkyColor = Vector3(0,0,0);

	dawnSunsetRainySkyColor = Vector3(0,0,0);
	startRainySkyColor = Vector3(0,0,0);
	endRainySkyColor = Vector3(0,0,0);

    dawnSunsetSkyColor = Vector3(0,0,0);
	startSkyColor = Vector3(0,0,0);
	endSkyColor = Vector3(0,0,0);

	rainyColorAlpha = 0.0f;

    brightFactor = 1.0f;
    starsFactor = 0.0f;
    dawnSunsetFactor = 0.0f;

    hatVertices = NULL;
    bodyVertices = NULL;
    headVertices = NULL;
    legVertices = NULL;
    handVertices = NULL;

    creeperLegVertices = NULL;

    pigBodyVertices = NULL;
    pigHeadVertices = NULL;
    pigSnoutVertices = NULL;
    pigLegVertices = NULL;

    cowBodyVertices = NULL;
    cowHeadVertices = NULL;
    cowLegVertices = NULL;
    cowHornsVertices = NULL;

    sheepBodyVertices = NULL;
    sheepHeadVertices = NULL;
    sheepHeadFurVertices = NULL;
    sheepLegFurVertices = NULL;
    sheepLegOPTVertices = NULL;
    sheepHeadOPTVertices = NULL;

    chickenBodyVertices = NULL;
    chickenHeadVertices = NULL;
    chickenLegVertices = NULL;
    chickenWingVertices = NULL;

    spiderBodyVertices = NULL;
    spiderNeckVertices = NULL;
    spiderHeadVertices = NULL;
    spiderLegVertices = NULL;
    spiderLegLeftVertices = NULL;

    // Skeleton limbs only; its head and body are the zombie's buffers (ModelSkeleton inherits them).
    skeletonHandVertices = NULL;
    skeletonLegVertices = NULL;

    helmetVertices = NULL;
    shoulderVertices = NULL;
    chestplateVertices = NULL;
    bellyVertices = NULL;
    leggingVertices = NULL;
    bootVertices = NULL;

    kickedStart = false;
    kickedBy = 0;
    kickedDamage = 0;
    kickedAngle = 0.0f;
    kickedTimer = 0.0f;
    kickedVelY = 0.0f;
    kickedPush = 0.0f;

    iVertexLeftRight = 0;
    pointsLeftRight = 0;
    iVertexBackFront = 0;
    pointsBackFront = 0;
    iVertexBottomTop = 0;
    pointsBottomTop = 0;

    textureTerrainId = 0;
    textureItemsId = 0;
    textureWaterAnimationId = 0;
    textureLavaAnimationId = 0;
	textureFireAnimationId = 0;
	textureFireAnimation2Id = 0;
	texturePortalAnimationId = 0;
    textureLeatherArmor1Id = 0;
    textureLeatherArmor2Id = 0;
    textureChainArmor1Id = 0;
    textureChainArmor2Id = 0;
    textureIronArmor1Id = 0;
    textureIronArmor2Id = 0;
    textureDiamondArmor1Id = 0;
    textureDiamondArmor2Id = 0;
    textureGoldenArmor1Id = 0;
    textureGoldenArmor2Id = 0;
    textureVegetationColorMapId = 0;
    textureTerrainMipsId = 0;
    textureClockId = 0;
    textureCompassId = 0;

    animationTimer = 0.0f;
    animationTimerEnd = 0.085f;
    animationWaterFrame = 0;
    animationLavaFrame = 0;
	animationFireFrame = 0;

    animationLavaStep = true;

    clockFrame = -1;
    lastClockFrame = -1;

    compassFrame = -1;
    lastCompassFrame = -1;

    InitBlockVector();
    InitItemVector();
    UpdateBlocksAndItemsName();

   // FetchNearestChunks();

    spawner_x = 0;
    spawner_z = 0;
    percent = 0.1f;
    percent2 = 0.1f;
    percent3 = 0.1f;
    texutreSize = 0;
    textureChunk = 0;
}

void CraftWorld::UpdateBlocksAndItemsName()
{
}


CraftWorld::~CraftWorld()
{
    // Columns still being written by the saver are finished and handed back before anything else
    // goes, then the worker is stopped for this world.
    ColumnSaver::Stop();
    ReapSavedColumns();
    for(unsigned int i = 0; i < loadedColumns.size(); i++)
        delete loadedColumns[i];
    loadedColumns.clear();
    for(int i = 0; i < kColHashSize; i++)
        colHash[i] = 0;
    lastCol = 0;
    if(m_Blocks)
        delete[] m_Blocks;
    if(m_BlockSettings)
        delete[] m_BlockSettings;
    m_Blocks = 0;
    m_BlockSettings = 0;
    if(worldGen)
    {
        delete worldGen;
        worldGen = 0;
    }

    //delete chests
    for(unsigned int i = 0; i < mChunks.size(); i++)
    {
        if(mChunks[i])
            delete mChunks[i];
    }
    mChunks.clear();

    //water chunks
    for(unsigned int i = 0; i < mTransparentChunks.size(); i++)
    {
        if(mTransparentChunks[i])
            delete mTransparentChunks[i];
    }
    mTransparentChunks.clear();

    for(unsigned int i = 0; i < mChests.size(); i++)
    {
        delete mChests[i];
    }
    mChests.clear();

    //delete furnace
    for(unsigned int i = 0; i < mFurnaces.size(); i++)
    {
        delete mFurnaces[i];
    }
    mFurnaces.clear();

    //delete note blocks
    for(unsigned int i = 0; i < mNoteBlocks.size(); i++)
    {
        delete mNoteBlocks[i];
    }
    mNoteBlocks.clear();

    //delete jukeboxes
    for(unsigned int i = 0; i < mJukeboxes.size(); i++)
    {
        delete mJukeboxes[i];
    }
    mJukeboxes.clear();

    //delete monster spawners
    for(unsigned int i = 0; i < mMonsterSpawners.size(); i++)
    {
        delete mMonsterSpawners[i];
    }
    mMonsterSpawners.clear();

    //delete item frames
    for(unsigned int i = 0; i < mItemFrames.size(); i++)
    {
        delete mItemFrames[i];
    }
    mItemFrames.clear();

    //delete all direction mind blocks
    for(unsigned int i = 0; i < mDirects.size(); i++)
    {
        delete mDirects[i];
    }
    mDirects.clear();

    //delete all zombies
    for(unsigned int i = 0; i < mZombies.size(); i++)
    {
        delete mZombies[i];
    }
    mZombies.clear();

    //delete all cows
    for(unsigned int i = 0; i < mCows.size(); i++)
    {
        delete mCows[i];
    }
    mCows.clear();

    //delete all creepers
    for(unsigned int i = 0; i < mCreepers.size(); i++)
    {
        delete mCreepers[i];
    }
    mCreepers.clear();

	//delete all pigs
    for(unsigned int i = 0; i < mPigs.size(); i++)
    {
        delete mPigs[i];
    }
    mPigs.clear();

    //delete all sheeps
    for(unsigned int i = 0; i < mSheeps.size(); i++)
    {
        delete mSheeps[i];
    }
    mSheeps.clear();

    //delete all chickens
    for(unsigned int i = 0; i < mChickens.size(); i++)
    {
        delete mChickens[i];
    }
    mChickens.clear();

    //delete all spiders
    for(unsigned int i = 0; i < mSpiders.size(); i++)
    {
        delete mSpiders[i];
    }
    mSpiders.clear();

    //delete all skeletons
    for(unsigned int i = 0; i < mSkeletons.size(); i++)
    {
        delete mSkeletons[i];
    }
    mSkeletons.clear();

    // Arrows in flight: they outlive the mob that fired them, so they are cleaned up on their own.
    for(unsigned int i = 0; i < mArrows.size(); i++)
    {
        delete mArrows[i];
    }
    mArrows.clear();

    if(mDrops.empty() == false)
    {
        for(unsigned int i = 0; i < mDrops.size(); i++)
        {
            delete mDrops[i];
        }
        mDrops.clear();
    }

    if(mTNTs.empty() == false)
    {
        for(unsigned int i = 0; i < mTNTs.size(); i++)
        {
            delete mTNTs[i];
        }
        mTNTs.clear();
    }

    delete worldWeather;

    // The block and item meshes belong to the world, not to BaseBlock/BaseItem, so they have to be
    // released here -- before the vectors that hold the pointers are emptied. See FreeBlockTypeMeshes.
    FreeBlockTypeMeshes();
    FreeItemTypeMeshes();
    blockTypes.clear();
    itemTypes.clear();
    toUpdate.clear();
    toFastUpdate.clear();
    redstoneMeshQueue.clear();
    redstoneMeshOpaque.clear();
    updatingChunks.clear();

    // Any section rebuild in flight is about to have its chunks freed underneath it; the job's own
    // MeshIdValid test would catch that a frame later, but dropping it here keeps the new world from
    // resuming a walk against a section id that now means something else.
    meshJobKind = kMeshJobNone;
    meshJobId = -1;
    meshJobCursor = 0;
    meshJobFillCursor = 0;
    meshJobCachesReady = false;
    free(sheepBodyVertices);
    free(sheepHeadVertices);
    free(sheepHeadFurVertices);
    free(sheepLegFurVertices);
    //free(sheepLegOPTVertices);
    //free(sheepHeadOPTVertices);

    free(creeperLegVertices);

    free(pigBodyVertices);
    free(pigHeadVertices);
    free(pigSnoutVertices);
    free(pigLegVertices);

    free(chickenBodyVertices);
    free(chickenHeadVertices);
    free(chickenLegVertices);
    free(chickenWingVertices);

    free(spiderBodyVertices);
    free(spiderNeckVertices);
    free(spiderHeadVertices);
    free(spiderLegVertices);
    free(spiderLegLeftVertices);
    free(skeletonHandVertices);
    free(skeletonLegVertices);

    free(hatVertices);
    free(bodyVertices);
    free(headVertices);
    free(legVertices);
    free(handVertices);

    free(cowBodyVertices);
    free(cowHeadVertices);
    free(cowLegVertices);
    free(cowHornsVertices);

    free(helmetVertices);
    free(shoulderVertices);
    free(chestplateVertices);
    free(bellyVertices);
    free(leggingVertices);
    free(bootVertices);
}

void CraftWorld::initWorld(int worldSize, int worldHeight, int chunkSize)
{
    WORLD_SIZE = worldSize;
    CHUNK_SIZE = chunkSize;
    WORLD_HEIGHT = worldHeight;
    worldLimit = 4096;
    if(worldLimit < WORLD_SIZE)
        worldLimit = WORLD_SIZE;
    m_Blocks = 0;
    m_BlockSettings = 0;
}

int CraftWorld::HashCol(int cx, int cz) const
{
    return (int)(((unsigned)cx * 73856093u) ^ ((unsigned)cz * 19349663u)) & (kColHashSize - 1);
}

void CraftWorld::HashInsertColumn(ChunkColumn* col)
{
    int h = HashCol(col->cx, col->cz);
    col->hashNext = colHash[h];
    colHash[h] = col;
    loadedColumns.push_back(col);
}

void CraftWorld::HashRemoveColumn(ChunkColumn* col)
{
    int h = HashCol(col->cx, col->cz);
    ChunkColumn** prev = &colHash[h];
    while(*prev)
    {
        if(*prev == col)
        {
            *prev = col->hashNext;
            break;
        }
        prev = &((*prev)->hashNext);
    }
    for(unsigned int i = 0; i < loadedColumns.size(); i++)
    {
        if(loadedColumns[i] == col)
        {
            loadedColumns.erase(loadedColumns.begin() + i);
            break;
        }
    }
    if(lastCol == col)
        lastCol = 0;
}

ChunkColumn* CraftWorld::FindColumn(int cx, int cz)
{
    if(lastCol && lastCx == cx && lastCz == cz)
        return lastCol;
    int h = HashCol(cx, cz);
    for(ChunkColumn* p = colHash[h]; p; p = p->hashNext)
    {
        if(p->cx == cx && p->cz == cz)
        {
            lastCol = p;
            lastCx = cx;
            lastCz = cz;
            return p;
        }
    }
    return 0;
}

void CraftWorld::PinLastColumn(ChunkColumn* col)
{
    lastCol = col;
    if(!col)
        return;
    lastCx = col->cx;
    lastCz = col->cz;
}

int CraftWorld::LocalBlockIndex(ChunkColumn* col, int x, int y, int z) const
{
    int lx = x - col->cx * col->chunkSize;
    int lz = z - col->cz * col->chunkSize;
    return col->Index(lx, y, lz);
}

bool CraftWorld::MeshIdValid(int id) const
{
    return id >= 0 && id < (int)mChunks.size() && mChunks[id] != 0 && id < (int)mTransparentChunks.size() && mTransparentChunks[id] != 0;
}

void CraftWorld::SetupSpawnGeneration()
{
    spawnGenClip = true;
    MC_PERF_RESET();
    Aurora::Utils::Logger::Instance()->LogMessage("world start heapUsedKB=%d heapFreeKB=%d\n", HeapUsedKB(), HeapFreeKB());
    MC_DebugHeapCheckpoint("world start");
    int view = EffectiveViewDistance();
    if(view < 1)
        view = 1;
    int padChunks = view + 2;
    if(padChunks < 4)
        padChunks = 4;
    // 32 MB PSP: keep the up-front spawn region to the view ring plus one buffer;
    // the rest streams in exactly the same way, so the world looks identical.
    if(HeapFreeKB() < 12 * 1024)
    {
        padChunks = view + 1;
        if(padChunks < 3)
            padChunks = 3;
        lowMemoryMode = true;
    }
    // Spawn sits in the middle of the streamable area so every direction has room.
    int mid = worldLimit / 2;
    if(CHUNK_SIZE > 0)
        mid = (mid / CHUNK_SIZE) * CHUNK_SIZE;
    int half = WORLD_SIZE / 2;
    int padBlocks = padChunks * CHUNK_SIZE;
    genMinX = mid - padBlocks;
    genMaxX = mid + padBlocks;
    genMinZ = mid - padBlocks;
    genMaxZ = mid + padBlocks;
    if(genMinX < mid - half) genMinX = mid - half;
    if(genMinZ < mid - half) genMinZ = mid - half;
    if(genMaxX > mid + half) genMaxX = mid + half;
    if(genMaxZ > mid + half) genMaxZ = mid + half;
    if(genMinX < 0) genMinX = 0;
    if(genMinZ < 0) genMinZ = 0;
    if(CHUNK_SIZE > 0)
    {
        genMinX = (genMinX / CHUNK_SIZE) * CHUNK_SIZE;
        genMinZ = (genMinZ / CHUNK_SIZE) * CHUNK_SIZE;
        genMaxX = ((genMaxX + CHUNK_SIZE - 1) / CHUNK_SIZE) * CHUNK_SIZE;
        genMaxZ = ((genMaxZ + CHUNK_SIZE - 1) / CHUNK_SIZE) * CHUNK_SIZE;
        if(genMaxX > worldLimit) genMaxX = worldLimit;
        if(genMaxZ > worldLimit) genMaxZ = worldLimit;
        for(int z = genMinZ; z < genMaxZ; z += CHUNK_SIZE)
        {
            for(int x = genMinX; x < genMaxX; x += CHUNK_SIZE)
                EnsureColumn(ChunkFloorDiv(x, CHUNK_SIZE), ChunkFloorDiv(z, CHUNK_SIZE), false);
        }
    }
}

bool CraftWorld::InGenBounds(int x, int z) const
{
    return x >= genMinX && x < genMaxX && z >= genMinZ && z < genMaxZ;
}

bool CraftWorld::SectionInPlayerZone(int startX, int startY, int startZ) const
{
    BoundingBox sectionBB(Vector3((float)startX, (float)startY, (float)startZ),
                          Vector3((float)(startX + CHUNK_SIZE), (float)(startY + CHUNK_SIZE), (float)(startZ + CHUNK_SIZE)));
    return playerZoneBB.intersect(sectionBB);
}

// The zone's footprint only: is the section's column within the horizontal zone?
bool CraftWorld::SectionInPlayerZoneXZ(int startX, int startZ) const
{
    return (float)(startX + CHUNK_SIZE) > playerZoneBB.min.x && (float)startX < playerZoneBB.max.x
        && (float)(startZ + CHUNK_SIZE) > playerZoneBB.min.z && (float)startZ < playerZoneBB.max.z;
}

// Does the ground pass through this section? The column's surface band (ChunkColumn::surfaceMinY..
// surfaceY) in sections.
bool CraftWorld::SectionInSurfaceBand(const ChunkColumn* col, int startY) const
{
    if(!col || !col->surfaceKnown || CHUNK_SIZE <= 0)
        return false;
    const int cy = startY / CHUNK_SIZE;
    return cy >= col->surfaceMinY / CHUNK_SIZE && cy <= col->surfaceY / CHUNK_SIZE;
}

// The zone, as the mesher and the renderer both read it. The box is +-16*verticalViewDistance around
// the eye, which is right for a player on the ground. For a flier it is wrong in a way that does not
// fill in: at y=95 over ground at 60..64 the sections holding the ground below 64 are outside the box,
// so they are never built and never drawn, and the terrain shows a hole wherever it dips -- a hole
// that stays however long the player hovers. So a section the ground passes through is in the zone
// whenever its column is, at any height. Caves and the air stay governed by the box.
bool CraftWorld::SectionInPlayerZone(int startX, int startY, int startZ, const ChunkColumn* col) const
{
    if(SectionInPlayerZone(startX, startY, startZ))
        return true;
    return SectionInSurfaceBand(col, startY) && SectionInPlayerZoneXZ(startX, startZ);
}

bool CraftWorld::MeshInPlayerZone(const SimpleMeshChunk* mesh) const
{
    if(!mesh)
        return false;
    BoundingBox box = mesh->bBox;
    if(playerZoneBB.intersect(box))
        return true;
    return mesh->surfaceBand && SectionInPlayerZoneXZ(mesh->chunkStartX, mesh->chunkStartZ);
}

// Should this section be given geometry at all?
//
// This is the "do not build what the camera cannot see" gate. It deliberately does NOT run once a
// section already has a mesh (both call sites check for that first), so nothing is ever unmeshed mid-
// session -- a section the player has looked at keeps its geometry and turning back to it costs
// nothing. What it prevents is the opposite case: a column that streams in beside or behind the player
// being meshed on sight, which is vertex memory spent on terrain nobody has seen, and build time taken
// out of a frame that could have used it up front.
//
// One chunk of margin, so a section is built slightly before it is genuinely on screen rather than
// popping in exactly at the screen edge as the player turns. The margin is also what keeps the ground
// immediately around the player built while they look straight ahead, where the near plane otherwise
// cuts through the section under their feet.
//
// Note the sense of the invalid case: until an actual frustum has been stored, everything is meshed,
// exactly as before this gate existed. That keeps world entry, the loading screen and any path that
// never draws working unchanged, and it means this can only ever remove work that provably has no
// frustum to be visible in.
bool CraftWorld::SectionInView(int startX, int startY, int startZ)
{
#if !MC_MESH_VISIBLE_ONLY
    (void)startX; (void)startY; (void)startZ;
    return true;
#else
    // No frustum yet means world entry or a dimension arrival, before the first frame has been drawn --
    // there is no honest answer to "can the camera see it" from here, because the frustum is only ever
    // computed during rendering (RenderManager::ExtractPlanes). Answering "yes, mesh it" is what the
    // code did before this gate existed, and it is kept.
    //
    // It was briefly changed to mesh only the player's immediate surroundings here, to stop the nether
    // building 75 sections (5.7 MB) at world entry. It does free that memory -- entry `mallUsedKB` fell
    // 15944 -> 11198 -- but it makes the nether WORSE, and the mechanism is worth recording because it
    // is counter-intuitive: deferring the entry mesh creates a backlog that is then built later, when
    // the heap is already full. Measured on the nether probe, same walk: mesh OOMs 1 -> 125 and
    // `mem pressure` 5 -> 27. Meshing EARLY, while there is still room, beats meshing late. Reverted.
    if(!viewFrustumValid)
        return true;
    const float m = (float)CHUNK_SIZE;
    return viewFrustum.BoxInFrustum((float)startX - m, (float)startY - m, (float)startZ - m,
                                    (float)(startX + CHUNK_SIZE) + m,
                                    (float)(startY + CHUNK_SIZE) + m,
                                    (float)(startZ + CHUNK_SIZE) + m) != Frustum::Outside;
#endif
}

// The frustum's forward axis, as the sum of the four side-plane normals.
//
// The side planes are paired left/right and top/bottom with inward normals, so their lateral components
// cancel in the sum and what is left points down the view axis. Exactness is not wanted here: this feeds
// a dot product against the last direction the mesher was armed on, so an approximate axis is enough to
// tell "the player turned" from "the player is still looking the same way".
Vector3 CraftWorld::ViewDirectionFromFrustum() const
{
    Vector3 d(0.0f, 0.0f, 0.0f);
    d += viewFrustum.planes[Frustum::FRUSTUM_PLANE_LEFT].n;
    d += viewFrustum.planes[Frustum::FRUSTUM_PLANE_RIGHT].n;
    d += viewFrustum.planes[Frustum::FRUSTUM_PLANE_BOTTOM].n;
    d += viewFrustum.planes[Frustum::FRUSTUM_PLANE_TOP].n;
    const float len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    if(len > 1.0e-6f)
        d = d / len;
    return d;
}

// The ground's band for a column that did not come through the generator this session (read from a
// file, or generated before the band was recorded): the y of the highest and lowest non-air cell per
// (x,z), once per column. Trees and plants count as surface here, which only widens the band upward.
void CraftWorld::EnsureSurfaceBand(ChunkColumn* col)
{
    if(!col || col->surfaceKnown || !col->blocks)
        return;
    int maxY = 0;
    int minY = col->height - 1;
    const int cs = col->chunkSize;
    for(int lz = 0; lz < cs; lz++)
    {
        for(int lx = 0; lx < cs; lx++)
        {
            int y = col->height - 1;
            const block_t* b = col->blocks + col->Index(lx, 0, lz);
            while(y > 0 && b[y * cs] == 0)
                y--;
            if(y > maxY) maxY = y;
            if(y < minY) minY = y;
        }
    }
    if(minY > maxY)
        minY = maxY;
    col->surfaceY = maxY;
    col->surfaceMinY = minY;
    col->surfaceKnown = true;
}

int CraftWorld::SectionMeshPriority(int startX, int startY, int startZ, const ChunkColumn* col) const
{
    if(CHUNK_SIZE <= 0)
        return 0;
    int pcx = ChunkFloorDiv((int)playerPos.x, CHUNK_SIZE);
    int pcz = ChunkFloorDiv((int)playerPos.z, CHUNK_SIZE);
    int scx = ChunkFloorDiv(startX, CHUNK_SIZE);
    int scz = ChunkFloorDiv(startZ, CHUNK_SIZE);
    int adx = scx - pcx; if(adx < 0) adx = -adx;
    int adz = scz - pcz; if(adz < 0) adz = -adz;
    int pcy = (int)playerPos.y / CHUNK_SIZE;
    int scy = CHUNK_SIZE > 0 ? startY / CHUNK_SIZE : 0;
    int ady = scy - pcy; if(ady < 0) ady = -ady;

    // The section around the player is the one that reads as the chunk that did not load.
    if(adx == 0 && adz == 0 && ady <= 1)
        return -100000 + ady;

    // The ground is as near as the player is. Measured from the player alone, a flier at y=80 over
    // ground at y=60 had the air section with the flowers in it (64..80) built before the ground under
    // them (48..64), and the flowers hung in the sky. A section the surface passes through, or the
    // one the player is in, is vertical distance 0; everything else is its distance to the nearer of
    // the two. Underground, the surface band is far above and the player's own section still wins.
    if(col && col->surfaceKnown)
    {
        int lo = col->surfaceMinY / CHUNK_SIZE;
        int hi = col->surfaceY / CHUNK_SIZE;
        int sd = scy < lo ? lo - scy : (scy > hi ? scy - hi : 0);
        if(sd < ady)
            ady = sd;
    }

    // Distance decides. The camera only orders sections inside the same ring, so a far chunk in front
    // can never jump a near one the player is standing next to. Ring is the chebyshev distance: the
    // eight columns touching the player's are all ring 1, including the diagonal ones being crossed.
    int ring = adx > adz ? adx : adz;
    int look = 128;
    if(viewArmedValid)
    {
        float dx = (float)(startX + CHUNK_SIZE / 2) - playerPos.x;
        float dy = (float)(startY + CHUNK_SIZE / 2) - playerPos.y;
        float dz = (float)(startZ + CHUNK_SIZE / 2) - playerPos.z;
        float len = sqrtf(dx * dx + dy * dy + dz * dz);
        float c = 0.0f;
        if(len > 0.001f)
            c = (dx * viewArmedDir.x + dy * viewArmedDir.y + dz * viewArmedDir.z) / len;
        // 0 straight ahead, 255 straight behind.
        look = (int)((1.0f - c) * 127.5f);
        if(look < 0) look = 0;
        if(look > 255) look = 255;
        // Already crossed and two or more chunks back: it waits one ring. The ring the player touches
        // is never pushed back, because that is the ground under their feet.
        if(c < 0.0f && ring >= 2)
            ring++;
    }
    if(ady > 15)
        ady = 15;
    return ring * 4096 + ady * 256 + look;
}

bool CraftWorld::BlockIsMeshWall(block_t id) const
{
    if(id == 0 || id >= blockTypes.size())
        return false;
    // Water is transparent (and a non-cube model). So are glass, leaves and plants. A downward
    // walk may stop on stone under them; it may not stop on them.
    if(blockTypes[id].transparent)
        return false;
    if(blockTypes[id].blockModel != 0)
        return false;
    return true;
}

bool CraftWorld::CellBuriedInWall(int x, int y, int z)
{
    if(!BlockIsMeshWall(GetBlockNoCheck(x, y, z)))
        return false;
    const int ox[6] = { 1, -1, 0, 0, 0, 0 };
    const int oy[6] = { 0, 0, 1, -1, 0, 0 };
    const int oz[6] = { 0, 0, 0, 0, 1, -1 };
    for(int i = 0; i < 6; i++)
    {
        int nx = x + ox[i];
        int ny = y + oy[i];
        int nz = z + oz[i];
        if(ny < 0 || ny >= WORLD_HEIGHT || nx < 0 || nz < 0 || nx >= worldLimit || nz >= worldLimit)
            return false;
        if(!BlockIsMeshWall(GetBlockNoCheck(nx, ny, nz)))
            return false;
    }
    return true;
}

bool CraftWorld::SectionHasExposedFaces(int startX, int startY, int startZ)
{
    int endY = startY + CHUNK_SIZE;
    if(endY > WORLD_HEIGHT)
        endY = WORLD_HEIGHT;
    if(CHUNK_SIZE <= 0)
        return false;

    ChunkColumn* col = FindColumn(ChunkFloorDiv(startX, CHUNK_SIZE), ChunkFloorDiv(startZ, CHUNK_SIZE));
    bool hasOpen = false;     // air, transparent or model blocks: faces next to these are drawn
    bool hasContent = false;  // anything that is not air (water counts, it has a mesh)
    if(col)
    {
        int cs = col->chunkSize;
        for(int y = startY; y < endY; y++)
        {
            for(int lz = 0; lz < cs; lz++)
            {
                for(int lx = 0; lx < cs; lx++)
                {
                    block_t id = col->blocks[col->Index(lx, y, lz)];
                    if(id != 0)
                        hasContent = true;
                    if(id == 0 || id >= blockTypes.size() || blockTypes[id].transparent || blockTypes[id].blockModel > 0)
                        hasOpen = true;
                    if(hasOpen && hasContent)
                        return true;
                }
            }
        }
    }
    else
        return true;
    if(!hasContent)
        return false;

    auto openAt = [this](int nx, int ny, int nz) -> bool
    {
        if(ny < 0)
            return false;
        if(ny >= WORLD_HEIGHT)
            return true;
        if(nx < 0 || nz < 0 || nx >= worldLimit || nz >= worldLimit)
            return true;
        block_t nid = _GetBlockNoCheck(nx, ny, nz);
        return nid == 0 || nid >= blockTypes.size() || blockTypes[nid].transparent;
    };

    int endX = startX + CHUNK_SIZE;
    int endZ = startZ + CHUNK_SIZE;
    for(int z = startZ; z < endZ; z++)
    {
        for(int y = startY; y < endY; y++)
        {
            if(openAt(startX - 1, y, z) || openAt(endX, y, z))
                return true;
        }
    }
    for(int x = startX; x < endX; x++)
    {
        for(int y = startY; y < endY; y++)
        {
            if(openAt(x, y, startZ - 1) || openAt(x, y, endZ))
                return true;
        }
    }
    for(int x = startX; x < endX; x++)
    {
        for(int z = startZ; z < endZ; z++)
        {
            if(openAt(x, startY - 1, z) || openAt(x, endY, z))
                return true;
        }
    }
    return false;
}

bool CraftWorld::SectionNeedsSimulation(int startX, int startY, int startZ)
{
    int endX = startX + CHUNK_SIZE;
    int endY = startY + CHUNK_SIZE;
    int endZ = startZ + CHUNK_SIZE;
    if(endY > WORLD_HEIGHT)
        endY = WORLD_HEIGHT;
    for(int z = startZ; z < endZ; z++)
    {
        for(int y = startY; y < endY; y++)
        {
            for(int x = startX; x < endX; x++)
            {
                block_t id = _GetBlockNoCheck(x, y, z);
                if(id == 0 || id >= blockTypes.size())
                    continue;
                if(blockTypes[id].update && !BlockNeedsPhysicsTick(id))
                    return true;
            }
        }
    }
    return false;
}

bool CraftWorld::BlockNeedsPhysicsTick(block_t id)
{
    if(id == 0)
        return false;
    if(BlockIsWater(id) || BlockIsLava(id))
        return true;
    return id == GravitySandBlock::getID() || id == Gravel::getID() || id == Fire::getID()
        || id == white_concrete_powder::getID()
        || id == yellow_concrete_powder::getID()
        || id == pink_concrete_powder::getID()
        || id == gray_concrete_powder::getID()
        || id == purple_concrete_powder::getID()
        || id == blue_concrete_powder::getID()
        || id == green_concrete_powder::getID()
        || id == red_concrete_powder::getID()
        || id == black_concrete_powder::getID();
}

bool CraftWorld::BoxNeedsPhysics(int x0, int y0, int z0, int x1, int y1, int z1)
{
    if(CHUNK_SIZE <= 0 || x1 <= x0 || y1 <= y0 || z1 <= z0)
        return false;
    int cx0 = ChunkFloorDiv(x0, CHUNK_SIZE);
    int cz0 = ChunkFloorDiv(z0, CHUNK_SIZE);
    int cx1 = ChunkFloorDiv(x1 - 1, CHUNK_SIZE);
    int cz1 = ChunkFloorDiv(z1 - 1, CHUNK_SIZE);
    for(int cz = cz0; cz <= cz1; cz++)
    {
        for(int cx = cx0; cx <= cx1; cx++)
        {
            ChunkColumn* col = FindColumn(cx, cz);
            if(!col)
                continue;
            int cs = col->chunkSize;
            int wx0 = cx * cs;
            int wz0 = cz * cs;
            int lx0 = x0 - wx0;
            int lz0 = z0 - wz0;
            int lx1 = x1 - wx0;
            int lz1 = z1 - wz0;
            if(lx0 < 0) lx0 = 0;
            if(lz0 < 0) lz0 = 0;
            if(lx1 > cs) lx1 = cs;
            if(lz1 > cs) lz1 = cs;
            int yy0 = y0;
            int yy1 = y1;
            if(yy0 < 0) yy0 = 0;
            if(yy1 > col->height) yy1 = col->height;
            for(int lz = lz0; lz < lz1; lz++)
            {
                for(int y = yy0; y < yy1; y++)
                {
                    int base = y * cs + lz * cs * col->height;
                    for(int lx = lx0; lx < lx1; lx++)
                    {
                        if(BlockNeedsPhysicsTick(col->blocks[base + lx]))
                            return true;
                    }
                }
            }
        }
    }
    return false;
}

bool CraftWorld::SectionNeedsPhysics(int startX, int startY, int startZ)
{
    return BoxNeedsPhysics(startX, startY, startZ, startX + CHUNK_SIZE, startY + CHUNK_SIZE, startZ + CHUNK_SIZE);
}

unsigned int CraftWorld::HashSectionBlocks(int startX, int startY, int startZ)
{
    unsigned int h = 2166136261u;
    if(CHUNK_SIZE <= 0)
        return 0;
    ChunkColumn* col = FindColumn(ChunkFloorDiv(startX, CHUNK_SIZE), ChunkFloorDiv(startZ, CHUNK_SIZE));
    if(!col)
        return 0;
    int cs = col->chunkSize;
    int endY = startY + cs;
    if(endY > col->height)
        endY = col->height;
    int y0 = startY;
    if(y0 < 0)
        y0 = 0;
    for(int lz = 0; lz < cs; lz++)
    {
        for(int y = y0; y < endY; y++)
        {
            int base = y * cs + lz * cs * col->height;
            for(int lx = 0; lx < cs; lx++)
            {
                h ^= (unsigned int)col->blocks[base + lx];
                h *= 16777619u;
            }
        }
    }
    return h;
}

// ---------------------------------------------------------------------------
// Columns given back under memory pressure, and the reads that must not put them straight back.
//
// Unloading a column was a pure loss on this console, and the log is unambiguous about it:
//
//   mem pressure: freeKB=2431 gave back a column, columns=36
//   mem pressure: freeKB=2431 gave back a column, columns=36     <- the count never moves
//   mem pressure: freeKB=2431 gave back a column, columns=36
//
// The count is fixed at 36 because every column the pressure path gave back was re-created within
// the same tick. Creating a column is not cheap: EnsureColumn runs the terrain generator, then
// InitColumnLighting over the whole 16x16x128 column (32,768 cells), then RelightSharedEdges and
// DirtyNeighborMeshes, and the remesh that follows. The unload costs a SaveColumnFile write to the
// memory stick as well. So each cycle paid a card write, a terrain generation, a full-column light
// scan and a remesh -- 3 to 6 times a second, for the entire session, changing nothing.
//
// The re-creation is not the streamer. The streamer only walks its own ring, and the ring the
// pressure path used was deliberately the same or smaller. It is ordinary block access: GetBlock,
// GetBlockSettings and their neighbours call EnsureColumn(.., true) on demand, so a single read of a
// block in a column that was just given back regenerates the whole column to answer it. That read
// repeats every frame, so the two cancel out permanently.
//
// The fix is to answer such a read without rebuilding the column. A column that was evicted a moment
// ago and that lies outside the streaming ring is one the streamer will not ask for again, so the
// read is for a block the player cannot see and returning "no column" is the correct answer -- the
// callers already handle null (writeSink / dummySettings). The memory is what was wanted; giving it
// back to answer an invisible read is not.
// ---------------------------------------------------------------------------
#define kEvictedRemember 24
static int gEvictedCx[kEvictedRemember];
static int gEvictedCz[kEvictedRemember];
static unsigned int gEvictedAt[kEvictedRemember];
static int gEvictedNext = 0;
static bool gEvictedInit = false;

static void RememberEvictedColumn(int cx, int cz)
{
    if(!gEvictedInit)
    {
        gEvictedInit = true;
        for(int i = 0; i < kEvictedRemember; i++)
        {
            gEvictedCx[i] = 0x7fffffff;
            gEvictedCz[i] = 0x7fffffff;
            gEvictedAt[i] = 0;
        }
    }
    gEvictedCx[gEvictedNext] = cx;
    gEvictedCz[gEvictedNext] = cz;
    gEvictedAt[gEvictedNext] = sceKernelGetSystemTimeLow();
    gEvictedNext = (gEvictedNext + 1) % kEvictedRemember;
}

// Five seconds. Long enough that a per-frame read cannot outlive it, short enough that the column is
// still reloadable if the player genuinely walks back towards it -- although in that case it re-enters
// the stream ring first, and the ring test below lets the streamer through.
static bool ColumnRecentlyEvicted(int cx, int cz)
{
    if(!gEvictedInit)
        return false;
    unsigned int now = sceKernelGetSystemTimeLow();
    for(int i = 0; i < kEvictedRemember; i++)
    {
        if(gEvictedCx[i] == cx && gEvictedCz[i] == cz)
            return (now - gEvictedAt[i]) < 5000000u;
    }
    return false;
}

// The one column the streamer is building. Generation used to run to completion inside the streaming
// tick, which is the 40 ms hitch, and the mesh hold that followed it stopped the next column until
// every visible section had faces -- so the ring never got more than a chunk ahead of a sprint.
// One column, resumed a few milliseconds at a time, and a short row kept in the direction of travel.
enum
{
    kStreamJobNone = 0,
    kStreamJobGen,
    kStreamJobLight,
    kStreamJobEdges
};
struct StreamColumnJob
{
    int phase;
    int cx;
    int cz;
    int step;
    int lightZ;
    int lightPass; // 0 = light the surface, then go back for caves. 1 = light after caves, then finish.
    ChunkColumn* col;
};
static StreamColumnJob sStreamJob;
static const int kStreamPrefetchMax = 3;
static int sPinCx[kStreamPrefetchMax];
static int sPinCz[kStreamPrefetchMax];
static int sPinCount = 0;
static float sMoveAccX = 0.0f;
static float sMoveAccZ = 0.0f;
static float sMoveLastX = 0.0f;
static float sMoveLastZ = 0.0f;
static bool sMoveHaveLast = false;

static bool StreamJobBusy()
{
    return sStreamJob.phase != kStreamJobNone;
}

static bool StreamColumnPinned(int cx, int cz)
{
    if(sStreamJob.phase != kStreamJobNone && sStreamJob.cx == cx && sStreamJob.cz == cz)
        return true;
    for(int i = 0; i < sPinCount; i++)
    {
        if(sPinCx[i] == cx && sPinCz[i] == cz)
            return true;
    }
    return false;
}

static void StreamCancelJob()
{
    sStreamJob.phase = kStreamJobNone;
    sStreamJob.col = 0;
    sPinCount = 0;
    sMoveHaveLast = false;
    sMoveAccX = 0.0f;
    sMoveAccZ = 0.0f;
}

// 0 = already had blocks, 1 = read off the card this call, 2 = a sliced generate is now in progress.
int StreamBeginColumn(CraftWorld* w, int cx, int cz)
{
    ChunkColumn* col = w->FindColumn(cx, cz);
    if(col && col->generated)
        return 0;
    if(!col)
    {
        MC_PERF_PHASE_BEGIN(kPhLoad);
        const bool loaded = w->LoadColumnFile(cx, cz);
        MC_PERF_PHASE_END(kPhLoad);
        if(loaded)
        {
            w->meshPending = true;
            w->streamOpsThisFrame++;
            return 1;
        }
        col = new ChunkColumn(cx, cz, w->CHUNK_SIZE, w->WORLD_HEIGHT);
        w->HashInsertColumn(col);
        w->meshPending = true;
    }
    sStreamJob.phase = kStreamJobGen;
    sStreamJob.cx = cx;
    sStreamJob.cz = cz;
    sStreamJob.step = 0;
    sStreamJob.lightZ = 0;
    sStreamJob.lightPass = 0;
    sStreamJob.col = col;
    w->pauseAfterTerrain = true;
    w->streamOpsThisFrame++;
    return 2;
}

void StreamAdvanceJob(CraftWorld* w, unsigned int budgetUs)
{
    if(sStreamJob.phase == kStreamJobNone || !w->worldGen)
        return;
    ChunkColumn* col = w->FindColumn(sStreamJob.cx, sStreamJob.cz);
    if(!col || col != sStreamJob.col)
    {
        sStreamJob.phase = kStreamJobNone;
        sStreamJob.col = 0;
        return;
    }
    w->streamOpsThisFrame++;
    unsigned int startUs = sceKernelGetSystemTimeLow();
    if(sStreamJob.phase == kStreamJobGen)
    {
        bool prev = w->worldGenerating;
        w->worldGenerating = true;
        MC_PERF_PHASE_BEGIN(kPhGen);
        bool done = w->worldGen->GenerateColumnSlice(w, sStreamJob.cx, sStreamJob.cz, sStreamJob.step, startUs, budgetUs);
        MC_PERF_PHASE_END(kPhGen);
        w->worldGenerating = prev;
        if(!done)
        {
            // The generator clears pauseAfterTerrain when it hands back exactly at the end of the
            // terrain rows. That is the surface: water included, caves not started. Light it and let
            // the mesher draw it while the caves run on later slices.
            if(sStreamJob.step == w->CHUNK_SIZE && !col->surfaceReady && !w->pauseAfterTerrain)
            {
                sStreamJob.phase = kStreamJobLight;
                sStreamJob.lightZ = 0;
                sStreamJob.lightPass = 0;
            }
            return;
        }
        sStreamJob.phase = kStreamJobLight;
        sStreamJob.lightZ = 0;
        sStreamJob.lightPass = 1;
    }
    if(sStreamJob.phase == kStreamJobLight)
    {
        MC_PERF_PHASE_BEGIN(kPhLight);
        int cs = col->chunkSize;
        while(sStreamJob.lightZ < cs)
        {
            int z1 = sStreamJob.lightZ + 4;
            if(z1 > cs)
                z1 = cs;
            w->InitColumnLighting(col, sStreamJob.lightZ, z1);
            sStreamJob.lightZ = z1;
            unsigned int now = sceKernelGetSystemTimeLow();
            if(budgetUs != 0 && (unsigned int)((int)now - (int)startUs) >= budgetUs)
            {
                MC_PERF_PHASE_END(kPhLight);
                return;
            }
        }
        MC_PERF_PHASE_END(kPhLight);
        if(sStreamJob.lightPass == 0)
        {
            col->surfaceReady = true;
            w->meshPending = true;
            sStreamJob.phase = kStreamJobGen;
            sStreamJob.lightPass = 1;
            return;
        }
        sStreamJob.phase = kStreamJobEdges;
        unsigned int now = sceKernelGetSystemTimeLow();
        if(budgetUs != 0 && (unsigned int)((int)now - (int)startUs) >= budgetUs)
            return;
    }
    if(sStreamJob.phase == kStreamJobEdges)
    {
        MC_PERF_PHASE_BEGIN(kPhLight);
        w->RemoveFloatingSnow(col);
        w->RelightSharedEdges(sStreamJob.cx, sStreamJob.cz);
        w->DirtyNeighborMeshes(sStreamJob.cx, sStreamJob.cz);
        // Trees and the shore arrived after the surface mesh, and a cave can open in the bottom of
        // that same section. Rebuilding it is what puts both in. Skipping it left the cave solid.
        // Deeper sections have no mesh yet and are built once, from the finished column.
        for(int cy = 0; cy < 16; cy++)
        {
            int id = col->meshId[cy];
            if(!w->MeshIdValid(id))
                continue;
            if(w->mChunks[id])
                w->mChunks[id]->needUpdate = true;
            if(id < (int)w->mTransparentChunks.size() && w->mTransparentChunks[id])
                w->mTransparentChunks[id]->needUpdate = true;
        }
        MC_PERF_PHASE_END(kPhLight);
        col->surfaceReady = true;
        col->generated = true;
        MC_PERF_INC(kColumnsDone);
        w->meshPending = true;
        if(col->blocks[col->Index(0, 0, 0)] == 0)
            Aurora::Utils::Logger::Instance()->LogMessage("WARNING empty column after generate c.%d.%d (grid=%d light=%d)\n", sStreamJob.cx, sStreamJob.cz, (int)w->meshGridActive, (int)w->meshLightActive);
        sStreamJob.phase = kStreamJobNone;
        sStreamJob.col = 0;
    }
}

void StreamUpdatePins(CraftWorld* w, int pcx, int pcz, int radius)
{
    sPinCount = 0;
    // Three columns is ~250 KB of block data. Below this the row is not worth the bytes; the
    // pressure path is already giving columns back, and a lookahead it immediately deletes is
    // just another generate hitch.
    if(w->HeapFreeKB() < 1600)
        return;
    if(!sMoveHaveLast)
    {
        sMoveHaveLast = true;
        sMoveLastX = w->playerPos.x;
        sMoveLastZ = w->playerPos.z;
        return;
    }
    sMoveAccX += w->playerPos.x - sMoveLastX;
    sMoveAccZ += w->playerPos.z - sMoveLastZ;
    sMoveLastX = w->playerPos.x;
    sMoveLastZ = w->playerPos.z;
    while(sMoveAccX * sMoveAccX + sMoveAccZ * sMoveAccZ > 4.0f)
    {
        sMoveAccX *= 0.5f;
        sMoveAccZ *= 0.5f;
    }
    int sx = (sMoveAccX > 0.4f) ? 1 : (sMoveAccX < -0.4f ? -1 : 0);
    int sz = (sMoveAccZ > 0.4f) ? 1 : (sMoveAccZ < -0.4f ? -1 : 0);
    if(sx == 0 && sz == 0)
        return;
    int dist = radius + 1;
    int ax = pcx + sx * dist;
    int az = pcz + sz * dist;
    int sideX = -sz;
    int sideZ = sx;
    int cxs[kStreamPrefetchMax] = { ax, ax + sideX, ax - sideX };
    int czs[kStreamPrefetchMax] = { az, az + sideZ, az - sideZ };
    for(int i = 0; i < kStreamPrefetchMax; i++)
    {
        int x0 = cxs[i] * w->CHUNK_SIZE;
        int z0 = czs[i] * w->CHUNK_SIZE;
        if(x0 < 0 || z0 < 0 || x0 >= w->worldLimit || z0 >= w->worldLimit)
            continue;
        sPinCx[sPinCount] = cxs[i];
        sPinCz[sPinCount] = czs[i];
        sPinCount++;
    }
}

bool StreamPickPin(CraftWorld* w, int pcx, int pcz, int& outCx, int& outCz)
{
    int best = 0x7fffffff;
    bool found = false;
    for(int i = 0; i < sPinCount; i++)
    {
        ChunkColumn* col = w->FindColumn(sPinCx[i], sPinCz[i]);
        if(col && col->generated)
            continue;
        int adx = sPinCx[i] - pcx;
        int adz = sPinCz[i] - pcz;
        if(adx < 0) adx = -adx;
        if(adz < 0) adz = -adz;
        int score = adx + adz;
        if(score < best)
        {
            best = score;
            outCx = sPinCx[i];
            outCz = sPinCz[i];
            found = true;
        }
    }
    return found;
}

ChunkColumn* CraftWorld::EnsureColumn(int cx, int cz, bool generate)
{
    ChunkColumn* col = FindColumn(cx, cz);
    int x0 = cx * CHUNK_SIZE;
    int z0 = cz * CHUNK_SIZE;
    if(x0 < 0 || z0 < 0 || x0 >= worldLimit || z0 >= worldLimit)
        return 0;
    if(spawnGenClip && !InGenBounds(x0, z0))
        return 0;

    // Do not rebuild a column that was just given back, if it is outside the ring the streamer can
    // ask for. See the note above RememberEvictedColumn: without this, the read that triggered the
    // eviction regenerates the column on the same tick and the eviction achieves nothing but a card
    // write, a terrain generation, a light scan and a remesh.
    //
    // The ring here must be the ring the streamer actually fills, which is why it asks StreamRadius()
    // rather than recomputing view+2. It was view+2 because that was the widest the stream ring could
    // be, but low memory now narrows the ring below that: with a fixed view+2 a column the pressure
    // path had just given back at distance 3 (outside a radius-2 ring) would still be inside this
    // test, so the next per-frame block read would regenerate it and the eviction would achieve
    // nothing but a card write, a terrain generation, a light scan and a remesh -- the loop the note
    // above RememberEvictedColumn exists to stop. The ring test stays so that a genuine revisit,
    // which re-enters the streaming ring first, is never blocked.
    if(generate && !col && ColumnRecentlyEvicted(cx, cz))
    {
        int ring = StreamRadius();
        int pcx = ChunkFloorDiv((int)playerPos.x, CHUNK_SIZE);
        int pcz = ChunkFloorDiv((int)playerPos.z, CHUNK_SIZE);
        int adx = cx - pcx;
        int adz = cz - pcz;
        if(adx < 0) adx = -adx;
        if(adz < 0) adz = -adz;
        if(adx > ring || adz > ring)
            return 0;
    }

    if(!col)
    {
        // A sliced column already owns the generator. Starting another one here would overwrite its
        // height cache mid-row. The read that missed gets air until the streamer is free.
        if(StreamJobBusy())
            return 0;
        if(!spawnGenClip && LoadColumnFile(cx, cz))
        {
            col = FindColumn(cx, cz);
            if(col)
            {
                meshPending = true;
                return col;
            }
        }

        col = new ChunkColumn(cx, cz, CHUNK_SIZE, WORLD_HEIGHT);
        HashInsertColumn(col);
        meshPending = true;
    }

    if(generate && !worldGenerating && worldGen && !col->generated)
    {
        if(StreamJobBusy())
            return col;
        bool prev = worldGenerating;
        worldGenerating = true;
        // The terrain generator for one column. This is the first suspect for the hitch that arrives
        // with new ground: it is the single most expensive thing a streaming tick can do. Split by who
        // asked, because the two cases need different fixes: `gen` is the streamer's own ring step (one
        // per ~0.12 s, expected), `demandGen` is a block READ dragging in a column the player is nowhere
        // near (a mob path, a light flood crossing a boundary), which the reading frame pays in full.
        const int genPhase = perfInStreaming ? kPhGen : kPhDemandGen;
        const int lightPhase = perfInStreaming ? kPhLight : kPhDemandLight;
        MC_PERF_PHASE_BEGIN(genPhase);
        worldGen->GenerateColumn(this, cx, cz);
        MC_PERF_PHASE_END(genPhase);
        worldGenerating = prev;
        col->generated = true;
        MC_PERF_INC(kColumnsDone);
        col->surfaceReady = true;
        RemoveFloatingSnow(col);
        meshPending = true;
        if(col->blocks[col->Index(0, 0, 0)] == 0)
            Aurora::Utils::Logger::Instance()->LogMessage("WARNING empty column after generate c.%d.%d (grid=%d light=%d)\n", cx, cz, (int)meshGridActive, (int)meshLightActive);
        // The column's first lighting, the shared-edge relight that follows it, and the neighbour
        // dirtying. Grouped because they are one indivisible follow-up to a new column.
        MC_PERF_PHASE_BEGIN(lightPhase);
        InitColumnLighting(col);
        RelightSharedEdges(cx, cz);
        DirtyNeighborMeshes(cx, cz);
        MC_PERF_PHASE_END(lightPhase);
    }

    return col;
}

void CraftWorld::InitColumnLighting(ChunkColumn* col, int zBegin, int zEnd)
{
    if(!col)
        return;
    const int cs = col->chunkSize;
    if(zBegin < 0)
        zBegin = 0;
    if(zEnd < 0 || zEnd > cs)
        zEnd = cs;
    const int height = col->height;
    const int yStride = cs;
    const int zStride = cs * height;
    const int typeCount = (int)blockTypes.size();
    const block_t water = WaterBlock::getID();
    const block_t glass = GlassBlock::getID();
    const block_t leaf = LeavesBlock::getID();
    const block_t birch = BirchLeaves::getID();
    const block_t spruce = SpruceLeaves::getID();
    block_t* blocks = col->blocks;
    block_settings_t* settings = col->settings;

    for(int lz = zBegin; lz < zEnd; lz++)
    {
        block_t* bcol = blocks + lz * zStride;
        block_settings_t* scol = settings + lz * zStride;
        for(int lx = 0; lx < cs; lx++)
        {
            int level = 0;
            for(int y = height - 1; y >= 0; --y)
            {
                const int i = lx + y * yStride;
                scol[i] = (block_settings_t)((scol[i] & OpRedstonePowerMask) | ((level & 15) << 4));
                if(level >= 15)
                {
                    // The rest of this column is in full shadow. The old loop still visited every
                    // cell and looked the block up; the value it wrote does not depend on the block.
                    for(int yy = y - 1; yy >= 0; --yy)
                    {
                        const int j = lx + yy * yStride;
                        scol[j] = (block_settings_t)((scol[j] & OpRedstonePowerMask) | (15 << 4));
                    }
                    break;
                }
                const block_t block = bcol[i];
                if(block == 0 || block >= typeCount)
                    continue;
                const int refraction = blockTypes[block].refraction;
                if((refraction != 0 && refraction != 2) || block == water)
                    level++;
                if(block == glass && level == 0)
                    level++;
                if((block == leaf || block == birch || block == spruce) && level == 0)
                    level += 2;
                if(block == water && level < 15)
                    level++;
                if(level > 15)
                    level = 15;
            }
        }
    }
}

ChunkColumn* CraftWorld::BlockColumn(int x, int z, bool create)
{
    if(CHUNK_SIZE <= 0)
        return 0;
    int cx = ChunkFloorDiv(x, CHUNK_SIZE);
    int cz = ChunkFloorDiv(z, CHUNK_SIZE);
    ChunkColumn* col = FindColumn(cx, cz);
    if(col || !create)
        return col;
    // Creation on demand is only ever granted inside the visible ring. Every current caller passes
    // create=false (ColumnForBlock), so this is belt-and-braces for the same reason as the GetBlock gate.
    if(!ColumnWithinStreamRing(cx, cz))
        return 0;
    return EnsureColumn(cx, cz, !worldGenerating);
}

ChunkColumn* CraftWorld::ColumnForBlock(int x, int z)
{
    if(lastCol)
    {
        int cs = lastCol->chunkSize;
        int dx = x - lastCol->cx * cs;
        int dz = z - lastCol->cz * cs;
        if(dx >= 0 && dx < cs && dz >= 0 && dz < cs)
            return lastCol;
    }
    if(meshGridActive && CHUNK_SIZE > 0)
    {
        int cx = ChunkFloorDiv(x, CHUNK_SIZE);
        int cz = ChunkFloorDiv(z, CHUNK_SIZE);
        int gx = cx - meshGridCx + 1;
        int gz = cz - meshGridCz + 1;
        if((unsigned)gx <= 2 && (unsigned)gz <= 2)
        {
            ChunkColumn* col = meshColGrid[gx][gz];
            if(col)
            {
                lastCol = col;
                lastCx = col->cx;
                lastCz = col->cz;
                return col;
            }
            // Grid slot was empty when the mesh started; the column may have been
            // created since (e.g. by a nested generate), so fall through to the hash.
        }
    }
    return BlockColumn(x, z, false);
}

void CraftWorld::BeginColumnGrid(int StartX, int StartZ)
{
    meshGridActive = false;
    if(CHUNK_SIZE <= 0)
        return;
    meshGridCx = ChunkFloorDiv(StartX, CHUNK_SIZE);
    meshGridCz = ChunkFloorDiv(StartZ, CHUNK_SIZE);
    for(int gz = 0; gz < 3; gz++)
    {
        for(int gx = 0; gx < 3; gx++)
            meshColGrid[gx][gz] = FindColumn(meshGridCx + gx - 1, meshGridCz + gz - 1);
    }
    meshGridActive = true;
}

void CraftWorld::BeginMeshCachesHead(int StartX, int StartY, int StartZ)
{
    meshLightActive = false;
    BeginColumnGrid(StartX, StartZ);

    meshLightOriginX = StartX - 1;
    meshLightOriginY = StartY - 1;
    meshLightOriginZ = StartZ - 1;
    meshLightDimX = CHUNK_SIZE + 2;
    meshLightDimY = CHUNK_SIZE + 2;
    meshLightDimZ = CHUNK_SIZE + 2;
    if(meshLightDimX > 18) meshLightDimX = 18;
    if(meshLightDimY > 18) meshLightDimY = 18;
    if(meshLightDimZ > 18) meshLightDimZ = 18;
    meshJobFillCursor = 0;
}

int CraftWorld::MeshLightTotal() const
{
    return meshLightDimX * meshLightDimY * meshLightDimZ;
}

// How brightly the nether's unlit air is drawn. See BlockFinalLight for why the dimension needs a
// floor of its own; this is the one number to turn if the nether reads too dark or too bright.
static const float kNetherAmbientLight = 0.4f;

// BlockFinalLightNoCheck's arithmetic once the two levels are known, kept expression for expression
// (including the double in the shadow-only branch) so the table below gives bit-identical values.
static float MeshFinalLight(int shadowLevel, int lightLevel, float lightColorX, bool nether)
{
    float Light = 0.0f;
    bool inShadow = shadowLevel > 0;
    if(lightLevel > 0)
    {
        float lightened = (lightLevel+1) / 15.0f;
        float normal = inShadow ? lightColorX*(1.0f-((float)shadowLevel/15.0f)) : lightColorX;
        Light = lightened > normal ? lightened : normal;
    }
    else
    {
        inShadow == true ? Light = lightColorX*(1.0-(float)(shadowLevel/15.0f)) : Light = lightColorX;
    }
    if(nether && Light < kNetherAmbientLight)
        Light = kNetherAmbientLight;
    if(Light < 0.1)
        return 0.1;
    return Light;
}

// The final light of a cell as a function of its settings byte: entry s for an ordinary cell, entry
// 256 + s for a cell whose low nibble is not light (a state cell, or a cell outside any column). The
// four shadow bits are stored high-bit-lowest (16 -> 8, 32 -> 4, 64 -> 2, 128 -> 1). Rebuilt only when
// the sky colour or the dimension changes, which is what makes the per-cell cost one load.
static float sMeshLightTable[512];
static float sMeshLightTableColor = -1.0f;
static int sMeshLightTableDim = -1;

static const float* MeshLightTable(float lightColorX, int dimension)
{
    if(lightColorX != sMeshLightTableColor || dimension != sMeshLightTableDim)
    {
        for(int s = 0; s < 256; s++)
        {
            int shadow = ((s & 16) ? 8 : 0) + ((s & 32) ? 4 : 0) + ((s & 64) ? 2 : 0) + ((s & 128) ? 1 : 0);
            sMeshLightTable[s] = MeshFinalLight(shadow, s & 15, lightColorX, dimension == 1);
            sMeshLightTable[256 + s] = MeshFinalLight(shadow, 0, lightColorX, dimension == 1);
        }
        sMeshLightTableColor = lightColorX;
        sMeshLightTableDim = dimension;
    }
    return sMeshLightTable;
}

void CraftWorld::FillMeshCachesSlice(unsigned int startUs, unsigned int budgetUs)
{
    const int total = MeshLightTotal();
    const int dimX = meshLightDimX;
    const int dimY = meshLightDimY;
    const int typeCount = (int)blockTypes.size();
    const float* lightTable = MeshLightTable(lightColor.x, dimension);
    const block_t lavaA = Lava::getID();
    const block_t lavaB = LavaBlock1::getID();
    int i = meshJobFillCursor;
    int rows = 0;
    // One row of x at a time, so the column is looked up when x crosses into the next one (three times
    // per row) instead of four times per cell, and the cell's block and settings are read straight from
    // the column arrays. Each value is what BlockFinalLightNoCheck and _GetBlockNoCheck return for it.
    while(i < total)
    {
        const int row = i / dimX;
        int lx = i - row * dimX;
        const int lz = row / dimY;
        const int ly = row - lz * dimY;
        const int wy = meshLightOriginY + ly;
        const int wz = meshLightOriginZ + lz;
        const bool rowInWorld = wy >= 0 && wy < WORLD_HEIGHT && wz >= 0 && wz < worldLimit;
        ChunkColumn* col = 0;
        int colX0 = 1;
        int colX1 = 0;
        for(; lx < dimX; ++lx, ++i)
        {
            const int wx = meshLightOriginX + lx;
            if(wx < colX0 || wx > colX1)
            {
                col = ColumnForBlock(wx, wz);
                if(col)
                {
                    colX0 = col->cx * col->chunkSize;
                    colX1 = colX0 + col->chunkSize - 1;
                }
                else
                {
                    colX0 = wx + 1;
                    colX1 = wx;
                }
            }
            block_t b = 0;
            if(col && wy >= 0 && wy < col->height)
            {
                const int cs = col->chunkSize;
                const int idx = (wx - colX0) + wy * cs + (wz - col->cz * cs) * cs * col->height;
                b = col->blocks[idx];
                const block_settings_t s = col->settings[idx];
                if(rowInWorld && wx >= 0 && wx < worldLimit && (b == lavaA || b == lavaB))
                    meshLight[i] = 1.0f;
                else if((s & 15) != 0 && IsStateCellBlock(b))
                    meshLight[i] = lightTable[256 + s];
                else
                    meshLight[i] = lightTable[s];
            }
            else
            {
                meshLight[i] = lightTable[256 + dummySettings];
            }
            meshBlocks[i] = b;
            unsigned char f = 0;
            if(b < typeCount)
            {
                const BaseBlock& t = blockTypes[b];
                if(t.transparent) f |= MeshFlagTransparent;
                if(t.lightSource) f |= MeshFlagLightSource;
                if(t.refraction == 2) f |= MeshFlagRefraction2;
                if(b != 0 && !t.transparent && t.blockModel == 0) f |= MeshFlagWall;
            }
            meshFlags[i] = f;
        }
        meshJobFillCursor = i;
        // Checked every eight rows (144 cells): the slice must not overshoot -- the whole point of
        // slicing it is the ~20 ms it used to cost in a single frame. budgetUs == 0 means "no limit",
        // for the synchronous callers.
        if(budgetUs != 0 && (++rows & 7) == 0 && (sceKernelGetSystemTimeLow() - startUs) >= budgetUs)
            break;
    }
}

void CraftWorld::BeginMeshCaches(int StartX, int StartY, int StartZ)
{
    BeginMeshCachesHead(StartX, StartY, StartZ);
    // Synchronous callers (a direct rebuild, the physics-box path) want the whole cache now; only the
    // frame job walks it in slices.
    FillMeshCachesSlice(0u, 0u);
    meshLightActive = true;
}

void CraftWorld::EndMeshCaches()
{
    meshLightActive = false;
    meshGridActive = false;
}

void CraftWorld::CreateMeshesForColumn(ChunkColumn* col, int forceCy)
{
    // An allocated column is all air until generation finishes. SectionHasExposedFaces then reports
    // "nothing to draw" and the caller stores meshId -2, which is permanent: the scan never revisits
    // -2. The blocks arrive a few frames later and the chunk stays invisible. That is the ground that
    // cannot catch up while you move.
    if(!col || !col->generated)
        return;
    int chunksY = (WORLD_HEIGHT + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if(chunksY > 16)
        chunksY = 16;
    int startX = col->cx * CHUNK_SIZE;
    int startZ = col->cz * CHUNK_SIZE;
    for(int cy = 0; cy < chunksY; cy++)
    {
        if(MeshIdValid(col->meshId[cy]))
            continue;
        int startY = cy * CHUNK_SIZE;
        bool force = (forceCy == cy);
        if(!force && col->meshId[cy] == -2)
            continue;
        if(!force && !SectionInPlayerZone(startX, startY, startZ))
            continue;
        // Off-camera sections are left as raw blocks until the camera looks at them -- see SectionInView.
        // `force` is a targeted rebuild of one named section and is never gated.
        if(!force && !SectionInView(startX, startY, startZ))
            continue;
        if(!force && !SectionHasExposedFaces(startX, startY, startZ))
        {
            col->meshId[cy] = -2;
            continue;
        }
        col->meshId[cy] = CreateFullMeshChunk(startX, startY, startZ);
    }
    DirtyNeighborMeshes(col->cx, col->cz);
}

void CraftWorld::DestroyMeshesForColumn(ChunkColumn* col)
{
    if(!col)
        return;
    for(int i = 0; i < 16; i++)
    {
        int id = col->meshId[i];
        if(MeshIdValid(id))
        {
            for(unsigned int u = 0; u < updatingChunks.size(); )
            {
                if(updatingChunks[u] == id)
                {
                    updatingChunks.erase(updatingChunks.begin() + u);
                    if(updatingChunksPosition > u)
                        updatingChunksPosition--;
                    else if(updatingChunksPosition >= updatingChunks.size() && !updatingChunks.empty())
                        updatingChunksPosition = 0;
                }
                else
                    u++;
            }
            for(unsigned int u = 0; u < toUpdate.size(); )
            {
                if(toUpdate[u] == id)
                    toUpdate.erase(toUpdate.begin() + u);
                else
                    u++;
            }
            for(unsigned int u = 0; u < toFastUpdate.size(); )
            {
                if(toFastUpdate[u] == id)
                    toFastUpdate.erase(toFastUpdate.begin() + u);
                else
                    u++;
            }
            // The redstone queue is a pair of parallel arrays, so the two have to be erased together or
            // the opaque flag of one entry becomes the flag of the next.
            for(unsigned int u = 0; u < redstoneMeshQueue.size(); )
            {
                if(redstoneMeshQueue[u] == id)
                {
                    redstoneMeshQueue.erase(redstoneMeshQueue.begin() + u);
                    if(u < redstoneMeshOpaque.size())
                        redstoneMeshOpaque.erase(redstoneMeshOpaque.begin() + u);
                }
                else
                    u++;
            }
            mChunks[id]->reset();
            delete mChunks[id];
            mChunks[id] = 0;
            delete mTransparentChunks[id];
            mTransparentChunks[id] = 0;
        }
        col->meshId[i] = -1;
    }
}

void CraftWorld::AddNearestMeshId(int id)
{
    if(!MeshIdValid(id))
        return;
    if(!MeshInPlayerZone(mChunks[id]))
        return;
    for(unsigned int i = 0; i < mNearestChunksIds.size(); i++)
    {
        if(mNearestChunksIds[i] == id)
            return;
    }
    mNearestChunksIds.push_back(id);
}

int CraftWorld::ColumnTopY(ChunkColumn* col) const
{
    if(!col)
        return 0;
    int cs = col->chunkSize;
    for(int y = col->height - 1; y >= 0; y--)
    {
        const block_t* row = col->blocks + col->Index(0, y, 0);
        int stride = col->Index(0, y, 1) - col->Index(0, y, 0);
        for(int lz = 0; lz < cs; lz++)
        {
            const block_t* p = row + lz * stride;
            for(int lx = 0; lx < cs; lx++)
                if(p[lx] != 0)
                    return y;
        }
    }
    return 0;
}

void CraftWorld::RemoveFloatingSnow(ChunkColumn* col)
{
    if(!col)
        return;
    int cs = col->chunkSize;
    block_t snow = Snow2::getID();
    for(int lz = 0; lz < cs; lz++)
    {
        for(int lx = 0; lx < cs; lx++)
        {
            for(int y = 1; y < col->height; y++)
            {
                int idx = col->Index(lx, y, lz);
                if(col->blocks[idx] != snow)
                    continue;
                block_t under = col->blocks[col->Index(lx, y - 1, lz)];
                if(under == 0 || under == snow || (under < blockTypes.size() && blockTypes[under].blockModel != 0))
                    col->blocks[idx] = 0;
            }
        }
    }
}

// Called when a column gets new block data. Only the four face neighbours share faces
// with it, and only their sections at or below this column's highest block can have
// drawn a wall against what used to be "air". Diagonal neighbours and sky sections are
// left alone: each remesh costs a whole frame on the PSP.
void CraftWorld::DirtyNeighborMeshes(int cx, int cz)
{
    ChunkColumn* self = FindColumn(cx, cz);
    int maxCy = 15;
    if(self && CHUNK_SIZE > 0)
    {
        maxCy = ColumnTopY(self) / CHUNK_SIZE + 1;
        if(maxCy > 15) maxCy = 15;
    }
    static const int ox[4] = { -1, 1, 0, 0 };
    static const int oz[4] = { 0, 0, -1, 1 };
    for(int n = 0; n < 4; n++)
    {
        ChunkColumn* col = FindColumn(cx + ox[n], cz + oz[n]);
        if(!col)
            continue;
        for(int cy = 0; cy <= maxCy; cy++)
        {
            int id = col->meshId[cy];
            if(!MeshIdValid(id))
                continue;
            mChunks[id]->needUpdate = true;
            if(id < (int)mTransparentChunks.size() && mTransparentChunks[id])
                mTransparentChunks[id]->needUpdate = true;
        }
    }
}

void CraftWorld::RelightSharedEdges(int cx, int cz)
{
    if(CHUNK_SIZE <= 0)
        return;
    ChunkColumn* west = FindColumn(cx - 1, cz);
    if(west)
    {
        int x = west->cx * CHUNK_SIZE + CHUNK_SIZE - 1;
        int z0 = west->cz * CHUNK_SIZE;
        for(int lz = 0; lz < CHUNK_SIZE; lz++)
            initPutBlocksLight(x, z0 + lz);
    }
    ChunkColumn* east = FindColumn(cx + 1, cz);
    if(east)
    {
        int x = east->cx * CHUNK_SIZE;
        int z0 = east->cz * CHUNK_SIZE;
        for(int lz = 0; lz < CHUNK_SIZE; lz++)
            initPutBlocksLight(x, z0 + lz);
    }
    ChunkColumn* north = FindColumn(cx, cz - 1);
    if(north)
    {
        int z = north->cz * CHUNK_SIZE + CHUNK_SIZE - 1;
        int x0 = north->cx * CHUNK_SIZE;
        for(int lx = 0; lx < CHUNK_SIZE; lx++)
            initPutBlocksLight(x0 + lx, z);
    }
    ChunkColumn* south = FindColumn(cx, cz + 1);
    if(south)
    {
        int z = south->cz * CHUNK_SIZE;
        int x0 = south->cx * CHUNK_SIZE;
        for(int lx = 0; lx < CHUNK_SIZE; lx++)
            initPutBlocksLight(x0 + lx, z);
    }
}

static bool meshRebuildRetryDue();

void CraftWorld::TryMeshOneSection()
{
    if(!meshPending || meshBuildsThisFrame > 0 || loadedColumns.empty() || CHUNK_SIZE <= 0)
        return;
    int pcy = (int)playerPos.y / CHUNK_SIZE;
    int chunksY = (WORLD_HEIGHT + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if(chunksY > 16)
        chunksY = 16;
    for(int skipBuried = 0; skipBuried < 8; skipBuried++)
    {
        ChunkColumn* bestCol = 0;
        int bestCy = -1;
        int bestScore = 0x7fffffff;
        // A hole the player can see always beats a brand-new section further out. Without this
        // split, look-direction plus the old "valid meshId waits" penalty made the picker start
        // the next horizon chunk while the hole they just crossed sat at tris=0/0.
        ChunkColumn* bestHoleCol = 0;
        int bestHoleCy = -1;
        int bestHoleScore = 0x7fffffff;
        for(unsigned int i = 0; i < loadedColumns.size(); i++)
        {
            ChunkColumn* col = loadedColumns[i];
            // An unfinished column is still the allocator's air until its terrain rows land. Scanning
            // that stores meshId -2 and the ground never appears. `surfaceReady` is the point the
            // ground and the water exist, which is before the caves.
            if(!col || (!col->generated && !col->surfaceReady))
                continue;
            EnsureSurfaceBand(col);
            int startX = col->cx * CHUNK_SIZE;
            int startZ = col->cz * CHUNK_SIZE;
            for(int cy = 0; cy < chunksY; cy++)
            {
                if(col->meshId[cy] == -3)
                {
                    if(!col->generated)
                        continue;
                    col->meshId[cy] = -1;
                }
                int startY = cy * CHUNK_SIZE;
                const int mid = col->meshId[cy];
                if(mid == -2)
                    continue;
                // Already the job in flight: do not pick it again.
                if(MeshIdValid(mid) && mid == meshJobId)
                    continue;
                bool emptyHole = !MeshIdValid(mid);
                bool hasFaces = false;
                bool emptyNeedsExposeCheck = false;
                bool oomHold = false;
                if(MeshIdValid(mid))
                {
                    SimpleMeshChunk* o = mid < (int)mChunks.size() ? mChunks[mid] : 0;
                    SimpleMeshChunk* t = mid < (int)mTransparentChunks.size() ? mTransparentChunks[mid] : 0;
                    const int tris = (o ? o->trienglesCount : 0) + (t ? t->trienglesCount : 0);
                    const bool dirty = (o && o->needUpdate) || (t && t->needUpdate);
                    const bool oom = (o && o->outOfMemory) || (t && t->outOfMemory);
                    hasFaces = tris > 0;
                    emptyHole = (tris == 0);
                    oomHold = oom;
                    if(hasFaces && !dirty && !oom)
                        continue;
                    // Placeholder meshes keep needUpdate set. A finished empty with no dirty flag
                    // is only a hole if the world still has faces -- checked after the view gate.
                    emptyNeedsExposeCheck = emptyHole && !dirty && !oom;
                }
                if(!col->generated)
                {
                    // Caves are still carving below this. Draw the ground -- every section the ground
                    // passes through, not only the summit's -- and, where the sea is higher than the
                    // floor, the water. Deeper sections wait until the column is finished.
                    int lo = col->surfaceMinY / CHUNK_SIZE;
                    int hi = col->surfaceY / CHUNK_SIZE;
                    int shore = waterLevel > col->surfaceMinY ? waterLevel / CHUNK_SIZE : lo;
                    if(shore > hi)
                        hi = shore;
                    if(cy < lo || cy > hi)
                        continue;
                }
                bool inZone = SectionInPlayerZone(startX, startY, startZ, col);
                // The three columns kept one chunk past the ring, in the direction of travel. Their
                // surface band is meshed before it enters the view, so the faces exist when the player
                // arrives. Overworld only: a nether column is mostly exposed faces, and meshing it
                // ahead of the camera is how that dimension runs out of memory. On-screen holes still
                // win; the penalty below is that rule.
                bool ahead = StreamColumnPinned(col->cx, col->cz) && (col->generated || col->surfaceReady) && dimension == 0;
                if(!inZone)
                {
                    if(!ahead)
                        continue;
                    int band = cy - pcy;
                    if(band < 0) band = -band;
                    if(band > 1)
                        continue;
                }
                else if(!SectionInView(startX, startY, startZ))
                    continue;
                if(emptyNeedsExposeCheck && !SectionHasExposedFaces(startX, startY, startZ))
                    continue;
                int score = SectionMeshPriority(startX, startY, startZ, col);
                if(!inZone)
                    score += 4096;
                // Rebuilds of geometry already on screen wait behind anything that has never been
                // drawn. The old +1000000 applied to every valid meshId, so a 0-triangle hole
                // lost to a brand-new meshId=-1 at the horizon.
                if(hasFaces)
                    score += 1000000;
                // Holes: empty meshes, or never-meshed ground in the ring the player is in or
                // touching. A new column at ring 2+ is not a hole -- it waits until these are gone.
                int adx = col->cx - ChunkFloorDiv((int)playerPos.x, CHUNK_SIZE);
                int adz = col->cz - ChunkFloorDiv((int)playerPos.z, CHUNK_SIZE);
                if(adx < 0) adx = -adx;
                if(adz < 0) adz = -adz;
                const int ring = adx > adz ? adx : adz;
                // A section two or more below the ground, and not next to the player, is a deep cave.
                // It was taking the only mesh slot (the 20:08 log: dozens of meshId=-1 under the
                // surface while the ground the player could see waited). The cliff directly under
                // the surface still builds. So does the section the player is in.
                if(col->surfaceKnown && !SectionInSurfaceBand(col, startY))
                {
                    int gap = col->surfaceMinY / CHUNK_SIZE - cy;
                    int vdy = cy - pcy;
                    if(vdy < 0) vdy = -vdy;
                    if(gap > 1 && vdy > 1)
                        continue;
                }
                const bool isHole = emptyHole && (MeshIdValid(mid) || ring <= 1);
                // A section that just failed to allocate is not rebuilt every frame. That loop
                // held one job and left the cave empty.
                if(oomHold && !meshRebuildRetryDue())
                    continue;
                if(isHole)
                {
                    if(score < bestHoleScore)
                    {
                        bestHoleScore = score;
                        bestHoleCol = col;
                        bestHoleCy = cy;
                    }
                }
                else if(score < bestScore)
                {
                    bestScore = score;
                    bestCol = col;
                    bestCy = cy;
                }
            }
        }
        if(bestHoleCol)
        {
            bestCol = bestHoleCol;
            bestCy = bestHoleCy;
        }
        if(!bestCol || bestCy < 0)
        {
            meshPending = false;
            return;
        }
        int startX = bestCol->cx * CHUNK_SIZE;
        int startY = bestCy * CHUNK_SIZE;
        int startZ = bestCol->cz * CHUNK_SIZE;
        if(MeshIdValid(bestCol->meshId[bestCy]))
        {
            RequestMeshRebuild(kMeshJobFull, bestCol->meshId[bestCy], true);
            return;
        }
        if(!SectionHasExposedFaces(startX, startY, startZ))
        {
            // Not finished yet: an empty answer is air the trees have not grown into, or stone the
            // caves have not opened. -2 would never be rebuilt. -3 means "ask again when the column
            // is done."
            if(!bestCol->generated)
            {
                bestCol->meshId[bestCy] = -3;
                continue;
            }
            bestCol->meshId[bestCy] = -2;
            continue;
        }
        // `false`: the walk is queued as a job rather than run here. This path used to build the section
        // inline, which is the same 4,096-cell walk as a rebuild -- a 30 ms section, every 0.08 s, right in
        // the middle of the streaming the perf lines are complaining about. It is thinned to one section at
        // a time by `meshIdleTimer` either way; the job just stops that one section from being a frame.
        bestCol->meshId[bestCy] = CreateFullMeshChunk(startX, startY, startZ, false);
        AddNearestMeshId(bestCol->meshId[bestCy]);
        // No neighbour dirtying here: neighbours were built from real block data (the
        // column already existed), so their faces against this section are correct.
        return;
    }
}

#if MC_FRAME_PROFILE
// The hole audit. A hole the player can see is a section in the view ring whose ground (or zone)
// should have faces and does not. It used to wait until they stood still, which is why the fly
// captures never named the holes that were already on screen mid-flight. Now it samples every 2 s
// while they move, and again after 2.5 s of standing -- that second pass is the "it does not fill"
// case. A stuck hole is re-queued (meshId -2 cleared, meshPending set) so standing still is no
// longer a reason for the mesher to ignore it. The first few findings also write a screenshot.
void CraftWorld::AuditHoles(float dt)
{
    const float moved = fabsf(playerPos.x - auditLastPos.x) + fabsf(playerPos.y - auditLastPos.y) + fabsf(playerPos.z - auditLastPos.z);
    auditLastPos = playerPos;
    if(moved > 0.05f)
        auditStillFor = 0.0f;
    else
        auditStillFor += dt;
    auditTimer += dt;
    const bool stuckPass = (auditStillFor >= 2.5f);
    // Moving: every 2 s. Standing: first pass at 2.5 s still, then every 3 s.
    const float period = stuckPass ? 3.0f : 2.0f;
    if(auditTimer < period)
        return;
    auditTimer = 0.0f;
    if(CHUNK_SIZE <= 0)
        return;

    int pcx = ChunkFloorDiv((int)playerPos.x, CHUNK_SIZE);
    int pcz = ChunkFloorDiv((int)playerPos.z, CHUNK_SIZE);
    int view = EffectiveViewDistance();
    if(view < 1)
        view = 1;
    int absent = 0;
    for(int dz = -view; dz <= view; dz++)
    {
        for(int dx = -view; dx <= view; dx++)
        {
            int cx = pcx + dx, cz = pcz + dz;
            if(cx < 0 || cz < 0)
                continue;
            if(FindColumn(cx, cz) != 0)
                continue;
            absent++;
            if(absent <= 4)
                Aurora::Utils::Logger::Instance()->LogMessage("hole: column %d,%d absent ring=%d streamJob=%d pinned=%d freeKB=%d loaded=%d\n",
                    cx, cz, (dx < 0 ? -dx : dx) > (dz < 0 ? -dz : dz) ? (dx < 0 ? -dx : dx) : (dz < 0 ? -dz : dz),
                    (int)sStreamJob.phase, (int)StreamColumnPinned(cx, cz), HeapFreeKB(), (int)loadedColumns.size());
        }
    }

    int chunksY = (WORLD_HEIGHT + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if(chunksY > 16)
        chunksY = 16;
    int holes = 0;
    int logged = 0;
    for(unsigned int i = 0; i < loadedColumns.size(); i++)
    {
        ChunkColumn* col = loadedColumns[i];
        if(!col)
            continue;
        int adx = col->cx - pcx; if(adx < 0) adx = -adx;
        int adz = col->cz - pcz; if(adz < 0) adz = -adz;
        if(adx > view || adz > view)
            continue;
        EnsureSurfaceBand(col);
        int startX = col->cx * CHUNK_SIZE;
        int startZ = col->cz * CHUNK_SIZE;
        for(int cy = 0; cy < chunksY; cy++)
        {
            int startY = cy * CHUNK_SIZE;
            // Ground in the ring, or anything already in the zone. Off-camera caves under solid
            // ground are not a hole; a hillside the player can see is, even if the frustum test
            // (one frame stale, and wrong while looking at the sky) says otherwise.
            const bool surface = SectionInSurfaceBand(col, startY);
            const bool inZone = SectionInPlayerZone(startX, startY, startZ, col);
            if(!surface && !inZone)
                continue;
            // Off-camera caves in the zone are not the hole the player is looking at. The first
            // probe run logged 11 of those at spawn (inView=0, y=16 under ground at 60).
            if(!SectionInView(startX, startY, startZ))
                continue;
            int id = col->meshId[cy];
            int opaqueTris = -1, transparentTris = -1, oom = 0, needUpd = 0;
            if(MeshIdValid(id))
            {
                SimpleMeshChunk* o = id < (int)mChunks.size() ? mChunks[id] : 0;
                SimpleMeshChunk* t = id < (int)mTransparentChunks.size() ? mTransparentChunks[id] : 0;
                opaqueTris = o ? o->trienglesCount : -1;
                transparentTris = t ? t->trienglesCount : -1;
                oom = (o && o->outOfMemory) || (t && t->outOfMemory);
                needUpd = (o && o->needUpdate) || (t && t->needUpdate);
                if((opaqueTris > 0 || transparentTris > 0) && !oom)
                    continue;
            }
            if(!SectionHasExposedFaces(startX, startY, startZ))
                continue;
            if(MeshIdValid(id))
            {
                if(id < (int)mChunks.size() && mChunks[id])
                    mChunks[id]->needUpdate = true;
                if(id < (int)mTransparentChunks.size() && mTransparentChunks[id])
                    mTransparentChunks[id]->needUpdate = true;
            }
            holes++;
            // A -2 here is the mesher having decided this had no faces, then being wrong once the
            // neighbours arrived. Standing still never woke it. Do that now.
            if(id == -2 || id == -3)
            {
                col->meshId[cy] = -1;
                id = -1;
            }
            meshPending = true;
            if(logged < 6)
            {
                logged++;
                Aurora::Utils::Logger::Instance()->LogMessage(
                    "hole: sec %d,%d,%d meshId=%d tris=%d/%d oom=%d needUpd=%d | col gen=%d surf=%d band=%d..%d known=%d | inZone=%d inView=%d pinned=%d ring=%d | meshPending=%d job=%d/%d builds=%d freeKB=%d %s\n",
                    startX, startY, startZ, id, opaqueTris, transparentTris, oom, needUpd,
                    (int)col->generated, (int)col->surfaceReady, col->surfaceMinY, col->surfaceY, (int)col->surfaceKnown,
                    (int)inZone, (int)SectionInView(startX, startY, startZ), (int)StreamColumnPinned(col->cx, col->cz), adx > adz ? adx : adz,
                    (int)meshPending, meshJobKind, meshJobId, meshBuildsThisFrame, HeapFreeKB(),
                    stuckPass ? "stuck" : "moving");
            }
        }
    }
    Aurora::Utils::Logger::Instance()->LogMessage("hole audit: %d hole section(s), %d absent column(s), player %d,%d,%d still %.1fs %s\n",
        holes, absent, (int)playerPos.x, (int)playerPos.y, (int)playerPos.z, auditStillFor,
        stuckPass ? "stuck" : "moving");
    // Screenshots are only when the player has stopped. Writing a PNG to the memory stick from the
    // update is what hung the console for ~2 s as holes came on screen (worstFrame 2166 ms).
    if(stuckPass && (holes > 0 || absent > 0) && auditShots < 4)
    {
        auditShots++;
        sceIoMkdir("ms0:/PICTURE", 0777);
        Aurora::Graphics::RenderManager::InstancePtr()->TakeNextScreenshot();
        Aurora::Utils::Logger::Instance()->LogMessage("hole shot %d at %d,%d,%d\n",
            auditShots, (int)playerPos.x, (int)playerPos.y, (int)playerPos.z);
    }
}
#endif

void CraftWorld::TickUnmeshedColumns(float dt)
{
    simTickTimer += dt;
    if(simTickTimer < 1.15f)
        return;
    simTickTimer = 0.0f;
    if(loadedColumns.empty())
        return;
    if(simColCursor < 0 || simColCursor >= (int)loadedColumns.size())
        simColCursor = 0;
    ChunkColumn* col = loadedColumns[simColCursor++];
    if(!col)
        return;
    int chunksY = (WORLD_HEIGHT + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if(chunksY > 16)
        chunksY = 16;
    int startX = col->cx * CHUNK_SIZE;
    int startZ = col->cz * CHUNK_SIZE;
    for(int cy = 0; cy < chunksY; cy++)
    {
        if(MeshIdValid(col->meshId[cy]) || col->meshId[cy] == -2)
            continue;
        int startY = cy * CHUNK_SIZE;
        if(physicsBoxesThisFrame == 0 && SectionNeedsPhysics(startX, startY, startZ))
            UpdateBlockPhysicsBox(startX, startY, startZ);
        if(SectionNeedsSimulation(startX, startY, startZ))
            TickSectionGrowth(startX, startY, startZ);
    }
}

void CraftWorld::TickSectionGrowth(int StartX, int StartY, int StartZ)
{
    int endX = StartX + CHUNK_SIZE;
    int endY = StartY + CHUNK_SIZE;
    int endZ = StartZ + CHUNK_SIZE;
    if(endY > WORLD_HEIGHT)
        endY = WORLD_HEIGHT;
    for(int z = StartZ; z < endZ; ++z)
    {
        for(int y = StartY; y < endY; ++y)
        {
            for(int x = StartX; x < endX; ++x)
            {
                block_t& block = GetBlock(x, y, z);
                if(block == 0)
                    continue;
                if(block == DirtBlock::getID() && GetShadowLevel(x, y, z) == 0)
                {
                    if(rand() % 6 == 0 && BlockTransparentOrSpecial(x, y + 1, z))
                        block = GrassBlock::getID();
                }
                else if(block == GrassBlock::getID() && (GetShadowLevel(x, y, z) > 6 || BlockTransparentOrSpecial(x, y + 1, z) == false))
                {
                    block = DirtBlock::getID();
                }
                else if(block == CaneBlock::getID() && GetBlock(x, y + 1, z) == 0 && GetBlock(x, y - 3, z) != 33 && GetBlock(x, y - 3, z) != 0)
                {
                    if(rand() % 100 > 92)
                        GetBlock(x, y + 1, z) = 33;
                }
                else if(block == Soil::getID() && BlockTransparent(x, y + 1, z) == false && rand() % 5 == 0)
                {
                    block = DirtBlock::getID();
                }
            }
        }
    }
}

void CraftWorld::SetWorldSavePath(std::string path)
{
    // Only called for a freshly generated world. The save slot number is reused after a
    // delete, so any column files from the previous "worldN" must go first or they would
    // be streamed in as terrain from a different seed (straight cliffs at the spawn edge).
    ColumnSaver::Drain();
    ReapSavedColumns();
    RemoveChunkDir(path);
    colFileDim = -1;
    worldSavePath = path;
    EnsureChunkDir();
    SaveLoadedColumns();
    for(int i = 0; i < 12; i++)
        UpdateStreaming();
}

// Removes a directory and everything under it. sceIoRmdir refuses a non-empty directory, and the
// per-dimension subdirectories (Save/<name>d/d1) make the column directory non-empty -- so a flat
// remove would leave an entire dimension's terrain behind, and the next world to reuse that slot
// number would stream it in as its own. Names are collected before anything is touched, because
// deleting entries while sceIoDread is still walking a directory can skip the ones after them.
static void RemoveDirTree(const std::string& dir)
{
    std::vector<std::string> entries;
    SceUID d = sceIoDopen(dir.c_str());
    if(d < 0)
        return;
    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));
    while(sceIoDread(d, &ent) > 0)
    {
        if(ent.d_name[0] == '.' && (ent.d_name[1] == 0 || (ent.d_name[1] == '.' && ent.d_name[2] == 0)))
        {
            memset(&ent, 0, sizeof(ent));
            continue;
        }
        entries.push_back(dir + "/" + ent.d_name);
        memset(&ent, 0, sizeof(ent));
    }
    sceIoDclose(d);

    for(size_t i = 0; i < entries.size(); i++)
    {
        // Whether this is a directory is asked by opening it as one: the dirent's mode bits are not
        // filled in reliably on every firmware, and sceIoDopen is already the API in use here.
        SceUID sub = sceIoDopen(entries[i].c_str());
        if(sub >= 0)
        {
            sceIoDclose(sub);
            RemoveDirTree(entries[i]);
        }
        else
            sceIoRemove(entries[i].c_str());
    }
    sceIoRmdir(dir.c_str());
}

void CraftWorld::RemoveChunkDir(const std::string& lmsPath)
{
    if(lmsPath.empty())
        return;
    RemoveDirTree(lmsPath + "d");
}

void CraftWorld::EnsureColumnFileIndex()
{
    if(worldSavePath.empty() || CHUNK_SIZE <= 0)
        return;
    // colFileSide < 0: the directory could not be read for this world and dimension, so there is no
    // index and every load asks the memory stick, as before. Never assume a file is absent on a guess:
    // a column regenerated over a saved one overwrites the player's work when it is unloaded.
    if(colFileSide != 0 && colFileDim == dimension && colFilePath == worldSavePath)
        return;
    EnsureChunkDir();
    colFileDim = dimension;
    colFilePath = worldSavePath;
    // The streamer's own limit (UpdateStreaming): 4096 blocks, or the world size if larger.
    int limit = worldLimit > 4096 ? worldLimit : 4096;
    if(limit < WORLD_SIZE)
        limit = WORLD_SIZE;
    colFileSide = (limit + CHUNK_SIZE - 1) / CHUNK_SIZE;
    colFileBits.assign((size_t)((colFileSide * colFileSide + 7) / 8), 0);

    char dir[512];
    if(dimension == 0)
        sprintf(dir, "%sd", worldSavePath.c_str());
    else
        sprintf(dir, "%sd/d%d", worldSavePath.c_str(), dimension);
    SceUID d = sceIoDopen(dir);
    if(d < 0)
    {
        colFileSide = -1;
        colFileBits.clear();
        Aurora::Utils::Logger::Instance()->LogMessage("column file index: cannot read %s (%08x), loads will probe the card\n", dir, (unsigned int)d);
        return;
    }
    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));
    int found = 0;
    int entries = 0;
    int readResult;
    while((readResult = sceIoDread(d, &ent)) > 0)
    {
        entries++;
        int cx = 0, cz = 0;
        if(sscanf(ent.d_name, "c.%d.%d.gz", &cx, &cz) == 2)
        {
            NoteColumnFile(cx, cz, true);
            found++;
        }
        memset(&ent, 0, sizeof(ent));
    }
    sceIoDclose(d);
    if(readResult < 0)
    {
        // The walk broke off part way: the bits set so far are true, the ones not reached are not.
        colFileSide = -1;
        colFileBits.clear();
        Aurora::Utils::Logger::Instance()->LogMessage("column file index: read of %s failed (%08x) after %d entries, loads will probe the card\n", dir, (unsigned int)readResult, entries);
        return;
    }
    Aurora::Utils::Logger::Instance()->LogMessage("column file index: %d file(s) in %s (%d entries)\n", found, dir, entries);
}

bool CraftWorld::ColumnFileKnownAbsent(int cx, int cz)
{
    EnsureColumnFileIndex();
    if(colFileSide <= 0 || cx < 0 || cz < 0 || cx >= colFileSide || cz >= colFileSide)
        return false;
    const int bit = cz * colFileSide + cx;
    return (colFileBits[bit >> 3] & (1u << (bit & 7))) == 0;
}

void CraftWorld::NoteColumnFile(int cx, int cz, bool exists)
{
    if(colFileSide <= 0 || cx < 0 || cz < 0 || cx >= colFileSide || cz >= colFileSide)
        return;
    const int bit = cz * colFileSide + cx;
    if(exists)
        colFileBits[bit >> 3] |= (unsigned char)(1u << (bit & 7));
    else
        colFileBits[bit >> 3] &= (unsigned char)~(1u << (bit & 7));
}

void CraftWorld::EnsureChunkDir()
{
    if(worldSavePath.empty())
        return;
    std::string dir = worldSavePath + "d";
    sceIoMkdir(dir.c_str(), 0777);
    if(dimension != 0)
    {
        char sub[512];
        sprintf(sub, "%s/d%d", dir.c_str(), dimension);
        sceIoMkdir(sub, 0777);
    }
}

std::string CraftWorld::ColumnFilePath(int cx, int cz)
{
    char name[256];
    if(dimension == 0)
        sprintf(name, "%sd/c.%d.%d.gz", worldSavePath.c_str(), cx, cz);
    else
        // Other dimensions get their own subdirectory. The overworld's files stay exactly where
        // they have always been, so every world saved before dimensions existed still loads and
        // nothing is ever migrated. The End takes d2 without another format change.
        sprintf(name, "%sd/d%d/c.%d.%d.gz", worldSavePath.c_str(), dimension, cx, cz);
    return std::string(name);
}

void CraftWorld::SaveColumnFile(ChunkColumn* col)
{
    // A column the sliced loader has not finished is air and unlit stone. Writing it would make the
    // next visit load that half-built column and never generate the rest.
    if(!col || !col->generated || worldSavePath.empty())
        return;
    MC_PERF_PHASE_BEGIN(kPhSave);
    EnsureColumnFileIndex();
    // level 1: block data compresses well anyway and the PSP CPU is the bottleneck
    gzFile f = gzopen(ColumnFilePath(col->cx, col->cz).c_str(), "wb1");
    if(!f)
    {
        MC_PERF_PHASE_END(kPhSave);
        return;
    }
    int magic = 0x3243484C; // LHC2: block ids are 16-bit. LHC L (0x4C43484C) was 8-bit.
    gzwrite(f, &magic, sizeof(int));
    gzwrite(f, &worldSeed, sizeof(int));
    gzwrite(f, &worldStamp, sizeof(int));
    gzwrite(f, &col->cx, sizeof(int));
    gzwrite(f, &col->cz, sizeof(int));
    gzwrite(f, &col->chunkSize, sizeof(int));
    gzwrite(f, &col->height, sizeof(int));
    int vol = col->Volume();
    gzwrite(f, col->blocks, sizeof(block_t) * vol);
    gzwrite(f, col->settings, sizeof(block_settings_t) * vol);
    gzclose(f);
    NoteColumnFile(col->cx, col->cz, true);
    MC_PERF_PHASE_END(kPhSave);
}

bool CraftWorld::LoadColumnFile(int cx, int cz)
{
    if(worldSavePath.empty())
        return false;
    // A column the saver still holds is the newest copy there is -- newer than any file, which may
    // not even be written yet. Take it straight back rather than reading it.
    ChunkColumn* held = ColumnSaver::Reclaim(cx, cz);
    if(held != 0)
    {
        held->generated = true;
        held->surfaceReady = true;
        MC_PERF_INC(kColumnsDone);
        HashInsertColumn(held);
        return true;
    }
    if(ColumnFileKnownAbsent(cx, cz))
        return false;
    gzFile f = gzopen(ColumnFilePath(cx, cz).c_str(), "rb");
    if(!f)
        return false;
    int magic = 0, fileSeed = 0, fileStamp = 0, fileCx = 0, fileCz = 0, cs = 0, ht = 0;
    gzread(f, &magic, sizeof(int));
    gzread(f, &fileSeed, sizeof(int));
    gzread(f, &fileStamp, sizeof(int));
    gzread(f, &fileCx, sizeof(int));
    gzread(f, &fileCz, sizeof(int));
    gzread(f, &cs, sizeof(int));
    gzread(f, &ht, sizeof(int));
    // A file that belongs to another world (old save slot reused) is ignored and will be
    // overwritten when this column is generated and saved.
    const int kColumnMagicV1 = 0x4C43484C;
    const int kColumnMagicV2 = 0x3243484C;
    if((magic != kColumnMagicV1 && magic != kColumnMagicV2) || fileSeed != worldSeed || fileStamp != worldStamp ||
       fileCx != cx || fileCz != cz || cs != CHUNK_SIZE || ht != WORLD_HEIGHT)
    {
        gzclose(f);

        // magic 0 means the header read returned nothing at all: a truncated write, from a card that
        // filled up or a session cut off mid-save. Such a file can never become valid. Left in place
        // it is re-opened, re-read, re-logged and the column regenerated and relit every time it
        // streams back in -- several times a second -- which is both log spam and allocation churn on
        // a heap that cannot afford it. Removing it lets the next save rewrite it cleanly.
        //
        // Only the unreadable case is removed. A file that merely belongs to another world is left
        // alone, as it was: that one is a legitimate old save the player may still want.
        if(magic == 0)
        {
            sceIoRemove(ColumnFilePath(cx, cz).c_str());
            NoteColumnFile(cx, cz, false);

            static unsigned int sNextUnreadableLogAt = 0;
            unsigned int now = sceKernelGetSystemTimeLow();
            if(sNextUnreadableLogAt != 0 && now < sNextUnreadableLogAt)
                return false;
            sNextUnreadableLogAt = now + 5000000u;
        }

        Aurora::Utils::Logger::Instance()->LogMessage("column file c.%d.%d rejected (magic %x seed %d/%d stamp %d/%d)\n",
            cx, cz, magic, fileSeed, worldSeed, fileStamp, worldStamp);
        return false;
    }
    ChunkColumn* col = new ChunkColumn(cx, cz, CHUNK_SIZE, WORLD_HEIGHT);
    int vol = col->Volume();
    if(magic == kColumnMagicV1)
    {
        unsigned char* narrow = new unsigned char[vol];
        int got = gzread(f, narrow, vol);
        if(got == vol)
        {
            for(int i = 0; i < vol; i++)
                col->blocks[i] = narrow[i];
        }
        delete[] narrow;
        gzread(f, col->settings, vol);
    }
    else
    {
        gzread(f, col->blocks, sizeof(block_t) * vol);
        gzread(f, col->settings, vol);
    }
    gzclose(f);
    col->generated = true;
    MC_PERF_INC(kColumnsDone);
    col->surfaceReady = true;
    HashInsertColumn(col);
    return true;
}

void CraftWorld::SaveLoadedColumns()
{
    EnsureChunkDir();
    // A world save must leave nothing in flight: what the saver holds is written before this returns.
    ColumnSaver::Drain();
    ReapSavedColumns();
    for(unsigned int i = 0; i < loadedColumns.size(); i++)
        SaveColumnFile(loadedColumns[i]);
}

void CraftWorld::MarkAllColumnsGenerated()
{
    for(unsigned int i = 0; i < loadedColumns.size(); i++)
    {
        loadedColumns[i]->generated = true;
        loadedColumns[i]->surfaceReady = true;
        RemoveFloatingSnow(loadedColumns[i]);   // caves / lakes may have carved under a snow layer
    }
    worldGenerating = false;
    spawnGenClip = false;
    Aurora::Utils::Logger::Instance()->LogMessage("gen done columns=%d region=%d..%d,%d..%d heapUsedKB=%d heapFreeKB=%d\n",
        (int)loadedColumns.size(), genMinX, genMaxX, genMinZ, genMaxZ, HeapUsedKB(), HeapFreeKB());
    MC_DebugHeapCheckpoint("gen done");
}

// How much of the heap the world keeps free at all times, in KB.
//
// This is the difference between finishing a load and dying inside the work that follows it. It used
// to be a bare 768 KB, checked only before starting a load, which is less than a single section mesh:
// the trace from the console shows the heap at "mallFreeKB=47" with the guard satisfied, then
// "mesh OOM at 2016,48,2000" and finally "HEAP GUARD: OUT OF MEMORY allocating 98304 bytes". The
// heap had been filled right to the brim by loading, and the next allocation - the mesher's, which
// runs after the guard has already been passed - had nowhere to go.
//
// A section mesh measures up to ~700 KB (the block-type mesh in the same log is 1007 KB), generation
// of a column is a ~200 KB burst, and a frame does some smaller work besides, so 768 KB was below the
// cost of the very next thing the game does. 2.5 MB covers a section plus a column plus slack.
//
// The cost is real and is the trade this makes: the heap settles ~1.7 MB lower than it used to, which
// is room for roughly 10 fewer resident columns. That is a shorter view on a 32 MB PSP-1000, in
// exchange for not exiting out of memory three seconds into the world.
static const int kWorldReserveKB = 2560;

// What one resident column actually costs, blocks and light plus its meshes. Measured from the
// console's own checkpoints: generation added 3036 KB for 36 columns (84 KB each) and meshing added
// another 3240 KB for the same 36 (90 KB each).
static const int kColumnCostKB = 180;

// How many columns one streaming tick may give back under memory pressure. Bounded because
// HeapFreeKB() is deliberately cached for a second (see its comment - the uncached walk is what used
// to stutter the streamer), so a loop cannot see its own progress and would otherwise free far more
// than the deficit called for.
static const int kPressureUnloadPerTick = 2;

// The floor the STREAMER obeys. Deliberately much smaller than the reserve above, and a different
// question: the reserve is a *target* ("what the heap should be left holding so the work after a load
// has somewhere to go"), the floor is a *limit* ("the point below which one more column is asking the
// allocator for something it does not have").
//
// The two used to be the same number, and on a 32 MB partition that stopped the streamer for good.
// The free memory actually achievable with a world resident is about 1 MB -- the card's own log sits
// between 599 and 1044 KB for an entire session -- so a 2560 KB gate is a condition that never comes
// true. needLoad was therefore false on every tick, the pressure path went on shedding whatever it
// could reach (42 columns -> 32 columns and still falling), and the player was left in a bubble that
// was being taken apart and never refilled. That, not the mesh rate, is the visible gap between
// chunks: the streamer had stopped, so the chunks ahead were never asked for.
//
// 896 KB is derived from what one load needs, not picked to fit. A column is ~84 KB to generate and
// ~90 KB per section to mesh, and the mesher's scratch tops out at 256 KB (see MeshScratch), so a
// column plus its first section plus scratch is ~430 KB. This leaves better than 2x headroom on the
// single largest allocation the next step can make. It is a floor for STARTING work and no promise
// that the work is free, which is why a failed mesh is now re-queued rather than dropped.
static const int kStreamFloorKB = 896;

// How many chunk columns around the player stay resident. One column is ~84 KB// (2-byte block ids + 1-byte light), so a radius of 5 is 121 columns = ~10 MB
// before meshes. On a 32 MB PSP-1000 (or whenever free memory gets tight) shrink
// to the view distance plus one buffer ring instead of two.
// The view distance the world actually streams and meshes at.
//
// The nether gets one chunk less than the player asked for, and the reason is measured rather than
// aesthetic. It is a cave network, so far more of its resident sections carry exposed faces and
// therefore geometry: 75 sections meshed at entry for its 36 resident columns, against the overworld's
// 51 for the same 36. Those 75 are ~5.7 MB of the entry heap (~76 KB each), which leaves 1919 KB free
// and a resident set that does not fit alongside the textures, sound and code on a 32 MB build. The
// probe reproduces the hardware fault: an out-of-memory exit ~14 s into a walk, while meshing 4-10
// sections a second.
//
// Mesh VISIBILITY culling (MC_MESH_VISIBLE_ONLY) does not rescue it, because at world entry there is no
// frustum yet, and the nether's peak is set by how much is resident, not by when it is built. So this
// reduces what is resident: one chunk less of zone, ring and spawn region. `playerZoneBB` is the box
// both the mesher and the drawn section list come from, so shortening it shortens the mesh set and the
// drawn set together and keeps them consistent; the fog is pulled to the same distance in StatePlay so
// the shorter view never shows an unmeshed edge.
//
// The overworld is unchanged -- this only ever returns `horizontalViewDistance` there.
int CraftWorld::EffectiveViewDistance() const
{
    int view = (int)mainOptions.horizontalViewDistance;
    if(view < 1)
        view = 1;
    if(dimension == 1)
    {
        view -= 1;
        if(view < 1)
            view = 1;
    }
    return view;
}

bool CraftWorld::ColumnWithinStreamRing(int cx, int cz)
{
    if(CHUNK_SIZE <= 0)
        return false;
    // The VISIBLE ring, not StreamRadius(): this is called from the block-read path, and StreamRadius()
    // calls HeapFreeKB() and can flip the low-memory hysteresis, which must not happen per block read.
    // Gating on the visible ring is also the correct side to err: a column inside it is one the streamer
    // is about to create anyway, and one outside it is one the player cannot see yet, so a read there
    // returning air (and the boundary being relit when the streamer arrives) is the right answer.
    int ring = EffectiveViewDistance();
    int pcx = ChunkFloorDiv((int)playerPos.x, CHUNK_SIZE);
    int pcz = ChunkFloorDiv((int)playerPos.z, CHUNK_SIZE);
    int adx = cx - pcx; if(adx < 0) adx = -adx;
    int adz = cz - pcz; if(adz < 0) adz = -adz;
    return adx <= ring && adz <= ring;
}

int CraftWorld::StreamRadius()
{
    int view = EffectiveViewDistance();
    if(view < 1)
        view = 1;
    // Hysteresis. A single threshold flip-flopped once the heap settled near 5 MB:
    // radius shrank -> outer ring unloaded -> memory freed -> radius grew -> ring
    // reloaded from disk + remeshed + relit -> memory dropped -> repeat forever.
    // That thrash is the "laggy for the rest of the session, even back at spawn".
    int freeKB = HeapFreeKB();
    if(lowMemoryMode)
    {
        if(freeKB > 8 * 1024)
            lowMemoryMode = false;
    }
    else if(freeKB < 4 * 1024)
        lowMemoryMode = true;
    // Low memory gives up the buffer ring, not just its width.
    //
    // It used to be view+1, and on this device view is 2, so the ring was 3 -- a 7x7 fill -- while
    // the nether's resident set already could not fit 6x6. That left the pressure path with nothing
    // to act on: UnloadDistantColumns only ever gives back columns *beyond* the ring it is handed,
    // and every resident column was inside radius 3, so it freed one and the streamer (same ring)
    // put it straight back. The card's log is that loop exactly -- `mesh OOM` at the same few
    // coordinates, `mem pressure: freeKB=693 gave back 1 column(s) outside ring 3, columns=37`, and
    // free parking in the hundreds of KB for the rest of the session.
    //
    // view+0 makes the ring the visible ring and nothing more, so the columns the pressure path can
    // reach actually exist: 5x5 = 25 resident instead of 7x7 = 49. The cost is the pre-load buffer
    // ring (the chunks you can't see yet), which is what a low-memory device should be spending
    // first -- the alternative measured here was not a shorter view, it was an out-of-memory exit.
    int radius = view + (lowMemoryMode ? 0 : 2);
    if(radius < 2)
        radius = 2;
    return radius;
}

bool CraftWorld::ColumnLoadedAt(int x, int z)
{
    if(CHUNK_SIZE <= 0)
        return false;
    if(x < 0 || z < 0 || x >= worldLimit || z >= worldLimit)
        return false;
    ChunkColumn* col = FindColumn(ChunkFloorDiv(x, CHUNK_SIZE), ChunkFloorDiv(z, CHUNK_SIZE));
    return col && col->generated;
}

// Two callers, one rule. Passing nothing means the ordinary streaming case: unload what has drifted
// outside the resident ring, guarded by the ring the player can see.
//
// The pressure caller passes `keepCount` as -1 for "no count limit", because there the question is not
// "is this column due to go" but "the heap is full and this is the one to give back". Those two limits
// answer different questions, so neither may veto the other -- see the early return below, which is
// where an earlier version of this got it wrong.
void CraftWorld::UnloadDistantColumns(int avoidRing, int keepCount)
{
    if(CHUNK_SIZE <= 0 || loadedColumns.empty())
        return;

    if(avoidRing < 0)
    {
        int radius = StreamRadius();
        avoidRing = radius + 1;
        keepCount = (2 * radius + 1) * (2 * radius + 1);
        if(keepCount < 25)
            keepCount = 25;
    }

    // The two limits are tested separately on purpose. This was one conjunctive test --
    // "inside the ring AND still under the count" -- which meant that passing -1 for the count, as the
    // pressure caller does, made "size <= -1" false and so switched the ring off with it. The unloader
    // then gave back whatever was farthest, including the column the player was standing in, and the
    // next frame read a column that no longer existed. Explicit and separate is the whole point here:
    // the ring is absolute, the count is a floor on how much may be given back at once.
    if(keepCount >= 0 && (int)loadedColumns.size() <= keepCount)
        return;

    int pcx = ChunkFloorDiv((int)playerPos.x, CHUNK_SIZE);
    int pcz = ChunkFloorDiv((int)playerPos.z, CHUNK_SIZE);

    int farthest = -1;
    int farScore = -1;
    for(int i = (int)loadedColumns.size() - 1; i >= 0; i--)
    {
        ChunkColumn* col = loadedColumns[i];
        if(!col)
            continue;
        int adx = col->cx - pcx;
        int adz = col->cz - pcz;
        if(adx < 0) adx = -adx;
        if(adz < 0) adz = -adz;
        // Absolute: a column the player can see is never given back, however tight memory gets. If
        // every remaining column is inside this ring then there is nothing to free and the caller has
        // to live with the heap it has.
        if(adx <= avoidRing && adz <= avoidRing)
            continue;
        // The row kept one chunk past the ring, and the column currently being sliced. Giving either
        // back is the loader deleting its own lookahead.
        if(StreamColumnPinned(col->cx, col->cz))
            continue;
        int score = adx + adz;
        if(score > farScore)
        {
            farScore = score;
            farthest = i;
        }
    }
    if(farthest < 0)
        return;

    ChunkColumn* col = loadedColumns[farthest];
    int cx = col->cx;
    int cz = col->cz;
    DestroyMeshesForColumn(col);
    HashRemoveColumn(col);
    // Recorded before the delete, so an on-demand block read cannot rebuild it on the next tick.
    RememberEvictedColumn(cx, cz);
    streamOpsThisFrame++;
    if(!worldSavePath.empty() && col->generated)
    {
        // Off the frame: the saver thread compresses and writes it while this thread waits on the GE
        // and the vblank, and ReapSavedColumns deletes it once the file is on the card. With both
        // saver slots busy the write happens here, as it always did.
        MC_PERF_PHASE_BEGIN(kPhSave);
        EnsureColumnFileIndex();
        const bool queued = ColumnSaver::Enqueue(col, ColumnFilePath(cx, cz).c_str(), worldSeed, worldStamp);
        MC_PERF_PHASE_END(kPhSave);
        if(queued)
            return;
        SaveColumnFile(col);
    }
    delete col;
}

// Columns the saver has finished writing: their files exist now, and their memory comes back.
void CraftWorld::ReapSavedColumns()
{
    ChunkColumn* done[4];
    int doneCx[4];
    int doneCz[4];
    const int n = ColumnSaver::Reap(done, doneCx, doneCz, 4);
    for(int i = 0; i < n; i++)
    {
        if(doneCx[i] != 0x7fffffff)
            NoteColumnFile(doneCx[i], doneCz[i], true);
        else
        {
            // The column is gone with its file unwritten; say so, but not once per column.
            static unsigned int sNextSaveFailLogAt = 0;
            const unsigned int now = sceKernelGetSystemTimeLow();
            if(sNextSaveFailLogAt == 0 || (int)(now - sNextSaveFailLogAt) >= 0)
            {
                sNextSaveFailLogAt = now + 5000000u;
                int stage = 0, code = 0;
                ColumnSaver::LastFailure(stage, code);
                Aurora::Utils::Logger::Instance()->LogMessage("column save failed off-frame (c.%d.%d) stage=%d code=%08x\n",
                    done[i] ? done[i]->cx : -1, done[i] ? done[i]->cz : -1, stage, (unsigned int)code);
            }
        }
        delete done[i];
    }
}

// How long the player must stand in a portal before it takes them. Java's survival figure is four
// seconds; two is used here so the mechanic is quick to test, and it is one constant to raise.
static const float kPortalTravelTime = 2.0f;
// The nether's lava sea. Mirrors NETHER_LAVA_LEVEL in WorldGenerator.cpp, which is file-static there.
static const int kPortalNetherLavaLevel = 31;

void CraftWorld::UnloadAllColumns()
{
    // The sliced column is about to be deleted with everything else. Drop the job first so the next
    // frame does not resume it into freed memory, and so a half-built column is not what gets saved
    // (SaveColumnFile refuses a column that is not generated).
    StreamCancelJob();
    // Whatever the saver still holds is finished and freed first; the saves below are synchronous.
    ColumnSaver::Drain();
    ReapSavedColumns();
    // Frees everything, one column at a time, saving first. UnloadDistantColumns cannot be pressed into
    // this job: it gives back a single column per call by design, and its ring is deliberately absolute
    // so it will never release the column the player is standing in -- which is exactly the one the
    // dimension swap most needs gone.
    while(!loadedColumns.empty())
    {
        ChunkColumn* col = loadedColumns.back();
        if(!col)
        {
            loadedColumns.pop_back();
            continue;
        }
        if(!worldSavePath.empty())
            SaveColumnFile(col);
        DestroyMeshesForColumn(col);
        HashRemoveColumn(col);   // erases it from loadedColumns too
        delete col;
    }
    streamOpsThisFrame = 0;

    // The near-player caches must be cleared, and this is the one place it matters. UnloadDistantColumns
    // only ever frees columns far enough away that they were never in these, so it can leave them alone;
    // a full unload frees the very columns they point at, and the next frame would read freed memory.
    lastCol = 0;
    for(int i = 0; i < 3; i++)
        for(int j = 0; j < 3; j++)
            meshColGrid[i][j] = 0;
    // Every column has been removed from the hash by HashRemoveColumn above, so this is belt and
    // braces -- but a stale head here would route a lookup to freed memory, and it costs nothing.
    for(int i = 0; i < kColHashSize; i++)
        colHash[i] = 0;
}

// Is the player's body inside a portal block? Both ends of the body are tested, so standing half in a
// portal -- which is what happens as you walk into one -- still counts and starts the wobble.
// The portal cell the player is standing in, if any. Travel derives its destination from this rather
// than from the exact feet position, so entering the same portal from either end of its interior -- or
// from any column of a wide one -- maps to the same destination.
bool CraftWorld::PlayerPortalCell(int &outX, int &outY, int &outZ)
{
    const int x = (int)floorf(playerPos.x);
    const int z = (int)floorf(playerPos.z);
    const int yFeet = (int)floorf(playerPos.y - playerGrowth + 0.1f);
    const int yHead = (int)floorf(playerPos.y - 0.1f);
    if(!ColumnLoadedAt(x, z))
        return false;
    for(int y = yFeet; y <= yHead; y++)
    {
        if(y < 0 || y >= WORLD_HEIGHT)
            continue;
        if(IsPortalBlock(GetBlock(x, y, z)))
        {
            outX = x;
            outY = y;
            outZ = z;
            return true;
        }
    }
    return false;
}

bool CraftWorld::PlayerInPortal()
{
    int px, py, pz;
    return PlayerPortalCell(px, py, pz);
}

// A portal is only held up by its obsidian ring, so taking any block out of the ring has to take the
// portal with it. Java does that with a scheduled tick on BlockPortal which re-measures the frame and
// calls breakBlock when it no longer closes; there is no block tick here, so the break does it instead.
//
// Called for a cell that has just been emptied, only when the block removed from it was obsidian. It
// looks in the 3x3x3 around the cell because the portal cells nearest a frame block are orthogonal for
// a straight run of the ring but *diagonal* at a corner: break the corner of a 4x5 frame and its
// closest portal block is one step on each of two axes, not one step on one axis.
void CraftWorld::BreakPortalNear(int x, int y, int z)
{
    std::vector<Vector3> stack;

    for(int dz = -1; dz <= 1; dz++)
    {
        for(int dy = -1; dy <= 1; dy++)
        {
            for(int dx = -1; dx <= 1; dx++)
            {
                int px = x + dx, py = y + dy, pz = z + dz;
                if(px < 0 || py < 0 || pz < 0 || px >= worldLimit || py >= WORLD_HEIGHT || pz >= worldLimit)
                    continue;
                if(IsPortalBlock(GetBlock(px, py, pz)))
                    stack.push_back(Vector3(px, py, pz));
            }
        }
    }
    if(stack.empty())
        return;

    // 6-connected flood over the portal cells. A frame is one flat sheet, so its portal is a single
    // connected component and any cell of it finds all of it. No visited set is needed: the cell is
    // emptied as it is taken, and a cell that is no longer a portal block is skipped when it is
    // reached again.
    std::vector<Vector3> touched;
    while(stack.empty() == false)
    {
        Vector3 p = stack.back();
        stack.pop_back();

        int px = (int)p.x, py = (int)p.y, pz = (int)p.z;
        if(px < 0 || py < 0 || pz < 0 || px >= worldLimit || py >= WORLD_HEIGHT || pz >= worldLimit)
            continue;
        if(IsPortalBlock(GetBlock(px, py, pz)) == false)
            continue;

        GetBlock(px, py, pz) = 0;
        touched.push_back(p);

        // A legal interior is at most 21x21, so 512 cells is generous; the cap is only here so a
        // corrupted save cannot turn this into an unbounded walk of the world.
        if(touched.size() > 512)
            break;

        stack.push_back(Vector3(px - 1, py, pz));
        stack.push_back(Vector3(px + 1, py, pz));
        stack.push_back(Vector3(px, py - 1, pz));
        stack.push_back(Vector3(px, py + 1, pz));
        stack.push_back(Vector3(px, py, pz - 1));
        stack.push_back(Vector3(px, py, pz + 1));
    }

    // The emptied cells have to be remeshed, and the light the portal was holding over them given back
    // to the block that now shines through the gap.
    //
    // The light refresh is a 19x19x19 scan per call and is capped the way the frame's own light queue is:
    // a legal interior can be 21x21, so one scan per emptied cell would be hundreds of scans in the frame
    // the player breaks a block. Eight cover any portal the ring test will light.
    int lightRefreshes = 0;
    for(unsigned int i = 0; i < touched.size(); i++)
    {
        AddChunkToUpdate(getChunkId(touched[i]));
        if(lightRefreshes < 8)
        {
            UpdateLightAreaIn(touched[i]);
            lightRefreshes++;
        }
    }
}

// Half-width of the square searched on arrival for a frame that is already there. 16 is not arbitrary:
// it is the largest radius the 3x3 chunk ring built by TravelToDimension is guaranteed to cover, so
// the search can never reach a column that is not loaded. It also comfortably absorbs the rounding of
// the 1:8 scale -- the mapping can land up to 8 blocks from the frame it came from, since eight nether
// blocks share one overworld one.
static const int kPortalSearchRadius = 16;

bool CraftWorld::FindPortalNear(int wx, int wz, int &outX, int &outY, int &outZ)
{
    int bestDist = -1;
    int bX = 0, bY = 0, bZ = 0;
    for(int dz = -kPortalSearchRadius; dz <= kPortalSearchRadius; dz++)
    {
        const int z = wz + dz;
        if(z < 0 || z >= worldLimit)
            continue;
        for(int dx = -kPortalSearchRadius; dx <= kPortalSearchRadius; dx++)
        {
            const int x = wx + dx;
            if(x < 0 || x >= worldLimit)
                continue;
            if(!ColumnLoadedAt(x, z))
                continue;
            const int dist = dx * dx + dz * dz;
            if(bestDist >= 0 && dist >= bestDist)
                continue;   // cannot beat the frame already found
            for(int y = 0; y < WORLD_HEIGHT; y++)
            {
                if(IsPortalBlock(GetBlock(x, y, z)))
                {
                    // The lowest cell of the column is the bottom of the interior, which is the height
                    // the player belongs at.
                    bestDist = dist;
                    bX = x; bY = y; bZ = z;
                    break;
                }
            }
        }
    }
    if(bestDist < 0)
        return false;
    outX = bX;
    outY = bY;
    outZ = bZ;
    return true;
}

// The camera roll while the player is in a portal, in radians. Two harmonics rather than one, so it
// reads as water rather than a metronome, and the peak is about 3.7 degrees -- clearly visible, and
// still far short of the point where a roll starts to feel like the game is broken.
float CraftWorld::GetPortalWobbleAngle() const
{
    if(portalWobble <= 0.0f)
        return 0.0f;

    // Eased in over the first fraction of a second. Without this the very first frame inside the portal
    // already carried the full amplitude, so the camera appeared to snap sideways the instant the player
    // stepped in rather than being drawn through -- the "too sharp" half of the entrance.
    const float kRamp = 0.7f;
    float envelope = portalTimer / kRamp;
    if(envelope > 1.0f)
        envelope = 1.0f;
    envelope = envelope * envelope * (3.0f - 2.0f * envelope);   // smoothstep

    return (sinf(portalWobble) + 0.30f * sinf(portalWobble * 2.0f)) * 0.050f * envelope;
}

// The purple wash over the screen during a transition. The ceiling is 0.78 rather than 1 on purpose:
// the world has to stay readable underneath, and a fully opaque frame reads as a loading screen rather
// than as being pulled through something.
static const float kPortalFadeMaxAlpha = 0.78f;

float CraftWorld::GetPortalOverlayAlpha() const
{
    if(portalFade <= 0.0f)
        return 0.0f;
    const float a = portalFade > 1.0f ? 1.0f : portalFade;
    // Square root, not square. The pull lasts kPortalTravelTime and portalFade climbs linearly across it,
    // so squaring pushed the wash under 20% for the whole of the first 1.1 seconds and left it at its
    // ceiling for only the instant before the swap -- which is why the transition read as having no
    // overlay at all rather than as a subtle one. The root rises immediately and still eases, so the
    // colour is plainly there while the player is being pulled and strongest as the world changes.
    return sqrtf(a) * kPortalFadeMaxAlpha;
}

// Builds a minimum 4x5 obsidian ring (a 2x3 interior) standing on the floor at (px, py-1, pz), in the
// X plane so its opening faces +-Z, then lights it. A pocket is cleared first: the nether is solid
// netherrack, and an arrival entombed in rock with only a 2x3 slot to stand in is not an arrival.
bool CraftWorld::BuildPortalFrameAt(int px, int py, int pz)
{
    if(py < 3 || py + 3 >= WORLD_HEIGHT)
        return false;

    for(int dx = -1; dx <= 2; dx++)
    {
        for(int dy = 0; dy <= 2; dy++)
        {
            for(int dz = 0; dz <= 2; dz++)
                GetBlock(px + dx, py + dy, pz + dz) = 0;
        }
    }

    const block_t obsidian = Obsidian::getID();
    // Floor and ceiling run one cell wider than the interior at each end, which is what folds the two
    // corner columns into the same run -- the frame test in TryLightPortal expects exactly that.
    for(int dx = -1; dx <= 2; dx++)
    {
        GetBlock(px + dx, py - 1, pz) = obsidian;
        GetBlock(px + dx, py + 3, pz) = obsidian;
    }
    for(int dy = -1; dy <= 3; dy++)
    {
        GetBlock(px - 1, py + dy, pz) = obsidian;
        GetBlock(px + 2, py + dy, pz) = obsidian;
    }

    return TryLightPortal(px, py, pz);
}

void CraftWorld::TravelToDimension(int newDim)
{
    if(newDim == dimension || newDim < 0 || newDim > 2)
        return;

    // The player lands here immediately and the mesher culls against the neighbours, so the whole ring
    // has to exist, not just the centre column -- a single column would leave a hole around them. This
    // is the one hitch in the swap, and it is deliberate: generation is the expensive part and it is
    // better paid once on arrival than streamed in while the player is already walking.
    auto ensureRing = [&](int wx, int wz)
    {
        const int ccx = ChunkFloorDiv(wx, CHUNK_SIZE);
        const int ccz = ChunkFloorDiv(wz, CHUNK_SIZE);
        for(int dz = -1; dz <= 1; dz++)
            for(int dx = -1; dx <= 1; dx++)
                EnsureColumn(ccx + dx, ccz + dz, true);
    };

    // Where the player is entering from: the portal cell under them, falling back to their feet if the
    // block were somehow gone by now.
    int srcX = (int)floorf(playerPos.x);
    int srcY = (int)floorf(playerPos.y);
    int srcZ = (int)floorf(playerPos.z);
    PlayerPortalCell(srcX, srcY, srcZ);

    const bool visited = dimState[newDim].valid;

    // 1. Remember the dimension being left exactly as it stands. The console holds one dimension at a
    // time, so this is the only record left once the columns below are gone.
    dimState[dimension].valid = true;
    dimState[dimension].playerPos = playerPos;
    dimState[dimension].dayTime = worldDayTime;

    // 2. Free everything, which saves each column on the way out. 32MB does not hold two dimensions,
    // and this is the step that makes the swap possible at all. UnloadAllColumns does the saving, so
    // there is deliberately no separate SaveLoadedColumns() first -- that would write every column
    // twice.
    UnloadAllColumns();

    // 3. Switch. The clock is per dimension, so coming back to a visited one restores the time of day it
    // was left at -- that is what keeps the nether dark and the overworld on its own cycle.
    dimension = newDim;
    if(visited)
        worldDayTime = dimState[newDim].dayTime;

    // Java's 1:8 scaling: the nether is entered at an eighth of the overworld coordinates and left at
    // eight times the nether ones. Both directions clamp away from the world edge, because a portal at
    // the border has nowhere to build.
    //
    // The source is the portal cell the player is standing in, not a remembered endpoint. Deriving the
    // destination from the portal being entered, every time, is the whole fix for "a portal built in
    // the nether sends me back to the base portal": the old code kept one position per dimension and
    // restored it, so the first portal you used in a dimension was the only one that meant anything.
    int targetX;
    int targetZ;
    if(dimension == 1)
    {
        targetX = (int)floorf(srcX / 8.0f);
        targetZ = (int)floorf(srcZ / 8.0f);
    }
    else
    {
        targetX = (int)floorf(srcX * 8.0f);
        targetZ = (int)floorf(srcZ * 8.0f);
    }
    const int margin = 8;
    if(targetX < margin) targetX = margin;
    if(targetZ < margin) targetZ = margin;
    if(targetX > worldLimit - margin) targetX = worldLimit - margin;
    if(targetZ > worldLimit - margin) targetZ = worldLimit - margin;

    // Move the player before generating: the eviction test inside EnsureColumn reads playerPos to
    // decide whether a column is close enough to be worth rebuilding, so it has to already be the
    // destination. The height is fixed below, once the ground there is known.
    playerPos = Vector3(targetX + 0.5f, playerPos.y, targetZ + 0.5f);
    ensureRing(targetX, targetZ);

    // Is a frame already waiting here? The ring is built, so this can look without generating anything
    // new. Only when nothing is found is a portal constructed, which is what makes a round trip land on
    // the frame that was left rather than beside a second identical one.
    int foundX, foundY, foundZ;
    if(FindPortalNear(targetX, targetZ, foundX, foundY, foundZ))
    {
        dimState[dimension].portalPos = Vector3((float)foundX, (float)foundY, (float)foundZ);
        // Stand in the frame rather than in front of it: the arrival frame's facing is not recorded
        // anywhere, and a guessed offset can plant the player inside netherrack. The 3s cooldown below
        // is what stops them stepping straight back through instead.
        playerPos = Vector3(foundX + 0.5f, foundY + playerGrowth - 0.05f, foundZ + 0.5f);
    }
    else
    {
        int surfaceY = -1;
        if(dimension == 1)
        {
            // Find netherrack with two blocks of headroom. Scanning down from the cavern roof rather
            // than up from the sea keeps the player off the lava.
            for(int y = 70; y >= kPortalNetherLavaLevel + 2; y--)
            {
                if(GetBlock(targetX, y, targetZ) == Netherrack::getID() &&
                   GetBlock(targetX, y + 1, targetZ) == 0 &&
                   GetBlock(targetX, y + 2, targetZ) == 0)
                {
                    surfaceY = y + 1;
                    break;
                }
            }
            if(surfaceY < 0)
            {
                // No room in this column at all. Make some at a fixed height rather than dropping the
                // player into solid rock.
                surfaceY = 40;
                for(int dy = -1; dy <= 2; dy++)
                    GetBlock(targetX, surfaceY + dy, targetZ) = (dy < 0) ? Netherrack::getID() : 0;
            }
        }
        else
        {
            // The overworld has a sky, so its own surface finder is the right one.
            surfaceY = groundHeight(targetX, targetZ) + 1;
        }

        dimState[dimension].portalPos = Vector3((float)targetX, (float)surfaceY, (float)targetZ);
        BuildPortalFrameAt(targetX, surfaceY, targetZ);

        // Stand the player in front of the portal, not in it, so the cooldown below is a courtesy
        // rather than the only thing stopping an immediate return.
        playerPos = Vector3(targetX + 0.5f, surfaceY + playerGrowth - 0.05f, targetZ + 1.5f);
    }

    portalTimer = 0.0f;
    portalCooldown = 3.0f;
    portalWobble = 0.0f;
    portalFade = 1.0f;   // fully washed at the moment of the swap, then decayed by the cooldown above
    portalLeaveFirst = true;
    travelledThisFrame = true;
}

void CraftWorld::UpdatePortalTravel(float dt)
{
    // The wash decays on its own schedule so that it outlives the swap: TravelToDimension leaves it at
    // 1 and the cooldown fades it out, instead of the colour vanishing in the frame the world changes.
    portalFade -= dt * 1.6f;
    if(portalFade < 0.0f)
        portalFade = 0.0f;

    const bool inPortal = PlayerInPortal();

    // Arrival latch. Landing inside the destination frame is normal -- that is where the frame is -- and
    // without this the player would be pulled straight back the moment the cooldown expired, just for
    // standing still. Nothing happens until they have been outside a portal at least once.
    if(portalLeaveFirst)
    {
        if(inPortal)
        {
            portalTimer = 0.0f;
            portalWobble = 0.0f;
            return;
        }
        portalLeaveFirst = false;
    }

    if(portalCooldown > 0.0f)
    {
        portalCooldown -= dt;
        portalTimer = 0.0f;
        portalWobble = 0.0f;
        return;
    }

    if(!inPortal)
    {
        portalTimer = 0.0f;
        portalWobble = 0.0f;
        return;
    }

    portalWobble += dt * 3.6f;   // the wave's phase -- slow enough to read as being pulled, not shaken
    portalTimer += dt;
    // Ramps with the timer, so the screen fills purple at the same rate the player is being pulled.
    portalFade += dt / kPortalTravelTime;
    if(portalFade > 1.0f)
        portalFade = 1.0f;

    if(portalTimer >= kPortalTravelTime)
        TravelToDimension(dimension == 1 ? 0 : 1);
}

// Seconds until the next streaming tick; set by UpdateStreaming based on how urgent
// the next column is (visible ring vs. buffer ring).
static float gStreamNextDelay = 0.12f;

void CraftWorld::UpdateStreaming()
{
    MC_PERF_INC(kStreamOps);
    spawnGenClip = false;
    worldGenerating = false;
    if(CHUNK_SIZE <= 0)
        return;
    worldLimit = 4096;
    if(worldLimit < WORLD_SIZE)
        worldLimit = WORLD_SIZE;
    if(!worldGen)
    {
        worldGen = new WorldGenerator();
        worldGen->SetSeed(worldSeed);
    }
    streamOpsThisFrame = 0;
    // `stream` brackets this whole function; gen and light below nest inside it, so the phased line
    // reads stream(total) with the two expensive parts broken out rather than three siblings.
    perfInStreaming = true;
    MC_PERF_PHASE_BEGIN(kPhStreamOther);
    ReapSavedColumns();
    int pcx = ChunkFloorDiv((int)playerPos.x, CHUNK_SIZE);
    int pcz = ChunkFloorDiv((int)playerPos.z, CHUNK_SIZE);
    int radius = StreamRadius();

    int bestCx = 0, bestCz = 0, bestScore = 0x7fffffff;
    bool needLoad = false;
    for(int dz = -radius; dz <= radius; dz++)
    {
        for(int dx = -radius; dx <= radius; dx++)
        {
            int cx = pcx + dx;
            int cz = pcz + dz;
            int x0 = cx * CHUNK_SIZE;
            int z0 = cz * CHUNK_SIZE;
            if(x0 < 0 || z0 < 0 || x0 >= worldLimit || z0 >= worldLimit)
                continue;
            ChunkColumn* existing = FindColumn(cx, cz);
            if(existing && existing->generated)
                continue;
            int adx = dx < 0 ? -dx : dx;
            int adz = dz < 0 ? -dz : dz;
            int score = adx + adz;
            if(score < bestScore)
            {
                bestScore = score;
                bestCx = cx;
                bestCz = cz;
                needLoad = true;
            }
        }
    }
    // The row one chunk past the ring, in the direction of travel. Computed before the unload so that
    // row is not the column the pressure path gives back.
    StreamUpdatePins(this, pcx, pcz, radius);
    // Unload before the load, so a tick that frees room can still spend it. It used to run EVERY tick
    // unconditionally because an older version only ran it when nothing needed loading, and a player who
    // kept walking then never freed a column. The gate below keeps both properties: a save is held back at
    // most one tick, and never stacked with the load.
    if(!worldSavePath.empty())
    {
        int keep = (2 * radius + 1) * (2 * radius + 1);
        if(keep < 25)
            keep = 25;
        // The lookahead row is supposed to be resident. Counting it as surplus makes the unloader
        // fight the loader on every tick.
        int over = (int)loadedColumns.size() - keep - sPinCount;
        if(over < 0)
            over = 0;
        // A save is a memory-stick write, and the frame probe is unambiguous about the cost: the
        // streaming tick showed 13-31 ms with NO terrain generation in it, which is 2-3 of these
        // stacked (the old count was 3 when far over, 2 when over at all). So at most ONE card write
        // per tick, and only when the frame is not busy -- a save is exactly the work that should not
        // land on a frame which has just paid ~50 ms for a column.
        //
        // The memory clause is not politeness. Holding an unload back keeps a column resident (~180 KB),
        // and the nether runs with free heap in the hundreds of KB -- the first version of this gate let
        // the loaded set sit several columns high and the 150 s probe's mesh-OOM count more than tripled.
        // So the hold-back is allowed only when there is real headroom, and `over > 1` bounds it at ONE
        // extra column even then. When memory is tight, unloading happens every tick as it always did.
        const unsigned int kStreamSaveBudgetUs = 20000;   // 20 ms: a frame at/under it is not "busy"
        const int kStreamSaveFreeKB = 4096;               // leave the hold-back to machines with room
        // A save and a load in the same tick is two memory-stick operations in one frame -- the frame probe
        // read the streaming tick at ~29 ms in the overworld, which is exactly one of each. The load is the
        // point of streaming and cannot be deferred; the save can, so it waits for a tick that is not loading
        // (and, on a busy frame, for a cheaper one). `over > 1` still forces it through, so the loaded set
        // stays bounded the same way as above.
        const bool unloadNow = (over > 1) ||
            (!needLoad && (lastProcessUs <= kStreamSaveBudgetUs) && HeapFreeKB() > kStreamSaveFreeKB);
        // The streamer now wakes every frame while a column is in progress. A save is still one per
        // tick, and a tick here is no longer 0.12 s, so the card write keeps that spacing itself.
        static unsigned int sNextUnloadAt = 0;
        unsigned int unloadNowUs = sceKernelGetSystemTimeLow();
        int passes = unloadNow ? 1 : 0;
        if(passes && sNextUnloadAt != 0 && (int)(sNextUnloadAt - unloadNowUs) > 0)
            passes = 0;
        for(int i = 0; i < passes; i++)
        {
            UnloadDistantColumns();
            sNextUnloadAt = unloadNowUs + 120000u;
        }
    }
    // Memory pressure. The count above is a shape, not a budget: radius 3 asks for 49 columns, and 49
    // columns plus their meshes is more than a PSP-1000 has left once the textures and sounds are
    // resident. Because that count was the only trigger, nothing was ever given back -- the heap filled
    // to 47 KB free with the guard below satisfied, and the mesher's next 96 KB allocation failed.
    //
    // So keep columns *and* bytes: while the heap is under the reserve, give back the farthest column
    // the player cannot see, a couple per tick until it is above. Runs before the load below, so a tick
    // that frees room can still spend it on a column the view actually needs.
    // Anti-thrash: what the shedder gives back, the streamer must not spend in the same breath.
    //
    // The two used to run unconditionally against each other. Shedding triggers below
    // `kWorldReserveKB`, loading is allowed above `kStreamFloorKB`, and the two numbers overlap by
    // 1.6 MB -- so on the nether, where free memory lives inside that overlap for the whole session,
    // a tick would free a column and the next would reload one. The card's log is the loop: 29
    // `mem pressure` lines, `columns=23` through `columns=33`, and free memory parked in the
    // hundreds of KB while the mesher never once had enough to finish a section.
    //
    // Raising the load floor until the bands stop overlapping was tried before and starves the
    // streamer outright (see kStreamFloorKB). A short cooldown does not: it does not change *when*
    // loading is allowed to happen in general, only that a column the shedder has just taken back
    // stays taken back long enough to be worth taking. Everything that is normally loaded still is;
    // what stops is the two halves of one budget fighting over the same kilobytes.
    static unsigned int sNoLoadUntil = 0;

    {
        // Throttled, and it uses the streamer's own ring rather than the view distance.
        //
        // Two things were wrong here. The trigger is "the heap is under the reserve", and on a console
        // that cannot reach that reserve the condition is true on every single tick for the whole
        // session -- so a destructive action (card write, mesh destroy, delete) was being driven by a
        // condition that never clears. And the ring it gave columns back from was view+1 while the
        // streamer loads out to StreamRadius(), which is view+2 whenever lowMemoryMode is off, so the
        // streamer reloaded columns this had just freed.
        //
        // Half a second is still far more responsive than a streaming ring needs: walking crosses a
        // chunk every ~4 s, and the ring it frees from is now the same one the streamer fills.
        static unsigned int sPressureNextAt = 0;
        unsigned int pressureNow = sceKernelGetSystemTimeLow();
        if(sPressureNextAt == 0)
            sPressureNextAt = pressureNow;
        int gaveBack = 0;
        if(pressureNow >= sPressureNextAt && HeapFreeKB() < kWorldReserveKB)
        {
            sPressureNextAt = pressureNow + 500000u;
            for(int i = 0; i < kPressureUnloadPerTick && HeapFreeKB() < kWorldReserveKB; i++)
            {
                int before = (int)loadedColumns.size();
                UnloadDistantColumns(radius, -1);
                if((int)loadedColumns.size() == before)
                    break;      // nothing left outside the ring the streamer will not reload
                streamOpsThisFrame++;
                gaveBack++;
            }
        }
        // One line per second rather than per column. Logger opens, writes and closes a file, so the
        // old message-per-column was itself memory-stick I/O on the frame path.
        if(gaveBack > 0)
        {
            // Hold the streamer off while the memory just won is spent on meshing rather than on the
            // next column. See sNoLoadUntil.
            sNoLoadUntil = pressureNow + 1500000u;

            static unsigned int sPressureReportAt = 0;
            if(sPressureReportAt == 0 || pressureNow - sPressureReportAt >= 1000000u)
            {
                sPressureReportAt = pressureNow;
                Aurora::Utils::Logger::Instance()->LogMessage(
                    "mem pressure: freeKB=%d gave back %d column(s) outside ring %d, columns=%d\n",
                    HeapFreeKB(), gaveBack, radius, (int)loadedColumns.size());
            }
        }
    }
    // Memory guard: a column plus its meshes is ~180 KB. When the heap is nearly gone,
    // only load the ring the player can actually see (view distance); the buffer ring
    // waits until something is freed. Better a late chunk than an out-of-memory exit.
    if(needLoad && HeapFreeKB() < kStreamFloorKB + kColumnCostKB)
    {
        int view = (int)mainOptions.horizontalViewDistance;
        if(view < 1) view = 1;
        int adx = bestCx - pcx; if(adx < 0) adx = -adx;
        int adz = bestCz - pcz; if(adz < 0) adz = -adz;
        if(adx > view || adz > view)
            needLoad = false;
    }
    // Hard floor, and the number that matters: below this nothing new starts, whatever the ring says.
    //
    // This is kStreamFloorKB and NOT kWorldReserveKB. Gating the streamer on the reserve is what left
    // the player in a shrinking bubble -- the reserve is not reachable on this device, so the test
    // never passed and no chunk was ever loaded again. See kStreamFloorKB for the whole mechanism.
    if(needLoad && HeapFreeKB() < kStreamFloorKB)
        needLoad = false;
    // Anti-thrash cooldown, last so it overrides every gate above rather than being overridden by
    // one. Signed difference, so the comparison wraps with the clock instead of against it.
    bool cooledDown = sNoLoadUntil != 0 && (int)(sNoLoadUntil - sceKernelGetSystemTimeLow()) > 0;
    if(needLoad && cooledDown)
        needLoad = false;
    // One column in flight, a few milliseconds at a time. The resident ring is filled first. When it
    // is full, the three columns in front of the player are filled the same way, so a sprint meets
    // ground that already has blocks. A card read is still one shot -- gzip on the memory stick does
    // not resume -- and it is spaced out so those reads do not stack. A generate is not: it continues
    // on the next frame.
    bool didFileLoad = false;
    // A column the player can already see that has no surface yet is the hole. Give that
    // generate the longer slice. Once the ground exists, the mesher is the thing that is behind.
    unsigned int kStreamSliceUs = 6000;
    if(StreamJobBusy() && sStreamJob.col && !sStreamJob.col->surfaceReady)
        kStreamSliceUs = 10000;
    if(StreamJobBusy())
        StreamAdvanceJob(this, kStreamSliceUs);
    else if(needLoad)
    {
        int began = StreamBeginColumn(this, bestCx, bestCz);
        if(began == 2)
            StreamAdvanceJob(this, kStreamSliceUs);
        else if(began == 1)
            didFileLoad = true;
    }
    else if(!cooledDown)
    {
        int pinCx = 0, pinCz = 0;
        if(StreamPickPin(this, pcx, pcz, pinCx, pinCz))
        {
            int began = StreamBeginColumn(this, pinCx, pinCz);
            if(began == 2)
                StreamAdvanceJob(this, kStreamSliceUs);
            else if(began == 1)
                didFileLoad = true;
        }
    }
    int pinCx = 0, pinCz = 0;
    bool pinMissing = StreamPickPin(this, pcx, pcz, pinCx, pinCz);
    if(StreamJobBusy())
        gStreamNextDelay = 0.0f;
    else if(didFileLoad)
        gStreamNextDelay = 0.12f;
    else if(needLoad || (pinMissing && !cooledDown))
        gStreamNextDelay = 0.0f;
    else
        gStreamNextDelay = 0.2f;
    MC_PERF_PHASE_END(kPhStreamOther);
    perfInStreaming = false;
}

void CraftWorld::initWorldBlocksLight()
{
    int x, y, z;
    int level = 0;
    for(unsigned int i = 0; i < loadedColumns.size(); i++)
    {
        ChunkColumn* col = loadedColumns[i];
        int x0 = col->cx * CHUNK_SIZE;
        int z0 = col->cz * CHUNK_SIZE;
        for (int lz = 0; lz < CHUNK_SIZE; ++lz)
        {
            for (int lx = 0; lx < CHUNK_SIZE; ++lx)
            {
                x = x0 + lx;
                z = z0 + lz;
            level = 0;
            for (y = WORLD_HEIGHT - 1; y >= 0; --y)
            {
                SetShadowLevel(x,y,z,level);
                if(level < 15)
                {
                    if(GetBlock(x,y,z) == DirtBlock::getID())
                    {
                        if(level == 0)
                        {
                            GetBlock(x,y,z) = GrassBlock::getID();
                        }
                    }
                    // Explicit parens: && binds tighter than ||, so the grouping is (opaque) || (water). It
                    // was correct as written, but it read as ambiguous and the compiler said so.
                    if((GetBlock(x,y,z) != 0 && BlockRefraction(x,y,z) != 0 && BlockRefraction(x,y,z) != 2) || GetBlock(x,y,z) == WaterBlock::getID())
                    {
                        level += 1;
                    }
                    if(GetBlock(x,y,z) == WaterBlock::getID())
                    {
                        if(level != 15)
                        {
                            level += 1;
                        }
                    }
                    if(GetBlock(x,y,z) == GlassBlock::getID())
                    {
                        if(level == 0)
                        {
                            level += 1;
                        }
                    }

                    if(GetBlock(x,y,z) == LeavesBlock::getID() || GetBlock(x,y,z) == BirchLeaves::getID() || GetBlock(x,y,z) == SpruceLeaves::getID())
                    {
                        if(level == 0)
                        {
                            level += 2;
                        }
                    }
                }
            }
        }
        }
    }
}

void CraftWorld::UpdateBlocksLightNearby(const int xx, const int yy, const int zz) // Not finished, not used.
{
    int low_range_x, high_range_x;
    int low_range_z, high_range_z;

    xx - 4 < 0 ? low_range_x = 0 : low_range_x = xx - 4;
    zz - 4 < 0 ? low_range_z = 0 : low_range_z = zz - 4;

    xx + 4 >= worldLimit ? high_range_x = worldLimit - 1 : high_range_x = xx + 4;
    zz + 4 >= worldLimit ? high_range_z = worldLimit - 1 : high_range_z = zz + 4;

    for(int iteration = 1; iteration <= 4; iteration++)
    {
        for(int x = low_range_x; x <= high_range_x; x++)
        {
            for(int z = low_range_z; z <= high_range_z; z++)
            {
                if(TaxicabDistance2d(xx,zz,x,z) <= 4)
                {
                    CalculateBlocksLightAtPos(x,yy,z,iteration);
                }
            }
        }
    }
}

void CraftWorld::CalculateBlocksLightAtPos(const int xx, const int yy, const int zz, int iteration) // Not finished, not used.
{
 
}

void CraftWorld::initPutBlocksLight(const int xx, const int zz)
{
    int x, y, z;
    int level = 0;

    x = xx;
    z = zz;

    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit)
    {
        return;
    }

    for (y = WORLD_HEIGHT - 1; y >= 0; --y)
    {
        SetShadowLevel(x,y,z,level);
        if(level < 15)
        {
            int block = GetBlockNoCheck(x,y,z);
            // Explicit parens: see the GetBlock twin above -- && binds tighter than ||.
            if((block != 0 && BlockRefraction(x,y,z) != 0 && BlockRefraction(x,y,z) != 2) || block == WaterBlock::getID())
            {
                level += 1;
            }

            if(block == GlassBlock::getID())
            {
                if(level == 0)
                {
                    level += 1;
                }
            }

            if(block == LeavesBlock::getID() || block == BirchLeaves::getID() || block == SpruceLeaves::getID())
            {
                if(level == 0)
                {
                    level += 2;
                }
            }

            if(block == WaterBlock::getID())
            {
                if(level != 15)
                {
                    level += 1;
                }
            }
        }
        if(level == 15)
        {
            break;
        }
    }
}

void CraftWorld::initPutBlocksLight(const int xx, const int yy, const int zz)
{
    int x, y, z;
    int level = 0;

    x = xx;
    z = zz;
    y = yy;

    if (x < 0 || y < 0 || z < 0  || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return;
    }

    level = GetShadowLevel(x,y,z);

    for (y = yy; y >= 0; --y)
    {
        SetShadowLevel(x,y,z,level);
        if(level < 15)
        {
            // Explicit parens: see the GetBlock twin above -- && binds tighter than ||.
            if((GetBlockNoCheck(x,y,z) != 0 && BlockRefraction(x,y,z) != 0 && BlockRefraction(x,y,z) != 2) || GetBlockNoCheck(x,y,z) == WaterBlock::getID())
            {
                level += 1;
            }
            if(GetBlockNoCheck(x,y,z) == WaterBlock::getID())
            {
                if(level != 15)
                {
                    level += 1;
                }
            }
        }
        if(level == 15)
        {
            break;
        }
    }
}


void CraftWorld::buildMap()
{
    createWorldChunkss();
}

void CraftWorld::destroyMap()
{
    for(unsigned int i = 0; i < mChunks.size(); i++)
    {
        if(mChunks[i])
            mChunks[i]->reset();
    }

    for(unsigned int i = 0; i < mTransparentChunks.size(); i++)
    {
        if(mTransparentChunks[i])
            mTransparentChunks[i]->reset();
    }
}

int CraftWorld::FindChestID(Vector3 pos)
{
    Vector3 intPos = Vector3((int)pos.x,(int)pos.y,(int)pos.z);

    int id = -1;
    for(unsigned int i = 0; i < mChests.size(); i++)
    {
        Chest* NewChest = mChests[i];
        if(NewChest->chestX == (int)intPos.x && NewChest->chestY == (int)intPos.y && NewChest->chestZ == (int)intPos.z)
        {
            id = i;
            break;
        }
    }

    return id;
}

int CraftWorld::FindFurnaceID(Vector3 pos)
{
    Vector3 intPos = Vector3((int)pos.x,(int)pos.y,(int)pos.z);

    int id;
    id = -1;
    for(unsigned int i = 0; i < mFurnaces.size(); i++)
    {
        Furnace* NewFurnace = mFurnaces[i];
        if(NewFurnace->furnaceX == intPos.x && NewFurnace->furnaceY == intPos.y && NewFurnace->furnaceZ == intPos.z)
        {
            id = i;
            break;
        }
    }

    return id;
}

int CraftWorld::FindNoteBlockID(Vector3 pos)
{
    Vector3 intPos = Vector3((int)pos.x,(int)pos.y,(int)pos.z);

    int id;
    id = -1;
    for(unsigned int i = 0; i < mNoteBlocks.size(); i++)
    {
        NoteBlockEntity* TestNoteBlock = mNoteBlocks[i];
        if(TestNoteBlock->GetPosition() == intPos)
        {
            id = i;
            break;
        }
    }

    return id;
}

int CraftWorld::FindJukeboxID(Vector3 pos)
{
    Vector3 intPos = Vector3((int)pos.x,(int)pos.y,(int)pos.z);

    int id;
    id = -1;
    for(unsigned int i = 0; i < mJukeboxes.size(); i++)
    {
        JukeboxEntity* TestJukebox = mJukeboxes[i];
        if(TestJukebox->GetPosition() == intPos)
        {
            id = i;
            break;
        }
    }

    return id;
}

int CraftWorld::FindMonsterSpawnerID(Vector3 pos)
{
    Vector3 intPos = Vector3((int)pos.x,(int)pos.y,(int)pos.z);

    int id;
    id = -1;
    for(unsigned int i = 0; i < mMonsterSpawners.size(); i++)
    {
        MonsterSpawnerEntity* TestMS = mMonsterSpawners[i];
        if(TestMS->GetPosition() == intPos)
        {
            id = i;
            break;
        }
    }

    return id;
}

int CraftWorld::FindItemFrameID(Vector3 pos)
{
    Vector3 intPos = Vector3((int)pos.x,(int)pos.y,(int)pos.z);

    int id;
    id = -1;
    int n = (int)mItemFrames.size();
    if(n < 0)
        n = 0;
    if(n > 256)
        n = 256;
    for(int i = 0; i < n; i++)
    {
        ItemFrameEntity* TestIF = mItemFrames[i];
        if(TestIF != NULL && TestIF->GetPosition() == intPos)
        {
            id = i;
            break;
        }
    }

    return id;
}

void CraftWorld::setTextureSize(int texture,int chunk,int animTexture, int animChunk, int itemTexture, int itemChunk)
{
    texutreSize = texture;
    textureChunk = chunk;

    percent = (float)textureChunk / (float)texutreSize;
    percent2 = (float)animChunk / (float)animTexture;
    percent3 = (float)itemChunk / (float)itemTexture;
}

void CraftWorld::InitLightValues()
{
    lightColor = Vector3(1, 1, 1);
    fogColor = lightColor * 0.8f;
}

void CraftWorld::GetTexturesIds()
{
    textureTerrainId = TextureHelper::Instance()->GetTexture(TextureHelper::Terrain1);
    textureTerrainMipsId = TextureHelper::Instance()->GetTexture(TextureHelper::Terrain2);
    textureItemsId = TextureHelper::Instance()->GetTexture(TextureHelper::Items1);
    textureWaterAnimationId = TextureHelper::Instance()->GetTexture(TextureHelper::WaterAnimation);
    textureLavaAnimationId = TextureHelper::Instance()->GetTexture(TextureHelper::LavaAnimation);
	textureFireAnimationId = TextureHelper::Instance()->GetTexture(TextureHelper::FireAnimation);
	textureFireAnimation2Id = TextureHelper::Instance()->GetTexture(TextureHelper::FireAnimation2);
	texturePortalAnimationId = TextureHelper::Instance()->GetTexture(TextureHelper::PortalAnimation);
    textureLeatherArmor1Id = TextureHelper::Instance()->GetTexture(TextureHelper::leatherArmor1);
    textureLeatherArmor2Id = TextureHelper::Instance()->GetTexture(TextureHelper::leatherArmor2);
    textureChainArmor1Id = TextureHelper::Instance()->GetTexture(TextureHelper::chainmailArmor1);
    textureChainArmor2Id = TextureHelper::Instance()->GetTexture(TextureHelper::chainmailArmor2);
    textureIronArmor1Id = TextureHelper::Instance()->GetTexture(TextureHelper::ironArmor1);
    textureIronArmor2Id = TextureHelper::Instance()->GetTexture(TextureHelper::ironArmor2);
    textureDiamondArmor1Id = TextureHelper::Instance()->GetTexture(TextureHelper::diamondArmor1);
    textureDiamondArmor2Id = TextureHelper::Instance()->GetTexture(TextureHelper::diamondArmor2);
    textureGoldenArmor1Id = TextureHelper::Instance()->GetTexture(TextureHelper::goldenArmor1);
    textureGoldenArmor2Id = TextureHelper::Instance()->GetTexture(TextureHelper::goldenArmor2);
    textureVegetationColorMapId = TextureHelper::Instance()->GetTexture(TextureHelper::vegetationColorMap);
    textureClockId = TextureHelper::Instance()->GetTexture(TextureHelper::clockTexture);
    textureCompassId = TextureHelper::Instance()->GetTexture(TextureHelper::compassTexture);
    textureSkyColorId = TextureHelper::Instance()->GetTexture(TextureHelper::skyColorTexture);

    GetSkyColor();
}

void CraftWorld::GetSkyColor()
{
    unsigned int r,g,b,r2,g2,b2;

    //clear sky colors
    r2 = TextureManager::Instance()->getRed(0,2,textureSkyColorId) << 24;
    g2 = TextureManager::Instance()->getGreen(0,2,textureSkyColorId);
    b2 = TextureManager::Instance()->getBlue(0,2,textureSkyColorId) << 24;

    r = r2 >> 24;
    g = g2;
    b = b2 >> 24;

    dawnSunsetClearSkyColor = Vector3(r,g,b);

    r2 = TextureManager::Instance()->getRed(0,0,textureSkyColorId) << 24;
    g2 = TextureManager::Instance()->getGreen(0,0,textureSkyColorId);
    b2 = TextureManager::Instance()->getBlue(0,0,textureSkyColorId) << 24;

    r = r2 >> 24;
    g = g2;
    b = b2 >> 24;

    startClearSkyColor = Vector3(r,g,b);

    r2 = TextureManager::Instance()->getRed(0,1,textureSkyColorId) << 24;
    g2 = TextureManager::Instance()->getGreen(0,1,textureSkyColorId);
    b2 = TextureManager::Instance()->getBlue(0,1,textureSkyColorId) << 24;

    r = r2 >> 24;
    g = g2;
    b = b2 >> 24;

    endClearSkyColor = Vector3(r,g,b);

    // rainy sky colors
    r2 = TextureManager::Instance()->getRed(0,5,textureSkyColorId) << 24;
    g2 = TextureManager::Instance()->getGreen(0,5,textureSkyColorId);
    b2 = TextureManager::Instance()->getBlue(0,5,textureSkyColorId) << 24;

    r = r2 >> 24;
    g = g2;
    b = b2 >> 24;

    dawnSunsetRainySkyColor = Vector3(r,g,b);

    r2 = TextureManager::Instance()->getRed(0,3,textureSkyColorId) << 24;
    g2 = TextureManager::Instance()->getGreen(0,3,textureSkyColorId);
    b2 = TextureManager::Instance()->getBlue(0,3,textureSkyColorId) << 24;

    r = r2 >> 24;
    g = g2;
    b = b2 >> 24;

    startRainySkyColor = Vector3(r,g,b);

    r2 = TextureManager::Instance()->getRed(0,4,textureSkyColorId) << 24;
    g2 = TextureManager::Instance()->getGreen(0,4,textureSkyColorId);
    b2 = TextureManager::Instance()->getBlue(0,4,textureSkyColorId) << 24;

    r = r2 >> 24;
    g = g2;
    b = b2 >> 24;

    endRainySkyColor = Vector3(r,g,b);

    startClearSkyColor *= 1.0f/255.0f;
	endClearSkyColor *= 1.0f/255.0f;
	dawnSunsetClearSkyColor *= 1.0f/255.0f;

    startRainySkyColor *= 1.0f/255.0f;
	endRainySkyColor *= 1.0f/255.0f;
	dawnSunsetRainySkyColor *= 1.0f/255.0f;

	startSkyColor = startClearSkyColor;
	endSkyColor = endClearSkyColor;
	dawnSunsetSkyColor = dawnSunsetClearSkyColor;
}

void CraftWorld::SetWolrdTime(float time)
{
    if(time >= 22)
    {
        skyTime = 280+22.5*(time-22);
    }
    if(time < 6)
    {
        skyTime = 325+22.5*time;
    }
    if(time < 22 && time >= 6)
    {
        skyTime = 100+11.25f*(time-6);
    }
    worldDayTime = time;//std::fmod(time, 24);

    lightAngle = worldDayTime / 24.0f * 360.0f - 90.0f;
    if (lightAngle < 0)lightAngle += 360.0f;

    if (worldDayTime <= 4 || worldDayTime >= 23)
    {
        westLight = 0.0f;
        lightColor = Vector3(0.15f, 0.15f, 0.15f);
    }
    else if (worldDayTime == 5)
    {
        lightColor = Vector3(0.15, 0.15, 0.15);
        eastLight = 0.05f;
    }
    else if (worldDayTime == 6)
    {
        lightColor = Vector3(0.2, 0.2, 0.2);
        eastLight = 0.15f;
    }
    else if (worldDayTime == 7)
    {
        lightColor = Vector3(0.35, 0.35, 0.35);
        eastLight = 0.25f;
    }
    else if (worldDayTime == 8)
    {
        lightColor = Vector3(0.55, 0.55, 0.55);
        eastLight = 0.15f;
    }
    else if (worldDayTime == 9)
    {
        lightColor = Vector3(0.8, 0.8, 0.8);
        eastLight = 0.05f;
    }
    else if (worldDayTime == 10)
    {
        lightColor = Vector3(1.0, 1.0, 1.0);
        eastLight = 0.0f;
    }
    else if (worldDayTime >= 11 && worldDayTime <= 18)
    {
        lightColor = Vector3(1.0, 1.0, 1.0);
    }
    else if (worldDayTime == 19)
    {
        lightColor = Vector3(0.9, 0.9, 0.9);
    }
    else if (worldDayTime == 20)
    {
        lightColor = Vector3(0.75, 0.75, 0.75);
        westLight = 0.15f;
    }
    else if (worldDayTime == 21)
    {
        lightColor = Vector3(0.4, 0.4, 0.4);
        westLight = 0.20f;
    }
    else if (worldDayTime == 22)
    {
        lightColor = Vector3(0.2, 0.2, 0.2);
        westLight = 0.20f;
    }
    else if (worldDayTime == 23)
    {
        lightColor = Vector3(0.15, 0.15, 0.15);
        westLight = 0.0f;
    }
    else	//Shouldn't get here
    {
        lightColor = Vector3(1, 1, 1);
    }

    //ambientColor = lightColor / 3.0f;
    ambientColor = Vector3(0,0,0);
    //light face factors
}

void CraftWorld::DropThis(int id, Vector3 position)
{
    if(id > 0)
    {
        dropNum ++;
        Drop *NewDrop = new Drop(Vector3((int)position.x+0.5f,(int)position.y+0.51f,(int)position.z+0.5f), id, 1, true, dropNum);
        NewDrop->myCrft = this;
        mDrops.push_back(NewDrop);
    }
}

void CraftWorld::DropThis(int id, int am, Vector3 position)
{
    if(id > 0 && am > 0)
    {
        dropNum ++;
        Drop *NewDrop = new Drop(Vector3((int)position.x+0.5f,(int)position.y+0.51f,(int)position.z+0.5f), id, am, true, dropNum);
        NewDrop->myCrft = this;
        mDrops.push_back(NewDrop);
    }
}

void CraftWorld::DropThis(int id, int am, bool st, Vector3 position)
{
    if(id > 0 && am > 0)
    {
        dropNum ++;
        Drop *NewDrop = new Drop(Vector3((int)position.x+0.5f,(int)position.y+0.51f,(int)position.z+0.5f), id, am, st, dropNum);
        NewDrop->myCrft = this;
        mDrops.push_back(NewDrop);
    }
}

void CraftWorld::DropThisNoAlign(int id, int am, bool st, Vector3 position, Vector3 velocity)
{
    if(id > 0 && am > 0)
    {
        dropNum ++;
        Drop *NewDrop = new Drop(Vector3(position.x,position.y,position.z), id, am, st, dropNum);
        NewDrop->myCrft = this;
        NewDrop->SetVelociy(velocity);
        mDrops.push_back(NewDrop);
    }
}


void CraftWorld::DestroyDrop(int id)
{
    if(id < mDrops.size() && id >= 0)
    {
        delete mDrops[id];
        mDrops.erase(mDrops.begin()+id);
    }
}



void CraftWorld::DestroyItemFrameEntity(Vector3 position)
{
    int id = FindItemFrameID(position);
    if(id != -1)
    {
        ItemFrameEntity* DestroyIF = mItemFrames[id];

        if(DestroyIF->GetAmount() != -1)
        {
            DropThis(DestroyIF->GetItem(),DestroyIF->GetAmount(),StackableItem(DestroyIF->GetItem()),Vector3((int)position.x+0.5f,(int)position.y+0.5f,(int)position.z+0.5f));
        }
        else
        {
            if(IsItemId(DestroyIF->GetItem()))
            {
                DropThis(DestroyIF->GetItem(),1,StackableItem(DestroyIF->GetItem()),Vector3((int)position.x+0.5f,(int)position.y+0.5f,(int)position.z+0.5f));
            }
            else
            {
                DropThis(DestroyIF->GetItem(),1,true,Vector3((int)position.x+0.5f,(int)position.y+0.5f,(int)position.z+0.5f));
            }
        }
        DropThis(ItemFrameItem::getID(),1,true,Vector3((int)position.x+0.5f,(int)position.y+0.5f,(int)position.z+0.5f));

        delete mItemFrames[id];
        mItemFrames.erase(mItemFrames.begin()+id);
    }
}

void CraftWorld::SpawnTNTentity(float xx, float yy, float zz)
{
    zombieNum += 1;
    TNT *NewTNT = new TNT(this, Vector3(xx,yy,zz), Vector3(0,0,0));
    NewTNT->num = zombieNum;
    NewTNT->ignitedByPlayer = true;
    mTNTs.push_back(NewTNT);
}

void CraftWorld::SpawnTNTentity(float xx, float yy, float zz, float time)
{
    zombieNum += 1;
    TNT *NewTNT = new TNT(this, Vector3(xx,yy,zz), Vector3(0,0,0));
    NewTNT->num = zombieNum;
    NewTNT->timeToExplode = time;
    mTNTs.push_back(NewTNT);
}

// A repeater drives one horizontal direction only. It reads the cell behind it and powers the cell in
// front of it, so both the input test and the output test need these vectors, and they are used by
// GetRedstoneDustPower long before the repeater section below.
// Output direction: 0 = -Z, 1 = +X, 2 = +Z, 3 = -X (Java's metadata order, mapped to this codebase's axes).
static const int kRepeaterDirX[4] = { 0, 1, 0,-1};
static const int kRepeaterDirZ[4] = {-1, 0, 1, 0};

// The same table, exposed for the mesher. The dust artwork has to know which sides of a repeater a wire
// may run into, and that answer is this axis -- so it reads it from here rather than keeping a second
// copy that could drift out of step with the power code.
int CraftWorld::RepeaterDirX(int facing)
{
    return kRepeaterDirX[facing & 3];
}

int CraftWorld::RepeaterDirZ(int facing)
{
    return kRepeaterDirZ[facing & 3];
}

// Java BlockRedstoneWire: a line also runs one step up or down unless an opaque full cube
// sits on the lower wire and cuts the climb. Glass and other non-cubes do not cut.
static bool RedstoneClimbCut(CraftWorld* w, int x, int y, int z)
{
    if(y < 0 || y >= w->WORLD_HEIGHT)
    {
        return true;
    }
    return w->BlockSolid(x,y,z) && w->BlockTransparentOrSpecial(x,y,z) == false;
}

int CraftWorld::GetRedstoneDustPower(int x, int y, int z)
{
    const block_t dustId = Redstoneoff::getID();
    const block_t sourceId = RedstoneBlock::getID();
    const int dx[6] = {1,-1,0,0,0,0};
    const int dy[6] = {0,0,1,-1,0,0};
    const int dz[6] = {0,0,0,0,1,-1};

    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }
    if(GetBlock(x,y,z) != dustId)
    {
        return 0;
    }
    MC_PERF_INC(kRsDustQueries);

    std::vector<int> cx;
    std::vector<int> cy;
    std::vector<int> cz;
    std::vector<int> cd;
    cx.push_back(x);
    cy.push_back(y);
    cz.push_back(z);
    cd.push_back(0);

    int best = 0;
    for(unsigned int i = 0; i < cx.size() && i < 256; i++)
    {
        MC_PERF_INC(kRsWalkCells);
        int dist = cd[i];
        if(dist >= 15)
        {
            continue;
        }
        for(int dir = 0; dir < 6; dir++)
        {
            int nx = cx[i] + dx[dir];
            int ny = cy[i] + dy[dir];
            int nz = cz[i] + dz[dir];
            if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
            {
                continue;
            }
            block_t b = GetBlock(nx,ny,nz);
            // A neighbour that feeds this wire at full strength. Java's BlockRedstoneWire asks each
            // neighbour for its output, and a pressed button or a thrown lever answers 15 -- wire is a
            // mechanism, so a switch is a *direct* source for the wire touching it. That is the
            // case the old test missed: it only accepted a conductive block a button was powering, so
            // the button's own cell (the cell wire actually runs alongside) contributed nothing, and a
            // flat button never lit the line beside it. The remaining cases are a block of redstone and
            // a conductive block a switch is powering.
            bool feedsWire = false;
            if(b == sourceId)
            {
                feedsWire = true;
            }
            else if(IsSwitchBlock(b))
            {
                feedsWire = ButtonPressedFromState(GetButtonState(nx,ny,nz));
            }
            else if(b == RepeaterOn::getID())
            {
                // A powered repeater drives one cell: the block it faces. Wire sitting in front of it
                // is that cell, so it takes 15 -- and wire to its side or behind takes nothing, which
                // is what makes a repeater a one-way component rather than a second block of redstone.
                int f = RepeaterFacingFromState(GetBlockSettingsNoCheck(nx,ny,nz) & OpRedstonePowerMask);
                feedsWire = (ny == cy[i] && nx + kRepeaterDirX[f] == cx[i] && nz + kRepeaterDirZ[f] == cz[i]);
            }
            else if(IsRedstoneTorchBlock(b))
            {
                // A lit torch drives every neighbour except the block it hangs on, so wire beside a
                // torch lights and wire underneath a wall torch does not. (dx,dy,dz) runs from this
                // wire cell to the torch, which is the direction Java hands to isProvidingWeakPower.
                feedsWire = IsRedstoneTorchLit(b)
                    && RedstoneTorchPowersTowards(TorchFaceFromBlock(b), nx - cx[i], ny - cy[i], nz - cz[i]);
            }
            else if(b != dustId && IsRedstoneConductive(b))
            {
                // A conductive block only carries the signal on if something is strongly powering *it*.
                // Three things can: a pressed button mounted on it, a powered repeater facing it, and
                // a lit torch touching it (the block above a standing torch is the usual case).
                // `RepeaterOutputAt` / `RedstoneTorchPowerAt` are used rather than the whole of
                // `GetBlockRedstonePower` deliberately -- that function asks this very wire cell for
                // its power, so calling it here would recurse into the walk currently being computed.
                feedsWire = GetButtonPowerAt(nx,ny,nz) > 0
                    || RepeaterOutputAt(nx,ny,nz) > 0
                    || RedstoneTorchPowerAt(nx,ny,nz) > 0;
            }
            if(feedsWire)
            {
                int p = 15 - dist;
                if(p > best)
                {
                    best = p;
                }
                if(best >= 15)
                {
                    return 15;
                }
                continue;
            }
            if(b != dustId || dist + 1 >= 15)
            {
                continue;
            }
            bool seen = false;
            for(unsigned int j = 0; j < cx.size(); j++)
            {
                if(cx[j] == nx && cy[j] == ny && cz[j] == nz)
                {
                    seen = true;
                    break;
                }
            }
            if(!seen)
            {
                cx.push_back(nx);
                cy.push_back(ny);
                cz.push_back(nz);
                cd.push_back(dist + 1);
            }
        }

        // Staircase: dust at (hx, y±1, hz). The 6-neighbour walk above never sees these -- they
        // are diagonal -- and without them a line cannot climb a block. Java cuts the climb when
        // an opaque cube sits on the lower wire.
        const int hx[4] = {1,-1,0,0};
        const int hz[4] = {0,0,1,-1};
        for(int d = 0; d < 4; d++)
        {
            int sx = cx[i] + hx[d];
            int sz = cz[i] + hz[d];
            if(sx < 0 || sz < 0 || sx >= worldLimit || sz >= worldLimit)
            {
                continue;
            }
            int stairY[2];
            int nStair = 0;
            int uy = cy[i] + 1;
            if(uy < WORLD_HEIGHT && RedstoneClimbCut(this, cx[i], uy, cz[i]) == false)
            {
                stairY[nStair++] = uy;
            }
            int dy2 = cy[i] - 1;
            if(dy2 >= 0 && RedstoneClimbCut(this, sx, cy[i], sz) == false)
            {
                stairY[nStair++] = dy2;
            }
            for(int c = 0; c < nStair; c++)
            {
                int sy = stairY[c];
                if(GetBlock(sx,sy,sz) != dustId || dist + 1 >= 15)
                {
                    continue;
                }
                bool seen = false;
                for(unsigned int j = 0; j < cx.size(); j++)
                {
                    if(cx[j] == sx && cy[j] == sy && cz[j] == sz)
                    {
                        seen = true;
                        break;
                    }
                }
                if(!seen)
                {
                    cx.push_back(sx);
                    cy.push_back(sy);
                    cz.push_back(sz);
                    cd.push_back(dist + 1);
                }
            }
        }
    }
    return best;
}

// Java redstone rule: only conductive blocks can be powered. Mechanisms and wires are not conductors
// (a block of redstone and redstone dust do not conduct, and TNT cannot be powered).
bool CraftWorld::IsRedstoneConductive(block_t b)
{
    if(b == 0 || b >= blockTypes.size())
    {
        return false;
    }
    if(b == TNTBlock::getID() || b == Redstoneoff::getID() || b == RedstoneBlock::getID())
    {
        return false;
    }
    return blockTypes[b].solid;
}

// The raw union: what power do the sources touching this cell provide to it? Dust resting on it or
// pointing into it, a neighbouring live switch, a powered repeater facing it, a lit torch beside it.
// A block of redstone is deliberately not in this list: Java's block of redstone activates adjacent
// *mechanisms* and feeds adjacent dust, but it does not power adjacent opaque blocks. Mechanisms
// pick it up in GetRedstonePowerTo instead. Deliberately says nothing about what kind of block the
// cell is -- that is the caller's business, and the two callers below want the same union for
// different reasons.
int CraftWorld::GetPowerProvidedToCell(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }

    const block_t dustId = Redstoneoff::getID();
    const int hx[4] = {1,-1,0,0};
    const int hz[4] = {0,0,1,-1};
    int best = 0;

    // Dust sitting on top of this block
    if(y + 1 < WORLD_HEIGHT && GetBlock(x,y+1,z) == dustId)
    {
        int p = GetRedstoneDustPower(x,y+1,z);
        if(p > best)
        {
            best = p;
        }
    }
    // Dust pointing into this block from the same level
    for(int d = 0; d < 4; d++)
    {
        int nx = x + hx[d];
        int nz = z + hz[d];
        if(nx < 0 || nz < 0 || nx >= worldLimit || nz >= worldLimit)
        {
            continue;
        }
        if(GetBlock(nx,y,nz) != dustId)
        {
            continue;
        }
        int p = GetRedstoneDustPower(nx,y,nz);
        if(p > best)
        {
            best = p;
        }
    }
    // A pressed button touching this block powers it (Java: weak power 15, which also covers the
    // mount face that is strongly powered)
    if(best < 15)
    {
        int buttonPower = GetButtonPowerAt(x,y,z);
        if(buttonPower > best)
        {
            best = buttonPower;
        }
    }
    // A powered repeater strongly powers the single block it faces (and nothing else).
    if(best < 15)
    {
        int repeaterPower = RepeaterOutputAt(x,y,z);
        if(repeaterPower > best)
        {
            best = repeaterPower;
        }
    }
    // A lit redstone torch weakly powers the blocks around it -- all except the one it hangs on, so a
    // torch standing on this block does not energise it and the inverter below cannot latch itself on.
    if(best < 15)
    {
        int torchPower = RedstoneTorchPowerAt(x,y,z);
        if(torchPower > best)
        {
            best = torchPower;
        }
    }
    return best;
}

// Java: "is this block powered?" -- and only a conductive block can be. TNT is a mechanism, dust is a
// wire, and neither conducts. Otherwise this is the union above. A powered block is what soft powers
// the mechanisms around it, which is how a classic cannon is wired: the dust line runs along the
// outside of the barrel, powers those blocks, and the TNT touching them lights.
int CraftWorld::GetBlockRedstonePower(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }
    if(IsRedstoneConductive(GetBlock(x,y,z)) == false)
    {
        return 0;
    }
    return GetPowerProvidedToCell(x,y,z);
}

// Java World.isBlockIndirectlyGettingPowered -- the question a *mechanism* asks: "am I activated?" A
// lamp, a charge of TNT and a door are mechanisms; a conductor is not one, and neither is a wire.
//
// This is not the same question as GetBlockRedstonePower, and the difference is all of "a lever on the
// middle of three lamps lights all three". A mechanism is activated by the sources touching it -- which
// the union above already answers -- and *also* by a neighbouring conductive block that is itself
// powered. That second term is Java's soft power, the single step of conduction that lets a lever
// light the lamp it is mounted on and then, through that lamp, the lamps on either side of it.
//
// The step is exactly one and cannot chain, and it is worth seeing why. This function asks
// GetBlockRedstonePower() about each neighbour, and that function counts *sources*, never neighbours.
// So the lamps at the ends of the row are activated (they were asked as mechanisms) but not powered
// (nobody is sourcing them), and the row stops dead there. That asymmetry is the whole trick, and it
// is why this cannot be folded into GetBlockRedstonePower: put the neighbour term in there and a row of
// lamps would light every lamp in the world, each one powering the next.
//
// Two other things stop this from being an alias of the union:
//   * the cell asking need not be conductive at all -- TNT is explicitly not, and it is the main caller;
//   * a wire answers on all six sides for a mechanism (Java BlockRedstoneWire.isProvidingWeakPower
//     reports the wire's charge whatever side it is asked about), so dust *under* a lamp activates it,
//     while dust under a block does not power that block.
int CraftWorld::GetRedstonePowerTo(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }

    int best = GetPowerProvidedToCell(x,y,z);
    if(best >= 15)
    {
        return best;
    }

    const block_t dustId = Redstoneoff::getID();
    const block_t sourceId = RedstoneBlock::getID();
    const int dx[6] = {1,-1,0,0,0,0};
    const int dy[6] = {0,0,1,-1,0,0};
    const int dz[6] = {0,0,0,0,1,-1};

    for(int dir = 0; dir < 6; dir++)
    {
        int nx = x + dx[dir];
        int ny = y + dy[dir];
        int nz = z + dz[dir];
        if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
        {
            continue;
        }
        block_t nb = GetBlock(nx,ny,nz);
        if(nb == dustId)
        {
            // A wire is a source in every direction, not only the one it points.
            int p = GetRedstoneDustPower(nx,ny,nz);
            if(p > best)
            {
                best = p;
            }
        }
        else if(nb == sourceId)
        {
            // A block of redstone activates adjacent mechanisms (lamps, TNT, pistons, doors)
            // without powering adjacent opaque blocks. That second part is why it is not in
            // GetPowerProvidedToCell -- a stone next to one must stay unpowered.
            best = 15;
        }
        else if(IsRedstoneConductive(nb))
        {
            // The one step of conduction. `IsRedstoneConductive` first, because it is one vector read and
            // it throws out air, wire, torches and TNT before any of them can cost a dust walk.
            int p = GetBlockRedstonePower(nx,ny,nz);
            if(p > best)
            {
                best = p;
            }
        }
        if(best >= 15)
        {
            break;
        }
    }
    return best;
}

// Java BlockTNT: a primed TNT becomes an entity and its cell turns to air. Redstone dust needs a block
// underneath it, so dust sitting on the TNT pops off as an item instead of floating. A cannon wired
// with dust running across the roof used to keep that dust after ignition.
bool CraftWorld::PrimeTNTBlock(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return false;
    }
    if(GetBlock(x,y,z) != TNTBlock::getID())
    {
        return false;
    }

    GetBlock(x,y,z) = 0;
    SpawnTNTentity(x + 0.5f, y + 0.5f, z + 0.5f);
    SoundManager::Instance()->PlayFuseSound();

    if(y + 1 < WORLD_HEIGHT && GetBlock(x,y+1,z) == Redstoneoff::getID())
    {
        GetBlock(x,y+1,z) = 0;
        DropThis(LootBlock(Redstoneoff::getID()), Vector3((float)x + 0.5f, (float)y + 1.5f, (float)z + 0.5f));
        QueueLightAreaIn(Vector3((float)x, (float)(y + 1), (float)z));
        int cidUp = getChunkId(Vector3((float)x, (float)(y + 1), (float)z));
        if(cidUp != -1)
        {
            RebuildTransparentMeshChunk(cidUp);
            rebuildNearestChunks(cidUp, Vector3((float)x, (float)(y + 1), (float)z));
        }
    }

    int cid = getChunkId(Vector3((float)x, (float)y, (float)z));
    if(cid != -1)
    {
        RebuildOpaqueMeshChunk(cid);
        rebuildNearestChunks(cid, Vector3((float)x, (float)y, (float)z));
    }
    return true;
}

// Java BlockTNT.onNeighborBlockChange: TNT lights as soon as it receives a redstone signal. A signal
// reaches it from any power source touching it, or from a neighbouring conductive block that something
// is powering (that last case is the plain cannon wired along its outside). See GetRedstonePowerTo.
bool CraftWorld::TryIgniteTNT(int x, int y, int z)
{
    if(GetBlock(x,y,z) != TNTBlock::getID())
    {
        return false;
    }
    // Java BlockTNT.onNeighborBlockChange calls World.isBlockIndirectlyGettingPowered, which is exactly
    // GetRedstonePowerTo: the sources touching the charge, plus one step through a neighbouring powered
    // block (that last case is the plain cannon wired along its outside).
    //
    // This replaces a hand-rolled six-neighbour loop that tested for dust, a block of redstone, a live
    // switch and a powered neighbour by name. The loop had grown a special case for a powered repeater
    // facing the charge and still missed two things the shared rule gets for free: a lit redstone torch
    // leaning on the charge, and a *lamp* that a lever or repeater has powered, either of which ignites
    // TNT in Java. Dust in any of the six directions also works now -- the loop checked all six, but the
    // block-power read it fell back on only ever looked above and along the four horizontals.
    return GetRedstonePowerTo(x,y,z) > 0;
}

// --- Button (Java BlockButton) ------------------------------------------------------------------

// Facing 0 mounts on the block below the cell, 1 on the block above, then -X, +X, -Z, +Z.
static const int kButtonDirX[6] = { 0, 0,-1, 1, 0, 0};
static const int kButtonDirY[6] = {-1, 1, 0, 0, 0, 0};
static const int kButtonDirZ[6] = { 0, 0, 0, 0,-1, 1};

bool CraftWorld::IsButtonBlock(block_t b)
{
    return b == ButtonStone::getID() || b == ButtonWood::getID();
}

bool CraftWorld::IsLever(block_t b)
{
    return b == Lever::getID();
}

// A switch is anything whose cell holds "on/off, pointing this way" in the state nibble. Buttons and
// levers differ only in what turns them off -- a timer and a second use -- and everything that *reads* a
// switch wants both: the wire feeding off one, a torch checking whether its mount is powered, a lamp
// being lit by one, TNT being primed by one, and the light engine being told not to write a light value
// over the nibble that holds the state. IsButtonBlock stays buttons-only for the paths that really are
// button-shaped: the press timer list, and the support-block announcement a press makes.
bool CraftWorld::IsSwitchBlock(block_t b)
{
    return IsButtonBlock(b) || IsLever(b);
}

// Blocks that keep their own state in the settings nibble instead of the block-light level. Every
// reader and writer of that nibble has to leave these cells alone, or the state it holds is silently
// overwritten with a light value (which is exactly what used to make a pressed button jump faces).
bool CraftWorld::IsStateCellBlock(block_t b)
{
    return IsSwitchBlock(b) || b == RepeaterOff::getID() || b == RepeaterOn::getID()
        || IsPistonBlock(b);
}

// Break one of those cells and the block id goes to air, but the nibble stays behind -- and every light
// reader decides whether to trust a nibble by asking IsStateCellBlock, which now answers no, because the
// id is air. So the leftover state gets read as a block light level by the entire neighbourhood. A fourth
// tick repeater leaves 12..15 there, the brightest of the four settings, which is why the artefact only
// appeared on the last tick: the spot where the repeater was broken was handed back to the light engine
// with a phantom source of 12 to 15 in it, and the drop back to the real level around it reads as a hard
// shadow. Clearing the nibble while the block is being removed is the only moment the information is
// still available to clear.
void CraftWorld::ClearStateCellNibble(int x, int y, int z)
{
    if (x < 0 || x >= worldLimit || z < 0 || z >= worldLimit || y < 0 || y >= WORLD_HEIGHT)
    {
        return;
    }
    ChunkColumn* col = ColumnForBlock(x, z);
    if(!col || y >= col->height)
    {
        return;
    }
    int cs = col->chunkSize;
    block_settings_t& blockSettings = col->settings[col->Index(x - col->cx * cs, y, z - col->cz * cs)];
    blockSettings &= (block_settings_t)(~OpRedstonePowerMask);
}

// --- Piston (Java BlockPistonBase / BlockPistonExtension) ----------------------------------------

// The push direction, in the same 6-neighbour order the button and lever mountings use. Sharing the
// order is not cosmetic: the direction tables, the support-block walk and the "which way is this thing
// pointing" reasoning are all the same shape of question, and two tables a file apart is how they drift.
static const int kPistonDirX[6] = { 0, 0,-1, 1, 0, 0};
static const int kPistonDirY[6] = {-1, 1, 0, 0, 0, 0};
static const int kPistonDirZ[6] = { 0, 0, 0, 0,-1, 1};

int CraftWorld::PistonDirX(int facing){ return (facing >= 0 && facing < 6) ? kPistonDirX[facing] : 0; }
int CraftWorld::PistonDirY(int facing){ return (facing >= 0 && facing < 6) ? kPistonDirY[facing] : 0; }
int CraftWorld::PistonDirZ(int facing){ return (facing >= 0 && facing < 6) ? kPistonDirZ[facing] : 0; }

bool CraftWorld::IsPistonBaseBlock(block_t b)
{
    return b == PistonOff::getID() || b == PistonOn::getID()
        || b == StickyPistonOff::getID() || b == StickyPistonOn::getID();
}

bool CraftWorld::IsPistonHeadBlock(block_t b)
{
    return b == PistonHead::getID() || b == StickyPistonHead::getID();
}

// Everything that has to be left alone by the light engine and by every other nibble reader. The head
// is included even though its own nibble is only a direction: the head has no direction of its own, it
// is placed to agree with the base it belongs to, so what the head really needs is for the *light*
// engine to stay out of the cell -- which IsStateCellBlock is what buys.
bool CraftWorld::IsPistonBlock(block_t b)
{
    return IsPistonBaseBlock(b) || IsPistonHeadBlock(b);
}

bool CraftWorld::IsStickyPiston(block_t b)
{
    return b == StickyPistonOff::getID() || b == StickyPistonOn::getID()
        || b == StickyPistonHead::getID();
}

bool CraftWorld::IsPistonExtended(block_t b)
{
    return b == PistonOn::getID() || b == StickyPistonOn::getID();
}

// Bits 0-2 hold one of six directions and nothing else. There is no bit 3 state to mask off, because
// "extended" is the block id rather than a state bit, so this returns the nibble whole.
int CraftWorld::PistonFacingFromState(int state)
{
    return state & 7;
}

int CraftWorld::PistonStateFrom(int facing)
{
    return facing & 7;
}

int CraftWorld::GetPistonFacing(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }
    if(IsPistonBlock(GetBlock(x,y,z)) == false)
    {
        return 0;
    }
    return PistonFacingFromState(GetBlockSettingsNoCheck(x,y,z) & OpRedstonePowerMask);
}

void CraftWorld::SetPistonState(int x, int y, int z, int facing)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return;
    }
    if(IsPistonBlock(GetBlock(x,y,z)) == false)
    {
        return;
    }
    block_settings_t& s = GetBlockSettings(x,y,z);
    s = (block_settings_t)((s & ~OpRedstonePowerMask) | (PistonStateFrom(facing) & OpRedstonePowerMask));
}

// Java BlockPistonBase.setBlockBoundsBasedOnState. Retracted, the base is the full cube it looks like.
// Extended, it is the twelve pixels of body that stay behind -- the whole 16 x 16 face, four pixels shy
// of the wall its head left -- and the recess that body opens in front of itself is
// deliberately not collidable, which is what lets a player stand in the space around the rod exactly as
// they can in vanilla.
//
// The thickness is 0.75 in the one other place it is written: the a 0..12 body box in the mesher. An
// earlier revision had a four-pixel plate against the back wall here instead, which is the rod's own
// cell rather than the body's -- so the player could walk through eight twelfths of a solid piston, and
// the mesh and the collider agreed on the wrong number. tools/check-piston-mesh.py compares the two.
//
// The box is centred on the cell like every other box in this build: a full block spans -0.5..0.5.
BoundingBox CraftWorld::PistonCellBox(int facing, bool extended)
{
    if(extended == false)
    {
        return BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0.5f,0.5f));
    }

    // Twelve of sixteen units thick, against the boundary the base pushes away from.
    const float f = 0.75f;
    const float dx = (float)PistonDirX(facing);
    const float dy = (float)PistonDirY(facing);
    const float dz = (float)PistonDirZ(facing);

    // min/max per axis, built so the body sits at -(dir)*0.5 .. -(dir)*0.5 + dir*f, i.e. against the
    // wall the head moved away from, with the four pixels it does not reach left open on the push side.
    // Writing it as a min/max pair rather than six branches keeps the negative-direction cases from
    // being a separate set of sign mistakes -- the axis whose dir is zero gets the full -0.5..0.5 span
    // either way.
    float x0 = -0.5f, x1 = 0.5f, y0 = -0.5f, y1 = 0.5f, z0 = -0.5f, z1 = 0.5f;
    if(dx > 0.0f)      { x0 = -0.5f;      x1 = -0.5f + f; }
    else if(dx < 0.0f) { x0 =  0.5f - f;  x1 =  0.5f;     }
    if(dy > 0.0f)      { y0 = -0.5f;      y1 = -0.5f + f; }
    else if(dy < 0.0f) { y0 =  0.5f - f;  y1 =  0.5f;     }
    if(dz > 0.0f)      { z0 = -0.5f;      z1 = -0.5f + f; }
    else if(dz < 0.0f) { z0 =  0.5f - f;  z1 =  0.5f;     }

    return BoundingBox(Vector3(x0,y0,z0),Vector3(x1,y1,z1));
}

// --- Piston behaviour (Java BlockPistonBase) -----------------------------------------------------

// How long a piston takes to act on a power edge. Java does not do the work inside the neighbour
// notification that noticed the edge either: it queues a block event that runs on the following tick.
// The delay is not really the point -- the point is that the notification arrives from inside a dust
// walk (`UpdateRedstonePower`) or from a placement, and extending a piston rewrites blocks and then
// announces the world again. Deferring means the piston reads a settled world when it fires, on the
// same list the repeaters and the torches already count down on.
static const float kPistonTickDelay = 0.05f;   // one redstone tick

// Java Block.getMobilityFlag()/canPush(), restated as the one rule this port needs. Each clause is a
// block that could not survive being one cell further along, and the reasons are different:
//
//   * A piston head is not a block in its own right, it is the far half of the base behind it. Shoving a
//     head on its own would leave a head with no piston; shoving the base would leave the head behind.
//     Both outcomes are refused, which is why an *extended* base is refused here as well -- the
//     retracted one is a self-contained cube and moves like anything else.
//   * A bed or a door is two cells drawn as one piece. Shoving one cell of it strands the other in mid
//     air, so the whole family is refused. Java marks these with mobility flag 1 for the same reason.
//   * A chest, furnace, note block, jukebox, spawner or item frame keeps its contents in a list keyed on
//     its coordinates, and there is no move for any of those lists. The block is refused rather than
//     shoved and silently emptied.
//
// Everything else moves, which is where a piston earns its keep: stone, dirt, planks, sand, wool, and a
// retracted piston. A block that is merely mounted on a face -- a torch, a ladder, a lever, a button --
// is movable too and pops off afterwards, which is what the support sweep on every changed cell is for:
// that is exactly what a vanilla piston does to the torch it pushes.
static bool PistonCanMoveBlock(block_t b)
{
    if(b == 0)
    {
        return false;
    }
    if(CraftWorld::IsPistonHeadBlock(b) || CraftWorld::IsPistonExtended(b))
    {
        return false;
    }
    if(b >= BedBlock1::getID() && b <= BedBlock8::getID())
    {
        return false;
    }
    if(b >= Door1::getID() && b <= Door10::getID())
    {
        return false;
    }
    if(b >= ChestBlock1::getID() && b <= ChestLock::getID())
    {
        return false;
    }
    if(b == FurnaceOff::getID() || b == FurnaceOn::getID())
    {
        return false;
    }
    if(b == NoteBlock::getID() || b == JukeBox::getID() || b == Spawner::getID())
    {
        return false;
    }
    if(b >= ItemFrame1::getID() && b <= ItemFrame4::getID())
    {
        return false;
    }
    return true;
}

// How far along the pushed line a cell is, for a cell in front of the base: 1 is the head cell, and 0
// means "not on the line at all" -- behind the piston, off to one side, or out of the world. The line is
// axis aligned in all six directions, so this is a sign test on the one axis the direction moves along.
static int PistonLineOffset(int bx, int by, int bz, int facing, int ex, int ey, int ez)
{
    const int ox = ex - bx;
    const int oy = ey - by;
    const int oz = ez - bz;
    switch(facing)
    {
        case 0: if(ox != 0 || oz != 0 || oy > 0) return 0; return -oy;  // pushing down
        case 1: if(ox != 0 || oz != 0 || oy < 0) return 0; return  oy;  // pushing up
        case 2: if(oy != 0 || oz != 0 || ox > 0) return 0; return -ox;  // pushing -X
        case 3: if(oy != 0 || oz != 0 || ox < 0) return 0; return  ox;  // pushing +X
        case 4: if(ox != 0 || oy != 0 || oz > 0) return 0; return -oz;  // pushing -Z
        case 5: if(ox != 0 || oy != 0 || oz < 0) return 0; return  oz;  // pushing +Z
    }
    return 0;
}

// One whole cell of an entity shove, for the entities whose position is not authoritative: a drop only
// ever tests the block *under* itself (Drop::Update), an arrow decides it is stuck from the single block
// at its own position, and a primed TNT integrates its own step. None of them resolves a horizontal
// block collision that a test here could predict, so a test here would be a promise nothing keeps. The
// mobs are the other case, and they are not shoved through this at all -- see PistonShoveMobs.
static void PistonShoveOffset(int facing, int k, int distance, float* ox, float* oy, float* oz)
{
    const float steps = (float)(distance + 1 - k);
    *ox = (float)CraftWorld::PistonDirX(facing) * steps;
    *oy = (float)CraftWorld::PistonDirY(facing) * steps;
    *oz = (float)CraftWorld::PistonDirZ(facing) * steps;
}

// Which axis a facing pushes along -- 0 = X, 1 = Y, 2 = Z -- and which way, as +1 or -1.
static void PistonPushAxis(int facing, int* axis, int* sign)
{
    switch(facing)
    {
        case 0: *axis = 1; *sign = -1; return;   // down
        case 1: *axis = 1; *sign =  1; return;   // up
        case 2: *axis = 0; *sign = -1; return;   // -X
        case 3: *axis = 0; *sign =  1; return;   // +X
        case 4: *axis = 2; *sign = -1; return;   // -Z
        default: *axis = 2; *sign = 1; return;   // +Z
    }
}

// Is this box in the way of the push, and how far along the line does it start?
//
// `distance` is the index of the first air cell, so the push ends up filling line indices 1..distance. A
// box is in the way when it overlaps any of those cells, which is three tests: on the two axes across the
// push it has to overlap the single cell column the piston pushes down, and along the push its own cell
// span has to meet 1..distance.
//
// The answer is the box's *trailing* cell -- the one its back face is in -- because that is what decides
// how far it has to go to be clear: a box whose back ends up past `distance` is out of the line, so the
// shove is `distance + 1 - trailing` cells. Testing the cell a mob's feet happen to stand in, which is
// what this did, gets both ends of that wrong: it misses a mob straddling a cell boundary, because
// floor(position) names only one of the two cells its body is in, and the blocks then arrive inside it;
// and for a mob whose body overhangs the line it names the wrong cell, so the shove is short by the
// overhang.
static bool PistonBoxTrailIndex(const BoundingBox& box, int bx, int by, int bz, int facing, int distance,
                                int* outTrail)
{
    int axis = 0, sign = 1;
    PistonPushAxis(facing, &axis, &sign);
    const float base[3] = {(float)bx, (float)by, (float)bz};
    const float lo[3] = {box.min.x, box.min.y, box.min.z};
    const float hi[3] = {box.max.x, box.max.y, box.max.z};

    for(int a = 0; a < 3; a++)
    {
        if(a != axis && (hi[a] <= base[a] || lo[a] >= base[a] + 1.0f))
        {
            return false;   // clear of the piston's column on an axis it does not push along
        }
    }

    // Distance along the push measured from the base cell's near face, so a cell's index is the integer
    // part of it. Taking the measurement from the far face instead when the piston pushes backwards keeps
    // one formula for all six facings.
    const float tLo = (sign > 0) ? (lo[axis] - base[axis]) : (base[axis] - hi[axis]);
    const float tHi = (sign > 0) ? (hi[axis] - base[axis]) : (base[axis] - lo[axis]);

    const int trailing = (int)floorf(tLo);
    // A box's leading face sitting exactly on a boundary occupies the cell behind it, not the one in
    // front, so the epsilon is what keeps a mob flush against the far end of the line in it.
    const int leading = (int)floorf(tHi - 0.0001f);
    if(leading < 1 || trailing > distance)
    {
        return false;
    }
    // A trailing index below 1 puts the box's back inside the piston's own cell, which the piston's
    // collision does not allow. Clamping rather than trusting it keeps the shove no longer than the line
    // the push actually asked for.
    *outTrail = (trailing < 1) ? 1 : trailing;
    return true;
}

// Mobs carry a bounding box beside their position -- their own collision and the arrow sweep both read
// the box and never the position -- so a shove has to move both. The eight mob lists are of eight
// concrete types, so this is a template over them; the test and the arithmetic are shared.
//
// What a shove may NOT do is put a mob somewhere it could not stand, which is what this did until
// 2026-09-27: the offset was added with no test at all, so a sheep pushed against a wall was placed
// inside it -- head in the glass, feet in the air -- and the mob AI spent the next frames jumping out of
// a block it should never have entered. The destination is now tested with the mob's own collision test,
// the same three points its AI walks with (feet, head, and a foot point just off the ground), so a shove
// can only land a mob where it could have walked. If it cannot stand there it stays put: vanilla's
// Entity.moveEntity would slide it as far as it could go, but a piston is not a walking mob and the cells
// it is being pushed out of are precisely the ones about to be filled, so a partial shove would leave it
// inside them anyway. Staying is the one answer that leaves it somewhere it fits.
template<class T>
static void PistonShoveMobs(std::vector<T*>& list, CraftWorld* world, int bx, int by, int bz, int facing, int distance)
{
    for(unsigned int i = 0; i < list.size(); i++)
    {
        T* mob = list[i];
        if(mob == NULL)
        {
            continue;
        }
        // The box is derived from the position, and a mob that has not updated yet would be judged by
        // the box of wherever it last was.
        mob->UpdateBBox();
        int trail = 0;
        if(PistonBoxTrailIndex(mob->bBox, bx, by, bz, facing, distance, &trail) == false)
        {
            continue;
        }
        const int steps = distance + 1 - trail;
        const Vector3 dest = mob->position + Vector3((float)(CraftWorld::PistonDirX(facing) * steps),
                                                     (float)(CraftWorld::PistonDirY(facing) * steps),
                                                     (float)(CraftWorld::PistonDirZ(facing) * steps));
        if(world->SolidAtPointForEntity(dest, mob->colliderD)
        || world->SolidAtPointForEntity(dest + mob->head, mob->colliderU)
        || world->SolidAtPointForEntity(dest + Vector3(mob->foot.x, mob->foot.y + 0.1f, mob->foot.z), mob->colliderD))
        {
            continue;
        }
        mob->position = dest;
        mob->UpdateBBox();
    }
}

void CraftWorld::SchedulePiston(int x, int y, int z)
{
    for(unsigned int i = 0; i < mRepeaterTimers.size(); i++)
    {
        if(mRepeaterTimers[i].kind == 2 && mRepeaterTimers[i].x == x && mRepeaterTimers[i].y == y && mRepeaterTimers[i].z == z)
        {
            return;
        }
    }
    RepeaterTimer t;
    t.x = x;
    t.y = y;
    t.z = z;
    t.kind = 2;
    t.timeLeft = kPistonTickDelay;
    mRepeaterTimers.push_back(t);
}

// Java BlockPistonBase.isIndirectlyPowered, which is a narrower question than the one a lamp asks: a
// piston does not listen to the cell it pushes into. Five of the six cells around a base can power it;
// the sixth holds the head, and anything in there is aimed at the head rather than at the base.
//
// That exclusion is not academic, because the one cell a piston *can* be built to push a source into is
// exactly this one -- a redstone block placed in front of a piston so the piston shoves it along. Left
// in the sum, that block is a source pointed straight at the base, and the machine it builds is a loop:
// the piston extends, shoves the block one cell further, loses the power, retracts, and on the sticky
// half drags the block back into the head cell, where the power is again. One cycle per redstone tick,
// for as long as the block sits there, and the piston never settles.
//
// The cell is hidden from the readers rather than skipped inside each of them, because this port's
// power readers answer per receiving cell and sum all six directions -- there is no per-source term to
// leave out. Nothing is lost by hiding it: a piston refuses to extend into a cell holding dust or any
// other block it cannot move (`PistonCanMoveBlock`), so the only cells that matter here are the movable
// ones, and the question being asked about them is whether they drive the piston or ride in front of it.
int CraftWorld::GetPistonPowerTo(int x, int y, int z)
{
    const int facing = GetPistonFacing(x,y,z);
    const int hx = x + PistonDirX(facing);
    const int hy = y + PistonDirY(facing);
    const int hz = z + PistonDirZ(facing);
    if(hx < 0 || hy < 0 || hz < 0 || hx >= worldLimit || hy >= WORLD_HEIGHT || hz >= worldLimit)
    {
        return GetRedstonePowerTo(x,y,z);
    }

    const block_t savedBlock = GetBlock(hx,hy,hz);
    const block_settings_t savedState = GetBlockSettingsNoCheck(hx,hy,hz);
    GetBlock(hx,hy,hz) = 0;
    ClearStateCellNibble(hx,hy,hz);
    const int power = GetRedstonePowerTo(x,y,z);
    GetBlock(hx,hy,hz) = savedBlock;
    GetBlockSettingsNoCheck(hx,hy,hz) = savedState;
    return power;
}

void CraftWorld::EvaluatePiston(int x, int y, int z)
{
    block_t b = GetBlock(x,y,z);
    if(IsPistonBaseBlock(b) == false)
    {
        return;
    }
    // The reader the lamp uses, not the conductor reader. A piston is activated either by a source
    // touching it or by a neighbouring *powered block*, and vanilla asks for both with one call.
    // GetBlockRedstonePower here instead would miss the commonest way to power a piston there is -- a
    // lever or a button on the block behind it -- because that answers only "is this cell powered".
    // It is not the reader as-is, though: the cell the head comes out of is taken out of it first.
    bool want = (GetPistonPowerTo(x,y,z) > 0);
    bool extended = IsPistonExtended(b);
    if(extended == want)
    {
        return;
    }
    if(want)
    {
        ExtendPiston(x,y,z);
    }
    else
    {
        RetractPiston(x,y,z);
    }
}

bool CraftWorld::PistonCanExtend(int x, int y, int z, int* outDistance)
{
    const int facing = GetPistonFacing(x,y,z);
    const int dx = PistonDirX(facing);
    const int dy = PistonDirY(facing);
    const int dz = PistonDirZ(facing);

    for(int n = 1; n <= kPistonPushLimit; n++)
    {
        const int cx = x + dx * n;
        const int cy = y + dy * n;
        const int cz = z + dz * n;
        if(cx < 0 || cy < 0 || cz < 0 || cx >= worldLimit || cy >= WORLD_HEIGHT || cz >= worldLimit)
        {
            return false;   // the world edge is not something to push into
        }
        block_t b = GetBlock(cx,cy,cz);
        if(b == 0)
        {
            *outDistance = n;
            return true;
        }
        if(PistonCanMoveBlock(b) == false)
        {
            return false;
        }
    }
    // Twelve cells and every one of them a block that could move if only there were room behind it.
    // There is not, so the piston stays shut -- this is the case Java answers by doing nothing at all,
    // which is also why a piston that cannot extend does not take the loss out on the block in front.
    return false;
}

bool CraftWorld::ExtendPiston(int x, int y, int z)
{
    block_t base = GetBlock(x,y,z);
    if(IsPistonBaseBlock(base) == false || IsPistonExtended(base))
    {
        return false;
    }
    const int facing = GetPistonFacing(x,y,z);
    const int dx = PistonDirX(facing);
    const int dy = PistonDirY(facing);
    const int dz = PistonDirZ(facing);

    int distance = 0;
    if(PistonCanExtend(x, y, z, &distance) == false)
    {
        return false;
    }
    const bool sticky = IsStickyPiston(base);

    // Whatever is standing in the way goes first, before a single block moves. Every step below this
    // point makes the line solid -- the shoved blocks land in their new cells, then the head takes cell 1
    // -- so a collision test run afterwards would be asking whether a mob can stand in a cell the piston
    // has just filled, and the answer would always be no. The cells a shove tests are the ones past the
    // far end of the line, which no part of the push touches either way.
    PistonPushEntities(x, y, z, facing, distance);

    // The far end first. Every block in the line moves one cell along d, and walking from the far end
    // back means each destination has already been stood down -- walking the other way would overwrite
    // the block that has not moved yet. The line runs 1..distance-1: the cell at `distance` is the air
    // the piston found, and it is where the last block ends up.
    for(int i = distance - 1; i >= 1; i--)
    {
        MoveBlockCell(x + dx*i, y + dy*i, z + dz*i, x + dx*(i+1), y + dy*(i+1), z + dz*(i+1));
    }

    // The head takes the cell in front of the base and the base becomes the extended id, both facing the
    // way they already faced. Nothing is lost by writing the id after comparing it: the facing is in the
    // nibble and SetPistonState puts it back.
    GetBlock(x + dx, y + dy, z + dz) = sticky ? StickyPistonHead::getID() : PistonHead::getID();
    SetPistonState(x + dx, y + dy, z + dz, facing);
    GetBlock(x, y, z) = sticky ? StickyPistonOn::getID() : PistonOn::getID();
    SetPistonState(x, y, z, facing);

    // Every cell of the line changed, and two of them changed *kind*: a retracted base is an opaque cube
    // and an extended one is a body with a rod standing out of it, so an extension moves a block id
    // between the opaque mesh and the transparent one. Every one of these cells ends up holding a block
    // -- the head takes cell 1, each shoved block takes the cell in front of it, and the far end takes
    // the last of them -- so they are all the "filled" shape and none of them is treated as vacated. A cell that is
    // only passed through empties and fills inside this same call, which is why the trapdoors and item
    // frames mounted on it must not be touched.
    for(int i = 0; i <= distance; i++)
    {
        PistonCellFilled(x + dx*i, y + dy*i, z + dz*i);
    }

    // The world moved, so everything around it has to re-read. Both ends, because the cell that was air
    // and the cell that emptied are the two most likely to have been carrying a wire or a repeater.
    UpdateRedstonePower(x, y, z);
    UpdateRedstonePower(x + dx*distance, y + dy*distance, z + dz*distance);

    // Java BlockPistonBase.onBlockEvent, which plays tile.piston.out for an extension. Played after the
    // world has settled so the sound belongs to the move that happened rather than to the one before it.
    SoundManager::Instance()->PlayPistonSound(true);

    // The arm's travel, on top of the world that has just moved: the base is already the extended id and
    // the head is already in the cell in front, so all that is left is to draw the pair further apart.
    StartPistonStroke(x, y, z, facing, true);
    return true;
}

bool CraftWorld::RetractPiston(int x, int y, int z)
{
    block_t base = GetBlock(x,y,z);
    if(IsPistonBaseBlock(base) == false || IsPistonExtended(base) == false)
    {
        return false;
    }
    const int facing = GetPistonFacing(x,y,z);
    const int dx = PistonDirX(facing);
    const int dy = PistonDirY(facing);
    const int dz = PistonDirZ(facing);
    const bool sticky = IsStickyPiston(base);

    const int hx = x + dx;
    const int hy = y + dy;
    const int hz = z + dz;
    const bool headInside = (hx >= 0 && hy >= 0 && hz >= 0 && hx < worldLimit && hy < WORLD_HEIGHT && hz < worldLimit);
    const bool hadHead = headInside && IsPistonHeadBlock(GetBlock(hx,hy,hz));
    if(hadHead)
    {
        GetBlock(hx,hy,hz) = 0;
        // Air now, so its nibble is light again rather than state -- and a leftover direction reads as a
        // light level of 0..5 in a cell that has no business being lit.
        ClearStateCellNibble(hx,hy,hz);
    }

    // The sticky half: one block, drawn back into the cell the head has just vacated. It is gated on
    // there having been a head, because a pull into a cell that still holds one would overwrite it, and
    // "there was no head" means the world is not the shape this function was promised.
    const int px = hx + dx;
    const int py = hy + dy;
    const int pz = hz + dz;
    const bool pullInside = (px >= 0 && py >= 0 && pz >= 0 && px < worldLimit && py < WORLD_HEIGHT && pz < worldLimit);
    bool pulled = false;
    if(sticky && hadHead && pullInside)
    {
        block_t pull = GetBlock(px,py,pz);
        // The same rule the push uses, so push and pull can never disagree about what is movable: air is
        // nothing to pull, and a bed or a chest is not something to drag into a piston cell.
        if(PistonCanMoveBlock(pull))
        {
            MoveBlockCell(px,py,pz, hx,hy,hz);
            pulled = true;
        }
    }

    GetBlock(x,y,z) = sticky ? StickyPistonOff::getID() : PistonOff::getID();
    SetPistonState(x,y,z,facing);

    // The base keeps a block the whole time -- it changes from an extended body back to a retracted cube
    // -- so nothing mounted on it comes loose and it is the "filled" shape.
    PistonCellFilled(x, y, z);
    if(hadHead)
    {
        // The head cell lost its head. If the sticky pull put a block back in it the cell is filled
        // again and only the support checks are owed; if it stayed air then everything mounted on the
        // head's faces comes off with it.
        if(pulled)
        {
            PistonCellFilled(hx, hy, hz);
        }
        else
        {
            PistonCellEmptied(hx, hy, hz);
        }
    }
    if(pulled)
    {
        // The cell the pulled block came out of is the one cell of a retraction that ends up empty.
        PistonCellEmptied(px, py, pz);
    }

    UpdateRedstonePower(x, y, z);
    if(hadHead)
    {
        UpdateRedstonePower(hx, hy, hz);
    }
    if(pulled)
    {
        UpdateRedstonePower(px, py, pz);
    }

    // Java BlockPistonBase.onBlockEvent plays tile.piston.in for the retraction, and it is the same
    // event for a sticky piston whether or not the pull found anything to drag back.
    SoundManager::Instance()->PlayPistonSound(false);

    // The arm's travel, on top of the world that has just moved. The head cell is empty from here on --
    // or holds the block a sticky piston pulled into it -- so the stroke is drawn by the base, which is
    // the cell both halves of the piston can be reached from.
    StartPistonStroke(x, y, z, facing, false);
    return true;
}

// -------------------------------------------------------------------------------------------------
// Piston stroke: the arm's travel between its two cells
// -------------------------------------------------------------------------------------------------
//
// Java keeps the block ids still while a piston moves -- the base, the head and the block in front are
// handed to a TileEntityPiston, which interpolates its own offset over two ticks -- and this port cannot
// do that: the ids move in one call, because everything that reads the world (collision, the redstone
// rules, the push rules) has to see the piston in its finished shape the moment it moves. The stroke is
// therefore built *on top of* the moved world rather than inside it, and the two places it shows are the
// two the entity model would have covered:
//
//   extending    the base is already the extended id, so it draws its own body, its recess floor and its
//                4 px rod stub, and the head -- which is already standing in the cell in front -- is
//                drawn by the *base* at the fraction of the way out the stroke has reached. The head
//                cell draws nothing.
//   retracting   the base is already the retracted cube and the head cell has already lost its block, so
//                the base draws the cube and the head travelling back into it. Again the head cell draws
//                nothing, and here it has nothing to draw anyway.
//
// Both cases are then one line of code: the base draws the head at `progress` of the way along the push,
// which is `progress * 16` pixels, and the head cell stays silent while the stroke lasts. The travel is
// a whole cell because the head is drawn from the *base* cell and 16 px is where the resting mesher
// draws it -- one cell along the push. At progress 0 the head's plate covers the base's recess and the
// pair is the retracted cube to the pixel -- the two the player would see if the animation were skipped
// entirely, which is why the stroke can start and end anywhere without a visible seam. At progress 1 the
// head is where the resting mesher puts it, again to the pixel.
//
// Two details make it look like an arm rather than like two boxes sliding:
//
//   * The rod stays rigid and travels with the plate, as vanilla's piston_head model does, and its
//     sprite columns are counted from the plate's back face so the arm's bands slide along with it.
//     Telescoping it instead -- anchored to the base's mouth, and so 12 px at home and 28 px out --
//     looks the same, because the length it gains is inside the base's body, but it reaches past the
//     end of the sprite's own 16 columns and puts a second set of flanks exactly on the base's rod
//     stub's for the middle of the travel. See emitHead for why one way of stating the arm's length
//     is worth more than the two.
//   * The faces that would land coplanar mid-slide are left out, and only for as long as a stroke lasts
//     -- see emitHead (the plate's back face, for the one pose where it is on the base's recess floor)
//     and the rod stub's front cap in CraftWorld_MeshBlocks.cpp.
//
// The cost is the reason this is a *frame* animation and not a per-step one. A pose is a whole chunk
// walk, and the engine's redstone queue takes one chunk a frame, so a stroke asks for the two cells it
// touches once per frame and is drawn in as many poses as the frame budget allows. It never runs ahead
// of the queue -- a frame that cannot afford a rebuild simply draws the previous pose for one more frame
// -- and it never falls behind either, because the pose drawn is the progress the clock is at when the
// rebuild happens, not the one it asked for.
static const float kPistonStrokeTime = 0.1f;   // Java's two ticks, both of them animated

void CraftWorld::StartPistonStroke(int x, int y, int z, int facing, bool extending)
{
    const int hx = x + PistonDirX(facing);
    const int hy = y + PistonDirY(facing);
    const int hz = z + PistonDirZ(facing);

    for(unsigned int i = 0; i < mPistonStrokes.size(); i++)
    {
        if(mPistonStrokes[i].x == x && mPistonStrokes[i].y == y && mPistonStrokes[i].z == z)
        {
            // A stroke that is somehow still running is restarted from the end the world has just moved
            // to rather than reversed: the ids have already changed, so the pose the last frame drew
            // belongs to the state before this one and finishing it would show the piston arriving back
            // where it came from.
            mPistonStrokes[i].facing = facing;
            mPistonStrokes[i].extending = extending ? 1 : 0;
            mPistonStrokes[i].progress = extending ? 0.0f : 1.0f;
            QueueRedstoneMesh(x, y, z, extending == false);
            QueueRedstoneMesh(hx, hy, hz, false);
            return;
        }
    }

    PistonStroke s;
    s.x = x;
    s.y = y;
    s.z = z;
    s.facing = facing;
    s.extending = extending ? 1 : 0;
    // 0 is the head home -- the plate covering the recess, which the world has just left behind -- and 1
    // is the head fully out. An extension therefore starts at 0 and a retraction at 1, and both travel
    // towards the other end.
    s.progress = extending ? 0.0f : 1.0f;
    mPistonStrokes.push_back(s);

    // The two cells ask for the pass the block id just moved between, and the head cell only for the
    // transparent one: it is a piston head (transparent) on the way out and air or a pulled block on the
    // way back, so its opaque half never held anything of the piston's. This is the same request
    // PistonCellFilled has already made for the same cells -- see its comment -- and QueueRedstoneMesh
    // keeps the strongest one, so the pair costs one chunk walk, not two.
    QueueRedstoneMesh(x, y, z, extending == false);
    QueueRedstoneMesh(hx, hy, hz, false);
}

void CraftWorld::UpdatePistonStrokes(float dt)
{
    for(unsigned int i = 0; i < mPistonStrokes.size(); )
    {
        PistonStroke& s = mPistonStrokes[i];
        s.progress += (s.extending ? 1.0f : -1.0f) * (dt / kPistonStrokeTime);

        const bool done = (s.extending ? (s.progress >= 1.0f) : (s.progress <= 0.0f));
        // Copied out before the erase: `s` points into the vector, and the queue call below is what the
        // rest of this needs the cell for.
        const int sx = s.x, sy = s.y, sz = s.z;
        const bool baseOpaque = (s.extending == 0);

        if(done)
        {
            // Back at rest. The ids never moved, so the resting pose *is* what the mesher builds for a
            // cell with no stroke on it, and dropping the entry together with one last request is the
            // whole of the finish -- no snapping, because the pose it stops on is the pose it ends at.
            mPistonStrokes.erase(mPistonStrokes.begin() + i);
        }
        else
        {
            i++;
        }

        // One request a frame for the base, and only ever for the base: the head cell's own geometry does
        // not depend on the progress -- it is a suppressed head on the way out and air or a pulled block
        // on the way back, and StartPistonStroke has already asked for its one rebuild -- while the
        // base's does, so asking for it again every frame is asking for exactly the work that has to be
        // redone. The queue holds one entry per chunk and the flush takes one chunk a frame, so a stroke
        // is drawn in as many poses as the budget allows and never runs ahead of it.
        QueueRedstoneMesh(sx, sy, sz, baseOpaque);
    }
}

bool CraftWorld::GetPistonStroke(int x, int y, int z, bool isHead, int facing, float* progress, bool* extending)
{
    if(mPistonStrokes.empty())
    {
        return false;
    }

    int bx = x, by = y, bz = z;
    if(isHead)
    {
        // A head is only ever the far half of the base behind it, and that base is the cell one step
        // back along the push -- which is where the stroke is filed, because it is the cell both halves
        // are drawn from.
        bx -= PistonDirX(facing);
        by -= PistonDirY(facing);
        bz -= PistonDirZ(facing);
        // A head whose base is gone is standing in the world on its own -- the break path takes the two
        // halves together, but a save can be damaged -- and then it has to draw itself.
        if(bx < 0 || by < 0 || bz < 0 || bx >= worldLimit || by >= WORLD_HEIGHT || bz >= worldLimit
           || IsPistonBaseBlock(GetBlock(bx,by,bz)) == false)
        {
            return false;
        }
    }

    for(unsigned int i = 0; i < mPistonStrokes.size(); i++)
    {
        if(mPistonStrokes[i].x == bx && mPistonStrokes[i].y == by && mPistonStrokes[i].z == bz
           && mPistonStrokes[i].facing == facing)
        {
            *progress = mPistonStrokes[i].progress;
            *extending = (mPistonStrokes[i].extending != 0);
            return true;
        }
    }
    return false;
}

void CraftWorld::ClearPistonStrokes()
{
    mPistonStrokes.clear();
}

void CraftWorld::MoveBlockCell(int fromX, int fromY, int fromZ, int toX, int toY, int toZ)
{
    const block_t b = GetBlock(fromX,fromY,fromZ);
    const block_settings_t state = GetBlockSettingsNoCheck(fromX,fromY,fromZ);

    GetBlock(fromX,fromY,fromZ) = 0;
    // Only ever clear a nibble that *was* state. For every other block the nibble is the cell's own light
    // level -- a value the light engine put there for the cell, not a property of the block -- and it is
    // still the right value for a cell that has just become air.
    if(IsStateCellBlock(b))
    {
        ClearStateCellNibble(fromX,fromY,fromZ);
    }

    GetBlock(toX,toY,toZ) = b;
    // Only state travels with a block. Carrying a light level from one cell into another would put a
    // value where the light engine never measured one, and nothing needs it: dust is the one block where
    // it is tempting to disagree, but dust power is not stored at all -- GetRedstoneDustPower derives it
    // from the network on every read. A cell that arrives holding a state nibble is a state cell from
    // then on, and IsStateCellBlock makes every light reader skip it.
    if(IsStateCellBlock(b))
    {
        GetBlockSettings(toX,toY,toZ) = (block_settings_t)(state & OpRedstonePowerMask);
    }

    // A light source has to take its light with it: the only record of where it is lives in the light
    // values it wrote when it was placed. This is the same remove-then-set pair the lava flow uses to
    // move a light-emitting fluid cell.
    if(LightSourceBlock(b))
    {
        RemoveLigtSourceAtPosition(fromX,fromY,fromZ, b);
        SetLigtSourcePosition(toX,toY,toZ, b);
    }
}

void CraftWorld::PistonCellEmptied(int x, int y, int z)
{
    // The same short list the break path runs at a cell it has emptied, for the same reasons: a torch, a
    // ladder, a lever, a button, a trapdoor and an item frame are each held up by their neighbours, and
    // each of the four sweeps below covers a 3x3 around this cell, so this is per changed cell and not
    // per neighbour.
    CheckForTorchSupport(x, y, z, 0);
    CheckForLadderSupport(x, y, z);
    DestroyAroundTrapdoors(x, y, z);
    DestroyAroundItemFrames(x, y, z);
    // A cell whose section was buried has no mesh to rebuild, so waking it is not optional: the break
    // path does exactly this pair, and a piston that moved the last block covering a cave would
    // otherwise leave the cave dark until something else disturbed it.
    //
    // Queued on the redstone queue rather than `AddChunkToUpdate`, and it matters for a clock: this runs
    // per *cell* a piston moves, so one piston stroke is two to four requests, and a piston clock is a
    // stroke every few ticks. `toUpdate` drains one chunk per 0.18 s -- a producer far faster than that
    // consumer fills the queue, and AddChunkToUpdate's runaway guard then clears it, which is how a
    // running piston clock ends up with geometry that never catches up. The redstone queue drains on the
    // frame budget instead. See QueueRedstoneMesh.
    WakeBuriedAround(x, y, z);
    QueueRedstoneMesh(x, y, z, true);
}

// A cell a piston has just filled. Only the two sweeps that *check* support run here, and they run for a
// reason: the block that arrived may be one nothing can be mounted on -- a piston head is not solid, so
// a torch nailed to the side of the cell it moved into has to come off. DestroyAroundTrapdoors and
// DestroyAroundItemFrames are deliberately *not* in this one: they take the cell they are given to be
// air and delete the trapdoors and frames hanging on its faces without checking, so a cell that just
// gained a block would silently destroy the things still properly mounted on it.
//
// The mesh is the other half of this, and it is not optional either: a retracted base and an extended
// one are an opaque cube and a body with a rod out of it, so a piston moves a block id *between* the opaque
// mesh and the transparent one. That is what the `true` asks for -- unlike a repeater flip, which only
// ever changes one mesh. The queue is the redstone one, for the reason spelled out at PistonCellEmptied.
void CraftWorld::PistonCellFilled(int x, int y, int z)
{
    CheckForTorchSupport(x, y, z, 0);
    CheckForLadderSupport(x, y, z);
    WakeBuriedAround(x, y, z);
    QueueRedstoneMesh(x, y, z, true);
}

void CraftWorld::PistonPushEntities(int x, int y, int z, int facing, int distance)
{
    // Drops, arrows and primed TNT carry nothing but a position and are shoved the same way. An arrow
    // also keeps the position it was at before its last step -- that is the segment its own collision
    // sweep reads next frame -- so a shoved arrow has to move both or it would sweep a phantom cell.
    for(unsigned int i = 0; i < mDrops.size(); i++)
    {
        if(mDrops[i] == NULL)
        {
            continue;
        }
        int k = PistonLineOffset(x, y, z, facing,
                                 (int)floorf(mDrops[i]->position.x), (int)floorf(mDrops[i]->position.y), (int)floorf(mDrops[i]->position.z));
        if(k < 1 || k > distance)
        {
            continue;
        }
        float ox, oy, oz;
        PistonShoveOffset(facing, k, distance, &ox, &oy, &oz);
        mDrops[i]->position.x += ox;
        mDrops[i]->position.y += oy;
        mDrops[i]->position.z += oz;
    }

    for(unsigned int i = 0; i < mArrows.size(); i++)
    {
        if(mArrows[i] == NULL)
        {
            continue;
        }
        int k = PistonLineOffset(x, y, z, facing,
                                 (int)floorf(mArrows[i]->position.x), (int)floorf(mArrows[i]->position.y), (int)floorf(mArrows[i]->position.z));
        if(k < 1 || k > distance)
        {
            continue;
        }
        float ox, oy, oz;
        PistonShoveOffset(facing, k, distance, &ox, &oy, &oz);
        mArrows[i]->position.x += ox;
        mArrows[i]->position.y += oy;
        mArrows[i]->position.z += oz;
        mArrows[i]->prevPosition.x += ox;
        mArrows[i]->prevPosition.y += oy;
        mArrows[i]->prevPosition.z += oz;
    }

    for(unsigned int i = 0; i < mTNTs.size(); i++)
    {
        if(mTNTs[i] == NULL)
        {
            continue;
        }
        int k = PistonLineOffset(x, y, z, facing,
                                 (int)floorf(mTNTs[i]->position.x), (int)floorf(mTNTs[i]->position.y), (int)floorf(mTNTs[i]->position.z));
        if(k < 1 || k > distance)
        {
            continue;
        }
        float ox, oy, oz;
        PistonShoveOffset(facing, k, distance, &ox, &oy, &oz);
        mTNTs[i]->position.x += ox;
        mTNTs[i]->position.y += oy;
        mTNTs[i]->position.z += oz;
    }

    PistonShoveMobs(mZombies,   this, x, y, z, facing, distance);
    PistonShoveMobs(mCows,      this, x, y, z, facing, distance);
    PistonShoveMobs(mCreepers,  this, x, y, z, facing, distance);
    PistonShoveMobs(mPigs,      this, x, y, z, facing, distance);
    PistonShoveMobs(mSheeps,    this, x, y, z, facing, distance);
    PistonShoveMobs(mChickens,  this, x, y, z, facing, distance);
    PistonShoveMobs(mSpiders,   this, x, y, z, facing, distance);
    PistonShoveMobs(mSkeletons, this, x, y, z, facing, distance);
}

void CraftWorld::BreakPistonPartner(int x, int y, int z)
{
    block_t b = GetBlock(x,y,z);
    if(IsPistonHeadBlock(b))
    {
        // The head is what the base extends; with the head gone the base has to go back to the state it
        // would have returned to anyway, and it keeps its facing. No item either way: the head drops
        // nothing (InitPistonHead) and the retracted base is not being broken.
        const int facing = GetPistonFacing(x,y,z);
        const int bx = x - PistonDirX(facing);
        const int by = y - PistonDirY(facing);
        const int bz = z - PistonDirZ(facing);
        if(bx < 0 || by < 0 || bz < 0 || bx >= worldLimit || by >= WORLD_HEIGHT || bz >= worldLimit)
        {
            return;
        }
        block_t behind = GetBlock(bx,by,bz);
        if(IsPistonBaseBlock(behind) && IsPistonExtended(behind))
        {
            GetBlock(bx,by,bz) = IsStickyPiston(behind) ? StickyPistonOff::getID() : PistonOff::getID();
            SetPistonState(bx,by,bz,facing);
            // The head being broken is the cell the break path is already sweeping; the base behind it is
            // this function's own change, and it goes from an extended plate back to a full cube, so it
            // keeps a block and only needs the filled shape here.
            PistonCellFilled(bx,by,bz);
            UpdateRedstonePower(bx,by,bz);
        }
        return;
    }

    if(IsPistonExtended(b))
    {
        // Breaking an extended base takes its head with it. Java drops nothing for an extension, which is
        // what stops a player from picking up two pistons for one.
        const int facing = GetPistonFacing(x,y,z);
        const int hx = x + PistonDirX(facing);
        const int hy = y + PistonDirY(facing);
        const int hz = z + PistonDirZ(facing);
        if(hx < 0 || hy < 0 || hz < 0 || hx >= worldLimit || hy >= WORLD_HEIGHT || hz >= worldLimit)
        {
            return;
        }
        if(IsPistonHeadBlock(GetBlock(hx,hy,hz)))
        {
            GetBlock(hx,hy,hz) = 0;
            ClearStateCellNibble(hx,hy,hz);
            // The head is a block away from the one being broken, so nothing else is sweeping this cell.
            // A head that vanishes leaves air behind, and that is the emptied shape.
            PistonCellEmptied(hx,hy,hz);
            UpdateRedstonePower(hx,hy,hz);
        }
    }
}

int CraftWorld::ButtonFacingFromState(int state)
{
    return state & 7;
}

bool CraftWorld::ButtonPressedFromState(int state)
{
    return (state & 8) != 0;
}

int CraftWorld::GetButtonState(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }
    if(IsSwitchBlock(GetBlock(x,y,z)) == false)
    {
        return 0;
    }
    return GetBlockSettingsNoCheck(x,y,z) & OpRedstonePowerMask;
}

void CraftWorld::SetButtonState(int x, int y, int z, int state)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return;
    }
    if(IsSwitchBlock(GetBlock(x,y,z)) == false)
    {
        return;
    }
    block_settings_t& s = GetBlockSettings(x,y,z);
    s = (block_settings_t)((s & ~OpRedstonePowerMask) | (state & OpRedstonePowerMask));
}

// Java BlockButton.setBlockBoundsBasedOnState, remapped to the centred space the pick ray and the
// outline work in. A button is a 6x4 px faceplate with a 2 px arm out of the block it is mounted on;
// pressing sinks it one pixel back into that block. The 6 px cross-section is on the two axes across
// the mounting face, and the arm runs along the third.
BoundingBox CraftWorld::ButtonCellBox(int state)
{
    const float w = 3.0f / 16.0f;   // half of the 6 px cross-section
    const float h = 2.0f / 16.0f;   // half of the 4 px cross-section
    const float d = 2.0f / 16.0f;   // arm sticking out of the mounting face
    const float sink = ButtonPressedFromState(state) ? (1.0f / 16.0f) : 0.0f;

    float x0 = -w, x1 = w;
    float y0 = -h, y1 = h;
    float z0 = -w, z1 = w;

    switch(ButtonFacingFromState(state))
    {
        case 0: y0 = -0.5f;  y1 = -0.5f + d - sink;  break; // mounted on the block below
        case 1: y0 =  0.5f - d + sink;  y1 = 0.5f;   break; // mounted on the block above
        case 2: x0 = -0.5f;  x1 = -0.5f + d - sink;  break; // mounted on the -X block
        case 3: x0 =  0.5f - d + sink;  x1 = 0.5f;   break; // mounted on the +X block
        case 4: z0 = -0.5f;  z1 = -0.5f + d - sink;  break; // mounted on the -Z block
        case 5: z0 =  0.5f - d + sink;  z1 = 0.5f;   break; // mounted on the +Z block
    }
    return BoundingBox(Vector3(x0,y0,z0),Vector3(x1,y1,z1));
}

// Java BlockLever.setBlockBoundsBasedOnState, in the same centred space as ButtonCellBox: an 8x8 px
// footprint six pixels deep out of the mounting face. It is deliberately *taller* than the 6x6x3 px base
// the mesher draws, because it also has to enclose the leaning handle -- vanilla's own box does the same.
// These bounds are what the collide box, the pick ray and the selection outline all read.
BoundingBox CraftWorld::LeverCellBox(int state)
{
    const float w = 4.0f / 16.0f;   // half of the 8 px footprint
    const float d = 6.0f / 16.0f;   // depth out of the mounting face

    float x0 = -w, x1 = w;
    float y0 = -w, y1 = w;
    float z0 = -w, z1 = w;

    switch(ButtonFacingFromState(state))
    {
        case 0: y0 = -0.5f;  y1 = -0.5f + d;  break; // mounted on the block below
        case 1: y0 =  0.5f - d;  y1 = 0.5f;   break; // mounted on the block above
        case 2: x0 = -0.5f;  x1 = -0.5f + d;  break; // mounted on the -X block
        case 3: x0 =  0.5f - d;  x1 = 0.5f;   break; // mounted on the +X block
        case 4: z0 = -0.5f;  z1 = -0.5f + d;  break; // mounted on the -Z block
        case 5: z0 =  0.5f - d;  z1 = 0.5f;   break; // mounted on the +Z block
    }
    return BoundingBox(Vector3(x0,y0,z0),Vector3(x1,y1,z1));
}

// Java BlockTorch.setBlockBoundsBasedOnState, in the same centred space as ButtonCellBox. A torch is
// *not* the same shape on every mounting: the standing box is 0.2 x 0.6 x 0.2, a narrow column rising
// off the floor of the cell, while a wall box is 0.3 x 0.6 x 0.3 hanging at mid-height against the face
// it is attached to. Which of the six it is comes free from the block id, because the wall ids already
// encode the face.
//
// Both boxes are wider than the torch the mesher draws -- the artwork's shaft is 2 px, and the standing
// box is 3.2 px -- which is vanilla's own generosity and the reason a torch is easier to aim at than it
// looks. This is the whole of the "torch hitboxes are hardcoded to the top face" bug: every variant
// used to share one `collideBox`, the floor torch's, so a wall torch's pick shape sat at the bottom of
// the cell in the centre, nowhere near the torch drawn against the wall. The crosshair therefore had to
// be dragged to the *floor* of the cell to select a wall torch at all, and the box it did report was
// 0.12 of a block across against the 0.3 a wall torch gets below. Aiming that finely is what made
// torches feel unhittable with a snappy camera.
//
// `face` is 0 for a floor torch and 1..4 for a wall torch, matching TorchFaceFromBlock and the mount
// the id names: 1 hangs on the -X block, 2 on +X, 3 on -Z, 4 on +Z.
BoundingBox CraftWorld::TorchCellBox(int face)
{
    switch(face)
    {
        // Java's wall bounds are (0.0, 0.2, 0.35) .. (0.3, 0.8, 0.65), plus the usual mirror per face.
        // Subtracting 0.5 per axis is what puts them in cell space.
        case 1: return BoundingBox(Vector3(-0.5f, -0.3f, -0.15f), Vector3(-0.2f, 0.3f, 0.15f));
        case 2: return BoundingBox(Vector3( 0.2f, -0.3f, -0.15f), Vector3( 0.5f, 0.3f, 0.15f));
        case 3: return BoundingBox(Vector3(-0.15f, -0.3f, -0.5f), Vector3(0.15f, 0.3f, -0.2f));
        case 4: return BoundingBox(Vector3(-0.15f, -0.3f,  0.2f), Vector3(0.15f, 0.3f,  0.5f));
        // Standing: Java's (0.4, 0.0, 0.4) .. (0.6, 0.6, 0.6).
        default: return BoundingBox(Vector3(-0.1f, -0.5f, -0.1f), Vector3(0.1f, 0.1f, 0.1f));
    }
}

BoundingBox CraftWorld::GetBlockCollideBox(int x, int y, int z, block_t b)
{
    if(IsTorchBlock(b))
    {
        return TorchCellBox(TorchFaceFromBlock(b));
    }
    if(IsButtonBlock(b))
    {
        return ButtonCellBox(GetButtonState(x,y,z));
    }
    if(IsLever(b))
    {
        return LeverCellBox(GetButtonState(x,y,z));
    }
    // A piston base is the one other block whose collision shape is not just per-type: an extended base
    // keeps a 12 px body against the wall its head left behind, so the shape depends on the direction in
    // the cell's nibble. A piston head is the whole cell in all six directions (Java gives
    // BlockPistonExtension the default bounds), so it needs nothing here.
    if(IsPistonBaseBlock(b))
    {
        return PistonCellBox(GetPistonFacing(x,y,z), IsPistonExtended(b));
    }
    // Guard the lookup: a save can name an id the current block table does not have.
    if(b < blockTypes.size())
    {
        return blockTypes[b].collideBox;
    }
    return blockTypes[0].collideBox;
}

// Java keeps two shapes per block: the collision shape the player walks into, and the selection shape
// the crosshair picks. They are the same for every block in the game except redstone wire, whose
// selection shape is the same 1 px floor plate Java draws -- *not* a cube, which is the part that is
// easy to get backwards. Because the plate is thin, vanilla's ray trace passes over a wire when the
// look is aimed past it and reports the wire only when the look actually lands on the plate. That is
// what lets a player build a wire line outward: each placement is aimed at the *next* floor cell, and
// the wire already lying in front of their feet does not intercept the look.
//
// This build answered the wire with a full cube instead. The reading at the time was that a 2 px plate
// was too thin to aim at, and it is true that a level look at the old plate sailed over it and anchored
// on the block below -- but the fix was on the wrong side of the seam, and it cost the other half of
// vanilla's behaviour. A cube in the cell directly in front of the player's feet eats the aim across a
// wide cone of look angles, so the cell the player was actually looking at -- one further along the
// line they were building -- was never the one reported, and a line could only be extended by taking a
// step forward between placements. The plate is also the shape the mesher draws, so with it the
// crosshair matches the art.
//
// Building *on* a wire does not need the cube: the placement site snaps a wire target up one cell,
// because a floor plate has no face a player would mean to build against -- see the comment at that
// call. That snap is what the cube was really standing in for, and it is still there.
//
// The plate is only half of the fix. The pick that reads this shape used to march the look in 0.075
// steps and ask whether the stepped point landed *inside* the box. That is exact for a cube -- every
// step inside the cell is inside the box -- and a coin toss on a 1 px plate, because a look steep
// enough to cross 0.0625 of height in less than one step can walk clean over the plate and report
// whatever is behind it. All seven pick loops (three in each play state: the aim/outline, the break
// and the placement, plus the creative punch) now clip the ray against the box with
// `Ray::hasIntersected(box, distance)`, the same call the placement face normal already used, which
// cannot pass a box it crosses at any angle. `tools/check-wire-aim.py` marches the pitch band both
// ways and prints which angles the stepped march misses.
//
// This function stays as a named seam rather than being deleted, because the one thing a future reader
// is likely to want to do here -- give some block a pick shape that differs from its walk shape -- is
// exactly what it is for. It now has its first user, and the reason it needs one is worth recording:
// **the plate was described here and never actually written.** The paragraph above has read as if the
// crosshair already answered a wire with the plate, while both this function and the block type went on
// giving it `BaseBlock`'s default cube, so the bug the paragraph describes was still live and the reader
// had no reason to look for it. Nothing in the tree could see the difference: a cube is a legal answer
// for every block, so a missing shape has no smell. The plate is stated below, in the one place that
// reads as a shape for the crosshair rather than for the world, and `tools/check-wire-aim.py` executes
// this function's own numbers against the mesher's plate height so the two cannot drift apart again.
BoundingBox CraftWorld::GetBlockSelectionBox(int x, int y, int z, block_t b)
{
    // The one block whose pick shape is not its walk shape. Java's own bounds for it are
    // `setBlockBounds(0, 0, 0, 1, 0.0625, 1)` -- 1 px off the floor of the cell, full width -- which in
    // this engine's centred space is -0.5 at the floor and -0.4375 at the top of the plate, and it is the
    // same 1 px the mesher draws the wire strips at (`emitWireStrip` in CraftWorld_MeshBlocks.cpp puts
    // them at y = 1 pixel), so the crosshair and the art agree.
    //
    // It is answered here rather than by writing `collideBox` on the block type because that field is the
    // *collision* shape, which every walk, shove and mob test in the port reads, and a wire is not solid:
    // only one of the two shapes is wrong, so only one of them moves.
    if(b == Redstoneoff::getID())
    {
        return BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,-0.4375f,0.5f));
    }
    return GetBlockCollideBox(x,y,z,b);
}

// The step a pick loop takes: the distance from `t` to the far wall of the cell the look is in, plus a
// hair so that the sample taken at the returned distance is inside the *next* cell. One iteration per
// cell the ray crosses, which is what makes a pick exact -- a fixed step visits cells on a grid, and can
// miss a whole cell whose crossing is shorter than the step. The header says why that matters (a 1 px
// selection plate), and tools/check-wire-aim.py walks the geometry every way and counts the misses.
float CraftWorld::NextCellDistance(const Vector3 &origin, const Vector3 &dir, float t)
{
    // The hair is there for float error *at* the wall: floorf() of a sample sitting exactly on a boundary
    // rounds by luck, and half a thousandth of a block is far too short to hide a cell a look really
    // crosses. It also has to be non-zero, or a step that lands exactly on a wall would not advance.
    const float hair = 0.0005f;

    // The far wall of the cell the look is in, per axis, and the wall beyond it.
    float axisO[3];
    float axisWall[3];
    float axisPast[3];
    bool axisLive[3];
    bool axisHasWall[3];

    float first = -1.0f;
    int firstAxis = -1;
    for(int axis = 0; axis < 3; ++axis)
    {
        const float d = (axis == 0) ? dir.x : ((axis == 1) ? dir.y : dir.z);
        const float o = (axis == 0) ? origin.x : ((axis == 1) ? origin.y : origin.z);
        axisO[axis] = o;
        axisWall[axis] = 0.0f;
        axisPast[axis] = 0.0f;
        axisLive[axis] = (d < -1E-06f || d > 1E-06f);
        axisHasWall[axis] = false;
        if(!axisLive[axis])
        {
            continue;   // parallel to this pair of walls, so it meets neither of them
        }
        // The cell the look is in *at* t, because that is the cell the caller has just clipped. Taking
        // the cell from a sample at t + hair instead reads the far wall of the next cell along whenever
        // the look crossed a boundary inside that hair -- the caller then steps over that cell and never
        // clips its box. That is the same corner graze this walk exists to stop, at the hair's scale
        // rather than a stride's, and it is a 1 px plate's worth of aim to lose.
        const float cell = floorf(o + d * t);
        // The wall the look is heading for: the far side of that cell in the direction of travel.
        const float wall = (d > 0.0f) ? (cell + 1.0f) : cell;
        const float at = (wall - o) / d;
        if(at > t)
        {
            axisWall[axis] = wall;
            axisPast[axis] = (d > 0.0f) ? (wall + 1.0f) : (wall - 1.0f);
            axisHasWall[axis] = true;
            if(first < 0.0f || at < first)
            {
                first = at;
                firstAxis = axis;
            }
        }
    }
    if(first < 0.0f)
    {
        return t + 1.0f;   // a unit vector always meets a wall; a caller must still not spin if one does not
    }

    // Where the look leaves the cell this step is entering: past the wall it crosses. On the first axis
    // that is the wall after it; on the others it is the wall this step already crosses, which the look
    // meets at the same time or later.
    float second = -1.0f;
    for(int axis = 0; axis < 3; ++axis)
    {
        if(!axisHasWall[axis])
        {
            continue;
        }
        const float d = (axis == 0) ? dir.x : ((axis == 1) ? dir.y : dir.z);
        const float wall = (axis == firstAxis) ? axisPast[axis] : axisWall[axis];
        const float at = (wall - axisO[axis]) / d;
        if(at > first && (second < 0.0f || at < second))
        {
            second = at;
        }
    }
    if(second > 0.0f && second < first + hair)
    {
        // A look can cross a cell in less than the hair -- it clips a corner of it, or crosses a wall
        // and the next one at almost the same distance. The sample has to land *in* that cell, not past
        // it, or the loop never clips its box; the middle of the cell is the one distance that is in it
        // by construction. The cell is that thin for one look in thousands -- it is a shared edge of two
        // blocks, seen exactly -- but "the pick is the geometry" has to hold for the one look too.
        return (first + second) * 0.5f;
    }
    return first + hair;
}

void CraftWorld::ButtonSupportPos(int x, int y, int z, int& sx, int& sy, int& sz)
{
    int f = ButtonFacingFromState(GetButtonState(x,y,z));
    sx = x + kButtonDirX[f];
    sy = y + kButtonDirY[f];
    sz = z + kButtonDirZ[f];
}

// Java BlockButton: while it is out, a button powers everything touching it. isProvidingWeakPower
// returns 15 on every side, and isProvidingStrongPower adds 15 on the face it is mounted on -- but the
// mounting block is itself one of the six neighbours, so "adjacent to a pressed button" already covers
// the strong case and the two collapse into a single rule. The old code implemented only the strong
// half: it matched the button's facing against the direction it was queried from, so a button only ever
// powered the block it was mounted on. That is why a button pressed flat would not power a wire running
// into it on its own level -- that wire is a neighbour of the button, not of its support block.
//
// A lever answers the same question with the same numbers (Java BlockLever.isProvidingWeakPower returns
// 15 on every side while it is on), so this one function serves both: a switch is a switch, and the only
// difference is what throws it back off.
int CraftWorld::GetButtonPowerAt(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }
    for(int f = 0; f < 6; f++)
    {
        int bx = x + kButtonDirX[f];
        int by = y + kButtonDirY[f];
        int bz = z + kButtonDirZ[f];
        if(bx < 0 || by < 0 || bz < 0 || bx >= worldLimit || by >= WORLD_HEIGHT || bz >= worldLimit)
        {
            continue;
        }
        if(IsSwitchBlock(GetBlock(bx,by,bz)) == false)
        {
            continue;
        }
        if(ButtonPressedFromState(GetButtonState(bx,by,bz)))
        {
            return 15;
        }
    }
    return 0;
}

void CraftWorld::PressButton(int x, int y, int z)
{
    if(IsButtonBlock(GetBlock(x,y,z)) == false)
    {
        return;
    }
    int st = GetButtonState(x,y,z);
    if(ButtonPressedFromState(st))
    {
        return; // Java: pressing one that is already out does nothing
    }
    SetButtonState(x,y,z, st | 8);

    // Java BlockButton.onBlockActivated: "random.click" at 0.6 pitch on stone, 0.5 on wood.
    SoundManager::Instance()->PlayButtonSound(GetBlock(x,y,z) == ButtonWood::getID());

    PressedButton pb;
    pb.x = x;
    pb.y = y;
    pb.z = z;
    // Java holds a stone button out for 20 ticks and a wooden one for 30.
    pb.timeLeft = (GetBlock(x,y,z) == ButtonWood::getID()) ? 1.5f : 1.0f;
    mPressedButtons.push_back(pb);

    // Its support block is now powered: refresh the wire network around the button, then light any TNT
    // touching the powered block, which is the plain cannon trigger.
    UpdateRedstonePower(x,y,z);
    int sx, sy, sz;
    ButtonSupportPos(x,y,z, sx, sy, sz);
    for(int f = 0; f < 6; f++)
    {
        if(TryIgniteTNT(sx + kButtonDirX[f], sy + kButtonDirY[f], sz + kButtonDirZ[f]))
        {
            break;
        }
    }

    int cid = getChunkId(Vector3((float)x,(float)y,(float)z));
    if(cid != -1)
    {
        RebuildTransparentMeshChunk(cid);
        rebuildNearestChunks(cid, Vector3((float)x,(float)y,(float)z));
    }
}

void CraftWorld::ReleaseButton(int x, int y, int z)
{
    if(IsButtonBlock(GetBlock(x,y,z)) == false)
    {
        return;
    }
    int st = GetButtonState(x,y,z);
    if(ButtonPressedFromState(st) == false)
    {
        return;
    }
    SetButtonState(x,y,z, st & ~8);
    UpdateRedstonePower(x,y,z);

    int cid = getChunkId(Vector3((float)x,(float)y,(float)z));
    if(cid != -1)
    {
        RebuildTransparentMeshChunk(cid);
        rebuildNearestChunks(cid, Vector3((float)x,(float)y,(float)z));
    }
}

// Java BlockLever.onBlockActivated: flip the latch. Nothing else about a lever needs code of its own --
// the state nibble it writes is the same one a button writes, so the wire beside it, the torch watching
// its cell, a lamp it is powering and a charge of TNT all read the change through the paths that already
// exist, and the only work left is to tell them the world moved. That is exactly what ReleaseButton does
// for a button, so this is that function with the latch write in place of the timer pop-out.
void CraftWorld::UseLever(int x, int y, int z)
{
    if(IsLever(GetBlock(x,y,z)) == false)
    {
        return;
    }
    int st = GetButtonState(x,y,z);
    // The click. Java plays "random.click" at 0.5 pitch for a lever, which is the wooden button's pitch,
    // so the same helper covers it.
    SoundManager::Instance()->PlayButtonSound(true);

    SetButtonState(x,y,z, st ^ 8);

    // Reseed the wire around the lever and light any TNT resting on the block it is mounted on, which is
    // the plain cannon trigger. Note that unlike a button there is no press timer and no entry in
    // mPressedButtons: a lever that is left on stays on until it is used again.
    UpdateRedstonePower(x,y,z);
    if(ButtonPressedFromState(st ^ 8))
    {
        int sx, sy, sz;
        ButtonSupportPos(x,y,z, sx, sy, sz);
        for(int f = 0; f < 6; f++)
        {
            if(TryIgniteTNT(sx + kButtonDirX[f], sy + kButtonDirY[f], sz + kButtonDirZ[f]))
            {
                break;
            }
        }
    }

    // A lever is transparent, so its own geometry lives in the transparent mesh.
    int cid = getChunkId(Vector3((float)x,(float)y,(float)z));
    if(cid != -1)
    {
        RebuildTransparentMeshChunk(cid);
        rebuildNearestChunks(cid, Vector3((float)x,(float)y,(float)z));
    }
}

// --- Redstone lamp (Java BlockRedstoneLight) -----------------------------------------------------
//
// The lamp is the first component here that reacts to power without ever being *asked* something. A
// button is used, a repeater is told, a torch is armed -- the lamp just sits there and has to notice.
// That is why it needs code of its own at all rather than a few id tests folded into an existing path.
//
// The hook is the redstone event window. `UpdateRedstonePower` already walks the 5x5x5 around any change
// and hands repeaters and torches their "the world moved, re-decide" call; a lamp inside that window gets
// the same treatment, which turns out to cover every case exactly:
//
//   * the lamp is powered by a dust cell on top of it, a dust cell pointing into it, a block of redstone,
//     a switch or a repeater facing it -- and all of those are within one cell of the lamp;
//   * when any of them changes, the change happens at a cell within one of the lamp's own neighbours, so
//     the lamp is inside the 5x5x5 window centred on it.
//
// So no lamp ever has to poll, and none of the reads below can recurse: `GetBlockRedstonePower` answers
// for the lamp's *neighbours* and never asks a neighbour what it thinks of the lamp.
static bool IsLampBlock(block_t b)
{
    return b == RedstoneLampOff::getID() || b == RedstoneLampOn::getID();
}

bool CraftWorld::IsRedstoneLampBlock(block_t b)
{
    return IsLampBlock(b);
}

// Java BlockRedstoneLight.func_149749_l: the lamp is lit while the *cell it occupies* is indirectly
// powered. `GetBlockRedstonePower` is this codebase's answer to that question -- it is the same union
// that lights TNT and drives every other consumer here (dust on top, dust pointing in, an adjacent block
// of redstone, an adjacent switch, a repeater facing it, a lit torch beside it), so a lamp lights under
// exactly the wiring a charge of TNT on its cell would fire under. That is the property worth having:
// one rule for "this cell is powered", and the lamp reads it rather than inventing a second one.
void CraftWorld::EvaluateRedstoneLamp(int x, int y, int z)
{
    block_t b = GetBlock(x,y,z);
    if(IsLampBlock(b) == false)
    {
        return;
    }
    bool lit = (b == RedstoneLampOn::getID());
    // Ask the *mechanism* question, not the block question. A lamp lights when it is activated, and a
    // lamp is activated either by a source touching it or by a neighbouring powered block -- which is
    // the second half of "a lever on the middle of three lamps lights all three": the middle lamp is
    // powered by the lever, and the outer two are activated by that powered middle lamp. Asking
    // GetBlockRedstonePower() here instead is what used to leave the outer two dark, because it answers
    // "is this lamp's own cell a powered block?", and for the outer lamps the answer is no -- they are
    // lit, not powered, which is exactly why the light does not spread any further along the row.
    bool want = (GetRedstonePowerTo(x,y,z) > 0);
    if(lit == want)
    {
        return;
    }
    SetRedstoneLampLit(x,y,z, want);
}

// Swap the lamp's id and carry the consequences. This is the same shape as SetRedstoneTorchLit, and for
// the same reason: the lit id is a light source and the dark one is not, so the light has to be pulled
// out of the neighbourhood *before* the area is relit, because the relight pass only ever adds. The one
// difference is the mesh. A torch is transparent and a lamp is a full opaque cube, so a lamp flip has to
// rebuild the opaque mesh -- which is why this is not just the torch function reused.
void CraftWorld::SetRedstoneLampLit(int x, int y, int z, bool lit)
{
    block_t now = GetBlock(x,y,z);
    if(IsLampBlock(now) == false || (now == RedstoneLampOn::getID()) == lit)
    {
        return;
    }
    GetBlock(x,y,z) = lit ? RedstoneLampOn::getID() : RedstoneLampOff::getID();

    if(lit == false)
    {
        RemoveLigtSourceAtPosition(x,y,z, Torch::getID());
    }
    QueueLightAreaIn(Vector3((float)x,(float)y,(float)z));

    // The lamp's own cell changed, and everything it neighbours can now disagree with what it read a
    // moment ago (a lamp is conductive, so a powered one feeds the wire the same way any powered block
    // does -- through `GetBlockRedstonePower`, which reads this cell).
    UpdateRedstonePower(x,y,z);

    int cid = getChunkId(Vector3((float)x,(float)y,(float)z));
    if(cid != -1)
    {
        WakeBuriedAround(x, y, z);
        RebuildOpaqueMeshChunk(cid);
        // The light did change, and the neighbouring chunks carry that in their vertex colours.
        rebuildNearestChunks(cid, Vector3((float)x,(float)y,(float)z));
    }
}

void CraftWorld::UpdateButtons(float dt)
{
    if(mPressedButtons.empty())
    {
        return;
    }
    for(unsigned int i = 0; i < mPressedButtons.size(); )
    {
        mPressedButtons[i].timeLeft -= dt;
        if(mPressedButtons[i].timeLeft <= 0.0f)
        {
            // Java plays the click again when the button pops back out. Only here, so tearing a
            // pressed button down (or dropping the whole list on save) stays silent.
            if(IsButtonBlock(GetBlock(mPressedButtons[i].x, mPressedButtons[i].y, mPressedButtons[i].z)))
            {
                SoundManager::Instance()->PlayButtonSound(GetBlock(mPressedButtons[i].x, mPressedButtons[i].y, mPressedButtons[i].z) == ButtonWood::getID());
            }
            ReleaseButton(mPressedButtons[i].x, mPressedButtons[i].y, mPressedButtons[i].z);
            mPressedButtons.erase(mPressedButtons.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

void CraftWorld::ReleaseAllButtons()
{
    for(unsigned int i = 0; i < mPressedButtons.size(); i++)
    {
        ReleaseButton(mPressedButtons[i].x, mPressedButtons[i].y, mPressedButtons[i].z);
    }
    mPressedButtons.clear();
}

// --- Redstone repeater (Java BlockRedstoneRepeater) ----------------------------------------------
//
// A repeater is the only component that re-emits a signal, so it is the thing that lets a line run past
// the 15-cell limit of dust. Two properties matter and both are directional:
//
//   * it reads exactly one cell -- the block it faces away from -- and nothing else;
//   * it drives exactly one cell -- the block it faces -- always at full strength (15).
//
// Facing and delay share the settings nibble with the buttons, which the light code already skips for
// us. The powered state is deliberately NOT a third state bit: it is a second block id (RepeaterOff /
// RepeaterOn). That keeps "does a repeater drive this cell" an id comparison instead of a scan over a
// live list, which matters because the dust BFS asks that question for every cell it touches. It also
// matches the furnace off/on pair this codebase already uses, and makes the mesher pick the lit tile
// for free.
//
// The delay is Java's scheduled tick: nothing is stored per repeater beyond its pending timer. When the
// timer fires the input is re-read, so a signal that flips and flips back within the delay simply
// settles with no output change.

// Output direction: 0 = -Z, 1 = +X, 2 = +Z, 3 = -X (Java's metadata order, rotated to this codebase's axes).

bool CraftWorld::IsRepeaterBlock(block_t b)
{
    return b == RepeaterOff::getID() || b == RepeaterOn::getID();
}

int CraftWorld::RepeaterFacingFromState(int state)
{
    return state & 3;
}

int CraftWorld::RepeaterDelayFromState(int state)
{
    return ((state >> 2) & 3) + 1; // 1..4 redstone ticks
}

int CraftWorld::RepeaterStateFrom(int facing, int delay)
{
    if(delay < 1) delay = 1;
    if(delay > 4) delay = 4;
    return (facing & 3) | (((delay - 1) & 3) << 2);
}

int CraftWorld::GetRepeaterState(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }
    if(IsRepeaterBlock(GetBlock(x,y,z)) == false)
    {
        return 0;
    }
    return GetBlockSettingsNoCheck(x,y,z) & OpRedstonePowerMask;
}

void CraftWorld::SetRepeaterState(int x, int y, int z, int state)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return;
    }
    if(IsRepeaterBlock(GetBlock(x,y,z)) == false)
    {
        return;
    }
    block_settings_t& s = GetBlockSettings(x,y,z);
    s = (block_settings_t)((s & ~OpRedstonePowerMask) | (state & OpRedstonePowerMask));
}

// 15 when a powered repeater sits on this cell's level and faces it, else 0. Only the cell in front of a
// powered repeater is ever driven; all four other sides read 0, which is the whole point of a repeater.
int CraftWorld::RepeaterOutputAt(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }
    for(int f = 0; f < 4; f++)
    {
        // The repeater that could be driving this cell sits one step back along f, and must itself face f.
        int rx = x - kRepeaterDirX[f];
        int rz = z - kRepeaterDirZ[f];
        if(rx < 0 || rz < 0 || rx >= worldLimit || rz >= worldLimit)
        {
            continue;
        }
        if(GetBlock(rx,y,rz) != RepeaterOn::getID())
        {
            continue;
        }
        if(RepeaterFacingFromState(GetBlockSettingsNoCheck(rx,y,rz) & OpRedstonePowerMask) == f)
        {
            return 15;
        }
    }
    return 0;
}

// A repeater's one and only input: whatever powers the cell it faces away from.
int CraftWorld::RepeaterInputPowered(int x, int y, int z)
{
    if(IsRepeaterBlock(GetBlock(x,y,z)) == false)
    {
        return 0;
    }
    int f = RepeaterFacingFromState(GetRepeaterState(x,y,z));
    int bx = x - kRepeaterDirX[f];
    int bz = z - kRepeaterDirZ[f];
    if(bx < 0 || bz < 0 || bx >= worldLimit || bz >= worldLimit)
    {
        return 0;
    }
    block_t b = GetBlock(bx,y,bz);
    if(b == RedstoneBlock::getID())
    {
        return 15;
    }
    if(b == RepeaterOn::getID())
    {
        // Chained repeaters feed each other through this, and the cell asked about matters: the question
        // is "is a powered repeater driving *me*", which is this cell, not the one behind me. Asking
        // about the cell behind (`bx,bz`) answers "is something driving the upstream repeater", and in a
        // plain A -> B chain the answer to that is no -- the cell behind A holds dust, not a repeater --
        // so B never fired and a chain stalled on its second stage.
        return RepeaterOutputAt(x,y,z);
    }
    if(b == Redstoneoff::getID())
    {
        return GetRedstoneDustPower(bx,y,bz);
    }
    if(IsSwitchBlock(b))
    {
        // A lever or button laid in line with a repeater, on the same flat plane, now drives it. The cell
        // behind the repeater is where its input is read, and a live switch provides 15 *to that cell*
        // (Java isProvidingWeakPower), which is the same rule the dust BFS above already applies to a wire
        // touching a switch. Without this case the switch fell through to the last line, which asks
        // GetBlockRedstonePower() about the switch's own cell -- and that early-outs, because a switch is
        // not a conductor. So a lever one block behind a repeater was invisible to it.
        //
        // The cell asked about matters, and it is deliberately (bx,bz) and not the repeater's own cell.
        // Asking about (x,z) would also accept a switch *beside* or *in front of* the repeater, and a
        // repeater that reads its flanks is no longer the one-way component RepeaterOutputAt exists to
        // keep it -- a lever next to it would fire its output in both directions.
        return ButtonPressedFromState(GetButtonState(bx, y, bz)) ? 15 : 0;
    }
    return GetBlockRedstonePower(bx,y,bz);
}

// Java BlockRedstoneRepeater locking: a *powered* repeater that fires into this repeater's side freezes
// it. The neighbour sits one step to the side and faces straight at us, so while it is on our output is
// pinned -- flip the neighbour off and the freeze lifts and we re-decide. This is what a "locked"
// repeater is, and it is how you gate a clock without stopping it.
//
// Every read here goes through _GetBlockNoCheck, including the cell being asked about, because the
// mesher calls this to decide whether to draw the lock bar: the write-accessor GetBlock would try to
// materialise a column for a neighbour that has not generated, which is exactly the trap the mesh build
// must not fall into.
bool CraftWorld::RepeaterIsLocked(int x, int y, int z)
{
    if(IsRepeaterBlock(_GetBlockNoCheck(x,y,z)) == false)
    {
        return false;
    }
    int f = RepeaterFacingFromState(GetBlockSettingsNoCheck(x,y,z) & OpRedstonePowerMask);
    for(int k = 1; k <= 3; k += 2)
    {
        int p = (f + k) & 3;                        // the two flanking sides, never front or back
        int ax = x - kRepeaterDirX[p];
        int az = z - kRepeaterDirZ[p];
        if(ax < 0 || az < 0 || ax >= worldLimit || az >= worldLimit)
        {
            continue;
        }
        if(_GetBlockNoCheck(ax,y,az) != RepeaterOn::getID())
        {
            continue;
        }
        // Only counts if it actually points at us; a repeater on our flank facing away does not lock us.
        if(RepeaterFacingFromState(GetBlockSettingsNoCheck(ax,y,az) & OpRedstonePowerMask) == p)
        {
            return true;
        }
    }
    return false;
}

// Queue a repeater's scheduled tick. One timer per cell: while one is counting down, further events do
// not restart it, which is what lets a repeater actually fire while its input is still settling.
void CraftWorld::ScheduleRepeater(int x, int y, int z)
{
    for(unsigned int i = 0; i < mRepeaterTimers.size(); i++)
    {
        if(mRepeaterTimers[i].kind == 0 && mRepeaterTimers[i].x == x && mRepeaterTimers[i].y == y && mRepeaterTimers[i].z == z)
        {
            return;
        }
        // Bounded: a repealed cell leaves at most one entry.
    }
    RepeaterTimer t;
    t.x = x;
    t.y = y;
    t.z = z;
    t.kind = 0;
    t.timeLeft = RepeaterDelayFromState(GetRepeaterState(x,y,z)) * 0.1f; // Java: 0.1 s per redstone tick
    mRepeaterTimers.push_back(t);
}

// Re-check one repeater after an event nearby: if the power behind it no longer matches what it is
// currently driving, arm its delay. Java does exactly this from updateTick/neighbourChanged.
void CraftWorld::EvaluateRepeater(int x, int y, int z)
{
    block_t b = GetBlock(x,y,z);
    if(IsRepeaterBlock(b) == false)
    {
        return;
    }
    // A locked repeater ignores its input entirely. Drop any tick already armed for it so an edge that
    // arrived just before the lock appeared cannot slip through and fire.
    if(RepeaterIsLocked(x,y,z))
    {
        for(unsigned int i = 0; i < mRepeaterTimers.size(); )
        {
            if(mRepeaterTimers[i].kind == 0 && mRepeaterTimers[i].x == x && mRepeaterTimers[i].y == y && mRepeaterTimers[i].z == z)
            {
                mRepeaterTimers.erase(mRepeaterTimers.begin() + i);
            }
            else
            {
                i++;
            }
        }
        return;
    }
    bool have = (b == RepeaterOn::getID());
    bool want = RepeaterInputPowered(x,y,z) > 0;
    if(want != have)
    {
        ScheduleRepeater(x,y,z);
    }
}

// Flip one repeater's output and propagate the change. Split out so the timer loop stays readable.
void CraftWorld::SetRepeaterPowered(int x, int y, int z, bool powered)
{
    block_t now = GetBlock(x,y,z);
    if(IsRepeaterBlock(now) == false)
    {
        return;
    }
    block_t want = powered ? RepeaterOn::getID() : RepeaterOff::getID();
    if(now == want)
    {
        return;
    }
    MC_PERF_INC(kRepeaterFlips);
    GetBlock(x,y,z) = want;

    // The output edge moves: refresh the wire around it, then light any TNT sitting in front of it,
    // which is how a repeater fires a cannon stage.
    UpdateRedstonePower(x,y,z);
    int f = RepeaterFacingFromState(GetRepeaterState(x,y,z));
    int ox = x + kRepeaterDirX[f];
    int oz = z + kRepeaterDirZ[f];
    if(ox >= 0 && oz >= 0 && ox < worldLimit && oz < worldLimit)
    {
        if(powered)
        {
            TryIgniteTNT(ox, y, oz);
        }
        // The cell a repeater faces is strongly powered. UpdateRedstonePower already walks a
        // 5x5x5, but a lamp or piston sitting in that cell is the one thing a player tests
        // with "does this repeater power the block?", so re-decide it here by name.
        if(IsRedstoneLampBlock(GetBlock(ox,y,oz)))
        {
            EvaluateRedstoneLamp(ox, y, oz);
        }
        if(IsPistonBaseBlock(GetBlock(ox,y,oz)))
        {
            SchedulePiston(ox, y, oz);
        }
    }

    // Refresh the block itself, and only the block itself. A repeater flip changes one cell of geometry
    // and nothing else: `InitRepeater` sets no `lightSource`, so the light in and around the cell is
    // untouched and no neighbour's shading can change -- which is why this asks for the transparent half
    // only. The tail is queued rather than inline. The old tail was queued onto the 0.05 s fast-update
    // tick as a *full* chunk rebuild and it also walked the nine neighbouring chunks, and a clock flipping
    // four times a second turned that into a backlog the fast path could never drain, because it builds
    // exactly one chunk per tick. That backlog is what made a running clock drag the frame rate down.
    // The fix is the queue that is quick enough and still bounded, not a synchronous call that pays two
    // chunk walks inside the frame for every flip -- see QueueRedstoneMesh.
    int cid = getChunkId(Vector3((float)x,(float)y,(float)z));
    if(cid != -1)
    {
        WakeBuriedAround(x, y, z);
        QueueRedstoneMesh(cid, false);
    }
}

void CraftWorld::UpdateRepeaters(float dt)
{
    // Torch heat ages on the frame clock, not on the tick clock: it must keep cooling even when nothing
    // is scheduled, which is exactly the case a burnt-out torch is waiting for.
    DecayTorchHeat(dt);

    if(mRepeaterTimers.empty())
    {
        return;
    }
    // The pass is bounded by the list as it stood when the pass began, so an entry added *by* the pass
    // waits for the next one. A timer that changes the world can arm more work -- extending a piston
    // rewrites the redstone around it and any of that can arm another piston -- and without the bound
    // that new entry is measured against the same dt that made the first one due. On a frame long
    // enough to reach the smallest delay (the piston's 0.05 s -- anything under 20 fps) every newly
    // appended entry is due on arrival, so the loop never reaches its end. That is not a slow frame but
    // a frame that never finishes: a hung game with the audio buffer still draining.
    const unsigned int pending = mRepeaterTimers.size();
    for(unsigned int i = 0; i < pending && i < mRepeaterTimers.size(); )
    {
        mRepeaterTimers[i].timeLeft -= dt;
        if(mRepeaterTimers[i].timeLeft <= 0.0f)
        {
            int rx = mRepeaterTimers[i].x;
            int ry = mRepeaterTimers[i].y;
            int rz = mRepeaterTimers[i].z;
            int kind = mRepeaterTimers[i].kind;
            mRepeaterTimers.erase(mRepeaterTimers.begin() + i);
            if(kind == 1)
            {
                // A redstone torch re-check carries no payload: it just asks the world "have you changed?"
                EvaluateRedstoneTorch(rx, ry, rz);
                continue;
            }
            if(kind == 2)
            {
                // A piston carries no payload either, and for the same reason: it re-reads the power at
                // its own cell and extends or retracts to match. This is the only place a piston moves,
                // so the world is settled by the time it does -- see SchedulePiston.
                EvaluatePiston(rx, ry, rz);
                continue;
            }
            // Re-read the input now, rather than trusting the value that armed the tick: a signal that
            // came and went inside the delay leaves the output where it was. A lock that appeared during
            // the delay also cancels the tick.
            if(IsRepeaterBlock(GetBlock(rx,ry,rz)) && RepeaterIsLocked(rx,ry,rz) == false)
            {
                SetRepeaterPowered(rx, ry, rz, RepeaterInputPowered(rx,ry,rz) > 0);
            }
        }
        else
        {
            i++;
        }
    }
}

// Drop pending ticks without applying them; used when the world is torn down or reloaded so a stale
// timer cannot touch a block in some other column. The torch flip history goes with them: it names
// absolute cells, so it is meaningless in a different world.
void CraftWorld::ClearRepeaters()
{
    mRepeaterTimers.clear();
    mTorchHeat.clear();
}

// -------------------------------------------------------------------------------------------------
// Redstone torch (Java BlockRedstoneTorch). A torch is a source that can be switched off by the very
// block it hangs on, which makes torch + block a NOT gate and the basis of every inverter, clock and
// latch. Java gives it tickRate 2 -- 0.1 s -- and that delay is not decoration: it is the floor that
// stops two torches looking at each other from flipping the world back and forth inside one frame.
//
// The design that follows from that: reading power here is a pure function of block ids, and only
// EvaluateRedstoneTorch ever writes. Nothing that reads power calls the evaluator, so an inverter
// cannot recurse into itself; it settles one scheduled tick at a time.
// -------------------------------------------------------------------------------------------------

// Weak power a lit torch pushes into this cell. Any of the six neighbours may be the torch, so scan
// them; only one whose blind direction does not contain this cell counts.
int CraftWorld::RedstoneTorchPowerAt(int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
    {
        return 0;
    }
    const int dx[6] = {1,-1,0,0,0,0};
    const int dy[6] = {0,0,1,-1,0,0};
    const int dz[6] = {0,0,0,0,1,-1};
    for(int i = 0; i < 6; i++)
    {
        int nx = x + dx[i];
        int ny = y + dy[i];
        int nz = z + dz[i];
        if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
        {
            continue;
        }
        block_t b = GetBlock(nx,ny,nz);
        if(IsRedstoneTorchLit(b) == false)
        {
            continue;
        }
        // (dx,dy,dz) runs from the cell being asked about to the torch, which is the direction Java
        // hands to isProvidingWeakPower as `side`.
        if(RedstoneTorchPowersTowards(TorchFaceFromBlock(b), dx[i], dy[i], dz[i]))
        {
            return 15;
        }
    }
    return 0;
}

// Java BlockRedstoneTorch.func_150110_m: is this torch's cell being powered? Two things can do it --
// a mechanism firing into the cell, and the block the torch hangs on being powered by something else.
bool CraftWorld::RedstoneTorchMountPowered(int x, int y, int z)
{
    block_t self = GetBlock(x,y,z);
    if(IsRedstoneTorchBlock(self) == false)
    {
        return false;
    }

    // A mechanism firing into the torch's own cell: another lit torch, a repeater aimed at it, or a
    // live wire touching it. This is how a torch that a wire runs into goes dark.
    if(RedstoneTorchPowerAt(x,y,z) > 0)
    {
        return true;
    }
    if(RepeaterOutputAt(x,y,z) > 0)
    {
        return true;
    }
    const int dx[6] = {1,-1,0,0,0,0};
    const int dy[6] = {0,0,1,-1,0,0};
    const int dz[6] = {0,0,0,0,1,-1};
    for(int i = 0; i < 6; i++)
    {
        int nx = x + dx[i];
        int ny = y + dy[i];
        int nz = z + dz[i];
        if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
        {
            continue;
        }
        block_t nb = GetBlock(nx,ny,nz);
        if(nb == RedstoneBlock::getID())
        {
            return true;
        }
        if(IsSwitchBlock(nb) && ButtonPressedFromState(GetButtonState(nx,ny,nz)))
        {
            return true;
        }
        // Dust is deliberately NOT a cause here, and the old code was wrong to list it. A lit torch
        // powers every neighbour except its mount, so dust lying beside a torch at the same level is
        // being fed *by that torch*; reading its power back as "the torch is being powered" made the
        // torch switch itself off the moment a wire ran alongside it, which is exactly the bug of dust
        // "in front of the torch on the same plane" killing it. In Java a torch is switched off by the
        // block it hangs on, not by the wire it is feeding -- and that case is handled below by the
        // mount read, which is safe from feedback because RedstoneTorchPowersTowards drops the mount
        // direction. Wire that reaches the torch through its mount block still turns it off there.
    }

    // The block the torch is attached to. This read cannot feed back in: RedstoneTorchPowersTowards
    // drops exactly the direction the mount sits in, so the mount's power can never be counting this
    // torch's own output. That invariant is the whole reason a torch can gate itself off a block.
    int mx, my, mz;
    RedstoneTorchMountOffset(TorchFaceFromBlock(self), mx, my, mz);
    int sx = x + mx;
    int sy = y + my;
    int sz = z + mz;
    if(sx < 0 || sy < 0 || sz < 0 || sx >= worldLimit || sy >= WORLD_HEIGHT || sz >= worldLimit)
    {
        return false;
    }
    return GetBlockRedstonePower(sx,sy,sz) > 0;
}

// Arm a torch's re-check. One entry per cell; while it is counting down, a second event does not
// restart it, so a torch reacts once to a burst of changes rather than once per change.
void CraftWorld::ScheduleRedstoneTorch(int x, int y, int z)
{
    for(unsigned int i = 0; i < mRepeaterTimers.size(); i++)
    {
        if(mRepeaterTimers[i].kind == 1 && mRepeaterTimers[i].x == x && mRepeaterTimers[i].y == y && mRepeaterTimers[i].z == z)
        {
            return;
        }
    }
    RepeaterTimer t;
    t.x = x;
    t.y = y;
    t.z = z;
    t.kind = 1;
    t.timeLeft = 0.1f; // Java BlockRedstoneTorch.tickRate(World) == 2, i.e. 0.1 s
    mRepeaterTimers.push_back(t);
}

// The dedupe above is what bounds this list: at most one pending re-check per torch cell, so even a
// torch that never settles cannot grow it. That is also why the heat budget below is worth having: the
// tick list stays small either way, but an oscillating torch would still re-mesh its chunk ten times a
// second, and on a PSP that is the cost that actually shows.

std::vector<CraftWorld::TorchHeat>::size_type CraftWorld::TorchHeatIndex(int x, int y, int z)
{
    for(std::vector<TorchHeat>::size_type i = 0; i < mTorchHeat.size(); i++)
    {
        if(mTorchHeat[i].x == x && mTorchHeat[i].y == y && mTorchHeat[i].z == z)
        {
            return i;
        }
    }
    return mTorchHeat.size();
}

bool CraftWorld::NoteTorchFlip(int x, int y, int z)
{
    std::vector<TorchHeat>::size_type i = TorchHeatIndex(x,y,z);
    if(i == mTorchHeat.size())
    {
        TorchHeat h;
        h.x = x;
        h.y = y;
        h.z = z;
        h.flips = 0;
        h.windowLeft = 3.0f;  // Java's window is 60 ticks
        mTorchHeat.push_back(h);
        i = mTorchHeat.size() - 1;
    }
    // The window is anchored to the *first* flip of the burst and later flips no longer refresh it. Java
    // keeps a list of flip times and drops the ones older than 60 ticks, so eight flips only burn a torch
    // out if they all landed inside the same three second window. Refreshing the timer on every flip
    // measured something else -- "eight flips, ever, while still flipping" -- which burns out a torch
    // that is merely slow: one pulse every half second reaches eight in four seconds and should be fine.
    mTorchHeat[i].flips++;
    return mTorchHeat[i].flips >= 8;
}

bool CraftWorld::TorchIsBurntOut(int x, int y, int z)
{
    std::vector<TorchHeat>::size_type i = TorchHeatIndex(x,y,z);
    if(i == mTorchHeat.size())
    {
        return false;
    }
    return mTorchHeat[i].flips >= 8;
}

void CraftWorld::DecayTorchHeat(float dt)
{
    for(std::vector<TorchHeat>::size_type i = 0; i < mTorchHeat.size(); )
    {
        mTorchHeat[i].windowLeft -= dt;
        if(mTorchHeat[i].windowLeft <= 0.0f)
        {
            // Cooled: the budget starts again from zero.
            //
            // The re-arm is what makes a torch clock run at all. Java latches a burnt-out torch dark
            // until something else updates it, and in a clock built from torches nothing else ever does --
            // every part of the circuit is waiting on the torch, so the clock stops on its first burnout
            // and stays stopped. Asking the cell to re-decide once its window has cooled is the smallest
            // change that gets the clock going again, and it is safe: the torch re-reads the world and
            // only relights if its mount really is unpowered, so a still-burntout-looking circuit settles.
            const int tx = mTorchHeat[i].x;
            const int ty = mTorchHeat[i].y;
            const int tz = mTorchHeat[i].z;
            mTorchHeat.erase(mTorchHeat.begin() + i);
            if(IsRedstoneTorchBlock(GetBlock(tx,ty,tz)))
            {
                ScheduleRedstoneTorch(tx,ty,tz);
            }
        }
        else
        {
            i++;
        }
    }
}

// Java BlockRedstoneTorch.updateTick: dark while its attachment is powered, lit while it is not. Flip
// only when the two disagree, so the tick a settled circuit is still holding ends right here.
void CraftWorld::EvaluateRedstoneTorch(int x, int y, int z)
{
    block_t b = GetBlock(x,y,z);
    if(IsRedstoneTorchBlock(b) == false)
    {
        return;
    }
    bool lit = IsRedstoneTorchLit(b);
    bool wanted = (RedstoneTorchMountPowered(x,y,z) == false);
    if(lit == wanted)
    {
        return;
    }
    // A hot torch may go dark but may not come back on; that asymmetry is Java's, and it is what makes a
    // feedback loop settle dark instead of flickering. It relights once its window has cooled and
    // something touches it again.
    if(wanted == true && TorchIsBurntOut(x,y,z))
    {
        return;
    }
    SetRedstoneTorchLit(x,y,z, wanted);
}

// Flip one torch, keeping its facing. Everything the torch touches can now read a different answer, so
// this is where the consequences of the flip are pushed out: the wire, the TNT, the meshes.
void CraftWorld::SetRedstoneTorchLit(int x, int y, int z, bool lit)
{
    block_t now = GetBlock(x,y,z);
    if(IsRedstoneTorchBlock(now) == false || IsRedstoneTorchLit(now) == lit)
    {
        return;
    }
    MC_PERF_INC(kTorchFlips);
    GetBlock(x,y,z) = RedstoneTorchToggle(now);
    NoteTorchFlip(x,y,z);

    // Light follows the flip: the lit id is a light source and the unlit one is not. Going dark has to
    // pull the old glow back out of the neighbourhood *before* the area is relit from the sources that
    // are left, because the relight pass only ever adds. Going lit is purely additive, so relighting is
    // enough on its own.
    if(lit == false)
    {
        RemoveLigtSourceAtPosition(x,y,z,Torch::getID());
    }
    QueueLightAreaIn(Vector3((float)x,(float)y,(float)z));

    // Reseed the wire around it and re-decide every repeater and torch in reach. A torch that has just
    // gone dark is no longer a source anywhere, so a single recompute handles both directions of the
    // edge -- there is nothing to un-power by hand.
    UpdateRedstonePower(x,y,z);

    // TNT next to a torch is a direct trigger, the same one dust and buttons get.
    if(lit)
    {
        const int dx[6] = {1,-1,0,0,0,0};
        const int dy[6] = {0,0,1,-1,0,0};
        const int dz[6] = {0,0,0,0,1,-1};
        for(int i = 0; i < 6; i++)
        {
            int nx = x + dx[i];
            int ny = y + dy[i];
            int nz = z + dz[i];
            if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
            {
                continue;
            }
            if(RedstoneTorchPowersTowards(TorchFaceFromBlock(now), dx[i], dy[i], dz[i]) == false)
            {
                continue; // the direction the torch hangs in: nothing to trigger there
            }
            if(TryIgniteTNT(nx,ny,nz))
            {
                break;
            }
        }
    }

    // The torch's own chunk, transparent mesh only -- and *queued*, not inline. The earlier queued form
    // this replaced was genuinely worse than the inline call, but not because queueing was wrong: it
    // queued the *full* chunk and it queued onto the 0.05 s fast path, so a clock asking for two of these
    // per redstone tick (and four a second for the repeater) kept the builder permanently behind. Both of
    // those are fixed here rather than worked around -- the request carries "transparent only" (see
    // QueueRedstoneMesh), and the queue it lands on drains on the frame budget, which is an order of
    // magnitude quicker than 0.05 s. The opaque pass cannot change on a torch flip at all: no block
    // moves, and the only thing that changes is light, whose vertex colours live in the neighbouring
    // chunks.
    int cid = getChunkId(Vector3((float)x,(float)y,(float)z));
    if(cid != -1)
    {
        WakeBuriedAround(x, y, z);
        QueueRedstoneMesh(cid, false);
        // The neighbours are queue-only already, and a torch does not leave its cell, so they are cheap.
        // They stay because the light *did* change on this flip, and their vertex colours need the update.
        rebuildNearestChunks(cid, Vector3((float)x,(float)y,(float)z));
    }
}

// Java BlockRedstoneRepeater.onBlockActivated cycles the delay and never touches the facing. This game
// deliberately keeps only that half of Java's behaviour: a repeater's direction is decided once, when
// it is placed, and then locked forever. Turning a placed repeater would let it be flipped round onto a
// different output without the geometry ever being re-derived from the player, and it makes the block
// impossible to aim reliably. So use always retimes, 1 -> 2 -> 3 -> 4 -> 1, by one redstone tick.
void CraftWorld::UseRepeater(int x, int y, int z)
{
    if(IsRepeaterBlock(GetBlock(x,y,z)) == false)
    {
        return;
    }
    int state = GetRepeaterState(x,y,z);
    int delay = (RepeaterDelayFromState(state) % 4) + 1;
    SetRepeaterState(x,y,z, RepeaterStateFrom(RepeaterFacingFromState(state), delay));

    // A delay change re-times the repeater, so it may now disagree with its input; let the normal event
    // path sort it out. Drop any tick already counting down first, because ScheduleRepeater deliberately
    // keeps the first delay it armed: without this, raising the delay while a tick is in flight would
    // still fire on the old, shorter one, and the delay control would look like it did nothing. Java
    // re-schedules from the metadata write that changes the delay, and this is the same intent.
    for(unsigned int i = 0; i < mRepeaterTimers.size(); )
    {
        if(mRepeaterTimers[i].kind == 0 && mRepeaterTimers[i].x == x && mRepeaterTimers[i].y == y && mRepeaterTimers[i].z == z)
        {
            mRepeaterTimers.erase(mRepeaterTimers.begin() + i);
        }
        else
        {
            i++;
        }
    }
    UpdateRedstonePower(x,y,z);
    EvaluateRepeater(x,y,z);

    int cid = getChunkId(Vector3((float)x,(float)y,(float)z));
    if(cid != -1)
    {
        RebuildTransparentMeshChunk(cid);
        rebuildNearestChunks(cid, Vector3((float)x,(float)y,(float)z));
    }
}

void CraftWorld::UpdateRedstonePower(int x, int y, int z)
{
    MC_PERF_INC(kRsEvents);
    const block_t dustId = Redstoneoff::getID();
    const block_t sourceId = RedstoneBlock::getID();
    const block_t tntId = TNTBlock::getID();
    const int dx[6] = {1,-1,0,0,0,0};
    const int dy[6] = {0,0,1,-1,0,0};
    const int dz[6] = {0,0,0,0,1,-1};

    // Seed the wire network around the change. The old code seeded only the changed cell and its six
    // neighbours, and that is not enough for the common case of a button: PressButton() reports the
    // button cell, while the wire a flat button drives hangs off the block the button is mounted on --
    // one step further out. Nothing was seeded, the wire was never recomputed, and it never fired the
    // cannon. A 5x5x5 neighbourhood covers the button -> support -> wire and dust -> block -> dust
    // chains. This runs on an event (button press, block place, fluid move), never per frame.
    int seedX[125];
    int seedY[125];
    int seedZ[125];
    int seedCount = 0;
    for(int sz = -2; sz <= 2; sz++)
    {
        for(int sy = -2; sy <= 2; sy++)
        {
            for(int sx = -2; sx <= 2; sx++)
            {
                MC_PERF_INC(kRsSeedCells);
                int nx = x + sx;
                int ny = y + sy;
                int nz = z + sz;
                if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
                {
                    continue;
                }
                block_t b = GetBlock(nx,ny,nz);
                // A repeater in range has to re-decide whether its input still matches its output. It
                // does not feed the wire BFS directly -- it only ever drives its single output cell --
                // so arm its delay and move on. The chain then propagates one delay at a time, which is
                // what makes a ring of repeaters a clock instead of an infinite loop.
                if(IsRepeaterBlock(b))
                {
                    EvaluateRepeater(nx,ny,nz);
                    continue;
                }
                // A redstone torch in range gets the same treatment, and for the same reason: what it
                // does next depends on the world, so it has to be told the world moved. Java does this
                // from onNeighborBlockChange; every redstone event already walks this window, so arming
                // the torch here covers dust, buttons, repeaters and neighbouring torches alike. A torch
                // that is armed and then finds nothing changed simply stops -- it never re-arms itself,
                // so a settled circuit goes quiet and only a real edge keeps ticking.
                if(IsRedstoneTorchBlock(b))
                {
                    ScheduleRedstoneTorch(nx,ny,nz);
                    continue;
                }
                // A lamp in range re-reads its power. This is the whole of the lamp: it is a consumer with
                // no event of its own, so the redstone window is the only thing that can tell it the world
                // moved. Every change that can light or unlight a lamp starts within one cell of it, which
                // is inside this window -- see EvaluateRedstoneLamp.
                // A piston base in range re-decides. It is a consumer with no event of its own beyond
                // this window, exactly like the lamp, and for the same reason every change that can power
                // or unpin it starts within one cell of it. *Armed*, not acted on: UpdateRedstonePower
                // runs from inside a dust walk and from placement, and extending a piston rewrites blocks
                // and announces the world again, so the work goes on the timer list -- see SchedulePiston.
                if(IsPistonBaseBlock(b))
                {
                    SchedulePiston(nx,ny,nz);
                    continue;
                }
                if(IsRedstoneLampBlock(b))
                {
                    EvaluateRedstoneLamp(nx,ny,nz);
                    continue;
                }
                if(b != dustId && b != sourceId)
                {
                    continue;
                }
                seedX[seedCount] = nx;
                seedY[seedCount] = ny;
                seedZ[seedCount] = nz;
                seedCount++;
            }
        }
    }

    std::vector<int> doneX;
    std::vector<int> doneY;
    std::vector<int> doneZ;

    for(int s = 0; s < seedCount; s++)
    {
        bool already = false;
        for(unsigned int d = 0; d < doneX.size(); d++)
        {
            if(doneX[d] == seedX[s] && doneY[d] == seedY[s] && doneZ[d] == seedZ[s])
            {
                already = true;
                break;
            }
        }
        if(already)
        {
            continue;
        }

        std::vector<int> cx;
        std::vector<int> cy;
        std::vector<int> cz;
        cx.push_back(seedX[s]);
        cy.push_back(seedY[s]);
        cz.push_back(seedZ[s]);

        for(unsigned int i = 0; i < cx.size() && i < 512; i++)
        {
            MC_PERF_INC(kRsWalkCells);
            for(int dir = 0; dir < 6; dir++)
            {
                int nx = cx[i] + dx[dir];
                int ny = cy[i] + dy[dir];
                int nz = cz[i] + dz[dir];
                if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
                {
                    continue;
                }
                block_t b = GetBlock(nx,ny,nz);
                if(b != dustId && b != sourceId)
                {
                    continue;
                }
                bool seen = false;
                for(unsigned int j = 0; j < cx.size(); j++)
                {
                    if(cx[j] == nx && cy[j] == ny && cz[j] == nz)
                    {
                        seen = true;
                        break;
                    }
                }
                if(!seen)
                {
                    cx.push_back(nx);
                    cy.push_back(ny);
                    cz.push_back(nz);
                }
            }
        }

        std::vector<int> pwr(cx.size(), 0);
        std::vector<int> q;
        q.reserve(cx.size());

        for(unsigned int i = 0; i < cx.size(); i++)
        {
            if(GetBlock(cx[i],cy[i],cz[i]) != sourceId)
            {
                continue;
            }
            for(int dir = 0; dir < 6; dir++)
            {
                int nx = cx[i] + dx[dir];
                int ny = cy[i] + dy[dir];
                int nz = cz[i] + dz[dir];
                for(unsigned int j = 0; j < cx.size(); j++)
                {
                    if(cx[j] == nx && cy[j] == ny && cz[j] == nz && GetBlock(nx,ny,nz) == dustId)
                    {
                        if(pwr[j] < 15)
                        {
                            pwr[j] = 15;
                            q.push_back((int)j);
                        }
                    }
                }
            }
        }

        // A pressed button powers the blocks touching it, which in turn power the wire touching them
        for(unsigned int j = 0; j < cx.size(); j++)
        {
            if(GetBlock(cx[j],cy[j],cz[j]) != dustId || pwr[j] >= 15)
            {
                continue;
            }
            for(int dir = 0; dir < 6; dir++)
            {
                int nx = cx[j] + dx[dir];
                int ny = cy[j] + dy[dir];
                int nz = cz[j] + dz[dir];
                if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
                {
                    continue;
                }
                block_t nb = GetBlock(nx,ny,nz);
                // The neighbour is the pressed button or thrown lever itself (a direct source for wire,
                // see GetRedstoneDustPower), or a conductive block the switch is powering.
                bool feedsWire = false;
                if(IsSwitchBlock(nb))
                {
                    feedsWire = ButtonPressedFromState(GetButtonState(nx,ny,nz));
                }
                else if(nb == RepeaterOn::getID())
                {
                    // Only the cell a powered repeater faces: it is a one-way component, so wire beside
                    // it or behind it stays unpowered.
                    int f = RepeaterFacingFromState(GetBlockSettingsNoCheck(nx,ny,nz) & OpRedstonePowerMask);
                    feedsWire = (ny == cy[j] && nx + kRepeaterDirX[f] == cx[j] && nz + kRepeaterDirZ[f] == cz[j]);
                }
                else if(IsRedstoneTorchBlock(nb))
                {
                    // A lit torch touching this wire is a direct source, exactly as it is in
                    // GetRedstoneDustPower. The BFS has to know about it too, because the power it
                    // solves is what tells this cell whether it is live for TNT priming.
                    feedsWire = IsRedstoneTorchLit(nb)
                        && RedstoneTorchPowersTowards(TorchFaceFromBlock(nb), nx - cx[j], ny - cy[j], nz - cz[j]);
                }
                else if(nb != dustId && IsRedstoneConductive(nb))
                {
                    // Same "something is strongly powering this block" cases as GetRedstoneDustPower:
                    // a pressed button on it, a powered repeater facing it, or a lit torch touching it.
                    // Keep the two in step -- this BFS is what tells a cell whether it is live for TNT
                    // priming.
                    feedsWire = GetButtonPowerAt(nx,ny,nz) > 0
                        || RepeaterOutputAt(nx,ny,nz) > 0
                        || RedstoneTorchPowerAt(nx,ny,nz) > 0;
                }
                if(feedsWire)
                {
                    pwr[j] = 15;
                    q.push_back((int)j);
                    break;
                }
            }
        }

        for(unsigned int qi = 0; qi < q.size(); qi++)
        {
            int i = q[qi];
            if(pwr[i] <= 1)
            {
                continue;
            }
            int nextp = pwr[i] - 1;
            auto tryProp = [&](int nx, int ny, int nz)
            {
                for(unsigned int j = 0; j < cx.size(); j++)
                {
                    if(cx[j] == nx && cy[j] == ny && cz[j] == nz && GetBlock(nx,ny,nz) == dustId)
                    {
                        if(nextp > pwr[j])
                        {
                            pwr[j] = nextp;
                            q.push_back((int)j);
                        }
                    }
                }
            };
            for(int dir = 0; dir < 6; dir++)
            {
                tryProp(cx[i] + dx[dir], cy[i] + dy[dir], cz[i] + dz[dir]);
            }
            // Same staircase edges GetRedstoneDustPower walks, so a climb carries charge
            // into the TNT-priming BFS rather than lighting the artwork and leaving the
            // cannon dark.
            const int hx[4] = {1,-1,0,0};
            const int hz[4] = {0,0,1,-1};
            for(int d = 0; d < 4; d++)
            {
                int sx = cx[i] + hx[d];
                int sz = cz[i] + hz[d];
                if(sx < 0 || sz < 0 || sx >= worldLimit || sz >= worldLimit)
                {
                    continue;
                }
                int uy = cy[i] + 1;
                int dy2 = cy[i] - 1;
                if(uy < WORLD_HEIGHT && RedstoneClimbCut(this, cx[i], uy, cz[i]) == false)
                {
                    tryProp(sx, uy, sz);
                }
                if(dy2 >= 0 && RedstoneClimbCut(this, sx, cy[i], sz) == false)
                {
                    tryProp(sx, dy2, sz);
                }
            }
        }

        std::vector<int> remeshChunks;
        std::vector<int> remeshX;
        std::vector<int> remeshY;
        std::vector<int> remeshZ;
        bool repairedLight = false;

        for(unsigned int i = 0; i < cx.size(); i++)
        {
            doneX.push_back(cx[i]);
            doneY.push_back(cy[i]);
            doneZ.push_back(cz[i]);

            if(GetBlock(cx[i],cy[i],cz[i]) != dustId)
            {
                continue;
            }

            int curLight = GetLightLevelNoCheck(cx[i],cy[i],cz[i]);
            if(curLight > 0 && curLight == (pwr[i] & 15))
            {
                SetLightLevelNoCheck(cx[i],cy[i],cz[i], 0);
                repairedLight = true;
            }

            int cid = getChunkId(Vector3((float)cx[i],(float)cy[i],(float)cz[i]));
            if(cid == -1)
            {
                continue;
            }
            bool seenChunk = false;
            for(unsigned int c = 0; c < remeshChunks.size(); c++)
            {
                if(remeshChunks[c] == cid)
                {
                    seenChunk = true;
                    break;
                }
            }
            if(!seenChunk)
            {
                remeshChunks.push_back(cid);
                remeshX.push_back(cx[i]);
                remeshY.push_back(cy[i]);
                remeshZ.push_back(cz[i]);
            }
        }

        // Any repeater hanging off this wire network has just had its input change, so re-decide it.
        // The dust BFS cannot reach a repeater by itself -- a repeater is a one-way component, not wire,
        // so the walk stops at it -- which means a repeater further along a long line would otherwise
        // never be re-checked when the signal at the far end moves, and a chain would stall after the
        // first stage. Picking repeaters up from the wire they sit against is what keeps a chain running.
        for(unsigned int i = 0; i < cx.size(); i++)
        {
            for(int dir = 0; dir < 6; dir++)
            {
                int nx = cx[i] + dx[dir];
                int ny = cy[i] + dy[dir];
                int nz = cz[i] + dz[dir];
                if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
                {
                    continue;
                }
                if(IsRepeaterBlock(GetBlock(nx,ny,nz)))
                {
                    EvaluateRepeater(nx,ny,nz);
                }
            }
        }

        block_t originBlock = GetBlock(x,y,z);
        if(originBlock != dustId && originBlock != sourceId && !LightSourceBlock(originBlock))
        {
            if(GetLightLevel(x,y,z) > 0)
            {
                SetLightLevel(x,y,z, 0);
                repairedLight = true;
            }
        }
        if(repairedLight)
        {
            // Queued, not inline, and for the same reason the mesh request above is: `UpdateLightAreaIn`
            // sweeps a 19x19x19 box and re-propagates every light source it finds in it, which is a
            // second uncapped cost per redstone event. The light queue is already the mechanism the torch
            // and button paths use for exactly this (`QueueLightAreaIn`), and it is drained on the frame
            // budget at the top of the update -- so this was the one redstone caller not using it.
            QueueLightAreaIn(Vector3((float)x,(float)y,(float)z));
        }

        // A dust network's cells can straddle chunks and a wired mechanism's block id moves between the
        // two meshes, so each touched chunk needs both walks -- but through the queue, not inline. This
        // loop is the hot one: it runs on every redstone event, and a clock makes events faster than a
        // frame. See QueueRedstoneMesh.
        for(unsigned int c = 0; c < remeshChunks.size(); c++)
        {
            QueueRedstoneMesh(remeshChunks[c], true);
            if(repairedLight)
            {
                rebuildNearestChunks(remeshChunks[c], Vector3((float)remeshX[c],(float)remeshY[c],(float)remeshZ[c]));
            }
        }

        // Java: powered dust weakly powers the block it sits on and the blocks it points into, and a
        // powered block activates adjacent mechanisms. Together with the direct dust/redstone-block
        // touch, that is what lights TNT in a plain cannon wired along its outside.
        const int hx[4] = {1,-1,0,0};
        const int hz[4] = {0,0,1,-1};
        auto primeTNTNextTo = [&](int bx, int by, int bz)
        {
            for(int dir = 0; dir < 6; dir++)
            {
                int tx = bx + dx[dir];
                int ty = by + dy[dir];
                int tz = bz + dz[dir];
                if(tx < 0 || ty < 0 || tz < 0 || tx >= worldLimit || ty >= WORLD_HEIGHT || tz >= worldLimit)
                {
                    continue;
                }
                if(GetBlock(tx,ty,tz) == tntId)
                {
                    PrimeTNTBlock(tx,ty,tz);
                }
            }
        };

        for(unsigned int i = 0; i < cx.size(); i++)
        {
            block_t b = GetBlock(cx[i],cy[i],cz[i]);
            bool live = (b == sourceId) || (b == dustId && pwr[i] > 0);
            if(!live)
            {
                continue;
            }

            // Dust or a block of redstone touching TNT lights it directly. A block of
            // redstone does *not* power the solids around it -- Java activates adjacent
            // mechanisms only -- so TNT sitting on a neighbour of the source (the
            // x1 / 23 layout) must stay dark. The 5x5x5 TryIgniteTNT pass below is
            // what lights a charge through a block that is *actually* powered (a
            // repeater facing it, dust pointing into it).
            primeTNTNextTo(cx[i],cy[i],cz[i]);

            if(b != sourceId)
            {
                // Dust weakly powers the block it is sitting on ...
                int by = cy[i] - 1;
                if(by >= 0 && IsRedstoneConductive(GetBlock(cx[i],by,cz[i])))
                {
                    primeTNTNextTo(cx[i],by,cz[i]);
                }
                // ... and the blocks it points into at its own level
                for(int d2 = 0; d2 < 4; d2++)
                {
                    int nx = cx[i] + hx[d2];
                    int nz = cz[i] + hz[d2];
                    if(nx < 0 || nz < 0 || nx >= worldLimit || nz >= worldLimit)
                    {
                        continue;
                    }
                    if(IsRedstoneConductive(GetBlock(nx,cy[i],nz)))
                    {
                        primeTNTNextTo(nx,cy[i],nz);
                    }
                }
            }
        }
    }

    // Java BlockTNT.onNeighborBlockChange: a charge re-reads its own power whenever the redstone next
    // to it moves. The two passes above ask the question the other way round -- "is this block I just
    // powered touching a charge?" -- and that only finds the charges whose *source* (dust, a block of
    // redstone) they can stand next to, which is why the launcher-style trigger worked. A repeater and
    // a torch act through a block instead: the repeater strongly powers the one cell it faces, and the
    // charge that has to light is the one against *that* cell, two steps from the source and invisible
    // to the source-side passes. That is the reported bug: a repeater powered its block, the block lit
    // up, and the charge leaning on it stayed dark.
    //
    // Asking the charge instead is direction-free and covers every source at once, because
    // TryIgniteTNT reads the same six neighbours whatever lit them. The window is the same 5x5x5 the
    // seeding above uses, and the charge is never further than two from the source that lights it
    // (source -> the block it powers -> the charge), so it is always inside. This runs on an event,
    // never per frame, and only ever fires for a charge that is genuinely powered.
    for(int tz = -2; tz <= 2; tz++)
    {
        for(int ty = -2; ty <= 2; ty++)
        {
            for(int tx = -2; tx <= 2; tx++)
            {
                int px = x + tx;
                int py = y + ty;
                int pz = z + tz;
                if(px < 0 || py < 0 || pz < 0 || px >= worldLimit || py >= WORLD_HEIGHT || pz >= worldLimit)
                {
                    continue;
                }
                if(GetBlock(px,py,pz) == tntId)
                {
                    TryIgniteTNT(px,py,pz);
                }
            }
        }
    }
}

void CraftWorld::DestroyTNTentity(int id)
{
    if(id < mTNTs.size() && id >= 0)
    {
        mTNTs.erase(mTNTs.begin()+id);
    }
}

void CraftWorld::CollisionWithOtherZombies(Zombie *Zombie1)
{

}

block_t CraftWorld::_GetBlock (const int x, const int y, const int z) // id блока в данной координате
{
    if(meshLightActive)
    {
        int lx = x - meshLightOriginX;
        int ly = y - meshLightOriginY;
        int lz = z - meshLightOriginZ;
        if((unsigned)lx < (unsigned)meshLightDimX && (unsigned)ly < (unsigned)meshLightDimY && (unsigned)lz < (unsigned)meshLightDimZ)
            return meshBlocks[lx + meshLightDimX * (ly + meshLightDimY * lz)];
    }
    if (x < 0 || z < 0 || y < 0 || x >= worldLimit || z >= worldLimit || y >= WORLD_HEIGHT)
        return 0;
    ChunkColumn* col = ColumnForBlock(x, z);
    if(!col)
        return 0;
    int cs = col->chunkSize;
    return col->blocks[col->Index(x - col->cx * cs, y, z - col->cz * cs)];
}

block_t CraftWorld::_GetBlockNoCheck(const int x, const int y, const int z) // id блока в данной координате
{
    if(meshLightActive)
    {
        int lx = x - meshLightOriginX;
        int ly = y - meshLightOriginY;
        int lz = z - meshLightOriginZ;
        if((unsigned)lx < (unsigned)meshLightDimX && (unsigned)ly < (unsigned)meshLightDimY && (unsigned)lz < (unsigned)meshLightDimZ)
            return meshBlocks[lx + meshLightDimX * (ly + meshLightDimY * lz)];
    }
    ChunkColumn* col = ColumnForBlock(x, z);
    if(!col || y < 0 || y >= col->height)
        return 0;
    int cs = col->chunkSize;
    return col->blocks[col->Index(x - col->cx * cs, y, z - col->cz * cs)];
}


block_t& CraftWorld::GetBlock (const int x, const int y, const int z) // id блока в данной координате
{
    writeSink = 0;
    if (x < 0 || z < 0 || y < 0 || x >= worldLimit || z >= worldLimit || y >= WORLD_HEIGHT)
        return writeSink;
    ChunkColumn* col = ColumnForBlock(x, z);
    if(!col)
    {
        // Never generate a column from inside a mesh/light build: the mesh column grid
        // is a snapshot, so every write of that generation would land in writeSink and
        // the column would be marked generated while still empty (a permanent hole).
        // The neighbour mesh is re-dirtied when the column really generates.
        if(worldGenerating || spawnGenClip || meshGridActive || meshLightActive)
            return writeSink;
        // And never generate one outside the visible ring. This is the other half of the same problem
        // (see ColumnWithinStreamRing): a plain gameplay read -- a mob step, a fluid check, a snow
        // removal -- that reaches one column past what the streamer has loaded used to run the whole
        // 16x16x128 generator (~50 ms) right there in the reading frame. The frame probe caught it three
        // times in 150 s, at ~49 ms each, and it is the largest single hitch while walking. Outside the
        // ring the block reads as air; the streamer creates the column a moment later and DirtyNeighbor-
        // Meshes/RelightSharedEdges fix the boundary then.
        if(!ColumnWithinStreamRing(ChunkFloorDiv(x, CHUNK_SIZE), ChunkFloorDiv(z, CHUNK_SIZE)))
            return writeSink;
        col = EnsureColumn(ChunkFloorDiv(x, CHUNK_SIZE), ChunkFloorDiv(z, CHUNK_SIZE), true);
    }
    if(!col)
        return writeSink;
    int cs = col->chunkSize;
    return col->blocks[col->Index(x - col->cx * cs, y, z - col->cz * cs)];
}

block_t& CraftWorld::GetBlockNoCheck(const int x, const int y, const int z) // id блока в данной координате
{
    ChunkColumn* col = ColumnForBlock(x, z);
    if(!col || y < 0 || y >= col->height)
        return dummyBlock;
    int cs = col->chunkSize;
    return col->blocks[col->Index(x - col->cx * cs, y, z - col->cz * cs)];
}

void CraftWorld::AddBlockType(block_t id, const BaseBlock& block)
{
    if((int)id < kExtendedBlockBase)
        return;
    if(blockTypes.size() <= (size_t)id)
        blockTypes.resize((size_t)id + 1);
    // Do not strand a mesh. Registering is normally a one-off before buildblocksVerts(), but
    // re-registering a slot that has already been built would copy this block's (still NULL) pointers
    // over the live ones and nothing would ever free them. Freeing first makes the overwrite safe.
    FreeBlockTypeMesh(id);
    blockTypes[id] = block;
}

block_settings_t& CraftWorld::GetBlockSettings(const int x, const int y, const int z)
{
    writeSink = 0;
    if (x < 0 || z < 0 || y < 0 || x >= worldLimit || z >= worldLimit || y >= WORLD_HEIGHT)
        return dummySettings;
    ChunkColumn* col = ColumnForBlock(x, z);
    if(!col)
    {
        if(worldGenerating || spawnGenClip || meshGridActive || meshLightActive)
            return dummySettings;
        // Same ring gate as GetBlock: a settings read must not generate terrain outside the view.
        if(!ColumnWithinStreamRing(ChunkFloorDiv(x, CHUNK_SIZE), ChunkFloorDiv(z, CHUNK_SIZE)))
            return dummySettings;
        col = EnsureColumn(ChunkFloorDiv(x, CHUNK_SIZE), ChunkFloorDiv(z, CHUNK_SIZE), true);
    }
    if(!col)
        return dummySettings;
    int cs = col->chunkSize;
    return col->settings[col->Index(x - col->cx * cs, y, z - col->cz * cs)];
}

block_settings_t& CraftWorld::GetBlockSettingsNoCheck(const int x, const int y, const int z)
{
    ChunkColumn* col = ColumnForBlock(x, z);
    if(!col || y < 0 || y >= col->height)
        return dummySettings;
    int cs = col->chunkSize;
    return col->settings[col->Index(x - col->cx * cs, y, z - col->cz * cs)];
}

int CraftWorld::GetShadowLevel(const int x, const int y, const int z)
{
    int value = 0;
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        block_t s = GetBlockSettingsNoCheck(x,y,z);
        if((s & OpFirstShadowBit) != 0)
            value += 8;
        if((s & OpSecondShadowBit) != 0)
            value += 4;
        if((s & OpThirdShadowBit) != 0)
            value += 2;
        if((s & OpFourthShadowBit) != 0)
            value += 1;
        return value;
    }
    return 0;
}

int CraftWorld::GetShadowLevelNoCheck(const int x, const int y, const int z)
{
    block_t s = GetBlockSettingsNoCheck(x,y,z);
    int value = 0;
    if((s & OpFirstShadowBit) != 0)
        value += 8;
    if((s & OpSecondShadowBit) != 0)
        value += 4;
    if((s & OpThirdShadowBit) != 0)
        value += 2;
    if((s & OpFourthShadowBit) != 0)
        value += 1;
    return value;
}

void CraftWorld::SetShadowLevel (const int x, const int y, const int z, int level)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        if((GetBlockSettingsNoCheck(x,y,z) & OpFirstShadowBit) != 0)
        {
            GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
        }
        if((GetBlockSettingsNoCheck(x,y,z) & OpSecondShadowBit) != 0)
        {
            GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
        }
        if((GetBlockSettingsNoCheck(x,y,z) & OpThirdShadowBit) != 0)
        {
            GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
        }
        if((GetBlockSettingsNoCheck(x,y,z) & OpFourthShadowBit) != 0)
        {
            GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
        }

        switch(level)
        {
            case 1:
            GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
            break;
            case 2:
            GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
            break;
            case 3:
            GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
            break;
            case 4:
            GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
            break;
            case 5:
            GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
            break;
            case 6:
            GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
            break;
            case 7:
            GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
            break;

            case 8:
            GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
            break;
            case 9:
            GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
            break;
            case 10:
            GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
            break;
            case 11:
            GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
            break;
            case 12:
            GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
            break;
            case 13:
            GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
            break;
            case 14:
            GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
            break;
            case 15:
            GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
            GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
            break;
        }
    }
}


void CraftWorld::SetShadowLevelNoCheck (const int x, const int y, const int z, int level)
{
    if((GetBlockSettingsNoCheck(x,y,z) & OpFirstShadowBit) != 0)
    {
        GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
    }
    if((GetBlockSettingsNoCheck(x,y,z) & OpSecondShadowBit) != 0)
    {
        GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
    }
    if((GetBlockSettingsNoCheck(x,y,z) & OpThirdShadowBit) != 0)
    {
        GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
    }
    if((GetBlockSettingsNoCheck(x,y,z) & OpFourthShadowBit) != 0)
    {
        GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
    }

    switch(level)
    {
        case 1:
        GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
        break;
        case 2:
        GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
        break;
        case 3:
        GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
        break;
        case 4:
        GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
        break;
        case 5:
        GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
        break;
        case 6:
        GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
        break;
        case 7:
        GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
        break;

        case 8:
        GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
        break;
        case 9:
        GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
        break;
        case 10:
        GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
        break;
        case 11:
        GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
        break;
        case 12:
        GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
        break;
        case 13:
        GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
        break;
        case 14:
        GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
        break;
        case 15:
        GetBlockSettingsNoCheck(x,y,z) ^= OpFirstShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpSecondShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpThirdShadowBit;
        GetBlockSettingsNoCheck(x,y,z) ^= OpFourthShadowBit;
        break;
    }
}


// A button stores its facing (bits 0-2) and pressed flag (bit 3) in the LOW nibble of its settings byte.
// That nibble is otherwise block light, and the high nibble is sky light, which is why InitColumnLighting
// (high nibble only) and GetShadowLevel preserve the button state and everything appeared to work. These
// four functions are the ones that own the low nibble, and they were happily overwriting the button's
// state with a light value: pressing a button ignites TNT, that queues a light update, and the update
// erased the facing, so the plate jumped to a different mounting.
//
// The fix is to keep block light out of a button's own cell entirely. That costs nothing visually: the
// mesher lights a button's faces from the neighbouring cells (BuildWorldBlockPlane calls
// BlockFinalLight on the cell the face looks into), so nothing ever reads the light level of the button
// cell itself. All four functions below therefore skip button cells.
void CraftWorld::SetLightLevel(const int x, const int y, const int z, unsigned int level)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        ChunkColumn* col = ColumnForBlock(x, z);
        if(!col || y >= col->height)
            return;
        int cs = col->chunkSize;
        int index = col->Index(x - col->cx * cs, y, z - col->cz * cs);
        if (IsStateCellBlock(col->blocks[index]))
            return; // this nibble holds the block's state, not light
        block_settings_t& blockSettings = col->settings[index];
        int current = blockSettings & 15;
        //clear this value
        blockSettings ^= current & 15;
        //set new value
        blockSettings ^= level & 15;
    }
}

// Returns false when this cell cannot hold light at all, which is NOT the same as "the write was
// unnecessary": there is no resident column here, or the cell is above the column's height, or its
// nibble belongs to a block's state rather than to light. The getter answers 0 for all three, so a
// caller that only reads before writing sees an unlit cell every time it looks and cannot tell that
// this is a cell light will never live in. LightFlood has to tell, because expanding from one is
// what used to turn a ~1159-cell flood into an unbounded walk -- see the comment there.
bool CraftWorld::SetLightLevelNoCheck(const int x, const int y, const int z, unsigned int level)
{
    ChunkColumn* col = ColumnForBlock(x, z);
    if(!col || y < 0 || y >= col->height)
        return false;
    int cs = col->chunkSize;
    int index = col->Index(x - col->cx * cs, y, z - col->cz * cs);
    if (IsStateCellBlock(col->blocks[index]))
        return false; // this nibble holds the block's state, not light
    block_settings_t& blockSettings = col->settings[index];
    int current = blockSettings & 15;
    //clear this value
    blockSettings ^= current & 15;
    //set new value
    blockSettings ^= level & 15;
    return true;
}

int CraftWorld::GetLightLevel(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        ChunkColumn* col = ColumnForBlock(x, z);
        if(!col || y >= col->height)
            return 0;
        int cs = col->chunkSize;
        int index = col->Index(x - col->cx * cs, y, z - col->cz * cs);
        // Reading a button's state as light made a pressed button (state 8-13) look like a light
        // source of that level to every neighbour that samples it. Repeaters store facing/delay in the
        // same nibble and must be skipped for the same reason.
        if (IsStateCellBlock(col->blocks[index]))
            return 0;
        return col->settings[index] & 15;
    }
    return 0;
}

int CraftWorld::GetLightLevelNoCheck(const int x, const int y, const int z)
{
    ChunkColumn* col = ColumnForBlock(x, z);
    if(!col || y < 0 || y >= col->height)
        return 0;
    int cs = col->chunkSize;
    int index = col->Index(x - col->cx * cs, y, z - col->cz * cs);
    if (IsStateCellBlock(col->blocks[index]))
        return 0; // the nibble holds the block's state, not light
    return col->settings[index] & 15;
}

bool CraftWorld::CheckForTorchSupport(const int x, const int y, const int z,int blockID)
{
    bool haveTorch = false;
    (void)blockID;

    for(int zz = z-1; zz <= z+1; zz++)
    {
        for(int xx = x-1; xx <= x+1; xx++)
        {
            for(int yy = y-1; yy <= y+1; yy++)
            {
                if(xx < 0 || yy < 0 || zz < 0 || xx >= worldLimit || yy >= WORLD_HEIGHT || zz >= worldLimit)
                    continue;
                block_t here = GetBlock(xx,yy,zz);
                if(IsTorchBlock(here))
                {
                    if(TorchSupportSolid(xx,yy,zz, here) == false)
                    {
                        GetBlock(xx,yy,zz) = 0;
                        RemoveLigtSourceAtPosition(xx,yy,zz,Torch::getID());
                        // A redstone torch drops a redstone torch, not a plain one, and an unlit torch
                        // drops the same item as the lit one -- that is what lets the player pick it up
                        // and put it back.
                        DropThis(IsRedstoneTorchBlock(here) ? RedstoneTorchItem::getID() : TorchItem::getID(),
                                 Vector3(xx,yy,zz));
                        haveTorch = true;
                        AddChunkToUpdate(getChunkId(Vector3(xx,yy,zz)));
                    }
                }
                else if(IsSwitchBlock(here))
                {
                    // Java BlockButton.onNeighborBlockChange / BlockLever.onNeighborBlockChange: a switch
                    // with nothing left to hold it up comes off as an item. Buttons and levers share this
                    // because they share the storage: the mounting face is in the same nibble, so the
                    // same ButtonSupportPos answers for both.
                    int sx, sy, sz;
                    ButtonSupportPos(xx,yy,zz, sx, sy, sz);
                    if(BlockSolid(sx,sy,sz) == false)
                    {
                        GetBlock(xx,yy,zz) = 0;
                        // The cell is air now, so its nibble is no longer state and would be read back as
                        // a block light level by every reader in the neighbourhood -- see
                        // ClearStateCellNibble. A thrown lever leaves the *brightest* of the four
                        // face+on combinations (15) behind, so this is not theoretical.
                        ClearStateCellNibble(xx,yy,zz);
                        // Taking a switch away changes what the wire around it reads.
                        UpdateRedstonePower(xx,yy,zz);
                        DropThis(here, Vector3(xx,yy,zz));
                        haveTorch = true;
                        AddChunkToUpdate(getChunkId(Vector3(xx,yy,zz)));
                        int buttonChunk = getChunkId(Vector3((float)xx,(float)yy,(float)zz));
                        if(buttonChunk != -1)
                        {
                            RebuildTransparentMeshChunk(buttonChunk);
                        }
                    }
                }
            }
        }
    }
    if(haveTorch)
        QueueLightAreaIn(Vector3(x,y,z));
    return haveTorch;
}

bool CraftWorld::TorchSupportSolid(const int x, const int y, const int z, block_t torchId)
{
    switch(TorchFaceFromBlock(torchId))
    {
        case 1: return BlockSolid(x-1,y,z);
        case 2: return BlockSolid(x+1,y,z);
        case 3: return BlockSolid(x,y,z-1);
        case 4: return BlockSolid(x,y,z+1);
        default: return BlockSolid(x,y-1,z);
    }
}

bool CraftWorld::CheckForLadderSupport(const int x, const int y, const int z)
{
    bool haveLadder = false;

    for(int zz = z-1; zz <= z+1; zz++)
    {
        for(int xx = x-1; xx <= x+1; xx++)
        {
            if(xx < 0 || y < 0 || zz < 0 || xx >= worldLimit || y >= WORLD_HEIGHT || zz >= worldLimit)
                continue;
            block_t here = GetBlock(xx,y,zz);
            if(here == Ladder1::getID() && _GetBlock(xx-1,y,zz) == 0)
            {
                GetBlock(xx,y,zz) = 0;
                DropThis(LadderItem::getID(), Vector3(xx,y,zz));
                haveLadder = true;
                AddChunkToUpdate(getChunkId(Vector3(xx,y,zz)));
            }
            else if(here == Ladder2::getID() && _GetBlock(xx+1,y,zz) == 0)
            {
                GetBlock(xx,y,zz) = 0;
                DropThis(LadderItem::getID(), Vector3(xx,y,zz));
                haveLadder = true;
                AddChunkToUpdate(getChunkId(Vector3(xx,y,zz)));
            }
            else if(here == Ladder3::getID() && _GetBlock(xx,y,zz-1) == 0)
            {
                GetBlock(xx,y,zz) = 0;
                DropThis(LadderItem::getID(), Vector3(xx,y,zz));
                haveLadder = true;
                AddChunkToUpdate(getChunkId(Vector3(xx,y,zz)));
            }
            else if(here == Ladder4::getID() && _GetBlock(xx,y,zz+1) == 0)
            {
                GetBlock(xx,y,zz) = 0;
                DropThis(LadderItem::getID(), Vector3(xx,y,zz));
                haveLadder = true;
                AddChunkToUpdate(getChunkId(Vector3(xx,y,zz)));
            }
        }
    }
    return haveLadder;
}

bool CraftWorld::DestroyAroundTrapdoors(const int x, const int y, const int z)
{
    auto dropIf = [&](int nx, int ny, int nz, int idA, int idB)
    {
        if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
            return;
        block_t& blk = GetBlock(nx, ny, nz);
        if(blk == idA || blk == idB)
        {
            blk = 0;
            DropThis(TrapdoorClosed1::getID(), Vector3(nx, ny, nz));
            if(!ChunksEqual(Vector3(nx, ny, nz), Vector3(x, y, z)))
                AddChunkToUpdate(getChunkId(Vector3(nx, ny, nz)));
        }
    };

    dropIf(x, y, z+1, 153, 149);
    dropIf(x, y, z-1, 154, 150);
    dropIf(x+1, y, z, 151, 147);
    dropIf(x-1, y, z, 152, 148);
    return false;
}

bool CraftWorld::DestroyAroundItemFrames(const int x, const int y, const int z)
{
    auto dropIf = [&](int nx, int ny, int nz, int frameId)
    {
        if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
            return;
        block_t& blk = GetBlock(nx, ny, nz);
        if(blk == frameId)
        {
            blk = 0;
            DestroyItemFrameEntity(Vector3(nx, ny, nz));
            if(!ChunksEqual(Vector3(nx, ny, nz), Vector3(x, y, z)))
                AddChunkToUpdate(getChunkId(Vector3(nx, ny, nz)));
        }
    };

    dropIf(x, y, z+1, ItemFrame3::getID());
    dropIf(x, y, z-1, ItemFrame4::getID());
    dropIf(x+1, y, z, ItemFrame1::getID());
    dropIf(x-1, y, z, ItemFrame2::getID());
    return false;
}



static int sLightTravelCalls = 0;
static int sSkipQueuedRebuilds = 0;

void CraftWorld::SetLigtSourcePosition(const int x, const int y, const int z,int blockID) // Установить источник света
{
    sLightTravelCalls = 0;
    LightFlood(x,y,z,11);
    // `sLightTravelCalls` is the flood's own visit counter, reset immediately above, so after the
    // call it is exactly the number of cells this source's glow touched -- which is the cost a
    // re-flood of an already-settled source should be 1 for, and now is.
    MC_PERF_INC(kLightFloods);
    MC_PERF_ADD(kLightFloodCalls, sLightTravelCalls);
}

void CraftWorld::RemoveLigtSourceAtPosition(const int x, const int y, const int z,int blockID) // Выпилить источник света
{
    MC_PERF_INC(kLightRemoves);
    int cells = 0;
    for(int zz = z-9; zz <= z+9; zz++)
    {
        int area_size = 9-abs(zz-z);
        for(int xx = x-area_size; xx <= x+area_size; xx++)
        {
            for(int yy = y-area_size; yy <= y+area_size; yy++)
            {
                cells++;
                if (xx >= 0 && yy >= 0 && zz >= 0 && xx < worldLimit && yy < WORLD_HEIGHT && zz < worldLimit)
                {
                    // Two cheap outs that together skip most of this box, which a torch flip walks in full.
                    // shouldBe <= 0 means the cell is outside the reach of an 11 level source, and
                    // current == 0 means there is nothing to pull back. The original write in both cases was
                    // SetLightLevel(..., 0) onto a cell that is already 0, so skipping it changes nothing.
                    int shouldBe = 11-(TaxicabDistance2d(xx,zz,x,z)+abs(yy-y));
                    if(shouldBe <= 0)
                        continue;

                    int current = GetLightLevel(xx,yy,zz);
                    if(current > 0 && current <= shouldBe)
                    {
                        SetLightLevel(xx,yy,zz,0);
                    }
                }
            }
        }
    }
    MC_PERF_ADD(kLightRemoveCells, cells);
}

// Light propagation, in light order.
//
// This was a plain depth-first recursion, and the two things wrong with that are the same thing:
//
//   * **It re-expands cells.** A cell is written at the first level that reaches it and expanded
//     from there; when a shorter path arrives later with a higher level the cell is written again
//     AND expanded again. In open air that is rare. In the nether it is the norm -- the terrain is
//     dense, so a cell is commonly reached first by a long way round. The perf probe measured one
//     source at the 4096 visit cap (`floodCalls` averaged 4093 of 4096), which is several times the
//     number of cells an 11-level source can actually light: most of the work was revisits.
//   * **It therefore had to be capped, and the cap truncated the light.** `sLightTravelCalls > 4096`
//     stops the walk mid-flood, so the far side of a dense area never got lit -- and that is
//     self-perpetuating: the region stays genuinely dark, so the next flood from the same source
//     walks the whole thing again and truncates again. That is the nether's measured
//     `floods=48 floodCalls=196479` in one second, and the ~180 ms frame where they landed together.
//
// A FIFO frontier fixes both. Children are pushed at `level - 1` and popped in insertion order, so
// the frontier is walked in non-increasing light order -- level 11 first, then 10, and so on. That
// makes the first value written to a cell its brightest possible one, so every cell is written once
// and expanded once: the cost becomes the number of cells the source actually lights (a few hundred
// to ~1200 for an 11-level source) rather than the number of times the walk re-reaches them.
//
// The cutoff is the old one, kept exactly. `LightTravel` was entered as (steps=10, level=11) and its
// guard was `steps > 0 && level > 0`, and steps tracked level - 1 -- so it stopped one step short and
// wrote levels 11 down to 2. Level 1 cells were never lit. That is preserved here (a node below
// level 2 is dropped, and neighbours are only pushed when the node is at level 3 or above) so this
// change alters where light stops at all: it only stops the truncation.
//
// The frontier is a fixed array in BSS rather than something allocated per flood. 4096 entries of 7
// bytes is 28 KB.
//
// It is a RING, and the capacity is measured against what is *pending* rather than against the total
// number of pushes. That distinction is the whole reason for the ring: a linear buffer that only
// advances its tail would have to hold every push the flood ever makes, and that number is not the
// number of cells -- an expanded node pushes up to six neighbours, most of which are already lit and
// are rejected when they come back off the front. A full 11-level flood writes ~1159 cells, of which
// the 833 above level 2 push six each, so a linear buffer sees ~5000 pushes where the pending
// frontier never exceeds the few hundred cells at one light level. Sizing to 8192 against 5000 leaves
// only 1.6x headroom on a number that is awkward to bound; sizing a ring to the same 4096 leaves two
// orders of magnitude, because the queue is drained as it is filled.
//
// Overflow is still counted rather than assumed away, and now it is reported (see
// kLightFrontierOverflow in PerfCounters.h) instead of only being incremented. It would mean the same
// truncation the old 4096-visit cap caused, i.e. the nether's original complaint, so it must be
// visible if it ever happens rather than silently reproducing the bug this replaced.
struct LightFrontierNode
{
    short x, y, z;
    unsigned char level;
};
static const int kLightFrontierSize = 4096;
static LightFrontierNode sLightFrontier[kLightFrontierSize];
static int sLightFrontierHead = 0;
static int sLightFrontierTail = 0;
static int sLightFrontierCount = 0;
static int sLightFrontierOverflow = 0;

static void LightFloodPush(short x, short y, short z, int level)
{
    if(sLightFrontierCount >= kLightFrontierSize)
    {
        sLightFrontierOverflow++;
        return;
    }
    LightFrontierNode& n = sLightFrontier[sLightFrontierTail];
    n.x = x;
    n.y = y;
    n.z = z;
    n.level = (unsigned char)level;
    sLightFrontierTail++;
    if(sLightFrontierTail >= kLightFrontierSize)
        sLightFrontierTail = 0;
    sLightFrontierCount++;
}

void CraftWorld::LightFlood(int x,int y,int z,int lightLevel)
{
    sLightFrontierHead = 0;
    sLightFrontierTail = 0;
    sLightFrontierCount = 0;
    sLightFrontierOverflow = 0;
    LightFloodPush((short)x, (short)y, (short)z, lightLevel);

    while(sLightFrontierCount > 0)
    {
        // Copied, not referenced: the pushes below write at the tail, and the tail can reach the head
        // once the buffer wraps. A reference would be overwritten by the node's own children.
        const LightFrontierNode node = sLightFrontier[sLightFrontierHead];
        sLightFrontierHead++;
        if(sLightFrontierHead >= kLightFrontierSize)
            sLightFrontierHead = 0;
        sLightFrontierCount--;
        const int nx = node.x;
        const int ny = node.y;
        const int nz = node.z;
        const int level = node.level;

        if(level < 2)
            continue;   // one step short of the source's reach, as the old guard was
        if (nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
            continue;

        sLightTravelCalls++;

        if(level < 11)
        {
            if(LightSourceBlock(GetBlockNoCheck(nx,ny,nz)))
            {
                continue;
            }
        }

        if(GetLightLevelNoCheck(nx,ny,nz) >= level)
            continue;   // already at least this bright: nothing to write and nothing to spread

        // THE WALL. Stop where the light cannot be stored, rather than writing and spreading from a
        // cell that will still read as unlit next time it is looked at.
        //
        // This is the difference between a bounded walk and an unbounded one, and it was measured:
        // the first run of this code without it produced `floodCalls=1031013 frontierOverflow=3817454`
        // per second and a 4,035 ms frame, against the 196,479 / ~180 ms of the capped depth-first
        // search it replaced. The cause is that outside the resident columns the write is a no-op
        // while the read still answers 0, so the cell is re-expanded on every path that reaches it and
        // the flood degrades from ~1159 cells into enumerating the paths between them -- about 79,000
        // pushes per source, most of them dropped, so the light still never settled and the 48
        // floods/s never stopped.
        //
        // Light cannot usefully travel through a column that is not there: the cells on the far side
        // are exactly as unlit, so nothing is lost by stopping. RelightSharedEdges already owns putting
        // the boundary right when a column does arrive.
        if(SetLightLevelNoCheck(nx,ny,nz,level) == false)
            continue;

        if(level <= 2)
            continue;   // children would be level 1, which the old guard never wrote either

        //go futher
        if(BlockAllowLight(nx+1,ny,nz) == true)
        {
            LightFloodPush(nx+1,ny,nz,level-1);
        }
        if(BlockAllowLight(nx-1,ny,nz) == true)
        {
            LightFloodPush(nx-1,ny,nz,level-1);
        }
        if(BlockAllowLight(nx,ny+1,nz) == true)
        {
            LightFloodPush(nx,ny+1,nz,level-1);
        }
        if(BlockAllowLight(nx,ny-1,nz) == true)
        {
            LightFloodPush(nx,ny-1,nz,level-1);
        }
        if(BlockAllowLight(nx,ny,nz+1) == true)
        {
            LightFloodPush(nx,ny,nz+1,level-1);
        }
        if(BlockAllowLight(nx,ny,nz-1) == true)
        {
            LightFloodPush(nx,ny,nz-1,level-1);
        }
    }

    // Reported, not just counted, and reported here rather than by the caller because the counter is
    // reset per flood and lives below SetLigtSourcePosition in this file. A non-zero value is light
    // this flood gave up on -- exactly the nether's original "stays dark, so every later update
    // re-floods it" failure. The ring makes it unreachable by construction, so this is the alarm for
    // that assumption rather than an expected number; if it ever moves off zero the perf line says so
    // instead of the old bug quietly returning.
    MC_PERF_ADD(kLightFrontierOverflow, sLightFrontierOverflow);
}

bool CraftWorld::CanPutBlockHere(const int x, const int y, const int z,int blockID)
{
    if (BlockSolid(x-1,y,z) == true || BlockSolid(x+1,y,z) == true || BlockSolid(x,y,z-1) == true || BlockSolid(x,y,z+1) == true || BlockSolid(x,y-1,z) == true || (BlockSolid(x,y+1,z) == true && blockID != TorchItem::getID()))
    {
        return true;
    }
    // Java: redstone dust is a non-solid block that you can still build on top of, so a block placed
    // directly over a wire counts as supported. Dust is not solid, which is exactly why this used to
    // fail here: the target cell's only neighbour was the wire itself, so the check above found
    // nothing and the placement was silently dropped.
    // Only plain blocks get the allowance. Dust in hand is an item, not a block, and its own path
    // still insists on a solid block underneath, so wire can never be stacked on top of wire.
    if(y > 0 && IsBlockId(blockID) && GetBlock(x,y-1,z) == Redstoneoff::getID())
    {
        return true;
    }
    return false;
}

bool CraftWorld::BlockTransparentOrSpecial(const int x, const int y, const int z)
{
    if (x < 0 || y < 0 || z < 0  || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit) return true;

    block_t id = _GetBlock(x,y,z);
    if(blockTypes[id].transparent || blockTypes[id].blockModel > 0)
        return true;

    return false;
}

bool CraftWorld::BlockTransparentOrLightSource(const int x, const int y, const int z)
{
    if(meshLightActive)
    {
        int lx = x - meshLightOriginX;
        int ly = y - meshLightOriginY;
        int lz = z - meshLightOriginZ;
        if((unsigned)lx < (unsigned)meshLightDimX && (unsigned)ly < (unsigned)meshLightDimY && (unsigned)lz < (unsigned)meshLightDimZ)
        {
            // out-of-world cells were snapshotted as id 0 (transparent), matching the
            // "true" the bounds check below returns
            return (meshFlags[lx + meshLightDimX * (ly + meshLightDimY * lz)] & (MeshFlagTransparent | MeshFlagLightSource | MeshFlagRefraction2)) != 0;
        }
    }
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        block_t id = _GetBlock(x,y,z);
        if(blockTypes[id].transparent || blockTypes[id].lightSource || blockTypes[id].refraction == 2)
            return true;

        return false;
    }
    return true;
}

bool CraftWorld::BlockTransparentOrLightSourceNoCheck(const int x, const int y, const int z)
{
    if(meshLightActive)
    {
        int lx = x - meshLightOriginX;
        int ly = y - meshLightOriginY;
        int lz = z - meshLightOriginZ;
        if((unsigned)lx < (unsigned)meshLightDimX && (unsigned)ly < (unsigned)meshLightDimY && (unsigned)lz < (unsigned)meshLightDimZ)
            return (meshFlags[lx + meshLightDimX * (ly + meshLightDimY * lz)] & (MeshFlagTransparent | MeshFlagLightSource | MeshFlagRefraction2)) != 0;
    }
    block_t id = _GetBlockNoCheck(x,y,z);
    if(blockTypes[id].transparent || blockTypes[id].lightSource || blockTypes[id].refraction == 2)
    {
        return true;
    }

    return false;
}

bool CraftWorld::BlockTransparent(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        return blockTypes[_GetBlock(x,y,z)].transparent;
    }
    return true;
}

bool CraftWorld::BlockTransparentNoCheck(const int x, const int y, const int z)
{
    return blockTypes[_GetBlockNoCheck(x,y,z)].transparent;
}

bool CraftWorld::BlockAnimated(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        return blockTypes[_GetBlock(x,y,z)].animated;
    }
    return false;
}

// Light a nether portal if this cell sits inside a valid obsidian frame.
//
// Java's minimum is a 2 x 3 interior -- a 4 x 5 obsidian ring -- and larger rings are allowed, up to
// 21 x 21. That "larger" is the part worth being explicit about: the interior is *measured* here, never
// assumed to be 2 x 3, so any closed rectangular ring inside those limits lights.
//
// A frame is planar, so the interior is a rectangle in one of the two vertical planes; both are tried
// and the first that validates wins. Validation is the four runs of the ring -- floor, ceiling and the
// two side columns, with the floor and ceiling reaching out to cover the corner columns. Those four
// runs closing around the measured rectangle is exactly Java's shape test, and it is what stops an L of
// obsidian or a lone wall from lighting.
bool CraftWorld::TryLightPortal(int x, int y, int z)
{
    const block_t obsidian = Obsidian::getID();

    auto inBounds = [&](int bx, int by, int bz) -> bool
    {
        return bx >= 0 && bx < worldLimit && bz >= 0 && bz < worldLimit && by >= 0 && by < WORLD_HEIGHT;
    };
    // Open means the cell could be part of the interior: air, or portal already there.
    auto open = [&](int bx, int by, int bz) -> bool
    {
        if(!inBounds(bx, by, bz))
            return false;
        block_t b = GetBlock(bx, by, bz);
        return b == 0 || IsPortalBlock(b);
    };
    // Out of bounds fails every frame check, which is what keeps a frame from being lit across the
    // world edge where the ring would run off into nothing.
    auto isObsidian = [&](int bx, int by, int bz) -> bool
    {
        return inBounds(bx, by, bz) && GetBlock(bx, by, bz) == obsidian;
    };

    // The player aims at the frame, so the clicked cell is usually the air just inside it -- but
    // clicking the obsidian itself should work too. Hence the click cell first, then its neighbours.
    const int offsets[7][3] = { {0,0,0}, {-1,0,0}, {1,0,0}, {0,0,-1}, {0,0,1}, {0,-1,0}, {0,1,0} };

    for(int c = 0; c < 7; c++)
    {
        const int ax = x + offsets[c][0];
        const int ay = y + offsets[c][1];
        const int az = z + offsets[c][2];
        if(!open(ax, ay, az))
            continue;

        for(int axis = 0; axis < 2; axis++)
        {
            const int ux = (axis == 0) ? 1 : 0;   // the in-plane horizontal direction
            const int uz = (axis == 0) ? 0 : 1;

            // How far the opening runs each way along u, measured on the anchor's own row.
            int nMin = 0;
            int nMax = 0;
            while(open(ax - ux * (nMin + 1), ay, az - uz * (nMin + 1))) nMin++;
            while(open(ax + ux * (nMax + 1), ay, az + uz * (nMax + 1))) nMax++;
            const int width = nMin + nMax + 1;
            if(width < 2)
                continue;   // a 2-wide interior is the minimum; one wide is a doorway, not a portal
            // The cells just past each end must be the frame's side columns.
            if(!isObsidian(ax - ux * (nMin + 1), ay, az - uz * (nMin + 1)))
                continue;
            if(!isObsidian(ax + ux * (nMax + 1), ay, az + uz * (nMax + 1)))
                continue;

            // Vertical extent. A row only counts if the whole measured width is open on it, so a
            // ceiling that comes down over part of the opening ends the interior there.
            int dMin = 0;
            int dMax = 0;
            bool grew = true;
            while(grew)
            {
                grew = false;
                const int yy = ay - (dMin + 1);
                if(inBounds(ax, yy, az))
                {
                    bool rowOpen = true;
                    for(int u = -nMin; u <= nMax && rowOpen; u++)
                        rowOpen = open(ax + ux * u, yy, az + uz * u);
                    if(rowOpen) { dMin++; grew = true; }
                }
            }
            grew = true;
            while(grew)
            {
                grew = false;
                const int yy = ay + (dMax + 1);
                if(inBounds(ax, yy, az))
                {
                    bool rowOpen = true;
                    for(int u = -nMin; u <= nMax && rowOpen; u++)
                        rowOpen = open(ax + ux * u, yy, az + uz * u);
                    if(rowOpen) { dMax++; grew = true; }
                }
            }
            const int height = dMin + dMax + 1;
            if(height < 3)
                continue;   // three tall is the minimum
            if(width > 21 || height > 21)
                continue;   // Java's upper bound

            // Only reached when a 2x3-or-larger open rectangle bounded by obsidian on both sides has
            // already been measured, so this cannot fire for an ordinary click on terrain. If a frame
            // is built and this line never appears, the search is not finding the ring at all.
            Aurora::Utils::Logger::Instance()->LogMessage("portal: candidate anchor=(%d,%d,%d) axis=%d w=%d h=%d\n",
                ax, ay, az, axis, width, height);

            // The ring. Floor and ceiling span the interior *plus* one cell each way, which is what
            // makes the two corner columns part of the same run rather than a separate check.
            const int bottom = ay - dMin - 1;
            const int top = ay + dMax + 1;
            bool framed = true;
            for(int u = -nMin - 1; u <= nMax + 1 && framed; u++)
                framed = isObsidian(ax + ux * u, bottom, az + uz * u);
            for(int u = -nMin - 1; u <= nMax + 1 && framed; u++)
                framed = isObsidian(ax + ux * u, top, az + uz * u);
            for(int yy = ay - dMin; yy <= ay + dMax && framed; yy++)
                framed = isObsidian(ax - ux * (nMin + 1), yy, az - uz * (nMin + 1));
            for(int yy = ay - dMin; yy <= ay + dMax && framed; yy++)
                framed = isObsidian(ax + ux * (nMax + 1), yy, az + uz * (nMax + 1));
            if(!framed)
            {
                Aurora::Utils::Logger::Instance()->LogMessage("portal: ring incomplete, bottom=%d top=%d\n", bottom, top);
                continue;
            }

            Aurora::Utils::Logger::Instance()->LogMessage("portal: LIT interior %dx%d at anchor=(%d,%d,%d) axis=%d\n",
                width, height, ax, ay, az, axis);

            // It is a portal. Fill the measured interior, with the id for this plane so the sheet the
            // mesher draws lies in the frame rather than across it. axis 0 runs along X, axis 1 along Z.
            const block_t fill = (axis == 0) ? PortalBlock::getID() : PortalBlockZ::getID();
            for(int u = -nMin; u <= nMax; u++)
            {
                for(int yy = ay - dMin; yy <= ay + dMax; yy++)
                    GetBlock(ax + ux * u, yy, az + uz * u) = fill;
            }
            return true;
        }
    }
    return false;
}

char CraftWorld::BlockSpecial(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        return blockTypes[_GetBlock(x,y,z)].blockModel;
    }
    return 0;
}

char CraftWorld::BlockSpecialNoCheck(const int x, const int y, const int z)
{
    return blockTypes[_GetBlockNoCheck(x,y,z)].blockModel;
}

bool CraftWorld::BlockUpdate2(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        return blockTypes[_GetBlock(x,y,z)].update;
    }
    return false;
}

bool CraftWorld::BlockSolid(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        unsigned int id = _GetBlock(x,y,z);
        if(id >= blockTypes.size())
            return false;
        return blockTypes[id].solid;
    }
    return false;
}

short CraftWorld::BlockLoot(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        return blockTypes[_GetBlock(x,y,z)].loot;
    }
    return -1;
}

short CraftWorld::BlockMaterial(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        return blockTypes[_GetBlock(x,y,z)].material;
    }
    return 0;
}

bool CraftWorld::BlockEditable(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        return blockTypes[_GetBlock(x,y,z)].editable;
    }
    return 0;
}


bool CraftWorld::BlockAllowLight(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        return blockTypes[_GetBlock(x,y,z)].allowLight;
    }
    return 0;
}

char CraftWorld::BlockRefraction(const int x, const int y, const int z)
{
    if (x >= 0 && x < worldLimit && z >= 0 && z < worldLimit && y >= 0 && y < WORLD_HEIGHT)
    {
        return blockTypes[_GetBlock(x,y,z)].refraction;
    }
    return 0;
}

float CraftWorld::BlockFinalLight(int x, int y, int z)
{
    if(meshLightActive)
    {
        int lx = x - meshLightOriginX;
        int ly = y - meshLightOriginY;
        int lz = z - meshLightOriginZ;
        if((unsigned)lx < (unsigned)meshLightDimX && (unsigned)ly < (unsigned)meshLightDimY && (unsigned)lz < (unsigned)meshLightDimZ)
            return meshLight[lx + meshLightDimX * (ly + meshLightDimY * lz)];
    }
    float Light = 0.0f;

    // Lava draws at full brightness regardless of the light data. The reasoning is on
    // BlockFinalLightNoCheck, which fills the mesh cache and so has to agree with this.
    if(x >= 0 && y >= 0 && z >= 0 && x < worldLimit && y < WORLD_HEIGHT && z < worldLimit
       && BlockIsLava(_GetBlock(x,y,z)))
    {
        return 1.0f;
    }

    int shadowLevel = GetShadowLevel(x,y,z);
    int lightLevel = GetLightLevel(x,y,z);

    bool inShadow = false;

    if(shadowLevel > 0)
    {
        inShadow = true;
    }

    if(lightLevel > 0)//block is lightened
    {
        float lightened = (lightLevel+1) / 15.0f;
        float normal = 0.0f;

        inShadow == true ? normal = lightColor.x*(1.0f-((float)shadowLevel/15.0f)) : normal = lightColor.x;

        if(lightened > normal)
        {
            Light = lightened;
        }
        else
        {
            Light = normal;
        }
    }
    else
    {
        inShadow == true ? Light = lightColor.x*(1.0-(float)(shadowLevel/15.0f)) : Light = lightColor.x;
    }

    // The nether has no sky at all: its bedrock ceiling makes the shadow level saturate to 15 within
    // about fifteen blocks of the top, so without this every face in the dimension falls to the 0.1
    // floor below and it renders black. It is not a cave, though -- the nether has a constant dim
    // ambient of its own. Deliberately constant rather than routed through lightColor: the nether has
    // no day or night, and taking the colour from there would make it brighten and dim with the
    // overworld's clock. Block light still adds on top, so lava and glowstone read as light sources.
    if(dimension == 1 && Light < kNetherAmbientLight)
        Light = kNetherAmbientLight;

    if(Light < 0.1)
    {
        return 0.1;
    }

    return Light;

    return 0.1;
}

float CraftWorld::BlockFinalLightNoCheck(int x, int y, int z)
{
    if(meshLightActive)
    {
        int lx = x - meshLightOriginX;
        int ly = y - meshLightOriginY;
        int lz = z - meshLightOriginZ;
        if((unsigned)lx < (unsigned)meshLightDimX && (unsigned)ly < (unsigned)meshLightDimY && (unsigned)lz < (unsigned)meshLightDimZ)
            return meshLight[lx + meshLightDimX * (ly + meshLightDimY * lz)];
    }
    float Light = 0.0f;

    // Lava is emissive: a lava cell is drawn at full brightness whatever the light data says.
    //
    // That is the same picture vanilla arrives at -- a lava block has a light level of 15, so the value
    // read at its own cell is 15 and the fluid is rendered at full brightness -- but it cannot be
    // reached through the light data here. LightTravel returns the moment it steps into a light-source
    // block with a level below 11, and lava is a light source, so a seed lights its own cell and the air
    // above it but can never light the next lava block along. Seeding the sea therefore lit isolated
    // cells and left every other one at the 0.4 ambient, which is exactly what the little bright squares
    // spread over the surface were.
    //
    // Reading the block itself makes the whole sea glow with no flood at all, and it lights the
    // netherrack touching the sea for free, because a face at the shore samples this function for its
    // neighbour. It applies in both dimensions, since overworld lava is the same block and should read
    // the same way.
    if(x >= 0 && y >= 0 && z >= 0 && x < worldLimit && y < WORLD_HEIGHT && z < worldLimit
       && BlockIsLava(_GetBlock(x,y,z)))
    {
        return 1.0f;
    }

    int shadowLevel = GetShadowLevelNoCheck(x,y,z);
    int lightLevel = GetLightLevelNoCheck(x,y,z);

    bool inShadow = false;

    if(shadowLevel > 0)
    {
        inShadow = true;
    }

    if(lightLevel > 0)//block is lightened
    {
        float lightened = (lightLevel+1) / 15.0f;
        float normal = 0.0f;

        inShadow == true ? normal = lightColor.x*(1.0f-((float)shadowLevel/15.0f)) : normal = lightColor.x;

        if(lightened > normal)
        {
            Light = lightened;
        }
        else
        {
            Light = normal;
        }
    }
    else
    {
        inShadow == true ? Light = lightColor.x*(1.0-(float)(shadowLevel/15.0f)) : Light = lightColor.x;
    }

    // Same floor as BlockFinalLight: the mesh light cache is filled from this function, so the two
    // have to agree or the nether would look one way and measure another.
    if(dimension == 1 && Light < kNetherAmbientLight)
        Light = kNetherAmbientLight;

    if(Light < 0.1)
    {
        return 0.1;
    }

    return Light;

    return 0.1;
}

string CraftWorld::NameBlock(int id)
{
    if(IsBlockId(id))
    {
        if(id >= 0 && id < (int)blockTypes.size())
        {
            return blockTypes[id].name;
        }
    }
    else
    {
        id -= kItemIdBase;
        if(id >= 0 && id < itemTypes.size())
        {
            return itemTypes[id].name;
        }
    }
    return "null";
}

bool CraftWorld::LightSourceBlock(int id)
{
    // 254..2047 are item ids. Wall torches sit at 250-253 and stay light blocks.
    if(id >= 254 && id < kExtendedBlockBase)
        return false;
    if(id < (int)blockTypes.size() && id >= 0)
        return blockTypes[id].lightSource;
    return false;
}


int CraftWorld::LootBlock(int id)
{
    if(id >= 254 && id < kExtendedBlockBase)
        return 0;
    if(id < (int)blockTypes.size() && id >= 0)
        return blockTypes[id].loot;
    return 0;
}

int CraftWorld::DurabilityPointsItem(int id)
{
    id -= 250; // it is item
    if(id < itemTypes.size() && id >= 0)
    {
        return itemTypes[id].durabilityPoints;
    }
    return -1;
}

// The amount field of an inventory slot means one of two things depending on the item. For an
// ordinary item it is a stack size. For a tool it is the *remaining durability* -- DurabilityPointsItem
// is the maximum, not the current value -- which is why a worn pickaxe can sit in a slot with an
// amount of 7 and still be a single pickaxe.
//
// So anything that creates a tool rather than moving one has to seed invAm with the maximum. Handing
// a creation path a literal 1 gives it a tool that breaks on its first use, and because 1 is also a
// perfectly legal durability for a nearly-spent tool, the mistake looks like a design decision rather
// than a bug. The bow used to arrive this way from four separate directions (its crafting recipe, a
// skeleton's drop, /give, and the hotbar drop), which is what this exists to stop.
//
// Returns the amount a slot should hold for a brand-new, single instance of `id`: the full durability
// for a tool, 1 for everything else. PutInInventory's `num` argument is exactly this value for a
// non-stacking item, so callers can pass the result straight through.
int CraftWorld::FreshItemAmount(int id)
{
    const int durability = DurabilityPointsItem(id);
    if(durability > 0)
        return durability;
    return 1;
}

bool CraftWorld::StackableItem(int id)
{
    id -= 250; // it is item
    if(id < itemTypes.size() && id >= 0)
    {
        return itemTypes[id].stackable;
    }
    return false;
}


bool CraftWorld::ItemHaveTerrainTexture(int id)
{
    id -= 250; // it is item
    if(id < itemTypes.size() && id >= 0)
    {
        return itemTypes[id].terrainTexture;
    }
    return false;
}

char CraftWorld::ItemType(int id)
{
    id -= 250; // it is item
    if(id < itemTypes.size() && id >= 0)
    {
        return itemTypes[id].itemType;
    }
    return '0';
}

void CraftWorld::GetSpecialBlockVerts(int i,BaseBlock* blockType) // Строим меш инвентарных блоков, имеющих особую модель
{
    iVertexLeftRight = 0;
    pointsLeftRight = 0;
    iVertexBackFront = 0;
    pointsBackFront = 0;
    iVertexBottomTop = 0;
    pointsBottomTop = 0;

    if(blockType->blockModel == 5) //not common model
    {
        if(i == 6 || i == 134 || i == 135 || i == 136 || (i >= 87 && i <= 90))
        {
            BuildGUIBlockPlane(blockType,1,Vector3(0,0,16),Vector3(0,16,16),Vector3(0,16,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,2,Vector3(16,0,0),Vector3(16,16,0),Vector3(16,16,16),Vector3(16,0,16));
            BuildGUIBlockPlane(blockType,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
            BuildGUIBlockPlane(blockType,4,Vector3(0,16,16),Vector3(16,16,16),Vector3(16,16,0),Vector3(0,16,0));
            BuildGUIBlockPlane(blockType,5,Vector3(0,16,0),Vector3(16,16,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,16,16),Vector3(0,16,16));
        }

        if(i >= BlackWoolCarpet::getID() && i <= WhiteWoolCarpet::getID())
        {
            BuildGUIBlockPlane(blockType,1,Vector3(0,0,16),Vector3(0,1,16),Vector3(0,1,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,2,Vector3(16,0,0),Vector3(16,1,0),Vector3(16,1,16),Vector3(16,0,16));
            BuildGUIBlockPlane(blockType,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
            BuildGUIBlockPlane(blockType,4,Vector3(0,1,16),Vector3(16,1,16),Vector3(16,1,0),Vector3(0,1,0));
            BuildGUIBlockPlane(blockType,5,Vector3(0,1,0),Vector3(16,1,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,1,16),Vector3(0,1,16));
        }

        if(i == TrapdoorClosed1::getID())
        {
            BuildGUIBlockPlane(blockType,1,Vector3(0,0,16),Vector3(0,3,16),Vector3(0,3,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,2,Vector3(16,0,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(16,0,16));
            BuildGUIBlockPlane(blockType,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
            BuildGUIBlockPlane(blockType,4,Vector3(0,3,16),Vector3(16,3,16),Vector3(16,3,0),Vector3(0,3,0));
            BuildGUIBlockPlane(blockType,5,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,3,16),Vector3(0,3,16));
        }

        if(i == Snow2::getID())
        {
            BuildGUIBlockPlane(blockType,1,Vector3(0,0,16),Vector3(0,2,16),Vector3(0,2,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,2,Vector3(16,0,0),Vector3(16,2,0),Vector3(16,2,16),Vector3(16,0,16));
            BuildGUIBlockPlane(blockType,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
            BuildGUIBlockPlane(blockType,4,Vector3(0,2,16),Vector3(16,2,16),Vector3(16,2,0),Vector3(0,2,0));
            BuildGUIBlockPlane(blockType,5,Vector3(0,2,0),Vector3(16,2,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,2,16),Vector3(0,2,16));
        }

        if(i == Torch::getID())
        {
            BuildGUIBlockPlane(blockType,1,Vector3(7,0,9),Vector3(7,10,9),Vector3(7,10,7),Vector3(7,0,7));
            BuildGUIBlockPlane(blockType,2,Vector3(10,0,7),Vector3(9,10,7),Vector3(9,10,9),Vector3(9,0,9));
            BuildGUIBlockPlane(blockType,3,Vector3(7,0,7),Vector3(9,0,7),Vector3(9,0,9),Vector3(7,0,10));
            BuildGUIBlockPlane(blockType,4,Vector3(7,10,9),Vector3(9,10,9),Vector3(9,10,7),Vector3(7,10,7));
            BuildGUIBlockPlane(blockType,5,Vector3(7,10,7),Vector3(9,10,7),Vector3(9,0,7),Vector3(7,0,7));
            BuildGUIBlockPlane(blockType,6,Vector3(7,0,9),Vector3(9,0,9),Vector3(9,10,9),Vector3(7,10,9));
        }

        if(i == 112 || i == LavaBlock1::getID() || i == 9 || i == 4 || i == 37 || i == 38 || (i >= WaterBlock1::getID() && i <= WaterBlock4::getID()) || (i >= WaterBlock5::getID() && i <= WaterBlock7::getID()))
        {
            BuildGUIBlockPlane(blockType,1,Vector3(0,0,16),Vector3(0,16,16),Vector3(0,16,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,2,Vector3(16,0,0),Vector3(16,16,0),Vector3(16,16,16),Vector3(16,0,16));
            BuildGUIBlockPlane(blockType,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
            BuildGUIBlockPlane(blockType,4,Vector3(0,16,16),Vector3(16,16,16),Vector3(16,16,0),Vector3(0,16,0));
            BuildGUIBlockPlane(blockType,5,Vector3(0,16,0),Vector3(16,16,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildGUIBlockPlane(blockType,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,16,16),Vector3(0,16,16));
        }

        if(i == CactusBlock::getID())
        {
            BuildGUIBlockPlane(blockType,1,Vector3(1,0,16),Vector3(1,16,16),Vector3(1,16,0),Vector3(1,0,0));
            BuildGUIBlockPlane(blockType,2,Vector3(15,0,0),Vector3(15,16,0),Vector3(15,16,16),Vector3(15,0,16));
            BuildGUIBlockPlane(blockType,3,Vector3(1,0,1),Vector3(15,0,1),Vector3(15,0,15),Vector3(1,0,15));
            BuildGUIBlockPlane(blockType,4,Vector3(0,16,15),Vector3(15,16,15),Vector3(15,16,1),Vector3(1,16,1));
            BuildGUIBlockPlane(blockType,5,Vector3(0,16,1),Vector3(16,16,1),Vector3(16,0,1),Vector3(0,0,1));
            BuildGUIBlockPlane(blockType,6,Vector3(0,0,15),Vector3(16,0,15),Vector3(16,16,15),Vector3(0,16,15));
        }

        if(i == ChestBlock3::getID())
        {
            BuildGUIBlockPlane(blockType,1,Vector3(1,0,15),Vector3(1,14,15),Vector3(1,14,1),Vector3(1,0,1));
            BuildGUIBlockPlane(blockType,2,Vector3(15,0,1),Vector3(15,14,1),Vector3(15,14,15),Vector3(15,0,15));
            BuildGUIBlockPlane(blockType,3,Vector3(1,0,1),Vector3(15,0,1),Vector3(15,0,15),Vector3(1,0,15));
            BuildGUIBlockPlane(blockType,4,Vector3(1,14,15),Vector3(15,14,15),Vector3(15,14,1),Vector3(1,14,1));
            BuildGUIBlockPlane(blockType,5,Vector3(1,14,1),Vector3(15,14,1),Vector3(15,0,1),Vector3(1,0,1));
            BuildGUIBlockPlane(blockType,6,Vector3(1,0,15),Vector3(15,0,15),Vector3(15,14,15),Vector3(1,14,15));

            blockType = &blockTypes[98];

            BuildGUIBlockPlane(blockType,1,Vector3(7,7,1),Vector3(7,11,1),Vector3(7,11,0),Vector3(7,7,0));
            BuildGUIBlockPlane(blockType,2,Vector3(9,7,0),Vector3(9,11,0),Vector3(9,11,1),Vector3(9,7,1));
            BuildGUIBlockPlane(blockType,4,Vector3(7,11,1),Vector3(9,11,1),Vector3(9,11,0),Vector3(7,11,0));
            BuildGUIBlockPlane(blockType,5,Vector3(7,11,0),Vector3(9,11,0),Vector3(9,7,0),Vector3(7,7,0));
        }
    }

    if(blockType->blockModel == 6)
    {
        BuildGUIBlockPlane(blockType,1,Vector3(0,0,16),Vector3(0,8,16),Vector3(0,8,0),Vector3(0,0,0));
        BuildGUIBlockPlane(blockType,2,Vector3(16,0,0),Vector3(16,8,0),Vector3(16,8,16),Vector3(16,0,16));
        BuildGUIBlockPlane(blockType,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
        BuildGUIBlockPlane(blockType,4,Vector3(0,8,8),Vector3(16,8,8),Vector3(16,8,0),Vector3(0,8,0));
        BuildGUIBlockPlane(blockType,5,Vector3(0,8,0),Vector3(16,8,0),Vector3(16,0,0),Vector3(0,0,0));
        BuildGUIBlockPlane(blockType,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,8,16),Vector3(0,8,16));

        BuildGUIBlockPlane(blockType,1,Vector3(0,8,16),Vector3(0,16,16),Vector3(0,16,8),Vector3(0,8,8));
        BuildGUIBlockPlane(blockType,2,Vector3(16,8,8),Vector3(16,16,8),Vector3(16,16,16),Vector3(16,8,16));
        BuildGUIBlockPlane(blockType,4,Vector3(0,16,16),Vector3(16,16,16),Vector3(16,16,8),Vector3(0,16,8));
        BuildGUIBlockPlane(blockType,5,Vector3(0,16,8),Vector3(16,16,8),Vector3(16,8,8),Vector3(0,8,8));
        BuildGUIBlockPlane(blockType,6,Vector3(0,8,16),Vector3(16,8,16),Vector3(16,16,16),Vector3(0,16,16));
    }


    if(blockType->blockModel == 2)
    {
        BuildGUIBlockPlane(blockType,1,Vector3(6,0,16),Vector3(6,16,16),Vector3(6,16,12),Vector3(6,0,12));
        BuildGUIBlockPlane(blockType,2,Vector3(10,0,12),Vector3(10,16,12),Vector3(10,16,16),Vector3(10,0,16));
        BuildGUIBlockPlane(blockType,3,Vector3(6,0,12),Vector3(10,0,12),Vector3(10,0,16),Vector3(6,0,16));
        BuildGUIBlockPlane(blockType,4,Vector3(6,16,16),Vector3(10,16,16),Vector3(10,16,12),Vector3(6,16,12));
        BuildGUIBlockPlane(blockType,5,Vector3(6,16,12),Vector3(10,16,12),Vector3(10,0,12),Vector3(6,0,12));
        BuildGUIBlockPlane(blockType,6,Vector3(6,0,16),Vector3(10,0,16),Vector3(10,16,16),Vector3(6,16,16));

        BuildGUIBlockPlane(blockType,1,Vector3(7,12,12),Vector3(7,15,12),Vector3(7,15,4),Vector3(7,12,4));
        BuildGUIBlockPlane(blockType,2,Vector3(9,12,4),Vector3(9,15,4),Vector3(9,15,12),Vector3(9,12,12));
        BuildGUIBlockPlane(blockType,3,Vector3(7,12,4),Vector3(9,12,4),Vector3(9,12,12),Vector3(7,12,12));
        BuildGUIBlockPlane(blockType,4,Vector3(7,15,12),Vector3(9,15,12),Vector3(9,15,4),Vector3(7,15,4));

        BuildGUIBlockPlane(blockType,1,Vector3(7,5,12),Vector3(7,8,12),Vector3(7,8,4),Vector3(7,5,4));
        BuildGUIBlockPlane(blockType,2,Vector3(9,5,4),Vector3(9,8,4),Vector3(9,8,12),Vector3(9,5,12));
        BuildGUIBlockPlane(blockType,3,Vector3(7,5,4),Vector3(9,5,4),Vector3(9,5,12),Vector3(7,5,12));
        BuildGUIBlockPlane(blockType,4,Vector3(7,8,12),Vector3(9,8,12),Vector3(9,8,4),Vector3(7,8,4));

        BuildGUIBlockPlane(blockType,1,Vector3(6,0,4),Vector3(6,16,4),Vector3(6,16,0),Vector3(6,0,0));
        BuildGUIBlockPlane(blockType,2,Vector3(10,0,0),Vector3(10,16,0),Vector3(10,16,4),Vector3(10,0,4));
        BuildGUIBlockPlane(blockType,3,Vector3(6,0,0),Vector3(10,0,0),Vector3(10,0,4),Vector3(6,0,4));
        BuildGUIBlockPlane(blockType,4,Vector3(6,16,4),Vector3(10,16,4),Vector3(10,16,0),Vector3(6,16,0));
        BuildGUIBlockPlane(blockType,5,Vector3(6,16,0),Vector3(10,16,0),Vector3(10,0,0),Vector3(6,0,0));
        BuildGUIBlockPlane(blockType,6,Vector3(6,0,4),Vector3(10,0,4),Vector3(10,16,4),Vector3(6,16,4));
    }

    if(blockType->blockModel == 1)
    {
        BuildGUIBlockPlane(blockType,1,Vector3(0,0,16),Vector3(0,8,16),Vector3(0,8,0),Vector3(0,0,0));
        BuildGUIBlockPlane(blockType,2,Vector3(16,0,0),Vector3(16,8,0),Vector3(16,8,16),Vector3(16,0,16));
        BuildGUIBlockPlane(blockType,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
        BuildGUIBlockPlane(blockType,4,Vector3(0,8,16),Vector3(16,8,16),Vector3(16,8,0),Vector3(0,8,0));
        BuildGUIBlockPlane(blockType,5,Vector3(0,8,0),Vector3(16,8,0),Vector3(16,0,0),Vector3(0,0,0));
        BuildGUIBlockPlane(blockType,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,8,16),Vector3(0,8,16));
    }

    if(blockType->blockModel == 14)
    {
        // Java BlockButton.setBlockBoundsForItemRender: a 6 x 4 x 4 px box centred in the cell,
        // so the inventory slot shows the button from the side rather than an empty square.
        BuildGUIBlockPlane(blockType,1,Vector3(5,6,10),Vector3(5,10,10),Vector3(5,10,6),Vector3(5,6,6));
        BuildGUIBlockPlane(blockType,2,Vector3(11,6,6),Vector3(11,10,6),Vector3(11,10,10),Vector3(11,6,10));
        BuildGUIBlockPlane(blockType,3,Vector3(5,6,6),Vector3(11,6,6),Vector3(11,6,10),Vector3(5,6,10));
        BuildGUIBlockPlane(blockType,4,Vector3(5,10,10),Vector3(11,10,10),Vector3(11,10,6),Vector3(5,10,6));
        BuildGUIBlockPlane(blockType,5,Vector3(5,10,6),Vector3(11,10,6),Vector3(11,6,6),Vector3(5,6,6));
        BuildGUIBlockPlane(blockType,6,Vector3(5,6,10),Vector3(11,6,10),Vector3(11,10,10),Vector3(5,10,10));
    }

    if(blockType->blockModel == 15)
    {
        // Java renders the repeater's item as its flat top sprite, not the 3D slab: the icon players
        // recognise is the top tile -- the smooth slab with the torch row baked into it -- seen face on.
        // Looking at it from a shallow angle, as the old 6-plane box did, turned the torch row into a
        // smudge and made the icon read as a small grey block instead of a repeater.
        BuildGUIBlockPlane(blockType,7,Vector3(0,16,2),Vector3(16,16,2),Vector3(16,0,2),Vector3(0,0,2));
    }

    if(blockType->blockModel == 16)
    {
        // The lever's inventory icon, and the one place in this function where the tile is *not* stretched
        // across the quad. BuildGUIBlockPlane ties its texture coordinates to the quad's own point
        // coordinates -- `point4.z * texturePixel` and so on -- which means a quad spanning 6 world pixels
        // shows six pixels of the tile, not the whole tile. That is usually a nuisance (see the button,
        // whose icon is a 6 x 4 px window into the stone tile) but here it is exactly what the lever needs:
        // the handle sprite is a 2 px strip at columns 7-8 spanning tile rows 6-15, so a quad running from
        // z 7 to z 9 and from y 1 to y 11 samples that strip and nothing else, at 1:1, knob at the top.
        // A stretched mapping would smear the transparent margins across the handle and it would vanish.
        //
        // Three deviations from the world model, all deliberate. The handle stands straight up instead of
        // leaning 40 degrees, because the shared builder can only map an axis-aligned face (the same trap
        // the world mesher side-steps by hand-emitting); the handle's own bottom face is left out, because
        // it sits inside the base and cannot be seen; and the base is 8 x 6 x 3 px here in the world model's
        // own orientation, which on screen reads as a plate wider than it is deep. At 16 px tall in a slot,
        // a plumb handle and a leaning one are the same icon.
        BaseBlock* cobble = &blockTypes[CobbleStone::getID()];
        BuildGUIBlockPlane(cobble,1,Vector3(4,0,11),Vector3(4,3,11),Vector3(4,3,5),Vector3(4,0,5));
        BuildGUIBlockPlane(cobble,2,Vector3(12,0,5),Vector3(12,3,5),Vector3(12,3,11),Vector3(12,0,11));
        BuildGUIBlockPlane(cobble,3,Vector3(4,0,5),Vector3(12,0,5),Vector3(12,0,11),Vector3(4,0,11));
        BuildGUIBlockPlane(cobble,4,Vector3(4,3,11),Vector3(12,3,11),Vector3(12,3,5),Vector3(4,3,5));
        BuildGUIBlockPlane(cobble,5,Vector3(4,3,5),Vector3(12,3,5),Vector3(12,0,5),Vector3(4,0,5));
        BuildGUIBlockPlane(cobble,6,Vector3(4,0,11),Vector3(12,0,11),Vector3(12,3,11),Vector3(4,3,11));

        BuildGUIBlockPlane(blockType,1,Vector3(7,1,9),Vector3(7,11,9),Vector3(7,11,7),Vector3(7,1,7));
        BuildGUIBlockPlane(blockType,2,Vector3(9,1,7),Vector3(9,11,7),Vector3(9,11,9),Vector3(9,1,9));
        BuildGUIBlockPlane(blockType,4,Vector3(7,11,9),Vector3(9,11,9),Vector3(9,11,7),Vector3(7,11,7));
        BuildGUIBlockPlane(blockType,5,Vector3(7,11,7),Vector3(9,11,7),Vector3(9,1,7),Vector3(7,1,7));
        BuildGUIBlockPlane(blockType,6,Vector3(7,1,9),Vector3(9,1,9),Vector3(9,11,9),Vector3(7,11,9));
    }

    if(blockType->blockModel == 17)
    {
        // The piston's inventory icon: a plain full cube, which is exactly how the retracted piston looks
        // in the world and how vanilla renders its item. A blockModel above 0 sends a block down this
        // function instead of GetNormalBlockVerts, so without this case a piston would have had *no*
        // inventory mesh at all -- and unlike the ladder and the flower families, a piston has no item
        // counterpart with a flat sprite to fall back on, so an empty mesh would have been an invisible
        // slot rather than a different icon.
        //
        // The six point sets are the same ones the model 5 case above uses for its cube, so the winding
        // is already known to be right. What is NOT shared with that case is the tile per face, and the
        // two disagree because of what the fields *mean* for a piston.
        //
        // BuildGUIBlockPlane picks a face's tile from the block's own field named after that face:
        // side 4 (+Y) reads upPlane, side 3 (-Y) downPlane, side 5 (-Z) botPlane, side 6 (+Z) forPlane.
        // For an ordinary block those names are the roles and everything lines up. For a piston they are
        // not: the world mesher reads upPlane as the *side* tile (`sideTileX` in GetSpecialBlock's model
        // 17 case), forPlane as the head face and botPlane as the back, because a piston's six faces are
        // not six independent materials -- four of them are the same side tile, and only the push face
        // and the back are their own. So `upPlane` holds piston_side, `forPlane` holds the head tile and
        // `botPlane` holds piston_bottom, and the GUI builder reading them face-wise put the side tile on
        // +Y, the head tile on +Z and the *back* tile on -Z.
        //
        // That is what a player saw: the icon's three visible faces are +Y (the top), +X (the right) and
        // -Z (the left), and of those the top read as plain side tile and the left as the back tile, while
        // the head tile sat on +Z facing away from the camera. The icon was a piston pointing out of the
        // slot, and what it should be is the same block a player gets when they place one facing up --
        // head plate on top, the arm band at the top edge of every flank. Verified offline with the six
        // quads this case emits: see tools/../_tmp_atlas/preview_piston_icon.py.
        //
        // So the copy below re-points the four face fields at the tiles the icon's roles call for, and the
        // six point sets stay exactly as the model 5 cube writes them. Nothing is written back to the real
        // block type: `upPlane` still holds piston_side for the world mesher, which is the whole reason
        // this cannot be fixed in InitPistonBase instead.
        BaseBlock icon = *blockType;
        icon.upPlane_x    = blockType->forPlane_x;   icon.upPlane_y    = blockType->forPlane_y;   // +Y -> head
        icon.downPlane_x  = blockType->botPlane_x;   icon.downPlane_y  = blockType->botPlane_y;   // -Y -> back
        icon.botPlane_x   = blockType->upPlane_x;    icon.botPlane_y   = blockType->upPlane_y;    // -Z -> side
        icon.forPlane_x   = blockType->upPlane_x;    icon.forPlane_y   = blockType->upPlane_y;    // +Z -> side
        // leftPlane and rightPlane are left alone: InitPistonBase already fills both with piston_side,
        // which is what the two X faces of a piston wear.

        BuildGUIBlockPlane(&icon,1,Vector3(0,0,16),Vector3(0,16,16),Vector3(0,16,0),Vector3(0,0,0));
        BuildGUIBlockPlane(&icon,2,Vector3(16,0,0),Vector3(16,16,0),Vector3(16,16,16),Vector3(16,0,16));
        BuildGUIBlockPlane(&icon,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
        BuildGUIBlockPlane(&icon,4,Vector3(0,16,16),Vector3(16,16,16),Vector3(16,16,0),Vector3(0,16,0));
        BuildGUIBlockPlane(&icon,5,Vector3(0,16,0),Vector3(16,16,0),Vector3(16,0,0),Vector3(0,0,0));
        BuildGUIBlockPlane(&icon,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,16,16),Vector3(0,16,16));
    }

    // blockModel 18 has no case here on purpose. The piston head can never reach an inventory: it drops
    // nothing when broken (loot 0, see InitPistonHead) and it is never the id a held piston carries, so an
    // empty mesh costs a slot of RAM and nothing else. The ladder, flower, wheat, stair, rail and fire
    // models are all in the same position.

    blockTypes[i].verticesLeftRight = (TexturesPSPVertex*)memalign(16,( mTriangleLeftRight.size() * 3) * sizeof(TexturesPSPVertex));
    blockTypes[i].pointsLeftRight = pointsLeftRight;
    //build verts
    //vertices
    int vert = 0;
    unsigned int size =  mTriangleLeftRight.size();
    for(unsigned int j = 0; j < size; j++)
    {
        blockTypes[i].verticesLeftRight[vert].u = mtexturesLeftRight[mTriangleLeftRight[j]->x]->x;
        blockTypes[i].verticesLeftRight[vert].v = mtexturesLeftRight[mTriangleLeftRight[j]->x]->y;
        blockTypes[i].verticesLeftRight[vert].x = mPositionLeftRight[mTriangleLeftRight[j]->x]->x;
        blockTypes[i].verticesLeftRight[vert].y = mPositionLeftRight[mTriangleLeftRight[j]->x]->y;
        blockTypes[i].verticesLeftRight[vert].z = mPositionLeftRight[mTriangleLeftRight[j]->x]->z;
        vert++;

        blockTypes[i].verticesLeftRight[vert].u = mtexturesLeftRight[mTriangleLeftRight[j]->y]->x;
        blockTypes[i].verticesLeftRight[vert].v = mtexturesLeftRight[mTriangleLeftRight[j]->y]->y;
        blockTypes[i].verticesLeftRight[vert].x = mPositionLeftRight[mTriangleLeftRight[j]->y]->x;
        blockTypes[i].verticesLeftRight[vert].y = mPositionLeftRight[mTriangleLeftRight[j]->y]->y;
        blockTypes[i].verticesLeftRight[vert].z = mPositionLeftRight[mTriangleLeftRight[j]->y]->z;
        vert++;

        blockTypes[i].verticesLeftRight[vert].u = mtexturesLeftRight[mTriangleLeftRight[j]->z]->x;
        blockTypes[i].verticesLeftRight[vert].v = mtexturesLeftRight[mTriangleLeftRight[j]->z]->y;
        blockTypes[i].verticesLeftRight[vert].x = mPositionLeftRight[mTriangleLeftRight[j]->z]->x;
        blockTypes[i].verticesLeftRight[vert].y = mPositionLeftRight[mTriangleLeftRight[j]->z]->y;
        blockTypes[i].verticesLeftRight[vert].z = mPositionLeftRight[mTriangleLeftRight[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(blockTypes[i].verticesLeftRight,(mTriangleLeftRight.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPositionLeftRight.size(); aa++)
    {
        delete mPositionLeftRight[aa];
        delete mtexturesLeftRight[aa];
    }
    mPositionLeftRight.clear();
    mtexturesLeftRight.clear();

    for(unsigned int aa = 0; aa < mTriangleLeftRight.size(); aa++)
    {
        delete mTriangleLeftRight[aa];
    }
    mTriangleLeftRight.clear();



    blockTypes[i].verticesBackFront = (TexturesPSPVertex*)memalign(16,(mTriangleBackFront.size() * 3) * sizeof(TexturesPSPVertex));
    blockTypes[i].pointsBackFront = pointsBackFront;
    //build verts
    //vertices
    vert = 0;
    size =  mTriangleBackFront.size();
    for(unsigned int j = 0; j < size; j++)
    {
        blockTypes[i].verticesBackFront[vert].u = mtexturesBackFront[mTriangleBackFront[j]->x]->x;
        blockTypes[i].verticesBackFront[vert].v = mtexturesBackFront[mTriangleBackFront[j]->x]->y;
        blockTypes[i].verticesBackFront[vert].x = mPositionBackFront[mTriangleBackFront[j]->x]->x;
        blockTypes[i].verticesBackFront[vert].y = mPositionBackFront[mTriangleBackFront[j]->x]->y;
        blockTypes[i].verticesBackFront[vert].z = mPositionBackFront[mTriangleBackFront[j]->x]->z;
        vert++;

        blockTypes[i].verticesBackFront[vert].u = mtexturesBackFront[mTriangleBackFront[j]->y]->x;
        blockTypes[i].verticesBackFront[vert].v = mtexturesBackFront[mTriangleBackFront[j]->y]->y;
        blockTypes[i].verticesBackFront[vert].x = mPositionBackFront[mTriangleBackFront[j]->y]->x;
        blockTypes[i].verticesBackFront[vert].y = mPositionBackFront[mTriangleBackFront[j]->y]->y;
        blockTypes[i].verticesBackFront[vert].z = mPositionBackFront[mTriangleBackFront[j]->y]->z;
        vert++;

        blockTypes[i].verticesBackFront[vert].u = mtexturesBackFront[mTriangleBackFront[j]->z]->x;
        blockTypes[i].verticesBackFront[vert].v = mtexturesBackFront[mTriangleBackFront[j]->z]->y;
        blockTypes[i].verticesBackFront[vert].x = mPositionBackFront[mTriangleBackFront[j]->z]->x;
        blockTypes[i].verticesBackFront[vert].y = mPositionBackFront[mTriangleBackFront[j]->z]->y;
        blockTypes[i].verticesBackFront[vert].z = mPositionBackFront[mTriangleBackFront[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(blockTypes[i].verticesBackFront,(mTriangleBackFront.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPositionBackFront.size(); aa++)
    {
        delete mPositionBackFront[aa];
        delete mtexturesBackFront[aa];
    }
    mPositionBackFront.clear();
    mtexturesBackFront.clear();

    for(unsigned int aa = 0; aa < mTriangleBackFront.size(); aa++)
    {
        delete mTriangleBackFront[aa];
    }
    mTriangleBackFront.clear();



    blockTypes[i].verticesBottomTop = (TexturesPSPVertex*)memalign(16,(mTriangleBottomTop.size() * 3) * sizeof(TexturesPSPVertex));
    blockTypes[i].pointsBottomTop = pointsBottomTop;
    //build verts
    //vertices
    vert = 0;
    size =  mTriangleBottomTop.size();
    for(unsigned int j = 0; j < size; j++)
    {
        blockTypes[i].verticesBottomTop[vert].u = mtexturesBottomTop[mTriangleBottomTop[j]->x]->x;
        blockTypes[i].verticesBottomTop[vert].v = mtexturesBottomTop[mTriangleBottomTop[j]->x]->y;
        blockTypes[i].verticesBottomTop[vert].x = mPositionBottomTop[mTriangleBottomTop[j]->x]->x;
        blockTypes[i].verticesBottomTop[vert].y = mPositionBottomTop[mTriangleBottomTop[j]->x]->y;
        blockTypes[i].verticesBottomTop[vert].z = mPositionBottomTop[mTriangleBottomTop[j]->x]->z;
        vert++;

        blockTypes[i].verticesBottomTop[vert].u = mtexturesBottomTop[mTriangleBottomTop[j]->y]->x;
        blockTypes[i].verticesBottomTop[vert].v = mtexturesBottomTop[mTriangleBottomTop[j]->y]->y;
        blockTypes[i].verticesBottomTop[vert].x = mPositionBottomTop[mTriangleBottomTop[j]->y]->x;
        blockTypes[i].verticesBottomTop[vert].y = mPositionBottomTop[mTriangleBottomTop[j]->y]->y;
        blockTypes[i].verticesBottomTop[vert].z = mPositionBottomTop[mTriangleBottomTop[j]->y]->z;
        vert++;

        blockTypes[i].verticesBottomTop[vert].u = mtexturesBottomTop[mTriangleBottomTop[j]->z]->x;
        blockTypes[i].verticesBottomTop[vert].v = mtexturesBottomTop[mTriangleBottomTop[j]->z]->y;
        blockTypes[i].verticesBottomTop[vert].x = mPositionBottomTop[mTriangleBottomTop[j]->z]->x;
        blockTypes[i].verticesBottomTop[vert].y = mPositionBottomTop[mTriangleBottomTop[j]->z]->y;
        blockTypes[i].verticesBottomTop[vert].z = mPositionBottomTop[mTriangleBottomTop[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(blockTypes[i].verticesBottomTop,(mTriangleBottomTop.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPositionBottomTop.size(); aa++)
    {
        delete mPositionBottomTop[aa];
        delete mtexturesBottomTop[aa];
    }
    mPositionBottomTop.clear();
    mtexturesBottomTop.clear();

    for(unsigned int aa = 0; aa < mTriangleBottomTop.size(); aa++)
    {
        delete mTriangleBottomTop[aa];
    }
    mTriangleBottomTop.clear();
}

void CraftWorld::GetCloudsVerts(BaseBlock *blockType)
{
    std::vector<Vector3*> mPosition;
    std::vector<Vector2*> mtextures;
    std::vector<Vector3*> mTriangle;

    int iVertex = 0;
    int points = 0;

    float x = 0;
    float y = 0;
    float z = 0;

    iVertex = 0;
    points = 0;

    for(float l = 1; l <= 10; l += 1)
    {
        for(float j = 1; j <= 10; j += 1)//0-|-
        {
            mPosition.push_back(new Vector3(x+((l-1)*48),   y, z+((j-1)*48)));
            mtextures.push_back(new Vector2(0+(l-1)/40, 0+(j-1)/10));
            mPosition.push_back(new Vector3(x+(l*48), y, z+((j-1)*48)));
            mtextures.push_back(new Vector2(l/40, 0+(j-1)/10));
            mPosition.push_back(new Vector3(x+(l*48), y, z+(j*48)));
            mtextures.push_back(new Vector2(l/40, j/10));
            mPosition.push_back(new Vector3(x+((l-1)*48),   y, z+(j*48)));
            mtextures.push_back(new Vector2(0+(l-1)/40, j/10));

            mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
            mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

            iVertex += 4;
            points += 6;
        }
    }

    blockTypes[141].verticesBottomTop = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    blockTypes[141].pointsBottomTop = points;
    //build verts
    //vertices
    int vert = 0;
    unsigned int size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        blockTypes[141].verticesBottomTop[vert].u = mtextures[mTriangle[j]->x]->x;
        blockTypes[141].verticesBottomTop[vert].v = mtextures[mTriangle[j]->x]->y;
        blockTypes[141].verticesBottomTop[vert].x = mPosition[mTriangle[j]->x]->x;
        blockTypes[141].verticesBottomTop[vert].y = mPosition[mTriangle[j]->x]->y;
        blockTypes[141].verticesBottomTop[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        blockTypes[141].verticesBottomTop[vert].u = mtextures[mTriangle[j]->y]->x;
        blockTypes[141].verticesBottomTop[vert].v = mtextures[mTriangle[j]->y]->y;
        blockTypes[141].verticesBottomTop[vert].x = mPosition[mTriangle[j]->y]->x;
        blockTypes[141].verticesBottomTop[vert].y = mPosition[mTriangle[j]->y]->y;
        blockTypes[141].verticesBottomTop[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        blockTypes[141].verticesBottomTop[vert].u = mtextures[mTriangle[j]->z]->x;
        blockTypes[141].verticesBottomTop[vert].v = mtextures[mTriangle[j]->z]->y;
        blockTypes[141].verticesBottomTop[vert].x = mPosition[mTriangle[j]->z]->x;
        blockTypes[141].verticesBottomTop[vert].y = mPosition[mTriangle[j]->z]->y;
        blockTypes[141].verticesBottomTop[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(blockTypes[141].verticesBottomTop,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
        delete mtextures[aa];
    }
        mPosition.clear();
        mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();
}

void CraftWorld::GetNormalBlockVerts(int i,BaseBlock *blockType)
{
    iVertexLeftRight = 0;
    pointsLeftRight = 0;
    iVertexBackFront = 0;
    pointsBackFront = 0;
    iVertexBottomTop = 0;
    pointsBottomTop = 0;

    BuildGUIBlockPlane(blockType,1,Vector3(0,0,16),Vector3(0,16,16),Vector3(0,16,0),Vector3(0,0,0));
    BuildGUIBlockPlane(blockType,2,Vector3(16,0,0),Vector3(16,16,0),Vector3(16,16,16),Vector3(16,0,16));
    BuildGUIBlockPlane(blockType,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
    BuildGUIBlockPlane(blockType,4,Vector3(0,16,16),Vector3(16,16,16),Vector3(16,16,0),Vector3(0,16,0));
    BuildGUIBlockPlane(blockType,5,Vector3(0,16,0),Vector3(16,16,0),Vector3(16,0,0),Vector3(0,0,0));
    BuildGUIBlockPlane(blockType,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,16,16),Vector3(0,16,16));

    blockTypes[i].verticesLeftRight = (TexturesPSPVertex*)memalign(16,( mTriangleLeftRight.size() * 3) * sizeof(TexturesPSPVertex));
    blockTypes[i].pointsLeftRight = pointsLeftRight;
    //build verts
    //vertices
    int vert = 0;
    unsigned int size =  mTriangleLeftRight.size();
    for(unsigned int j = 0; j < size; j++)
    {
        blockTypes[i].verticesLeftRight[vert].u = mtexturesLeftRight[mTriangleLeftRight[j]->x]->x;
        blockTypes[i].verticesLeftRight[vert].v = mtexturesLeftRight[mTriangleLeftRight[j]->x]->y;
        blockTypes[i].verticesLeftRight[vert].x = mPositionLeftRight[mTriangleLeftRight[j]->x]->x;
        blockTypes[i].verticesLeftRight[vert].y = mPositionLeftRight[mTriangleLeftRight[j]->x]->y;
        blockTypes[i].verticesLeftRight[vert].z = mPositionLeftRight[mTriangleLeftRight[j]->x]->z;
        vert++;

        blockTypes[i].verticesLeftRight[vert].u = mtexturesLeftRight[mTriangleLeftRight[j]->y]->x;
        blockTypes[i].verticesLeftRight[vert].v = mtexturesLeftRight[mTriangleLeftRight[j]->y]->y;
        blockTypes[i].verticesLeftRight[vert].x = mPositionLeftRight[mTriangleLeftRight[j]->y]->x;
        blockTypes[i].verticesLeftRight[vert].y = mPositionLeftRight[mTriangleLeftRight[j]->y]->y;
        blockTypes[i].verticesLeftRight[vert].z = mPositionLeftRight[mTriangleLeftRight[j]->y]->z;
        vert++;

        blockTypes[i].verticesLeftRight[vert].u = mtexturesLeftRight[mTriangleLeftRight[j]->z]->x;
        blockTypes[i].verticesLeftRight[vert].v = mtexturesLeftRight[mTriangleLeftRight[j]->z]->y;
        blockTypes[i].verticesLeftRight[vert].x = mPositionLeftRight[mTriangleLeftRight[j]->z]->x;
        blockTypes[i].verticesLeftRight[vert].y = mPositionLeftRight[mTriangleLeftRight[j]->z]->y;
        blockTypes[i].verticesLeftRight[vert].z = mPositionLeftRight[mTriangleLeftRight[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(blockTypes[i].verticesLeftRight,(mTriangleLeftRight.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPositionLeftRight.size(); aa++)
    {
        delete mPositionLeftRight[aa];
        delete mtexturesLeftRight[aa];
    }
    mPositionLeftRight.clear();
    mtexturesLeftRight.clear();

    for(unsigned int aa = 0; aa < mTriangleLeftRight.size(); aa++)
    {
        delete mTriangleLeftRight[aa];
    }
    mTriangleLeftRight.clear();



    blockTypes[i].verticesBackFront = (TexturesPSPVertex*)memalign(16,(mTriangleBackFront.size() * 3) * sizeof(TexturesPSPVertex));
    blockTypes[i].pointsBackFront = pointsBackFront;
    //build verts
    //vertices
    vert = 0;
    size =  mTriangleBackFront.size();
    for(unsigned int j = 0; j < size; j++)
    {
        blockTypes[i].verticesBackFront[vert].u = mtexturesBackFront[mTriangleBackFront[j]->x]->x;
        blockTypes[i].verticesBackFront[vert].v = mtexturesBackFront[mTriangleBackFront[j]->x]->y;
        blockTypes[i].verticesBackFront[vert].x = mPositionBackFront[mTriangleBackFront[j]->x]->x;
        blockTypes[i].verticesBackFront[vert].y = mPositionBackFront[mTriangleBackFront[j]->x]->y;
        blockTypes[i].verticesBackFront[vert].z = mPositionBackFront[mTriangleBackFront[j]->x]->z;
        vert++;

        blockTypes[i].verticesBackFront[vert].u = mtexturesBackFront[mTriangleBackFront[j]->y]->x;
        blockTypes[i].verticesBackFront[vert].v = mtexturesBackFront[mTriangleBackFront[j]->y]->y;
        blockTypes[i].verticesBackFront[vert].x = mPositionBackFront[mTriangleBackFront[j]->y]->x;
        blockTypes[i].verticesBackFront[vert].y = mPositionBackFront[mTriangleBackFront[j]->y]->y;
        blockTypes[i].verticesBackFront[vert].z = mPositionBackFront[mTriangleBackFront[j]->y]->z;
        vert++;

        blockTypes[i].verticesBackFront[vert].u = mtexturesBackFront[mTriangleBackFront[j]->z]->x;
        blockTypes[i].verticesBackFront[vert].v = mtexturesBackFront[mTriangleBackFront[j]->z]->y;
        blockTypes[i].verticesBackFront[vert].x = mPositionBackFront[mTriangleBackFront[j]->z]->x;
        blockTypes[i].verticesBackFront[vert].y = mPositionBackFront[mTriangleBackFront[j]->z]->y;
        blockTypes[i].verticesBackFront[vert].z = mPositionBackFront[mTriangleBackFront[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(blockTypes[i].verticesBackFront,(mTriangleBackFront.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPositionBackFront.size(); aa++)
    {
        delete mPositionBackFront[aa];
        delete mtexturesBackFront[aa];
    }
    mPositionBackFront.clear();
    mtexturesBackFront.clear();

    for(unsigned int aa = 0; aa < mTriangleBackFront.size(); aa++)
    {
        delete mTriangleBackFront[aa];
    }
    mTriangleBackFront.clear();



    blockTypes[i].verticesBottomTop = (TexturesPSPVertex*)memalign(16,(mTriangleBottomTop.size() * 3) * sizeof(TexturesPSPVertex));
    blockTypes[i].pointsBottomTop = pointsBottomTop;
    //build verts
    //vertices
    vert = 0;
    size =  mTriangleBottomTop.size();
    for(unsigned int j = 0; j < size; j++)
    {
        blockTypes[i].verticesBottomTop[vert].u = mtexturesBottomTop[mTriangleBottomTop[j]->x]->x;
        blockTypes[i].verticesBottomTop[vert].v = mtexturesBottomTop[mTriangleBottomTop[j]->x]->y;
        blockTypes[i].verticesBottomTop[vert].x = mPositionBottomTop[mTriangleBottomTop[j]->x]->x;
        blockTypes[i].verticesBottomTop[vert].y = mPositionBottomTop[mTriangleBottomTop[j]->x]->y;
        blockTypes[i].verticesBottomTop[vert].z = mPositionBottomTop[mTriangleBottomTop[j]->x]->z;
        vert++;

        blockTypes[i].verticesBottomTop[vert].u = mtexturesBottomTop[mTriangleBottomTop[j]->y]->x;
        blockTypes[i].verticesBottomTop[vert].v = mtexturesBottomTop[mTriangleBottomTop[j]->y]->y;
        blockTypes[i].verticesBottomTop[vert].x = mPositionBottomTop[mTriangleBottomTop[j]->y]->x;
        blockTypes[i].verticesBottomTop[vert].y = mPositionBottomTop[mTriangleBottomTop[j]->y]->y;
        blockTypes[i].verticesBottomTop[vert].z = mPositionBottomTop[mTriangleBottomTop[j]->y]->z;
        vert++;

        blockTypes[i].verticesBottomTop[vert].u = mtexturesBottomTop[mTriangleBottomTop[j]->z]->x;
        blockTypes[i].verticesBottomTop[vert].v = mtexturesBottomTop[mTriangleBottomTop[j]->z]->y;
        blockTypes[i].verticesBottomTop[vert].x = mPositionBottomTop[mTriangleBottomTop[j]->z]->x;
        blockTypes[i].verticesBottomTop[vert].y = mPositionBottomTop[mTriangleBottomTop[j]->z]->y;
        blockTypes[i].verticesBottomTop[vert].z = mPositionBottomTop[mTriangleBottomTop[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(blockTypes[i].verticesBottomTop,(mTriangleBottomTop.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPositionBottomTop.size(); aa++)
    {
        delete mPositionBottomTop[aa];
        delete mtexturesBottomTop[aa];
    }
    mPositionBottomTop.clear();
    mtexturesBottomTop.clear();

    for(unsigned int aa = 0; aa < mTriangleBottomTop.size(); aa++)
    {
        delete mTriangleBottomTop[aa];
    }
    mTriangleBottomTop.clear();
}

void CraftWorld::GetItemVerts(int i,BaseItem *itemType)
{
    std::vector<Vector3*> mPosition;
    std::vector<Vector2*> mtextures;
    std::vector<Vector3*> mTriangle;

    std::vector<Vector3*> mPosition2;
    std::vector<Vector2*> mtextures2;
    std::vector<Vector3*> mTriangle2;

    int iVertex = 0;
    int points = 0;

    int iVertex2 = 0;
    int points2 = 0;

    float x = -0.5f;
    float y = -0.5f;
    float z = -1.0f;

    float pixel = 1.0f / 16.0f;
    float texturepixel = percent3 / 16.0f;

    float down = 1.0f - percent3 * (itemType->textureRow + 1);
    float up = down + percent3;

    float left = percent3 * itemType->plane;
    float right = left + percent3;

    int startPixelsX = itemType->plane*16;
    int startPixelsY = 256 - ((itemType->textureRow + 1)*16);

    iVertex = 0;
    //light

    mPosition.push_back(new Vector3(x,   y,   z+1));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+1, y,   z+1));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+1, y+1, z+1));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+1, z+1));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    itemTypes[i].vertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    itemTypes[i].points = points;
    //build verts
    //vertices
    int vert = 0;
    unsigned int size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        itemTypes[i].vertices[vert].u = mtextures[mTriangle[j]->x]->x;
        itemTypes[i].vertices[vert].v = mtextures[mTriangle[j]->x]->y;
        itemTypes[i].vertices[vert].x = mPosition[mTriangle[j]->x]->x;
        itemTypes[i].vertices[vert].y = mPosition[mTriangle[j]->x]->y;
        itemTypes[i].vertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        itemTypes[i].vertices[vert].u = mtextures[mTriangle[j]->y]->x;
        itemTypes[i].vertices[vert].v = mtextures[mTriangle[j]->y]->y;
        itemTypes[i].vertices[vert].x = mPosition[mTriangle[j]->y]->x;
        itemTypes[i].vertices[vert].y = mPosition[mTriangle[j]->y]->y;
        itemTypes[i].vertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        itemTypes[i].vertices[vert].u = mtextures[mTriangle[j]->z]->x;
        itemTypes[i].vertices[vert].v = mtextures[mTriangle[j]->z]->y;
        itemTypes[i].vertices[vert].x = mPosition[mTriangle[j]->z]->x;
        itemTypes[i].vertices[vert].y = mPosition[mTriangle[j]->z]->y;
        itemTypes[i].vertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(itemTypes[i].vertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    // drop vertices
    float size2 = 0.25f;
    iVertex = 0;

    mPosition.push_back(new Vector3(0,   -size2,   size2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(0, size2,   size2));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(0, size2, -size2));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(0,   -size2, -size2));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;

    itemTypes[i].dropVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    vert = 0;
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        itemTypes[i].dropVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        itemTypes[i].dropVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        itemTypes[i].dropVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        itemTypes[i].dropVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        itemTypes[i].dropVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        itemTypes[i].dropVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        itemTypes[i].dropVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        itemTypes[i].dropVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        itemTypes[i].dropVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        itemTypes[i].dropVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        itemTypes[i].dropVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        itemTypes[i].dropVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        itemTypes[i].dropVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        itemTypes[i].dropVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        itemTypes[i].dropVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(itemTypes[i].dropVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    // add vertices
    points = 0;
    iVertex = 0;

    points2 = 0;
    iVertex2 = 0;

    int texture;
    itemTypes[i].terrainTexture == true ? texture = textureTerrainId : texture = textureItemsId;

    if(TextureManager::Instance()->Images[texture]->Width == 256)
    {
        for(int i = 15; i >= 0; i--)
        {
            for(int j = 15; j >= 0; j--)
            {
                int xx = startPixelsX+i;
                int yy = startPixelsY+j;
                if(TextureManager::Instance()->getAlpha(xx,yy,texture) == 255)
                {

                    if( (j == 255) || (j != 255 && TextureManager::Instance()->getAlpha(xx,yy+1,texture) == 0) )
                    {
                        mPosition.push_back(new Vector3(x+i*pixel,y+(j+1)*pixel,   z+pixel*17));
                        mtextures.push_back(new Vector2(left+texturepixel*i, down+texturepixel*j));

                        mPosition.push_back(new Vector3(x+(i+1)*pixel, y+(j+1)*pixel,   z+pixel*17));
                        mtextures.push_back(new Vector2(left+texturepixel*(i+1), down+texturepixel*j));

                        mPosition.push_back(new Vector3(x+(i+1)*pixel, y+(j+1)*pixel, z+pixel*16));
                        mtextures.push_back(new Vector2(left+texturepixel*(i+1), down+texturepixel*(j+1)));

                        mPosition.push_back(new Vector3(x+i*pixel, y+(j+1)*pixel, z+pixel*16));
                        mtextures.push_back(new Vector2(left+texturepixel*i, down+texturepixel*(j+1)));

                        mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
                        mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

                        iVertex += 4;
                        points += 6;
                    }

                    if( (i == 255) || (i != 255 && TextureManager::Instance()->getAlpha(xx+1,yy,texture) == 0))
                    {
                        mPosition2.push_back(new Vector3(x+(i+1)*pixel,y+j*pixel,   z+pixel*17));
                        mtextures2.push_back(new Vector2(left+texturepixel*(i+1), down+texturepixel*(j+1)));

                        mPosition2.push_back(new Vector3(x+(i+1)*pixel, y+(j+1)*pixel,   z+pixel*17));
                        mtextures2.push_back(new Vector2(left+texturepixel*(i+1), down+texturepixel*j));

                        mPosition2.push_back(new Vector3(x+(i+1)*pixel, y+(j+1)*pixel, z+pixel*16));
                        mtextures2.push_back(new Vector2(left+texturepixel*i, down+texturepixel*j));

                        mPosition2.push_back(new Vector3(x+(i+1)*pixel, y+j*pixel, z+pixel*16));
                        mtextures2.push_back(new Vector2(left+texturepixel*i, down+texturepixel*(j+1)));

                        mTriangle2.push_back(new Vector3(iVertex2, iVertex2+1, iVertex2+2));
                        mTriangle2.push_back(new Vector3(iVertex2+2, iVertex2+3, iVertex2));

                        iVertex2 += 4;
                        points2 += 6;
                    }

                    if( (j == 0) || (j != 0 && TextureManager::Instance()->getAlpha(xx,yy-1,texture) == 0) )
                    {
                        mPosition2.push_back(new Vector3(x+i*pixel,y+j*pixel,   z+pixel*17));
                        mtextures2.push_back(new Vector2(left+texturepixel*i, down+texturepixel*j));

                        mPosition2.push_back(new Vector3(x+(i+1)*pixel, y+j*pixel,   z+pixel*17));
                        mtextures2.push_back(new Vector2(left+texturepixel*(i+1), down+texturepixel*j));

                        mPosition2.push_back(new Vector3(x+(i+1)*pixel, y+j*pixel, z+pixel*16));
                        mtextures2.push_back(new Vector2(left+texturepixel*(i+1), down+texturepixel*(j+1)));

                        mPosition2.push_back(new Vector3(x+i*pixel, y+j*pixel, z+pixel*16));
                        mtextures2.push_back(new Vector2(left+texturepixel*i, down+texturepixel*(j+1)));

                        mTriangle2.push_back(new Vector3(iVertex2, iVertex2+1, iVertex2+2));
                        mTriangle2.push_back(new Vector3(iVertex2+2, iVertex2+3, iVertex2));

                        iVertex2 += 4;
                        points2 += 6;
                    }
                }
            }
        }
    }

    itemTypes[i].addVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    itemTypes[i].addPoints = points;


    //build verts for addVertices
    //vertices
    vert = 0;
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        itemTypes[i].addVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        itemTypes[i].addVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        itemTypes[i].addVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        itemTypes[i].addVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        itemTypes[i].addVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        itemTypes[i].addVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        itemTypes[i].addVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        itemTypes[i].addVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        itemTypes[i].addVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        itemTypes[i].addVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        itemTypes[i].addVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        itemTypes[i].addVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        itemTypes[i].addVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        itemTypes[i].addVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        itemTypes[i].addVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(itemTypes[i].addVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    itemTypes[i].add2Vertices = (TexturesPSPVertex*)memalign(16,( mTriangle2.size() * 3) * sizeof(TexturesPSPVertex));
    itemTypes[i].add2Points = points2;

    //build verts for add2Vertices
    //vertices
    vert = 0;
    size =  mTriangle2.size();
    for(unsigned int j = 0; j < size; j++)
    {
        itemTypes[i].add2Vertices[vert].u = mtextures2[mTriangle2[j]->x]->x;
        itemTypes[i].add2Vertices[vert].v = mtextures2[mTriangle2[j]->x]->y;
        itemTypes[i].add2Vertices[vert].x = mPosition2[mTriangle2[j]->x]->x;
        itemTypes[i].add2Vertices[vert].y = mPosition2[mTriangle2[j]->x]->y;
        itemTypes[i].add2Vertices[vert].z = mPosition2[mTriangle2[j]->x]->z;
        vert++;

        itemTypes[i].add2Vertices[vert].u = mtextures2[mTriangle2[j]->y]->x;
        itemTypes[i].add2Vertices[vert].v = mtextures2[mTriangle2[j]->y]->y;
        itemTypes[i].add2Vertices[vert].x = mPosition2[mTriangle2[j]->y]->x;
        itemTypes[i].add2Vertices[vert].y = mPosition2[mTriangle2[j]->y]->y;
        itemTypes[i].add2Vertices[vert].z = mPosition2[mTriangle2[j]->y]->z;
        vert++;

        itemTypes[i].add2Vertices[vert].u = mtextures2[mTriangle2[j]->z]->x;
        itemTypes[i].add2Vertices[vert].v = mtextures2[mTriangle2[j]->z]->y;
        itemTypes[i].add2Vertices[vert].x = mPosition2[mTriangle2[j]->z]->x;
        itemTypes[i].add2Vertices[vert].y = mPosition2[mTriangle2[j]->z]->y;
        itemTypes[i].add2Vertices[vert].z = mPosition2[mTriangle2[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(itemTypes[i].add2Vertices,( mTriangle2.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition2.size(); aa++)
    {
        delete mPosition2[aa];
        delete mtextures2[aa];
    }
    mPosition2.clear();
    mtextures2.clear();

    for(unsigned int aa = 0; aa < mTriangle2.size(); aa++)
    {
        delete mTriangle2[aa];
    }
    mTriangle2.clear();
}

// The one place that owns the per-block mesh buffers. It exists because ownership inside BaseBlock was
// a landmine, and this is worth understanding before touching either end of it.
//
// The three buffers are memalign'd, BaseBlock's copy is the compiler's (shallow), and BaseBlock is held
// **by value** in `blockTypes`. So while ~BaseBlock owned them, every way of copying a block that had
// already been built produced two BaseBlocks pointing at one buffer and a free for each: a vector
// reallocation (`AddBlockType` resizing past the reserve), or any accidental copy of a built block.
// `InitBlockVector` reserved the whole id space to make the reallocation impossible, which is the kind of
// safety that only holds while everyone remembers it.
//
// Ownership now lives here instead, so the copy is harmless -- nothing frees through a copy any more --
// and the vector's reserve is ordinary hygiene rather than load bearing. `blockTypes.size()` is the only
// upper bound needed, and a buffer is freed exactly once because it is NULLed as it goes.
//
// It also fixes a real leak rather than a hypothetical one. buildblocksVerts() runs on *every* entry
// into a play state -- six call sites between StatePlay.cpp and StatePlayCreative.cpp -- and it used to
// assign a fresh memalign over each live pointer without releasing the old one, so every visit to a
// world stranded the entire block mesh set. That is why buildblocksVerts() now calls this first.
void CraftWorld::FreeBlockTypeMeshes()
{
    for(size_t i = 0; i < blockTypes.size(); i++)
    {
        FreeBlockTypeMesh((int)i);
    }
}

// One slot. Used by the loop above and by AddBlockType, which must not strand a mesh by overwriting a
// slot that already has one.
void CraftWorld::FreeBlockTypeMesh(int id)
{
    if(id < 0 || id >= (int)blockTypes.size())
    {
        return;
    }
    BaseBlock& slot = blockTypes[id];
    if(slot.verticesLeftRight != NULL)
    {
        free(slot.verticesLeftRight);
        slot.verticesLeftRight = NULL;
    }
    if(slot.verticesBackFront != NULL)
    {
        free(slot.verticesBackFront);
        slot.verticesBackFront = NULL;
    }
    if(slot.verticesBottomTop != NULL)
    {
        free(slot.verticesBottomTop);
        slot.verticesBottomTop = NULL;
    }
    // The counts are only meaningful next to a live pointer, and the draw calls test the pointer before
    // reading the count -- but clearing them keeps "points > 0 implies verts != NULL" true at all times.
    slot.pointsLeftRight = 0;
    slot.pointsBackFront = 0;
    slot.pointsBottomTop = 0;
}

// Item meshes: same contract and same reasoning as the block ones above -- the world owns them, so
// BaseItem can be copied freely. There are four here, not three, and the old ~BaseItem freed all four
// behind a single `vertices != NULL` test, which both leaked and double-freed in the wrong situation.
void CraftWorld::FreeItemTypeMeshes()
{
    for(size_t i = 0; i < itemTypes.size(); i++)
    {
        FreeItemTypeMesh((int)i);
    }
}

void CraftWorld::FreeItemTypeMesh(int id)
{
    if(id < 0 || id >= (int)itemTypes.size())
    {
        return;
    }
    BaseItem& slot = itemTypes[id];
    if(slot.vertices != NULL)
    {
        free(slot.vertices);
        slot.vertices = NULL;
    }
    if(slot.addVertices != NULL)
    {
        free(slot.addVertices);
        slot.addVertices = NULL;
    }
    if(slot.add2Vertices != NULL)
    {
        free(slot.add2Vertices);
        slot.add2Vertices = NULL;
    }
    if(slot.dropVertices != NULL)
    {
        free(slot.dropVertices);
        slot.dropVertices = NULL;
    }
    slot.points = 0;
    slot.addPoints = 0;
    slot.add2Points = 0;
}

void CraftWorld::buildblocksVerts()
{
    // Release the previous meshes before rebuilding. This runs once per entry into a play state, so
    // without these lines each visit to a world leaks every block and item mesh. See FreeBlockTypeMeshes.
    FreeBlockTypeMeshes();
    FreeItemTypeMeshes();

    for(unsigned int i = 0; i < itemTypes.size(); i++)
    {
        BaseItem *itemType = &itemTypes[i];
        GetItemVerts(i,itemType);
    }
    //create vertices for each block type
    for(unsigned int i = 1; i < blockTypes.size(); i++)
    {
        // The gap is item ids. Extended slots stay default until AddBlockType fills one.
        if(i >= 254 && i < (unsigned int)kExtendedBlockBase)
            continue;
        if(i >= (unsigned int)kExtendedBlockBase && blockTypes[i].name == " ")
            continue;
        BaseBlock *blockType = &blockTypes[i];
        if(blockType->blockModel > 0)
            GetSpecialBlockVerts(i,blockType);
        else
            GetNormalBlockVerts(i,blockType);
    }

    // A readout, because this function's main failure mode is invisible. It runs on every entry into a
    // world, and if the previous meshes were not released first the only symptom is a heap that never
    // comes back. So: visit a world, leave, visit again, and compare two "block mesh" lines in Log.txt.
    // types, meshKB and heapFreeKB must all be identical. Before the Free*TypeMeshes calls, heapFreeKB
    // dropped by roughly meshKB on every visit.
    size_t meshVerts = 0;
    int meshBuffers = 0;
    for(unsigned int i = 0; i < blockTypes.size(); i++)
    {
        meshVerts += (size_t)blockTypes[i].pointsLeftRight
                   + (size_t)blockTypes[i].pointsBackFront
                   + (size_t)blockTypes[i].pointsBottomTop;
    }
    for(unsigned int i = 0; i < itemTypes.size(); i++)
    {
        meshVerts += (size_t)itemTypes[i].points
                   + (size_t)itemTypes[i].addPoints
                   + (size_t)itemTypes[i].add2Points;
    }
    meshBuffers = (int)blockTypes.size();
    Aurora::Utils::Logger::Instance()->LogMessage("block mesh: blockTypes=%d items=%d verts=%d meshKB=%d heapUsedKB=%d heapFreeKB=%d\n",
        meshBuffers, (int)itemTypes.size(), (int)meshVerts,
        (int)((meshVerts * sizeof(TexturesPSPVertex)) / 1024),
        HeapUsedKB(), HeapFreeKB());
}

void CraftWorld::buildcloudsVerts()
{
    // The cloud mesh shares block 141's slot with its ordinary mesh, which buildblocksVerts() has just
    // built. Release that first, or this overwrite strands a buffer on every world entry.
    FreeBlockTypeMesh(141);
    BaseBlock *blockType = &blockTypes[141];
    GetCloudsVerts(blockType);
}

void CraftWorld::buildZombieVerts()
{

}

/*
 * How often a section whose mesh could not be built is allowed to TRY again.
 *
 * A mesh whose scratch does not fit is left empty (see MeshScratch) and has to be rebuilt, or the
 * hole stays until something else happens to touch that section -- which, under the memory pressure
 * that caused the failure, may be a long time. It cannot be retried every frame either: each attempt
 * is a fresh request for up to 256 KB on a heap that has just refused one, so a per-frame retry would
 * turn a single missing section into a rebuild treadmill.
 *
 * This gate used to decide whether the section was put back in the queue AT ALL, and that was wrong in
 * a way that is visible from the player's seat. The window is a second wide and global, so when a
 * batch of sections failed together -- which is exactly what memory pressure does -- only the first
 * one each second was re-queued and every other one was DROPPED: end() had already cleared
 * needUpdate, and nothing set it again, so the section drew nothing until an unrelated block change
 * happened to dirty it. That is the "blocks refuse to load until you place or break one" report.
 *
 * So the intent is now unconditional and this only paces the retries. `meshRebuildRetryDue` gates the
 * START of a rebuild for a section that is already marked outOfMemory; the section stays queued
 * meanwhile, so a hole is always a section waiting for room rather than one that has been forgotten.
 * A quarter second recovers a batch of forty failed sections in ten seconds while keeping the wasted
 * work bounded, where a second apiece took forty.
 */
static bool meshRebuildRetryDue()
{
    static unsigned int sNextAt = 0;
    unsigned int now = sceKernelGetSystemTimeLow();
    if(sNextAt != 0 && now < sNextAt)
        return false;
    sNextAt = now + 250000u;
    return true;
}

int CraftWorld::CreateFullMeshChunk(const int StartX, const int StartY, const int StartZ, bool buildNow)
{
    SimpleMeshChunk* OpaqueMeshChunk = new SimpleMeshChunk();
    SimpleMeshChunk* TransparentMeshChunk = new SimpleMeshChunk();
    // The two chunks are walked in one pass and both accumulate, so they must not share a build
    // scratch. Slot 0 is opaque, slot 1 transparent -- see SimpleMeshChunk::SetScratchSlot.
    TransparentMeshChunk->SetScratchSlot(1);

    int iOpaqueVertex = 0;
    int iTransparentVertex = 0;
    block_t Block;

    /* Only create visible faces of each chunk */
    OpaqueMeshChunk->chunkStartZ = StartZ;
    OpaqueMeshChunk->chunkStartY = StartY;
    OpaqueMeshChunk->chunkStartX = StartX;

    TransparentMeshChunk->chunkStartZ = StartZ;
    TransparentMeshChunk->chunkStartY = StartY;
    TransparentMeshChunk->chunkStartX = StartX;

    OpaqueMeshChunk->bBox = BoundingBox(Vector3(StartX,StartY,StartZ),Vector3(StartX + CHUNK_SIZE,StartY + CHUNK_SIZE,StartZ + CHUNK_SIZE));
    TransparentMeshChunk->bBox = BoundingBox(Vector3(StartX,StartY,StartZ),Vector3(StartX + CHUNK_SIZE,StartY + CHUNK_SIZE,StartZ + CHUNK_SIZE));

    // Reuse a slot freed by an unloaded column so the mesh arrays (walked every frame by the renderer) do
    // not grow forever while streaming. Done before the build rather than after it, because a rebuild is a
    // job addressed by slot id now and the request below needs the section to be in `mChunks` already. For
    // the inline build the order does not matter -- these are the same two pointers either way.
    int id = -1;
    for(unsigned int i = 0; i < mChunks.size() && i < mTransparentChunks.size(); i++)
    {
        if(mChunks[i] == 0 && mTransparentChunks[i] == 0)
        {
            mChunks[i] = OpaqueMeshChunk;
            mTransparentChunks[i] = TransparentMeshChunk;
            id = (int)i;
            break;
        }
    }
    if(id < 0)
    {
        mChunks.push_back(OpaqueMeshChunk);
        mTransparentChunks.push_back(TransparentMeshChunk);
        id = (int)mChunks.size() - 1;
    }

    // Whether the ground passes through this section travels with the mesh, so the renderer's zone test
    // can keep it on screen at any height (MeshInPlayerZone).
    {
        ChunkColumn* bandCol = FindColumn(ChunkFloorDiv(StartX, CHUNK_SIZE), ChunkFloorDiv(StartZ, CHUNK_SIZE));
        EnsureSurfaceBand(bandCol);
        OpaqueMeshChunk->surfaceBand = SectionInSurfaceBand(bandCol, StartY);
        TransparentMeshChunk->surfaceBand = OpaqueMeshChunk->surfaceBand;
    }
    const bool inZone = MeshInPlayerZone(OpaqueMeshChunk);

    // Two ways a section is registered without being built here. `buildNow` is false for the streaming path
    // (TryMeshOneSection), which wants the walk spread over frames like every other rebuild; and a build
    // cannot run at all while a job holds the caches for another section's box, or it would fill them out
    // from under it. Either way `created` false is the flag that says "this section exists but has no
    // geometry yet", which is exactly what drawWorld's rebuild pick looks for.
    if(!inZone || !buildNow || meshJobKind != kMeshJobNone)
    {
        createdChunksCount++;
        OpaqueMeshChunk->end();
        OpaqueMeshChunk->created = false;
        TransparentMeshChunk->end();
        TransparentMeshChunk->created = false;
        // In the zone, this section is wanted now; it just cannot be walked here. `buildNow == false` means
        // the streaming path asked for a scheduled build rather than an inline one, and the other two cases
        // are a job already holding the caches. Out of the zone there is nothing to schedule -- that section
        // is built when it comes into range, exactly as it always was.
        if(inZone)
            RequestMeshRebuild(kMeshJobFull, id, false);
        // end() cleared needUpdate. Until the job actually writes faces this slot is a hole,
        // and the picker must see it that way -- otherwise TryMeshOneSection keeps creating
        // the next frontier section while this one sits at tris=0/0 needUpd=0.
        OpaqueMeshChunk->needUpdate = true;
        TransparentMeshChunk->needUpdate = true;
        return id;
    }

    //if we are in player zone on the start then build map
    // (Always taken here: the guard above has already settled that this section is in the zone, is wanted
    // built inline, and that no job holds the caches. Kept as a test rather than a bare block so the walk
    // below reads the same as it always has.)
    if(inZone)
    {
        meshBuildsThisFrame++;
        createdChunksCount++;
        BeginMeshCaches(StartX, StartY, StartZ);
        //light
        for (int z = StartZ; z < CHUNK_SIZE + StartZ; ++z)
        {
            for (int y = StartY; y < CHUNK_SIZE + StartY; ++y)
            {
                for (int x = StartX; x < CHUNK_SIZE + StartX; ++x)
                {
                    Block = GetBlockNoCheck(x,y,z);
                    if(Block == 0) continue;

                    if(blockTypes[Block].transparent == true)
                    {
                        if(blockTypes[Block].blockModel == 0)
                        {
                            GetNormalBlock(x,y,z,iTransparentVertex,TransparentMeshChunk,Block,true);
                        }
                        else
                        {
                            GetSpecialBlock(x,y,z,iTransparentVertex,TransparentMeshChunk,Block,true);
                        }
                    }
                    else
                    {
                        if(blockTypes[Block].blockModel == 0)
                        {
                            GetNormalBlock(x,y,z,iOpaqueVertex,OpaqueMeshChunk,Block,false);
                        }
                        else
                        {
                            GetSpecialBlock(x,y,z,iOpaqueVertex,OpaqueMeshChunk,Block,false);
                        }
                    }
                }
            }
        }

        OpaqueMeshChunk->end();
        TransparentMeshChunk->end();
        EndMeshCaches();
        if(OpaqueMeshChunk->outOfMemory || TransparentMeshChunk->outOfMemory)
        {
            Aurora::Utils::Logger::Instance()->LogMessage("mesh OOM at %d,%d,%d heapUsedKB=%d heapFreeKB=%d\n",
                StartX, StartY, StartZ, HeapUsedKB(), HeapFreeKB());

            // ALWAYS re-queued. end() cleared the flag, so this is the only thing standing between a
            // section that could not be meshed and a permanent hole in the world; see
            // meshRebuildRetryDue for what dropping it here used to cost. The rate is enforced when the
            // rebuild is about to start, not by forgetting the request.
            OpaqueMeshChunk->needUpdate = true;
            TransparentMeshChunk->needUpdate = true;
        }
    }

    return id;
}

void CraftWorld::RebuildFullMeshChunk(int id)
{
    if(!MeshIdValid(id))
        return;

    RequestMeshRebuild(kMeshJobFull, id, true);

    // The two-frame settle the full rebuild has always asked for. It belongs here rather than in
    // RequestMeshRebuild because the redstone drain calls that directly and wants the opposite.
    sSkipQueuedRebuilds = 2;
}

// ---------------------------------------------------------------------------------------------
// A section rebuild is spread over frames, and this is why.
//
// One walk of a section is 4,096 cells and costs roughly 30 ms -- the probe's own numbers put it at
// `mesh full=29949 us`, against a 16.7 ms frame at 60 fps. It used to run to completion inside the frame
// that asked for it, and there are dozens of direct callers of RebuildFullMeshChunk/Opaque/Transparent in
// the two play states with no re-entrancy guard between them, so one busy frame could stack several of
// these and land on the 196-221 ms worst frame the perf lines report. Making the walk *faster* is not the
// fix and never was: the frame budget exists, the walk simply must not all happen in one frame.
//
// So a rebuild is a job now. RequestMeshRebuild starts one -- or defers the request if one is already
// running, because the caches belong to a single section's box and a second build would overwrite them
// under the first -- and AdvanceMeshJob walks a slice of it once per frame from UpdateWorldProcesses.
//
// Two properties make the split sound rather than merely smaller:
//
//  - The old mesh stays on screen for the whole build. `SimpleMeshChunk::beginRebuild` drops the scratch
//    but leaves `meshVertices`/`trienglesCount` alone, and `end` is what swaps them, so a section five
//    frames into a rebuild is still drawn with the geometry it had. The rebuild is invisible while it
//    runs instead of being a hole in the world.
//  - The vertex counters restart from the scratch. The only per-cell state a walk carries is the two
//    `i*Vertex` bases, and those are exactly `currentVertexCount()`, so a slice picks up where the last
//    one stopped without the walk having to store anything.
//
// What a slice does *not* re-derive is the cache. Slice 0 fills the 18^3 light cache and the 3x3 column
// grid; the slices after it re-point the two flags at the arrays those filled. The arrays are members and
// outlive the frame, so that is free -- and the staleness window stays what it was, because the flags are
// dropped again as each slice returns, so a cached read can only happen inside a slice. A block changed
// mid-build is picked up by the rebuild the change itself queued, not by this one.
//
// One walk still covers both meshes: the per-cell cost splits in two and only one half scales with the
// geometry. Filling the cache and stepping the 4,096 cells happens whatever the section holds; the mesher
// calls happen only for cells that emit a face. Walking twice to build the two halves therefore pays the
// fixed half twice for nothing -- the probe measures 28 us for the transparent half and 25 for the opaque
// one against 34 for both through this loop, the same geometry 19 us cheaper.
//
// `meshJobScanPlants` is the one thing here that is not geometry. A full rebuild uses it to discover the
// animated crops in a section and hand them to the animation tick; a redstone change cannot have placed a
// crop, so that path skips the scan and its per-cell cost.
void CraftWorld::RequestMeshRebuild(int kind, int id, bool scanPlants)
{
    if(!MeshIdValid(id))
        return;

    if(meshJobKind != kMeshJobNone)
    {
        // One build at a time. Deferring is always safe because the toUpdate drain rebuilds both halves of
        // a section, which is a superset of an opaque-only or transparent-only request -- a deferred
        // request can never be satisfied by less work than it asked for.
        if(id != meshJobId)
            AddChunkToUpdate(id);
        return;
    }

    meshJobKind = kind;
    meshJobId = id;
    meshJobCursor = 0;
    meshJobFillCursor = 0;
    meshJobCachesReady = false;
    meshJobScanPlants = scanPlants;
    meshJobSkin = false;
    meshScanSealed = 0;
    meshScanCol = -1;

    // The frame's one-rebuild budget is claimed for the whole job rather than per slice: everything that
    // feeds a rebuild tests this, so it is what keeps the drains off the caches this job walks against.
    meshBuildsThisFrame++;
    if(kind == kMeshJobFull)
    {
        MC_PERF_INC(kMeshFull);
        MC_PERF_ADD(kMeshCells, 2 * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
        // The two-frame settle is the caller's to ask for, not this function's: the redstone drain wants
        // the opposite of it (see FlushRedstoneMeshes), and it is the one other full-rebuild caller.
    }
    else if(kind == kMeshJobOpaque)
    {
        MC_PERF_INC(kMeshOpaque);
        MC_PERF_ADD(kMeshCells, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
    }
    else
    {
        MC_PERF_INC(kMeshTransparent);
        MC_PERF_ADD(kMeshCells, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
    }
}

bool CraftWorld::AdvanceMeshJob(unsigned int budgetUs)
{
    if(meshJobKind == kMeshJobNone)
        return true;

    if(!MeshIdValid(meshJobId))
    {
        // The section went away under the job -- a column unload. Its mesh chunks went with it, so there
        // is nothing left to finish and nothing to release here.
        meshJobKind = kMeshJobNone;
        meshJobId = -1;
        meshJobCursor = 0;
        return true;
    }

    SimpleMeshChunk* OpaqueMeshChunk = mChunks[meshJobId];
    SimpleMeshChunk* TransparentMeshChunk = mTransparentChunks[meshJobId];

    const bool wantsOpaque = (meshJobKind != kMeshJobTransparent);
    const bool wantsTransparent = (meshJobKind != kMeshJobOpaque);

    int StartZ = OpaqueMeshChunk->chunkStartZ;
    int StartY = OpaqueMeshChunk->chunkStartY;
    int StartX = OpaqueMeshChunk->chunkStartX;

    // The slice clock starts BEFORE the setup, so the setup spends the same budget the scan does. The
    // setup's expensive half -- the 18^3 light/block/flags fill -- is walked in slices below, which is
    // what turns a ~20 ms per-section spike into a few frames of a few ms. The caller chooses the
    // length: 4 ms beside a streaming hitch, longer on a frame that had nothing else to do.
    if(budgetUs == 0u)
        budgetUs = 4000u;
    const unsigned int kMeshSliceUs = budgetUs;
    const unsigned int sliceStart = sceKernelGetSystemTimeLow();
    if(meshJobCursor == 0 && !meshJobCachesReady)
    {
        MC_PERF_PHASE_BEGIN(kPhMeshSetup);
        if(meshJobFillCursor == 0)
            BeginMeshCachesHead(StartX, StartY, StartZ);
        FillMeshCachesSlice(sliceStart, kMeshSliceUs);
        MC_PERF_PHASE_END(kPhMeshSetup);
        if(meshJobFillCursor < MeshLightTotal())
        {
            // Cache not finished: the scan cannot run against a half-filled cache. Keep the cursor and
            // come back next frame on a fresh budget. meshLightActive stays false so the rest of the
            // frame reads the live world rather than the half-built cache.
            meshLightActive = false;
            meshGridActive = false;
            return false;
        }
        meshJobCachesReady = true;
        meshLightActive = true;      // the cache is complete; the scan reads it rather than the live world
        if(wantsOpaque)
            OpaqueMeshChunk->beginRebuild();
        if(wantsTransparent)
            TransparentMeshChunk->beginRebuild();
    }
    else
    {
        // Mid-scan, or a job whose cache is already complete. Re-point the light cache at the arrays slice
        // 0 filled -- those are members and survived the frame.
        //
        // The column grid is re-derived rather than restored, and that is deliberate: it is nine hash lookups
        // and it is shared. UpdateBlockPhysicsBox fills the same three-by-three grid from its own section and
        // can run between two of this job's slices, so a job cannot assume the grid it filled is still the
        // grid it left. Re-deriving costs nothing worth measuring and removes that whole class of aliasing.
        meshLightActive = true;
        BeginColumnGrid(StartX, StartZ);
    }

    int iOpaqueVertex = wantsOpaque ? OpaqueMeshChunk->currentVertexCount() : 0;
    int iTransparentVertex = wantsTransparent ? TransparentMeshChunk->currentVertexCount() : 0;

    // A slice ends when it has spent kMeshSliceUs on the scan, checked every 64 cells because the clock read
    // is not free either. The budget is a *time* rather than a cell count on purpose: the probe's per-build
    // figures are ratios and not budgets -- it rebuilds the same section ten times with everything cold, and
    // its 30 ms for a rebuild does not fit inside the 26 ms frame it was measured in -- so the number of
    // cells that fits in a frame is not knowable in advance, but the frame's own budget is. A section cheap
    // enough to walk inside the budget therefore still finishes in the frame it was asked for, exactly as
    // before, and only a section that would have blown the frame is split. 64 cells is also the floor: even
    // a pathologically slow section advances, so a job cannot stall.
    const int cs = CHUNK_SIZE;
    const int totalCells = cs * cs * cs;

    block_t Block;
    // Every cell, top to bottom. A block is skipped only when it and all six neighbours are opaque
    // full cubes. The neighbour above the grass sits in the 18^3 halo, so the surface is not treated
    // as buried, and a cave stops the skip because the air next to the wall is not a wall. Nothing
    // about this walk stops early and calls the section done.
    const int cdx = meshLightDimX;
    const int cdxy = meshLightDimX * meshLightDimY;
    const bool cacheCovers = (meshLightDimX == cs + 2 && meshLightDimY == cs + 2 && meshLightDimZ == cs + 2);
    while(meshJobCursor < totalCells)
    {
        const int col = meshJobCursor / cs;
        const int local = meshJobCursor % cs;
        const int x = StartX + (col % cs);
        const int z = StartZ + (col / cs);
        const int y = StartY + (cs - 1 - local);

        const int lx = x - meshLightOriginX;
        const int ly = y - meshLightOriginY;
        const int lz = z - meshLightOriginZ;
        const int bi = lx + cdx * ly + cdxy * lz;
        Block = cacheCovers ? meshBlocks[bi] : GetBlockNoCheck(x,y,z);

        bool buried = false;
        if(cacheCovers && (meshFlags[bi] & MeshFlagWall))
        {
            buried = (meshFlags[bi + 1] & MeshFlagWall)
                  && (meshFlags[bi - 1] & MeshFlagWall)
                  && (meshFlags[bi + cdx] & MeshFlagWall)
                  && (meshFlags[bi - cdx] & MeshFlagWall)
                  && (meshFlags[bi + cdxy] & MeshFlagWall)
                  && (meshFlags[bi - cdxy] & MeshFlagWall);
        }

        // Air is not a reason to skip the slice check below. It used to `continue` straight past it, so
        // a section that was mostly air -- which is most of the nether -- advanced thousands of cells
        // between two clock reads and could sail past kMeshSliceUs entirely. Hoisting the increment and
        // the check out of the block-type branch is what makes the budget actually bound the call.
        if(!buried && Block != 0 && Block < blockTypes.size())
        {
            if(meshJobScanPlants && OpaqueMeshChunk->periodicallyUpadted == false)
            {
                if((Block >= MelonPlant1::getID() && Block <= MelonPlant4::getID()) ||
                   (Block >= WheatBlock1::getID() && Block <= WheatBlock4::getID()) ||
                   (Block >= OakSapling::getID() && Block <= BirchSapling::getID()) ||
                   (Block >= PotatoPlant1::getID() && Block <= PotatoPlant4::getID()) ||
                   (Block >= CarrotPlant1::getID() && Block <= CarrotPlant4::getID()) ||
                   (Block >= PumpkinPlant1::getID() && Block <= PumpkinPlant4::getID()))
                {
                    updatingChunks.push_back(meshJobId);
                    OpaqueMeshChunk->periodicallyUpadted = true;
                }
            }

            if(blockTypes[Block].transparent == true)
            {
                if(wantsTransparent)
                {
                    if(blockTypes[Block].blockModel == 0)
                    {
                        GetNormalBlock(x,y,z,iTransparentVertex,TransparentMeshChunk,Block,true);
                    }
                    else
                    {
                        GetSpecialBlock(x,y,z,iTransparentVertex,TransparentMeshChunk,Block,true);
                    }
                }
            }
            else if(wantsOpaque)
            {
                if(blockTypes[Block].blockModel == 0)
                {
                    GetNormalBlock(x,y,z,iOpaqueVertex,OpaqueMeshChunk,Block,false);
                }
                else
                {
                    GetSpecialBlock(x,y,z,iOpaqueVertex,OpaqueMeshChunk,Block,false);
                }
            }
        }

        ++meshJobCursor;

        if(meshJobCursor >= totalCells)
            break;
        if((meshJobCursor & 63) == 0 && (sceKernelGetSystemTimeLow() - sliceStart) >= kMeshSliceUs)
            break;
    }

    if(meshJobCursor < totalCells)
    {
        // Not finished. Drop the flags so the rest of the frame reads the live world rather than this
        // section's build-time snapshot; the arrays are kept for the next slice.
        meshLightActive = false;
        meshGridActive = false;
        return false;
    }

    // The end step -- build the vertex buffer, upload it, and (on a full heap) the failed-allocation
    // path -- runs OUTSIDE the loop above and therefore outside kMeshSliceUs. It is timed separately so
    // the slice overrun seen in the frame probe (18-26 ms against a 4 ms budget) can be attributed to
    // the scan or to this, rather than assumed.
    MC_PERF_PHASE_BEGIN(kPhMeshFinish);
    const int opaqueNewVerts = wantsOpaque ? OpaqueMeshChunk->currentVertexCount() : 0;
    const int transNewVerts = wantsTransparent ? TransparentMeshChunk->currentVertexCount() : 0;
    MC_PERF_ADD(kMeshQuads, (opaqueNewVerts + transNewVerts) / 4);
    // Both halves empty, and the world still has something to draw here: publishing that result
    // is how a chunk that was on screen became air when the player walked up to it. Keep the
    // mesh that is already drawn. A rebuild that produced faces, or a section that is actually
    // empty, still replaces it.
    const bool newBuildEmpty = (wantsOpaque ? opaqueNewVerts == 0 : true)
                             && (wantsTransparent ? transNewVerts == 0 : true);
    const bool allowEmpty = !newBuildEmpty || !SectionHasExposedFaces(StartX, StartY, StartZ);
    if(wantsOpaque)
        OpaqueMeshChunk->end(allowEmpty);
    if(wantsTransparent)
        TransparentMeshChunk->end(allowEmpty);
    if((wantsOpaque && OpaqueMeshChunk->rejectedEmpty) || (wantsTransparent && TransparentMeshChunk->rejectedEmpty))
        Aurora::Utils::Logger::Instance()->LogMessage("mesh kept at %d,%d,%d (empty rebuild, faces stayed)\n", StartX, StartY, StartZ);
    EndMeshCaches();
    MC_PERF_INC(kMeshDone);

    // The rebuild path is where the crash used to happen, because rebuilding is what drives the
    // scratch back up its growth ladder while the heap is full. A section the scratch could not fit
    // is left empty and has to come back.
    // A section whose last build failed is left EMPTY (see MeshScratch) and must come back; this is the
    // only place that sets that up. The retry RATE is enforced in the drain, which will not start a
    // rebuild for an already-failed section until meshRebuildRetryDue allows one -- but it keeps the
    // request queued, so the section is waiting for room rather than lost. See meshRebuildRetryDue.
    if((wantsOpaque && OpaqueMeshChunk->outOfMemory) || (wantsTransparent && TransparentMeshChunk->outOfMemory))
    {
        Aurora::Utils::Logger::Instance()->LogMessage("mesh OOM at %d,%d,%d heapUsedKB=%d heapFreeKB=%d\n",
            StartX, StartY, StartZ, HeapUsedKB(), HeapFreeKB());
        if(wantsOpaque)
            OpaqueMeshChunk->needUpdate = true;
        if(wantsTransparent)
            TransparentMeshChunk->needUpdate = true;
    }
    else if((!wantsOpaque || OpaqueMeshChunk->trienglesCount == 0)
         && (!wantsTransparent || TransparentMeshChunk->trienglesCount == 0)
         && SectionHasExposedFaces(StartX, StartY, StartZ))
    {
        // Finished, empty, and the world still has faces here. Leaving needUpdate false is the
        // permanent hole the card logged (tris=0/0 needUpd=0). Ask again.
        if(wantsOpaque)
            OpaqueMeshChunk->needUpdate = true;
        if(wantsTransparent)
            TransparentMeshChunk->needUpdate = true;
        meshPending = true;
    }

    meshJobKind = kMeshJobNone;
    meshJobId = -1;
    meshJobCursor = 0;
    MC_PERF_PHASE_END(kPhMeshFinish);
    return true;
}

void CraftWorld::RebuildOpaqueMeshChunk(int id)
{
    RequestMeshRebuild(kMeshJobOpaque, id, false);
}

void CraftWorld::RebuildTransparentMeshChunk(int id)
{
    RequestMeshRebuild(kMeshJobTransparent, id, false);
}

#if MC_PERF_PROBE
void CraftWorld::PerfProbeMeshCache(int id, int times)
{
    if(!MeshIdValid(id))
        return;
    const int StartX = mChunks[id]->chunkStartX;
    const int StartY = mChunks[id]->chunkStartY;
    const int StartZ = mChunks[id]->chunkStartZ;
    for(int i = 0; i < times; i++)
    {
        BeginMeshCaches(StartX, StartY, StartZ);
        EndMeshCaches();
    }
}
#else
void CraftWorld::PerfProbeMeshCache(int id, int times)
{
    (void)id;
    (void)times;
}
#endif


void CraftWorld::UpdateLightAreaIn(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    int z_start, z_end, x_start, x_end, y_start, y_end;
    z-9 >= 0 ? z_start = z-9 : z_start = 0;
    z+9 < worldLimit ? z_end = z+9 : z_end = worldLimit-1;
    x-9 >= 0 ? x_start = x-9 : x_start = 0;
    x+9 < worldLimit ? x_end = x+9 : x_end = worldLimit-1;
    y-9 >= 0 ? y_start = y-9 : y_start = 0;
    y+9 < WORLD_HEIGHT ? y_end = y+9 : y_end = WORLD_HEIGHT-1;

    int lit[8][3];
    int n = 0;
    MC_PERF_INC(kLightScans);
    MC_PERF_ADD(kLightScanCells, (unsigned int)(z_end - z_start + 1) * (unsigned int)(x_end - x_start + 1) * (unsigned int)(y_end - y_start + 1));
    for(int zz = z_start; zz <= z_end; zz++)
    {
        for(int xx = x_start; xx <= x_end; xx++)
        {
            for(int yy = y_start; yy <= y_end; yy++)
            {
                if(LightSourceBlock(GetBlock(xx,yy,zz)) == true)
                {
                    if(n < 8)
                    {
                        lit[n][0] = xx;
                        lit[n][1] = yy;
                        lit[n][2] = zz;
                        n++;
                    }
                }
            }
        }
    }
    for(int i = 0; i < n; i++)
        SetLigtSourcePosition(lit[i][0], lit[i][1], lit[i][2], Torch::getID());
}

void CraftWorld::QueueLightAreaIn(Vector3 pos)
{
    if(pendingLightCount >= 8)
        return;
    pendingLightPos[pendingLightCount] = pos;
    pendingLightCount++;
    MC_PERF_SET(kLightQueueDepth, (unsigned int)pendingLightCount);
}

void CraftWorld::ProcessQueuedLightArea()
{
    if(pendingLightCount <= 0)
        return;
    MC_PERF_INC(kLightProcessed);
    UpdateLightAreaIn(pendingLightPos[0]);
    for(int i = 1; i < pendingLightCount; i++)
        pendingLightPos[i-1] = pendingLightPos[i];
    pendingLightCount--;
}

void CraftWorld::AddChunkToUpdate(const int x, const int y, const int z)
{
    AddChunkToUpdate(getChunkId(Vector3((float)x, (float)y, (float)z)));
}

void CraftWorld::AddChunkToUpdate(int id)
{
    if(id < 0 || id >= (int)mChunks.size())
        return;

    // Bounded, because the drain takes one entry a frame and a burst can outrun it. 256 distinct
    // sections is far above anything ordinary play dirties -- a dig touches one section and its
    // neighbours, and the bursts that come anywhere near it are a portal break (up to 512 cells,
    // AddChunkToUpdate from the portal teardown) or the nether's spreading lava -- so the cap is
    // only here so a runaway producer cannot grow the queue without limit.
    //
    // What is given up when it is reached is the section FARTHEST FROM THE PLAYER, not the oldest.
    // Oldest was the wrong end, and not because of ordering: a burst's entries have no relation to
    // where the player is looking, so discarding the front of the queue discarded work chosen
    // arbitrarily while keeping work also chosen arbitrarily. The cost of a wrong choice is the one
    // this queue already has a documented way to produce -- a section that never gets meshed is a
    // hole in the world until some later block change happens to re-dirty it, which is exactly the
    // "it comes back when I place a block" report. Distance is the right ranking because it is the
    // one TryMeshOneSection and the streamer already use (see the `score` in each), so the queue
    // then agrees with them about which sections matter instead of contradicting them.
    //
    // The newest request is pushed after this, so it is never the one dropped -- the original
    // intent, that the block the player just touched survives, is preserved.
    static const int kUpdateQueueMax = 256;
    if((int)toUpdate.size() > kUpdateQueueMax)
    {
        const int pcx = ChunkFloorDiv((int)playerPos.x, CHUNK_SIZE);
        const int pcz = ChunkFloorDiv((int)playerPos.z, CHUNK_SIZE);
        const int pcy = (int)playerPos.y / CHUNK_SIZE;
        while((int)toUpdate.size() > kUpdateQueueMax)
        {
            int worst = -1;
            int worstScore = -1;
            for(unsigned int i = 0; i < toUpdate.size(); i++)
            {
                SimpleMeshChunk* s = mChunks[toUpdate[i]];
                if(s == NULL)
                {
                    worst = (int)i;     // gone from under us: the cheapest thing to drop
                    break;
                }
                int adx = s->chunkStartX / CHUNK_SIZE - pcx;
                int adz = s->chunkStartZ / CHUNK_SIZE - pcz;
                int ady = s->chunkStartY / CHUNK_SIZE - pcy;
                if(adx < 0) adx = -adx;
                if(adz < 0) adz = -adz;
                if(ady < 0) ady = -ady;
                int score = (adx + adz) * 16 + ady;   // the same shape as TryMeshOneSection's
                if(score > worstScore)
                {
                    worstScore = score;
                    worst = (int)i;
                }
            }
            if(worst < 0)
                break;
            toUpdate.erase(toUpdate.begin() + worst);
        }
    }

    int n = (int)toUpdate.size();
    for(int i = 0; i < n; i++)
    {
        if(toUpdate[i] == id)
            return;
    }
    toUpdate.push_back(id);
    MC_PERF_INC(kToUpdatePushed);
    MC_PERF_SET(kToUpdateDepth, (unsigned int)toUpdate.size());
}

void CraftWorld::AddChunkToFastUpdate(int id)
{
    if(id >= 0 && id < mChunks.size())
    {
        bool push = true;
        if(toFastUpdate.empty() == false)
        {
            for(std::vector<int>::size_type i = 0; i < toFastUpdate.size(); i++)
            {
                if(toFastUpdate[i] == id)
                {
                    push = false;
                    break;
                }
            }
        }

        if(push)
        {
            toFastUpdate.push_back(id);
        }
    }
}

// Redstone is the one part of the world that rewrites itself on a timer, and it is what a clock turns
// into a machine gun: a plain torch clock flips ten times a second, a repeater-timed one more often
// still, and every flip changes the geometry of the cell the dust or the component sits in. Meshing a
// chunk is the single most expensive thing this file does -- `RebuildTransparentMeshChunk` and
// `RebuildOpaqueMeshChunk` each walk all 4096 cells of a 16x16x16 section -- so the engine has a
// standing policy of at most one full chunk rebuild per frame: every queue that feeds
// `RebuildFullMeshChunk` tests `meshBuildsThisFrame == 0` first, and the drains below run in that order.
//
// The redstone paths were the one set of callers that ignored the policy, and they had to: both of the
// queues that existed were wrong for the job. `toUpdate` drains at 0.18 s a chunk and `toFastUpdate` at
// 0.05 s, and a clock feeding either of those is a producer faster than the consumer -- the exact
// backlog the notes at `FlipRepeater` and `EvaluateTorch` describe, which is why those two were moved
// *off* the queues and onto direct calls. A direct call fixes the latency and costs the frame instead:
// two to six synchronous chunk walks per redstone event, inside the frame, uncapped.
//
// A queue drained once per frame is neither of those. It is bounded (one walk per frame, the same
// budget the rest of the engine already spends), and it is an order of magnitude *quicker* than the
// 0.05 s trickle that starved the clocks in the first place, so a running clock's glow appears within a
// frame or two rather than appearing late or not at all. It is also deduplicated, so a clock flipping
// the same cell repeatedly collapses into one entry and cannot grow the queue by itself.
//
// `opaque` is the one thing the queue has to carry beyond a chunk id. A torch, a repeater and a piece
// of wire are all `transparent`: their flip moves no block and changes no vertex colour, so only the
// transparent half of the chunk can differ and walking the opaque half re-derives thousands of vertices
// to redraw a handful. A piston cell is the opposite -- a block id moves *between* the two meshes -- so
// it asks for both.
void CraftWorld::QueueRedstoneMesh(int id, bool opaque)
{
    if(id < 0 || id >= (int)mChunks.size())
    {
        return;
    }

    // The same runaway guard AddChunkToUpdate uses, for the same reason: a damaged save or a bug must
    // not be able to grow this without bound. The redstone walk is itself capped (512 cells), so a
    // legitimate queue is a handful of entries.
    int n = (int)redstoneMeshQueue.size();
    if(n < 0 || n > 64)
    {
        redstoneMeshQueue.clear();
        redstoneMeshOpaque.clear();
        n = 0;
    }
    for(int i = 0; i < n; i++)
    {
        if(redstoneMeshQueue[i] == id)
        {
            // Already queued. Keep the strongest request: if one cell in this chunk needs the opaque pass
            // then the chunk needs both walks, whichever order the two redstone changes arrived in.
            redstoneMeshOpaque[i] = (char)(redstoneMeshOpaque[i] || opaque);
            return;
        }
    }

    redstoneMeshQueue.push_back(id);
    redstoneMeshOpaque.push_back((char)opaque);
    MC_PERF_INC(kRedstonePushed);
    MC_PERF_SET(kRedstoneDepth, (unsigned int)redstoneMeshQueue.size());
}

void CraftWorld::QueueRedstoneMesh(int x, int y, int z, bool opaque)
{
    int id = getChunkId(Vector3((float)x, (float)y, (float)z));
    if(id != -1)
    {
        QueueRedstoneMesh(id, opaque);
    }
}

// One chunk per frame, in arrival order, and only on a frame the other rebuilders did not claim -- this
// is called last on purpose so world loading and lighting keep their claim on the budget and a running
// clock simply waits for a frame they left free.
void CraftWorld::FlushRedstoneMeshes()
{
    if(redstoneMeshQueue.empty())
    {
        return;
    }

    const int id = redstoneMeshQueue[0];
    const bool opaque = redstoneMeshOpaque[0] != 0;
    redstoneMeshQueue.erase(redstoneMeshQueue.begin());
    redstoneMeshOpaque.erase(redstoneMeshOpaque.begin());
    MC_PERF_INC(kRedstoneDrained);

    if(!MeshIdValid(id))
    {
        return;
    }

    if(opaque)
    {
        // Both halves, through the shared walk. A clock asks for both on nearly every flip -- a torch's
        // light reaches the opaque half of its own section -- so this is the path a running clock takes,
        // and the pair of public rebuilds it used to be costs an extra light cache and an extra 4,096
        // cell step for the same geometry (28 + 25 us against 34 for both; see RequestMeshRebuild).
        // Two things the full-rebuild caller does are deliberately left out here: the plant scan, which a
        // redstone change cannot change the answer to, and the two-frame settle (`sSkipQueuedRebuilds`),
        // which would stall this very queue -- the drain is already limited to one section a frame.
        RequestMeshRebuild(kMeshJobFull, id, false);
    }
    else
    {
        RebuildTransparentMeshChunk(id);
    }
}


void CraftWorld::rebuildNearestChunksForLight(int id,Vector3 pos)
{
    WakeBuriedAround((int)pos.x, (int)pos.y, (int)pos.z);
    if(!MeshIdValid(id))
        return;
    // The unused `bBox.onBorder` read that stood here is gone. It was not the border test this function
    // decided with -- the margin comparisons on `posInChunk` below are -- and it was the same call that
    // could not see the +X/+Y/+Z side, so leaving it in place was leaving a reader two answers to the
    // same question with only one of them right. See SectionBorderOffset.
    Vector3 temp = pos;
    Vector3 posInChunk = pos-Vector3(mChunks[id]->chunkStartX,mChunks[id]->chunkStartY,mChunks[id]->chunkStartZ);

    int chunkNumber = -1;
    ///X
    if(posInChunk.x < 10)
    {
        chunkNumber = getChunkId(Vector3(temp.x-CHUNK_SIZE/2,temp.y,temp.z));
        if(chunkNumber != -1)
        {
            AddChunkToUpdate(chunkNumber);
        }

        // diagonale
        if(posInChunk.z < 10)
        {
            chunkNumber = getChunkId(Vector3(temp.x-CHUNK_SIZE/2,temp.y,temp.z-CHUNK_SIZE/2));
            if(chunkNumber != -1)
            {
                AddChunkToUpdate(chunkNumber);
            }

            if(posInChunk.y < 10)
            {
                chunkNumber = getChunkId(Vector3(temp.x-CHUNK_SIZE/2,temp.y-CHUNK_SIZE/2,temp.z-CHUNK_SIZE/2));
                if(chunkNumber != -1)
                {
                    AddChunkToUpdate(chunkNumber);
                }
            }
            if(posInChunk.y > CHUNK_SIZE-10)
            {
                chunkNumber = getChunkId(Vector3(temp.x-CHUNK_SIZE/2,temp.y+CHUNK_SIZE/2,temp.z-CHUNK_SIZE/2));
                if(chunkNumber != -1)
                {
                    AddChunkToUpdate(chunkNumber);
                }
            }
        }
        if(posInChunk.z > CHUNK_SIZE-10)
        {
            chunkNumber = getChunkId(Vector3(temp.x-CHUNK_SIZE/2,temp.y,temp.z+CHUNK_SIZE/2));
            if(chunkNumber != -1)
            {
                AddChunkToUpdate(chunkNumber);
            }

            if(posInChunk.y < 10)
            {
                chunkNumber = getChunkId(Vector3(temp.x-CHUNK_SIZE/2,temp.y-CHUNK_SIZE/2,temp.z+CHUNK_SIZE/2));
                if(chunkNumber != -1)
                {
                    AddChunkToUpdate(chunkNumber);
                }
            }
            if(posInChunk.y > CHUNK_SIZE-10)
            {
                chunkNumber = getChunkId(Vector3(temp.x-CHUNK_SIZE/2,temp.y+CHUNK_SIZE/2,temp.z+CHUNK_SIZE/2));
                if(chunkNumber != -1)
                {
                    AddChunkToUpdate(chunkNumber);
                }
            }
        }
    }
    if(posInChunk.x > CHUNK_SIZE-10)
    {
        chunkNumber = getChunkId(Vector3(temp.x+CHUNK_SIZE/2,temp.y,temp.z));
        if(chunkNumber != -1)
        {
            AddChunkToUpdate(chunkNumber);
        }

        // diagonale
        if(posInChunk.z < 10)
        {
            chunkNumber = getChunkId(Vector3(temp.x+CHUNK_SIZE/2,temp.y,temp.z-CHUNK_SIZE/2));
            if(chunkNumber != -1)
            {
                AddChunkToUpdate(chunkNumber);
            }

            if(posInChunk.y < 10)
            {
                chunkNumber = getChunkId(Vector3(temp.x+CHUNK_SIZE/2,temp.y-CHUNK_SIZE/2,temp.z-CHUNK_SIZE/2));
                if(chunkNumber != -1)
                {
                    AddChunkToUpdate(chunkNumber);
                }
            }
            if(posInChunk.y > CHUNK_SIZE-10)
            {
                chunkNumber = getChunkId(Vector3(temp.x+CHUNK_SIZE/2,temp.y+CHUNK_SIZE/2,temp.z-CHUNK_SIZE/2));
                if(chunkNumber != -1)
                {
                    AddChunkToUpdate(chunkNumber);
                }
            }
        }
        if(posInChunk.z > CHUNK_SIZE-10)
        {
            chunkNumber = getChunkId(Vector3(temp.x+CHUNK_SIZE/2,temp.y,temp.z+CHUNK_SIZE/2));
            if(chunkNumber != -1)
            {
                AddChunkToUpdate(chunkNumber);
            }

            if(posInChunk.y < 10)
            {
                chunkNumber = getChunkId(Vector3(temp.x+CHUNK_SIZE/2,temp.y-CHUNK_SIZE/2,temp.z+CHUNK_SIZE/2));
                if(chunkNumber != -1)
                {
                    AddChunkToUpdate(chunkNumber);
                }
            }
            if(posInChunk.y > CHUNK_SIZE-10)
            {
                chunkNumber = getChunkId(Vector3(temp.x+CHUNK_SIZE/2,temp.y+CHUNK_SIZE/2,temp.z+CHUNK_SIZE/2));
                if(chunkNumber != -1)
                {
                    AddChunkToUpdate(chunkNumber);
                }
            }
        }
    }

    ///Y
    if(posInChunk.y < 10)
    {
        chunkNumber = getChunkId(Vector3(temp.x,temp.y-CHUNK_SIZE/2,temp.z));
        if(chunkNumber != -1)
        {
            AddChunkToUpdate(chunkNumber);
        }
    }
    if(posInChunk.y > CHUNK_SIZE-10)
    {
        chunkNumber = getChunkId(Vector3(temp.x,temp.y+CHUNK_SIZE/2,temp.z));
        if(chunkNumber != -1)
        {
            AddChunkToUpdate(chunkNumber);
        }
    }

    ///Z
    if(posInChunk.z < 10)
    {
        chunkNumber = getChunkId(Vector3(temp.x,temp.y,temp.z-CHUNK_SIZE/2));
        if(chunkNumber != -1)
        {
            AddChunkToUpdate(chunkNumber);
        }
    }
    if(posInChunk.z > CHUNK_SIZE-10)
    {
        chunkNumber = getChunkId(Vector3(temp.x,temp.y,temp.z+CHUNK_SIZE/2));
        if(chunkNumber != -1)
        {
            AddChunkToUpdate(chunkNumber);
        }
    }
}

// Which face-neighbours of a cell fall outside the section the cell is in, as ±1 per axis and 0 where
// the neighbour is in the same section -- the shape the two functions below want, and the shape
// `BoundingBox::onBorder` was being asked for.
//
// It is stated here rather than read off `mChunks[id]->bBox` because **the box cannot answer it**. A
// section's box runs `[start, start + CHUNK_SIZE]`, so its *maximum* is the first cell of the next
// section, and no cell of this one is ever at it: `onBorder`'s `max` test (which fires on `max - pt == 0`)
// could not be true for any point a caller owned, so the -X, -Y and -Z neighbours were queued for a
// rebuild and the +X, +Y and +Z ones never were. The `min` test worked, because the first cell of a
// section *is* at the section's minimum -- which is exactly what made this read as a working border test.
//
// That asymmetry is the boundary x-ray. Dig down a shaft that straddles a 16 line -- which the player
// cannot see, and a 1x1 shaft put down "on a corner" crosses it about half the time -- and every block
// you remove takes the face it was hiding off a block whose *own* section is never remeshed. Its mesh
// still has that face culled from when it was buried, so it is not drawn, and you see through the block
// instead: one cell wide, straight down the seam, for as deep as you dig. Breaking the block on the
// other side of the same seam does not show it, because that half of the test was never broken.
Vector3 CraftWorld::SectionBorderOffset(int id, int x, int y, int z)
{
    Vector3 result(0,0,0);
    if(!MeshIdValid(id))
    {
        return result;
    }

    // Local to the section, and only meaningful while the cell is inside it -- a cell from anywhere else
    // falls outside 0..CHUNK_SIZE-1 on at least one axis and simply raises no flag, which is the honest
    // answer for a caller whose position and section id disagree.
    const int lx = x - mChunks[id]->chunkStartX;
    const int ly = y - mChunks[id]->chunkStartY;
    const int lz = z - mChunks[id]->chunkStartZ;

    // The last cell of a section has its far neighbour in the next section, which is the case the box
    // test lost. CHUNK_SIZE-1 rather than CHUNK_SIZE is the whole of the fix.
    if(lx == 0)                   result.x = -1.0f;
    else if(lx == CHUNK_SIZE - 1) result.x =  1.0f;

    if(ly == 0)                   result.y = -1.0f;
    else if(ly == CHUNK_SIZE - 1) result.y =  1.0f;

    if(lz == 0)                   result.z = -1.0f;
    else if(lz == CHUNK_SIZE - 1) result.z =  1.0f;

    return result;
}

void CraftWorld::rebuildNearestTransparentChunks(int id,Vector3 pos)
{
    Vector3 test = SectionBorderOffset(id, (int)pos.x, (int)pos.y, (int)pos.z);
    Vector3 temp = pos;
    int chunkNumber = -1;

    if(test.x != 0)
    {
        temp.x +=test.x;

        chunkNumber = getChunkId(temp);
        if(chunkNumber != -1)
        {
            RebuildTransparentMeshChunk(chunkNumber);
        }

        if(test.y != 0)
        {
            temp.y +=test.y;
            chunkNumber = getChunkId(temp);
            if(chunkNumber != -1)
            {
                RebuildTransparentMeshChunk(chunkNumber);
            }
        }

        if(test.z != 0)
        {
            temp.z +=test.z;
            chunkNumber = getChunkId(temp);
            if(chunkNumber != -1)
            {
                RebuildTransparentMeshChunk(chunkNumber);
            }
        }
    }


    if(test.y != 0)
    {
        temp = pos;
        temp.y +=test.y;

        chunkNumber = getChunkId(temp);
        if(chunkNumber != -1)
        {
            RebuildTransparentMeshChunk(chunkNumber);
        }

        if(test.z != 0)
        {
            temp.z +=test.z;
            chunkNumber = getChunkId(temp);
            if(chunkNumber != -1)
            {
                RebuildTransparentMeshChunk(chunkNumber);
            }
        }
    }

    if(test.z != 0)
    {
        temp = pos;
        temp.z +=test.z;

        chunkNumber = getChunkId(temp);
        if(chunkNumber != -1)
        {
            RebuildTransparentMeshChunk(chunkNumber);
        }

        if(test.x != 0)
        {
            temp.x +=test.x;

            chunkNumber = getChunkId(temp);
            if(chunkNumber != -1)
            {
                RebuildTransparentMeshChunk(chunkNumber);
            }
        }
    }
}

void CraftWorld::WakeBuriedSections(int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
{
    if(CHUNK_SIZE <= 0)
        return;
    if(minY < 0) minY = 0;
    if(maxY >= WORLD_HEIGHT) maxY = WORLD_HEIGHT - 1;
    if(maxY < minY)
        return;
    int cx0 = ChunkFloorDiv(minX, CHUNK_SIZE), cx1 = ChunkFloorDiv(maxX, CHUNK_SIZE);
    int cz0 = ChunkFloorDiv(minZ, CHUNK_SIZE), cz1 = ChunkFloorDiv(maxZ, CHUNK_SIZE);
    int cy0 = minY / CHUNK_SIZE, cy1 = maxY / CHUNK_SIZE;
    if(cy1 > 15) cy1 = 15;
    for(int cz = cz0; cz <= cz1; cz++)
    {
        for(int cx = cx0; cx <= cx1; cx++)
        {
            ChunkColumn* col = FindColumn(cx, cz);
            if(!col)
                continue;
            for(int cy = cy0; cy <= cy1; cy++)
            {
                if(col->meshId[cy] == -2)
                {
                    col->meshId[cy] = -1;
                    meshPending = true;
                }
            }
        }
    }
}

void CraftWorld::WakeBuriedAround(int x, int y, int z)
{
    MC_PERF_INC(kWakeBuried);
    WakeBuriedSections(x - 1, y - 1, z - 1, x + 1, y + 1, z + 1);
}

void CraftWorld::rebuildNearestChunks(int id,Vector3 pos)
{
    WakeBuriedAround((int)pos.x, (int)pos.y, (int)pos.z);
    if(!MeshIdValid(id))
        return;
    Vector3 test = SectionBorderOffset(id, (int)pos.x, (int)pos.y, (int)pos.z);
    Vector3 temp = pos;
    int chunkNumber = -1;

    if(test.x != 0)
    {
        temp.x +=test.x;

        chunkNumber = getChunkId(temp);
        if(chunkNumber != -1)
        {
            AddChunkToUpdate(chunkNumber);
        }

        if(test.y != 0)
        {
            temp.y +=test.y;
            chunkNumber = getChunkId(temp);
            if(chunkNumber != -1)
            {
                AddChunkToUpdate(chunkNumber);
            }
        }

        if(test.z != 0)
        {
            temp.z +=test.z;
            chunkNumber = getChunkId(temp);
            if(chunkNumber != -1)
            {
                AddChunkToUpdate(chunkNumber);
            }
        }
    }


    if(test.y != 0)
    {
        temp = pos;
        temp.y +=test.y;

        chunkNumber = getChunkId(temp);
        if(chunkNumber != -1)
        {
            AddChunkToUpdate(chunkNumber);
        }

        if(test.z != 0)
        {
            temp.z +=test.z;
            chunkNumber = getChunkId(temp);
            if(chunkNumber != -1)
            {
                AddChunkToUpdate(chunkNumber);
            }
        }
    }

    if(test.z != 0)
    {
        temp = pos;
        temp.z +=test.z;

        chunkNumber = getChunkId(temp);
        if(chunkNumber != -1)
        {
            AddChunkToUpdate(chunkNumber);
        }

        if(test.x != 0)
        {
            temp.x +=test.x;

            chunkNumber = getChunkId(temp);
            if(chunkNumber != -1)
            {
                AddChunkToUpdate(chunkNumber);
            }
        }
    }
}

void CraftWorld::createWorldChunkss()
{
    createdChunksCount = 0;
    for(unsigned int i = 0; i < loadedColumns.size(); i++)
        CreateMeshesForColumn(loadedColumns[i]);
    FetchNearestChunks();
    int meshCount = 0;
    for(unsigned int i = 0; i < mChunks.size(); i++)
    {
        if(MeshIdValid(i))
            meshCount++;
    }
    Aurora::Utils::Logger::Instance()->LogMessage("mesh build columns=%d meshes=%d heapUsedKB=%d heapFreeKB=%d\n",
        (int)loadedColumns.size(), meshCount, HeapUsedKB(), HeapFreeKB());
    MC_DebugHeapCheckpoint("mesh build");
}

void CraftWorld::UpdateChunkBlocks(int id)
{
    if(!MeshIdValid(id))
        return;
    SimpleMeshChunk* MeshChunk = mChunks[id];

    int StartZ = MeshChunk->chunkStartZ;
    int StartY = MeshChunk->chunkStartY;
    int StartX = MeshChunk->chunkStartX;

    for (int z = StartZ; z < CHUNK_SIZE + StartZ; ++z)
    {
        for (int y = StartY; y < CHUNK_SIZE + StartY; ++y)
        {
            for (int x = StartX; x < CHUNK_SIZE + StartX; ++x)
            {
                block_t& block = GetBlock(x,y,z);

                if(block == 0)
                {
                    continue;
                }

                // snow layer needs a full block under it; drop it if that block went away
                if(block == Snow2::getID())
                {
                    block_t under = (y > 0) ? GetBlockNoCheck(x, y - 1, z) : 0;
                    if(under == 0 || under == Snow2::getID() || (under < blockTypes.size() && blockTypes[under].blockModel != 0))
                    {
                        block = 0;
                        continue;
                    }
                }

                if(worldWeather->itIsSnowing && worldWeather->GetWeatherType() == 1 && y != CHUNK_SIZE + StartY-1)
                {
                    if(rand() % 20 < 3)
                    {
                        if(blockTypes[block].blockModel == 0) // snow can be laid only on normal blocks
                        {
                            if(GetBlockNoCheck(x,y+1,z) == 0)
                            {
                                if(GetShadowLevel(x,y+1,z) == 0)
                                {
                                   GetBlock(x,y+1,z) = Snow2::getID();
                                }
                            }
                        }
                    }
                }

                if(block == DirtBlock::getID() && GetShadowLevel(x,y,z) == 0)
                {
                    if(rand() % 6 == 0)
                    {
                        if(BlockTransparentOrSpecial(x,y+1,z) == true)
                        {
                            block = GrassBlock::getID();
                            if(block == Snow2::getID())
                            {
                                block = SnowSoil::getID();
                            }
                            continue;
                        }
                    }
                }
                if(block == GrassBlock::getID() && (GetShadowLevel(x,y,z) > 6 || BlockTransparentOrSpecial(x,y+1,z) == false))
                {
                    block = DirtBlock::getID();
                    continue;
                }

                if(block == CaneBlock::getID() && GetBlock(x,y+1,z) == 0 && GetBlock(x,y-3,z) != 33 && GetBlock(x,y-3,z) != 0)
                {
                    if(rand() % 100 > 92)
                    {
                        GetBlock(x,y+1,z) = 33;
                        continue;
                    }
                }

                if(block == Soil::getID())
                {
                    if(BlockTransparent(x,y+1,z) == false)
                    {
                        if(rand() % 5 == 0)
                        {
                            block = DirtBlock::getID();
                        }
                    }
                    continue;
                }
                if(block == WateredSoil::getID())
                {
                    if(BlockTransparent(x,y+1,z) == false)
                    {
                        if(rand() % 5 == 0)
                        {
                            block = DirtBlock::getID();
                        }
                    }
                    continue;
                }

                if(block == Mooshroom1::getID())
                {
                    if(BlockFinalLight(x,y,z) > 0.5f) // Mooshrooms can grow only in dark places
                        return;

                    if(rand() % 30 == 0)
                    {
                        if(rand() % 3 == 0)
                        {
                            if(BlockSolid(x+1,y-1,z) == true && GetBlock(x+1,y,z) == 0)
                            {
                                GetBlock(x+1,y,z) = Mooshroom1::getID();
                            }
                        }
                        if(rand() % 3 == 0)
                        {
                            if(BlockSolid(x-1,y-1,z) == true && GetBlock(x-1,y,z) == 0)
                            {
                                GetBlock(x-1,y,z) = Mooshroom1::getID();
                            }
                        }
                        if(rand() % 3 == 0)
                        {
                            if(BlockSolid(x,y-1,z-1) == true && GetBlock(x,y,z-1) == 0)
                            {
                                GetBlock(x,y,z-1) = Mooshroom1::getID();
                            }
                        }
                        if(rand() % 3 == 0)
                        {
                            if(BlockSolid(x,y-1,z+1) == true && GetBlock(x,y,z+1) == 0)
                            {
                                GetBlock(x,y,z+1) = Mooshroom1::getID();
                            }
                        }
                    }
                    continue;
                }
                if(block == Mooshroom2::getID())
                {
                    if(BlockFinalLight(x,y,z) > 0.5f) // Mooshrooms can grow only in dark places
                        return;

                    if(rand() % 30 == 0)
                    {
                        if(rand() % 3 == 0)
                        {
                            if(BlockSolid(x+1,y-1,z) == true && GetBlock(x+1,y,z) == 0)
                            {
                                GetBlock(x+1,y,z) = Mooshroom2::getID();
                            }
                        }
                        if(rand() % 3 == 0)
                        {
                            if(BlockSolid(x-1,y-1,z) == true && GetBlock(x-1,y,z) == 0)
                            {
                                GetBlock(x-1,y,z) = Mooshroom2::getID();
                            }
                        }
                        if(rand() % 3 == 0)
                        {
                            if(BlockSolid(x,y-1,z-1) == true && GetBlock(x,y,z-1) == 0)
                            {
                                GetBlock(x,y,z-1) = Mooshroom2::getID();
                            }
                        }
                        if(rand() % 3 == 0)
                        {
                            if(BlockSolid(x,y-1,z+1) == true && GetBlock(x,y,z+1) == 0)
                            {
                                GetBlock(x,y,z+1) = Mooshroom2::getID();
                            }
                        }
                    }
                    continue;
                }

                if(block == Lava::getID() && GetBlock(x+2,y,z) == WaterBlock::getID() && GetBlock(x+1,y,z) == 0)
                {
                    GetBlock(x+1,y,z) = CobbleStone::getID(); //make coblestone
                    continue;
                }
                if(block == Lava::getID() && GetBlock(x-2,y,z) == WaterBlock::getID() && GetBlock(x-1,y,z) == 0)
                {
                    GetBlock(x-1,y,z) = CobbleStone::getID();
                    continue;
                }

                if(block == Lava::getID() && GetBlock(x,y,z+2) == WaterBlock::getID() && GetBlock(x,y,z+1) == 0)
                {
                    GetBlock(x,y,z+1) = Obsidian::getID(); //make obsidian
                    continue;
                }
                if(block == Lava::getID() && GetBlock(x,y,z-2) == WaterBlock::getID() && GetBlock(x,y,z-1) == 0)
                {
                    GetBlock(x,y,z-1) = Obsidian::getID();
                    continue;
                }
            }
        }
    }
}


void CraftWorld::UpdateChunkGrowingBlocks(int id, int vectorId)
{
    if(!MeshIdValid(id))
        return;
    SimpleMeshChunk* MeshChunk = mChunks[id];

    int StartZ = MeshChunk->chunkStartZ;
    int StartY = MeshChunk->chunkStartY;
    int StartX = MeshChunk->chunkStartX;

    bool somethingWasUpdated = false;
    bool waterFoundNearBy = false;

    int water_x, water_y, water_z;
    water_x = -1;
    water_y = -1;
    water_z = -1;

    for (int z = StartZ; z < CHUNK_SIZE + StartZ; ++z)
    {
        for (int y = StartY; y < CHUNK_SIZE + StartY; ++y)
        {
            for (int x = StartX; x < CHUNK_SIZE + StartX; ++x)
            {
                if (BlockUpdate2(x,y,z) == 0)
                {
                    continue;
                }

                int block = GetBlock(x,y,z);
                float block_light = BlockFinalLight(x,y,z);

                if(block == 0)
                {
                    continue;
                }
                if(x > 0 && x < worldLimit && y > 0 && y < WORLD_HEIGHT && z > 0 && z < worldLimit)
                {
                    if(block == Soil::getID() || block == WateredSoil::getID())
                    {
                        if(water_x != -1 && water_z != -1)
                        {
                            BoundingBox hitBox = BoundingBox(Vector3(water_x - 2,water_y - 1,water_z - 2),Vector3(water_x + 2,water_y + 1,water_z + 2));
                            if(hitBox.contains(Vector3(x,y,z)) == false)
                            {
                                waterFoundNearBy = false;
                            }
                        }

                        if(waterFoundNearBy == false)
                        {
                            for(int x2 = x - 2; x2 <= x+2; x2++)
                            {
                                if(waterFoundNearBy)
                                {
                                    break;
                                }
                                for(int z2 = z - 2; z2 <= z+2; z2++)
                                {
                                    if(GetBlock(x2,y,z2) == WaterBlock::getID())
                                    {
                                        waterFoundNearBy = true;
                                        water_x = x2;
                                        water_y = y;
                                        water_z = z2;
                                    }
                                    if(waterFoundNearBy)
                                    {
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if(block == Soil::getID())
                    {
                        if(!worldWeather->itIsRaining) // if it is clear weather
                        {
                            if(waterFoundNearBy)
                            {
                                if(rand() % 10 > 3)
                                {
                                    GetBlock(x,y,z) = WateredSoil::getID();
                                    somethingWasUpdated = true;
                                }
                            }
                            continue;
                        }
                        else
                        {
                            if(rand() % 10 > 3)
                            {
                                GetBlock(x,y,z) = WateredSoil::getID();
                                somethingWasUpdated = true;
                            }
                        }
                    }
                    if(block == WateredSoil::getID())
                    {
                        if(!waterFoundNearBy)
                        {
                            if(rand() % 10 > 3)
                            {
                                GetBlock(x,y,z) = Soil::getID();
                                somethingWasUpdated = true;
                            }
                        }
                        continue;
                    }

                    if(block == Soil::getID())
                    {
                        if(BlockTransparent(x,y+1,z) == false)
                        {
                            if(rand() % 5 == 0)
                            {
                                GetBlock(x,y,z) = DirtBlock::getID();
                            }
                        }
                        continue;
                    }
                    if(block == WateredSoil::getID())
                    {
                        if(BlockTransparent(x,y+1,z) == false)
                        {
                            if(rand() % 5 == 0)
                            {
                                GetBlock(x,y,z) = DirtBlock::getID();
                            }
                        }
                        continue;
                    }

                    // pumpkin plants
                    if(block >= PumpkinPlant1::getID() && block <= PumpkinPlant3::getID())
                    {
                        somethingWasUpdated = true;
                        if(GetBlock(x,y-1,z) == Soil::getID())
                        {
                            if(rand() % 100 <= 10)
                            {
                                GetBlock(x,y,z) = block+1;
                                continue;
                            }
                        }
                        if(GetBlock(x,y-1,z) == WateredSoil::getID())
                        {
                            if(rand() % 100 <= 14)
                            {
                                GetBlock(x,y,z) = block+1;
                                continue;
                            }
                        }
                    }

                    if(block == PumpkinPlant4::getID())
                    {
                        somethingWasUpdated = true;
                        bool canGrow = true;
                        if(GetBlock(x-1,y,z) == Pumpkin1::getID() || GetBlock(x+1,y,z) == Pumpkin1::getID() || GetBlock(x,y,z-1) == Pumpkin1::getID() || GetBlock(x,y,z+1) == Pumpkin1::getID())
                        {
                            canGrow = false;
                        }
                        if(rand() % 7 == 0 && canGrow)
                        {
                            switch(rand() % 4)
                            {
                            case 0:
                                if(GetBlock(x-1,y,z) == 0)
                                {
                                    GetBlock(x-1,y,z) = Pumpkin1::getID();
                                }
                            break;
                            case 1:
                                if(GetBlock(x+1,y,z) == 0)
                                {
                                    GetBlock(x+1,y,z) = Pumpkin1::getID();
                                }
                            break;
                            case 2:
                                if(GetBlock(x,y,z-1) == 0)
                                {
                                    GetBlock(x,y,z-1) = Pumpkin1::getID();
                                }
                            break;
                            case 3:
                                if(GetBlock(x,y,z+1) == 0)
                                {
                                    GetBlock(x,y,z+1) = Pumpkin1::getID();
                                }
                            break;
                            }
                        }
                        continue;
                    }


                    // melon plants
                    if(block == MelonPlant1::getID() && block <= MelonPlant3::getID())
                    {
                        somethingWasUpdated = true;
                        if(GetBlock(x,y-1,z) == Soil::getID())
                        {
                            if(rand() % 100 <= 10)
                            {
                                GetBlock(x,y,z) = block+1;
                                continue;
                            }
                        }
                        if(GetBlock(x,y-1,z) == WateredSoil::getID())
                        {
                            if(rand() % 100 <= 14)
                            {
                                GetBlock(x,y,z) = block+1;
                                continue;
                            }
                        }
                    }

                    if(block == MelonPlant4::getID())
                    {
                        somethingWasUpdated = true;
                        bool canGrow = true;
                        if(GetBlock(x-1,y,z) == MelonBlock::getID() || GetBlock(x+1,y,z) == MelonBlock::getID() || GetBlock(x,y,z-1) == MelonBlock::getID() || GetBlock(x,y,z+1) == MelonBlock::getID())
                        {
                            canGrow = false;
                        }
                        if(rand() % 7 == 0 && canGrow)
                        {
                            switch(rand() % 4)
                            {
                            case 0:
                                if(GetBlock(x-1,y,z) == 0)
                                {
                                    GetBlock(x-1,y,z) = MelonBlock::getID();
                                }
                            break;
                            case 1:
                                if(GetBlock(x+1,y,z) == 0)
                                {
                                    GetBlock(x+1,y,z) = MelonBlock::getID();
                                }
                            break;
                            case 2:
                                if(GetBlock(x,y,z-1) == 0)
                                {
                                    GetBlock(x,y,z-1) = MelonBlock::getID();
                                }
                            break;
                            case 3:
                                if(GetBlock(x,y,z+1) == 0)
                                {
                                    GetBlock(x,y,z+1) = MelonBlock::getID();
                                }
                            break;
                            }
                        }
                        continue;
                    }

                    // wheat
                    if(block >= WheatBlock1::getID() && block <= WheatBlock5::getID())
                    {
                        somethingWasUpdated = true;
                        if(block_light >= 0.33f)
                        {
                            if(GetBlock(x,y-1,z) == Soil::getID())
                            {
                                if(rand() % 100 > 86)
                                {
                                    if(block == WheatBlock5::getID())
                                    {
                                        mainStatistics.cropsGrowned += 1;
                                    }
                                    GetBlock(x,y,z) = block+1;
                                    continue;
                                }
                            }
                            if(GetBlock(x,y-1,z) == WateredSoil::getID())
                            {
                                if(rand() % 100 > 78)
                                {
                                    if(block == WheatBlock5::getID())
                                    {
                                        mainStatistics.cropsGrowned += 1;
                                    }
                                    GetBlock(x,y,z) = block+1;
                                    continue;
                                }
                            }
                        }
                        continue;
                    }

                    // potato
                    if(block >= PotatoPlant1::getID() && block <= PotatoPlant3::getID())
                    {
                        somethingWasUpdated = true;
                        if(block_light >= 0.33f)
                        {
                            if(GetBlock(x,y-1,z) == Soil::getID())
                            {
                                if(rand() % 100 > 93)
                                {
                                    if(block == PotatoPlant3::getID())
                                    {
                                        mainStatistics.cropsGrowned += 1;
                                    }
                                    GetBlock(x,y,z) = block+1;
                                    continue;
                                }
                            }
                            if(GetBlock(x,y-1,z) == WateredSoil::getID())
                            {
                                if(rand() % 100 > 87)
                                {
                                    if(block == PotatoPlant3::getID())
                                    {
                                        mainStatistics.cropsGrowned += 1;
                                    }
                                    GetBlock(x,y,z) = block+1;
                                    continue;
                                }
                            }
                        }
                        continue;
                    }

                    // carrot
                    if(block >= CarrotPlant1::getID() && block <= CarrotPlant3::getID())
                    {
                        somethingWasUpdated = true;
                        if(block_light >= 0.33f)
                        {
                            if(GetBlock(x,y-1,z) == Soil::getID())
                            {
                                if(rand() % 100 > 92)
                                {
                                    if(block == CarrotPlant3::getID())
                                    {
                                        mainStatistics.cropsGrowned += 1;
                                    }
                                    GetBlock(x,y,z) = block+1;
                                    continue;
                                }
                            }
                            if(GetBlock(x,y-1,z) == WateredSoil::getID())
                            {
                                if(rand() % 100 > 86)
                                {
                                    if(block == CarrotPlant3::getID())
                                    {
                                        mainStatistics.cropsGrowned += 1;
                                    }
                                    GetBlock(x,y,z) = block+1;
                                    continue;
                                }
                            }
                        }
                        continue;
                    }

                    if(block == Mooshroom1::getID())
                    {
                        if(block_light > 0.5f) // Mooshrooms can grow only in dark places
                            return;

                        if(rand() % 30 == 0)
                        {
                            if(rand() % 3 == 0)
                            {
                                if(BlockSolid(x+1,y-1,z) == true && GetBlock(x+1,y,z) == 0)
                                {
                                    GetBlock(x+1,y,z) = Mooshroom1::getID();
                                }
                            }
                            if(rand() % 3 == 0)
                            {
                                if(BlockSolid(x-1,y-1,z) == true && GetBlock(x-1,y,z) == 0)
                                {
                                    GetBlock(x-1,y,z) = Mooshroom1::getID();
                                }
                            }
                            if(rand() % 3 == 0)
                            {
                                if(BlockSolid(x,y-1,z-1) == true && GetBlock(x,y,z-1) == 0)
                                {
                                    GetBlock(x,y,z-1) = Mooshroom1::getID();
                                }
                            }
                            if(rand() % 3 == 0)
                            {
                                if(BlockSolid(x,y-1,z+1) == true && GetBlock(x,y,z+1) == 0)
                                {
                                    GetBlock(x,y,z+1) = Mooshroom1::getID();
                                }
                            }
                        }
                        continue;
                    }
                    if(block == Mooshroom2::getID())
                    {
                        if(block_light > 0.5f) // Mooshrooms can grow only in dark places
                            return;

                        if(rand() % 30 == 0)
                        {
                            if(rand() % 3 == 0)
                            {
                                if(BlockSolid(x+1,y-1,z) == true && GetBlock(x+1,y,z) == 0)
                                {
                                    GetBlock(x+1,y,z) = Mooshroom2::getID();
                                }
                            }
                            if(rand() % 3 == 0)
                            {
                                if(BlockSolid(x-1,y-1,z) == true && GetBlock(x-1,y,z) == 0)
                                {
                                    GetBlock(x-1,y,z) = Mooshroom2::getID();
                                }
                            }
                            if(rand() % 3 == 0)
                            {
                                if(BlockSolid(x,y-1,z-1) == true && GetBlock(x,y,z-1) == 0)
                                {
                                    GetBlock(x,y,z-1) = Mooshroom2::getID();
                                }
                            }
                            if(rand() % 3 == 0)
                            {
                                if(BlockSolid(x,y-1,z+1) == true && GetBlock(x,y,z+1) == 0)
                                {
                                    GetBlock(x,y,z+1) = Mooshroom2::getID();
                                }
                            }
                        }
                        continue;
                    }

                    if(block == CaneBlock::getID() && GetBlock(x,y+1,z) == 0 && GetBlock(x,y-3,z) != 33 && GetBlock(x,y-3,z) != 0)
                    {
                        somethingWasUpdated = true;
                        if(rand() % 20 < 3)
                        {
                            GetBlock(x,y+1,z) = 33;
                        }
                        continue;
                    }

                    /// oak tree growing
                    if(block == 121 && (GetBlock(x,y-1,z) == 1 || GetBlock(x,y-1,z) == 2))
                    {
                        somethingWasUpdated = true;
                        if((rand() % 20 == 0 && (GetBlock(x,y-1,z) == 1 || GetBlock(x,y-1,z) == 2)) || (rand() % 32 == 0  && GetBlock(x,y-1,z) == 29))
                        {
                            mainStatistics.treesGrowned += 1;
                            int height = 1 + rand() % 2;

                            for(int x2 = x - 2; x2 <= x + 2; x2 ++)
                            {
                                for(int z2 = z - 2; z2 <= z + 2; z2 ++)
                                {
                                    for(int y2 = y+height; y2 <= y+height+1; y2 ++)
                                    {
                                        if(GetBlock(x2,y2,z2) == 0)
                                        {
                                            GetBlock(x2,y2,z2) = 9;
                                        }
                                    }
                                }
                            }

                            for(int x2 = x - 1; x2 <= x + 1; x2 ++)
                            {
                                for(int z2 = z - 1; z2 <= z + 1; z2 ++)
                                {
                                    int y2 = y+height+2;
                                    if(rand() % 4 <= 1)
                                    {
                                        if(GetBlock(x2,y2,z2) == 0)
                                        {
                                            GetBlock(x2,y2,z2) = 9;
                                        }
                                    }
                                }
                            }

                            for(int x2 = x - 1; x2 <= x + 1; x2 ++)
                            {
                                for(int y2 = y+height+2; y2 <= y+height+3; y2 ++)
                                {
                                    if(GetBlock(x2,y2,z) == 0)
                                    {
                                        GetBlock(x2,y2,z) = 9;
                                    }
                                }
                            }

                            for(int z2 = z - 1; z2 <= z + 1; z2 ++)
                            {
                                for(int y2 = y+height+2; y2 <= y+height+3; y2 ++)
                                {
                                    if(GetBlock(x,y2,z2) == 0)
                                    {
                                        GetBlock(x,y2,z2) = 9;
                                    }
                                }
                            }

                            for(int yy1 = y; yy1 <= y+height+2; yy1++)
                            {
                               GetBlock(x,yy1,z) = 8;
                            }
                        }
                    }


                    /// birch tree growing
                    if(block == 123 && (GetBlock(x,y-1,z) == 1 || GetBlock(x,y-1,z) == 2))
                    {
                        somethingWasUpdated = true;
                        if((rand() % 22 == 0 && (GetBlock(x,y-1,z) == 1 || GetBlock(x,y-1,z) == 2)) || (rand() % 40 == 0 && GetBlock(x,y-1,z) == 29))
                        {
                            mainStatistics.treesGrowned += 1;
                            int height = 2 + rand() % 2;

                            for(int x2 = x - 2; x2 <= x + 2; x2 ++)
                            {
                                for(int z2 = z - 2; z2 <= z + 2; z2 ++)
                                {
                                    for(int y2 = y+height; y2 <= y+height+1; y2 ++)
                                    {
                                        if(GetBlock(x2,y2,z2) == 0)
                                        {
                                            GetBlock(x2,y2,z2) = 38;
                                        }
                                    }
                                }
                            }

                            for(int x2 = x - 1; x2 <= x + 1; x2 ++)
                            {
                                for(int z2 = z - 1; z2 <= z + 1; z2 ++)
                                {
                                    int y2 = y+height+2;
                                    if(rand() % 4 <= 1)
                                    {
                                        if(GetBlock(x2,y2,z2) == 0)
                                        {
                                            GetBlock(x2,y2,z2) = 38;
                                        }
                                    }
                                }
                            }

                            for(int x2 = x - 1; x2 <= x + 1; x2 ++)
                            {
                                for(int y2 = y+height+2; y2 <= y+height+3; y2 ++)
                                {
                                    if(GetBlock(x2,y2,z) == 0)
                                    {
                                        GetBlock(x2,y2,z) = 38;
                                    }
                                }
                            }

                            for(int z2 = z - 1; z2 <= z + 1; z2 ++)
                            {
                                for(int y2 = y+height+2; y2 <= y+height+3; y2 ++)
                                {
                                    GetBlock(x,y2,z2) = 38;
                                }
                            }

                            for(int yy1 = y; yy1 <= y+height+2; yy1++)
                            {
                               GetBlock(x,yy1,z) = 31;
                            }
                        }
                    }


                    /// spruce tree growing
                    if(block == 122 && (GetBlock(x,y-1,z) == 1 || GetBlock(x,y-1,z) == 2 || GetBlock(x,y-1,z) == 29))
                    {
                        somethingWasUpdated = true;
                        if((rand() % 22 < 2 && (GetBlock(x,y-1,z) == 1 || GetBlock(x,y-1,z) == 2)) || (rand() % 19 == 0  && GetBlock(x,y-1,z) == 29))
                        {
                            mainStatistics.treesGrowned += 1;

                            y = groundHeight(x,z);
                            int TrunkHeight = 6;
                            int type = 0;

                            //check if there is an water on there or another tree
                            bool flag = 1;

                            for (int y1 = y+1; y1 <= y + TrunkHeight ; y1++)
                            {
                                if(GetBlock(x, y1, z) != 0)
                                {
                                    flag = 0;
                                    break;
                                }
                            }

                            if(flag == 0)continue;

                            if(rand() % 7 == 1)
                            {
                                type = 1;
                                TrunkHeight = 6 + rand() % 3;
                            }
                            else
                            {
                                type = 0;
                            }

                            //Create the tree trunk
                            for (int y1 = y; y1 <= y + TrunkHeight ; y1++)
                            {
                                block_t& Block = GetBlock(x, y1, z);
                                Block = 30;
                            }

                            if(type == 0)
                            {
                                for(int xx = -1; xx <= 1; xx++)
                                {
                                    for(int zz = -2; zz <= 2; zz++)
                                    {
                                        int x1 = xx + x;
                                        int y1 = y + 1;
                                        int z1 = zz + z;

                                        block_t& Block = GetBlock(x1, y1, z1);
                                        if (Block == 0) Block = 37;
                                    }
                                }

                                for(int xx = -2; xx <= 2; xx++)
                                {
                                    for(int zz = -1; zz <= 1; zz++)
                                    {
                                        int x1 = xx + x;
                                        int y1 = y + 1;
                                        int z1 = zz + z;

                                        block_t& Block = GetBlock(x1, y1, z1);
                                        if (Block == 0) Block = 37;
                                    }
                                }

                                for(int xx = -1; xx <= 1; xx++)
                                {
                                    for(int zz = -2; zz <= 2; zz++)
                                    {
                                        int x1 = xx + x;
                                        int y1 = y + 3;
                                        int z1 = zz + z;

                                        block_t& Block = GetBlock(x1, y1, z1);
                                        if (Block == 0) Block = 37;
                                    }
                                }

                                for(int xx = -2; xx <= 2; xx++)
                                {
                                    for(int zz = -1; zz <= 1; zz++)
                                    {
                                        int x1 = xx + x;
                                        int y1 = y + 3;
                                        int z1 = zz + z;

                                        block_t& Block = GetBlock(x1, y1, z1);
                                        if (Block == 0) Block = 37;
                                    }
                                }

                                for(int xx = -1; xx <= 1; xx++)
                                {
                                        int x1 = xx + x;
                                        int y1 = y + 2;
                                        int z1 = z;

                                        block_t& Block = GetBlock(x1, y1, z1);
                                        if (Block == 0) Block = 37;
                                }


                                for(int zz = -1; zz <= 1; zz++)
                                {
                                    int x1 = x;
                                    int y1 = y + 2;
                                    int z1 = zz + z;

                                    block_t& Block = GetBlock(x1, y1, z1);
                                    if (Block == 0) Block = 37;
                                }

                                for(int xx = -1; xx <= 1; xx++)
                                {
                                        int x1 = xx + x;
                                        int y1 = y + 4;
                                        int z1 = z;

                                        block_t& Block = GetBlock(x1, y1, z1);
                                        if (Block == 0) Block = 37;
                                }


                                for(int zz = -1; zz <= 1; zz++)
                                {
                                    int x1 = x;
                                    int y1 = y + 4;
                                    int z1 = zz + z;

                                    block_t& Block = GetBlock(x1, y1, z1);
                                    if (Block == 0) Block = 37;
                                }

                                for(int xx = -1; xx <= 1; xx++)
                                {
                                    int x1 = xx + x;
                                    int y1 = y + 6;
                                    int z1 = z;

                                    block_t& Block = GetBlock(x1, y1, z1);
                                    if (Block == 0) Block = 37;
                                }

                                for(int zz = -1; zz <= 1; zz++)
                                {
                                    int x1 = x;
                                    int y1 = y + 6;
                                    int z1 = zz + z;

                                    block_t& Block = GetBlock(x1, y1, z1);
                                    if (Block == 0) Block = 37;
                                }

                                block_t& Block = GetBlock(x, y+7, z);
                                if (Block == 0) Block = 37;
                            }
                            else // type == 1
                            {
                                block_t& Block = GetBlock(x, y+TrunkHeight+1, z);
                                if (Block == 0) Block = 37;

                                for(int xx = -1; xx <= 1; xx++)
                                {
                                        int x1 = xx + x;
                                        int y1 = y + TrunkHeight;
                                        int z1 = z;

                                        block_t& Block = GetBlock(x1, y1, z1);
                                        if (Block == 0) Block = 37;
                                }

                                for(int zz = -1; zz <= 1; zz++)
                                {
                                    int x1 = x;
                                    int y1 = y + TrunkHeight;
                                    int z1 = zz + z;

                                    block_t& Block = GetBlock(x1, y1, z1);
                                    if (Block == 0) Block = 37;
                                }

                                for(int xx = -1; xx <= 1; xx++)
                                {
                                        int x1 = xx + x;
                                        int y1 = y + TrunkHeight-1;
                                        int z1 = z;

                                        block_t& Block = GetBlock(x1, y1, z1);
                                        if (Block == 0) Block = 37;
                                }

                                for(int zz = -1; zz <= 1; zz++)
                                {
                                    int x1 = x;
                                    int y1 = y + TrunkHeight-1;
                                    int z1 = zz + z;

                                    block_t& Block = GetBlock(x1, y1, z1);
                                    if (Block == 0) Block = 37;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if(!somethingWasUpdated)
    {
        updatingChunks.erase(updatingChunks.begin()+vectorId);
        MeshChunk->periodicallyUpadted = false;
    }
}

void CraftWorld::UpdateFlood(Vector3 pos) // not used
{
}

void CraftWorld::AddToFloodPos(Vector3 pos) // not used
{
}


void CraftWorld::AddChunkToFloodUpdate(int id)
{
    if(id > 0 && id < mChunks.size())
    {
        bool push = true;
        if(floodChunks.empty() == false)
        {
            for(std::vector<int>::size_type i = 0; i < floodChunks.size(); i++)
            {
                if(floodChunks[i] == id)
                {
                    push = false;
                    break;
                }
            }
        }

        if(push)
        {
            floodChunks.push_back(id);
        }
    }
}

void CraftWorld::SetAllChunksToUpdate()
{
    for(unsigned int i = 0; i < mChunks.size(); i++)
    {
        if(!MeshIdValid(i))
            continue;
        mChunks[i]->needUpdate = true;
        mTransparentChunks[i]->needUpdate = true;
    }
}

int CraftWorld::getChunkId(Vector3 pos)
{
    if(CHUNK_SIZE <= 0)
        return -1;
    int x = (int)pos.x;
    int y = (int)pos.y;
    int z = (int)pos.z;
    if(y < 0) y = 0;
    if(y >= WORLD_HEIGHT) y = WORLD_HEIGHT - 1;
    int cx = ChunkFloorDiv(x, CHUNK_SIZE);
    int cy = y / CHUNK_SIZE;
    int cz = ChunkFloorDiv(z, CHUNK_SIZE);
    ChunkColumn* col = FindColumn(cx, cz);
    if(!col)
        return -1;
    if(!MeshIdValid(col->meshId[cy]))
        return -1;
    return col->meshId[cy];
}

bool CraftWorld::ChunksEqual(Vector3 pos1, Vector3 pos2)
{
    return (getChunkId(pos1) == getChunkId(pos2));
}

void CraftWorld::drawCubes(int i, float light)
{
    if(i < 0 || i >= (int)blockTypes.size() || blockTypes[i].verticesLeftRight == NULL)
        return;

    DrawPlaceTexture(true);
    sceGuEnable(GU_BLEND);

    sceGuColor(GU_COLOR(light*0.51,light*0.51,light*0.51,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, blockTypes[i].pointsLeftRight, 0, blockTypes[i].verticesLeftRight);

    sceGuColor(GU_COLOR(light*0.71,light*0.71,light*0.71,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, blockTypes[i].pointsBackFront, 0, blockTypes[i].verticesBackFront);

    sceGuColor(GU_COLOR(light,light,light,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, blockTypes[i].pointsBottomTop, 0, blockTypes[i].verticesBottomTop);

    sceGuDisable(GU_BLEND);
    DrawPlaceTexture(false);
}


void CraftWorld::drawHudCubes(int i)
{
    sceGuDisable(GU_DEPTH_TEST);
    DrawSetDepthMask(true);

    MatrixScale(Vector3(18,18,18));
    MatrixAngle(Vector3(150,44.6f,0));

    DrawSetCulling(true);
    drawCubes(i,1.0f);
    DrawSetCulling(false);

    DrawSetDepthMask(false);
    sceGuEnable(GU_DEPTH_TEST);
}


void CraftWorld::drawWhiteCube(int i, float light)
{
    DrawPlaceTexture(false);

    sceGuColor(GU_COLOR(light,light,light,1));

    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, blockTypes[i].pointsLeftRight, 0, blockTypes[i].verticesLeftRight);
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, blockTypes[i].pointsBackFront, 0, blockTypes[i].verticesBackFront);
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, blockTypes[i].pointsBottomTop, 0, blockTypes[i].verticesBottomTop);

    DrawPlaceTexture(true);
}

void CraftWorld::drawHand()
{
    DrawPlaceTexture(true);
    DrawSetBlend(true);

    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 36, 0, handVertices);

    DrawSetBlend(false);
    DrawPlaceTexture(false);
}

void CraftWorld::drawClouds()
{
    DrawPlaceTexture(true);
    DrawSetBlend(true);

    DrawSetCulling(true);
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 600, 0, blockTypes[141].verticesBottomTop);
    DrawSetCulling(false);
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 600, 0, blockTypes[141].verticesBottomTop);

    DrawSetBlend(false);
    DrawPlaceTexture(false);
}

void CraftWorld::drawItems(int i)
{
    DrawPlaceTexture(true);
    sceGuDisable(GU_DEPTH_TEST);
    DrawSetDepthMask(true);
    DrawSetBlend(true);

    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, itemTypes[i-250].points, 0, itemTypes[i-250].vertices);

    DrawSetBlend(false);
    DrawSetDepthMask(false);
    sceGuEnable(GU_DEPTH_TEST);
    DrawPlaceTexture(false);
}

void CraftWorld::drawHudItems(int i)
{
    MatrixAngle(Vector3(0,0,0));
    MatrixScale(Vector3(32,32,32));

    DrawSetCulling(true);
    drawItems(i);
    DrawSetCulling(false);
}

void CraftWorld::drawDropItems(int i)
{
    DrawPlaceTexture(true);
    DrawSetBlend(true);

    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, itemTypes[i-250].points, 0, itemTypes[i-250].dropVertices);

    DrawSetBlend(false);
    DrawPlaceTexture(false);
}

void CraftWorld::drawHandItems(int i, float light)
{
    DrawPlaceTexture(true);
    DrawSetBlend(true);

    sceGuColor(GU_COLOR(light*0.75,light*0.75,light*0.75,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, itemTypes[i-250].addPoints, 0, itemTypes[i-250].addVertices);

    DrawSetCulling(true);
    sceGuColor(GU_COLOR(light*0.75,light*0.75,light*0.75,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, itemTypes[i-250].add2Points, 0, itemTypes[i-250].add2Vertices);
    sceGuColor(GU_COLOR(light,light,light,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, itemTypes[i-250].points, 0, itemTypes[i-250].vertices);
    DrawSetCulling(false);

    DrawSetBlend(false);
    DrawPlaceTexture(false);
}

void CraftWorld::drawFull3DItem(int i, float light)
{
    DrawPlaceTexture(true);
    DrawSetBlend(true);

    DrawSetCulling(false);

    sceGuColor(GU_COLOR(light*0.75,light*0.75,light*0.75,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, itemTypes[i-250].add2Points, 0, itemTypes[i-250].add2Vertices);

    DrawSetCulling(true);
    sceGuColor(GU_COLOR(light*0.75,light*0.75,light*0.75,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, itemTypes[i-250].addPoints, 0, itemTypes[i-250].addVertices);

    sceGuColor(GU_COLOR(light,light,light,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, itemTypes[i-250].points, 0, itemTypes[i-250].vertices);
    DrawSetCulling(false);
    sceGuColor(GU_COLOR(light,light,light,1));
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, itemTypes[i-250].points, 0, itemTypes[i-250].vertices);

    // Leave the front face on CCW, which is the winding the GU init sets once at startup and which
    // every other draw helper here ends on (drawHandItems, drawHudItems, drawClouds). `DrawSetCulling`
    // is not a cull toggle -- it is `sceGuFrontFace`, so it survives the call and the frame: it was
    // ending on `true` (CW), and the held item is drawn in the middle of the world pass, so everything
    // after it in the frame came out with the wrong facing. Holding an item in third person inverted
    // every mob mesh drawn later in the frame and culled every triangle strip -- which is the arrows
    // in flight, and the hotbar. One `false` here, not a per-caller patch.
    DrawSetCulling(false);

    DrawSetBlend(false);
    DrawPlaceTexture(false);
}

void CraftWorld::drawBowInHand(int stage, float light)
{
    // Same mesh family, different atlas cell -- see BowHeldMeshId. The three draw frames are ordinary
    // item types, so the whole of drawFull3DItem applies to them unchanged.
    drawFull3DItem(BowHeldMeshId(stage), light);
}

void CraftWorld::drawArmor(int i, float light)
{
    if(i >= 328 && i <= 331) // leather
    {
        TextureManager::Instance()->SetTextureModeulate(textureLeatherArmor1Id);
        if(i == 330)
        {
            TextureManager::Instance()->SetTextureModeulate(textureLeatherArmor2Id);
        }
    }

    if(i >= 332 && i <= 335) // chain
    {
        TextureManager::Instance()->SetTextureModeulate(textureChainArmor1Id);
        if(i == 334)
        {
            TextureManager::Instance()->SetTextureModeulate(textureChainArmor2Id);
        }
    }

    if(i >= 336 && i <= 339) // iron
    {
        TextureManager::Instance()->SetTextureModeulate(textureIronArmor1Id);
        if(i == 338)
        {
            TextureManager::Instance()->SetTextureModeulate(textureIronArmor2Id);
        }
    }

    if(i >= 340 && i <= 343) // diamond
    {
        TextureManager::Instance()->SetTextureModeulate(textureDiamondArmor1Id);
        if(i == 342)
        {
            TextureManager::Instance()->SetTextureModeulate(textureDiamondArmor2Id);
        }
    }

    if(i >= 344 && i <= 347) // diamond
    {
        TextureManager::Instance()->SetTextureModeulate(textureGoldenArmor1Id);
        if(i == 346)
        {
            TextureManager::Instance()->SetTextureModeulate(textureGoldenArmor2Id);
        }
    }

    float points = 0;

    DrawSetBlend(true);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_ALPHA_TEST);

    sceGuColor(GU_COLOR(light,light,light,1));
    switch(itemTypes[i-250].itemType)
    {
    case 'H':
        points = 30;
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, points, 0, helmetVertices);
    break;
    case 'C':
        points = 24;
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, points, 0, chestplateVertices);
    break;
    case 'L':
        points = 24;
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, points, 0, leggingVertices);
    break;
    case 'B':
        points = 30;
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, points, 0, bootVertices);
    break;
    }

    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    DrawSetBlend(false);
}

void CraftWorld::drawShoulder(int chestplateId, float light)
{
    if(chestplateId >= 328 && chestplateId <= 331) // leather
    {
        TextureManager::Instance()->SetTextureModeulate(textureLeatherArmor1Id);
    }

    if(chestplateId >= 332 && chestplateId <= 335) // chain
    {
        TextureManager::Instance()->SetTextureModeulate(textureChainArmor1Id);
    }

    if(chestplateId >= 336 && chestplateId <= 339) // iron
    {
        TextureManager::Instance()->SetTextureModeulate(textureIronArmor1Id);
    }

    if(chestplateId >= 340 && chestplateId <= 343) // diamond
    {
        TextureManager::Instance()->SetTextureModeulate(textureDiamondArmor1Id);
    }

    if(chestplateId >= 344 && chestplateId <= 347) // diamond
    {
        TextureManager::Instance()->SetTextureModeulate(textureGoldenArmor1Id);
    }

    float points = 0;

    DrawSetBlend(true);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_ALPHA_TEST);

    sceGuColor(GU_COLOR(light,light,light,1));
    switch(itemTypes[chestplateId-250].itemType)
    {
    case 'C':
        points = 30;
        DrawSetCulling(false);
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, points, 0, shoulderVertices);
        DrawSetCulling(true);
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, points, 0, shoulderVertices);
    break;
    }

    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    DrawSetBlend(false);
    DrawSetCulling(true);
}

void CraftWorld::drawBelly(int leggingsId, float light)
{
    if(leggingsId >= 328 && leggingsId <= 331) // leather
    {
        TextureManager::Instance()->SetTextureModeulate(textureLeatherArmor2Id);
    }

    if(leggingsId >= 332 && leggingsId <= 335) // chain
    {
        TextureManager::Instance()->SetTextureModeulate(textureChainArmor2Id);
    }

    if(leggingsId >= 336 && leggingsId <= 339) // iron
    {
        TextureManager::Instance()->SetTextureModeulate(textureIronArmor2Id);
    }

    if(leggingsId >= 340 && leggingsId <= 343) // diamond
    {
        TextureManager::Instance()->SetTextureModeulate(textureDiamondArmor2Id);
    }

    if(leggingsId >= 344 && leggingsId <= 347) // diamond
    {
        TextureManager::Instance()->SetTextureModeulate(textureGoldenArmor2Id);
    }

    float points = 0;

    DrawSetBlend(true);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_ALPHA_TEST);

    sceGuColor(GU_COLOR(light,light,light,1));
    switch(itemTypes[leggingsId-250].itemType)
    {
    case 'L':
        points = 24;
        DrawSetCulling(true);
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, points, 0, bellyVertices);
    break;
    }

    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    DrawSetBlend(false);
    DrawSetCulling(true);
}

void CraftWorld::AddNearestChunk(int id)
{
    bool alreadyHadThisChunk = false;
    for(unsigned int i = 0; i < mNearestChunksIds.size(); i++)
    {
        if(mNearestChunksIds[i] == id)
        {
            alreadyHadThisChunk = true;
            break;
        }
    }
    if(!alreadyHadThisChunk)
    {
        mNearestChunksIds.push_back(id);
    }
}

double CraftWorld::TestWriteChunk(float dt)
{
    if(dt < 0.03f)
    {

        char *testChunkData;
        testChunkData = new  char[14 * 98 * 14];
        for(int i = 0; i < 14*98*14; i++)
        {
            testChunkData[i] = rand() % 122;
        }

        struct timeval  tv;
        gettimeofday(&tv, NULL);
        double time_begin = ((double)tv.tv_sec) * 1000 + ((double)tv.tv_usec) / 1000;

        SceUID fd = sceIoOpen("ms0:/test.txt",  PSP_O_WRONLY|PSP_O_CREAT, 0777);
        sceIoChangeAsyncPriority(fd, 0x8);
        sceIoWriteAsync(fd, testChunkData, sizeof(char)*14*98*14);
        SceInt64 res;
        sceIoWaitAsync(fd, &res);
        sceIoClose(fd);

        gettimeofday(&tv, NULL);
        double time_end = ((double)tv.tv_sec) * 1000 + ((double)tv.tv_usec) / 1000 ;
        double total_time_ms = time_end - time_begin;
        delete[] testChunkData;
        return total_time_ms;
    }
    return 0;
}

void CraftWorld::FetchNearestChunks()
{
    mNearestChunksIds.clear();
    for(unsigned int i = 0; i < loadedColumns.size(); i++)
    {
        ChunkColumn* col = loadedColumns[i];
        for(int cy = 0; cy < 16; cy++)
        {
            int id = col->meshId[cy];
            if(!MeshIdValid(id))
                continue;
            if(MeshInPlayerZone(mChunks[id]))
                mNearestChunksIds.push_back(id);
        }
    }
}

void CraftWorld::drawWorld(Frustum &camFrustum, bool camUpdate)
{
    //DrawSetCulling(true);
    drawnTriangles = 0;

    // Remember what the camera can see. This is the only place the frustum is available to the world,
    // and it is what SectionInView consults when the mesher decides what to build -- so the mesher
    // works on the frame's real view from the previous frame's draw rather than on a guess.
    viewFrustum = camFrustum;
    viewFrustumValid = true;

    // Re-arm the mesher when the player turns. Without this, a section skipped as off-camera would only
    // be reconsidered when something else happened to set meshPending -- the player could turn to face
    // a wall of unmeshed terrain and see holes that nothing was queued to fill.
    //
    // The threshold is 0.999 (about 2.5 degrees), and it is derived from the margin in SectionInView
    // rather than picked. A section is built when it is within one chunk (16 blocks) of the frustum, so
    // turning to face a section d blocks away needs only about atan(16/d) of turn before it is inside
    // that margin and should have been built. At 200 blocks that is 4.6 degrees -- well UNDER the 20
    // degrees (0.94) this used to require, so a small turn toward distant terrain would put it on screen
    // with nothing queued to build it: the same hole by a different route. 2.5 degrees covers the whole
    // view distance (the crossover is at ~366 blocks, beyond what is drawn at all), and because
    // TryMeshOneSection is rate-limited to one section per 0.08 s this costs a little more mesh
    // throughput and no extra work per frame.
    {
        const Vector3 dir = ViewDirectionFromFrustum();
        const float dot = dir.x * viewArmedDir.x + dir.y * viewArmedDir.y + dir.z * viewArmedDir.z;
        if(!viewArmedValid || dot < 0.999f)
        {
            viewArmedValid = true;
            viewArmedDir = dir;
            meshPending = true;
        }
    }
    //normal not transparent chunks
    sceGuColor(0xFFFFFFFF);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_ALPHA_TEST);

    int lowestDistanse = 0x7fffffff;
    int toUpdateId = -1;

    if(mNearestChunksIds.empty() == false)
    {
        for(unsigned int i2 = 0; i2 != mNearestChunksIds.size(); i2++)
        {
            int i = mNearestChunksIds[i2];
            if(!MeshIdValid(i))
                continue;

            mTransparentChunks[i]->inFrustum = false;

            if(MeshInPlayerZone(mChunks[i]))
            {
                if(mChunks[i]->trienglesCount > 0 || mTransparentChunks[i]->trienglesCount > 0 || !mChunks[i]->created)
                {
                    // The frustum test is the one place the section's own box is not quite enough: a
                    // piston's *base* draws its arm into the cell in front of it while a stroke runs (see
                    // StartPistonStroke), so a piston standing on a section boundary would have that arm
                    // culled with its own section while the cell the arm is in was still on screen. One
                    // block of margin here and only here; the box itself stays exact, because it is what
                    // the section build and wake tests measure a section's extent with, and a margin in
                    // any of those would move cells between sections. Note this margin is for geometry
                    // that *reaches* past its cell -- it is not a fix for a neighbour that was never
                    // remeshed, which is what SectionBorderOffset is about.
                    const BoundingBox& fb = mChunks[i]->bBox;
                    if(camFrustum.BoxInFrustum(fb.min.x - 1.0f, fb.min.y - 1.0f, fb.min.z - 1.0f,
                                               fb.max.x + 1.0f, fb.max.y + 1.0f, fb.max.z + 1.0f) == Frustum::Intersects)
                    {
                        mTransparentChunks[i]->inFrustum = true;

                        if((!mChunks[i]->created || mChunks[i]->needUpdate) && chunksCreatedInFrameCount == 0 && meshBuildsThisFrame == 0 && (physicsBoxesThisFrame == 0 || !mChunks[i]->created))
                        {
                            int distance = SectionMeshPriority(mChunks[i]->chunkStartX, mChunks[i]->chunkStartY, mChunks[i]->chunkStartZ);
                            if(distance < lowestDistanse)
                            {
                                lowestDistanse = distance;
                                toUpdateId = i;
                            }
                        }
                    }
                }
            }
            else
            {
                // swap-remove: draw order of sections does not matter (depth tested)
                mNearestChunksIds[i2] = mNearestChunksIds.back();
                mNearestChunksIds.pop_back();
                i2 --;

                if(mNearestChunksIds.empty() == true)
                {
                    break;
                }
            }
        }
    }

    if(toUpdateId != -1)
    {
        chunksCreatedInFrameCount++;
        RebuildFullMeshChunk(toUpdateId);
    }

    if(mNearestChunksIds.empty() == false)
    {
        for(unsigned int i2 = 0; i2 != mNearestChunksIds.size(); i2++)
        {
            int i = mNearestChunksIds[i2];
            if(mTransparentChunks[i]->inFrustum == true && mChunks[i]->trienglesCount > 0)
            {
                mChunks[i]->drawChunk();
                drawnTriangles += mChunks[i]->trienglesCount;
            }
        }
    }

    //transparent chunks
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);

    if(mNearestChunksIds.empty() == false)
    {
        unsigned int length = mNearestChunksIds.size();
        for(unsigned int i2 = 0; i2 != length; i2++)
        {
            int i = mNearestChunksIds[i2];

            if(mTransparentChunks[i]->trienglesCount > 0 || !mTransparentChunks[i]->created)
            {
                if(mTransparentChunks[i]->inFrustum == true)
                {
                    mTransparentChunks[i]->drawChunk();
                    drawnTriangles += mTransparentChunks[i]->trienglesCount;
                }
            }
        }
    }

    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    // Cleared for the next frame, unless a section rebuild is still walking -- that job has claimed the
    // frame's rebuild budget and keeps it across frames until it is done.
    meshBuildsThisFrame = (meshJobKind != kMeshJobNone) ? 1 : 0;
    physicsBoxesThisFrame = 0;
}

void CraftWorld::UpdatePlayerZoneBB(Vector3 playerPosition)
{
    playerPos = playerPosition;
    int hv = EffectiveViewDistance();
    int vv = mainOptions.verticalViewDistance;
    if(hv < 1) hv = 1;
    if(vv < 1) vv = 1;
    int cs = CHUNK_SIZE > 0 ? CHUNK_SIZE : 16;
    playerZoneSize = Vector3((float)(cs * hv), (float)(cs * vv), (float)(cs * hv));
    playerZoneBB = BoundingBox(Vector3(playerPosition.x - playerZoneSize.x,playerPosition.y - playerZoneSize.y,playerPosition.z - playerZoneSize.z),Vector3(playerPosition.x + playerZoneSize.x,playerPosition.y + playerZoneSize.y,playerPosition.z + playerZoneSize.z));
    if(CHUNK_SIZE > 0)
    {
        int pcx = ChunkFloorDiv((int)playerPosition.x, CHUNK_SIZE);
        int pcz = ChunkFloorDiv((int)playerPosition.z, CHUNK_SIZE);
        int pcy = (int)playerPosition.y / CHUNK_SIZE;
        // The zone box is chunk aligned on every axis, so sections only enter it when the
        // player crosses a section boundary; vertical crossings (digging down, falling,
        // pillaring) must refetch too or sections below/above never get drawn again.
        if(pcx != meshWatchCx || pcz != meshWatchCz || pcy != meshWatchCy)
        {
            meshWatchCx = pcx;
            meshWatchCz = pcz;
            meshWatchCy = pcy;
            meshPending = true;
            FetchNearestChunks();
        }
        float mdx = playerPosition.x - meshPendPosX;
        float mdy = playerPosition.y - meshPendPosY;
        float mdz = playerPosition.z - meshPendPosZ;
        if(mdx * mdx + mdz * mdz >= 16.0f || mdy * mdy >= 4.0f)
        {
            meshPendPosX = playerPosition.x;
            meshPendPosY = playerPosition.y;
            meshPendPosZ = playerPosition.z;
            meshPending = true;
        }
    }
}

Vector3 CraftWorld::GetPlayerPos()
{
    return playerPos;
}

// Java's EntityPlayer.addExhaustion + the FoodStats.onUpdate conversion, in one place. The 4.0 is
// Java's threshold and is not a tunable: it is what sets how many metres of walking (400) or of
// sprinting (40) a hunger point is worth, so changing it would change the whole economy rather than
// the feel. A while loop because a single call can in principle overshoot by more than one unit.
void CraftWorld::AddExhaustion(float amount)
{
    if(amount <= 0.0f)
        return;

    foodExhaustion += amount;
    while(foodExhaustion > 4.0f)
    {
        foodExhaustion -= 4.0f;
        if(HG > 0.0f)
        {
            HG -= 1.0f;
        }
        if(HG < 0.0f)
        {
            HG = 0.0f;
        }
    }
}

void CraftWorld::UpdateClockTexture()
{
    // 102.5f = noon
    // 0.715277 21.458333
    float realSkyTime = skyTime-100.0f;

    if(realSkyTime >= 102.5f && realSkyTime < 360.0f)
    {
        clockFrame = (int)(((realSkyTime-102.5f)/257.5f)*(257.5f/360.0f*30));
    }
    if(realSkyTime >= 0.0f && realSkyTime < 102.5f)
    {
        clockFrame = (int)((257.5f/360.0f*30)+((realSkyTime)/102.5f)*(102.5f/360.0f*30));
    }

    if(lastClockFrame != clockFrame)
    {
        lastClockFrame = clockFrame;
        int frameSize = TextureManager::Instance()->getWidth(textureItemsId)/16;

        TextureManager* tm = TextureManager::Instance();
        for(int i = 0; i < frameSize; i++)
        {
            for(int j = 0; j < frameSize; j++)
            {
                tm->setPixelRaw(frameSize*6+i, frameSize*4+j,
                                tm->getPixelRaw(i, lastClockFrame*frameSize+j, textureClockId),
                                textureItemsId);
            }
        }
        tm->FlushTexelRegion(frameSize*6, frameSize*4, frameSize*7-1, frameSize*5-1, textureItemsId);
    }
}

void CraftWorld::UpdateCompassTexture(float cameraAngle)
{
    float angle = ATAN2(playerPos.z-playerSpawnPointPosition.z,playerPos.x-playerSpawnPointPosition.x)-(cameraAngle-PI);
    if(angle < -PI)
    {
        angle = angle+2*PI;
    }
    if(angle > PI)
    {
        angle = angle-2*PI;
    }
    //atan2f(playerPos.x-playerSpawnPointPosition.x,playerPos.z-playerSpawnPointPosition.z) +

    compassFrame = roundf((angle+PI)/PI*8);
    if(compassFrame >= 16)
    {
        compassFrame = 0;
    }
    // The texels follow in ApplyTextureAnimation, once the GE is done with the last frame.
}

void CraftWorld::ApplyTextureAnimation()
{
    MC_PERF_PHASE_BEGIN(kPhAnim);
    UpdateClockTexture();
    UpdateWorldAnimation(animPendingDt);
    animPendingDt = 0.0f;

    if(lastCompassFrame != (int)compassFrame)
    {
        lastCompassFrame = (int)compassFrame;

        int frameSize = TextureManager::Instance()->getWidth(textureItemsId)/16;

        TextureManager* tm = TextureManager::Instance();
        for(int i = 0; i < frameSize; i++)
        {
            for(int j = 0; j < frameSize; j++)
            {
                tm->setPixelRaw(frameSize*6+i, frameSize*3+j,
                                tm->getPixelRaw(i, lastCompassFrame*frameSize+j, textureCompassId),
                                textureItemsId);
            }
        }
        tm->FlushTexelRegion(frameSize*6, frameSize*3, frameSize*7-1, frameSize*4-1, textureItemsId);
    }
    MC_PERF_PHASE_END(kPhAnim);
}

void CraftWorld::UpdateWorldAnimation(float dt)
{
    if(mainOptions.worldBlockAnimation == true)
    {
        animationTimer += dt;

        if(animationTimer >= animationTimerEnd) // every 0.075 secs
        {
            int waterFrameSize = TextureManager::Instance()->getWidth(textureWaterAnimationId);
            if(waterFrameSize > 16)
                return;

            animationTimer = 0.0f; // zeroize timer
            animationWaterFrame += 1; // go to the next water frame
            if(animationWaterFrame >= TextureManager::Instance()->getHeight(textureWaterAnimationId)/TextureManager::Instance()->getWidth(textureWaterAnimationId)) // if all frames passed
            {
                animationWaterFrame = 0; // zeroize frames
            }

            if(animationLavaFrame >= TextureManager::Instance()->getHeight(textureLavaAnimationId)/TextureManager::Instance()->getWidth(textureLavaAnimationId)-1)
            {
                animationLavaStep = false;
            }
            if(animationLavaFrame == 0)
            {
                animationLavaStep = true;
            }

            if(animationLavaStep == true)
            {
                animationLavaFrame += 1;
            }
            else
            {
                animationLavaFrame -= 1;
            }

			animationFireFrame += 1; // go to the next water frame
			if (animationFireFrame >= TextureManager::Instance()->getHeight(textureFireAnimationId) / TextureManager::Instance()->getWidth(textureFireAnimationId)) // if all frames passed
			{
				animationFireFrame = 0; // zeroize frames
			}

			// The portal loop, 0..31. Guarded on a usable width because unlike water and lava this
			// strip is a newer file: a texture pack that predates it has no portal.png, and dividing
			// by a zero width would take the whole animation tick down with it.
			const int portalTexW = TextureManager::Instance()->getWidth(texturePortalAnimationId);
			if(portalTexW > 0)
			{
				animationPortalFrame += 1;
				if(animationPortalFrame >= TextureManager::Instance()->getHeight(texturePortalAnimationId) / portalTexW)
				{
					animationPortalFrame = 0;
				}
			}
			else
			{
				animationPortalFrame = 0;
			}

            // Copy the current water/lava/fire frame into the terrain atlas (tiles 14,15 /
            // 15,15 / 13,15) as whole 32-bit texels. Water and lava force their alpha.
            TextureManager* tm = TextureManager::Instance();
            const int fs = waterFrameSize;
            // When mip texturing is off, the half-size atlas is never bound, so copying into it and
            // flushing its dcache range is pure waste -- on the one path that runs ~13 times a second.
            const bool updateMips = (mainOptions.mipMapTexturing == true);
            const int waterRow = animationWaterFrame * fs;
            const int lavaRow = animationLavaFrame * fs;
            const int fireRow = animationFireFrame * fs;
            for(int i = 0; i < fs; i++)
            {
                for(int j = 0; j < fs; j++)
                {
                    unsigned int water = (tm->getPixelRaw(i, waterRow + j, textureWaterAnimationId) & 0x00FFFFFFu) | (184u << 24);
                    unsigned int lava = (tm->getPixelRaw(i, lavaRow + j, textureLavaAnimationId) & 0x00FFFFFFu) | (255u << 24);
                    unsigned int fire = tm->getPixelRaw(i, fireRow + j, textureFireAnimationId);

                    tm->setPixelRaw(fs*14+i, fs*15+j, water, textureTerrainId);
                    tm->setPixelRaw(fs*15+i, fs*15+j, lava, textureTerrainId);
                    tm->setPixelRaw(fs*13+i, fs*15+j, fire, textureTerrainId);

                    // half-size mip atlas: same tiles at 8px
                    if(updateMips && i < 8 && j < 8)
                    {
                        tm->setPixelRaw(8*14+i, 8*15+j, water, textureTerrainMipsId);
                        tm->setPixelRaw(8*15+i, 8*15+j, lava, textureTerrainMipsId);
                        tm->setPixelRaw(8*13+i, 8*15+j, fire, textureTerrainMipsId);
                    }
                }
            }
			tm->FlushTexelRegion(fs*13, fs*15, fs*16-1, fs*16-1, textureTerrainId);
            if(updateMips)
                tm->FlushTexelRegion(8*13, 8*15, 8*16-1, 8*16-1, textureTerrainMipsId);

            // The portal, at tile (15,8) -- the one tile every shipped pack leaves empty, which is why
            // it was chosen. Its alpha is kept as authored: the vanilla swirl is translucent, and
            // forcing it opaque would turn the portal into a solid purple wall.
            if(portalTexW > 0)
            {
                const int portalRow = animationPortalFrame * fs;
                for(int i = 0; i < fs; i++)
                {
                    for(int j = 0; j < fs; j++)
                    {
                        unsigned int portal = tm->getPixelRaw(i, portalRow + j, texturePortalAnimationId);
                        tm->setPixelRaw(fs*15+i, fs*8+j, portal, textureTerrainId);
                        if(updateMips && i < 8 && j < 8)
                            tm->setPixelRaw(8*15+i, 8*8+j, portal, textureTerrainMipsId);
                    }
                }
                tm->FlushTexelRegion(fs*15, fs*8, fs*16-1, fs*9-1, textureTerrainId);
                if(updateMips)
                    tm->FlushTexelRegion(8*15, 8*8, 8*16-1, 8*9-1, textureTerrainMipsId);
            }
        }
    }
}

void CraftWorld::SpawnMobs(float dt)
{
    MaintainPassiveHerd(dt);
    spawnZombiesTimer += dt;
    if(spawnZombiesTimer > 15.0f)
    {
        bool wasSpawned = false;
        if((worldDayTime > 21 || worldDayTime < 6) && mainOptions.difficult != 0 && playerPos.y > 55.0f) // spawn zombs
        {
            if(rand() % 50 <= 26+(24*mZombies.empty()) && mZombies.size() < 4 && CanSpawnHostile())
            {
                float spawnX = 0;
                float spawnY = 0;
                float spawnZ = 0;

                rand() % 2 == 0 ? spawnX = playerPos.x-8-rand() % 18 : spawnX = playerPos.x+8+rand() % 18;
                rand() % 2 == 0 ? spawnZ = playerPos.z-8-rand() % 18 : spawnZ = playerPos.z+8+rand() % 18;

                spawnY = groundHeightWater(spawnX,spawnZ);

                if(BlockFinalLight(spawnX,spawnY,spawnZ) < 0.25f) // spawn only at black areas
                {
                    if(spawnY != -1 && spawnY < WORLD_HEIGHT && spawnX < worldLimit && spawnZ < worldLimit && spawnY > 0 && spawnX > 0 && spawnZ > 0)
                    {
                        SpawnZombie(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                        SpawnZombie(spawnX+2.5f,groundHeightWater(spawnX+2.0f,spawnZ+2.0f)+1.8f,spawnZ+2.5f);
                        wasSpawned = true;
                    }
                }
            }
            spawnZombiesTimer = -10.0f+rand()%11;
        }

        if(mainOptions.difficult != 0 && playerPos.y <= 55.0f) // spawn zombs underground
        {
            if(rand() % 50 <= 30+(24*mZombies.empty()) && mZombies.size() < 4 && CanSpawnHostile())
            {
                float spawnX = 0;
                float spawnY = 0;
                float spawnZ = 0;

                int side = rand() % 4;
                if(side == 0)
                {
                    spawnZ = playerPos.z;
                    for(spawnX = playerPos.x-26; spawnX <= playerPos.x-16; spawnX++)
                    {
                        spawnZ = playerPos.z-1+rand()%3;
                        spawnY = groundHeightFromBottom(spawnX,playerPos.y-5,spawnZ);
                        if(spawnY != -1)
                        {
                            if(GetBlock(spawnX,spawnY+1,spawnZ) == 0)
                            {
                                if(BlockFinalLight(spawnX,spawnY,spawnZ) <= 0.15f)
                                {
                                    if(spawnY != -1 && spawnY < WORLD_HEIGHT && spawnX < worldLimit && spawnZ < worldLimit && spawnY > 0 && spawnX > 0 && spawnZ > 0)
                                    {
                                        SpawnZombie(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                                        if(rand() % 5 == 0)
                                        {
                                            SpawnZombie(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                                        }
                                        wasSpawned = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if(side == 1)
                {
                    spawnZ = playerPos.z;
                    for(spawnX = playerPos.x+16; spawnX <= playerPos.x+26; spawnX++)
                    {
                        spawnZ = playerPos.z-1+rand()%3;
                        spawnY = groundHeightFromBottom(spawnX,playerPos.y-5,spawnZ);
                        if(spawnY != -1)
                        {
                            if(GetBlock(spawnX,spawnY+1,spawnZ) == 0)
                            {
                                if(BlockFinalLight(spawnX,spawnY,spawnZ) <= 0.15f)
                                {
                                    if(spawnY != -1 && spawnY < WORLD_HEIGHT && spawnX < worldLimit && spawnZ < worldLimit && spawnY > 0 && spawnX > 0 && spawnZ > 0)
                                    {
                                        SpawnZombie(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                                        if(rand() % 5 == 0)
                                        {
                                            SpawnZombie(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                                        }
                                        wasSpawned = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if(side == 2)
                {
                    spawnX = playerPos.z;
                    for(spawnZ = playerPos.z+16; spawnZ <= playerPos.z+26; spawnZ++)
                    {
                        spawnX = playerPos.x-1+rand()%3;
                        spawnY = groundHeightFromBottom(spawnX,playerPos.y-5,spawnZ);
                        if(spawnY != -1)
                        {
                            if(GetBlock(spawnX,spawnY+1,spawnZ) == 0)
                            {
                                if(BlockFinalLight(spawnX,spawnY,spawnZ) <= 0.15f)
                                {
                                    if(spawnY != -1 && spawnY < WORLD_HEIGHT && spawnX < worldLimit && spawnZ < worldLimit && spawnY > 0 && spawnX > 0 && spawnZ > 0)
                                    {
                                        SpawnZombie(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                                        if(rand() % 5 == 0)
                                        {
                                            SpawnZombie(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                                        }
                                        wasSpawned = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if(side == 3)
                {
                    spawnX = playerPos.z;
                    for(spawnZ = playerPos.z-26; spawnZ <= playerPos.z-16; spawnZ++)
                    {
                        spawnX = playerPos.x-1+rand()%3;
                        spawnY = groundHeightFromBottom(spawnX,playerPos.y-5,spawnZ);
                        if(spawnY != -1)
                        {
                            if(GetBlock(spawnX,spawnY+1,spawnZ) == 0)
                            {
                                if(BlockFinalLight(spawnX,spawnY,spawnZ) <= 0.15f)
                                {
                                    if(spawnY != -1 && spawnY < WORLD_HEIGHT && spawnX < worldLimit && spawnZ < worldLimit && spawnY > 0 && spawnX > 0 && spawnZ > 0)
                                    {
                                        SpawnZombie(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                                        if(rand() % 5 == 0)
                                        {
                                            SpawnZombie(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                                        }
                                        wasSpawned = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if(wasSpawned)
            {
                spawnZombiesTimer = -45+rand()%15;
            }
            else
            {
                spawnZombiesTimer = -15.0f;
            }
        }
    }

    spawnCreeperTimer -= dt;
    if(spawnCreeperTimer < 0.0f)
    {
        if((worldDayTime > 21 || worldDayTime < 6) && mainOptions.difficult != 0) // spawn creepers
        {
            if(rand() % 50 <= 27-mCreepers.size()*1.5f && mCreepers.size() < 4 && CanSpawnHostile())
            {
                float spawnX = 0;
                float spawnY = 0;
                float spawnZ = 0;

                if(rand() % 4 != 0)
                {
                    rand() % 2 == 0 ? spawnX = playerPos.x-20-rand() % 14 : spawnX = playerPos.x+20+rand() % 14;
                    rand() % 2 == 0 ? spawnZ = playerPos.z-20-rand() % 14 : spawnZ = playerPos.z+20+rand() % 14;
                }
                else
                {
                    rand() % 2 == 0 ? spawnX = playerPos.x-8-rand() % 16 : spawnX = playerPos.x+8+rand() % 16;
                    rand() % 2 == 0 ? spawnZ = playerPos.z-8-rand() % 16 : spawnZ = playerPos.z+8+rand() % 16;
                }

                spawnY = groundHeightWater(spawnX,spawnZ);

                if(BlockFinalLight(spawnX,spawnY,spawnZ) < 0.25f) // spawn only at black areas
                {
                    if(spawnY != -1 && spawnY < WORLD_HEIGHT && spawnX < worldLimit && spawnZ < worldLimit && spawnY > 0 && spawnX > 0 && spawnZ > 0)
                    {
                        SpawnCreeper(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                        if(rand() % 4 == 0)
                        {
                            SpawnCreeper(spawnX+2.5f,groundHeightWater(spawnX+2.0f,spawnZ+2.0f)+1.8f,spawnZ+2.5f);
                        }
                    }
                }
            }
            if(rand() % 10 < 7 && mCreepers.size() < 6 && CanSpawnHostile())
            {
                int spawnX, spawnY, spawnZ;
                if(FindMobSpawnSpot(spawnX, spawnY, spawnZ, true, 12))
                    SpawnCreeper(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f,800);
            }
        }
        spawnCreeperTimer = 30+rand()%30;
    }

    // Spiders. Vanilla gates them on the same light level as the other hostiles, which is why they
    // are the mob you meet in a cave at noon as well as on the surface at night -- so unlike the
    // creeper block above, the second pass here is not wrapped in the night check; it only ever
    // succeeds on a genuinely dark spot. The cap is higher than the zombie's and creeper's because
    // a spider is the cheapest hostile to render (13 boxes, no arms) and because in vanilla they
    // arrive in pairs far more often than the others.
    spawnSpiderTimer -= dt;
    if(spawnSpiderTimer < 0.0f)
    {
        if((worldDayTime > 21 || worldDayTime < 6) && mainOptions.difficult != 0)
        {
            if(rand() % 50 <= 30-mSpiders.size()*1.5f && mSpiders.size() < 5 && CanSpawnHostile())
            {
                float spawnX = 0;
                float spawnY = 0;
                float spawnZ = 0;

                if(rand() % 4 != 0)
                {
                    rand() % 2 == 0 ? spawnX = playerPos.x-20-rand() % 14 : spawnX = playerPos.x+20+rand() % 14;
                    rand() % 2 == 0 ? spawnZ = playerPos.z-20-rand() % 14 : spawnZ = playerPos.z+20+rand() % 14;
                }
                else
                {
                    rand() % 2 == 0 ? spawnX = playerPos.x-8-rand() % 16 : spawnX = playerPos.x+8+rand() % 16;
                    rand() % 2 == 0 ? spawnZ = playerPos.z-8-rand() % 16 : spawnZ = playerPos.z+8+rand() % 16;
                }

                spawnY = groundHeightWater(spawnX,spawnZ);

                if(BlockFinalLight(spawnX,spawnY,spawnZ) < 0.25f) // spawn only at black areas
                {
                    if(spawnY != -1 && spawnY < WORLD_HEIGHT && spawnX < worldLimit && spawnZ < worldLimit && spawnY > 0 && spawnX > 0 && spawnZ > 0)
                    {
                        SpawnSpider(spawnX+0.5f,spawnY+1.6f,spawnZ+0.5f);
                        if(rand() % 3 == 0)
                        {
                            SpawnSpider(spawnX+2.5f,groundHeightWater(spawnX+2.0f,spawnZ+2.0f)+1.6f,spawnZ+2.5f);
                        }
                    }
                }
            }
        }
        if(rand() % 10 < 8 && mSpiders.size() < 6 && CanSpawnHostile())
        {
            int spawnX, spawnY, spawnZ;
            if(FindMobSpawnSpot(spawnX, spawnY, spawnZ, true, 12))
                SpawnSpider(spawnX+0.5f,spawnY+1.6f,spawnZ+0.5f,800);
        }
        spawnSpiderTimer = 30+rand()%30;
    }

    // Skeletons. The same two passes as the spider -- a dark spawn at any hour and a surface pass
    // gated on night -- but on the biped's +1.8 placement rather than the spider's +1.6, and with a
    // tighter roll because vanilla gives the skeleton a lower spawn weight than the spider.
    spawnSkeletonTimer -= dt;
    if(spawnSkeletonTimer < 0.0f)
    {
        if((worldDayTime > 21 || worldDayTime < 6) && mainOptions.difficult != 0)
        {
            if(rand() % 50 <= 24-mSkeletons.size()*2 && mSkeletons.size() < 4 && CanSpawnHostile())
            {
                float spawnX = 0;
                float spawnY = 0;
                float spawnZ = 0;

                if(rand() % 4 != 0)
                {
                    rand() % 2 == 0 ? spawnX = playerPos.x-20-rand() % 14 : spawnX = playerPos.x+20+rand() % 14;
                    rand() % 2 == 0 ? spawnZ = playerPos.z-20-rand() % 14 : spawnZ = playerPos.z+20+rand() % 14;
                }
                else
                {
                    rand() % 2 == 0 ? spawnX = playerPos.x-8-rand() % 16 : spawnX = playerPos.x+8+rand() % 16;
                    rand() % 2 == 0 ? spawnZ = playerPos.z-8-rand() % 16 : spawnZ = playerPos.z+8+rand() % 16;
                }

                spawnY = groundHeightWater(spawnX,spawnZ);

                if(BlockFinalLight(spawnX,spawnY,spawnZ) < 0.25f) // spawn only at black areas
                {
                    if(spawnY != -1 && spawnY < WORLD_HEIGHT && spawnX < worldLimit && spawnZ < worldLimit && spawnY > 0 && spawnX > 0 && spawnZ > 0)
                    {
                        SpawnSkeleton(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f);
                    }
                }
            }
        }
        if(rand() % 10 < 6 && mSkeletons.size() < 4 && CanSpawnHostile())
        {
            int spawnX, spawnY, spawnZ;
            if(FindMobSpawnSpot(spawnX, spawnY, spawnZ, true, 12))
                SpawnSkeleton(spawnX+0.5f,spawnY+1.8f,spawnZ+0.5f,800);
        }
        spawnSkeletonTimer = 30+rand()%30;
    }

    spawnCowsTimer -= dt;
    if(spawnCowsTimer < 0.0f)
    {
        if(worldDayTime > 6 && worldDayTime < 21)
        {
            if(rand() % 2 == 0 && PassiveMobCount() < NaturalPassiveCap())
            {
                int spawnX, spawnY, spawnZ;
                if(FindMobSpawnSpot(spawnX, spawnY, spawnZ, false, 16))
                {
                    bool pig = (rand() % 3 == 0);
                    if(pig) SpawnPig(spawnX+0.5f,spawnY+2.0f,spawnZ+0.5f);
                    else    SpawnCow(spawnX+0.5f,spawnY+2.0f,spawnZ+0.5f);
                    // second one next to it only if that spot is also clear
                    int x2 = spawnX + 2, z2 = spawnZ + 2;
                    int y2 = ColumnLoadedAt(x2, z2) ? groundHeightWater(x2, z2) : -1;
                    if(y2 > 0 && MobSpawnSpotOk(x2, y2, z2, false))
                    {
                        if(pig) SpawnPig(x2+0.5f,y2+2.0f,z2+0.5f);
                        else    SpawnCow(x2+0.5f,y2+2.0f,z2+0.5f);
                    }
                }
            }
        }
        spawnCowsTimer = (PassiveMobCount() < 4 ? 20.0f : 80.0f)+(mCows.size()+mSheeps.size()+mPigs.size())*10+rand()%60;
    }

    spawnSheepTimer -= dt;
    if(spawnSheepTimer < 0.0f)
    {
        if(worldDayTime > 6 && worldDayTime < 21)
        {
            if(rand() % 2 == 0 && PassiveMobCount() < NaturalPassiveCap())
            {
                int spawnX, spawnY, spawnZ;
                if(FindMobSpawnSpot(spawnX, spawnY, spawnZ, false, 16))
                {
                    SpawnSheep(spawnX+0.5f,spawnY+2.0f,spawnZ+0.5f);
                    int x2 = spawnX + 2, z2 = spawnZ + 2;
                    int y2 = ColumnLoadedAt(x2, z2) ? groundHeightWater(x2, z2) : -1;
                    if(y2 > 0 && MobSpawnSpotOk(x2, y2, z2, false))
                        SpawnSheep(x2+0.5f,y2+2.0f,z2+0.5f);
                }
            }
        }
        spawnSheepTimer = (PassiveMobCount() < 4 ? 25.0f : 100.0f)+(mCows.size()+mSheeps.size())*10+rand()%60;
    }

    // Chickens spawn in small groups and pop out two at a time, like the farmland birds they are.
    spawnChickenTimer -= dt;
    if(spawnChickenTimer < 0.0f)
    {
        if(worldDayTime > 6 && worldDayTime < 21)
        {
            if(rand() % 2 == 0 && PassiveMobCount() < NaturalPassiveCap())
            {
                int spawnX, spawnY, spawnZ;
                if(FindMobSpawnSpot(spawnX, spawnY, spawnZ, false, 16))
                {
                    SpawnChicken(spawnX+0.5f,spawnY+2.0f,spawnZ+0.5f);
                    int x2 = spawnX + 1, z2 = spawnZ + 1;
                    int y2 = ColumnLoadedAt(x2, z2) ? groundHeightWater(x2, z2) : -1;
                    if(y2 > 0 && MobSpawnSpotOk(x2, y2, z2, false))
                        SpawnChicken(x2+0.5f,y2+2.0f,z2+0.5f);
                }
            }
        }
        spawnChickenTimer = (PassiveMobCount() < 4 ? 30.0f : 120.0f)+(mChickens.size())*10+rand()%80;
    }
}

bool CraftWorld::MobSpawnSpotOk(int x, int y, int z, bool needDark)
{
    if(x < 1 || z < 1 || x >= worldLimit - 1 || z >= worldLimit - 1)
        return false;
    if(y < 1 || y + 3 >= WORLD_HEIGHT)
        return false;
    if(CHUNK_SIZE <= 0)
        return false;
    // only spawn in columns that already exist; never trigger a synchronous generate here
    // (during spawn-region generation the columns exist but are not flagged yet)
    ChunkColumn* col = FindColumn(ChunkFloorDiv(x, CHUNK_SIZE), ChunkFloorDiv(z, CHUNK_SIZE));
    if(!col || (!col->generated && !worldGenerating))
        return false;

    block_t ground = GetBlockNoCheck(x, y, z);
    if(ground == 0 || ground >= blockTypes.size())
        return false;
    // natural walkable ground only: no tree tops, water, ice, plants, snow layer
    if(ground != GrassBlock::getID() && ground != DirtBlock::getID() && ground != SandBlock::getID() &&
       ground != SnowSoil::getID() && ground != RockBlock::getID() && ground != Gravel::getID())
        return false;

    // a mob box is ~0.9 wide; make sure the whole 3x3 footprint two blocks up is free
    for(int dx = -1; dx <= 1; dx++)
    {
        for(int dz = -1; dz <= 1; dz++)
        {
            ChunkColumn* c = FindColumn(ChunkFloorDiv(x + dx, CHUNK_SIZE), ChunkFloorDiv(z + dz, CHUNK_SIZE));
            if(!c || (!c->generated && !worldGenerating))
                return false;
            for(int dy = 1; dy <= 2; dy++)
            {
                block_t b = GetBlockNoCheck(x + dx, y + dy, z + dz);
                if(b == 0)
                    continue;
                if(b >= blockTypes.size())
                    return false;
                if(b == WaterBlock::getID() || b == Lava::getID())
                    return false;
                if(blockTypes[b].solid && blockTypes[b].blockModel == 0)
                    return false;
            }
        }
    }

    if(needDark && BlockFinalLight(x, y + 1, z) >= 0.25f)
        return false;
    return true;
}

bool CraftWorld::FindMobSpawnSpot(int& outX, int& outY, int& outZ, bool needDark, int tries)
{
    // Mobs are culled (not updated/rendered) beyond horizontalViewDistance*14 using
    // FastDistance2d, so spawn just outside that radius - the mob only becomes visible
    // once the player walks toward it, instead of popping into an area already on screen.
    // Stay inside the ring of columns that is guaranteed generated (view+1 chunks).
    int view = (int)mainOptions.horizontalViewDistance;
    if(view < 1)
        view = 1;
    int minDist = view * 14 + 6;
    int maxDist = (view + 1) * CHUNK_SIZE - 2;
    if(maxDist <= minDist)
        maxDist = minDist + 4;
    for(int t = 0; t < tries; t++)
    {
        int dx = -maxDist + rand() % (2 * maxDist + 1);
        int dz = -maxDist + rand() % (2 * maxDist + 1);
        int d = (int)FastDistance2d(abs(dx), abs(dz));
        if(d < minDist || d > maxDist)
            continue;
        int x = (int)playerPos.x + dx;
        int z = (int)playerPos.z + dz;
        if(CHUNK_SIZE <= 0)
            return false;
        ChunkColumn* col = FindColumn(ChunkFloorDiv(x, CHUNK_SIZE), ChunkFloorDiv(z, CHUNK_SIZE));
        if(!col || !col->generated)
            continue;
        int y = groundHeightWater(x, z);
        if(y <= 0)
            continue;
        if(!MobSpawnSpotOk(x, y, z, needDark))
            continue;
        outX = x;
        outY = y;
        outZ = z;
        return true;
    }
    return false;
}

// Initial animals for a fresh world. Picks random spots inside the generated spawn
// region and only places a mob on solid natural ground with air above - the old code
// dropped them on whatever was on top (lake surface, tree canopy).
void CraftWorld::SpawnStartAnimals(int cowPairs, int sheepPairs)
{
    int spanX = genMaxX - genMinX - 40;
    int spanZ = genMaxZ - genMinZ - 40;
    if(spanX <= 0 || spanZ <= 0)
        return;
    // kind 0=cow, 1=sheep, 2=pig, 3=chicken (one starter pair each of bird and pig)
    for(int kind = 0; kind < 4; kind++)
    {
        int pairs = kind == 0 ? cowPairs : (kind == 1 ? sheepPairs : 1);
        for(int p = 0; p < pairs; p++)
        {
            for(int t = 0; t < 40; t++)
            {
                int x = genMinX + 20 + rand() % spanX;
                int z = genMinZ + 20 + rand() % spanZ;
                int y = groundHeightWater(x, z);
                if(y <= 0 || !MobSpawnSpotOk(x, y, z, false))
                    continue;
                if(kind == 0)      SpawnCow(x + 0.5f, y + 2.0f, z + 0.5f);
                else if(kind == 1) SpawnSheep(x + 0.5f, y + 2.0f, z + 0.5f);
                else if(kind == 2) SpawnPig(x + 0.5f, y + 2.0f, z + 0.5f);
                else               SpawnChicken(x + 0.5f, y + 2.0f, z + 0.5f);
                int x2 = x + 2, z2 = z + 2;
                int y2 = groundHeightWater(x2, z2);
                if(y2 > 0 && MobSpawnSpotOk(x2, y2, z2, false))
                {
                    if(kind == 0)      SpawnCow(x2 + 0.5f, y2 + 2.0f, z2 + 0.5f);
                    else if(kind == 1) SpawnSheep(x2 + 0.5f, y2 + 2.0f, z2 + 0.5f);
                    else if(kind == 2) SpawnPig(x2 + 0.5f, y2 + 2.0f, z2 + 0.5f);
                    else               SpawnChicken(x2 + 0.5f, y2 + 2.0f, z2 + 0.5f);
                }
                break;
            }
        }
    }
}

void CraftWorld::UpdateWorldProcesses(float dt)
{
    MC_PERF_PROCESS_BEGIN();
    const unsigned int processStartUs = sceKernelGetSystemTimeLow();
    streamOpsThisFrame = 0;
    streamTickTimer += dt;
    if(streamTickTimer >= gStreamNextDelay)
    {
        streamTickTimer = 0.0f;
        UpdateStreaming();
    }
    // How long this frame may spend building geometry.
    //
    // 4 ms when the frame is already streaming, or the previous one was heavy: that is the hitch, and
    // mesh time must not be piled on it. 8 ms when the frame is quiet and the heap is not the thing
    // running out. The sections are ones that were going to be built anyway -- this only spends idle
    // time on the nearest visible one, so new ground draws in over fewer frames without a larger worst
    // frame. Below ~1.5 MB free the nether stays on the slow rate; allocating meshes any faster there
    // is how it runs out of memory.
    const bool quietMeshFrame = (streamOpsThisFrame == 0)
        && (lastProcessUs <= 14000u)
        && (HeapFreeKB() > 1536);
    const unsigned int meshBudgetUs = quietMeshFrame ? 8000u : 4000u;
    const unsigned int meshBudgetStart = sceKernelGetSystemTimeLow();

    // The section rebuild in flight gets this frame's slice before anything else can ask for one, and
    // `meshBuildsThisFrame` stays set for as long as it runs so every drain below keeps its hands off
    // the caches the job is walking against. See RequestMeshRebuild.
    if(meshJobKind != kMeshJobNone)
    {
        MC_PERF_PHASE_BEGIN(kPhMeshJob);
        bool jobDone = AdvanceMeshJob(meshBudgetUs);
        MC_PERF_PHASE_END(kPhMeshJob);
        // A finished job releases the frame's rebuild claim here, not at the end of drawWorld. Otherwise
        // the scan below -- what gives a freshly loaded column its first mesh -- waits out the rest of
        // this frame, which is the gap that leaves new ground untextured after the column has arrived.
        meshBuildsThisFrame = jobDone ? 0 : 1;
    }
    meshIdleTimer += dt;
    // The zone scan is the safety net for sections that never got geometry. Streaming no longer spends
    // the whole frame, so the scan does not have to wait 0.08 s between sections: one section is still
    // all it enqueues, and that section is a sliced job.
    if(meshPending && meshBuildsThisFrame == 0)
    {
        meshIdleTimer = 0.0f;
        MC_PERF_PHASE_BEGIN(kPhMeshTick);
        TryMeshOneSection();
        MC_PERF_PHASE_END(kPhMeshTick);

        // The scan only queues the job. If this frame has not already spent its mesh budget, walk the
        // first slice now so the new section does not sit invisible for an extra frame.
        unsigned int meshUsed = (unsigned int)((int)sceKernelGetSystemTimeLow() - (int)meshBudgetStart);
        if(meshJobKind != kMeshJobNone && meshUsed + 2000u < meshBudgetUs)
        {
            MC_PERF_PHASE_BEGIN(kPhMeshJob);
            bool jobDone = AdvanceMeshJob(meshBudgetUs - meshUsed);
            MC_PERF_PHASE_END(kPhMeshJob);
            meshBuildsThisFrame = jobDone ? 0 : 1;
        }
    }
    if(meshBuildsThisFrame == 0 && physicsBoxesThisFrame == 0 && streamOpsThisFrame == 0)
    {
        MC_PERF_PHASE_BEGIN(kPhUnmeshed);
        TickUnmeshedColumns(dt);
        MC_PERF_PHASE_END(kPhUnmeshed);
    }
#if MC_FRAME_PROFILE
    AuditHoles(dt);
#endif
    // The clock, compass, water/lava/fire/portal texel copies happen in ApplyTextureAnimation, which
    // the play states call after StartFrame: the update runs while the GE is still drawing the last
    // frame, and these write into atlases that frame samples from.
    animPendingDt += dt;
    MC_PERF_PHASE_BEGIN(kPhSim);
    SpawnMobs(dt);
    //stuff that goes on tick
    //1 tick = 1 second

    minuteTimer += dt;
    if(minuteTimer > 60.0f)
    {
        mainStatistics.minutesPlayed += 1;
        minuteTimer -= 60.0f;
    }

    if(updatingChunks.empty() == false)
    {
        updatingChunksTimer += dt;
        updatingChunksTimerMax = 30.0f/updatingChunks.size();
        if(updatingChunksTimerMax < 1.25f)
            updatingChunksTimerMax = 1.25f;
        if(updatingChunksTimer > updatingChunksTimerMax)
        {
            int growId = updatingChunks[updatingChunksPosition];
            unsigned int beforeHash = 0;
            if(MeshIdValid(growId))
                beforeHash = HashSectionBlocks(mChunks[growId]->chunkStartX, mChunks[growId]->chunkStartY, mChunks[growId]->chunkStartZ);
            UpdateChunkGrowingBlocks(growId,updatingChunksPosition);

            if(MeshIdValid(growId))
            {
                unsigned int afterHash = HashSectionBlocks(mChunks[growId]->chunkStartX, mChunks[growId]->chunkStartY, mChunks[growId]->chunkStartZ);
                if(beforeHash != afterHash)
                    mChunks[growId]->needUpdate = true;
            }

            updatingChunksPosition += 1;
            if(updatingChunksPosition >= updatingChunks.size())
            {
                updatingChunksPosition = 0;
            }
            updatingChunksTimer = 0.0f;
        }
    }

    updateFastChunkTimer += dt;
    if(updateFastChunkTimer >= 0.05f)
    {
        updateFastChunkTimer -= 0.05f;

        if(toFastUpdate.empty() == false)
        {
            if(sSkipQueuedRebuilds <= 0 && meshBuildsThisFrame == 0 && physicsBoxesThisFrame == 0)
            {
                unsigned int o = 0;
                MC_PERF_INC(kFastDrained);
                RebuildFullMeshChunk(toFastUpdate[o]);
                toFastUpdate.erase(toFastUpdate.begin());
            }
        }
    }

    if(sSkipQueuedRebuilds <= 0 && meshBuildsThisFrame == 0 && physicsBoxesThisFrame == 0)
        ProcessQueuedLightArea();

    fetchNearestChunksTimer += dt;
    if(fetchNearestChunksTimer >= 0.5f)
    {
        FetchNearestChunks();
        fetchNearestChunksTimer = 0.0f;
    }

    // The queued rebuilds drain every frame now, paced by the job rather than by a clock.
    //
    // This used to wait 0.21 s between drains (0.18 at view 4), which is a throttle from when a rebuild
    // was a synchronous ~30 ms walk that had to be kept off the frame. A rebuild is a job spread over
    // frames now (see RequestMeshRebuild), so the frame is already bounded by the job's own 4 ms slice
    // and `meshBuildsThisFrame` is what refuses a second one. All the timer did was idle through the
    // frames between one job finishing and the next one starting.
    //
    // The cost of that idle time is visible from the player's seat, because this drain is one of the two
    // paths a freshly streamed section takes to get geometry (the other is the nearest-visible pick in
    // drawWorld, gated on the same counter). Measured with the nether probe at `mesh/s full=5`: one
    // section every 0.21 s, so a column that has just loaded -- two or three sections -- stands there
    // untextured for half a second and a row of them for two and a half. Breaking a block dirties its
    // section directly and jumps that queue, which is why touching the world made the rest appear.
    //
    // Draining per frame spends the same slice per frame it always did; it simply stops idling through
    // two thirds of them. Throughput rises to whatever the job length allows, at no extra per-frame cost.
    MC_PERF_PHASE_END(kPhSim);
    chunksCreatedInFrameCount = 0;

    if(toUpdate.empty() == false)
    {
        if(sSkipQueuedRebuilds <= 0 && meshBuildsThisFrame == 0 && physicsBoxesThisFrame == 0)
        {
            MC_PERF_PHASE_BEGIN(kPhMeshDrain);
            // Prefer an empty mesh (a hole) nearest the player. FIFO took whatever was queued
            // first -- often a new frontier section -- and the hole the player was crossing
            // waited. A failed section is skipped rather than removed until meshRebuildRetryDue.
            int pick = -1;
            int bestDrain = 0x7fffffff;
            bool haveHole = false;
            for(unsigned int i = 0; i < toUpdate.size(); i++)
            {
                const int cand = toUpdate[i];
                if(!MeshIdValid(cand))
                {
                    pick = (int)i;      // the section is gone; drop the entry
                    break;
                }
                const bool failedBefore = mChunks[cand]->outOfMemory || mTransparentChunks[cand]->outOfMemory;
                if(failedBefore && !meshRebuildRetryDue())
                    continue;
                SimpleMeshChunk* o = mChunks[cand];
                SimpleMeshChunk* t = cand < (int)mTransparentChunks.size() ? mTransparentChunks[cand] : 0;
                const int tris = (o ? o->trienglesCount : 0) + (t ? t->trienglesCount : 0);
                const bool hole = (tris == 0);
                ChunkColumn* dcol = o ? FindColumn(ChunkFloorDiv(o->chunkStartX, CHUNK_SIZE),
                                                   ChunkFloorDiv(o->chunkStartZ, CHUNK_SIZE)) : 0;
                int score = o ? SectionMeshPriority(o->chunkStartX, o->chunkStartY, o->chunkStartZ, dcol)
                              : 0x7fffffff;
                if(!hole)
                    score += 1000000;
                if(hole)
                {
                    if(!haveHole || score < bestDrain)
                    {
                        haveHole = true;
                        bestDrain = score;
                        pick = (int)i;
                    }
                }
                else if(!haveHole && score < bestDrain)
                {
                    bestDrain = score;
                    pick = (int)i;
                }
            }
            if(pick >= 0)
            {
                MC_PERF_INC(kToUpdateDrained);
                RebuildFullMeshChunk(toUpdate[pick]);
                toUpdate.erase(toUpdate.begin() + pick);
            }
        }
        MC_PERF_PHASE_END(kPhMeshDrain);
    }

    // Last of the rebuilders, so world loading and lighting keep their claim on the frame's one-chunk
    // budget and a running redstone clock waits for a frame they left free. See QueueRedstoneMesh.
    //
    // A piston's stroke is advanced immediately before it, so the pose the flush builds is the one the
    // clock is at when the frame's budget was spent rather than the one it was at a frame ago: a stroke
    // is then drawn in as many poses as there are frames to draw it in, and never asks for more rebuilds
    // than the queue will take. See StartPistonStroke.
    MC_PERF_PHASE_BEGIN(kPhMeshDrain);
    UpdatePistonStrokes(dt);
    if(sSkipQueuedRebuilds <= 0 && meshBuildsThisFrame == 0 && physicsBoxesThisFrame == 0)
        FlushRedstoneMeshes();
    MC_PERF_PHASE_END(kPhMeshDrain);

    if(sSkipQueuedRebuilds > 0)
        sSkipQueuedRebuilds--;
    streamOpsThisFrame = 0;

    // Hand the streamer the length of the frame it just ran in, so it can keep a card write off a
    // busy one. Signed subtraction, so it stays correct across the ~71-minute clock wrap.
    lastProcessUs = (unsigned int)((int)sceKernelGetSystemTimeLow() - (int)processStartUs);
    MC_PERF_PROCESS_END();
    MC_PERF_FRAME(dt,
                  (int)(mZombies.size() + mCows.size() + mCreepers.size() + mPigs.size() + mSheeps.size()
                        + mChickens.size() + mSpiders.size() + mSkeletons.size()),
                  (int)mDrops.size(), (int)mTNTs.size(), (int)mArrows.size(), Particle::ActiveCount());
}

void CraftWorld::UpdateWorldTime(float dt)
{
    worldTime += dt;
    if(worldTime >= 0.05f)
    {
        if(worldDayTime >= 22 || worldDayTime < 6)
        {
            skyTime += 180.0f/8.0f/50.0f/20.0f;
        }
        if(worldDayTime >= 6 && worldDayTime < 22)
        {
            skyTime += 180.0f/16.0f/50.0f/20.0f;
        }
        worldTime -= 0.05f;
    }

    //world day/night time
    worldHour += dt;
    if(worldHour >= 50.0f)//one game hour = 50 sec
    {
        worldHour -= 50.0f;
        worldDayTime += 1.0f;
        if(worldDayTime > 23.0f)
        {
            worldDayTime = 0.0f;
            mainStatistics.daysInGame += 1;
        }

        Vector3 oldLightColor = lightColor;
        SetWolrdTime(worldDayTime);

        if(oldLightColor != lightColor)
        {
            SetAllChunksToUpdate();
            chunksWasSetToUpdate = true;
        }
        else
        {
            chunksWasSetToUpdate = false;
        }
    }
}

int CraftWorld::GetBlockTypesCount()
{
    return blockTypes.size();
}

int CraftWorld::GetItemTypesCount()
{
    return itemTypes.size();
}

int CraftWorld::groundHeight(const int x, const int z)
{
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return -1;

    for (int y = WORLD_HEIGHT - 1; y >= 0; --y)
    {
        block_t Block = GetBlock(x, y, z);
        if (Block != 0 && Block != 4)
        {
            return y;
        }
    }

    return -1;
}

int CraftWorld::groundHeight(const int x, const int y, const int z)
{
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return -1;

    for (int yy = y; yy >= 0; --yy)
    {
        block_t Block = GetBlock(x, yy, z);
        if (Block != 0 && Block != WaterBlock::getID())
        {
            return yy;
        }
    }

    return -1;
}

int CraftWorld::groundHeightFromBottom(const int x, const int y, const int z)
{
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return -1;


    for (int yy = y; yy < 56; yy++)
    {
        if(yy < 3)
        {
            yy = 3;
        }
        block_t Block = GetBlock(x, yy, z);
        if (Block == 0)
        {
            return yy;
        }
    }

    return -1;
}

int CraftWorld::groundHeightExcept(const int x, const int z, int bl)
{
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return -1;


    for (int y = WORLD_HEIGHT - 1; y >= 0; --y)
    {
        block_t Block = GetBlock(x, y, z);
        if(Block == bl)
        {
            continue;
        }
        if (Block != 0) return (y);
    }

    return -1;
}

int CraftWorld::groundHeightWater(const int x, const int z)
{
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return -1;

    for (int y = WORLD_HEIGHT - 1; y >= 0; --y)
    {
        block_t Block = GetBlock(x, y, z);
        if (Block != 0)
        {
            return y;
        }
    }

    return -1;
}

int CraftWorld::BlockSoundAtPos(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    //outside the world so collision
    if (x <= 0 || y <= 0 || z <= 0  || x >= worldLimit-1 || y >= WORLD_HEIGHT-1 || z >= worldLimit-1) return -1;

    return blockTypes[GetBlockNoCheck(x,y,z)].soundType;
}


bool CraftWorld::SolidAtPoint(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    float relative_x = (float)pos.x-x;
    float relative_y = (float)pos.y-y;
    float relative_z = (float)pos.z-z;

    //outside the world so collision
    if (y <= 0 || y >= WORLD_HEIGHT-1) return true;
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return true;

    if(CHUNK_SIZE > 0 && !worldGenerating)
    {
        // Collision never generates terrain any more: a mob wandering in unloaded
        // land used to force a synchronous column generate every frame (and kept far
        // columns alive, eating RAM). Missing column = solid, so nothing falls through.
        int cx = ChunkFloorDiv(x, CHUNK_SIZE);
        int cz = ChunkFloorDiv(z, CHUNK_SIZE);
        ChunkColumn* col = FindColumn(cx, cz);
        if(!col || !col->generated)
            return true;
    }

    //don't collide with water and air
    if(BlockSolid(x,y,z) == 0)
    {
        return false;
    }

    int posBlockID = GetBlockNoCheck(x,y,z);
    if(blockTypes[posBlockID].blockModel == 1) // if it is half block
    {
        if(relative_y < 0.5f)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    if(blockTypes[posBlockID].blockModel == 9)
    {
        if(relative_x < 0.5f)
        {
            return true;
        }
        else
        {
            if(relative_y < 0.5f)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    if(blockTypes[posBlockID].blockModel == 8)
    {
        if(relative_x >= 0.5f)
        {
            return true;
        }
        else
        {
            if(relative_y < 0.5f)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    if(blockTypes[posBlockID].blockModel == 7)
    {
        if(relative_z < 0.5f)
        {
            return true;
        }
        else
        {
            if(relative_y < 0.5f)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    if(blockTypes[posBlockID].blockModel == 6)
    {
        if(relative_z >= 0.5f)
        {
            return true;
        }
        else
        {
            if(relative_y < 0.5f)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    return true;
}

bool CraftWorld::SolidAtPointForPlayerOnGround(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    //outside the world so collision
    if (y <= 0 || y >= WORLD_HEIGHT-1) return false;
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return true;

    //4 corners
    if(SolidAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z-playerCollisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z+playerCollisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z-playerCollisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z+playerCollisionSize))) return true;
    //
    return false;
}


bool CraftWorld::SolidAtPointForPlayer(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    //outside the world so collision
    if (y <= 0 || y >= WORLD_HEIGHT-1) return false;
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return true;

    //4 corners
    if(SolidAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z-playerCollisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z+playerCollisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z-playerCollisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z+playerCollisionSize))) return true;
    //
    return false;
}

bool CraftWorld::SolidAtPointForMovingPlayer(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    //outside the world so collision
    if (y <= 0 || y >= WORLD_HEIGHT-1) return false;
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return true;

    //4 corners
    if(SolidAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z-playerCollisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z+playerCollisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z-playerCollisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z+playerCollisionSize))) return true;
    //
    return false;
}

bool CraftWorld::SolidAtPointForPlayerHead(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    //outside the world so collision
    if (y <= 0 || y >= WORLD_HEIGHT-1) return false;
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return true;

    float collisionSize = playerCollisionSize-0.125f;
    //4 corners
    if(SolidAtPoint(Vector3(pos.x-collisionSize,pos.y,pos.z-collisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+collisionSize,pos.y,pos.z+collisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+collisionSize,pos.y,pos.z-collisionSize))) return true;
    if(SolidAtPoint(Vector3(pos.x-collisionSize,pos.y,pos.z+collisionSize))) return true;
    //
    return false;
}

bool CraftWorld::SolidAtPointForEntity(Vector3 pos, Vector3 collider1)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    float xSize = collider1.x;
    float zSize = collider1.z;

    //outside the world so collision
    if (y <= 0 || y >= WORLD_HEIGHT-1) return false;
    if (x < 0 || z < 0 || x >= worldLimit || z >= worldLimit) return true;
    //4 corners
    if(SolidAtPoint(Vector3(pos.x-xSize,pos.y,pos.z-zSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+xSize,pos.y,pos.z+zSize))) return true;
    if(SolidAtPoint(Vector3(pos.x+xSize,pos.y,pos.z-zSize))) return true;
    if(SolidAtPoint(Vector3(pos.x-xSize,pos.y,pos.z+zSize))) return true;
    //
    return false;
}

bool CraftWorld::HighStairAtPoint(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    float relativePlayer_x = pos.x-(float)x;
    //float relativePlayer_y = pos.y-(float)y;
    float relativePlayer_z = pos.z-(float)z;

    bool highPoint = false;

    //outside the world so collision
    if (x <= 0 || y <= 0 || z <= 0  || x >= worldLimit-1 || y >= WORLD_HEIGHT-1 || z >= worldLimit-1) return true;

    int posBlockID = GetBlock(x,y,z);
    bool stairsFound = false;

    if(blockTypes[posBlockID].blockModel == 9)
    {
        stairsFound = true;
        if(relativePlayer_x < 0.5f)
        {
            highPoint = true;
        }
    }
    if(blockTypes[posBlockID].blockModel == 8)
    {
        stairsFound = true;
        if(relativePlayer_x >= 0.5f)
        {
            highPoint = true;
        }
    }
    if(blockTypes[posBlockID].blockModel == 7)
    {
        stairsFound = true;
        if(relativePlayer_z < 0.5f)
        {
            highPoint = true;
        }
    }
    if(blockTypes[posBlockID].blockModel == 6)
    {
        stairsFound = true;
        if(relativePlayer_z >= 0.5f)
        {
            highPoint = true;
        }
    }

    if(stairsFound == false)
    {
        if(SolidAtPoint(Vector3(pos.x,pos.y+0.5f,pos.z)))
        {
            highPoint = true;
        }
    }

    return highPoint;
}

bool CraftWorld::PlayerInWater(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    if (x < 0 || y < 0 || z < 0  || x >= worldLimit-1 || y >= WORLD_HEIGHT-1 || z >= worldLimit-1) return false;

    if(BlockIsWater(GetBlockNoCheck(x,y,z)) || GetBlockNoCheck(x,y,z) == IceBlock::getID())
        return true;

    return false;
}

bool CraftWorld::PlayerOnHalfBlock(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;
    int points = 0;
    int badPoints = 0;
    if (x < 0 || y < 0 || z < 0  || x >= worldLimit-1 || y >= WORLD_HEIGHT-1 || z >= worldLimit-1) return false;

    int posBlockID = BlockAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z-playerCollisionSize));
    if(blockTypes[posBlockID].blockModel == 1 ||
       blockTypes[posBlockID].solid == false  ||
       (blockTypes[posBlockID].blockModel >= 6 && blockTypes[posBlockID].blockModel <= 9 && !HighStairAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z-playerCollisionSize)))
       )
    {
        if(blockTypes[posBlockID].solid == false)
        {
            badPoints++;
        }
        points ++;
    }


    posBlockID = BlockAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z-playerCollisionSize));
    if(blockTypes[posBlockID].blockModel == 1 ||
       blockTypes[posBlockID].solid == false  ||
       (blockTypes[posBlockID].blockModel >= 6 && blockTypes[posBlockID].blockModel <= 9 && !HighStairAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z-playerCollisionSize)))
       )
    {
        if(blockTypes[posBlockID].solid == false)
        {
            badPoints++;
        }
        points ++;
    }


    posBlockID = BlockAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z+playerCollisionSize));
    if(blockTypes[posBlockID].blockModel == 1 ||
       blockTypes[posBlockID].solid == false  ||
       (blockTypes[posBlockID].blockModel >= 6 && blockTypes[posBlockID].blockModel <= 9 && !HighStairAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z+playerCollisionSize)))
       )
    {
        if(blockTypes[posBlockID].solid == false)
        {
            badPoints++;
        }
        points ++;
    }

    posBlockID = BlockAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z+playerCollisionSize));
    if(blockTypes[posBlockID].blockModel == 1 ||
       blockTypes[posBlockID].solid == false  ||
       (blockTypes[posBlockID].blockModel >= 6 && blockTypes[posBlockID].blockModel <= 9 && !HighStairAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z+playerCollisionSize)))
       )
    {
        if(blockTypes[posBlockID].solid == false)
        {
            badPoints++;
        }
        points ++;
    }

    if(points == 4 && badPoints != 4)
    {
        return true;
    }
    return false;
}

bool CraftWorld::PlayerOnStairsBlock(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    if (x < 0 || y < 0 || z < 0  || x >= worldLimit-1 || y >= WORLD_HEIGHT-1 || z >= worldLimit-1) return false;

    int posBlockID = BlockAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z-playerCollisionSize));
    if(blockTypes[posBlockID].blockModel >= 6 && blockTypes[posBlockID].blockModel <= 9)
    {
        return true;
    }
    posBlockID = BlockAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z-playerCollisionSize));
    if(blockTypes[posBlockID].blockModel >= 6 && blockTypes[posBlockID].blockModel <= 9)
    {
        return true;
    }

    posBlockID = BlockAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z+playerCollisionSize));
    if(blockTypes[posBlockID].blockModel >= 6 && blockTypes[posBlockID].blockModel <= 9)
    {
        return true;
    }

    posBlockID = BlockAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z+playerCollisionSize));
    if(blockTypes[posBlockID].blockModel >= 6 && blockTypes[posBlockID].blockModel <= 9)
    {
        return true;
    }
    return false;
}

bool CraftWorld::PlayerOnHighStair(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    bool onHighStair = false;

    if (x < 0 || y < 0 || z < 0  || x >= worldLimit-1 || y >= WORLD_HEIGHT-1 || z >= worldLimit-1) return false;

    if(HighStairAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z-playerCollisionSize)) ||
       HighStairAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z-playerCollisionSize)) ||
       HighStairAtPoint(Vector3(pos.x-playerCollisionSize,pos.y,pos.z+playerCollisionSize)) ||
       HighStairAtPoint(Vector3(pos.x+playerCollisionSize,pos.y,pos.z+playerCollisionSize)))
    {
        onHighStair = true;
    }
    else
    {
        onHighStair = false;
    }


    return onHighStair;
}

bool CraftWorld::PlayerInLava(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    if (x < 0 || y < 0 || z < 0  || x >= worldLimit-1 || y >= WORLD_HEIGHT-1 || z >= worldLimit-1) return false;

	if (BlockIsLava(GetBlock(x, y, z))) 
	{
		return true;
	}
	if (GetBlock(x, y, z) == Fire::getID())
	{
		return true;
	}
        
    return false;
}

bool CraftWorld::PlayerInBlock(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    if (x < 0 || y < 0 || z < 0  || x >= worldLimit-1 || y >= WORLD_HEIGHT-1 || z >= worldLimit-1) return false;

    if(GetBlock(x,y,z) != Lava::getID() || GetBlock(x,y,z) != 4 || GetBlock(x,y,z) != 0)
        return true;

    return false;
}

bool CraftWorld::PlayerCollisionWithCactus(Vector3 pos)
{
    float x = pos.x;
    float y = pos.y;
    float z = pos.z;

    float collision_distance = 1.05f;

    if(GetBlock(x-1,y,z) == CactusBlock::getID())
    {
        float cactus_x = (int)(x-1) + 0.5f;

        if(abs(x-cactus_x) <= collision_distance)
        {
            return true;
        }
    }
    if(GetBlock(x+1,y,z) == CactusBlock::getID())
    {
        float cactus_x = (int)(x+1) + 0.5f;

        if(abs(x-cactus_x) <= collision_distance)
        {
            return true;
        }
    }

    if(GetBlock(x,y,z-1) == CactusBlock::getID())
    {
        float cactus_z = (int)(z-1) + 0.5f;

        if(abs(z-cactus_z) <= collision_distance)
        {
            return true;
        }
    }
    if(GetBlock(x,y,z+1) == CactusBlock::getID())
    {
        float cactus_z = (int)(z+1) + 0.5f;

        if(abs(z-cactus_z) <= collision_distance)
        {
            return true;
        }
    }

    return false;
}

bool CraftWorld::PlayerCollisionWithLadder(Vector3 pos)
{
    float x = pos.x;
    float y = pos.y;
    float z = pos.z;

    float pixel = 10.0f/16.0f;

    if (x < 0 || y < 0 || z < 0  || x >= worldLimit-1 || y >= WORLD_HEIGHT-1 || z >= worldLimit-1) return false;

    if(GetBlock(x,y,z) == Ladder1::getID())
    {
        if(BlockSolid(x-pixel,y,z) || BlockSolid(x-pixel,y-0.7f,z) || BlockSolid(x-pixel,y-1.55f,z))
        {
            return true;
        }
    }
    if(GetBlock(x,y,z) == Ladder2::getID())
    {
        if(BlockSolid(x+pixel,y,z) || BlockSolid(x+pixel,y-0.7f,z) || BlockSolid(x+pixel,y-1.55f,z))
        {
            return true;
        }
    }
    if(GetBlock(x,y,z) == Ladder3::getID())
    {
        if(BlockSolid(x,y,z-pixel) || BlockSolid(x,y-0.7f,z-pixel) || BlockSolid(x,y-1.55f,z-pixel))
        {
            return true;
        }
    }
    if(GetBlock(x,y,z) == Ladder4::getID())
    {
        if(BlockSolid(x,y,z+pixel) || BlockSolid(x,y-0.7f,z+pixel) || BlockSolid(x,y-1.55f,z+pixel))
        {
            return true;
        }
    }

    return false;
}

block_t CraftWorld::BlockAtPoint(Vector3 pos)
{
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    float relative_x = (float)pos.x-x;
    float relative_y = (float)pos.y-y;
    float relative_z = (float)pos.z-z;

    if (x < 0 || y < 0 || z < 0  || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit) return -1;

    block_t block = GetBlock(x,y,z);

    if(blockTypes[block].blockModel == 1)
    {
        if(relative_y < 0.5f)
        {
            return block;
        }
        else
        {
            return 0;
        }
    }

    if(blockTypes[block].blockModel == 9)
    {
        if(relative_x < 0.5f)
        {
            return block;
        }
        else
        {
            if(relative_y < 0.5f)
            {
                return block;
            }
            else
            {
                return 0;
            }
        }
    }
    if(blockTypes[block].blockModel == 8)
    {
        if(relative_x >= 0.5f)
        {
            return block;
        }
        else
        {
            if(relative_y < 0.5f)
            {
                return block;
            }
            else
            {
                return 0;
            }
        }
    }
    if(blockTypes[block].blockModel == 7)
    {
        if(relative_z < 0.5f)
        {
            return block;
        }
        else
        {
            if(relative_y < 0.5f)
            {
                return block;
            }
            else
            {
                return 0;
            }
        }
    }
    if(blockTypes[block].blockModel == 6)
    {
        if(relative_z >= 0.5f)
        {
            return block;
        }
        else
        {
            if(relative_y < 0.5f)
            {
                return block;
            }
            else
            {
                return 0;
            }
        }
    }

    return block;
}

float CraftWorld::LengthDirX(float horAngle, float verAngle)
{
    return -sinf(horAngle) * cosf(verAngle);
}

float CraftWorld::LengthDirY(float horAngle, float verAngle)
{
    return sinf(verAngle);
}

float CraftWorld::LengthDirZ(float horAngle, float verAngle)
{
    return cosf(horAngle) * cosf(verAngle);
}

float CraftWorld::Distance3d(float x1, float y1, float z1, float x2, float y2, float z2)
{
    return sqrt(pow((x1-x2),2)+pow((y1-y2),2)+pow((z1-z2),2));
}

float CraftWorld::Distance2d(float x1, float y1, float x2, float y2)
{
    return sqrt(pow((x1-x2),2)+pow((y1-y2),2));
}

unsigned int CraftWorld::FastDistance2d(unsigned int dx, unsigned int dy)
{
    unsigned int w;
    if ( dy < dx ) {
        w = dy >> 2;
        return (dx + w + (w >> 1));
    }
    else {
        w = dx >> 2;
        return (dy + w + (w >> 1));
    }
}


float CraftWorld::TaxicabDistance2d(float x1, float y1, float x2, float y2)
{
    return abs(x1-x2)+abs(y1-y2);
}


void CraftWorld::BuildExplodeSphere(float radius, int X, int Y, int Z)
{
    int wakeR = (int)radius + 2;
    WakeBuriedSections(X - wakeR, Y - wakeR, Z - wakeR, X + wakeR, Y + wakeR, Z + wakeR);
    std::vector<int> toUpdate2;
    std::vector<int> dropIDs;

    float reference = 125 / 20.0f;

    float latitudes = (float)(radius * reference);
    float longitudes = (float)(latitudes/2);

    float latitude_increment = PI*2 / latitudes;
    float longitude_increment = PI / longitudes;

    bool wasLight = false;

    for (float u = 0; u < PI*2; u += latitude_increment)
    {
        for (float t = 0; t < PI; t += longitude_increment)
        {

            float rad = radius;

            int x = X + (rad * sinf(t) * sinf(u));
            int y = Y + (rad * cosf(t));
            int z = Z + (rad * sinf(t) * cosf(u));

            if(x < 0 || x >= worldLimit || y < 0 || y >= WORLD_HEIGHT || z < 0 || z >= worldLimit)
            {
                continue;
            }

            if(x > X)
            {
                for(int x2 = x; x2 >= X - (x2-X); x2 --)
                {
                    block_t& block = GetBlock(x2,y,z);
                    if(block == bedrock::getID() || block == Obsidian::getID() || BlockIsWater(block) || BlockIsLava(block) || block == 0)
                    {
                        continue;
                    }

                    if(block == TNTBlock::getID())
                    {
                        Vector3 testPos1 = Vector3(x2,y,z);

                        SpawnTNTentity(((int)testPos1.x)+0.5f,((int)testPos1.y)+0.5f,((int)testPos1.z)+0.5f,0.5f+(rand()%20)*0.05f);

                        bool pushToUpdate = true;
                        int id = getChunkId(testPos1);
                        for(unsigned int i = 0; i < toUpdate2.size(); i++)
                        {
                            if(toUpdate2[i] == id)
                            {
                                pushToUpdate = false;
                            }
                        }

                        if(pushToUpdate == true)
                        {
                            toUpdate2.push_back(id);
                        }
                        block = 0;
                        continue;
                    }
                    if(LightSourceBlock(block))
                    {
                        RemoveLigtSourceAtPosition(x2,y,z,0);
                        wasLight = true;
                    }
                    if(block >= ItemFrame1::getID() && block <= ItemFrame4::getID())
                    {
                        DestroyItemFrameEntity(Vector3(x2,y,z));
                    }

                    switch(blockTypes[block].material)
                    {
                        case 1:
                        if(rand() % 12 < 11)
                        {
                            if(rand() % 10 == 0)
                            {
                                dropIDs.push_back(blockTypes[block].loot);
                                //DropThis(blockTypes[block].loot, Vector3(x2,y,z));
                            }
                            block = 0;

                        }
                        break;
                        case 2:
                        if(rand() % 12 < 11)
                        {
                            if(rand() % 10 == 0)
                            {
                                dropIDs.push_back(blockTypes[block].loot);
                            }
                            block = 0;
                        }
                        break;
                        case 3:
                        if(rand() % 10 == 0)
                        {
                            dropIDs.push_back(blockTypes[block].loot);
                        }
                        block = 0;
                        break;
                        case 4:
                        block = 0;
                        break;
                        case 7:
                        if(rand() % 4 > 0)
                        {
                            dropIDs.push_back(blockTypes[block].loot);
                        }
                        block = 0;
                        break;
                        case 0:
                        block = 0;
                        break;
                    }
                }
            }

            if(x < X)
            {
                for(int x2 = x; x2 <= X + (X-x2); x2 ++)
                {
                    block_t& block = GetBlock(x2,y,z);
                    if(block == bedrock::getID() || block == Obsidian::getID() || BlockIsWater(block) || BlockIsLava(block) || block == 0)
                    {
                        continue;
                    }

                    if(block == TNTBlock::getID())
                    {
                        Vector3 testPos1 = Vector3(x2,y,z);

                        SpawnTNTentity(((int)testPos1.x)+0.5f,((int)testPos1.y)+0.5f,((int)testPos1.z)+0.5f,0.5f+(rand()%20)*0.05f);
                        //RebuildOpaqueMeshChunk(getChunkId(testPos1));

                        bool pushToUpdate = true;
                        int id = getChunkId(testPos1);
                        for(unsigned int i = 0; i < toUpdate2.size(); i++)
                        {
                            if(toUpdate2[i] == id)
                            {
                                pushToUpdate = false;
                            }
                        }

                        if(pushToUpdate == true)
                        {
                            toUpdate2.push_back(id);
                        }
                        block = 0;
                        continue;
                    }
                    if(LightSourceBlock(block))
                    {
                        RemoveLigtSourceAtPosition(x2,y,z,0);
                        wasLight = true;
                    }
                    if(block >= ItemFrame1::getID() && block <= ItemFrame4::getID())
                    {
                        DestroyItemFrameEntity(Vector3(x2,y,z));
                    }
                    switch(blockTypes[block].material)
                    {
                        case 0:
                        block = 0;
                        break;
                        case 1:
                        if(rand() % 12 < 11)
                        {
                            if(rand() % 10 == 0)
                            {
                                dropIDs.push_back(blockTypes[block].loot);
                            }
                            block = 0;

                        }
                        break;
                        case 2:
                        if(rand() % 12 < 11)
                        {
                            if(rand() % 10 == 0)
                            {
                                dropIDs.push_back(blockTypes[block].loot);
                            }
                            block = 0;
                        }
                        break;
                        case 3:
                        if(rand() % 10 == 0)
                        {
                            dropIDs.push_back(blockTypes[block].loot);
                        }
                        block = 0;
                        break;
                        case 4:
                        block = 0;
                        break;
                        case 7:
                        if(rand() % 4 > 0)
                        {
                            dropIDs.push_back(blockTypes[block].loot);
                        }
                        block = 0;
                        break;
                    }
                }
            }
        }
    }

    for(int x2 = X-radius; x2 <= X+radius; x2++)
    {
        for(int z2 = Z-radius; z2 <= Z+radius; z2++)
        {
            initPutBlocksLight(x2,Y+radius+1,z2);
        }
    }

    //clever drop
    if(dropIDs.size() > 1)
    {
        std::sort(dropIDs.begin(), dropIDs.end());
        int temp = dropIDs[0];
        int num = 0;
        for(unsigned int i = 0; i < dropIDs.size(); i++)
        {
            if(dropIDs[i] != temp)
            {
                DropThis(temp,num,Vector3(X-1.5f+(rand()%7)*0.5f,Y-radius+1.5f,Z-1.5f+(rand()%7)*0.5));
                num = 0;
            }

            num ++;
            temp = dropIDs[i];

            if(i == dropIDs.size()-1)
            {
                DropThis(temp,num,Vector3(X-1.5f+(rand()%7)*0.5f,Y-radius+1.5f,Z-1.5f+(rand()%7)*0.5));
                break;
            }
        }
    }
    dropIDs.clear();

    if(wasLight)
    {
        UpdateLightAreaIn(Vector3(X,Y,Z));
    }

    for(unsigned int i = 0; i < toUpdate2.size(); i++)
    {
        RebuildFullMeshChunk(toUpdate2[i]);
    }
    toUpdate2.clear();
}

void CraftWorld::BuildBlockSphere(float radius, block_t block, int X, int Y, int Z)
{
    if(spawnGenClip && !InGenBounds(X, Z))
        return;
    float reference = 125 / 20.0f;

    float latitudes = (float)(radius * reference);
    float longitudes = (float)(latitudes/2);

    float latitude_increment = PI*2 / latitudes;
    float longitude_increment = PI / longitudes;

    for (float u = 0; u < PI*2; u += latitude_increment)
    {
        for (float t = 0; t < PI; t += longitude_increment)
        {

            float rad = radius;

            int x = X + (rad * sinf(t) * sinf(u));
            int y = Y + (rad * cosf(t));
            int z = Z + (rad * sinf(t) * cosf(u));

            if(x > X)
            {
                for(int x2 = x; x2 >= X - (x2-X); x2 --)
                {
                    if(GetBlock(x2,y,z) == bedrock::getID() || GetBlock(x2,y,z) == WaterBlock::getID())
                    {
                        continue;
                    }
                    GetBlock(x2,y,z) = block;
                }
            }

            if(x < X)
            {
                for(int x2 = x; x2 <= X + (X-x2); x2 ++)
                {
                    if(GetBlock(x2,y,z) == bedrock::getID() || GetBlock(x2,y,z) == WaterBlock::getID())
                    {
                        continue;
                    }
                    GetBlock(x2,y,z) = block;
                }
            }
        }
    }
}

void CraftWorld::BuildBlockEllipsoid(float w_radius, float h_radius, block_t block, int X, int Y, int Z)
{
    float reference = 125 / 20.0f;

    float latitudes = (float)(w_radius * reference);
    float longitudes = (float)((h_radius * reference) /2.0f);

    float latitude_increment = PI*2 / latitudes;
    float longitude_increment = PI / longitudes;

    for (float u = 0; u < PI*2; u += latitude_increment)
    {
        for (float t = 0; t < PI; t += longitude_increment)
        {

           // float rad = radius;

            int x = X + (w_radius * sinf(t) * sinf(u));
            int y = Y + (h_radius * cosf(t));
            int z = Z + (w_radius * sinf(t) * cosf(u));

            if(x > X)
            {
                for(int x2 = x; x2 >= X - (x2-X); x2 --)
                {
                    if(GetBlock(x2,y,z) == bedrock::getID() || GetBlock(x2,y,z) == WaterBlock::getID())
                    {
                        continue;
                    }
                    GetBlock(x2,y,z) = block;
                }
            }

            if(x < X)
            {
                for(int x2 = x; x2 <= X + (X-x2); x2 ++)
                {
                    if(GetBlock(x2,y,z) == bedrock::getID() || GetBlock(x2,y,z) == WaterBlock::getID())
                    {
                        continue;
                    }
                    GetBlock(x2,y,z) = block;
                }
            }
        }
    }
}

void CraftWorld::BuildBlockSphere(float radius, block_t block, int X, int Y, int Z, block_t blockToChange)
{
    if(spawnGenClip && !InGenBounds(X, Z))
        return;
    float reference = 125 / 20.0f;

    float latitudes = (float)(radius * reference);
    float longitudes = (float)(latitudes/2);

    float latitude_increment = PI*2 / latitudes;
    float longitude_increment = PI / longitudes;

    for (float u = 0; u < PI*2; u += latitude_increment)
    {
        for (float t = 0; t < PI; t += longitude_increment)
        {

            float rad = radius;

            int x = X + (rad * sinf(t) * sinf(u));
            int y = Y + (rad * cosf(t));
            int z = Z + (rad * sinf(t) * cosf(u));

            if(x > X)
            {
                for(int x2 = x; x2 >= X - (x2-X); x2 --)
                {
                    if (GetBlock(x2,y,z) == blockToChange)
                    {
                        if(GetBlock(x2,y,z) == bedrock::getID() || GetBlock(x2,y,z) == WaterBlock::getID())
                        {
                            continue;
                        }
                        GetBlock(x2,y,z) = block;
                    }
                }
            }

            if(x < X)
            {
                for(int x2 = x; x2 <= X + (X-x2); x2 ++)
                {
                    if (GetBlock(x2,y,z) == blockToChange)
                    {
                        if(GetBlock(x2,y,z) == bedrock::getID() || GetBlock(x2,y,z) == WaterBlock::getID())
                        {
                            continue;
                        }
                        GetBlock(x2,y,z) = block;
                    }
                }
            }
        }
    }
}

void CraftWorld::SetPlayerSpawnPosition(Vector3 newPosition)
{
    playerSpawnPointPosition = newPosition;
}

int CraftWorld::FetchArmorPoints(int bootsId, int leggingsId, int chestId, int helmetId)
{
    int armorPoints = 0;
    switch(helmetId)
    {
       case 328 : armorPoints += 1.0f; break;
       case 332 : armorPoints += 2.0f; break;
       case 336 : armorPoints += 2.0f; break;
       case 340 : armorPoints += 3.0f; break;
       case 344 : armorPoints += 2.0f; break;
    }
    switch(chestId)
    {
       case 329 : armorPoints += 3.0f; break;
       case 333 : armorPoints += 5.0f; break;
       case 337 : armorPoints += 6.0f; break;
       case 341 : armorPoints += 8.0f; break;
       case 345 : armorPoints += 5.0f; break;
    }
    switch(leggingsId)
    {
       case 330 : armorPoints += 2.0f; break;
       case 334 : armorPoints += 4.0f; break;
       case 338 : armorPoints += 5.0f; break;
       case 342 : armorPoints += 6.0f; break;
       case 346 : armorPoints += 3.0f; break;
    }
    switch(bootsId)
    {
       case 331 : armorPoints += 1.0f; break;
       case 335 : armorPoints += 1.0f; break;
       case 339 : armorPoints += 2.0f; break;
       case 343 : armorPoints += 3.0f; break;
       case 347 : armorPoints += 1.0f; break;
    }
    return armorPoints;
}

float CraftWorld::FetchDamagePoints(int weaponId, bool &weapon)
{
    int damagePoints = 1.0f;
    switch(weaponId)
    {
        //picks
        case 250 : damagePoints = 4.0f; weapon = true; break;
        case 251 : damagePoints = 5.0f; weapon = true; break;
        case 252 : damagePoints = 6.0f; weapon = true; break;
        case 253 : damagePoints = 7.0f; weapon = true; break;
        case 254 : damagePoints = 4.0f; weapon = true; break;

        //swords
        case 255 : damagePoints = 2.0f; weapon = true; break;
        case 256 : damagePoints = 3.0f; weapon = true; break;
        case 257 : damagePoints = 4.0f; weapon = true; break;
        case 258 : damagePoints = 5.0f; weapon = true; break;
        case 259 : damagePoints = 2.0f; weapon = true; break;

        //shovels
        case 260 : damagePoints = 1.0f; weapon = true; break;
        case 261 : damagePoints = 2.0f; weapon = true; break;
        case 262 : damagePoints = 3.0f; weapon = true; break;
        case 263 : damagePoints = 4.0f; weapon = true; break;
        case 264 : damagePoints = 1.0f; weapon = true; break;

        //axes
        case 265 : damagePoints = 3.0f; weapon = true; break;
        case 266 : damagePoints = 4.0f; weapon = true; break;
        case 267 : damagePoints = 5.0f; weapon = true; break;
        case 268 : damagePoints = 6.0f; weapon = true; break;
        case 269 : damagePoints = 3.0f; weapon = true; break;
    }
    return damagePoints;
}

bool CraftWorld::HaveItemInBarSlots(int id)
{
    for(int j = 27; j <= 35; j++)
    {
        if(invId[j] == id)
        {
            return true;
        }
    }
    return false;
}

bool CraftWorld::HasItemInInventory(int id)
{
    for(int j = 0; j < 36; j++)
    {
        if(invId[j] == id)
            return true;
    }
    return false;
}

// Spends one of `id` out of whichever slot holds it (hotbar first, because that is the order the
// player sees them). Same slot-clearing idiom as every other one-item use in the states: 0 becomes
// id -1, amount -1, stack flag clear.
bool CraftWorld::ConsumeOneItemFromInventory(int id)
{
    for(int pass = 0; pass < 2; pass++)
    {
        int from = (pass == 0) ? 27 : 0;
        int to   = (pass == 0) ? 35 : 26;
        for(int j = from; j <= to; j++)
        {
            if(invId[j] != id)
                continue;
            invAm[j] -= 1;
            if(invAm[j] <= 0)
            {
                invAm[j] = -1;
                invId[j] = -1;
                invSt[j] = 0;
            }
            return true;
        }
    }
    return false;
}

// Flatten a name to its comparable form: separators dropped, letters folded to lower case.
static std::string FlattenItemName(const std::string& src)
{
    std::string out;
    for(size_t i = 0; i < src.size(); i++)
    {
        char c = src[i];
        if(c == ' ' || c == '_' || c == '-')
            continue;
        if(c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        out += c;
    }
    return out;
}

int CraftWorld::GetItemIdByName(const std::string& name)
{
    std::string want = FlattenItemName(name);
    if(want.empty())
        return -1;

    for(size_t i = 0; i < itemTypes.size(); i++)
    {
        if(FlattenItemName(itemTypes[i].name) == want)
            return (int)(i + 250);
    }
    return -1;
}

int CraftWorld::InventoryRoomFor(int id)
{
    bool stacks = true;
    if(id >= 250 && id < 250 + (int)itemTypes.size())
        stacks = (itemTypes[id - 250].stackable == true);

    int cap = 0;
    for(int j = 0; j < 36; j++)
    {
        if(invId[j] == id && invSt[j] == 1)
            cap += 64 - invAm[j];
        else if(invId[j] == -1)
            cap += stacks ? 64 : 1;
    }
    if(cap < 0)
        cap = 0;
    return cap;
}

int CraftWorld::GiveItemToPlayer(int id, int amount)
{
    if(id < 250 || id >= 250 + (int)itemTypes.size())
        return 0;
    if(amount < 1)
        amount = 1;

    const bool stacks = (itemTypes[id - 250].stackable == true);
    if(stacks == false)
        amount = 1;

    // A tool is given one at a time, and the value that has to travel through PutInInventory is its
    // full durability rather than a count of one -- for a tool those are the same number only by
    // coincidence, and passing the literal 1 is what used to make `/give wooden_pickaxe` hand over a
    // pickaxe with a single use. `amount` stays a count so the message above it still reads right.
    const int slotAmount = stacks ? 1 : FreshItemAmount(id);

    // Give only what fits. PutInInventory is void and drops whatever it cannot place, so the room is
    // measured first rather than asked about afterwards -- a full pack should say so, not lie.
    int room = InventoryRoomFor(id);
    if(amount > room)
        amount = room;

    const int chunkSize = stacks ? 64 : 1;

    int given = 0;
    while(given < amount)
    {
        int chunk = amount - given;
        if(chunk > chunkSize)
            chunk = chunkSize;
        PutInInventory(id, stacks ? chunk : slotAmount, stacks);
        given += chunk;
    }
    return given;
}

void CraftWorld::PutInInventory(int id, int num, bool st)
{
    bool ok;
    ok = false;
    for(int j = 27; j <= 35; j++)
    {
        if (invSt[j]==1)
        {
            if (invId[j]== id)
            {
                if(invAm[j] + num <= 64)
                {
                    invAm[j] += num;
                    ok = true;
                    break;
                }
                else
                {
                    continue;
                }
            }
        }

        if (invId[j]== -1)
        {
            invSt[j] = st;
            invId[j] = id;
            invAm[j] = num;
            ok = true;
            break;
        }
    }

    if(ok == false)
    {
        for(int j = 0; j <= 26; j++)
        {
            if (invSt[j]==1)
            {
                if (invId[j]== id)
                {
                    if(invAm[j] + num <= 64)
                    {
                        invAm[j] += num;
                        break;
                        return;
                    }
                    else
                    {
                        continue;
                    }
                }
            }

            if (invId[j]== -1)
            {
                invSt[j] = st;
                invId[j] = id;
                invAm[j] = num;
                break;
                return;
            }
        }
    }
}


void CraftWorld::PutInInventory(int id, int num, bool st, int& slot)
{
    slot = -1; // some animation stuff


    bool ok;
    ok = false;
    for(int j = 27; j <= 35; j++)
    {
        if (invSt[j]==1)
        {
            if (invId[j]== id)
            {
                if(invAm[j] + num <= 64)
                {
                    invAm[j] += num;
                    ok = true;
                    slot = j-27;
                    break;
                }
                else
                {
                    continue;
                }
            }
        }

        if (invId[j]== -1)
        {
            invSt[j] = st;
            invId[j] = id;
            invAm[j] = num;
            ok = true;
            slot = j-27;
            break;
        }
    }

    if(ok == false)
    {
        for(int j = 0; j <= 26; j++)
        {
            if (invSt[j]==1)
            {
                if (invId[j]== id)
                {
                    if(invAm[j] + num <= 64)
                    {
                        invAm[j] += num;
                        break;
                        return;
                    }
                    else
                    {
                        continue;
                    }
                }
            }

            if (invId[j]== -1)
            {
                invSt[j] = st;
                invId[j] = id;
                invAm[j] = num;
                break;
                return;
            }
        }
    }
}

bool CraftWorld::InventoryIsFull(int id, int num, bool st)
{
    bool ok;
    ok = false;
    for(int j = 27; j <= 35; j++)
    {
        if (invSt[j]==1)
        {
            if (invId[j]== id)
            {
                if(invAm[j] + num <= 64)
                {
                    ok = true;
                    return false;
                }
                else
                {
                    continue;
                }
            }
        }

        if (invId[j]== -1)
        {
            ok = true;
            return false;
            break;

        }
    }

    if(ok == false)
    {
        for(int j = 0; j <= 26; j++)
        {
            if (invSt[j]==1)
            {
                if (invId[j]== id)
                {
                    if(invAm[j] + num <= 64)
                    {
                        return false;
                        break;

                    }
                    else
                    {
                        continue;
                    }
                }
            }

            if (invId[j]== -1)
            {
                return false;
                break;
            }
        }
    }
    return true;
}

int CraftWorld::FindDirId(int x, int y, int z)
{
    int o;
    o = -1;
    for(unsigned int i = 0; i < mDirects.size(); i++)
    {
        DirectionBlock* NewDirectionBlock = mDirects[i];
        if(NewDirectionBlock->X == x && NewDirectionBlock->Y == y && NewDirectionBlock->Z == z)
        {
            o = i;
            break;
        }
    }

    return o;
}

void CraftWorld::RebuildVisibleChunks()
{
    for(unsigned int i = 0; i < mChunks.size(); i++)
    {
        if(!MeshIdValid(i))
            continue;
        if(MeshInPlayerZone(mChunks[i]))
        {
            RebuildFullMeshChunk(i);
        }
    }
}

