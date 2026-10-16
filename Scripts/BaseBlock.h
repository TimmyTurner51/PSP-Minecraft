#ifndef BASEBLOCK_H
#define BASEBLOCK_H

#include <Aurora/Graphics/Vertex.h>
#include <Aurora/Math/BoundingBox.h>
#include <stdlib.h>
#include <string>
#include <Aurora/Graphics/RenderManager.h>
#include <pspmath.h>
#include <psptypes.h>

using namespace Aurora::Graphics;

typedef unsigned short block_t;
typedef unsigned char block_settings_t;

// Items occupy 250 .. kExtendedBlockBase-1 (Items.cpp). That is about 1800 item
// slots, enough for a Minecraft-sized item list. New block ids start at
// kExtendedBlockBase and run to 65535 in the world. A block kept in an inventory
// slot has to fit in a signed short, so the practical ceiling for a held block
// is 32767. Register those with CraftWorld::AddBlockType, not push_back
// (push_back would land on item ids 254+).
enum
{
    kItemIdBase = 250,
    kExtendedBlockBase = 2048
};

inline bool IsItemId(int id)
{
    return id >= kItemIdBase && id < kExtendedBlockBase;
}

inline bool IsBlockId(int id)
{
    return id < kItemIdBase || id >= kExtendedBlockBase;
}

class BaseBlock
{
public:
	char upPlane_x, upPlane_y;
	char downPlane_x, downPlane_y;
	char leftPlane_x, leftPlane_y;
	char rightPlane_x, rightPlane_y;
    char botPlane_x, botPlane_y;
    char forPlane_x, forPlane_y;

	// The one tile that is not one of the six cube faces. Only the piston has a use for it: the face
	// at the bottom of the 4 px recess in an extended base, which vanilla paints with piston_inner.
	// Every other block leaves it unset and nothing reads it -- the six above it are the ones the
	// shared cube builders walk, and this one is named by the hand-emitted piston model alone.
    char innerPlane_x, innerPlane_y;

	// The three block meshes. Borrowed pointers, NOT owned: CraftWorld allocates them in
	// buildblocksVerts() and releases them in FreeBlockTypeMeshes(), and ~BaseBlock deliberately frees
	// nothing. That is what makes this class safe to copy -- and it must be safe, because it is held by
	// value in a std::vector and copied on every reallocation. See FreeBlockTypeMeshes().
	TexturesPSPVertex *verticesLeftRight;
	short pointsLeftRight;

    TexturesPSPVertex *verticesBackFront;
	short pointsBackFront;

    TexturesPSPVertex *verticesBottomTop;
	short pointsBottomTop;

	bool editable;
	bool transparent; 
	bool lightSource; 
	bool update;
	bool solid;
	bool animated;
	bool allowLight;

    short furnItem;
    short loot;

    char material; 
    //char points; 
	char soundType;
    char blockModel;         

    char refraction;
    BoundingBox collideBox;

    std::string name;

    virtual bool MatchWithRecipe2x2(std::string recipe_string[2], int& return_id, int& return_am, bool& return_st); // may be in future
    virtual bool MatchWithRecipe3x3(std::string recipe_string[3], int& return_id, int& return_am, bool& return_st);

	BaseBlock();
	virtual ~BaseBlock();

	static block_t getBaseID();
};

#endif
