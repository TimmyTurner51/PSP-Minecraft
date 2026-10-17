#ifndef CraftWorld_H
#define CraftWorld_H

#include <string>
#include <math.h>
#include <psptypes.h>

#include <algorithm>

#include <Aurora/Math/Frustum.h>
#include <Aurora/Math/Vector4.h>
#include <Aurora/Graphics/Effects/Drop.h>
#include <Aurora/Graphics/Effects/TNT.h>
#include <Aurora/Graphics/Effects/WeatherSystem.h>

// Arrow2 is forward-declared, not included. Arrow2.h includes this header (Arrow2::Update and
// CheckCollision take a CraftWorld*), so including it back here leaves Arrow2 undefined at the point
// mArrows is declared below. The .cpp files that construct, step or delete an arrow include Arrow2.h
// themselves, by which point this header is complete.
namespace Aurora
{
	namespace Graphics
	{
		class Arrow2;
	}
}

#include "Blocks.h"
#include "Items.h"
#include "SimpleMeshChunk2.h"
#include "ChunkColumn.h"
#include "Zombie2.h"
#include "Cow2.h"
#include "Pig2.h"
#include "Creeper2.h"
#include "Sheep2.h"
#include "Chicken2.h"
#include "Spider2.h"
#include "Skeleton2.h"
#include "LameMob.h"
#include "Furnace2.h"
#include "Chest2.h"
#include "DirectionBlock.h"
#include "NoteBlockEntity.h"
#include "JukeboxEntity.h"
#include "MonsterSpawnerEntity.h"
#include "ItemFrameEntity.h"

#include "LameFunctions.h"
#include <sys/time.h>
#include <Aurora/Utils/Logger.h>
#include <Aurora/Utils/Timer.h>

#include <fstream>
#include <ostream>

class Cow;
class Zombie;
class Creeper;
class Pig;
class Sheep;
class Chicken;
class Spider;
class LameMob;

class Drop;
class TNT;
class ItemFrameEntity;
class WeatherSystem;
class WorldGenerator;

using namespace std;
using namespace Aurora::Utils;
using namespace Aurora::System;
using namespace Aurora;

class CraftWorld
{
public:

	double TestWriteChunk(float dt);

	int WORLD_HEIGHT;

	enum BlockSettings
	{
		 OpRedstonePowerBit = 1,
		 OpRedstonePowerMask = 15,
		 OpFirstShadowBit = 16,
		 OpSecondShadowBit = 32,
		 OpThirdShadowBit = 64,
		 OpFourthShadowBit = 128
	};

	CraftWorld();
	~CraftWorld();

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
        // Where the terrain atlas crosses from full resolution to the half-size mip atlas. 0 is the
        // GE's own choice; positive pushes toward the smaller mip (cheaper, softer at distance) and
        // negative keeps full resolution further out. Exposed because the right value is a visual
        // judgement on the device, not something this side can pick -- the atlas binding in
        // TextureManager::SetMipMaps2Textures passes it straight to sceGuTexLevelMode.
        float mipMapBias;
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

    typedef struct WorldVariables
    {
        int worldDiamondOre;
        int worldIronOre;
        int worldCoalOre;
        int worldGoldenOre;
        int worldRedstoneOre;
        int worldLapisOre;
        int worldTrees;
        int worldFlowers;
        int worldClay;
    } wvars;

    WeatherSystem* worldWeather;

    float updateChunkTimer;
    float updateTransparentChunkTimer2;
    float updateChunkTimer2;
    float updateTransparentChunkTimer;
    float updateFastChunkTimer;

    float fetchNearestChunksTimer;

    bool chunksWasSetToUpdate;

    st mainStatistics;
    opt mainOptions;
    wvars mainWorldVariables;

    char gameModeWorld;
    bool gameLocked;

    void UpdateBlocksAndItemsName();
    void InitBlockVector();
    void InitItemVector();

    block_t _GetBlock(const int x, const int y, const int z);
    block_t _GetBlockNoCheck(const int x, const int y, const int z);

	block_t& GetBlock (const int x, const int y, const int z) ;
	block_t& GetBlockNoCheck (const int x, const int y, const int z) ;
	block_settings_t& GetBlockSettings (const int x, const int y, const int z);
	block_settings_t& GetBlockSettingsNoCheck (const int x, const int y, const int z);
	void AddBlockType(block_t id, const BaseBlock& block);

	int GetShadowLevel (const int x, const int y, const int z);
	int GetShadowLevelNoCheck (const int x, const int y, const int z);
	void SetShadowLevel (const int x, const int y, const int z, int level);
	void SetShadowLevelNoCheck (const int x, const int y, const int z, int level);

	int GetLightLevel(const int x, const int y, const int z);
	int GetLightLevelNoCheck(const int x, const int y, const int z);
	void SetLightLevel(const int x, const int y, const int z, unsigned int level);
	// false when the cell cannot hold light: no resident column, above the column's height, or the
	// nibble belongs to a block's state. Callers that walk light must treat that as a wall.
	bool SetLightLevelNoCheck(const int x, const int y, const int z, unsigned int level);

	block_t BlockAtPoint(Vector3 pos);

    void BuildWorldBlockPlane(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4);
    void BuildWorldBlockPlane(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4, bool alwaysBuild, bool constantColor);
    void BuildWorldBlockPlane(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4, bool alwaysBuild, bool constantColor, int texturePlane);
    void BuildWorldBlockPlane(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4, bool alwaysBuild, bool constantColor, int texturePlane, int textureRotation);
    void BuildWorldBlockPlaneNoCheck(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4);
    void BuildGUIBlockPlane(BaseBlock* blockType, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4);

    float LengthDirX(float horAngle, float verAngle);
    float LengthDirY(float horAngle, float verAngle);
    float LengthDirZ(float horAngle, float verAngle);

    float TaxicabDistance2d(float x1, float y1, float x2, float y2);
    unsigned int FastDistance2d(unsigned int dx, unsigned int dy);
    float Distance2d(float x1, float y1, float x2, float y2);
    float Distance3d(float x1, float y1, float z1, float x2, float y2, float z2);
	//inline void BuildWorldBlockPlane(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, std::string side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4);

	//map creation
	void initWorld(int worldSize, int worldHeight, int chunkSize);
	void initWorldBlocksLight();

	void UpdateBlocksLightNearby(const int xx, const int yy, const int zz);
    void CalculateBlocksLightAtPos(const int xx, const int yy, const int zz, int iteration);

	void initPutBlocksLight(const int xx, const int zz);
	void initPutBlocksLight(const int xx, const int yy, const int zz);

	void GetSpecialBlockVerts(int i,BaseBlock *blockType);
	void GetNormalBlockVerts(int i,BaseBlock *blockType);
	void GetItemVerts(int i,BaseItem *itemType);
	void GetCloudsVerts(BaseBlock *blockType);

	void buildblocksVerts();
	// One place owns the per-block meshes: the world, not BaseBlock. Call before a rebuild and before
	// the vector is emptied. See the definition in CraftWorld2.cpp for why.
	void FreeBlockTypeMeshes();
	void FreeBlockTypeMesh(int id);
	// Same contract for the item meshes (four buffers per item, allocated by GetItemVerts).
	void FreeItemTypeMeshes();
	void FreeItemTypeMesh(int id);
	void buildcloudsVerts();
	void buildskyVerts();
	void buildmobVerts();
	void buildCowVerts();
	void buildZombieVerts();
	void buildArmorVerts();

    void setTextureSize(int texture,int chunk,int animTexture, int animChunk, int itemTexture, int itemChunk);

    void PutInInventory(int id, int num, bool st);
    void PutInInventory(int id, int num, bool st, int &slot);
    bool InventoryIsFull(int id, int num, bool st);

	int CreateFullMeshChunk(const int StartX, const int StartY, const int StartZ, bool buildNow = true); // returns mesh id

	void RebuildFullMeshChunk(int id);
	void RebuildOpaqueMeshChunk(int id);
	void RebuildTransparentMeshChunk(int id);

	// Development-only measurement hook (MC_PERF_PROBE, see DebugConfig.h): times the per-section light
	// cache fill on its own, with no meshing around it, so the fixed cost of a remesh can be told apart
	// from the cost of the geometry in it. Empty in every other build, and present in both so the class
	// layout does not move.
	void PerfProbeMeshCache(int id, int times);

	// Redstone's own mesh queue. A clock rewrites redstone geometry many times a second and each rebuild
	// is a whole 16x16x16 chunk walk, so these go through a queue drained once per frame under the same
	// one-rebuild-per-frame rule the rest of the engine follows -- see QueueRedstoneMesh in the .cpp.
	// `opaque` is false for the components whose flip changes only the transparent mesh (a torch, a
	// repeater, wire); the opaque pass cannot have changed and walking it is pure waste.
	void QueueRedstoneMesh(int id, bool opaque);
	void QueueRedstoneMesh(int x, int y, int z, bool opaque);
	void FlushRedstoneMeshes();

	void rebuildNearestChunks(int id,Vector3 pos);
	void rebuildNearestTransparentChunks(int id, Vector3 pos);
	// The offset from a cell to each face-neighbour of it that lies in a different section, ±1 per axis
	// with 0 where the neighbour is in the same one. Replaces reading this off the section's box, which
	// could not name the +X/+Y/+Z side at all -- see the definition.
	Vector3 SectionBorderOffset(int id, int x, int y, int z);

