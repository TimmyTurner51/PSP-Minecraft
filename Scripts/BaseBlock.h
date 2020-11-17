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

typedef unsigned char block_t;

class BaseBlock
{
public:
	char upPlane_x, upPlane_y;
	char downPlane_x, downPlane_y;
	char leftPlane_x, leftPlane_y;
	char rightPlane_x, rightPlane_y;
    char botPlane_x, botPlane_y;
    char forPlane_x, forPlane_y;

	TexturesPSPVertex *verticesLeftRight;
	char pointsLeftRight;

    TexturesPSPVertex *verticesBackFront;
	char pointsBackFront;

    TexturesPSPVertex *verticesBottomTop;
	char pointsBottomTop;

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
