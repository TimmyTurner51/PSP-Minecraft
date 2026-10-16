#include "BaseItem.h"


BaseItem::BaseItem()
{
	textureRow = 0;
	plane = 0;

	points = 0;
	addPoints = 0;
	add2Points = 0;

	furnItem = -1;
	durabilityPoints = -1;
	itemType = 'D';

	// All four mesh pointers, not just two. add2Vertices and dropVertices used to be left
	// uninitialised, so every item carried garbage there -- and because items are held by value in
	// `itemTypes`, that garbage was handed to free() as if it were a buffer. FreeItemTypeMeshes() now
	// tests these against NULL, so they have to actually start as NULL.
	vertices = NULL;
	addVertices = NULL;
	add2Vertices = NULL;

	dropVertices = NULL;

	terrainTexture = false;
	stackable = true;

	name = "00";
}

// Frees nothing, for the same reason as ~BaseBlock: CraftWorld owns the item meshes (see
// FreeItemTypeMeshes) so that copying a BaseItem cannot free them twice. It also removes a latent leak
// in the old code, which only freed anything at all when `vertices` happened to be non-NULL -- an item
// whose main mesh was never built would strand addVertices/add2Vertices/dropVertices.
BaseItem::~BaseItem()
{
}

item_t BaseItem::getBaseID(){	return 0;}