	void rebuildNearestChunksForLight(int id,Vector3 pos);
	// Sections skipped as fully buried (meshId -2) inside this block box get meshed again
	void WakeBuriedSections(int minX, int minY, int minZ, int maxX, int maxY, int maxZ);
	void WakeBuriedAround(int x, int y, int z);
	void createWorldChunkss();
	void SetAllChunksToUpdate();
    int getChunkId(Vector3 pos);
    bool ChunksEqual(Vector3 pos1, Vector3 pos2);

    ChunkColumn* FindColumn(int cx, int cz);
    // GetBlock's fast path. Generation writes thousands of blocks in one column; pointing this at
    // that column keeps each of them out of the column hash.
    void PinLastColumn(ChunkColumn* col);
    ChunkColumn* EnsureColumn(int cx, int cz, bool generate);
    void UpdateStreaming();
    void SetWorldSavePath(std::string path);
    void SaveLoadedColumns();
    void UnloadDistantColumns(int avoidRing = -1, int keepCount = -1);
    // Frees every resident column. UnloadDistantColumns cannot do this: it gives back one column per
    // call by design (memory-pressure pacing) and its ring is absolute, so it never releases the column
    // the player stands in. Dimension travel needs all of them gone at once.
    void UnloadAllColumns();
    // Portal travel, driven once per frame from the play states. Counts the player's time inside a
    // portal, wobbles the camera, and performs the swap when the timer expires.
    void UpdatePortalTravel(float dt);
    void TravelToDimension(int newDim);
    bool PlayerInPortal();
    // The portal cell the player occupies, if any. Travel derives its destination from this rather than
    // from the exact feet position, so entering a wide portal from either end maps to the same place.
    bool PlayerPortalCell(int &outX, int &outY, int &outZ);
    // Dismantles the portal an obsidian break has just invalidated. See the note on the definition for
    // the 3x3x3 it scans and why a corner break needs it.
    void BreakPortalNear(int x, int y, int z);
    // Nearest portal block to (wx, wz) among already-loaded columns, used to find the frame waiting at a
    // scaled destination. Does not generate; the caller ensures the ring first.
    bool FindPortalNear(int wx, int wz, int &outX, int &outY, int &outZ);
    float GetPortalWobbleAngle() const;
    // Overlay alpha for the purple wash, already scaled to its ceiling.
    float GetPortalOverlayAlpha() const;
    // Builds and lights a minimal 4x5 obsidian frame standing on (x, y-1, z), used for arrival.
    bool BuildPortalFrameAt(int x, int y, int z);
    int StreamRadius();
    /// Whether (cx,cz) is inside the visible ring, i.e. a column the streamer's own ring covers. A block
    /// read uses this to decide whether it may create the column on demand; outside it, a missing column
    /// reads as air. See the note in GetBlock.
    bool ColumnWithinStreamRing(int cx, int cz);
    // The view distance the world actually streams and meshes at. The nether gets one chunk less than
    // the player asked for -- see the definition for the measurement behind it.
    int EffectiveViewDistance() const;
    bool ColumnLoadedAt(int x, int z);
    bool lowMemoryMode;
    static int HeapFreeKB();
    static int HeapUsedKB();
    void MarkAllColumnsGenerated();
    // Deletes Save/<name>d/ (per-column chunk files) for a world file path.
    static void RemoveChunkDir(const std::string& lmsPath);
    int worldLimit;
    // Random per-world id stored in the .lmsc and in every column file, so chunk files
    // left behind by an older world with the same save number are never loaded.
    int worldStamp;
    int worldType;
    // The nether world type, as the generator numbers it (FlatNETHER). It lives here because a
    // loaded world has to recover its dimension from its saved type, and a bare 8 at that call site
    // would be a magic number with nothing to check it against.
    static const int kWorldTypeNether = 8;
    // Which world this CraftWorld is: 0 the overworld, 1 the Nether, 2 the End. Only one dimension
    // is ever resident -- the console cannot hold two (see the streaming budget in CraftWorld2.cpp),
    // so travel between them unloads one and loads the other rather than running both. Each
    // dimension's columns are filed separately, so neither side is lost across the swap.
    int dimension;
    // What each dimension needs to be returned to exactly as it was left: where the player stood, the
    // clock, and the portal that arrival built so the return trip maps back to it. Indexed by
    // dimension, so slot 0 is the overworld and slot 1 the Nether. Only one dimension is resident at a
    // time, which is why this has to be remembered rather than read back from the live world.
    struct DimensionState
    {
        bool valid;
        Vector3 playerPos;
        float dayTime;
        Vector3 portalPos;
    };
    DimensionState dimState[3];
    // Portal travel. portalTimer counts up while the player stands in a portal and is what actually
    // fires the swap; portalCooldown is the pause after arriving, without which the arrival portal
    // would send the player straight back; portalWobble is the phase behind the camera roll.
    float portalTimer;
    float portalCooldown;
    float portalWobble;
    // 0..1 purple wash over the screen while the player is being pulled through, paired with the wobble
    // so the two read as one effect. Rises while standing in a portal and decays after the swap, so the
    // colour does not snap off at the moment the world changes.
    float portalFade;
    // Set on arrival and cleared the first time the player has stood outside a portal since. An arrival
    // that lands inside the destination frame -- which is where the frame is, so that is where the
    // player goes -- would otherwise send them straight back the instant the cooldown expired.
    bool portalLeaveFirst;
    // Set by TravelToDimension and cleared by the play state once it has moved its own copy of the
    // player, so physics and the camera follow the swap in the same frame rather than a frame behind.
    bool travelledThisFrame;
    int waterLevel;
    bool makeTrees;
    bool makeWater;
    bool makeCaves;
    WorldGenerator* worldGen;
    std::string worldSavePath;
    bool worldGenerating;
    bool spawnGenClip;
    int genMinX;
    int genMaxX;
    int genMinZ;
    int genMaxZ;
    void SetupSpawnGeneration();
    bool InGenBounds(int x, int z) const;
    bool SectionHasExposedFaces(int startX, int startY, int startZ);
    bool SectionNeedsSimulation(int startX, int startY, int startZ);
    bool SectionNeedsPhysics(int startX, int startY, int startZ);
    unsigned int HashSectionBlocks(int startX, int startY, int startZ);
    void UpdateBlockPhysicsBox(int StartX, int StartY, int StartZ);

	void RebuildVisibleChunks();

	void GetSpecialBlock(int x,int y, int z,int &iVertex,SimpleMeshChunk* MeshChunk,block_t Block,bool transparent);
	void GetNormalBlock(int x,int y, int z,int &iVertex,SimpleMeshChunk* MeshChunk,block_t Block,bool transparent);

	//load/save options
	void SaveCompressedWorld(std::string filename);
	void LoadWorld(const char *filename);
	void LoadCompressedWorld(std::string filename);

	//lightening
	void InitLightValues();
	void SetWolrdTime(float time);
	void LightFlood(int x,int y,int z,int lightLevel);

	void buildMap();
	void destroyMap();

	//rendering
	void FetchNearestChunks();
	void drawWorld(Frustum &camFrustum,bool camUpdate);
	void drawCubes(int i, float light);
	void drawHudCubes(int i);
	void drawWhiteCube(int i, float light);
	void drawHand();
	void drawClouds();
	void drawDropItems(int i);
	void drawItems(int i);
	void drawHudItems(int i);
	void drawFull3DItem(int i,float light);
	// The bow at a given draw stage: 0 at rest, 1..3 the pulling frames. A wrapper over
	// drawFull3DItem so the three callers that hold a bow -- the skeleton and the player in both
	// views -- do not each have to know the frame ids.
	void drawBowInHand(int stage,float light);
	void drawHandItems(int i, float light);
	void drawArmor(int i, float light);
	void drawShoulder(int chestplateId, float light);
	void drawBelly(int leggingsId, float light);

	//pickin,collision etc
	int groundHeightFromBottom (const int x, const int y, const int z);
	int groundHeight (const int x, const int z);
	int groundHeight (const int x, const int y, const int z);
	int groundHeightWater (const int x, const int z);
	int groundHeightExcept (const int x, const int z, int bl);
	int BlockSoundAtPos(Vector3 pos);

    bool SolidAtPointForPlayerHead(Vector3 pos);
	bool SolidAtPointForPlayer(Vector3 pos);
	bool SolidAtPointForMovingPlayer(Vector3 pos);
	bool SolidAtPointForPlayerOnGround(Vector3 pos);
	bool SolidAtPointForEntity(Vector3 pos, Vector3 collider1);
	bool SolidAtPoint(Vector3 pos);
	bool HighStairAtPoint(Vector3 pos);

	bool PlayerOnHalfBlock(Vector3 pos);
	bool PlayerOnStairsBlock(Vector3 pos);
	bool PlayerOnHighStair(Vector3 pos);

	bool PlayerInWater(Vector3 pos);
	bool PlayerInLava(Vector3 pos);
	bool PlayerInBlock(Vector3 pos);
	bool PlayerCollisionWithLadder(Vector3 pos);
	bool PlayerCollisionWithCactus(Vector3 pos);

	bool BlockTransparentOrSpecial(const int x, const int y, const int z);
	bool BlockTransparentOrLightSource(const int x, const int y, const int z);
	bool BlockTransparentOrLightSourceNoCheck(const int x, const int y, const int z);
	bool BlockTransparent(const int x, const int y, const int z);
	bool BlockTransparentNoCheck(const int x, const int y, const int z);
    bool BlockAnimated(const int x, const int y, const int z);
    // Lights a nether portal if the given cell sits inside a valid obsidian frame, filling the interior.
    // Returns false when there is no frame, so the flint and steel caller can light an ordinary fire.
    bool TryLightPortal(int x, int y, int z);
	char BlockSpecial(const int x, const int y, const int z);
	char BlockSpecialNoCheck(const int x, const int y, const int z);
	bool BlockEditable(const int x, const int y, const int z);
	bool BlockAllowLight(const int x, const int y, const int z);
	float BlockFinalLight(int x, int y, int z);
	float BlockFinalLightNoCheck(int x, int y, int z);
	char BlockRefraction(const int x, const int y, const int z);
	short BlockLoot(const int x, const int y, const int z);
	short BlockMaterial(const int x, const int y, const int z);
	bool BlockUpdate2(const int x, const int y, const int z);
	bool BlockSolid(const int x, const int y, const int z);

    int LootBlock(int id);
	bool LightSourceBlock(int id);
	int DurabilityPointsItem(int id);
	// The amount a slot should hold for a brand-new single instance of an item: its full durability
	// for a tool, 1 for anything else. See the definition for why creation paths need this.
	int FreshItemAmount(int id);
	bool StackableItem(int id);
	string NameBlock(int id);

	bool ItemHaveTerrainTexture(int id);
	char ItemType(int id);

	bool CheckForTorchSupport(const int x, const int y, const int z,int blockID);
	bool CheckForLadderSupport(const int x, const int y, const int z);
	/// Torches. The plain family is Torch (floor) plus WallTorch1-4; the redstone family is a lit
	/// RedstoneTorch / RedstoneWallTorch1-4 and an unlit counterpart for each. Both families keep their
	/// mounting face in the block id, because the settings nibble is block light. Faces:
	/// 0 = mounted on the block below, 1 = -X, 2 = +X, 3 = -Z, 4 = +Z.
	static bool IsRedstoneTorchBlock(block_t id)
	{
		return id >= RedstoneTorch::getID() && id <= UnlitRedstoneWallTorch4::getID();
	}
	static bool IsRedstoneTorchLit(block_t id)
	{
		return id >= RedstoneTorch::getID() && id <= RedstoneWallTorch4::getID();
	}
	/// Lit and unlit ids for the same face are exactly five apart, so switching a torch is an id add.
	static block_t RedstoneTorchToggle(block_t id)
	{
		return IsRedstoneTorchLit(id) ? (block_t)(id + 5) : (block_t)(id - 5);
	}
	static block_t RedstoneTorchFromFace(int face, bool lit)
	{
		if(face < 1 || face > 4) return lit ? RedstoneTorch::getID() : UnlitRedstoneTorch::getID();
		return (block_t)((lit ? RedstoneTorch::getID() : UnlitRedstoneTorch::getID()) + face);
	}
	static block_t TorchBlockFromFace(int face)
	{
		switch(face)
		{
			case 1: return WallTorch1::getID();
			case 2: return WallTorch2::getID();
			case 3: return WallTorch3::getID();
			case 4: return WallTorch4::getID();
			default: return Torch::getID();
		}
	}
	static bool IsTorchBlock(block_t id)
	{
		return id == Torch::getID() || (id >= WallTorch1::getID() && id <= WallTorch4::getID())
			|| IsRedstoneTorchBlock(id);
	}
	static int TorchFaceFromBlock(block_t id)
	{
		if(id == WallTorch1::getID()) return 1;
		if(id == WallTorch2::getID()) return 2;
		if(id == WallTorch3::getID()) return 3;
		if(id == WallTorch4::getID()) return 4;
		if(IsRedstoneTorchBlock(id))
		{
			// Both ranges are floor, -X, +X, -Z, +Z in id order, so the offset from the range base is
			// already the face.
			int face = IsRedstoneTorchLit(id) ? (int)(id - RedstoneTorch::getID())
			                                 : (int)(id - UnlitRedstoneTorch::getID());
			return (face >= 0 && face <= 4) ? face : 0;
		}
		return 0;
	}
	/// Offset from a torch cell to the block it hangs on. The inverter reads that block's power, and a
	/// torch whose attachment is gone comes off as an item, the rule the plain torches already follow.
	static void RedstoneTorchMountOffset(int face, int& dx, int& dy, int& dz)
	{
		dx = 0; dy = 0; dz = 0;
		switch(face)
		{
			case 1: dx = -1; break;
			case 2: dx =  1; break;
			case 3: dz = -1; break;
			case 4: dz =  1; break;
			default: dy = -1; break;
		}
	}
	/// Java BlockRedstoneTorch.isProvidingWeakPower: a lit torch answers 15 in every direction except
	/// the one its attachment sits in -- a standing torch does not power the block it stands on, and a
	/// wall torch does not power the block it hangs on. That single exclusion is the whole difference
	/// between a torch and a block of redstone, and it is why a torch can be turned off by the very
	/// block it feeds. Java's `side` argument is the direction from the queried cell *to* the torch, so
	/// `dx/dy/dz` here follow that same convention.
	static bool RedstoneTorchPowersTowards(int face, int dx, int dy, int dz)
	{
		switch(face)
		{
			case 1: return !(dx ==  1 && dy == 0 && dz ==  0);
			case 2: return !(dx == -1 && dy == 0 && dz ==  0);
			case 3: return !(dx ==  0 && dy == 0 && dz ==  1);
			case 4: return !(dx ==  0 && dy == 0 && dz == -1);
			default: return !(dx == 0 && dy == 1 && dz == 0);
		}
	}
	bool TorchSupportSolid(const int x, const int y, const int z, block_t torchId);

	// --- Redstone torch (Java BlockRedstoneTorch) ---------------------------------------------------
	/// Weak power a lit redstone torch pushes into this cell, 0 when none does. Any of the six
	/// neighbours may be the torch, so this scans them and only counts one whose blind direction does
	/// not contain this cell.
	int RedstoneTorchPowerAt(int x, int y, int z);
	/// Java BlockRedstoneTorch.func_150110_m: the attachment block is powered by something other than us.
	bool RedstoneTorchMountPowered(int x, int y, int z);
	/// Arm a torch's re-check. Java tickRate(World) == 2, i.e. 0.1 s, so a torch never reacts instantly
	/// and feedback loops settle instead of spinning.
	void ScheduleRedstoneTorch(int x, int y, int z);
	/// Java BlockRedstoneTorch.updateTick: dark while the attachment is powered, lit while it is not.
	void EvaluateRedstoneTorch(int x, int y, int z);
	/// Flip one torch, keeping its facing. Relights the cell, refreshes the wire around it and rebuilds
	/// the meshes that show it.
	void SetRedstoneTorchLit(int x, int y, int z, bool lit);


	bool DestroyAroundTrapdoors(const int x, const int y, const int z);
	bool DestroyAroundItemFrames(const int x, const int y, const int z);

	bool CanPutBlockHere(const int x, const int y, const int z,int blockID);
	void SetLigtSourcePosition(const int x, const int y, const int z,int blockID);
	void RemoveLigtSourceAtPosition(const int x, const int y, const int z,int blockID);
	void AddChunkToUpdate(const int x, const int y, const int z);
    void AddChunkToUpdate(int id);
    void AddChunkToFastUpdate(int id);
    void AddNearestChunk(int id);
	void UpdateLightAreaIn(Vector3 pos);
	void QueueLightAreaIn(Vector3 pos);
	void ProcessQueuedLightArea();

	void UpdateFlood(Vector3 pos);
	void AddChunkToFloodUpdate(int id);
	void AddToFloodPos(Vector3 pos);

	void SpawnZombie(float xx, float yy, float zz);
	void DestroyZombie(int id);
	void DestroyAllZombies();

	void SpawnCreeper(float xx, float yy, float zz);
	void SpawnCreeper(float xx, float yy, float zz, float freezedTimerMax);
	void DestroyCreeper(int id);
	void DestroyAllCreepers();

	void SpawnPig(float xx, float yy, float zz);
	void SpawnPig(float xx, float yy, float zz, float freezedTimerMax);
	void DestroyPig(int id);
	void DestroyAllPigs();

	void SpawnChicken(float xx, float yy, float zz);
	void SpawnChicken(float xx, float yy, float zz, float freezedTimerMax);
	void DestroyChicken(int id);
	void DestroyAllChickens();

	void SpawnSpider(float xx, float yy, float zz);
	void SpawnSpider(float xx, float yy, float zz, float freezedTimerMax);
	void DestroySpider(int id);
	void DestroyAllSpiders();

	void SpawnSkeleton(float xx, float yy, float zz);
	void SpawnSkeleton(float xx, float yy, float zz, float freezedTimerMax);
	void DestroySkeleton(int id);
	void DestroyAllSkeletons();

	// How many landed arrows the world will hold before the oldest is evicted. Nothing despawns a
	// landed arrow by time any more, so this is the only thing standing between a long session next to
	// a skeleton and an unbounded list: every entry is a heap allocation plus a twelve-vertex buffer,
	// and every one is a draw call per frame whether or not it is on screen.
	//
	// 64 rather than a tighter number because the arrows the player actually cares about are the ones
	// near them, and eviction takes the oldest landed shot in the list -- which is a mob's, from three
	// minutes ago, almost every time. A skeleton's own shots are never collectable, so they are
	// exactly the ones worth throwing away first.
	//
	// A member constant rather than a local, because three places need the same number and three
	// copies of it are three chances for them to drift: the eviction itself (UpdateArrows), the save
	// (which writes only landed arrows) and the load (which refuses a count larger than the world is
	// allowed to hold, so a corrupted file cannot make it allocate an unbounded list).
	static const int kMaxLandedArrows = 64;

	// Removes one arrow from mArrows. Index-based, like the Destroy* mob helpers, because the callers
	// that still own an index -- the fleet-wide sweep in UpdateArrows and the eviction of the oldest
	// landed shot -- walk the list by position. A shot that *hits* something does not always end up
	// here any more: a block hit lands the arrow instead (see Arrow2::Stick), and only a shot that
	// hits a mob or the player, expires in flight or is collected is destroyed outright.
	void DestroyArrow(int id);

	// One tick of every arrow in the world: fly the ones in the air, resolve what they hit, land the
	// ones that hit a block, collect the player's own landed ones underfoot, and keep both lists
	// bounded. This is the only place a shot's consequences are decided, so the survival and creative
	// play states cannot drift apart on it -- they both just call this and then draw what comes out.
	//
	// Must be called with the world running. Both states gate it on WorldRunning(), the same way they
	// gate the mob updates, so a paused world does not resolve a hit either.
	void UpdateArrows(float dt);

	void DestroyItemFrameEntity(Vector3 position);

    // Picks a spot 24..48 blocks from the player in a loaded column with solid natural
    // ground and a clear 3x3x2 space above it. Returns false if none found.
    bool FindMobSpawnSpot(int& outX, int& outY, int& outZ, bool needDark, int tries);
    bool MobSpawnSpotOk(int x, int y, int z, bool needDark);
    void SpawnStartAnimals(int cowPairs, int sheepPairs);

    // Mob caps by console: PSP-1000 (32 MB) vs PSP-2000/3000/Go (64 MB).
    // Natural spawns stop well below the hard cap so breeding/eggs have room.
    bool bigMemory;
    int PassiveMobCap();
    int HostileMobCap();
    int NaturalPassiveCap();
    // An animal past the loaded ring is stored here instead of kept as a live mob or deleted.
    // It is not ticked. Coming back within range turns the record into a mob again. 48 is the
    // whole remembered herd; a farther one is dropped when a closer one needs the slot.
    struct ParkedPassive
    {
        unsigned char kind;
        unsigned char sheared;
        unsigned char color;
        unsigned char pad;
        float x, y, z, angle, hp, age;
    };
    static const int kParkedPassiveMax = 48;
    ParkedPassive parkedPassive[kParkedPassiveMax];
    int parkedPassiveCount;
    float passiveHerdTimer;
    void MaintainPassiveHerd(float dt);
    void TryAnimalPack(int cx, int cz);
    void StoreParkedPassive(int kind, LameMob* mob);
    int PassiveMobCount();
    int HostileMobCount();
    bool CanSpawnPassive();
    bool CanSpawnHostile();

    void SpawnCow(float xx, float yy, float zz);
	void DestroyCow(int id);

	void SpawnSheep(float xx, float yy, float zz);
	void DestroySheep(int id);

    bool IsSpawnEgg(int itemId);
    bool SpawnFromEgg(int itemId, float x, float y, float z);
    // cow / sheep / pig / chicken / zombie / creeper / spider / skeleton / villager.
    // Returns the new mob, or NULL if the name is unknown. Does not pose-lock; the caller does.
    LameMob* SpawnMobByName(const char* name, float x, float y, float z);

    void CollisionWithOtherZombies(Zombie *Zombie1);

    void SpawnTNTentity(float xx, float yy, float zz);
    void SpawnTNTentity(float xx, float yy, float zz, float time);
    int GetRedstoneDustPower(int x, int y, int z);
    void UpdateRedstonePower(int x, int y, int z);
    // Java: only conductive blocks can be powered. TNT is a mechanism (and dust a wire), not a conductor.
    bool IsRedstoneConductive(block_t b);
    // Power a conductive block receives from dust sitting on it / pointing into it, or from an adjacent
    // block of redstone, switch, repeater or torch (Java: "is this block powered?"). Only a conductive
    // block can be powered, and a powered block soft powers the mechanisms around it.
    int GetBlockRedstonePower(int x, int y, int z);
    // The raw union of what the sources touching a cell provide to it, before the question of what kind
    // of block the cell is. Split out so GetRedstonePowerTo can ask it about a cell that is *not*
    // conductive -- TNT, the main caller, is a mechanism and explicitly not a conductor.
    int GetPowerProvidedToCell(int x, int y, int z);
    // Java World.isBlockIndirectlyGettingPowered: the question a *mechanism* asks -- "am I activated?"
    // The sources touching this cell, plus one step of conduction through a neighbouring powered block
    // (Java's soft power). That one step is why a lever on the middle of three lamps lights all three.
    int GetRedstonePowerTo(int x, int y, int z);
    // Turn a TNT block into a primed entity. Redstone dust resting on it loses its support and drops.
    bool PrimeTNTBlock(int x, int y, int z);
    // Prime the TNT at this cell if it is receiving a redstone signal from any side.
    bool TryIgniteTNT(int x, int y, int z);

    // --- Redstone lamp (Java BlockRedstoneLight) --------------------------------------------------
    // The lit state is a block id, like the furnace's and the repeater's, so "is this cell a light
    // source" and the mesher's choice of tile are both plain id comparisons. A lamp keeps no state
    // nibble, so it is NOT a state cell.
    static bool IsRedstoneLampBlock(block_t b);
    // Re-read this lamp's power and flip it if it disagrees. Called from the redstone event window for
    // every lamp inside it; a lamp has no event of its own to react to.
    void EvaluateRedstoneLamp(int x, int y, int z);
    void SetRedstoneLampLit(int x, int y, int z, bool lit);

    // --- Button (Java BlockButton) ---------------------------------------------------------------
    // Facing (bits 0-2: 0 = mounted on the block below, 1 = above, then -X, +X, -Z, +Z) and the pressed
    // flag (bit 3) live in the 4-bit block-state nibble, which InitColumnLighting preserves. Storing
    // it per block instead of per id is what keeps one id per material covering all six mountings.
    struct PressedButton
    {
        int x, y, z;
        float timeLeft;
    };
    std::vector<PressedButton> mPressedButtons;

    static bool IsButtonBlock(block_t b);
    // A lever: the same state nibble and the same wiring as a button, but a latch instead of a timer.
    static bool IsLever(block_t b);
    // True for anything holding "on/off, pointing this way" in the state nibble -- buttons and levers.
    // Every *reader* of a switch asks this; IsButtonBlock stays buttons-only for the press-timer paths.
    static bool IsSwitchBlock(block_t b);
    // True for any block whose settings nibble is state (facing, delay, pressed) rather than light.
    static bool IsStateCellBlock(block_t b);
    // Zero the state nibble of a cell that is no longer a state cell, so it cannot be misread as light.
    void ClearStateCellNibble(int x, int y, int z);
    static int ButtonFacingFromState(int state);
    static bool ButtonPressedFromState(int state);
    int GetButtonState(int x, int y, int z);
    void SetButtonState(int x, int y, int z, int state);
    // The block a button or lever is mounted on, derived from its facing (the two share the encoding).
    void ButtonSupportPos(int x, int y, int z, int& sx, int& sy, int& sz);
    // Button box in its own cell: Java BlockButton.setBlockBoundsBasedOnState, centred on the cell
    // (a full block spans -0.5..0.5). The mesher, the pick ray and the selection outline all read
    // this one box, so the model and the hitbox cannot drift apart.
    static BoundingBox ButtonCellBox(int state);
    // The same for a lever: Java BlockLever.setBlockBoundsBasedOnState, an 8x8 px footprint six pixels
    // out of the mounting face -- deliberately taller than the base the mesher draws, so it encloses the
    // leaning handle, as vanilla's own box does.
    static BoundingBox LeverCellBox(int state);
    // The same for a torch: Java BlockTorch.setBlockBoundsBasedOnState. A standing torch is a centred
    // 0.2 x 0.6 x 0.2 column off the floor, a wall torch a 0.3 x 0.6 x 0.3 box at mid-height against the
    // face its id names, so the pick shape has to come from the id rather than from one shared
    // `collideBox`. face is 0 floor, 1 -X, 2 +X, 3 -Z, 4 +Z, the encoding TorchFaceFromBlock already uses.
    static BoundingBox TorchCellBox(int face);
    // Pick/collide box of the block in a cell. Most blocks are just their per-type box; the ones that
    // are not are the blocks whose shape depends on cell state rather than on the id alone -- the two
    // switches (facing plus pressed), the piston base (facing plus extended) and the torches (mounting).
    BoundingBox GetBlockCollideBox(int x, int y, int z, block_t b);
    // Shape the crosshair sees (Java's "selection box"), which is the same box as above except for
    // redstone dust: Java's BlockRedstoneWire is 1 px tall, so the selection shape is a floor plate
    // that the look can pass *over* when the player is aiming at the next cell along a line they are
    // building, and this build keeps the 2 px floor plate in `collideBox` as the collision shape.
    // Every ray that answers "which block is the player looking at" goes through here so the pick ray,
    // the break ray and the placement ray can never disagree about a wire.
    BoundingBox GetBlockSelectionBox(int x, int y, int z, block_t b);
    // How far along a look to travel before it leaves the cell it is in at `t`, plus a hair so the next
    // sample lands inside the next cell. This is the step a pick loop takes, so that it visits *every*
    // cell the ray crosses; a fixed step cannot promise that, and the shape it costs is exactly the thin
    // one. Redstone wire's selection box is 1 px tall, a look steeper than about 56 degrees crosses it
    // in less than the 0.075 step the loops used to take, and even a stride of half the plate's height
    // still lets a cell be entered and left between two samples where the look clips the corner between
    // the plate and the cell below it. See tools/check-wire-aim.py, which walks the same geometry the
    // other ways and counts what each one misses.
    static float NextCellDistance(const Vector3 &origin, const Vector3 &dir, float t);
    // 15 when a pressed button is mounted on this block: a button strongly powers its support.
    int GetButtonPowerAt(int x, int y, int z);
    void PressButton(int x, int y, int z);
    void ReleaseButton(int x, int y, int z);
    // Right-click action on a lever: flip the latch. Shares the state nibble, the power rules and the
    // click with a button; it just has no timer.
    void UseLever(int x, int y, int z);
    // Pop buttons back out once their press has elapsed (stone 1 s, wood 1.5 s).
    void UpdateButtons(float dt);
    // Drop every pressed button back to rest; used before saving so a save never holds a pressed one.
    void ReleaseAllButtons();

    // --- Redstone repeater (Java BlockRedstoneRepeater) -------------------------------------------
    // Facing (bits 0-1: the direction it drives) and delay (bits 2-3: 1-4 redstone ticks) live in the
    // same settings nibble the buttons use. The powered state is a block id, not a state bit, so a
    // powered repeater can answer "am I driving you" with an id comparison.
    struct RepeaterTimer
    {
        int x, y, z;
        float timeLeft;
        // 0 = repeater delay, 1 = redstone torch re-check, 2 = piston re-check. Torches and pistons share
        // the list because all three are plain "wait, then look at the world again" jobs that carry no
        // payload, and each of them only ever has a handful outstanding.
        int kind;
    };
    std::vector<RepeaterTimer> mRepeaterTimers;

    // Java BlockRedstoneTorch.func_150111_a: a torch that toggles eight times inside 60 ticks (3 s) is
    // burnt out and stops relighting itself. This is not flavour -- it is the rule that stops two
    // torches looking at each other from re-meshing their chunks at the 0.1 s tick rate forever. One
    // entry per cell is enough, because the budget is per cell.
    struct TorchHeat
    {
        int x, y, z;
        int flips;
        float windowLeft;
    };
    std::vector<TorchHeat> mTorchHeat;
    std::vector<TorchHeat>::size_type TorchHeatIndex(int x, int y, int z);
    /// Record one toggle of a torch and report whether it is now over the budget.
    bool NoteTorchFlip(int x, int y, int z);
    /// True while a torch is too hot to light itself; Java refuses the dark -> lit edge, never the lit
    /// -> dark one, so a hot torch always ends up dark instead of jittering.
    bool TorchIsBurntOut(int x, int y, int z);
    /// Age every torch's window and drop the cells that have cooled all the way down.
    void DecayTorchHeat(float dt);

    static bool IsRepeaterBlock(block_t b);
    static int RepeaterFacingFromState(int state);
    // The single horizontal direction a repeater drives, as unit steps: +X or +Z on the axis it faces.
    // Exposed so the dust mesher's connection test and the power code read the same table.
    static int RepeaterDirX(int facing);
    static int RepeaterDirZ(int facing);
    static int RepeaterDelayFromState(int state);   // 1..4 redstone ticks
    static int RepeaterStateFrom(int facing, int delay);
    int GetRepeaterState(int x, int y, int z);
    void SetRepeaterState(int x, int y, int z, int state);
    // 15 when a powered repeater faces this cell, else 0. Only the cell in front of a powered repeater
    // is ever driven, which is what stops a repeater leaking power sideways or backwards.
    int RepeaterOutputAt(int x, int y, int z);
    // Power on the one cell a repeater reads: the block it faces away from.
    int RepeaterInputPowered(int x, int y, int z);
    // Java repeater locking: a *powered* repeater pointing into this one's side freezes it, so it holds
    // its current output until that neighbour switches off.
    bool RepeaterIsLocked(int x, int y, int z);
    void ScheduleRepeater(int x, int y, int z);
    void EvaluateRepeater(int x, int y, int z);
    void SetRepeaterPowered(int x, int y, int z, bool powered);
    // Right-click action: cycle the 1-4 redstone-tick delay. The facing is fixed at placement and is
    // never changed afterwards.
    void UseRepeater(int x, int y, int z);
    // Fire repeaters whose delay has elapsed; called once per frame from both play states.
    void UpdateRepeaters(float dt);
    void ClearRepeaters();

    // --- Piston (Java BlockPistonBase / BlockPistonExtension) --------------------------------------
    // Java spends one metadata bit on "sticky" and one on "extended"; this codebase spends block ids on
    // both, which leaves the state nibble holding nothing but the direction the piston pushes. That is
    // bits 0-2, in the same 6-neighbour order the button and lever mountings use
    // (0 = down, 1 = up, then -X, +X, -Z, +Z). Bit 3 is unused, and the nibble is still registered as
    // state -- see IsStateCellBlock -- so that the light engine leaves it alone.
    static bool IsPistonBaseBlock(block_t b);
    static bool IsPistonHeadBlock(block_t b);
    // Any piston cell: base or head. This is the one IsStateCellBlock asks about.
    static bool IsPistonBlock(block_t b);
    // The sticky half of the family, for the one place it matters: a retraction that pulls the block in
    // front of the head back into the cell the head vacates.
    static bool IsStickyPiston(block_t b);
    // True for the base ids whose head is out. "Extended" is the id, so this is an id test and not a
    // nibble read -- which is what lets it be asked from the mesher and the collision path.
    static bool IsPistonExtended(block_t b);
    static int PistonFacingFromState(int state);
    static int PistonStateFrom(int facing);
    int GetPistonFacing(int x, int y, int z);
    void SetPistonState(int x, int y, int z, int facing);
    // Unit step the piston pushes along, for a facing index. Exposed so the push code, the mesher and the
    // placement code all read one table instead of three copies of it.
    static int PistonDirX(int facing);
    static int PistonDirY(int facing);
    static int PistonDirZ(int facing);
    // Collision shape of a piston base cell: the full cube when retracted, and the 4 px plate the head
    // retracts onto when extended. GetBlockCollideBox recomputes it per cell from the state nibble.
    static BoundingBox PistonCellBox(int facing, bool extended);

    // --- Piston behaviour (Java BlockPistonBase) ---------------------------------------------------
    // Java's push limit, written as the distance to the first air cell in front rather than as a count
    // of moved blocks: the cell the head goes into is distance 1, so a full run of twelve solid cells
    // ahead is one block too many and the piston stays shut.
    enum { kPistonPushLimit = 12 };
    // Queue a piston's re-check on the shared timer list (kind 2), one entry per cell. A piston decides
    // from the world it finds when its timer fires, so an edge arriving while one is pending does not
    // restart it -- the same shape as the repeater and the torch, and the reason a row of pistons runs
    // itself in order instead of all of them judging a world that is halfway through changing.
    void SchedulePiston(int x, int y, int z);
    // Re-read the power at one piston base and extend or retract to match. Safe to call on any cell: it
    // returns unless there is a base there. Nothing else may move a piston, so the redstone window and
    // the timer can never disagree about what it should be doing.
    void EvaluatePiston(int x, int y, int z);
    // The power a piston actually answers to, which is GetRedstonePowerTo with the cell it pushes into
    // left out of the world for the length of the query (Java BlockPistonBase.isIndirectlyPowered).
    int GetPistonPowerTo(int x, int y, int z);
    // Room to extend? Walks the line, refuses the first block that cannot move, and reports how far the
    // first air cell is when there is one.
    bool PistonCanExtend(int x, int y, int z, int* outDistance);
    bool ExtendPiston(int x, int y, int z);
    bool RetractPiston(int x, int y, int z);
    // Move one whole cell: the id, the state nibble when it has one, and the light when the block is a
    // source. Every block a push shoves and the single block a sticky head pulls back goes through here.
    void MoveBlockCell(int fromX, int fromY, int fromZ, int toX, int toY, int toZ);
    // A cell a piston empties and a cell a piston fills do not need the same follow-up, and the
    // difference is not cosmetic: DestroyAroundTrapdoors and DestroyAroundItemFrames take the cell they
    // are given to be air and delete whatever is mounted on its faces without checking, so they belong
    // only on the empty case. The filled case gets the two sweeps that check support and the mesh.
    void PistonCellEmptied(int x, int y, int z);
    void PistonCellFilled(int x, int y, int z);
    // Shove whatever is standing in the line out of the way. Called with the base cell, the facing and
    // the distance to the first air cell.
    void PistonPushEntities(int x, int y, int z, int facing, int distance);

    // --- Piston stroke (the arm's travel) ----------------------------------------------------------
    // Java draws a moving piston with a TileEntityPiston: the base and head cells keep their blocks and
    // the renderer slides the head between them, half a block a tick. This port has no per-block
    // entities and moves the block ids at once, so the stroke is a short list of cells instead -- one
    // entry per piston that is mid-move, holding how far the head has travelled. While an entry lasts,
    // the *base* cell draws the pair and the head cell stands down; that is what lets the rod telescope
    // into the base instead of the head jumping a whole cell at a time.
    struct PistonStroke
    {
        int x, y, z;        // the base cell
        int facing;
        float progress;     // 0 with the head home, 1 with it fully out
        char extending;     // 1 while a head is travelling out, 0 while one is going back in
    };
    std::vector<PistonStroke> mPistonStrokes;
    // Begin (or restart) this base's stroke from the end the world has just moved to. Called by
    // ExtendPiston and RetractPiston once the ids are in place.
    void StartPistonStroke(int x, int y, int z, int facing, bool extending);
    // Advance every stroke and queue the two cells for a remesh while one lasts. Called once per frame,
    // from UpdateWorldProcesses: the mesh queue takes one chunk a frame, so a stroke is drawn in as many
    // poses as the frame budget allows rather than in a fixed number of them.
    void UpdatePistonStrokes(float dt);
    // The stroke drawing through this cell, if any, with how far its head is out. A head cell reports
    // the stroke that owns it -- so it can stand down and let the base draw the pair -- and a base cell
    // reports its own. False when the cell is not part of a moving piston.
    bool GetPistonStroke(int x, int y, int z, bool isHead, int facing, float* progress, bool* extending);
    // Drop every stroke; transient, like the pending timer list, so a save does not carry one.
    void ClearPistonStrokes();
    // Breaking half a piston has to take the other half with it. Called by the break path *before* the
    // cell is emptied, because the direction it needs lives in the nibble that is about to be cleared.
    void BreakPistonPartner(int x, int y, int z);

    void DestroyTNTentity(int id);

    void DropThis(int id, Vector3 position);
    void DropThis(int id, int am, Vector3 position);
    void DropThis(int id, int am, bool st, Vector3 position);

    void DropThisNoAlign(int id, int am, bool st, Vector3 position, Vector3 velocity);
    void DestroyDrop(int id);

    int FetchArmorPoints(int bootsId, int leggingsId, int chestId, int helmetId);
    float FetchDamagePoints(int weaponId, bool &weapon);
    bool HaveItemInBarSlots(int id);

    // Whole-pack search and spend. HaveItemInBarSlots only looks at the nine hotbar slots, which is
    // all the compass needed; a bow is different because the arrow it spends does not have to be the
    // stack in hand (vanilla takes it out of the pack), so both of these walk all 36 slots.
    bool HasItemInInventory(int id);
    bool ConsumeOneItemFromInventory(int id);

    // /give. Resolves a typed name to an item id, or -1. The comparison flattens the separators a
    // player might use -- "Debug Stick", "debug_stick", "debug-stick" -- into one, and folds case, so
    // the command never depends on which spelling the item's `name` happens to use.
    int GetItemIdByName(const std::string& name);

    // How many of `id` the pack could still take. Counts partial stacks to their 64 and treats each
    // empty slot as one full stack, or one item when the item does not stack.
    int InventoryRoomFor(int id);

    // /give. Hands over as much of `amount` as will fit and returns how many were actually given, so
    // the command can report a full inventory instead of silently swallowing the request.
    int GiveItemToPlayer(int id, int amount);

	int GetDrawntTrianglesCount() { return drawnTriangles; }

	Vector3 GetPlayerPos();

	// Java's EntityPlayer.addExhaustion. Adds to the accumulator and converts each full 4.0 into one
	// hunger point (Java's FoodStats.onUpdate), so a caller only has to say how exhausting the action
	// was and never has to know the exchange rate. Amounts live in the StatePlay defines.
	void AddExhaustion(float amount);

	void UpdatePlayerZoneBB(Vector3 playerPosition);
	void UpdateWorldTime(float dt);
	void UpdateWorldProcesses(float dt);
	void UpdateWorldAnimation(float dt);
	void UpdateClockTexture();
	void UpdateCompassTexture(float cameraAngle);
	// Every texel copy the update used to make (clock, compass, water/lava/fire/portal frames),
	// done after StartFrame so the GE is not sampling the atlas while it changes. The update only
	// picks the frames and accumulates the time.
	void ApplyTextureAnimation();
	float animPendingDt;
	void SpawnMobs(float dt);

	int clockFrame;
	int lastClockFrame;

	int compassFrame;
	int lastCompassFrame;
	bool haveCompass;

	int GetBlockTypesCount();
	int GetItemTypesCount();

	void UpdateChunkBlocks(int id);
	void UpdateBlockPhysics(int id);
	void UpdateChunkGrowingBlocks(int id, int vectorId);

	bool BlockIsWater(block_t b);
	bool BlockIsLava(block_t b);
	bool BlockIsIgnitable(block_t b);
	// Non-solid, non-fluid blocks flowing water/lava replaces (plants, torches, dust, carpet...)
	bool FluidWashesAway(block_t b);
	// Unit horizontal direction water in this cell pushes entities (Java getFlowVector), 0 if still/none
	Vector3 WaterFlowVector(int x, int y, int z);
	int FluidSpreadLevel(block_t b);
	// FluidSpreadLevel, except a cell under a real waterfall (2+ tall column) counts as a source
	int WaterEffectiveLevel(int x, int y, int z, block_t b);
	block_t FluidBlockFromLevel(bool lava, int level);
	int LavaDistanceToSource(int x, int y, int z);
	int FluidCornerHeightPx(int cx, int cy, int cz, bool lava);
	void BuildFluidBlock(int x, int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent);

	int FindDirId(int x, int y, int z);

	void GetTexturesIds();
	void GetSkyColor();

    short armorId[4];
    short armorAm[4];
    bool armorSt[4];
    short invId[36];
    short invAm[36];
    bool invSt[36];
    short mId;
    short mAm;
    bool mSt;
    float playerGrowth;
    float playerCollisionSize;

    std::vector<BaseBlock> blockTypes;
	std::vector<BaseItem> itemTypes;

    float brightFactor;
    float starsFactor;
    float dawnSunsetFactor;
    Vector3	fogColor;
	Vector3 startSkyColor;
	Vector3 endSkyColor;
	Vector3 dawnSunsetSkyColor;

	Vector3 startClearSkyColor;
	Vector3 endClearSkyColor;
	Vector3 dawnSunsetClearSkyColor;

	Vector3 startRainySkyColor;
	Vector3 endRainySkyColor;
	Vector3 dawnSunsetRainySkyColor;

	float rainyColorAlpha;

	float blockZConstLight;
	float blockXConstLight;

    unsigned int worldSeed;
    float HP;
    float HG;
    float AP;
    char OS;

    // Java's FoodStats.foodExhaustionLevel. Every movement, jump, hit and blow adds a little; when it
    // passes 4.0 that is one hunger point and 4.0 is taken back off. Keeping it as an accumulator is
    // what makes the drains add up the way they do in Java -- a single sprinted metre (0.1) is ten
    // walked ones (0.01), so sprinting empties the bar about ten times faster for the same distance --
    // where a per-second subtraction cannot express the difference at all. Not saved: at worst a load
    // loses the part-filled unit, which is under one hunger point.
    float foodExhaustion;

    bool kickedStart;
    int kickedBy;
    int kickedDamage;
    float kickedAngle;
    float kickedTimer;
    // Distance/exposure-scaled explosion launch, supplied by TNT when a blast catches the player (see
    // TNT::Update). Zero means "no launch supplied", and the game state falls back to the flat
    // zombie/creeper kick constants -- so mob knockback is unchanged while a blast can scale with how
    // close the player was and how much of the blast reached them. Cleared once the launch is applied.
    float kickedVelY;
    float kickedPush;

    float animationTimer;
    float animationTimerEnd;
    char animationWaterFrame;
    char animationLavaFrame;
    bool animationLavaStep;
	char animationFireFrame;
	// 0..31 for the portal swirl. Unlike lava it only ever steps forward, because the strip is a
	// loop rather than a ping-pong.
	char animationPortalFrame;

    float spawnZombiesTimer;
    float spawnCowsTimer;
    float spawnCreeperTimer;
	float spawnPigTimer;
    float spawnSheepTimer;
    float spawnChickenTimer;
    float spawnSpiderTimer;
    float spawnSkeletonTimer;

    int textureWaterAnimationId;
    int textureLavaAnimationId;
	int textureFireAnimationId;
	int textureFireAnimation2Id; // 1.7.3 fire_layer_1, second layer of burning-mob fire
	// The nether portal swirl, patched frame by frame into its own terrain tile like water and lava.
	int texturePortalAnimationId;
    int textureItemsId;
    int textureTerrainId;
    int textureTerrainMipsId;
    int textureLeatherArmor1Id;
    int textureLeatherArmor2Id;
    int textureChainArmor1Id;
    int textureChainArmor2Id;
    int textureIronArmor1Id;
    int textureIronArmor2Id;
    int textureDiamondArmor1Id;
    int textureDiamondArmor2Id;
    int textureGoldenArmor1Id;
    int textureGoldenArmor2Id;
    int textureVegetationColorMapId;
    int textureClockId;
    int textureCompassId;
    int textureSkyColorId;

	char worldName[50];
	int createdChunksCount;

	float worldDayTime;
	float worldTime;

	float sunTime;
	int worldVersion;
	float lightAngle;

	Vector3 playerZoneSize;
	Vector3 playerSpawnPointPosition;

	Vector3 snowBiomePosition;
	Vector3 desertBiomePosition;
	float snowBiomeRadius;
	float desertBiomeRadius;

    std::vector<Chest*> mChests;
    std::vector<Furnace*> mFurnaces;
    std::vector<DirectionBlock*> mDirects;
    std::vector<Zombie*> mZombies;
    std::vector<Cow*> mCows;
    std::vector<Creeper*> mCreepers;
	std::vector<Pig*> mPigs;
    std::vector<Sheep*> mSheeps;
    std::vector<Chicken*> mChickens;
    std::vector<Spider*> mSpiders;
    std::vector<Skeleton*> mSkeletons;
    // Arrows in flight. World-owned rather than state-owned like the snowballs, because a mob has to
    // be able to add one when its bow fires; the play states walk the list to step, sweep and draw it.
    // Arrow2 is only forward-declared here (see the top of the file), hence the full name.
    std::vector<Aurora::Graphics::Arrow2*> mArrows;
    std::vector<Drop*> mDrops;
    std::vector<TNT*> mTNTs;
    std::vector<NoteBlockEntity*> mNoteBlocks;
    std::vector<JukeboxEntity*> mJukeboxes;
    std::vector<MonsterSpawnerEntity*> mMonsterSpawners;
    std::vector<ItemFrameEntity*> mItemFrames;

    /// Gui block properties
    std::vector<Vector3*> mPositionLeftRight;
    std::vector<Vector2*> mtexturesLeftRight;
    std::vector<Vector3*> mTriangleLeftRight;

    std::vector<Vector3*> mPositionBackFront;
    std::vector<Vector2*> mtexturesBackFront;
    std::vector<Vector3*> mTriangleBackFront;

    std::vector<Vector3*> mPositionBottomTop;
    std::vector<Vector2*> mtexturesBottomTop;
    std::vector<Vector3*> mTriangleBottomTop;

    int iVertexLeftRight;
    int pointsLeftRight;

    int iVertexBackFront;
    int pointsBackFront;

    int iVertexBottomTop;
    int pointsBottomTop;

    int mChestsize;
    int mFurnacesize;
    int mDirectsize;
    int mZombiesize;
    int mCowsize;
    int mSheepsize;
    int mCreepersize;
	int mPigsize;
    int mTNTsize;
    int mNoteBlocksize;
    int mJukeboxsize;
    int mMonsterSpawnersize;
    int mItemFramesize;

    int spawner_x;
    int spawner_z;

    int zombieNum;
    int dropNum;
    int TNTnum;

    Vector3 lightColor;
    Vector3	ambientColor;
    float westLight;
    float eastLight;

    float skyTime;

    int FindFurnaceID(Vector3 pos);
    int FindChestID(Vector3 pos);
    int FindNoteBlockID(Vector3 pos);
    int FindJukeboxID(Vector3 pos);
    int FindMonsterSpawnerID(Vector3 pos);
    int FindItemFrameID(Vector3 pos);

    void SetPlayerSpawnPosition(Vector3 newPosition);

    void BuildExplodeSphere(float radius, int X, int Y, int Z);
    void BuildBlockSphere(float radius, block_t block, int X, int Y, int Z);

    void BuildBlockSphere(float radius, block_t block, int X, int Y, int Z, block_t blockToChange);

    void BuildBlockEllipsoid(float w_radius, float h_radius, block_t block, int X, int Y, int Z);

    std::vector<SimpleMeshChunk*> mChunks;
    std::vector<int> toUpdate;
    std::vector<int> toFastUpdate;
    // Parallel arrays: redstoneMeshQueue[i] is a chunk id, redstoneMeshOpaque[i] says whether its
    // opaque half changed along with the transparent one. See QueueRedstoneMesh.
    std::vector<int> redstoneMeshQueue;
    std::vector<char> redstoneMeshOpaque;
    int pendingLightCount;
    Vector3 pendingLightPos[8];

    std::vector<Vector3> floodPos;
    std::vector<int> floodChunks;

    float updatingChunksTimer;
    float updatingChunksTimerMax;
    unsigned int updatingChunksPosition;

    std::vector<int> updatingChunks;

    // for humanoids
    TexturesPSPVertex *bodyVertices;
    TexturesPSPVertex *headVertices;
    TexturesPSPVertex *hatVertices;
    TexturesPSPVertex *legVertices;
    TexturesPSPVertex *handVertices;

    // for creeper
    TexturesPSPVertex *creeperLegVertices;

	// for pig (classic ModelPig / pig.png 64x32)
    TexturesPSPVertex *pigBodyVertices;
    TexturesPSPVertex *pigHeadVertices;
    TexturesPSPVertex *pigSnoutVertices;
    TexturesPSPVertex *pigLegVertices;

    // for chicken (ModelChicken / entity/chicken/chicken.png 64x32). The head carries its beak
    // and wattle already baked in, so there is no separate snout mesh.
    TexturesPSPVertex *chickenBodyVertices;
    TexturesPSPVertex *chickenHeadVertices;
    TexturesPSPVertex *chickenLegVertices;
    TexturesPSPVertex *chickenWingVertices;

    // for spider (ModelSpider / entity/spider/spider.png 64x32). Three body bones plus one
    // leg box drawn eight times: spiderLegVertices grows toward -Z, spiderLegLeftVertices
    // toward +Z, so the two sides show the art the right way round. The eyes are a second
    // texture (spider_eyes.png) drawn unlit on top of the head and thorax.
    TexturesPSPVertex *spiderBodyVertices;
    TexturesPSPVertex *spiderNeckVertices;
    TexturesPSPVertex *spiderHeadVertices;
    TexturesPSPVertex *spiderLegVertices;
    TexturesPSPVertex *spiderLegLeftVertices;

    // for skeleton (ModelSkeleton / entity/skeleton/skeleton.png 64x32). ModelSkeleton extends
    // ModelZombie and overrides only the limbs -- 2x12x2 with the arm art at 40,16 and the leg art
    // at 0,16 -- so there are just two buffers here and the skeleton draws the zombie's
    // bodyVertices / headVertices, which are identical between the two mobs.
    TexturesPSPVertex *skeletonHandVertices;
    TexturesPSPVertex *skeletonLegVertices;

    // for cows
    TexturesPSPVertex *cowBodyVertices;
    TexturesPSPVertex *cowHeadVertices;
    TexturesPSPVertex *cowLegVertices;
    TexturesPSPVertex *cowHornsVertices;

    // for sheep
    TexturesPSPVertex *sheepBodyVertices;
    TexturesPSPVertex *sheepHeadVertices;
    TexturesPSPVertex *sheepHeadOPTVertices;
    TexturesPSPVertex *sheepHeadFurVertices;
    TexturesPSPVertex *sheepLegOPTVertices;
    TexturesPSPVertex *sheepLegFurVertices;

    // for armor
    TexturesPSPVertex *helmetVertices;
    TexturesPSPVertex *shoulderVertices;
    TexturesPSPVertex *chestplateVertices;
    TexturesPSPVertex *bellyVertices;
    TexturesPSPVertex *leggingVertices;
    TexturesPSPVertex *bootVertices;

    Vector3 playerPos;
    bool playerContactLadder;

    //player zone aabb
	BoundingBox playerZoneBB;

	// The camera frustum as of the last frame drawn, and whether it is usable yet.
	//
	// Mesh builds are limited to what the camera can see -- see SectionInView. Nothing here ever takes
	// geometry AWAY: the gate is consulted only before a section's FIRST build, so a section that has
	// been on screen keeps its mesh for as long as it is resident. What it stops is spending vertex
	// memory and build time on sections that streamed in behind or beside the player and have never
	// been visible. Simulation is untouched: blocks, light, mobs, redstone and growth never read a mesh.
	Frustum viewFrustum;
	bool viewFrustumValid;
	// The view direction the mesher was last armed on. A turn past ~20 degrees re-arms it, so ground the
	// player turns to face is built without scanning the whole zone every frame.
	Vector3 viewArmedDir;
	bool viewArmedValid;

	int WORLD_SIZE;
	int CHUNK_SIZE;
    // The streamed column yields once, after its terrain rows and before its caves, so the surface can
    // be lit and drawn while the caves are still being carved. Sync generation passes a 0 budget and
    // never looks at this.
    bool pauseAfterTerrain;

	std::vector<int> mNearestChunksIds;

private:
    static const int kColHashSize = 256;
    ChunkColumn* colHash[kColHashSize];
    std::vector<ChunkColumn*> loadedColumns;
    ChunkColumn* lastCol;
    int lastCx;
    int lastCz;
    block_t dummyBlock;
    block_settings_t dummySettings;
    block_t writeSink;
    int streamOpsThisFrame;
    // True only for the duration of UpdateStreaming, so EnsureColumn can tell a column the streamer
    // asked for (expected, one per ring step) from one a block read dragged in (a stall: the reading
    // frame pays for a generate, a light scan and a relight it never budgeted for). Perf only.
    bool perfInStreaming;
    // How long the previous UpdateWorldProcesses took, in microseconds. Used by the streamer to hold a
    // memory-stick write back off a frame that was already busy (see the save gate in UpdateStreaming).
    // Release-visible, unlike the Perf counters, because the decision it feeds is a shipping one.
    unsigned int lastProcessUs;
    float streamTickTimer;
    int simColCursor;
    int meshColCursor;
    float simTickTimer;
    int meshBuildsThisFrame;
    int physicsBoxesThisFrame;

    // The section rebuild in flight, if any. One rebuild walks all 4,096 cells of a section and costs
    // roughly 30 ms, which is two frames' worth of budget at 60 fps -- so it is no longer finished in the
    // frame that asks for it. It is started by RequestMeshRebuild and advanced a slice at a time by
    // AdvanceMeshJob, once per frame, from UpdateWorldProcesses.
    enum { kMeshJobNone = 0, kMeshJobFull, kMeshJobOpaque, kMeshJobTransparent };
    int meshJobKind;
    int meshJobId;
    int meshJobCursor;       // cells already walked, 0 .. CHUNK_SIZE^3
    // The section's light/block/flags cache is an 18^3 = 5,832-cell fill before the scan can start, and
    // on the overworld that fill alone measured ~20 ms -- the whole per-section spike. It is walked with
    // the same slice clock as the scan now: meshJobFillCursor is how far in, meshJobCachesReady says the
    // scan may begin. See FillMeshCachesSlice.
    int meshJobFillCursor;
    bool meshJobCachesReady;
    bool meshJobScanPlants;  // full jobs only: discover animated crops for the animation tick
    bool meshJobSkin;
    int meshScanSealed;
    int meshScanCol;

    void RequestMeshRebuild(int kind, int id, bool scanPlants);
    // Walks one slice of the job, or finishes it. `budgetUs` is how long the slice may run; a quiet
    // frame passes a longer one so new ground draws in sooner, and a frame that just streamed passes
    // the short one so the hitch does not grow. Returns true when no job is left to run.
    bool AdvanceMeshJob(unsigned int budgetUs = 4000u);

    ChunkColumn* meshColGrid[3][3];
    bool meshGridActive;
    int meshGridCx;
    int meshGridCz;
    bool meshLightActive;
    int meshLightOriginX;
    int meshLightOriginY;
    int meshLightOriginZ;
    int meshLightDimX;
    int meshLightDimY;
    int meshLightDimZ;
    float meshLight[5832];
    // Snapshot of block ids and face-culling flags for the same 18^3 region, filled
    // once per section build so the mesher's ~30k neighbour probes are array reads.
    block_t meshBlocks[5832];
    unsigned char meshFlags[5832];
    // MeshFlagWall is BlockIsMeshWall: an opaque full cube, the cell type the scan may skip when it is
    // sealed on all six sides.
    enum { MeshFlagTransparent = 1, MeshFlagLightSource = 2, MeshFlagRefraction2 = 4, MeshFlagWall = 8 };
    bool meshPending;
    float meshIdleTimer;
    int meshWatchCx;
    int meshWatchCz;
    int meshWatchCy;
    float meshPendPosX;
    float meshPendPosY;
    float meshPendPosZ;

    int HashCol(int cx, int cz) const;
    void HashInsertColumn(ChunkColumn* col);
    void HashRemoveColumn(ChunkColumn* col);
    ChunkColumn* BlockColumn(int x, int z, bool create);
    ChunkColumn* ColumnForBlock(int x, int z);
    int LocalBlockIndex(ChunkColumn* col, int x, int y, int z) const;
    void BeginColumnGrid(int StartX, int StartZ);
    void BeginMeshCaches(int StartX, int StartY, int StartZ);
    /// The setup half of BeginMeshCaches: origin, dimensions and the column grid, without the fill.
    void BeginMeshCachesHead(int StartX, int StartY, int StartZ);
    /// Walk the light/block/flags fill from meshJobFillCursor, stopping at the time budget. The number of
    /// cells the cache covers. Zero budget means fill it all (the synchronous callers).
    int  MeshLightTotal() const;
    void FillMeshCachesSlice(unsigned int startUs, unsigned int budgetUs);
    void EndMeshCaches();
    bool BlockNeedsPhysicsTick(block_t id);
    bool BoxNeedsPhysics(int x0, int y0, int z0, int x1, int y1, int z1);
    void CreateMeshesForColumn(ChunkColumn* col, int forceCy = -1);
    void DestroyMeshesForColumn(ChunkColumn* col);
    void TryMeshOneSection();
    friend int StreamBeginColumn(CraftWorld* w, int cx, int cz);
    friend void StreamAdvanceJob(CraftWorld* w, unsigned int budgetUs);
    friend void StreamUpdatePins(CraftWorld* w, int pcx, int pcz, int radius);
    friend bool StreamPickPin(CraftWorld* w, int pcx, int pcz, int& outCx, int& outCz);
    void AddNearestMeshId(int id);
    void DirtyNeighborMeshes(int cx, int cz);
    int ColumnTopY(ChunkColumn* col) const;
    void RemoveFloatingSnow(ChunkColumn* col);
    void RelightSharedEdges(int cx, int cz);
    void InitColumnLighting(ChunkColumn* col, int zBegin = 0, int zEnd = -1);
    bool SectionInPlayerZone(int startX, int startY, int startZ) const;
    // Zone test that also admits the column's surface band at any height (the flier's ground).
    bool SectionInPlayerZone(int startX, int startY, int startZ, const ChunkColumn* col) const;
    bool SectionInPlayerZoneXZ(int startX, int startZ) const;
    bool SectionInSurfaceBand(const ChunkColumn* col, int startY) const;
    bool MeshInPlayerZone(const SimpleMeshChunk* mesh) const;
    // True when this section should be given geometry. Off-camera sections are left as raw blocks until
    // the camera looks at them. Non-const because Frustum::BoxInFrustum is.
    bool SectionInView(int startX, int startY, int startZ);
    // Lower is sooner. The section the player is standing in wins, then the one the camera is aimed
    // at, then anything already behind them. With the column given, a section the ground passes
    // through counts as near whatever height the player flies at.
    int SectionMeshPriority(int startX, int startY, int startZ, const ChunkColumn* col = 0) const;
    // Fills in the column's surface band (ChunkColumn::surfaceY / surfaceMinY) if the generator did not.
    void EnsureSurfaceBand(ChunkColumn* col);
    // Profile build: every couple of seconds, and again after the player stands still, every section
    // in the view ring that should have faces and does not is logged ("hole:") and a screenshot is
    // taken (ms0:/PICTURE). A hole that is still there after they stop is also re-queued.
    void AuditHoles(float dt);
    Vector3 auditLastPos;
    float auditStillFor;
    float auditTimer;
    int auditShots;
    // A full opaque cube. Water, glass, leaves and plants are not: they are part of the visible
    // surface, and the mesh walk is not allowed to stop on them.
    bool BlockIsMeshWall(block_t id) const;
    // True when this cell is a wall block with wall blocks on all six sides, so it has no face to draw.
    bool CellBuriedInWall(int x, int y, int z);
    // The frustum's view direction, for the turn test that re-arms the mesher. See viewArmedDir.
    Vector3 ViewDirectionFromFrustum() const;
    void TickUnmeshedColumns(float dt);
    void TickSectionGrowth(int StartX, int StartY, int StartZ);
    void SaveColumnFile(ChunkColumn* col);
    // Columns handed to the saver thread (see ColumnSaver.h) whose files are written: marks them in
    // the file index and frees them. Called from the streaming tick and before any bulk save/unload.
    void ReapSavedColumns();
    bool LoadColumnFile(int cx, int cz);
    void EnsureChunkDir();
    std::string ColumnFilePath(int cx, int cz);
    // Which column files exist in the current save folder: one bit per column, read from the directory
    // once per world and dimension and kept current by SaveColumnFile. A column that was never saved is
    // then generated without first asking the memory stick for a file that is not there.
    void EnsureColumnFileIndex();
    bool ColumnFileKnownAbsent(int cx, int cz);
    void NoteColumnFile(int cx, int cz, bool exists);
    std::vector<unsigned char> colFileBits;
    int colFileSide;
    int colFileDim;
    std::string colFilePath;
    bool MeshIdValid(int id) const;

	int texutreSize;
	int textureChunk;
	float percent;
	float percent2;
	float percent3;

	std::vector<SimpleMeshChunk*> mTransparentChunks;

	block_settings_t* m_BlockSettings;
    block_t* m_Blocks;


	int chunksCreatedInFrameCount;
	int transparentchunksCreatedInFrameCount;
	int chunksCreatedInFrameCount2;
	int transparentchunksCreatedInFrameCount2;


	float minuteTimer;
	float allChunksUpdateTimer;
	bool updateChunksSwitch;

	//world time
	float worldHour;//1 game hour = 50 real seconds; 24 game hours = 20 real minutes

	//information
	int drawnTriangles;
	float lightShadowFactor;
	Vector3 lightFactor;
};
#endif