#include "Blocks.h"

GrassBlock::GrassBlock():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 0;

    downPlane_x = 2;
    downPlane_y = 0;

    leftPlane_x = 3;
    leftPlane_y = 0;

    rightPlane_x = 3;
    rightPlane_y = 0;

    botPlane_x = 3;
    botPlane_y = 0;

    forPlane_x = 3;
    forPlane_y = 0;

    solid = true;
    editable = true;
	transparent = false;
	allowLight = false;

	soundType = 0;
	loot = 2;
	material = 3;
	update = 1;

    name = "Grass Block";
}

block_t GrassBlock::getID(){ return 1;}

DirtBlock::DirtBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 0;

    downPlane_x = 2;
    downPlane_y = 0;

    leftPlane_x = 2;
    leftPlane_y = 0;

    rightPlane_x = 2;
    rightPlane_y = 0;

    botPlane_x = 2;
    botPlane_y = 0;

    forPlane_x = 2;
    forPlane_y = 0;

    solid = true;
	editable = true;
	transparent = false;
	allowLight = false;

	soundType = 1;
	loot = 2;
	material = 3;
	update = 1;

	name = "Dirt";
}

block_t DirtBlock::getID(){ return 2;}


RockBlock::RockBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 0;

    downPlane_x = 1;
    downPlane_y = 0;

    leftPlane_x = 1;
    leftPlane_y = 0;

    rightPlane_x = 1;
    rightPlane_y = 0;

    botPlane_x = 1;
    botPlane_y = 0;

    forPlane_x = 1;
    forPlane_y = 0;

    solid = true;
	editable = true;
	transparent = false;
	allowLight = false;

	soundType = 2;
	loot = 36;
	material = 2;
	update = 0;
	refraction = 3;

	name = "Stone";
}

block_t RockBlock::getID(){ return 3;}


WaterBlock::WaterBlock():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 15;

    downPlane_x = 14;
    downPlane_y = 15;

    leftPlane_x = 14;
    leftPlane_y = 15;

    rightPlane_x = 14;
    rightPlane_y = 15;

    botPlane_x = 14;
    botPlane_y = 15;

    forPlane_x = 14;
    forPlane_y = 15;

	editable = false;
	transparent = true;
    solid = false;

    soundType = -1;
    blockModel = 5;
    allowLight = true;
    refraction = 0;

    name = "Water Block";
}

block_t WaterBlock::getID(){ return 4;}

GoldBlock::GoldBlock():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 2;

    downPlane_x = 0;
    downPlane_y = 2;

    leftPlane_x = 0;
    leftPlane_y = 2;

    rightPlane_x = 0;
    rightPlane_y = 2;

    botPlane_x = 0;
    botPlane_y = 2;

    forPlane_x = 0;
    forPlane_y = 2;

	editable = true;
	transparent = false;
    solid = true;

	loot = 5;
	material = 7;
    soundType = 2;
	furnItem = 280;

	name = "Gold Ore";
}

block_t GoldBlock::getID(){ return 5;}

LadderBlock::LadderBlock():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 5;

    downPlane_x = 3;
    downPlane_y = 5;

    leftPlane_x = 3;
    leftPlane_y = 5;

    rightPlane_x = 3;
    rightPlane_y = 5;

    botPlane_x = 3;
    botPlane_y = 5;

    forPlane_x = 3;
    forPlane_y = 5;
	editable = true;
	transparent = true;
	soundType = 3;

	loot = 6;
	material = 1;
	solid = true;
	allowLight = true;

	name = "Ladder Block";
}
block_t LadderBlock::getID(){ return 6;}


SandBlock::SandBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 1;

    downPlane_x = 2;
    downPlane_y = 1;

    leftPlane_x = 2;
    leftPlane_y = 1;

    rightPlane_x = 2;
    rightPlane_y = 1;

    botPlane_x = 2;
    botPlane_y = 1;

    forPlane_x = 2;
    forPlane_y = 1;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	loot = 248;
	material = 3;
	update = 1;
	solid = true;

	furnItem = 40;

	name = "Sand";
}
block_t SandBlock::getID(){ return 7;}


WoodBlock::WoodBlock():BaseBlock()
{
    upPlane_x = 5;
    upPlane_y = 1;

    downPlane_x = 5;
    downPlane_y = 1;

    leftPlane_x = 4;
    leftPlane_y = 1;

    rightPlane_x = 4;
    rightPlane_y = 1;

    botPlane_x = 4;
    botPlane_y = 1;

    forPlane_x = 4;
    forPlane_y = 1;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 3;

	loot = 8;
	material = 1;
	solid = true;

	furnItem = 277;

	name = "Oak Wood";
}
block_t WoodBlock::getID(){ return 8;}

LeavesBlock::LeavesBlock():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 3;

    downPlane_x = 4;
    downPlane_y = 3;

    leftPlane_x = 4;
    leftPlane_y = 3;

    rightPlane_x = 4;
    rightPlane_y = 3;

    botPlane_x = 4;
    botPlane_y = 3;

    forPlane_x = 4;
    forPlane_y = 3;

	editable = true;
	allowLight = true;
	transparent = true;
	soundType = 0;
	blockModel = 5;

	loot = 9;
	material = 4;
	solid = true;
	refraction = 1;

	name = "Oak Leaves";
}
block_t LeavesBlock::getID(){ return 9;}


BlackWoolBlock::BlackWoolBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 7;

    downPlane_x = 1;
    downPlane_y = 7;

    leftPlane_x = 1;
    leftPlane_y = 7;

    rightPlane_x = 1;
    rightPlane_y = 7;

    botPlane_x = 1;
    botPlane_y = 7;

    forPlane_x = 1;
    forPlane_y = 7;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 10;
	material = 4;
	solid = true;

	name = "Black Wool";
}
block_t BlackWoolBlock::getID(){ return 10;}


GrayWoolBlock::GrayWoolBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 7;

    downPlane_x = 2;
    downPlane_y = 7;

    leftPlane_x = 2;
    leftPlane_y = 7;

    rightPlane_x = 2;
    rightPlane_y = 7;

    botPlane_x = 2;
    botPlane_y = 7;

    forPlane_x = 2;
    forPlane_y = 7;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 11;
	solid = true;

	name = "Gray Wool";
}
block_t GrayWoolBlock::getID(){ return 11;}


RedWoolBlock::RedWoolBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 8;

    downPlane_x = 1;
    downPlane_y = 8;

    leftPlane_x = 1;
    leftPlane_y = 8;

    rightPlane_x = 1;
    rightPlane_y = 8;

    botPlane_x = 1;
    botPlane_y = 8;

    forPlane_x = 1;
    forPlane_y = 8;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 12;
	solid = true;

	name = "Red Wool";
}
block_t RedWoolBlock::getID(){ return 12;}


PinkWoolBlock::PinkWoolBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 8;

    downPlane_x = 2;
    downPlane_y = 8;

    leftPlane_x = 2;
    leftPlane_y = 8;

    rightPlane_x = 2;
    rightPlane_y = 8;

    botPlane_x = 2;
    botPlane_y = 8;

    forPlane_x = 2;
    forPlane_y = 8;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 13;
	solid = true;

	name = "Pink Wool";
}
block_t PinkWoolBlock::getID(){ return 13;}


DarkGreenWoolBlock::DarkGreenWoolBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 9;

    downPlane_x = 1;
    downPlane_y = 9;

    leftPlane_x = 1;
    leftPlane_y = 9;

    rightPlane_x = 1;
    rightPlane_y = 9;

    botPlane_x = 1;
    botPlane_y = 9;

    forPlane_x = 1;
    forPlane_y = 9;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 14;
	solid = true;

	name = "Green Wool";
}
block_t DarkGreenWoolBlock::getID(){ return 14;}


GreenWoolBlock::GreenWoolBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 9;

    downPlane_x = 2;
    downPlane_y = 9;

    leftPlane_x = 2;
    leftPlane_y = 9;

    rightPlane_x = 2;
    rightPlane_y = 9;

    botPlane_x = 2;
    botPlane_y = 9;

    forPlane_x = 2;
    forPlane_y = 9;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 15;
	solid = true;

	name = "Lime Wool";
}
block_t GreenWoolBlock::getID(){ return 15;}



BrownWoolBlock::BrownWoolBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 10;

    downPlane_x = 1;
    downPlane_y = 10;

    leftPlane_x = 1;
    leftPlane_y = 10;

    rightPlane_x = 1;
    rightPlane_y = 10;

    botPlane_x = 1;
    botPlane_y = 10;

    forPlane_x = 1;
    forPlane_y = 10;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 16;
	solid = true;

	name = "Brown Wool";
}
block_t BrownWoolBlock::getID(){ return 16;}


YellowWoolBlock::YellowWoolBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 10;

    downPlane_x = 2;
    downPlane_y = 10;

    leftPlane_x = 2;
    leftPlane_y = 10;

    rightPlane_x = 2;
    rightPlane_y = 10;

    botPlane_x = 2;
    botPlane_y = 10;

    forPlane_x = 2;
    forPlane_y = 10;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 17;
	solid = true;

	name = "Yellow Wool";
}
block_t YellowWoolBlock::getID(){ return 17;}

BlueWoolBlock::BlueWoolBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 11;

    downPlane_x = 1;
    downPlane_y = 11;

    leftPlane_x = 1;
    leftPlane_y = 11;

    rightPlane_x = 1;
    rightPlane_y = 11;

    botPlane_x = 1;
    botPlane_y = 11;

    forPlane_x = 1;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 18;
	solid = true;

	name = "Blue Wool";
}
block_t BlueWoolBlock::getID(){ return 18;}

LightBlueWoolBlock::LightBlueWoolBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 11;

    downPlane_x = 2;
    downPlane_y = 11;

    leftPlane_x = 2;
    leftPlane_y = 11;

    rightPlane_x = 2;
    rightPlane_y = 11;

    botPlane_x = 2;
    botPlane_y = 11;

    forPlane_x = 2;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 19;
	solid = true;

	name = "Light Blue Wool";
}
block_t LightBlueWoolBlock::getID(){ return 19;}

VioletWoolBlock::VioletWoolBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 12;

    downPlane_x = 1;
    downPlane_y = 12;

    leftPlane_x = 1;
    leftPlane_y = 12;

    rightPlane_x = 1;
    rightPlane_y = 12;

    botPlane_x = 1;
    botPlane_y = 12;

    forPlane_x = 1;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 20;
	solid = true;

	name = "Purple Wool";
}
block_t VioletWoolBlock::getID(){ return 20;}

PastelWoolBlock::PastelWoolBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 12;

    downPlane_x = 2;
    downPlane_y = 12;

    leftPlane_x = 2;
    leftPlane_y = 12;

    rightPlane_x = 2;
    rightPlane_y = 12;

    botPlane_x = 2;
    botPlane_y = 12;

    forPlane_x = 2;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;
	loot = 21;
	solid = true;

	name = "Magneta Wool";
}
block_t PastelWoolBlock::getID(){ return 21;}

OrangeWoolBlock::OrangeWoolBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 13;

    downPlane_x = 2;
    downPlane_y = 13;

    leftPlane_x = 2;
    leftPlane_y = 13;

    rightPlane_x = 2;
    rightPlane_y = 13;

    botPlane_x = 2;
    botPlane_y = 13;

    forPlane_x = 2;
    forPlane_y = 13;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 22;
	solid = true;

	name = "Orange Wool";
}
block_t OrangeWoolBlock::getID(){ return 22;}

LightGrayWoolBlock::LightGrayWoolBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 14;

    downPlane_x = 1;
    downPlane_y = 14;

    leftPlane_x = 1;
    leftPlane_y = 14;

    rightPlane_x = 1;
    rightPlane_y = 14;

    botPlane_x = 1;
    botPlane_y = 14;

    forPlane_x = 1;
    forPlane_y = 14;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 23;
	solid = true;

	name = "Light Gray Wool";
}
block_t LightGrayWoolBlock::getID(){ return 23;}

WhiteWoolBlock::WhiteWoolBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 14;

    downPlane_x = 2;
    downPlane_y = 14;

    leftPlane_x = 2;
    leftPlane_y = 14;

    rightPlane_x = 2;
    rightPlane_y = 14;

    botPlane_x = 2;
    botPlane_y = 14;

    forPlane_x = 2;
    forPlane_y = 14;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;
	material = 1;

	loot = 24;
	solid = true;

	name = "Wool";
}
block_t WhiteWoolBlock::getID(){ return 24;}

CactusBlock::CactusBlock():BaseBlock()
{
    upPlane_x = 5;
    upPlane_y = 4;

    downPlane_x = 7;
    downPlane_y = 4;

    leftPlane_x = 6;
    leftPlane_y = 4;

    rightPlane_x = 6;
    rightPlane_y = 4;

    botPlane_x = 6;
    botPlane_y = 4;

    forPlane_x = 6;
    forPlane_y = 4;

	editable = true;
	transparent = true;
    blockModel = 5;
	soundType = 3;

	loot = 25;
	material = 1;
	update = 1;
	solid = true;
	refraction = 1;
    allowLight = true;
    furnItem = 373;

    name = "Cactus";

    collideBox = BoundingBox(Vector3(-0.4375f,-0.5f,-0.4375f),Vector3(0.4375f,0.5,0.4375f));
}
block_t CactusBlock::getID(){ return 25;}

SandStone::SandStone():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 11;

    downPlane_x = 0;
    downPlane_y = 13;

    leftPlane_x = 0;
    leftPlane_y = 12;

    rightPlane_x = 0;
    rightPlane_y = 12;

    botPlane_x = 0;
    botPlane_y = 12;

    forPlane_x = 0;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

	loot = 26;
	solid = true;
	material = 2;

	name = "Sandstone";
}
block_t SandStone::getID(){ return 26;}

BrickBlock::BrickBlock():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 0;

    downPlane_x = 7;
    downPlane_y = 0;

    leftPlane_x = 7;
    leftPlane_y = 0;

    rightPlane_x = 7;
    rightPlane_y = 0;

    botPlane_x = 7;
    botPlane_y = 0;

    forPlane_x = 7;
    forPlane_y = 0;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

	loot = 27;
	material = 9;
	solid = true;

	name = "Bricks";
}
block_t BrickBlock::getID(){ return 27;}

CageBlock::CageBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 4;

    downPlane_x = 1;
    downPlane_y = 4;

    leftPlane_x = 1;
    leftPlane_y = 4;

    rightPlane_x = 1;
    rightPlane_y = 4;

    botPlane_x = 1;
    botPlane_y = 4;

    forPlane_x = 1;
    forPlane_y = 4;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;

	loot = 28;
	material = 10;
	solid = true;
	refraction = 1;
    allowLight = true;

    name = "Spawner";
}
block_t CageBlock::getID(){ return 28;}

SnowSoil::SnowSoil():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 4;

    downPlane_x = 2;
    downPlane_y = 0;

    leftPlane_x = 4;
    leftPlane_y = 4;

    rightPlane_x = 4;
    rightPlane_y = 4;

    botPlane_x = 4;
    botPlane_y = 4;

    forPlane_x = 4;
    forPlane_y = 4;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 6;

	loot = 2;
	material = 3;
	solid = true;

	name = "Snow Soil";
}
block_t SnowSoil::getID(){ return 29;}

DarkWoodBlock::DarkWoodBlock():BaseBlock()
{
    upPlane_x = 5;
    upPlane_y = 1;

    downPlane_x = 5;
    downPlane_y = 1;

    leftPlane_x = 4;
    leftPlane_y = 7;

    rightPlane_x = 4;
    rightPlane_y = 7;

    botPlane_x = 4;
    botPlane_y = 7;

    forPlane_x = 4;
    forPlane_y = 7;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 3;

	loot = 30;
	material = 1;
	solid = true;

	name = "Spruce Wood";
}
block_t DarkWoodBlock::getID(){ return 30;}

WhiteWoodBlock::WhiteWoodBlock():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 9;

    downPlane_x = 4;
    downPlane_y = 9;

    leftPlane_x = 5;
    leftPlane_y = 7;

    rightPlane_x = 5;
    rightPlane_y = 7;

    botPlane_x = 5;
    botPlane_y = 7;

    forPlane_x = 5;
    forPlane_y = 7;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 3;

	loot = 31;
	material = 1;
	solid = true;

	furnItem = 277;

	name = "Birch Wood";
}
block_t WhiteWoodBlock::getID(){ return 31;}

IceBlock::IceBlock():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 4;

    downPlane_x = 3;
    downPlane_y = 4;

    leftPlane_x = 3;
    leftPlane_y = 4;

    rightPlane_x = 3;
    rightPlane_y = 4;

    botPlane_x = 3;
    botPlane_y = 4;

    forPlane_x = 3;
    forPlane_y = 4;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 3;

	loot = -1;
	solid = true;
	refraction = 1;

	name = "Ice";
}
block_t IceBlock::getID(){ return 32;}

CaneBlock::CaneBlock():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 4;

    downPlane_x = 9;
    downPlane_y = 4;

    leftPlane_x = 9;
    leftPlane_y = 4;

    rightPlane_x = 9;
    rightPlane_y = 4;

    botPlane_x = 9;
    botPlane_y = 4;

    forPlane_x = 9;
    forPlane_y = 4;

	editable = true;
	transparent = true;
    blockModel = 3;
    allowLight = true;

    loot = 293;
    update = 1;
    solid = false;
    refraction = 0;
	material = 11;

    name = "Cane Block";

    collideBox = BoundingBox(Vector3(-0.4375f,-0.5f,-0.4375f),Vector3(0.4375f,0.5,0.4375f));
}
block_t CaneBlock::getID(){ return 33;}

OakPlanks::OakPlanks():BaseBlock() // planks
{
    upPlane_x = 4;
    upPlane_y = 0;

    downPlane_x = 4;
    downPlane_y = 0;

    leftPlane_x = 4;
    leftPlane_y = 0;

    rightPlane_x = 4;
    rightPlane_y = 0;

    botPlane_x = 4;
    botPlane_y = 0;

    forPlane_x = 4;
    forPlane_y = 0;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 3;

    loot = 34;
    material = 1;
    solid = true;

    name = "Oak Planks";
}
block_t OakPlanks::getID(){ return 34;}

ShelfBlock::ShelfBlock():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 0;

    downPlane_x = 4;
    downPlane_y = 0;

    leftPlane_x = 3;
    leftPlane_y = 2;

    rightPlane_x = 3;
    rightPlane_y = 2;

    botPlane_x = 3;
    botPlane_y = 2;

    forPlane_x = 3;
    forPlane_y = 2;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 3;

    loot = 35;
    solid = true;

    name = "Bookshelf";
}
block_t ShelfBlock::getID(){ return 35;}

CobbleStone::CobbleStone():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 1;

    downPlane_x = 0;
    downPlane_y = 1;

    leftPlane_x = 0;
    leftPlane_y = 1;

    rightPlane_x = 0;
    rightPlane_y = 1;

    botPlane_x = 0;
    botPlane_y = 1;

    forPlane_x = 0;
    forPlane_y = 1;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

    loot = 36;
    material = 2;
    solid = true;

    furnItem = 3;

    name = "Cobblestone";
}
block_t CobbleStone::getID(){ return 36;}

SpruceLeaves::SpruceLeaves():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 8;

    downPlane_x = 4;
    downPlane_y = 8;

    leftPlane_x = 4;
    leftPlane_y = 8;

    rightPlane_x = 4;
    rightPlane_y = 8;

    botPlane_x = 4;
    botPlane_y = 8;

    forPlane_x = 4;
    forPlane_y = 8;

	editable = true;
	allowLight = true;
	transparent = true;
	allowLight = true;
	soundType = 0;
	blockModel = 5;

	loot = 37;
	material = 4;
	solid = true;
	refraction = 1;

    name = "Spruce Leaves";
}
block_t SpruceLeaves::getID(){ return 37;}

BirchLeaves::BirchLeaves():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 6;

    downPlane_x = 4;
    downPlane_y = 6;

    leftPlane_x = 4;
    leftPlane_y = 6;

    rightPlane_x = 4;
    rightPlane_y = 6;

    botPlane_x = 4;
    botPlane_y = 6;

    forPlane_x = 4;
    forPlane_y = 6;

	editable = true;
	allowLight = true;
	transparent = true;
	allowLight = true;
	soundType = 0;
	blockModel = 5;

	loot = 38;
	material = 4;
	solid = true;
	refraction = 1;

    name = "Birch Leaves";
}
block_t BirchLeaves::getID(){ return 38;}

Sponge::Sponge():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 3;

    downPlane_x = 0;
    downPlane_y = 3;

    leftPlane_x = 0;
    leftPlane_y = 3;

    rightPlane_x = 0;
    rightPlane_y = 3;

    botPlane_x = 0;
    botPlane_y = 3;

    forPlane_x = 0;
    forPlane_y = 3;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 1;

    loot = 38;
    solid = true;

    name = "Sponge";
}
block_t Sponge::getID(){ return 39;}

GlassBlock::GlassBlock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 3;

    downPlane_x = 1;
    downPlane_y = 3;

    leftPlane_x = 1;
    leftPlane_y = 3;

    rightPlane_x = 1;
    rightPlane_y = 3;

    botPlane_x = 1;
    botPlane_y = 3;

    forPlane_x = 1;
    forPlane_y = 3;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;

    loot = -1;
    solid = true;
    allowLight = true;

    name = "Glass";
}
block_t GlassBlock::getID(){ return 40;}

IronOre::IronOre():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 2;

    downPlane_x = 1;
    downPlane_y = 2;

    leftPlane_x = 1;
    leftPlane_y = 2;

    rightPlane_x = 1;
    rightPlane_y = 2;

    botPlane_x = 1;
    botPlane_y = 2;

    forPlane_x = 1;
    forPlane_y = 2;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

    loot = 41;
    material = 7;
    solid = true;

    furnItem = 278;

    name = "Iron Ore";
}
block_t IronOre::getID(){ return 41;}

CoalOre::CoalOre():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 2;

    downPlane_x = 2;
    downPlane_y = 2;

    leftPlane_x = 2;
    leftPlane_y = 2;

    rightPlane_x = 2;
    rightPlane_y = 2;

    botPlane_x = 2;
    botPlane_y = 2;

    forPlane_x = 2;
    forPlane_y = 2;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

	loot = 277;
	material = 7;
	solid = true;

	furnItem = 277;

	name = "Coal Ore";
}
block_t CoalOre::getID(){ return 42;}

DiamondOre::DiamondOre():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 3;

    downPlane_x = 2;
    downPlane_y = 3;

    leftPlane_x = 2;
    leftPlane_y = 3;

    rightPlane_x = 2;
    rightPlane_y = 3;

    botPlane_x = 2;
    botPlane_y = 3;

    forPlane_x = 2;
    forPlane_y = 3;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

	loot = 279;
	material = 7;
	solid = true;

	furnItem = 279;

	name = "Diamond Ore";
}
block_t DiamondOre::getID(){ return 43;}

RedstoneOre::RedstoneOre():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 3;

    downPlane_x = 3;
    downPlane_y = 3;

    leftPlane_x = 3;
    leftPlane_y = 3;

    rightPlane_x = 3;
    rightPlane_y = 3;

    botPlane_x = 3;
    botPlane_y = 3;

    forPlane_x = 3;
    forPlane_y = 3;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

	loot = 353;
	material = 7;
	solid = true;

	furnItem = 353;

    name = "Redstone Ore";
}
block_t RedstoneOre::getID(){ return 44;}

LapisOre::LapisOre():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 10;

    downPlane_x = 0;
    downPlane_y = 10;

    leftPlane_x = 0;
    leftPlane_y = 10;

    rightPlane_x = 0;
    rightPlane_y = 10;

    botPlane_x = 0;
    botPlane_y = 10;

    forPlane_x = 0;
    forPlane_y = 10;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

	loot = 318;
	material = 7;
	solid = true;

	name = "Lapis Lazuli Ore";
}
block_t LapisOre::getID(){ return 45;}

Obsidian::Obsidian():BaseBlock()
{
    upPlane_x = 5;
    upPlane_y = 2;

    downPlane_x = 5;
    downPlane_y = 2;

    leftPlane_x = 5;
    leftPlane_y = 2;

    rightPlane_x = 5;
    rightPlane_y = 2;

    botPlane_x = 5;
    botPlane_y = 2;

    forPlane_x = 5;
    forPlane_y = 2;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

	loot = 46;
	material = 5;
	solid = true;

	name = "Obsidian";
}
block_t Obsidian::getID(){ return 46;}

SnowBlock::SnowBlock():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 4;

    downPlane_x = 2;
    downPlane_y = 4;

    leftPlane_x = 2;
    leftPlane_y = 4;

    rightPlane_x = 2;
    rightPlane_y = 4;

    botPlane_x = 2;
    botPlane_y = 4;

    forPlane_x = 2;
    forPlane_y = 4;

	editable = true;
	transparent = false;
	soundType = 6;

	loot = 47;
	material = 3;
	solid = true;

    name = "Snow Block";
}
block_t SnowBlock::getID(){ return 47;}

Torch::Torch():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 5;

    downPlane_x = 0;
    downPlane_y = 5;

    leftPlane_x = 0;
    leftPlane_y = 5;

    rightPlane_x = 0;
    rightPlane_y = 5;

    botPlane_x = 0;
    botPlane_y = 5;

    forPlane_x = 0;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	soundType = 2;
	lightSource = true;
    blockModel = 5;
    allowLight = true;

    loot = 308;
    solid = false;

    name = "Torch block";

    collideBox = BoundingBox(Vector3(-0.06f,-0.5f,-0.06f),Vector3(0.06f,0.15f,0.06f));
}
block_t Torch::getID(){ return 48;}

static void InitWallTorch(BaseBlock& b)
{
    b.upPlane_x = 0;
    b.upPlane_y = 5;
    b.downPlane_x = 0;
    b.downPlane_y = 5;
    b.leftPlane_x = 0;
    b.leftPlane_y = 5;
    b.rightPlane_x = 0;
    b.rightPlane_y = 5;
    b.botPlane_x = 0;
    b.botPlane_y = 5;
    b.forPlane_x = 0;
    b.forPlane_y = 5;
    b.editable = true;
    b.transparent = true;
    b.soundType = 2;
    b.lightSource = true;
    b.blockModel = 5;
    b.allowLight = true;
    b.loot = 308;
    b.solid = false;
    b.name = "Torch block";
    b.collideBox = BoundingBox(Vector3(-0.06f,-0.5f,-0.06f),Vector3(0.06f,0.15f,0.06f));
}

WallTorch1::WallTorch1():BaseBlock(){ InitWallTorch(*this); }
block_t WallTorch1::getID(){ return 250;}
WallTorch2::WallTorch2():BaseBlock(){ InitWallTorch(*this); }
block_t WallTorch2::getID(){ return 251;}
WallTorch3::WallTorch3():BaseBlock(){ InitWallTorch(*this); }
block_t WallTorch3::getID(){ return 252;}
WallTorch4::WallTorch4():BaseBlock(){ InitWallTorch(*this); }
block_t WallTorch4::getID(){ return 253;}

Door1::Door1():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = true;
    material = 8;
    allowLight = true;

    name = "Door Block";
}
block_t Door1::getID(){ return 49;}

Door2::Door2():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = true;
    material = 8;
    allowLight = true;
}
block_t Door2::getID(){ return 50;}

Door3::Door3():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = true;
    material = 8;
    allowLight = true;

}
block_t Door3::getID(){ return 51;}

Door4::Door4():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = true;
    material = 8;
    allowLight = true;
}
block_t Door4::getID(){ return 52;}

Door5::Door5():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = false;
    material = 8;
    allowLight = true;
}
block_t Door5::getID(){ return 53;}

Door6::Door6():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = false;
    material = 8;
    allowLight = true;
}
block_t Door6::getID(){ return 54;}

Door7::Door7():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = false;
    material = 8;
    allowLight = true;
}
block_t Door7::getID(){ return 55;}

Door8::Door8():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = false;
    material = 8;
    allowLight = true;
}
block_t Door8::getID(){ return 56;}

Door9::Door9():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = true;
    material = 8;
    allowLight = true;
    refraction = 0;
}
block_t Door9::getID(){ return 57;}

Door10::Door10():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 6;

    downPlane_x = 1;
    downPlane_y = 6;

    leftPlane_x = 1;
    leftPlane_y = 6;

    rightPlane_x = 1;
    rightPlane_y = 6;

    botPlane_x = 1;
    botPlane_y = 6;

    forPlane_x = 1;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	soundType = 2;
    blockModel = 5;

    loot = 306;
    solid = false;
    material = 8;
    allowLight = true;
    refraction = 0;
}
block_t Door10::getID(){ return 58;}

WoodenFence::WoodenFence():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 0;

    downPlane_x = 4;
    downPlane_y = 0;

    leftPlane_x = 4;
    leftPlane_y = 0;

    rightPlane_x = 4;
    rightPlane_y = 0;

    botPlane_x = 4;
    botPlane_y = 0;

    forPlane_x = 4;
    forPlane_y = 0;

	editable = true;
	transparent = false;
    allowLight = true;
	solid = true;

    blockModel = 2;
	loot = 59;
	material = 8;
	refraction = 2;
	soundType = 3;

	name = "Oak Fence";

	collideBox = BoundingBox(Vector3(-0.125f,-0.5f,-0.125f),Vector3(0.125f,0.5,0.125f));
}
block_t WoodenFence::getID(){ return 59;}

CobbleStair1::CobbleStair1():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 1;

    downPlane_x = 0;
    downPlane_y = 1;

    leftPlane_x = 0;
    leftPlane_y = 1;

    rightPlane_x = 0;
    rightPlane_y = 1;

    botPlane_x = 0;
    botPlane_y = 1;

    forPlane_x = 0;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    solid = true;
    allowLight = false;

    blockModel = 6;
    loot = 60;
    material = 1;
    soundType = 2;

    name = "Cobblestone Stairs";
}
block_t CobbleStair1::getID(){ return 60;}

CobbleStair2::CobbleStair2():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 1;

    downPlane_x = 0;
    downPlane_y = 1;

    leftPlane_x = 0;
    leftPlane_y = 1;

    rightPlane_x = 0;
    rightPlane_y = 1;

    botPlane_x = 0;
    botPlane_y = 1;

    forPlane_x = 0;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    solid = true;
    allowLight = false;

    blockModel = 7;
    loot = 60;
    material = 1;
    soundType = 2;

    name = "Cobblestone Stairs";
}
block_t CobbleStair2::getID(){ return 61;}

CobbleStair3::CobbleStair3():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 1;

    downPlane_x = 0;
    downPlane_y = 1;

    leftPlane_x = 0;
    leftPlane_y = 1;

    rightPlane_x = 0;
    rightPlane_y = 1;

    botPlane_x = 0;
    botPlane_y = 1;

    forPlane_x = 0;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    solid = true;
    allowLight = false;

    blockModel = 8;
    loot = 60;
    material = 1;
    soundType = 2;

    name = "Cobblestone Stairs";
}
block_t CobbleStair3::getID(){ return 62;}

CobbleStair4::CobbleStair4():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 1;

    downPlane_x = 0;
    downPlane_y = 1;

    leftPlane_x = 0;
    leftPlane_y = 1;

    rightPlane_x = 0;
    rightPlane_y = 1;

    botPlane_x = 0;
    botPlane_y = 1;

    forPlane_x = 0;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    solid = true;
    allowLight = false;

    blockModel = 9;
    loot = 60;
    material = 1;
    soundType = 2;

    name = "Cobblestone Stairs";
}
block_t CobbleStair4::getID(){ return 63;}

Diamond::Diamond():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 1;

    downPlane_x = 8;
    downPlane_y = 1;

    leftPlane_x = 8;
    leftPlane_y = 1;

    rightPlane_x = 8;
    rightPlane_y = 1;

    botPlane_x = 8;
    botPlane_y = 1;

    forPlane_x = 8;
    forPlane_y = 1;

	editable = true;
	transparent = false;
	allowLight = false;

    loot = 64;
    material = 10;
    solid = true;

    name = "Block of Diamond";
}
block_t Diamond::getID(){ return 64;}

Gold::Gold():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 1;

    downPlane_x = 7;
    downPlane_y = 1;

    leftPlane_x = 7;
    leftPlane_y = 1;

    rightPlane_x = 7;
    rightPlane_y = 1;

    botPlane_x = 7;
    botPlane_y = 1;

    forPlane_x = 7;
    forPlane_y = 1;

	editable = true;
	transparent = false;
	allowLight = false;

    loot = 65;
    material = 10;
    solid = true;

    name = "Block of Gold";
}
block_t Gold::getID(){ return 65;}


Iron::Iron():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 1;

    downPlane_x = 6;
    downPlane_y = 1;

    leftPlane_x = 6;
    leftPlane_y = 1;

    rightPlane_x = 6;
    rightPlane_y = 1;

    botPlane_x = 6;
    botPlane_y = 1;

    forPlane_x = 6;
    forPlane_y = 1;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

    loot = 66;
    material = 10;
    solid = true;

    name = "Block of Iron";
}
block_t Iron::getID(){ return 66;}

BrickStair1::BrickStair1():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 0;

    downPlane_x = 7;
    downPlane_y = 0;

    leftPlane_x = 7;
    leftPlane_y = 0;

    rightPlane_x = 7;
    rightPlane_y = 0;

    botPlane_x = 7;
    botPlane_y = 0;

    forPlane_x = 7;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    blockModel = 6;

    loot = 67;
    material = 9;
    solid = true;
    soundType = 2;

    name = "Brick Stairs";
}
block_t BrickStair1::getID(){ return 67;}

BrickStair2::BrickStair2():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 0;

    downPlane_x = 7;
    downPlane_y = 0;

    leftPlane_x = 7;
    leftPlane_y = 0;

    rightPlane_x = 7;
    rightPlane_y = 0;

    botPlane_x = 7;
    botPlane_y = 0;

    forPlane_x = 7;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    blockModel = 7;
    soundType = 2;

    loot = 67;
    material = 9;
    solid = true;

    name = "Brick Stairs";
}
block_t BrickStair2::getID(){ return 68;}

BrickStair3::BrickStair3():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 0;

    downPlane_x = 7;
    downPlane_y = 0;

    leftPlane_x = 7;
    leftPlane_y = 0;

    rightPlane_x = 7;
    rightPlane_y = 0;

    botPlane_x = 7;
    botPlane_y = 0;

    forPlane_x = 7;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    blockModel = 8;
    soundType = 2;

    loot = 67;
    material = 9;
    solid = true;

    name = "Brick Stairs";
}
block_t BrickStair3::getID(){ return 69;}

BrickStair4::BrickStair4():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 0;

    downPlane_x = 7;
    downPlane_y = 0;

    leftPlane_x = 7;
    leftPlane_y = 0;

    rightPlane_x = 7;
    rightPlane_y = 0;

    botPlane_x = 7;
    botPlane_y = 0;

    forPlane_x = 7;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    blockModel = 9;
    soundType = 2;

    loot = 67;
    material = 9;
    solid = true;

    name = "Brick Stairs";
}
block_t BrickStair4::getID(){ return 70;}

NoteBlock::NoteBlock():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 4;

    downPlane_x = 10;
    downPlane_y = 4;

    leftPlane_x = 10;
    leftPlane_y = 4;

    rightPlane_x = 10;
    rightPlane_y = 4;

    botPlane_x = 10;
    botPlane_y = 4;

    forPlane_x = 10;
    forPlane_y = 4;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    soundType = 3;
    loot = 71;
    material = 1;

    name = "Note Block";
}
block_t NoteBlock::getID(){ return 71;}

JukeBox::JukeBox():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 4;

    downPlane_x = 10;
    downPlane_y = 4;

    leftPlane_x = 10;
    leftPlane_y = 4;

    rightPlane_x = 10;
    rightPlane_y = 4;

    botPlane_x = 10;
    botPlane_y = 4;

    forPlane_x = 10;
    forPlane_y = 4;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    soundType = 3;
    material = 1;

    name = "Jukebox";
}
block_t JukeBox::getID(){ return 72;}


Pumpkin1::Pumpkin1():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 6;

    downPlane_x = 6;
    downPlane_y = 7;

    leftPlane_x = 7;
    leftPlane_y = 7;

    rightPlane_x = 6;
    rightPlane_y = 7;

    botPlane_x = 6;
    botPlane_y = 7;

    forPlane_x = 6;
    forPlane_y = 7;

    editable = true;
    transparent = false;
    allowLight = false;
    soundType = 3;

    loot = 75;
    material = 1;
    solid = true;

    name = "Pumpkin";
}
block_t Pumpkin1::getID(){ return 73;}

Pumpkin2::Pumpkin2():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 6;

    downPlane_x = 6;
    downPlane_y = 7;

    leftPlane_x = 6;
    leftPlane_y = 7;

    rightPlane_x = 7;
    rightPlane_y = 7;

    botPlane_x = 6;
    botPlane_y = 7;

    forPlane_x = 6;
    forPlane_y = 7;

    editable = true;
    transparent = false;
    allowLight = false;
    soundType = 3;

    loot = 75;
    material = 1;
    solid = true;

    name = "Pumpkin";
}
block_t Pumpkin2::getID(){ return 74;}

Pumpkin3::Pumpkin3():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 6;

    downPlane_x = 6;
    downPlane_y = 7;

    leftPlane_x = 6;
    leftPlane_y = 7;

    rightPlane_x = 6;
    rightPlane_y = 7;

    botPlane_x = 7;
    botPlane_y = 7;

    forPlane_x = 6;
    forPlane_y = 7;

    editable = true;
    transparent = false;
    allowLight = false;
    soundType = 3;

    loot = 75;
    material = 1;
    solid = true;

    name = "Pumpkin";
}
block_t Pumpkin3::getID(){ return 75;}

Pumpkin4::Pumpkin4():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 6;

    downPlane_x = 6;
    downPlane_y = 7;

    leftPlane_x = 6;
    leftPlane_y = 7;

    rightPlane_x = 6;
    rightPlane_y = 7;

    botPlane_x = 6;
    botPlane_y = 7;

    forPlane_x = 7;
    forPlane_y = 7;

    editable = true;
    transparent = false;
    allowLight = false;
    soundType = 3;

    loot = 75;
    material = 1;
    solid = true;

    name = "Pumpkin";
}
block_t Pumpkin4::getID(){ return 76;}

Mooshroom1::Mooshroom1():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 1;

    downPlane_x = 12;
    downPlane_y = 1;

    leftPlane_x = 12;
    leftPlane_y = 1;

    rightPlane_x = 12;
    rightPlane_y = 1;

    botPlane_x = 12;
    botPlane_y = 1;

    forPlane_x = 12;
    forPlane_y = 1;

	editable = true;
	transparent = true;
    blockModel = 3;

    loot = 300;
    solid = false;
    update = 1;
    allowLight = true;
    material = 11;
    soundType = 0;

    collideBox = BoundingBox(Vector3(-0.2f,-0.5f,-0.2f),Vector3(0.2f,-0.1,0.2f));
}
block_t Mooshroom1::getID(){ return 77;}

Mooshroom2::Mooshroom2():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 1;

    downPlane_x = 13;
    downPlane_y = 1;

    leftPlane_x = 13;
    leftPlane_y = 1;

    rightPlane_x = 13;
    rightPlane_y = 1;

    botPlane_x = 13;
    botPlane_y = 1;

    forPlane_x = 13;
    forPlane_y = 1;

	editable = true;
	transparent = true;
    blockModel = 3;

    loot = 301;
    solid = false;
    update = 1;
    allowLight = true;
    material = 11;
    soundType = 0;

    collideBox = BoundingBox(Vector3(-0.2f,-0.5f,-0.2f),Vector3(0.2f,-0.1,0.2f));
}
block_t Mooshroom2::getID(){ return 78;}

Soil::Soil():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 5;

    downPlane_x = 2;
    downPlane_y = 0;

    leftPlane_x = 2;
    leftPlane_y = 0;

    rightPlane_x = 2;
    rightPlane_y = 0;

    botPlane_x = 2;
    botPlane_y = 0;

    forPlane_x = 2;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
	blockModel = 12;
    soundType = 1;

    loot = 2;
    material = 3;
    update = 1;
    solid = true;
	collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0.45,0.5f));
}
block_t Soil::getID(){ return 79;}

WateredSoil::WateredSoil():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 5;

    downPlane_x = 2;
    downPlane_y = 0;

    leftPlane_x = 2;
    leftPlane_y = 0;

    rightPlane_x = 2;
    rightPlane_y = 0;

    botPlane_x = 2;
    botPlane_y = 0;

    forPlane_x = 2;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
	blockModel = 12;
    soundType = 1;

    loot = 2;
    material = 3;
    update = 1;
    solid = true;
	collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0.45,0.5f));
}
block_t WateredSoil::getID(){ return 80;}

MelonBlock::MelonBlock():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 6;

    downPlane_x = 14;
    downPlane_y = 6;

    leftPlane_x = 14;
    leftPlane_y = 6;

    rightPlane_x = 14;
    rightPlane_y = 6;

    botPlane_x = 14;
    botPlane_y = 6;

    forPlane_x = 14;
    forPlane_y = 6;

    editable = true;
    allowLight = false;
    transparent = false;
    soundType = 3;

    loot = 311;
    solid = true;
    material = 1;

    name = "Melon";
}
block_t MelonBlock::getID(){ return 81;}

OakHalfBlock::OakHalfBlock():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 0;

    downPlane_x = 4;
    downPlane_y = 0;

    leftPlane_x = 4;
    leftPlane_y = 0;

    rightPlane_x = 4;
    rightPlane_y = 0;

    botPlane_x = 4;
    botPlane_y = 0;

    forPlane_x = 4;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    solid = true;
    allowLight = true;

    blockModel = 1;
    loot = 82;
    material = 1;
    soundType = 3;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0,0.5f));

    name = "Oak Wood Slab";
}
block_t OakHalfBlock::getID(){ return 82;}

CobbleHalfBlock::CobbleHalfBlock():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 1;

    downPlane_x = 0;
    downPlane_y = 1;

    leftPlane_x = 0;
    leftPlane_y = 1;

    rightPlane_x = 0;
    rightPlane_y = 1;

    botPlane_x = 0;
    botPlane_y = 1;

    forPlane_x = 0;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    allowLight = true;
    solid = true;

    blockModel = 1;
    loot = 83;
    material = 2;
    soundType = 2;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0,0.5f));

    name = "Cobblestone Slab";
}
block_t CobbleHalfBlock::getID(){ return 83;}

BrickHalfBlock::BrickHalfBlock():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 0;

    downPlane_x = 7;
    downPlane_y = 0;

    leftPlane_x = 7;
    leftPlane_y = 0;

    rightPlane_x = 7;
    rightPlane_y = 0;

    botPlane_x = 7;
    botPlane_y = 0;

    forPlane_x = 7;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = true;

    soundType = 2;
    blockModel = 1;

    loot = 84;
    material = 9;
    solid = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0,0.5f));

    name = "Bricks Slab";
}
block_t BrickHalfBlock::getID(){ return 84;}

Slab::Slab():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 0;

    downPlane_x = 6;
    downPlane_y = 0;

    leftPlane_x = 5;
    leftPlane_y = 0;

    rightPlane_x = 5;
    rightPlane_y = 0;

    botPlane_x = 5;
    botPlane_y = 0;

    forPlane_x = 5;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    blockModel = 1;
    lightSource = false;
    allowLight = true;
    soundType = 2;

    loot = 85;
    material = 2;
    solid = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0,0.5f));

    name = "Stone Slab";
}
block_t Slab::getID(){ return 85;}

StoneSandHalfBlock::StoneSandHalfBlock():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 11;

    downPlane_x = 0;
    downPlane_y = 13;

    leftPlane_x = 0;
    leftPlane_y = 12;

    rightPlane_x = 0;
    rightPlane_y = 12;

    botPlane_x = 0;
    botPlane_y = 12;

    forPlane_x = 0;
    forPlane_y = 12;

    editable = true;
    transparent = false;
    blockModel = 1;
    lightSource = false;
    allowLight = true;

    loot = 86;
    material = 2;
    solid = true;
    soundType = 2;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0,0.5f));

    name = "Sandstone Slab";
}
block_t StoneSandHalfBlock::getID(){ return 86;}

Ladder1::Ladder1():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 5;

    downPlane_x = 3;
    downPlane_y = 5;

    leftPlane_x = 3;
    leftPlane_y = 5;

    rightPlane_x = 3;
    rightPlane_y = 5;

    botPlane_x = 3;
    botPlane_y = 5;

    forPlane_x = 3;
    forPlane_y = 5;

    blockModel = 5;

    refraction = 0;
    loot = 307;

    lightSource = false;
    solid = false;
    allowLight = true;
    editable = true;
    transparent = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(-0.375f,0.5f,0.5f));
}
block_t Ladder1::getID(){ return 87;}

Ladder2::Ladder2():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 5;

    downPlane_x = 3;
    downPlane_y = 5;

    leftPlane_x = 3;
    leftPlane_y = 5;

    rightPlane_x = 3;
    rightPlane_y = 5;

    botPlane_x = 3;
    botPlane_y = 5;

    forPlane_x = 3;
    forPlane_y = 5;

    blockModel = 5;

    refraction = 0;
    loot = 307;

    lightSource = false;
    solid = false;
    allowLight = true;
    editable = true;
    transparent = true;

    collideBox = BoundingBox(Vector3(0.375f,-0.5,-0.5f),Vector3(0.5f,0.5f,0.5f));
}
block_t Ladder2::getID(){ return 88;}

Ladder3::Ladder3():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 5;

    downPlane_x = 3;
    downPlane_y = 5;

    leftPlane_x = 3;
    leftPlane_y = 5;

    rightPlane_x = 3;
    rightPlane_y = 5;

    botPlane_x = 3;
    botPlane_y = 5;

    forPlane_x = 3;
    forPlane_y = 5;

    blockModel = 5;

    refraction = 0;
    loot = 307;

    lightSource = false;
    solid = false;
    allowLight = true;
    editable = true;
    transparent = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,0.5f,-0.375f));
}
block_t Ladder3::getID(){ return 89;}

Ladder4::Ladder4():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 5;

    downPlane_x = 3;
    downPlane_y = 5;

    leftPlane_x = 3;
    leftPlane_y = 5;

    rightPlane_x = 3;
    rightPlane_y = 5;

    botPlane_x = 3;
    botPlane_y = 5;

    forPlane_x = 3;
    forPlane_y = 5;

    blockModel = 5;

    refraction = 0;
    loot = 307;

    lightSource = false;
    solid = false;
    allowLight = true;
    editable = true;
    transparent = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5,0.375f),Vector3(0.5f,0.5f,0.5f));
}
block_t Ladder4::getID(){ return 90;}

DoubleSlab::DoubleSlab():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 0;

    downPlane_x = 6;
    downPlane_y = 0;

    leftPlane_x = 5;
    leftPlane_y = 0;

    rightPlane_x = 5;
    rightPlane_y = 0;

    botPlane_x = 5;
    botPlane_y = 0;

    forPlane_x = 5;
    forPlane_y = 0;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

    loot = 85;
    material = 2;
    solid = true;

    name = "Double Slab";
}
block_t DoubleSlab::getID(){ return 91;}

MossyCobblestone::MossyCobblestone():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 2;

    downPlane_x = 4;
    downPlane_y = 2;

    leftPlane_x = 4;
    leftPlane_y = 2;

    rightPlane_x = 4;
    rightPlane_y = 2;

    botPlane_x = 4;
    botPlane_y = 2;

    forPlane_x = 4;
    forPlane_y = 2;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

    loot = 36;
    material = 2;
    solid = true;

    name = "Moss Stone";
}
block_t MossyCobblestone::getID(){ return 92;}

Snow2::Snow2():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 4;

    downPlane_x = 2;
    downPlane_y = 4;

    leftPlane_x = 2;
    leftPlane_y = 4;

    rightPlane_x = 2;
    rightPlane_y = 4;

    botPlane_x = 2;
    botPlane_y = 4;

    forPlane_x = 2;
    forPlane_y = 4;

    editable = true;
    transparent = false;
    blockModel = 5;
    lightSource = false;
    allowLight = true;

    loot = -1;
    solid = false;
    refraction = 2;
    soundType = 6;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));

    name = "Snow";
}
block_t Snow2::getID(){ return 93;}

ChestBlock1::ChestBlock1():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 1;

    downPlane_x = 9;
    downPlane_y = 1;

    leftPlane_x = 11;
    leftPlane_y = 1;

    rightPlane_x = 10;
    rightPlane_y = 1;

    botPlane_x = 10;
    botPlane_y = 1;

    forPlane_x = 10;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = true;
    solid = true;

    loot = 96;
    material = 8;
	refraction = 2;
	blockModel = 5;
	soundType = 3;

	collideBox = BoundingBox(Vector3(-0.4375f,-0.5f,-0.4375f),Vector3(0.4375f,0.375f,0.4375f));

	name = "Chest";
}
block_t ChestBlock1::getID(){ return 94;}

ChestBlock2::ChestBlock2():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 1;

    downPlane_x = 9;
    downPlane_y = 1;

    leftPlane_x = 10;
    leftPlane_y = 1;

    rightPlane_x = 11;
    rightPlane_y = 1;

    botPlane_x = 10;
    botPlane_y = 1;

    forPlane_x = 10;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    lightSource = false;
    solid = true;
    allowLight = true;

    loot = 96;
    material = 8;
    blockModel = 5;
    refraction = 2;
    soundType = 3;

    collideBox = BoundingBox(Vector3(-0.4375f,-0.5f,-0.4375f),Vector3(0.4375f,0.375f,0.4375f));

    name = "Chest";
}
block_t ChestBlock2::getID(){ return 95;}

ChestBlock3::ChestBlock3():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 1;

    downPlane_x = 9;
    downPlane_y = 1;

    leftPlane_x = 10;
    leftPlane_y = 1;

    rightPlane_x = 10;
    rightPlane_y = 1;

    botPlane_x = 11;
    botPlane_y = 1;

    forPlane_x = 10;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    lightSource = false;
    solid = true;
    allowLight = true;

    loot = 96;
    material = 8;
    refraction = 2;
    blockModel = 5;
    soundType = 3;

    collideBox = BoundingBox(Vector3(-0.4375f,-0.5f,-0.4375f),Vector3(0.4375f,0.375f,0.4375f));

    name = "Chest";
}
block_t ChestBlock3::getID(){ return 96;}

ChestBlock4::ChestBlock4():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 1;

    downPlane_x = 9;
    downPlane_y = 1;

    leftPlane_x = 10;
    leftPlane_y = 1;

    rightPlane_x = 10;
    rightPlane_y = 1;

    botPlane_x = 10;
    botPlane_y = 1;

    forPlane_x = 11;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    lightSource = false;
    solid = true;
    allowLight = true;

    loot = 96;
    material = 8;
    refraction = 2;
    blockModel = 5;
    soundType = 3;

    collideBox = BoundingBox(Vector3(-0.4375f,-0.5f,-0.4375f),Vector3(0.4375f,0.375f,0.4375f));

    name = "Chest";
}
block_t ChestBlock4::getID(){ return 97;}

ChestLock::ChestLock():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 2;

    downPlane_x = 8;
    downPlane_y = 2;

    leftPlane_x = 8;
    leftPlane_y = 2;

    rightPlane_x = 8;
    rightPlane_y = 2;

    botPlane_x = 8;
    botPlane_y = 2;

    forPlane_x = 8;
    forPlane_y = 2;

    // A locked chest is still a chest: it can be looked at, broken, and built against. `editable`
    // defaults to false, and left there the outline passes straight through it and the pick ray never
    // reports it -- nothing generates one today, but a save can hold the id.
    editable = true;
}
block_t ChestLock::getID(){ return 98;}

ClayBlock::ClayBlock():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 4;

    downPlane_x = 8;
    downPlane_y = 4;

    leftPlane_x = 8;
    leftPlane_y = 4;

    rightPlane_x = 8;
    rightPlane_y = 4;

    botPlane_x = 8;
    botPlane_y = 4;

    forPlane_x = 8;
    forPlane_y = 4;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    soundType = 1;

    loot = 283;
    material = 3;
    solid = true;
}
block_t ClayBlock::getID(){ return 99;}

Chest5::Chest5():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 0;

    downPlane_x = 2;
    downPlane_y = 0;

    leftPlane_x = 3;
    leftPlane_y = 0;

    rightPlane_x = 3;
    rightPlane_y = 0;

    botPlane_x = 3;
    botPlane_y = 0;

    forPlane_x = 3;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    solid = true;

    loot = 100;
    material = 8;
}
block_t Chest5::getID(){ return 100;}

LapisBlock::LapisBlock():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 9;

    downPlane_x = 0;
    downPlane_y = 9;

    leftPlane_x = 0;
    leftPlane_y = 9;

    rightPlane_x = 0;
    rightPlane_y = 9;

    botPlane_x = 0;
    botPlane_y = 9;

    forPlane_x = 0;
    forPlane_y = 9;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;

    loot = 101;
    material = 2;
    soundType = 2;

    name = "Lapis Lazuli Block";
}
block_t LapisBlock::getID(){ return 101;}

StoneBrick::StoneBrick():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 2;

    downPlane_x = 9;
    downPlane_y = 2;

    leftPlane_x = 9;
    leftPlane_y = 2;

    rightPlane_x = 9;
    rightPlane_y = 2;

    botPlane_x = 9;
    botPlane_y = 2;

    forPlane_x = 9;
    forPlane_y = 2;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;

    loot = 102;
    material = 9;
    furnItem = 103;
    soundType = 2;

    name = "Stone Bricks";
}
block_t StoneBrick::getID(){ return 102;}

CrackedStoneBrick::CrackedStoneBrick():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 2;

    downPlane_x = 10;
    downPlane_y = 2;

    leftPlane_x = 10;
    leftPlane_y = 2;

    rightPlane_x = 10;
    rightPlane_y = 2;

    botPlane_x = 10;
    botPlane_y = 2;

    forPlane_x = 10;
    forPlane_y = 2;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;

    loot = 103;
    material = 9;
    soundType = 2;

    name = "Cracked Stone Bricks";
}
block_t CrackedStoneBrick::getID(){ return 103;}

CarvedStoneBrick::CarvedStoneBrick():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 3;

    downPlane_x = 9;
    downPlane_y = 3;

    leftPlane_x = 9;
    leftPlane_y = 3;

    rightPlane_x = 9;
    rightPlane_y = 3;

    botPlane_x = 9;
    botPlane_y = 3;

    forPlane_x = 9;
    forPlane_y = 3;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;

    soundType = 2;
    loot = 104;
    material = 9;


    name = "Chiseled Stone Bricks";
}
block_t CarvedStoneBrick::getID(){ return 104;}

CraftingTable::CraftingTable():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 2;

    downPlane_x = 11;
    downPlane_y = 2;

    leftPlane_x = 12;
    leftPlane_y = 3;

    rightPlane_x = 11;
    rightPlane_y = 3;

    botPlane_x = 12;
    botPlane_y = 3;

    forPlane_x = 12;
    forPlane_y = 3;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;

    loot = 105;
    material = 8;
    soundType = 3;

    name = "Crafting Table";
}
block_t CraftingTable::getID(){ return 105;}

FurnaceOff::FurnaceOff():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 3;

    downPlane_x = 14;
    downPlane_y = 3;

    leftPlane_x = 13;
    leftPlane_y = 2;

    rightPlane_x = 13;
    rightPlane_y = 2;

    botPlane_x = 13;
    botPlane_y = 2;

    forPlane_x = 13;
    forPlane_y = 2;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;

    loot = 106;
    material = 9;
    soundType = 2;

    name = "Furnace";
}
block_t FurnaceOff::getID(){ return 106;}

FurnaceOn::FurnaceOn():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 3;

    downPlane_x = 14;
    downPlane_y = 3;

    leftPlane_x = 13;
    leftPlane_y = 2;

    rightPlane_x = 13;
    rightPlane_y = 2;

    botPlane_x = 13;
    botPlane_y = 2;

    forPlane_x = 13;
    forPlane_y = 2;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;
    lightSource = true;

    loot = 106;
    material = 9;
    soundType = 2;

    name = "Furnace";
}
block_t FurnaceOn::getID(){ return 107;}

HalfStoneBrick::HalfStoneBrick():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 2;

    downPlane_x = 9;
    downPlane_y = 2;

    leftPlane_x = 9;
    leftPlane_y = 2;

    rightPlane_x = 9;
    rightPlane_y = 2;

    botPlane_x = 9;
    botPlane_y = 2;

    forPlane_x = 9;
    forPlane_y = 2;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = true;
    solid = true;

    loot = 108;
    material = 9;
    blockModel = 1;
    soundType = 2;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0,0.5f));

    name = "Stobe Bricks Slab";
}
block_t HalfStoneBrick::getID(){ return 108;}

Spawner::Spawner():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 4;

    downPlane_x = 1;
    downPlane_y = 4;

    leftPlane_x = 1;
    leftPlane_y = 4;

    rightPlane_x = 1;
    rightPlane_y = 4;

    botPlane_x = 1;
    botPlane_y = 4;

    forPlane_x = 1;
    forPlane_y = 4;

    solid = true;
	editable = true;
	transparent = true;
	allowLight = true;

	soundType = 2;
	loot = -1;
	material = 5;
	update = 0;

	name = "Monster Spawner";
}
block_t Spawner::getID(){ return 109;}

TNTBlock::TNTBlock():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 0;

    downPlane_x = 10;
    downPlane_y = 0;

    leftPlane_x = 8;
    leftPlane_y = 0;

    rightPlane_x = 8;
    rightPlane_y = 0;

    botPlane_x = 8;
    botPlane_y = 0;

    forPlane_x = 8;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    lightSource = false;
    solid = true;
    loot = 110;

    name = "TNT";
}
block_t TNTBlock::getID(){ return 110;}

Nuclear::Nuclear():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 0;

    downPlane_x = 2;
    downPlane_y = 0;

    leftPlane_x = 3;
    leftPlane_y = 0;

    rightPlane_x = 3;
    rightPlane_y = 0;

    botPlane_x = 3;
    botPlane_y = 0;

    forPlane_x = 3;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;
}
block_t Nuclear::getID(){ return 111;}

Lava::Lava():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 15;

    downPlane_x = 15;
    downPlane_y = 15;

    leftPlane_x = 15;
    leftPlane_y = 15;

    rightPlane_x = 15;
    rightPlane_y = 15;

    botPlane_x = 15;
    botPlane_y = 15;

    forPlane_x = 15;
    forPlane_y = 15;

    editable = false;
	transparent = false;
	lightSource = true;
    allowLight = true;
    solid = false;
    animated = true;

    blockModel = 5;
	soundType = 2;

	name = "Lava Block";
}
block_t Lava::getID(){ return 112;}

Gravel::Gravel():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 1;

    downPlane_x = 3;
    downPlane_y = 1;

    leftPlane_x = 3;
    leftPlane_y = 1;

    rightPlane_x = 3;
    rightPlane_y = 1;

    botPlane_x = 3;
    botPlane_y = 1;

    forPlane_x = 3;
    forPlane_y = 1;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;

    soundType = 1;
    update = 1;
    loot = 113;
    material = 3;

    name = "Gravel";
}
block_t Gravel::getID(){ return 113;}

GlassPanel::GlassPanel():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 13;

    downPlane_x = 15;
    downPlane_y = 13;

    leftPlane_x = 1;
    leftPlane_y = 3;

    rightPlane_x = 15;
    rightPlane_y = 12;

    botPlane_x = 15;
    botPlane_y = 12;

    forPlane_x = 1;
    forPlane_y = 3;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
	loot = -1;
    blockModel = 10;

    name = "Glass Panel";
}
block_t GlassPanel::getID(){ return 114;}

IronBars::IronBars():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 13;

    downPlane_x = 14;
    downPlane_y = 13;

    leftPlane_x = 5;
    leftPlane_y = 5;

    rightPlane_x = 14;
    rightPlane_y = 12;

    botPlane_x = 14;
    botPlane_y = 12;

    forPlane_x = 5;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = 350;
    blockModel = 10;
    material = 10;

    name = "Iron Bars";
}
block_t IronBars::getID(){ return 115;}

TallGrass::TallGrass():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 2;

    downPlane_x = 7;
    downPlane_y = 2;

    leftPlane_x = 7;
    leftPlane_y = 2;

    rightPlane_x = 7;
    rightPlane_y = 2;

    botPlane_x = 7;
    botPlane_y = 2;

    forPlane_x = 7;
    forPlane_y = 2;

	editable = true;
	transparent = true;
    blockModel = 3;
    
    soundType = 0;
    solid = false;
    allowLight = true;
    refraction = 0;
    material = 11;
}
block_t TallGrass::getID(){ return 116;}

Fern::Fern():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 3;

    downPlane_x = 8;
    downPlane_y = 3;

    leftPlane_x = 8;
    leftPlane_y = 3;

    rightPlane_x = 8;
    rightPlane_y = 3;

    botPlane_x = 8;
    botPlane_y = 3;

    forPlane_x = 8;
    forPlane_y = 3;

	editable = true;
	transparent = true;
    blockModel = 3;

    solid = false;
    allowLight = true;
    refraction = 0;
    material = 11;
    soundType = 0;

    collideBox = BoundingBox(Vector3(-0.2f,-0.5f,-0.2f),Vector3(0.2f,0.35,0.2f));
}
block_t Fern::getID(){ return 117;}

DrySapling::DrySapling():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 3;

    downPlane_x = 7;
    downPlane_y = 3;

    leftPlane_x = 7;
    leftPlane_y = 3;

    rightPlane_x = 7;
    rightPlane_y = 3;

    botPlane_x = 7;
    botPlane_y = 3;

    forPlane_x = 7;
    forPlane_y = 3;

	editable = true;
	transparent = true;
    blockModel = 3;

    solid = false;
    allowLight = true;
    refraction = 0;
    material = 11;
    soundType = 0;

    collideBox = BoundingBox(Vector3(-0.2f,-0.5f,-0.2f),Vector3(0.2f,0.35,0.2f));
}
block_t DrySapling::getID(){ return 118;}

Flower1::Flower1():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 0;

    downPlane_x = 12;
    downPlane_y = 0;

    leftPlane_x = 12;
    leftPlane_y = 0;

    rightPlane_x = 12;
    rightPlane_y = 0;

    botPlane_x = 12;
    botPlane_y = 0;

    forPlane_x = 12;
    forPlane_y = 0;

	editable = true;
	transparent = true;
    blockModel = 3;

    loot = 294;
    solid = false;
    allowLight = true;
    refraction = 0;
    material = 11;
    soundType = 0;

    collideBox = BoundingBox(Vector3(-0.2f,-0.5f,-0.2f),Vector3(0.2f,0.15,0.2f));
}
block_t Flower1::getID(){ return 119;}

Flower2::Flower2():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 0;

    downPlane_x = 13;
    downPlane_y = 0;

    leftPlane_x = 13;
    leftPlane_y = 0;

    rightPlane_x = 13;
    rightPlane_y = 0;

    botPlane_x = 13;
    botPlane_y = 0;

    forPlane_x = 13;
    forPlane_y = 0;

	editable = true;
	transparent = true;
    blockModel = 3;

    loot = 295;
    solid = false;
    allowLight = true;
    refraction = 0;
    material = 11;
    soundType = 0;

    collideBox = BoundingBox(Vector3(-0.2f,-0.5f,-0.2f),Vector3(0.2f,0.15,0.2f));
}
block_t Flower2::getID(){ return 120;}


OakSapling::OakSapling():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 0;

    downPlane_x = 15;
    downPlane_y = 0;

    leftPlane_x = 15;
    leftPlane_y = 0;

    rightPlane_x = 15;
    rightPlane_y = 0;

    botPlane_x = 15;
    botPlane_y = 0;

    forPlane_x = 15;
    forPlane_y = 0;

    editable = true;
    transparent = true;
    blockModel = 3;

    loot = 296;
    update = true;
    solid = false;
    allowLight = true;
    refraction = 0;
    material = 11;
    soundType = 0;

    collideBox = BoundingBox(Vector3(-0.2f,-0.5f,-0.2f),Vector3(0.2f,0.35,0.2f));
}
block_t OakSapling::getID(){ return 121;}

SpruceSapling::SpruceSapling():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 0;

    downPlane_x = 14;
    downPlane_y = 0;

    leftPlane_x = 14;
    leftPlane_y = 0;

    rightPlane_x = 14;
    rightPlane_y = 0;

    botPlane_x = 14;
    botPlane_y = 0;

    forPlane_x = 14;
    forPlane_y = 0;

    editable = true;
    transparent = true;
    blockModel = 3;

    loot = 309;
    update = true;
    solid = false;
    allowLight = true;
    refraction = 0;
    material = 11;
    soundType = 0;

    collideBox = BoundingBox(Vector3(-0.2f,-0.5f,-0.2f),Vector3(0.2f,0.35,0.2f));
}
block_t SpruceSapling::getID(){ return 122;}

BirchSapling::BirchSapling():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 1;

    downPlane_x = 14;
    downPlane_y = 1;

    leftPlane_x = 14;
    leftPlane_y = 1;

    rightPlane_x = 14;
    rightPlane_y = 1;

    botPlane_x = 14;
    botPlane_y = 1;

    forPlane_x = 14;
    forPlane_y = 1;

    editable = true;
    transparent = true;
    blockModel = 3;

    loot = 310;
    update = true;
    solid = false;
    allowLight = true;
    refraction = 0;
    material = 11;
    soundType = 0;

    collideBox = BoundingBox(Vector3(-0.2f,-0.5f,-0.2f),Vector3(0.2f,0.35,0.2f));
}
block_t BirchSapling::getID(){ return 123;}

FreeBlock5::FreeBlock5():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 0;

    downPlane_x = 2;
    downPlane_y = 0;

    leftPlane_x = 3;
    leftPlane_y = 0;

    rightPlane_x = 3;
    rightPlane_y = 0;

    botPlane_x = 3;
    botPlane_y = 0;

    forPlane_x = 3;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    soundType = 3;

    material = 1;
    loot = 74;
    solid = true;

}
block_t FreeBlock5::getID(){ return 124;}

WoodenStair1::WoodenStair1():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 0;

    downPlane_x = 4;
    downPlane_y = 0;

    leftPlane_x = 4;
    leftPlane_y = 0;

    rightPlane_x = 4;
    rightPlane_y = 0;

    botPlane_x = 4;
    botPlane_y = 0;

    forPlane_x = 4;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    blockModel = 6;
    soundType = 3;

    loot = 125;
    material = 1;
    solid = true;

    name = "Oak Wood Stairs";
}
block_t WoodenStair1::getID(){ return 125;}

WoodenStair2::WoodenStair2():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 0;

    downPlane_x = 4;
    downPlane_y = 0;

    leftPlane_x = 4;
    leftPlane_y = 0;

    rightPlane_x = 4;
    rightPlane_y = 0;

    botPlane_x = 4;
    botPlane_y = 0;

    forPlane_x = 4;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    blockModel = 7;
    soundType = 3;

    loot = 125;
    material = 1;
    solid = true;

    name = "Oak Wood Stairs";
}
block_t WoodenStair2::getID(){ return 126;}

WoodenStair3::WoodenStair3():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 0;

    downPlane_x = 4;
    downPlane_y = 0;

    leftPlane_x = 4;
    leftPlane_y = 0;

    rightPlane_x = 4;
    rightPlane_y = 0;

    botPlane_x = 4;
    botPlane_y = 0;

    forPlane_x = 4;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    blockModel = 8;
    soundType = 3;

    loot = 125;
    material = 1;
    solid = true;

    name = "Oak Wood Stairs";
}
block_t WoodenStair3::getID(){ return 127;}

WoodenStair4::WoodenStair4():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 0;

    downPlane_x = 4;
    downPlane_y = 0;

    leftPlane_x = 4;
    leftPlane_y = 0;

    rightPlane_x = 4;
    rightPlane_y = 0;

    botPlane_x = 4;
    botPlane_y = 0;

    forPlane_x = 4;
    forPlane_y = 0;

    editable = true;
    transparent = false;
    allowLight = false;
    blockModel = 9;
    soundType = 3;

    loot = 125;
    material = 1;
    solid = true;

    name = "Oak Wood Stairs";
}
block_t WoodenStair4::getID(){ return 128;}

MelonPlant4Reverse::MelonPlant4Reverse():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 7;

    downPlane_x = 13;
    downPlane_y = 7;

    leftPlane_x = 13;
    leftPlane_y = 7;

    rightPlane_x = 13;
    rightPlane_y = 7;

    botPlane_x = 13;
    botPlane_y = 7;

    forPlane_x = 13;
    forPlane_y = 7;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 5;
	material = 11;
}
block_t MelonPlant4Reverse::getID(){ return 129;}

MelonPlant1::MelonPlant1():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 6;

    downPlane_x = 10;
    downPlane_y = 6;

    leftPlane_x = 10;
    leftPlane_y = 6;

    rightPlane_x = 10;
    rightPlane_y = 6;

    botPlane_x = 10;
    botPlane_y = 6;

    forPlane_x = 10;
    forPlane_y = 6;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 3;
	material = 11;
	collideBox = BoundingBox(Vector3(-0.5f,-0.6f,-0.5f),Vector3(0.5f,-0.3,0.5f));
}
block_t MelonPlant1::getID(){ return 130;}

MelonPlant2::MelonPlant2():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 6;

    downPlane_x = 11;
    downPlane_y = 6;

    leftPlane_x = 11;
    leftPlane_y = 6;

    rightPlane_x = 11;
    rightPlane_y = 6;

    botPlane_x = 11;
    botPlane_y = 6;

    forPlane_x = 11;
    forPlane_y = 6;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 3;
	material = 11;
	collideBox = BoundingBox(Vector3(-0.5f,-0.6f,-0.5f),Vector3(0.5f,-0.1,0.5f));
}
block_t MelonPlant2::getID(){ return 131;}

MelonPlant3::MelonPlant3():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 6;

    downPlane_x = 12;
    downPlane_y = 6;

    leftPlane_x = 12;
    leftPlane_y = 6;

    rightPlane_x = 12;
    rightPlane_y = 6;

    botPlane_x = 12;
    botPlane_y = 6;

    forPlane_x = 12;
    forPlane_y = 6;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 3;
	material = 11;
	collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,0.4,0.5f));
}
block_t MelonPlant3::getID(){ return 132;}

MelonPlant4::MelonPlant4():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 6;

    downPlane_x = 13;
    downPlane_y = 6;

    leftPlane_x = 13;
    leftPlane_y = 6;

    rightPlane_x = 13;
    rightPlane_y = 6;

    botPlane_x = 13;
    botPlane_y = 6;

    forPlane_x = 13;
    forPlane_y = 6;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 5;
	material = 11;
	collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,0.5,0.5f));
}
block_t MelonPlant4::getID(){ return 133;}


WheatBlock1::WheatBlock1():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 5;

    downPlane_x = 8;
    downPlane_y = 5;

    leftPlane_x = 8;
    leftPlane_y = 5;

    rightPlane_x = 8;
    rightPlane_y = 5;

    botPlane_x = 8;
    botPlane_y = 5;

    forPlane_x = 8;
    forPlane_y = 5;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 286;
    update = 1;
    solid = false;
    allowLight = true;
	material = 11;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,-0.3f,0.5f));
}
block_t WheatBlock1::getID(){ return 134;}

WheatBlock2::WheatBlock2():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 5;

    downPlane_x = 9;
    downPlane_y = 5;

    leftPlane_x = 9;
    leftPlane_y = 5;

    rightPlane_x = 9;
    rightPlane_y = 5;

    botPlane_x = 9;
    botPlane_y = 5;

    forPlane_x = 9;
    forPlane_y = 5;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 286;
    update = 1;
    solid = false;
    allowLight = true;
	material = 11;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,-0.1f,0.5f));
}
block_t WheatBlock2::getID(){ return 135;}

WheatBlock3::WheatBlock3():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 5;

    downPlane_x = 11;
    downPlane_y = 5;

    leftPlane_x = 11;
    leftPlane_y = 5;

    rightPlane_x = 11;
    rightPlane_y = 5;

    botPlane_x = 11;
    botPlane_y = 5;

    forPlane_x = 13;
    forPlane_y = 5;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 286;
    update = 1;
    solid = false;
    allowLight = true;
	material = 11;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,0.2,0.5f));
}
block_t WheatBlock3::getID(){ return 136;}

WheatBlock4::WheatBlock4():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 5;

    downPlane_x = 12;
    downPlane_y = 5;

    leftPlane_x = 12;
    leftPlane_y = 5;

    rightPlane_x = 12;
    rightPlane_y = 5;

    botPlane_x = 12;
    botPlane_y = 5;

    forPlane_x = 12;
    forPlane_y = 5;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 286;
    update = 1;
    solid = false;
    allowLight = true;
	material = 11;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,0.4,0.5f));
}
block_t WheatBlock4::getID(){ return 137;}

WheatBlock5::WheatBlock5():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 5;

    downPlane_x = 14;
    downPlane_y = 5;

    leftPlane_x = 14;
    leftPlane_y = 5;

    rightPlane_x = 14;
    rightPlane_y = 5;

    botPlane_x = 14;
    botPlane_y = 5;

    forPlane_x = 14;
    forPlane_y = 5;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 286;
    update = 1;
    solid = false;
    allowLight = true;
	material = 11;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,0.5,0.5f));
}
block_t WheatBlock5::getID(){ return 138;}

WheatBlock6::WheatBlock6():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 5;

    downPlane_x = 15;
    downPlane_y = 5;

    leftPlane_x = 15;
    leftPlane_y = 5;

    rightPlane_x = 15;
    rightPlane_y = 5;

    botPlane_x = 15;
    botPlane_y = 5;

    forPlane_x = 15;
    forPlane_y = 5;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 287;
    update = 1;
    solid = false;
    allowLight = true;
	material = 11;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,0.5,0.5f));
}
block_t WheatBlock6::getID(){ return 139;}

WorldSky::WorldSky():BaseBlock()
{
    transparent = false;
    blockModel = 5;

}
block_t WorldSky::getID(){ return 140;}

Cloud::Cloud():BaseBlock()
{

    transparent = true;
    blockModel = 5;

}
block_t Cloud::getID(){ return 141;}

bedrock::bedrock():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 1;

    downPlane_x = 1;
    downPlane_y = 1;

    leftPlane_x = 1;
    leftPlane_y = 1;

    rightPlane_x = 1;
    rightPlane_y = 1;

    botPlane_x = 1;
    botPlane_y = 1;

    forPlane_x = 1;
    forPlane_y = 1;

    transparent = false;
	editable = true;

	soundType = 2;
	solid = true;
	allowLight = false;
	material = 6;

	loot = 142;

	name = "Bedrock";
}
block_t bedrock::getID(){ return 142;}

JackOLantern1::JackOLantern1():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 6;

    downPlane_x = 6;
    downPlane_y = 7;

    leftPlane_x = 8;
    leftPlane_y = 7;

    rightPlane_x = 6;
    rightPlane_y = 7;

    botPlane_x = 6;
    botPlane_y = 7;

    forPlane_x = 6;
    forPlane_y = 7;

    editable = true;
    transparent = false;
    allowLight = true;
    lightSource = true;
    soundType = 3;

    loot = 145;
    material = 1;
    solid = true;

    name = "Jack o'Lantern";
}
block_t JackOLantern1::getID(){ return 143;}

JackOLantern2::JackOLantern2():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 6;

    downPlane_x = 6;
    downPlane_y = 7;

    leftPlane_x = 6;
    leftPlane_y = 7;

    rightPlane_x = 8;
    rightPlane_y = 7;

    botPlane_x = 6;
    botPlane_y = 7;

    forPlane_x = 6;
    forPlane_y = 7;

    editable = true;
    transparent = false;
    allowLight = true;
    lightSource = true;
    soundType = 3;

    loot = 145;
    material = 1;
    solid = true;

    name = "Jack o'Lantern";
}
block_t JackOLantern2::getID(){ return 144;}

JackOLantern3::JackOLantern3():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 6;

    downPlane_x = 6;
    downPlane_y = 7;

    leftPlane_x = 6;
    leftPlane_y = 7;

    rightPlane_x = 6;
    rightPlane_y = 7;

    botPlane_x = 8;
    botPlane_y = 7;

    forPlane_x = 6;
    forPlane_y = 7;

    editable = true;
    transparent = false;
    allowLight = true;
    lightSource = true;
    soundType = 3;

    loot = 145;
    material = 1;
    solid = true;

    name = "Jack o'Lantern";
}
block_t JackOLantern3::getID(){ return 145;}

JackOLantern4::JackOLantern4():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 6;

    downPlane_x = 6;
    downPlane_y = 7;

    leftPlane_x = 6;
    leftPlane_y = 7;

    rightPlane_x = 6;
    rightPlane_y = 7;

    botPlane_x = 6;
    botPlane_y = 7;

    forPlane_x = 8;
    forPlane_y = 7;

    editable = true;
    transparent = false;
    allowLight = true;
    lightSource = true;
    soundType = 3;

    loot = 145;
    material = 1;
    solid = true;

    name = "Jack o'Lantern";
}
block_t JackOLantern4::getID(){ return 146;}

TrapdoorOpen1::TrapdoorOpen1():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 5;

    downPlane_x = 4;
    downPlane_y = 5;

    leftPlane_x = 4;
    leftPlane_y = 5;

    rightPlane_x = 4;
    rightPlane_y = 5;

    botPlane_x = 4;
    botPlane_y = 5;

    forPlane_x = 4;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = false;

	soundType = 2;
    blockModel = 5;
    material = 8;

    loot = 151;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(-0.3125f,0.5f,0.5f));

    name = "Trapdoor";
}
block_t TrapdoorOpen1::getID(){ return 147;}

TrapdoorOpen2::TrapdoorOpen2():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 5;

    downPlane_x = 4;
    downPlane_y = 5;

    leftPlane_x = 4;
    leftPlane_y = 5;

    rightPlane_x = 4;
    rightPlane_y = 5;

    botPlane_x = 4;
    botPlane_y = 5;

    forPlane_x = 4;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = false;

	soundType = 2;
    blockModel = 5;
    material = 8;

    loot = 151;

    collideBox = BoundingBox(Vector3(0.3125f,-0.5,-0.5f),Vector3(0.5f,0.5f,0.5f));

    name = "Trapdoor";
}
block_t TrapdoorOpen2::getID(){ return 148;}

TrapdoorOpen3::TrapdoorOpen3():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 5;

    downPlane_x = 4;
    downPlane_y = 5;

    leftPlane_x = 4;
    leftPlane_y = 5;

    rightPlane_x = 4;
    rightPlane_y = 5;

    botPlane_x = 4;
    botPlane_y = 5;

    forPlane_x = 4;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = false;

	soundType = 2;
    blockModel = 5;
    material = 8;

    loot = 151;

    name = "Trapdoor";

    collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,0.5f,-0.3125f));
}
block_t TrapdoorOpen3::getID(){ return 149;}

TrapdoorOpen4::TrapdoorOpen4():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 5;

    downPlane_x = 4;
    downPlane_y = 5;

    leftPlane_x = 4;
    leftPlane_y = 5;

    rightPlane_x = 4;
    rightPlane_y = 5;

    botPlane_x = 4;
    botPlane_y = 5;

    forPlane_x = 4;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = false;

	soundType = 2;
    blockModel = 5;
    material = 8;

    loot = 151;

    name = "Trapdoor";

    collideBox = BoundingBox(Vector3(-0.5f,-0.5,0.3125f),Vector3(0.5f,0.5f,0.5f));
}
block_t TrapdoorOpen4::getID(){ return 150;}

TrapdoorClosed1::TrapdoorClosed1():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 5;

    downPlane_x = 4;
    downPlane_y = 5;

    leftPlane_x = 4;
    leftPlane_y = 5;

    rightPlane_x = 4;
    rightPlane_y = 5;

    botPlane_x = 4;
    botPlane_y = 5;

    forPlane_x = 4;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = true;

	soundType = 2;
    blockModel = 5;
    material = 8;

    loot = 151;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,-0.3125f,0.5f));

    name = "Trapdoor";
}
block_t TrapdoorClosed1::getID(){ return 151;}

TrapdoorClosed2::TrapdoorClosed2():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 5;

    downPlane_x = 4;
    downPlane_y = 5;

    leftPlane_x = 4;
    leftPlane_y = 5;

    rightPlane_x = 4;
    rightPlane_y = 5;

    botPlane_x = 4;
    botPlane_y = 5;

    forPlane_x = 4;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = true;

	soundType = 2;
    blockModel = 5;
    material = 8;

    loot = 151;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,-0.3125f,0.5f));

    name = "Trapdoor";
}
block_t TrapdoorClosed2::getID(){ return 152;}

TrapdoorClosed3::TrapdoorClosed3():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 5;

    downPlane_x = 4;
    downPlane_y = 5;

    leftPlane_x = 4;
    leftPlane_y = 5;

    rightPlane_x = 4;
    rightPlane_y = 5;

    botPlane_x = 4;
    botPlane_y = 5;

    forPlane_x = 4;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = true;

	soundType = 2;
    blockModel = 5;
    material = 8;

    loot = 151;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,-0.3125f,0.5f));

    name = "Trapdoor";
}
block_t TrapdoorClosed3::getID(){ return 153;}

TrapdoorClosed4::TrapdoorClosed4():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 5;

    downPlane_x = 4;
    downPlane_y = 5;

    leftPlane_x = 4;
    leftPlane_y = 5;

    rightPlane_x = 4;
    rightPlane_y = 5;

    botPlane_x = 4;
    botPlane_y = 5;

    forPlane_x = 4;
    forPlane_y = 5;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = true;

	soundType = 2;
    blockModel = 5;
    material = 8;

    loot = 151;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,-0.3125f,0.5f));

    name = "Trapdoor";
}
block_t TrapdoorClosed4::getID(){ return 154;}

BirchPlanks::BirchPlanks():BaseBlock() // planks
{
    upPlane_x = 9;
    upPlane_y = 8;

    downPlane_x = 9;
    downPlane_y = 8;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 3;

    loot = 155;
    material = 1;
    solid = true;

    name = "Birch Planks";
}
block_t BirchPlanks::getID(){ return 155;}

SprucePlanks::SprucePlanks():BaseBlock() // planks
{
    upPlane_x = 8;
    upPlane_y = 8;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 8;
    leftPlane_y = 8;

    rightPlane_x = 8;
    rightPlane_y = 8;

    botPlane_x = 8;
    botPlane_y = 8;

    forPlane_x = 8;
    forPlane_y = 8;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 3;

    loot = 156;
    material = 1;
    solid = true;

    name = "Spruce Planks";
}
block_t SprucePlanks::getID(){ return 156;}

BirchHalfBlock::BirchHalfBlock():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 8;

    downPlane_x = 9;
    downPlane_y = 8;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    solid = true;
    allowLight = true;

    blockModel = 1;
    loot = 157;
    material = 1;
    soundType = 3;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0,0.5f));

    name = "Birch Wood Slab";
}
block_t BirchHalfBlock::getID(){ return 157;}

SpruceHalfBlock::SpruceHalfBlock():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 8;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 8;
    leftPlane_y = 8;

    rightPlane_x = 8;
    rightPlane_y = 8;

    botPlane_x = 8;
    botPlane_y = 8;

    forPlane_x = 8;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    solid = true;
    allowLight = true;

    blockModel = 1;
    loot = 158;
    material = 1;
    soundType = 3;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0,0.5f));

    name = "Spruce Wood Slab";
}
block_t SpruceHalfBlock::getID(){ return 158;}

BirchFence::BirchFence():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 8;

    downPlane_x = 9;
    downPlane_y = 8;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

	editable = true;
	transparent = false;
    allowLight = true;
	solid = true;

    blockModel = 2;
	loot = 159;
	material = 8;
	refraction = 2;
	soundType = 3;

	name = "Birch Fence";

	collideBox = BoundingBox(Vector3(-0.125f,-0.5f,-0.125f),Vector3(0.125f,0.5,0.125f));
}
block_t BirchFence::getID(){ return 159;}

SpruceFence::SpruceFence():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 8;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 8;
    leftPlane_y = 8;

    rightPlane_x = 8;
    rightPlane_y = 8;

    botPlane_x = 8;
    botPlane_y = 8;

    forPlane_x = 8;
    forPlane_y = 8;

	editable = true;
	transparent = false;
    allowLight = true;
	solid = true;

    blockModel = 2;
	loot = 160;
	material = 8;
	refraction = 2;
	soundType = 3;

	name = "Spruce Fence";

	collideBox = BoundingBox(Vector3(-0.125f,-0.5f,-0.125f),Vector3(0.125f,0.5,0.125f));
}
block_t SpruceFence::getID(){ return 160;}

BirchStair1::BirchStair1():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 8;

    downPlane_x = 9;
    downPlane_y = 8;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    blockModel = 6;
    soundType = 3;
    material = 1;
    loot = 161;

    name = "Birch Wood Stairs";
}
block_t BirchStair1::getID(){ return 161;}

BirchStair2::BirchStair2():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 8;

    downPlane_x = 9;
    downPlane_y = 8;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    blockModel = 7;
    soundType = 3;
    material = 1;
    loot = 161;

    name = "Birch Wood Stairs";
}
block_t BirchStair2::getID(){ return 162;}

BirchStair3::BirchStair3():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 8;

    downPlane_x = 9;
    downPlane_y = 8;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    blockModel = 8;
    soundType = 3;
    material = 1;
    loot = 161;

    name = "Birch Wood Stairs";
}
block_t BirchStair3::getID(){ return 163;}

BirchStair4::BirchStair4():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 8;

    downPlane_x = 9;
    downPlane_y = 8;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    blockModel = 9;
    soundType = 3;
    material = 1;
    loot = 161;

    name = "Birch Wood Stairs";
}
block_t BirchStair4::getID(){ return 164;}

SpruceStair1::SpruceStair1():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 8;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 8;
    leftPlane_y = 8;

    rightPlane_x = 8;
    rightPlane_y = 8;

    botPlane_x = 8;
    botPlane_y = 8;

    forPlane_x = 8;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    blockModel = 6;
    soundType = 3;
    material = 1;
    loot = 165;

    name = "Spruce Wood Stairs";
}
block_t SpruceStair1::getID(){ return 165;}

SpruceStair2::SpruceStair2():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 8;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 8;
    leftPlane_y = 8;

    rightPlane_x = 8;
    rightPlane_y = 8;

    botPlane_x = 8;
    botPlane_y = 8;

    forPlane_x = 8;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    blockModel = 7;
    soundType = 3;
    material = 1;
    loot = 165;

    name = "Spruce Wood Stairs";
}
block_t SpruceStair2::getID(){ return 166;}

SpruceStair3::SpruceStair3():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 8;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 8;
    leftPlane_y = 8;

    rightPlane_x = 8;
    rightPlane_y = 8;

    botPlane_x = 8;
    botPlane_y = 8;

    forPlane_x = 8;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    blockModel = 8;
    soundType = 3;
    material = 1;
    loot = 165;

    name = "Spruce Wood Stairs";
}
block_t SpruceStair3::getID(){ return 167;}

SpruceStair4::SpruceStair4():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 8;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 8;
    leftPlane_y = 8;

    rightPlane_x = 8;
    rightPlane_y = 8;

    botPlane_x = 8;
    botPlane_y = 8;

    forPlane_x = 8;
    forPlane_y = 8;

    editable = true;
    transparent = false;
    allowLight = false;
    solid = true;

    blockModel = 9;
    soundType = 3;
    material = 1;
    loot = 165;

    name = "Spruce Wood Stairs";
}
block_t SpruceStair4::getID(){ return 168;}

WaterBlock1::WaterBlock1():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 15;

    downPlane_x = 14;
    downPlane_y = 15;

    leftPlane_x = 14;
    leftPlane_y = 15;

    rightPlane_x = 14;
    rightPlane_y = 15;

    botPlane_x = 14;
    botPlane_y = 15;

    forPlane_x = 14;
    forPlane_y = 15;

	editable = false;
	transparent = true;
    solid = false;

    soundType = -1;
    blockModel = 5;
    allowLight = true;
    refraction = 0;

    name = "Water Block";
}

block_t WaterBlock1::getID(){ return 169;}

WaterBlock2::WaterBlock2():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 15;

    downPlane_x = 14;
    downPlane_y = 15;

    leftPlane_x = 14;
    leftPlane_y = 15;

    rightPlane_x = 14;
    rightPlane_y = 15;

    botPlane_x = 14;
    botPlane_y = 15;

    forPlane_x = 14;
    forPlane_y = 15;

	editable = false;
	transparent = true;
    solid = false;

    soundType = -1;
    blockModel = 5;
    allowLight = true;
    refraction = 0;

    name = "Water Block";
}

block_t WaterBlock2::getID(){ return 170;}

WaterBlock3::WaterBlock3():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 15;

    downPlane_x = 14;
    downPlane_y = 15;

    leftPlane_x = 14;
    leftPlane_y = 15;

    rightPlane_x = 14;
    rightPlane_y = 15;

    botPlane_x = 14;
    botPlane_y = 15;

    forPlane_x = 14;
    forPlane_y = 15;

	editable = false;
	transparent = true;
    solid = false;

    soundType = -1;
    blockModel = 5;
    allowLight = true;
    refraction = 0;

    name = "Water Block";
}

block_t WaterBlock3::getID(){ return 171;}

WaterBlock4::WaterBlock4():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 15;

    downPlane_x = 14;
    downPlane_y = 15;

    leftPlane_x = 14;
    leftPlane_y = 15;

    rightPlane_x = 14;
    rightPlane_y = 15;

    botPlane_x = 14;
    botPlane_y = 15;

    forPlane_x = 14;
    forPlane_y = 15;

	editable = false;
	transparent = true;
    solid = false;

    soundType = -1;
    blockModel = 5;
    allowLight = true;
    refraction = 0;

    name = "Water Block";
}

block_t WaterBlock4::getID(){ return 172;}

block_t WaterBlock5::getID(){ return kExtendedBlockBase + 0;}
block_t WaterBlock6::getID(){ return kExtendedBlockBase + 1;}
block_t WaterBlock7::getID(){ return kExtendedBlockBase + 2;}

ItemFrame1::ItemFrame1():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 10;

    downPlane_x = 8;
    downPlane_y = 10;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = false;

	soundType = 3;
	blockModel = 5;
	material = 1;

	name = "Item Frame";

	collideBox = BoundingBox(Vector3(-0.5f,-0.375f,-0.375f),Vector3(-0.375f,0.375f,0.375f));
}
block_t ItemFrame1::getID(){ return 173;}

ItemFrame2::ItemFrame2():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 10;

    downPlane_x = 8;
    downPlane_y = 10;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = false;

	soundType = 3;
	blockModel = 5;
	material = 1;

	name = "Item Frame";

	collideBox = BoundingBox(Vector3(0.375f,-0.375f,-0.375f),Vector3(0.5f,0.375f,0.375f));
}
block_t ItemFrame2::getID(){ return 174;}

ItemFrame3::ItemFrame3():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 10;

    downPlane_x = 8;
    downPlane_y = 10;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = false;

	soundType = 3;
	blockModel = 5;
	material = 1;

	name = "Item Frame";

	collideBox = BoundingBox(Vector3(-0.375f,-0.375f,-0.5f),Vector3(0.375f,0.375f,-0.375f));
}
block_t ItemFrame3::getID(){ return 175;}

ItemFrame4::ItemFrame4():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 10;

    downPlane_x = 8;
    downPlane_y = 10;

    leftPlane_x = 9;
    leftPlane_y = 8;

    rightPlane_x = 9;
    rightPlane_y = 8;

    botPlane_x = 9;
    botPlane_y = 8;

    forPlane_x = 9;
    forPlane_y = 8;

	editable = true;
	transparent = true;
	allowLight = true;
	solid = false;

	soundType = 3;
	blockModel = 5;
	material = 1;

	name = "Item Frame";

	collideBox = BoundingBox(Vector3(-0.375f,-0.375f,0.375f),Vector3(0.375f,0.375f,0.5f));
}
block_t ItemFrame4::getID(){ return 176;}

PumpkinPlant4Reverse::PumpkinPlant4Reverse():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 7;

    downPlane_x = 13;
    downPlane_y = 7;

    leftPlane_x = 13;
    leftPlane_y = 7;

    rightPlane_x = 13;
    rightPlane_y = 7;

    botPlane_x = 13;
    botPlane_y = 7;

    forPlane_x = 13;
    forPlane_y = 7;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 5;
}
block_t PumpkinPlant4Reverse::getID(){ return 177;}

PumpkinPlant1::PumpkinPlant1():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 6;

    downPlane_x = 10;
    downPlane_y = 6;

    leftPlane_x = 10;
    leftPlane_y = 6;

    rightPlane_x = 10;
    rightPlane_y = 6;

    botPlane_x = 10;
    botPlane_y = 6;

    forPlane_x = 10;
    forPlane_y = 6;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 3;
	collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,-0.3f,0.5f));
}
block_t PumpkinPlant1::getID(){ return 178;}

PumpkinPlant2::PumpkinPlant2():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 6;

    downPlane_x = 11;
    downPlane_y = 6;

    leftPlane_x = 11;
    leftPlane_y = 6;

    rightPlane_x = 11;
    rightPlane_y = 6;

    botPlane_x = 11;
    botPlane_y = 6;

    forPlane_x = 11;
    forPlane_y = 6;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 3;
	collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,-0.1f,0.5f));
}
block_t PumpkinPlant2::getID(){ return 179;}

PumpkinPlant3::PumpkinPlant3():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 6;

    downPlane_x = 12;
    downPlane_y = 6;

    leftPlane_x = 12;
    leftPlane_y = 6;

    rightPlane_x = 12;
    rightPlane_y = 6;

    botPlane_x = 12;
    botPlane_y = 6;

    forPlane_x = 12;
    forPlane_y = 6;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 3;
	collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,0.4,0.5f));
}
block_t PumpkinPlant3::getID(){ return 180;}

PumpkinPlant4::PumpkinPlant4():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 6;

    downPlane_x = 13;
    downPlane_y = 6;

    leftPlane_x = 13;
    leftPlane_y = 6;

    rightPlane_x = 13;
    rightPlane_y = 6;

    botPlane_x = 13;
    botPlane_y = 6;

    forPlane_x = 13;
    forPlane_y = 6;

    editable = true;
    transparent = true;
    solid = false;
    allowLight = true;

    update = 1;
    blockModel = 5;
}
block_t PumpkinPlant4::getID(){ return 181;}

PotatoPlant1::PotatoPlant1():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 8;

    downPlane_x = 10;
    downPlane_y = 8;

    leftPlane_x = 10;
    leftPlane_y = 8;

    rightPlane_x = 10;
    rightPlane_y = 8;

    botPlane_x = 10;
    botPlane_y = 8;

    forPlane_x = 10;
    forPlane_y = 8;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 379;
    update = 1;
    solid = false;
    allowLight = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,-0.3f,0.5f));
}
block_t PotatoPlant1::getID(){ return 182;}

PotatoPlant2::PotatoPlant2():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 8;

    downPlane_x = 11;
    downPlane_y = 8;

    leftPlane_x = 11;
    leftPlane_y = 8;

    rightPlane_x = 11;
    rightPlane_y = 8;

    botPlane_x = 11;
    botPlane_y = 8;

    forPlane_x = 11;
    forPlane_y = 8;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 379;
    update = 1;
    solid = false;
    allowLight = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,-0.1f,0.5f));
}
block_t PotatoPlant2::getID(){ return 183;}

PotatoPlant3::PotatoPlant3():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 8;

    downPlane_x = 12;
    downPlane_y = 8;

    leftPlane_x = 12;
    leftPlane_y = 8;

    rightPlane_x = 12;
    rightPlane_y = 8;

    botPlane_x = 12;
    botPlane_y = 8;

    forPlane_x = 12;
    forPlane_y = 8;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 379;
    update = 1;
    solid = false;
    allowLight = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,0.1f,0.5f));
}
block_t PotatoPlant3::getID(){ return 184;}

PotatoPlant4::PotatoPlant4():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 8;

    downPlane_x = 13;
    downPlane_y = 8;

    leftPlane_x = 13;
    leftPlane_y = 8;

    rightPlane_x = 13;
    rightPlane_y = 8;

    botPlane_x = 13;
    botPlane_y = 8;

    forPlane_x = 13;
    forPlane_y = 8;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 379;
    update = 1;
    solid = false;
    allowLight = true;

    collideBox = BoundingBox(Vector3(-0.45f,-0.5f,-0.45f),Vector3(0.45f,0.5,0.45f));
}
block_t PotatoPlant4::getID(){ return 185;}

CarrotPlant1::CarrotPlant1():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 8;

    downPlane_x = 10;
    downPlane_y = 8;

    leftPlane_x = 10;
    leftPlane_y = 8;

    rightPlane_x = 10;
    rightPlane_y = 8;

    botPlane_x = 10;
    botPlane_y = 8;

    forPlane_x = 10;
    forPlane_y = 8;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 382;
    update = 1;
    solid = false;
    allowLight = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,-0.3f,0.5f));
}
block_t CarrotPlant1::getID(){ return 186;}

CarrotPlant2::CarrotPlant2():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 8;

    downPlane_x = 11;
    downPlane_y = 8;

    leftPlane_x = 11;
    leftPlane_y = 8;

    rightPlane_x = 11;
    rightPlane_y = 8;

    botPlane_x = 11;
    botPlane_y = 8;

    forPlane_x = 11;
    forPlane_y = 8;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 382;
    update = 1;
    solid = false;
    allowLight = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,-0.1f,0.5f));
}
block_t CarrotPlant2::getID(){ return 187;}

CarrotPlant3::CarrotPlant3():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 8;

    downPlane_x = 12;
    downPlane_y = 8;

    leftPlane_x = 12;
    leftPlane_y = 8;

    rightPlane_x = 12;
    rightPlane_y = 8;

    botPlane_x = 12;
    botPlane_y = 8;

    forPlane_x = 12;
    forPlane_y = 8;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 382;
    update = 1;
    solid = false;
    allowLight = true;

    collideBox = BoundingBox(Vector3(-0.5f,-0.55f,-0.5f),Vector3(0.5f,0.1f,0.5f));
}
block_t CarrotPlant3::getID(){ return 188;}

CarrotPlant4::CarrotPlant4():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 8;

    downPlane_x = 14;
    downPlane_y = 8;

    leftPlane_x = 14;
    leftPlane_y = 8;

    rightPlane_x = 14;
    rightPlane_y = 8;

    botPlane_x = 14;
    botPlane_y = 8;

    forPlane_x = 14;
    forPlane_y = 8;

	editable = true;
	transparent = true;
    blockModel = 4;
    soundType = 0;

    loot = 382;
    update = 1;
    solid = false;
    allowLight = true;

    collideBox = BoundingBox(Vector3(-0.45f,-0.55f,-0.45f),Vector3(0.45f,0.5,0.45f));
}
block_t CarrotPlant4::getID(){ return 189;}

BedBlock1::BedBlock1():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 8;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 5;
    leftPlane_y = 9;

    rightPlane_x = 8;
    rightPlane_y = 9;

    botPlane_x = 6;
    botPlane_y = 10;

    forPlane_x = 6;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	soundType = 3;
    blockModel = 1;

    loot = 361;
    solid = true;
    material = 1;
    allowLight = false;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,1.0f/16.0f,0.5f));
}
block_t BedBlock1::getID(){ return 190;}

BedBlock2::BedBlock2():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 11;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 8;
    leftPlane_y = 9;

    rightPlane_x = 5;
    rightPlane_y = 9;

    botPlane_x = 6;
    botPlane_y = 9;

    forPlane_x = 6;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	soundType = 3;
    blockModel = 1;

    loot = 361;
    solid = true;
    material = 1;
    allowLight = false;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,1.0f/16.0f,0.5f));
}
block_t BedBlock2::getID(){ return 191;}

BedBlock3::BedBlock3():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 12;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 6;
    leftPlane_y = 9;

    rightPlane_x = 6;
    rightPlane_y = 10;

    botPlane_x = 5;
    botPlane_y = 9;

    forPlane_x = 8;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	soundType = 3;
    blockModel = 1;

    loot = 361;
    solid = true;
    material = 1;
    allowLight = false;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,1.0f/16.0f,0.5f));
}
block_t BedBlock3::getID(){ return 192;}

BedBlock4::BedBlock4():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 13;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 6;
    leftPlane_y = 10;

    rightPlane_x = 6;
    rightPlane_y = 9;

    botPlane_x = 8;
    botPlane_y = 9;

    forPlane_x = 5;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	soundType = 3;
    blockModel = 1;

    loot = 361;
    solid = true;
    material = 1;
    allowLight = false;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,1.0f/16.0f,0.5f));
}
block_t BedBlock4::getID(){ return 193;}

BedBlock5::BedBlock5():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 8;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 5;
    leftPlane_y = 9;

    rightPlane_x = 8;
    rightPlane_y = 9;

    botPlane_x = 7;
    botPlane_y = 10;

    forPlane_x = 7;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	soundType = 3;
    blockModel = 1;

    loot = 361;
    solid = true;
    material = 1;
    allowLight = false;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,1.0f/16.0f,0.5f));
}
block_t BedBlock5::getID(){ return 194;}

BedBlock6::BedBlock6():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 11;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 8;
    leftPlane_y = 9;

    rightPlane_x = 5;
    rightPlane_y = 9;

    botPlane_x = 7;
    botPlane_y = 9;

    forPlane_x = 7;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	soundType = 3;
    blockModel = 1;

    loot = 361;
    solid = true;
    material = 1;
    allowLight = false;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,1.0f/16.0f,0.5f));
}
block_t BedBlock6::getID(){ return 195;}

BedBlock7::BedBlock7():BaseBlock()
{
    upPlane_x = 6;
    upPlane_y = 13;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 7;
    leftPlane_y = 9;

    rightPlane_x = 7;
    rightPlane_y = 10;

    botPlane_x = 5;
    botPlane_y = 9;

    forPlane_x = 8;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	soundType = 3;
    blockModel = 1;

    loot = 361;
    solid = true;
    material = 1;
    allowLight = false;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,1.0f/16.0f,0.5f));
}
block_t BedBlock7::getID(){ return 196;}

BedBlock8::BedBlock8():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 12;

    downPlane_x = 8;
    downPlane_y = 8;

    leftPlane_x = 7;
    leftPlane_y = 10;

    rightPlane_x = 7;
    rightPlane_y = 9;

    botPlane_x = 8;
    botPlane_y = 9;

    forPlane_x = 5;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	soundType = 3;
    blockModel = 1;

    loot = 361;
    solid = true;
    material = 1;
    allowLight = false;

    collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,1.0f/16.0f,0.5f));
}
block_t BedBlock8::getID(){ return 197;}


BlackWoolCarpet::BlackWoolCarpet():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 7;

    downPlane_x = 1;
    downPlane_y = 7;

    leftPlane_x = 1;
    leftPlane_y = 7;

    rightPlane_x = 1;
    rightPlane_y = 7;

    botPlane_x = 1;
    botPlane_y = 7;

    forPlane_x = 1;
    forPlane_y = 7;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 198;
	material = 4;
	solid = false;

	name = "Black Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t BlackWoolCarpet::getID(){ return 198;}


GrayWoolCarpet::GrayWoolCarpet():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 7;

    downPlane_x = 2;
    downPlane_y = 7;

    leftPlane_x = 2;
    leftPlane_y = 7;

    rightPlane_x = 2;
    rightPlane_y = 7;

    botPlane_x = 2;
    botPlane_y = 7;

    forPlane_x = 2;
    forPlane_y = 7;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 199;
	solid = false;

	name = "Gray Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t GrayWoolCarpet::getID(){ return 199;}


RedWoolCarpet::RedWoolCarpet():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 8;

    downPlane_x = 1;
    downPlane_y = 8;

    leftPlane_x = 1;
    leftPlane_y = 8;

    rightPlane_x = 1;
    rightPlane_y = 8;

    botPlane_x = 1;
    botPlane_y = 8;

    forPlane_x = 1;
    forPlane_y = 8;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 200;
	solid = false;

	name = "Red Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t RedWoolCarpet::getID(){ return 200;}


PinkWoolCarpet::PinkWoolCarpet():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 8;

    downPlane_x = 2;
    downPlane_y = 8;

    leftPlane_x = 2;
    leftPlane_y = 8;

    rightPlane_x = 2;
    rightPlane_y = 8;

    botPlane_x = 2;
    botPlane_y = 8;

    forPlane_x = 2;
    forPlane_y = 8;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 201;
	solid = false;

	name = "Pink Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t PinkWoolCarpet::getID(){ return 201;}


DarkGreenWoolCarpet::DarkGreenWoolCarpet():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 9;

    downPlane_x = 1;
    downPlane_y = 9;

    leftPlane_x = 1;
    leftPlane_y = 9;

    rightPlane_x = 1;
    rightPlane_y = 9;

    botPlane_x = 1;
    botPlane_y = 9;

    forPlane_x = 1;
    forPlane_y = 9;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 202;
	solid = false;

	name = "Green Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t DarkGreenWoolCarpet::getID(){ return 202;}


GreenWoolCarpet::GreenWoolCarpet():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 9;

    downPlane_x = 2;
    downPlane_y = 9;

    leftPlane_x = 2;
    leftPlane_y = 9;

    rightPlane_x = 2;
    rightPlane_y = 9;

    botPlane_x = 2;
    botPlane_y = 9;

    forPlane_x = 2;
    forPlane_y = 9;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 203;
	solid = false;

	name = "Lime Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t GreenWoolCarpet::getID(){ return 203;}


BrownWoolCarpet::BrownWoolCarpet():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 10;

    downPlane_x = 1;
    downPlane_y = 10;

    leftPlane_x = 1;
    leftPlane_y = 10;

    rightPlane_x = 1;
    rightPlane_y = 10;

    botPlane_x = 1;
    botPlane_y = 10;

    forPlane_x = 1;
    forPlane_y = 10;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 204;
	solid = false;

	name = "Brown Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t BrownWoolCarpet::getID(){ return 204;}


YellowWoolCarpet::YellowWoolCarpet():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 10;

    downPlane_x = 2;
    downPlane_y = 10;

    leftPlane_x = 2;
    leftPlane_y = 10;

    rightPlane_x = 2;
    rightPlane_y = 10;

    botPlane_x = 2;
    botPlane_y = 10;

    forPlane_x = 2;
    forPlane_y = 10;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 205;
	solid = false;

	name = "Yellow Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t YellowWoolCarpet::getID(){ return 205;}


BlueWoolCarpet::BlueWoolCarpet():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 11;

    downPlane_x = 1;
    downPlane_y = 11;

    leftPlane_x = 1;
    leftPlane_y = 11;

    rightPlane_x = 1;
    rightPlane_y = 11;

    botPlane_x = 1;
    botPlane_y = 11;

    forPlane_x = 1;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 206;
	solid = false;

	name = "Blue Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t BlueWoolCarpet::getID(){ return 206;}


LightBlueWoolCarpet::LightBlueWoolCarpet():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 11;

    downPlane_x = 2;
    downPlane_y = 11;

    leftPlane_x = 2;
    leftPlane_y = 11;

    rightPlane_x = 2;
    rightPlane_y = 11;

    botPlane_x = 2;
    botPlane_y = 11;

    forPlane_x = 2;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 207;
	solid = false;

	name = "Light Blue Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t LightBlueWoolCarpet::getID(){ return 207;}


VioletWoolCarpet::VioletWoolCarpet():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 12;

    downPlane_x = 1;
    downPlane_y = 12;

    leftPlane_x = 1;
    leftPlane_y = 12;

    rightPlane_x = 1;
    rightPlane_y = 12;

    botPlane_x = 1;
    botPlane_y = 12;

    forPlane_x = 1;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 208;
	solid = false;

	name = "Purple Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t VioletWoolCarpet::getID(){ return 208;}


PastelWoolCarpet::PastelWoolCarpet():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 12;

    downPlane_x = 2;
    downPlane_y = 12;

    leftPlane_x = 2;
    leftPlane_y = 12;

    rightPlane_x = 2;
    rightPlane_y = 12;

    botPlane_x = 2;
    botPlane_y = 12;

    forPlane_x = 2;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

    loot = 209;
	solid = false;

	name = "Magneta Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t PastelWoolCarpet::getID(){ return 209;}


OrangeWoolCarpet::OrangeWoolCarpet():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 13;

    downPlane_x = 2;
    downPlane_y = 13;

    leftPlane_x = 2;
    leftPlane_y = 13;

    rightPlane_x = 2;
    rightPlane_y = 13;

    botPlane_x = 2;
    botPlane_y = 13;

    forPlane_x = 2;
    forPlane_y = 13;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 210;
	solid = false;

	name = "Orange Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t OrangeWoolCarpet::getID(){ return 210;}


LightGrayWoolCarpet::LightGrayWoolCarpet():BaseBlock()
{
    upPlane_x = 1;
    upPlane_y = 14;

    downPlane_x = 1;
    downPlane_y = 14;

    leftPlane_x = 1;
    leftPlane_y = 14;

    rightPlane_x = 1;
    rightPlane_y = 14;

    botPlane_x = 1;
    botPlane_y = 14;

    forPlane_x = 1;
    forPlane_y = 14;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 211;
	solid = false;

	name = "Light Gray Carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t LightGrayWoolCarpet::getID(){ return 211;}


WhiteWoolCarpet::WhiteWoolCarpet():BaseBlock()
{
    upPlane_x = 2;
    upPlane_y = 14;

    downPlane_x = 0;
    downPlane_y = 14;

    leftPlane_x = 2;
    leftPlane_y = 14;

    rightPlane_x = 2;
    rightPlane_y = 14;

    botPlane_x = 2;
    botPlane_y = 14;

    forPlane_x = 2;
    forPlane_y = 14;

	editable = true;
	transparent = false;
	allowLight = true;
	soundType = 6;
	material = 1;
	blockModel = 5;
	refraction = 2;

	loot = 212;
	solid = false;

	name = "White carpet";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t WhiteWoolCarpet::getID(){ return 212;}

HayBale::HayBale():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 11;

    downPlane_x = 4;
    downPlane_y = 11;

    leftPlane_x = 5;
    leftPlane_y = 11;

    rightPlane_x = 5;
    rightPlane_y = 11;

    botPlane_x = 4;
    botPlane_y = 11;

    forPlane_x = 4;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	loot = 7;
	material = 3;
	solid = true;

	name = "Hay Bale";
}
block_t HayBale::getID(){ return 213;}

CoalBlock::CoalBlock():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 10;

    downPlane_x = 3;
    downPlane_y = 10;

    leftPlane_x = 3;
    leftPlane_y = 10;

    rightPlane_x = 3;
    rightPlane_y = 10;

    botPlane_x = 3;
    botPlane_y = 10;

    forPlane_x = 3;
    forPlane_y = 10;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;

    loot = 101;
    material = 2;
    soundType = 2;

    name = "Coal Block";
}
block_t CoalBlock::getID(){ return 214;}

RedstoneBlock::RedstoneBlock():BaseBlock()
{
    upPlane_x = 3;
    upPlane_y = 11;

    downPlane_x = 3;
    downPlane_y = 11;

    leftPlane_x = 3;
    leftPlane_y = 11;

    rightPlane_x = 3;
    rightPlane_y = 11;

    botPlane_x = 3;
    botPlane_y = 11;

    forPlane_x = 3;
    forPlane_y = 11;

    editable = true;
    transparent = false;
    lightSource = false;
    allowLight = false;
    solid = true;

    loot = 101;
    material = 2;
    soundType = 2;

    name = "Redstone Block";
}
block_t RedstoneBlock::getID(){ return 215;}

YellowGlassBlock::YellowGlassBlock():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 9;

    downPlane_x = 9;
    downPlane_y = 9;

    leftPlane_x = 9;
    leftPlane_y = 9;

    rightPlane_x = 9;
    rightPlane_y = 9;

    botPlane_x = 9;
    botPlane_y = 9;

    forPlane_x = 9;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Yellow Stained Glass";
}
block_t YellowGlassBlock::getID(){ return 216;}

GreenGlassBlock::GreenGlassBlock():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 9;

    downPlane_x = 10;
    downPlane_y = 9;

    leftPlane_x = 10;
    leftPlane_y = 9;

    rightPlane_x = 10;
    rightPlane_y = 9;

    botPlane_x = 10;
    botPlane_y = 9;

    forPlane_x = 10;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Green Stained Glass";
}
block_t GreenGlassBlock::getID(){ return 217;}

PinkGlassBlock::PinkGlassBlock():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 9;

    downPlane_x = 11;
    downPlane_y = 9;

    leftPlane_x = 11;
    leftPlane_y = 9;

    rightPlane_x = 11;
    rightPlane_y = 9;

    botPlane_x = 11;
    botPlane_y = 9;

    forPlane_x = 11;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;
    name = "Pink Stained Glass";
}
block_t PinkGlassBlock::getID(){ return 218;}

PurpleGlassBlock::PurpleGlassBlock():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 9;

    downPlane_x = 12;
    downPlane_y = 9;

    leftPlane_x = 12;
    leftPlane_y = 9;

    rightPlane_x = 12;
    rightPlane_y = 9;

    botPlane_x = 12;
    botPlane_y = 9;

    forPlane_x = 12;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	solid = true;
	loot = -1;
	refraction = 1;

    name = "Purple Stained Glass";
}
block_t PurpleGlassBlock::getID(){ return 219;}

/*OrangeGlassBlock::OrangeGlassBlock():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 9;

    downPlane_x = 13;
    downPlane_y = 9;

    leftPlane_x = 13;
    leftPlane_y = 9;

    rightPlane_x = 13;
    rightPlane_y = 9;

    botPlane_x = 13;
    botPlane_y = 9;

    forPlane_x = 13;
    forPlane_y = 9;
	
	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	solid = true;
	loot = -1;
	refraction = 1;

    name = "Orange Stained Glass";
}
block_t OrangeGlassBlock::getID(){ return 220;}*/

RedGlassBlock::RedGlassBlock():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 9;

    downPlane_x = 14;
    downPlane_y = 9;

    leftPlane_x = 14;
    leftPlane_y = 9;

    rightPlane_x = 14;
    rightPlane_y = 9;

    botPlane_x = 14;
    botPlane_y = 9;

    forPlane_x = 14;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Red Stained Glass";
}
block_t RedGlassBlock::getID(){ return 220;}

/*DGreenGlassBlock::DGreenGlassBlock():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 9;

    downPlane_x = 15;
    downPlane_y = 9;

    leftPlane_x = 15;
    leftPlane_y = 9;

    rightPlane_x = 15;
    rightPlane_y = 9;

    botPlane_x = 15;
    botPlane_y = 9;

    forPlane_x = 15;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;
    name = "Dark Green Stained Glass";
}
block_t DGreenGlassBlock::getID(){ return 222;}*/

BlackGlassBlock::BlackGlassBlock():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 10;

    downPlane_x = 10;
    downPlane_y = 10;

    leftPlane_x = 10;
    leftPlane_y = 10;

    rightPlane_x = 10;
    rightPlane_y = 10;

    botPlane_x = 10;
    botPlane_y = 10;

    forPlane_x = 10;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Black Stained Glass";
}
block_t BlackGlassBlock::getID(){ return 221;}

/*LGrayGlassBlock::LGrayGlassBlock():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 10;

    downPlane_x = 11;
    downPlane_y = 10;

    leftPlane_x = 11;
    leftPlane_y = 10;

    rightPlane_x = 11;
    rightPlane_y = 10;

    botPlane_x = 11;
    botPlane_y = 10;

    forPlane_x = 11;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Light Gray Stained Glass";
}
block_t LGrayGlassBlock::getID(){ return 224;}*/

/*GrayGlassBlock::GrayGlassBlock():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 10;

    downPlane_x = 10;
    downPlane_y = 10;

    leftPlane_x = 10;
    leftPlane_y = 10;

    rightPlane_x = 10;
    rightPlane_y = 10;

    botPlane_x = 10;
    botPlane_y = 10;

    forPlane_x = 10;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Gray Stained Glass";
}
block_t GrayGlassBlock::getID(){ return 223;}*/

/*WhiteGlassBlock::WhiteGlassBlock():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 10;

    downPlane_x = 12;
    downPlane_y = 10;

    leftPlane_x = 12;
    leftPlane_y = 10;

    rightPlane_x = 12;
    rightPlane_y = 10;

    botPlane_x = 12;
    botPlane_y = 10;

    forPlane_x = 12;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 1;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "White Stained Glass";
}
block_t WhiteGlassBlock::getID(){ return 223;}*/

/*BrownGlassBlock::BrownGlassBlock():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 10;

    downPlane_x = 13;
    downPlane_y = 10;

    leftPlane_x = 13;
    leftPlane_y = 10;

    rightPlane_x = 13;
    rightPlane_y = 10;

    botPlane_x = 13;
    botPlane_y = 10;

    forPlane_x = 13;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Brown Stained Glass";
}
block_t BrownGlassBlock::getID(){ return 223;}

DBlueGlassBlock::DBlueGlassBlock():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 10;

    downPlane_x = 14;
    downPlane_y = 10;

    leftPlane_x = 14;
    leftPlane_y = 10;

    rightPlane_x = 14;
    rightPlane_y = 10;

    botPlane_x = 14;
    botPlane_y = 10;

    forPlane_x = 14;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Dark Blue Stained Glass";
}
block_t DBlueGlassBlock::getID(){ return 226;}*/

/*LBlueGlassBlock::LBlueGlassBlock():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 10;

    downPlane_x = 15;
    downPlane_y = 10;

    leftPlane_x = 15;
    leftPlane_y = 10;

    rightPlane_x = 15;
    rightPlane_y = 10;

    botPlane_x = 15;
    botPlane_y = 10;

    forPlane_x = 15;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Light Blue Stained Glass";
}
block_t LBlueGlassBlock::getID(){ return 229;}*/

BlueGlassBlock::BlueGlassBlock():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 11;

    downPlane_x = 14;
    downPlane_y = 11;

    leftPlane_x = 14;
    leftPlane_y = 11;

    rightPlane_x = 14;
    rightPlane_y = 11;

    botPlane_x = 14;
    botPlane_y = 11;

    forPlane_x = 14;
    forPlane_y = 11;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Blue Stained Glass";
}
block_t BlueGlassBlock::getID(){ return 222;}

/*DPurpleGlassBlock::DPurpleGlassBlock():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 11;

    downPlane_x = 15;
    downPlane_y = 11;

    leftPlane_x = 15;
    leftPlane_y = 11;

    rightPlane_x = 15;
    rightPlane_y = 11;

    botPlane_x = 15;
    botPlane_y = 11;

    forPlane_x = 15;
    forPlane_y = 11;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
    material = 7;

	loot = -1;
	solid = true;
	refraction = 1;

    name = "Dark Purple Stained Glass";
}
block_t DPurpleGlassBlock::getID(){ return 231;}*/

GlowstoneBlock::GlowstoneBlock():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 6;

    downPlane_x = 9;
    downPlane_y = 6;

    leftPlane_x = 9;
    leftPlane_y = 6;

    rightPlane_x = 9;
    rightPlane_y = 6;

    botPlane_x = 9;
    botPlane_y = 6;

    forPlane_x = 9;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;

    loot = -1;
    solid = true;
	lightSource = true;

    name = "Glowstone";
}
block_t GlowstoneBlock::getID(){ return 223;}

Soulsand::Soulsand():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 6;

    downPlane_x = 8;
    downPlane_y = 6;

    leftPlane_x = 8;
    leftPlane_y = 6;

    rightPlane_x = 8;
    rightPlane_y = 6;

    botPlane_x = 8;
    botPlane_y = 6;

    forPlane_x = 8;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 1;
	material = 3;

    loot = -1;
    solid = true;
    allowLight = true;

    name = "Soul Sand";
}
block_t Soulsand::getID(){ return 224;}

Netherrack::Netherrack():BaseBlock()
{
    upPlane_x = 7;
    upPlane_y = 6;

    downPlane_x = 7;
    downPlane_y = 6;

    leftPlane_x = 7;
    leftPlane_y = 6;

    rightPlane_x = 7;
    rightPlane_y = 6;

    botPlane_x = 7;
    botPlane_y = 6;

    forPlane_x = 7;
    forPlane_y = 6;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
	material = 2;

    loot = -1;
    solid = true;
    allowLight = true;

    name = "Netherrack";
}
block_t Netherrack::getID(){ return 225;}

MagmaBlock::MagmaBlock():BaseBlock()
{
    upPlane_x = 0;
    upPlane_y = 4;

    downPlane_x = 0;
    downPlane_y = 4;

    leftPlane_x = 0;
    leftPlane_y = 4;

    rightPlane_x = 0;
    rightPlane_y = 4;

    botPlane_x = 0;
    botPlane_y = 4;

    forPlane_x = 0;
    forPlane_y = 4;

	editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;
	material = 2;

    loot = -1;
    solid = true;
    allowLight = true;
	lightSource = true;

    name = "Magma Block";
}
block_t MagmaBlock::getID(){ return 226;}

Redstoneoff::Redstoneoff():BaseBlock()
{
	upPlane_x = 4;
    upPlane_y = 10;

    downPlane_x = 4;
    downPlane_y = 10;

    leftPlane_x = 15;
    leftPlane_y = 8;

    rightPlane_x = 15;
    rightPlane_y = 8;

    botPlane_x = 15;
    botPlane_y = 8;

    forPlane_x = 15;
    forPlane_y = 8;

	editable = true;
	transparent = true;
	allowLight = true;
	soundType = 2;
	blockModel = 11;
	refraction = 2;

	loot = 353;
	solid = false;

	name = "Redstoneoff";

	collideBox = BoundingBox(Vector3(-0.5f,-0.5,-0.5f),Vector3(0.5f,-0.375f,0.5f));
}
block_t Redstoneoff::getID(){ return 227;}

BlockofQuartz::BlockofQuartz():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 4;

    downPlane_x = 12;
    downPlane_y = 4;

    leftPlane_x = 12;
    leftPlane_y = 4;

    rightPlane_x = 12;
    rightPlane_y = 4;

    botPlane_x = 12; 
    botPlane_y = 4;

    forPlane_x = 12;
    forPlane_y = 4;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

    loot = -1;
    solid = true;
    allowLight = true;

    name = "Block of Quartz";
}
block_t BlockofQuartz::getID(){ return 228;}

ChiseledQuartzBlock::ChiseledQuartzBlock():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 4;

    downPlane_x = 13;
    downPlane_y = 4;

    leftPlane_x = 13;
    leftPlane_y = 4;

    rightPlane_x = 13;
    rightPlane_y = 4;

    botPlane_x = 13; 
    botPlane_y = 4;

    forPlane_x = 13;
    forPlane_y = 4;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

    loot = -1;
    solid = true;
    allowLight = true;

    name = "Chiseled Quartz Block";
}
block_t ChiseledQuartzBlock::getID(){ return 229;}

PillarQuartzBlock::PillarQuartzBlock():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 4;

    downPlane_x = 14;
    downPlane_y = 4;

    leftPlane_x = 15;
    leftPlane_y = 4;

    rightPlane_x = 15;
    rightPlane_y = 4;

    botPlane_x = 15; 
    botPlane_y = 4;

    forPlane_x = 15;
    forPlane_y = 4;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 2;

    loot = -1;
    solid = true;
    allowLight = true;

    name = "Pillar Quartz Block";
}
block_t PillarQuartzBlock::getID(){ return 230;}

white_concrete_powder::white_concrete_powder():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 12;

    downPlane_x = 12;
    downPlane_y = 12;

    leftPlane_x = 12;
    leftPlane_y = 12;

    rightPlane_x = 12;
    rightPlane_y = 12;

    botPlane_x = 12; 
    botPlane_y = 12;

    forPlane_x = 12;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "White Concrete Powder";
}
block_t white_concrete_powder::getID(){ return 231;}

/*orange_concrete_powder::orange_concrete_powder():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 11;

    downPlane_x = 12;
    downPlane_y = 11;

    leftPlane_x = 12;
    leftPlane_y = 11;

    rightPlane_x = 12;
    rightPlane_y = 11;

    botPlane_x = 12; 
    botPlane_y = 11;

    forPlane_x = 12;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Orange Concrete Powder";
}
block_t orange_concrete_powder::getID(){ return 233;}*/

/*magenta_concrete_powder::magenta_concrete_powder():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 11;

    downPlane_x = 11;
    downPlane_y = 11;

    leftPlane_x = 11;
    leftPlane_y = 11;

    rightPlane_x = 11;
    rightPlane_y = 11;

    botPlane_x = 11; 
    botPlane_y = 11;

    forPlane_x = 11;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Magenta Concrete Powder";
}
block_t magenta_concrete_powder::getID(){ return 236;}*/

/*light_blue_concrete_powder::light_blue_concrete_powder():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 13;

    downPlane_x = 9;
    downPlane_y = 13;

    leftPlane_x = 9;
    leftPlane_y = 13;

    rightPlane_x = 9;
    rightPlane_y = 13;

    botPlane_x = 9; 
    botPlane_y = 13;

    forPlane_x = 9;
    forPlane_y = 13;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Light Blue Concrete Powder";
}
block_t light_blue_concrete_powder::getID(){ return 243;}*/

yellow_concrete_powder::yellow_concrete_powder():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 11;

    downPlane_x = 8;
    downPlane_y = 11;

    leftPlane_x = 8;
    leftPlane_y = 11;

    rightPlane_x = 8;
    rightPlane_y = 11;

    botPlane_x = 8; 
    botPlane_y = 11;

    forPlane_x = 8;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Yellow Concrete Powder";
}
block_t yellow_concrete_powder::getID(){ return 232;}

/*lime_concrete_powder::lime_concrete_powder():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 11;

    downPlane_x = 9;
    downPlane_y = 11;

    leftPlane_x = 9;
    leftPlane_y = 11;

    rightPlane_x = 9;
    rightPlane_y = 11;

    botPlane_x = 9; 
    botPlane_y = 11;

    forPlane_x = 9;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Lime Concrete Powder";
}
block_t lime_concrete_powder::getID(){ return 238;}*/

pink_concrete_powder::pink_concrete_powder():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 11;

    downPlane_x = 10;
    downPlane_y = 11;

    leftPlane_x = 10;
    leftPlane_y = 11;

    rightPlane_x = 10;
    rightPlane_y = 11;

    botPlane_x = 10; 
    botPlane_y = 11;

    forPlane_x = 10;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Pink Concrete Powder";
}
block_t pink_concrete_powder::getID(){ return 233;}

gray_concrete_powder::gray_concrete_powder():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 12;

    downPlane_x = 10;
    downPlane_y = 12;

    leftPlane_x = 10;
    leftPlane_y = 12;

    rightPlane_x = 10;
    rightPlane_y = 12;

    botPlane_x = 10; 
    botPlane_y = 12;

    forPlane_x = 10;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Gray Concrete Powder";
}
block_t gray_concrete_powder::getID(){ return 234;}

/*light_gray_concrete_powder::light_gray_concrete_powder():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 12;

    downPlane_x = 11;
    downPlane_y = 12;

    leftPlane_x = 11;
    leftPlane_y = 12;

    rightPlane_x = 11;
    rightPlane_y = 12;

    botPlane_x = 11; 
    botPlane_y = 12;

    forPlane_x = 11;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Light Gray Concrete Powder";
}
block_t light_gray_concrete_powder::getID(){ return 248;}*/

/*cyan_concrete_powder::cyan_concrete_powder():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 13;

    downPlane_x = 10;
    downPlane_y = 13;

    leftPlane_x = 10;
    leftPlane_y = 13;

    rightPlane_x = 10;
    rightPlane_y = 13;

    botPlane_x = 10; 
    botPlane_y = 13;

    forPlane_x = 10;
    forPlane_y = 13;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Cyan Concrete Powder";
}
block_t cyan_concrete_powder::getID(){ return 249;}*/

purple_concrete_powder::purple_concrete_powder():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 13;

    downPlane_x = 11;
    downPlane_y = 13;

    leftPlane_x = 11;
    leftPlane_y = 13;

    rightPlane_x = 11;
    rightPlane_y = 13;

    botPlane_x = 11; 
    botPlane_y = 13;

    forPlane_x = 11;
    forPlane_y = 13;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Purple Concrete Powder";
}
block_t purple_concrete_powder::getID() { return 235; }

blue_concrete_powder::blue_concrete_powder():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 13;

    downPlane_x = 8;
    downPlane_y = 13;

    leftPlane_x = 8;
    leftPlane_y = 13;

    rightPlane_x = 8;
    rightPlane_y = 13;

    botPlane_x = 8; 
    botPlane_y = 13;

    forPlane_x = 8;
    forPlane_y = 13;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Blue Concrete Powder";
}
block_t blue_concrete_powder::getID(){ return 236; }

/*brown_concrete_powder::brown_concrete_powder():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 12;

    downPlane_x = 13;
    downPlane_y = 12;

    leftPlane_x = 13;
    leftPlane_y = 12;

    rightPlane_x = 13;
    rightPlane_y = 12;

    botPlane_x = 13; 
    botPlane_y = 12;

    forPlane_x = 13;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Brown Concrete Powder";
}
block_t brown_concrete_powder::getID() { return 238; }*/

green_concrete_powder::green_concrete_powder():BaseBlock()
{
    upPlane_x = 8;
    upPlane_y = 12;

    downPlane_x = 8;
    downPlane_y = 12;

    leftPlane_x = 8;
    leftPlane_y = 12;

    rightPlane_x = 8;
    rightPlane_y = 12;

    botPlane_x = 8; 
    botPlane_y = 12;

    forPlane_x = 8;
    forPlane_y = 12;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Green Concrete Powder";
}
block_t green_concrete_powder::getID(){ return 237; }

red_concrete_powder::red_concrete_powder():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 11;

    downPlane_x = 13;
    downPlane_y = 11;

    leftPlane_x = 13;
    leftPlane_y = 11;

    rightPlane_x = 13;
    rightPlane_y = 11;

    botPlane_x = 13; 
    botPlane_y = 11;

    forPlane_x = 13;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;
	material = 3;

    loot = -1;
    solid = true;
    allowLight = true;

    name = "Red Concrete Powder";
}
block_t red_concrete_powder::getID(){ return 238; }

black_concrete_powder::black_concrete_powder():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 11;

    downPlane_x = 10;
    downPlane_y = 11;

    leftPlane_x = 10;
    leftPlane_y = 11;

    rightPlane_x = 10;
    rightPlane_y = 11;

    botPlane_x = 10; 
    botPlane_y = 11;

    forPlane_x = 10;
    forPlane_y = 11;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	material = 3;
    loot = -1;
    solid = true;
    allowLight = true;

    name = "Black Concrete Powder";
}
block_t black_concrete_powder::getID(){ return 239; }

YellowGlassBlockPannel::YellowGlassBlockPannel():BaseBlock()
{
    upPlane_x = 9;
    upPlane_y = 9;

    downPlane_x = 9;
    downPlane_y = 9;

    leftPlane_x = 9;
    leftPlane_y = 9;

    rightPlane_x = 9;
    rightPlane_y = 9;

    botPlane_x = 9;
    botPlane_y = 9;

    forPlane_x = 9;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Yellow Stained Glass Pane";
}
block_t YellowGlassBlockPannel::getID() { return 240; }

GreenGlassBlockPannel::GreenGlassBlockPannel():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 9;

    downPlane_x = 10;
    downPlane_y = 9;

    leftPlane_x = 10;
    leftPlane_y = 9;

    rightPlane_x = 10;
    rightPlane_y = 9;

    botPlane_x = 10;
    botPlane_y = 9;

    forPlane_x = 10;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Green Stained Glass Pane";
}
block_t GreenGlassBlockPannel::getID(){ return 241; }

PinkGlassBlockPannel::PinkGlassBlockPannel():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 9;

    downPlane_x = 11;
    downPlane_y = 9;

    leftPlane_x = 11;
    leftPlane_y = 9;

    rightPlane_x = 11;
    rightPlane_y = 9;

    botPlane_x = 11;
    botPlane_y = 9;

    forPlane_x = 11;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Pink Stained Glass Pane";
}
block_t PinkGlassBlockPannel::getID(){ return 242; }

PurpleGlassBlockPannel::PurpleGlassBlockPannel():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 9;

    downPlane_x = 12;
    downPlane_y = 9;

    leftPlane_x = 12;
    leftPlane_y = 9;

    rightPlane_x = 12;
    rightPlane_y = 9;

    botPlane_x = 12;
    botPlane_y = 9;

    forPlane_x = 12;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Purple Stained Glass Pane";
}
block_t PurpleGlassBlockPannel::getID(){ return 243; }

/*OrangeGlassBlockPannel::OrangeGlassBlockPannel():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 9;

    downPlane_x = 13;
    downPlane_y = 9;

    leftPlane_x = 13;
    leftPlane_y = 9;

    rightPlane_x = 13;
    rightPlane_y = 9;

    botPlane_x = 13;
    botPlane_y = 9;

    forPlane_x = 13;
    forPlane_y = 9;
	
	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Orange Stained Glass Pane";
}
block_t OrangeGlassBlockPannel::getID(){ return 247; }*/

RedGlassBlockPannel::RedGlassBlockPannel():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 9;

    downPlane_x = 14;
    downPlane_y = 9;

    leftPlane_x = 14;
    leftPlane_y = 9;

    rightPlane_x = 14;
    rightPlane_y = 9;

    botPlane_x = 14;
    botPlane_y = 9;

    forPlane_x = 14;
    forPlane_y = 9;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Red Stained Glass Pane";
}
block_t RedGlassBlockPannel::getID(){ return 244; }

/*DGreenGlassBlockPannel::DGreenGlassBlockPannel():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 9;

    downPlane_x = 15;
    downPlane_y = 9;

    leftPlane_x = 15;
    leftPlane_y = 9;

    rightPlane_x = 15;
    rightPlane_y = 9;

    botPlane_x = 15;
    botPlane_y = 9;

    forPlane_x = 15;
    forPlane_y = 9;
	
	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;
	refraction = 1;

    name = "Dark Green Stained Glass Pane";
}
block_t DGreenGlassBlockPannel::getID(){ return -14; }*/

BlackGlassBlockPannel::BlackGlassBlockPannel():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 10;

    downPlane_x = 10;
    downPlane_y = 10;

    leftPlane_x = 10;
    leftPlane_y = 10;

    rightPlane_x = 10;
    rightPlane_y = 10;

    botPlane_x = 10;
    botPlane_y = 10;

    forPlane_x = 10;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Black Stained Glass Pane";
}
block_t BlackGlassBlockPannel::getID(){ return 245; }

/*LGrayGlassBlockPannel::LGrayGlassBlockPannel():BaseBlock()
{
    upPlane_x = 11;
    upPlane_y = 10;

    downPlane_x = 11;
    downPlane_y = 10;

    leftPlane_x = 11;
    leftPlane_y = 10;

    rightPlane_x = 11;
    rightPlane_y = 10;

    botPlane_x = 11;
    botPlane_y = 10;

    forPlane_x = 11;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Light Gray Stained Glass Pane";
}
block_t LGrayGlassBlockPannel::getID(){ return -16; }*/

/*GrayGlassBlockPannel::GrayGlassBlockPannel():BaseBlock()
{
    upPlane_x = 10;
    upPlane_y = 10;

    downPlane_x = 10;
    downPlane_y = 10;

    leftPlane_x = 10;
    leftPlane_y = 10;

    rightPlane_x = 10;
    rightPlane_y = 10;

    botPlane_x = 10;
    botPlane_y = 10;

    forPlane_x = 10;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Gray Stained Glass Pane";
}
block_t GrayGlassBlockPannel::getID(){ return 256; }*/

/*WhiteGlassBlockPannel::WhiteGlassBlockPannel():BaseBlock()
{
    upPlane_x = 12;
    upPlane_y = 10;

    downPlane_x = 12;
    downPlane_y = 10;

    leftPlane_x = 12;
    leftPlane_y = 10;

    rightPlane_x = 12;
    rightPlane_y = 10;

    botPlane_x = 12;
    botPlane_y = 10;

    forPlane_x = 12;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "White Stained Glass Pane";
}
block_t WhiteGlassBlockPannel::getID(){ return 252; }*/

/*BrownGlassBlockPannel::BrownGlassBlockPannel():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 10;

    downPlane_x = 13;
    downPlane_y = 10;

    leftPlane_x = 13;
    leftPlane_y = 10;

    rightPlane_x = 13;
    rightPlane_y = 10;

    botPlane_x = 13;
    botPlane_y = 10;

    forPlane_x = 13;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Brown Stained Glass Pane";
}
block_t BrownGlassBlockPannel::getID(){ return 251; }

DBlueGlassBlockPannel::DBlueGlassBlockPannel():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 10;

    downPlane_x = 14;
    downPlane_y = 10;

    leftPlane_x = 14;
    leftPlane_y = 10;

    rightPlane_x = 14;
    rightPlane_y = 10;

    botPlane_x = 14;
    botPlane_y = 10;

    forPlane_x = 14;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Dark Blue Stained Glass Pane";
}
block_t DBlueGlassBlockPannel::getID(){ return 259; }*/

/*LBlueGlassBlockPannel::LBlueGlassBlockPannel():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 10;

    downPlane_x = 15;
    downPlane_y = 10;

    leftPlane_x = 15;
    leftPlane_y = 10;

    rightPlane_x = 15;
    rightPlane_y = 10;

    botPlane_x = 15;
    botPlane_y = 10;

    forPlane_x = 15;
    forPlane_y = 10;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Light Blue Stained Glass Pane";
}
block_t LBlueGlassBlockPannel::getID(){ return -21; }*/

BlueGlassBlockPannel::BlueGlassBlockPannel():BaseBlock()
{
    upPlane_x = 14;
    upPlane_y = 11;

    downPlane_x = 14;
    downPlane_y = 11;

    leftPlane_x = 14;
    leftPlane_y = 11;

    rightPlane_x = 14;
    rightPlane_y = 11;

    botPlane_x = 14;
    botPlane_y = 11;

    forPlane_x = 14;
    forPlane_y = 11;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Blue Stained Glass Pane";
}
block_t BlueGlassBlockPannel::getID(){ return 246; }

/*DPurpleGlassBlockPannel::DPurpleGlassBlockPannel():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 11;

    downPlane_x = 15;
    downPlane_y = 11;

    leftPlane_x = 15;
    leftPlane_y = 11;

    rightPlane_x = 15;
    rightPlane_y = 11;

    botPlane_x = 15;
    botPlane_y = 11;

    forPlane_x = 15;
    forPlane_y = 11;

	editable = true;
	transparent = true;
	solid = true;
    allowLight = true;

	soundType = 2;
    loot = -1;
    blockModel = 10;

    name = "Dark Purple Stained Glass Pane";
}
block_t DPurpleGlassBlockPannel::getID(){ return -23; }*/

/*redstone_lamp::redstone_lamp():BaseBlock()
{
    upPlane_x = 4;
    upPlane_y = 12;

    downPlane_x = 4;
    downPlane_y = 12;

    leftPlane_x = 4;
    leftPlane_y = 12;

    rightPlane_x = 4;
    rightPlane_y = 12;

    botPlane_x = 4;
    botPlane_y = 12;

    forPlane_x = 4;
    forPlane_y = 12;

    editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;

    loot = -1;
    solid = true;
    allowLight = true;

	name = "Redstone Lamp";
}
block_t redstone_lamp::getID(){ return -24; }*/

/*lit_redstone_lamp::lit_redstone_lamp():BaseBlock()
{
    upPlane_x = 5;
    upPlane_y = 12;

    downPlane_x = 5;
    downPlane_y = 12;

    leftPlane_x = 5;
    leftPlane_y = 12;

    rightPlane_x = 5;
    rightPlane_y = 12;

    botPlane_x = 5;
    botPlane_y = 12;

    forPlane_x = 5;
    forPlane_y = 12;

    editable = true;
	transparent = true;
	allowLight = false;
	soundType = 2;

    loot = -1;
    solid = true;
	lightSource = true;

	name = "Lit Redstone Lamp";
}
block_t lit_redstone_lamp::getID(){ return -25; }*/

Fire::Fire():BaseBlock()
{
    upPlane_x = 13;
    upPlane_y = 15;

    downPlane_x = 13;
    downPlane_y = 15;

    leftPlane_x = 13;
    leftPlane_y = 15;

    rightPlane_x = 13;
    rightPlane_y = 15;

    botPlane_x = 13;
    botPlane_y = 15;

    forPlane_x = 13;
    forPlane_y = 15;

	editable = true;
	transparent = true;
	allowLight = true;
	blockModel = 13;
	loot = -1;
    solid = false;
	lightSource = true;
	animated = true;
	refraction = 1;
	material = 11;

    name = "Fire";
}
block_t Fire::getID(){ return 247; }

PortalBlock::PortalBlock():BaseBlock()
{
    // Tile (15,8): empty in every shipped texture pack, which is the reason it was picked. Water, lava
    // and fire have their own tiles on the bottom row; this is the free one that does not collide with
    // anything a pack already draws.
    upPlane_x = 15;
    upPlane_y = 8;

    downPlane_x = 15;
    downPlane_y = 8;

    leftPlane_x = 15;
    leftPlane_y = 8;

    rightPlane_x = 15;
    rightPlane_y = 8;

    botPlane_x = 15;
    botPlane_y = 8;

    forPlane_x = 15;
    forPlane_y = 8;

    editable = false;   // the frame is the block you work with, never the portal itself
    transparent = true;
    allowLight = true;
    solid = false;      // you walk into it, not onto it
    lightSource = true;
    animated = true;
    blockModel = 30;    // the flat portal sheet, drawn in CraftWorld_MeshBlocks.cpp
    loot = -1;          // breaking it yields nothing
    refraction = 1;
    material = 11;      // the same class as fire: walk-through, not a floor

    name = "Nether Portal";
}
block_t PortalBlock::getID(){ return kExtendedBlockBase + 26; }

// The same block in the other plane. Everything is identical to PortalBlock except the id, which is
// the only thing that tells the mesher which way the sheet faces; see the comment in Blocks.h.
PortalBlockZ::PortalBlockZ():BaseBlock()
{
    upPlane_x = 15;
    upPlane_y = 8;

    downPlane_x = 15;
    downPlane_y = 8;

    leftPlane_x = 15;
    leftPlane_y = 8;

    rightPlane_x = 15;
    rightPlane_y = 8;

    botPlane_x = 15;
    botPlane_y = 8;

    forPlane_x = 15;
    forPlane_y = 8;

    editable = false;
    transparent = true;
    allowLight = true;
    solid = false;
    lightSource = true;
    animated = true;
    blockModel = 30;
    loot = -1;
    refraction = 1;
    material = 11;

    name = "Nether Portal";
}
block_t PortalBlockZ::getID(){ return kExtendedBlockBase + 27; }
GravitySandBlock::GravitySandBlock() :BaseBlock()
{
	upPlane_x = 2;
	upPlane_y = 1;

	downPlane_x = 2;
	downPlane_y = 1;

	leftPlane_x = 2;
	leftPlane_y = 1;

	rightPlane_x = 2;
	rightPlane_y = 1;

	botPlane_x = 2;
	botPlane_y = 1;

	forPlane_x = 2;
	forPlane_y = 1;

	editable = true;
	transparent = false;
	allowLight = false;
	soundType = 5;

	loot = 248;
	material = 3;
	update = 1;
	solid = true;

	furnItem = 40;

	name = "Sand";
}
block_t GravitySandBlock::getID() { return 248; }

static void InitFlowingLava(BaseBlock* b)
{
	b->upPlane_x = 15;
	b->upPlane_y = 15;
	b->downPlane_x = 15;
	b->downPlane_y = 15;
	b->leftPlane_x = 15;
	b->leftPlane_y = 15;
	b->rightPlane_x = 15;
	b->rightPlane_y = 15;
	b->botPlane_x = 15;
	b->botPlane_y = 15;
	b->forPlane_x = 15;
	b->forPlane_y = 15;
	b->editable = false;
	b->transparent = false;
	b->lightSource = true;
	b->allowLight = true;
	b->solid = false;
	b->animated = true;
	b->blockModel = 5;
	b->soundType = 2;
	b->name = "Lava Block";
}

LavaBlock1::LavaBlock1():BaseBlock()
{
	InitFlowingLava(this);
}
block_t LavaBlock1::getID(){ return 249; }

// Button. 1.7.3 shipped no button texture: a stone button was drawn with the stone tile and the wooden
// button with the oak planks tile (buttons only got their own textures in 1.8), so no atlas work here.
// Facing and the pressed flag are per-block state, not per-id, so one id covers all six mountings.
static void InitButton(BaseBlock& b, bool wood)
{
	if(wood)
	{
		b.upPlane_x = 4;
		b.upPlane_y = 0;
	}
	else
	{
		b.upPlane_x = 1;
		b.upPlane_y = 0;
	}
	b.downPlane_x = b.upPlane_x;
	b.downPlane_y = b.upPlane_y;
	b.leftPlane_x = b.upPlane_x;
	b.leftPlane_y = b.upPlane_y;
	b.rightPlane_x = b.upPlane_x;
	b.rightPlane_y = b.upPlane_y;
	b.botPlane_x = b.upPlane_x;
	b.botPlane_y = b.upPlane_y;
	b.forPlane_x = b.upPlane_x;
	b.forPlane_y = b.upPlane_y;

	b.editable = true;
	b.transparent = true;
	b.allowLight = true;
	b.solid = false;
	b.blockModel = 14;
	b.soundType = 2;
	b.name = wood ? "Wooden Button" : "Stone Button";
	// Fallback box only: a button's real box depends on the cell's facing and pressed state, and
	// CraftWorld::GetBlockCollideBox() supplies it. This is the floor-mounted rest shape, centred.
	b.collideBox = BoundingBox(Vector3(-3.0f/16.0f,-0.5f,-3.0f/16.0f),Vector3(3.0f/16.0f,-0.5f+2.0f/16.0f,3.0f/16.0f));
}

ButtonStone::ButtonStone():BaseBlock()
{
	InitButton(*this, false);
	loot = ButtonStone::getID();
}
block_t ButtonStone::getID(){ return kExtendedBlockBase + 3; }

ButtonWood::ButtonWood():BaseBlock()
{
	InitButton(*this, true);
	loot = ButtonWood::getID();
}
block_t ButtonWood::getID(){ return kExtendedBlockBase + 4; }

// Lever. 1.7.3 shipped no lever texture worth speaking of either: the lever *tile* is the handle only
// (a 2 px strip down columns 7-8 of the sprite) and the base is drawn with the cobblestone tile, which
// is exactly how the vanilla model is put together -- `base` = cobblestone, `lever` = lever.png. So the
// atlas needs no new art: cobblestone is (0,1) and the handle is (0,6).
//
// Facing and the on/off latch live in the same state nibble the button uses, so a lever gets the wire,
// torch, TNT and lamp wiring for free by answering IsSwitchBlock. blockModel 16 is its own mesh: the
// button's uniform box cannot express a base with a handle leaning out of it.
static void InitLever(BaseBlock& b)
{
	b.upPlane_x = 0;  b.upPlane_y = 6;   // the handle's tile; the base is drawn from cobblestone instead
	b.downPlane_x = 0; b.downPlane_y = 6;
	b.leftPlane_x = 0; b.leftPlane_y = 6;
	b.rightPlane_x = 0; b.rightPlane_y = 6;
	b.botPlane_x = 0; b.botPlane_y = 6;
	b.forPlane_x = 0; b.forPlane_y = 6;

	b.editable = true;
	b.transparent = true;
	b.allowLight = true;
	b.solid = false;
	b.blockModel = 16;
	b.soundType = 2;          // Java plays the wood click for a lever, as it does for a torch
	b.name = "Lever";

	// Java BlockLever.setBlockBoundsBasedOnState for a floor-mounted lever: an 8x8 px footprint six
	// pixels tall, which is the base *and* the room the leaning handle needs. CraftWorld::LeverCellBox
	// derives the real box per mounting, and the pick ray and the outline share it.
	b.collideBox = BoundingBox(Vector3(-4.0f/16.0f,-0.5f,-4.0f/16.0f),Vector3(4.0f/16.0f,-0.5f+6.0f/16.0f,4.0f/16.0f));
}

Lever::Lever():BaseBlock()
{
	InitLever(*this);
	loot = Lever::getID();
}
block_t Lever::getID(){ return kExtendedBlockBase + 17; }

// Redstone lamp. Vanilla atlas pair (4,12) off and (5,12) on: a plain opaque cube, so blockModel 0 and
// the normal-cube mesher are exactly right. `solid` matters more than it looks: GetBlockRedstonePower
// only answers for conductive blocks, and that is the function the lamp reads, so the lamp has to be a
// normal cube to be powered by the dust on top of it or pointing into it.
static void InitRedstoneLamp(BaseBlock& b, bool lit)
{
	b.upPlane_x = lit ? 5 : 4;   b.upPlane_y = 12;
	b.downPlane_x = lit ? 5 : 4; b.downPlane_y = 12;
	b.leftPlane_x = lit ? 5 : 4; b.leftPlane_y = 12;
	b.rightPlane_x = lit ? 5 : 4; b.rightPlane_y = 12;
	b.botPlane_x = lit ? 5 : 4;  b.botPlane_y = 12;
	b.forPlane_x = lit ? 5 : 4;  b.forPlane_y = 12;

	b.editable = true;
	b.transparent = false;
	b.allowLight = false;
	b.solid = true;
	b.lightSource = lit;         // the lit id is a light source, the dark one must not be
	b.soundType = 2;

	b.loot = RedstoneLampOff::getID();   // a lamp breaks back to the dark one, whatever state it is in
	b.name = lit ? "Redstone Lamp (lit)" : "Redstone Lamp";

	b.collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0.5f,0.5f));
}

RedstoneLampOff::RedstoneLampOff():BaseBlock(){ InitRedstoneLamp(*this, false); }
block_t RedstoneLampOff::getID(){ return kExtendedBlockBase + 18; }

RedstoneLampOn::RedstoneLampOn():BaseBlock(){ InitRedstoneLamp(*this, true); }
block_t RedstoneLampOn::getID(){ return kExtendedBlockBase + 19; }

// Java BlockRedstoneRepeater. Tiles are the vanilla atlas pair at (3,8)/(3,9): a smooth-stone slab with
// the torch row baked in, the lit variant of the same art. upPlane carries the top tile, so RepeaterOn
// simply points at the lit one and the mesher needs no per-state texture switch.
static void InitRepeater(BaseBlock& b, bool powered)
{
	b.upPlane_x = 3;
	b.upPlane_y = powered ? 9 : 8;
	// Sides and underside are the smooth stone slab; a repeater is only 2 px tall so they are a thin
	// grey rim, matching the base baked into the top tile.
	b.downPlane_x = 5;  b.downPlane_y = 0;
	b.leftPlane_x = 5;  b.leftPlane_y = 0;
	b.rightPlane_x = 5; b.rightPlane_y = 0;
	b.botPlane_x = 5;   b.botPlane_y = 0;
	b.forPlane_x = 5;   b.forPlane_y = 0;

	b.editable = true;
	b.transparent = true;
	b.allowLight = true;
	b.solid = false;
	b.blockModel = 15;
	b.soundType = 2;
	b.name = powered ? "Redstone Repeater (on)" : "Redstone Repeater";

	// Java BlockRedstoneRepeater.setBlockBounds: a 2 px slab, centred on the cell like every other box.
	b.collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,-0.5f+2.0f/16.0f,0.5f));
}

RepeaterOff::RepeaterOff():BaseBlock()
{
	InitRepeater(*this, false);
	// The item counterpart, not a block id: the inventory icons a repeater as a flat sprite, and only an
	// item id reaches the flat-sprite draw path. Java drops the item form too. Written as the literal id
	// because this file sees only Blocks.h, the same reason the torch above uses 408.
	loot = 409;               // RepeaterItem
}
block_t RepeaterOff::getID(){ return kExtendedBlockBase + 5; }

RepeaterOn::RepeaterOn():BaseBlock()
{
	InitRepeater(*this, true);
	loot = 409;               // RepeaterItem
}
block_t RepeaterOn::getID(){ return kExtendedBlockBase + 6; }

// Java BlockRedstoneTorch. Same geometry and tile layout as the plain torch, but the sprite carries a
// redstone tip: terrain (3,6) lit, (3,7) unlit. The lit/unlit pair are block ids five apart so the
// scheduler can flip a torch with an id add and the settings nibble stays free for block light.
static void InitRedstoneTorch(BaseBlock& b, bool lit)
{
	b.upPlane_x = 3;
	b.upPlane_y = lit ? 6 : 7;
	b.downPlane_x = 3;
	b.downPlane_y = lit ? 6 : 7;
	b.leftPlane_x = 3;
	b.leftPlane_y = lit ? 6 : 7;
	b.rightPlane_x = 3;
	b.rightPlane_y = lit ? 6 : 7;
	b.botPlane_x = 3;
	b.botPlane_y = lit ? 6 : 7;
	b.forPlane_x = 3;
	b.forPlane_y = lit ? 6 : 7;

	b.editable = true;
	b.transparent = true;
	b.soundType = 2;          // Java plays the wood click for torches
	b.lightSource = lit;      // an unlit torch must stop lighting its cell
	b.blockModel = 5;         // the plain-torch mesher, reused unchanged
	b.allowLight = true;
	b.solid = false;

	b.loot = 408;             // RedstoneTorchItem, the item counterpart of both the lit and unlit family
	b.name = lit ? "Redstone Torch" : "Redstone Torch (off)";

	b.collideBox = BoundingBox(Vector3(-0.06f,-0.5f,-0.06f),Vector3(0.06f,0.15f,0.06f));
}

RedstoneTorch::RedstoneTorch():BaseBlock(){ InitRedstoneTorch(*this, true); }
block_t RedstoneTorch::getID(){ return kExtendedBlockBase + 7; }

RedstoneWallTorch1::RedstoneWallTorch1():BaseBlock(){ InitRedstoneTorch(*this, true); }
block_t RedstoneWallTorch1::getID(){ return kExtendedBlockBase + 8; }

RedstoneWallTorch2::RedstoneWallTorch2():BaseBlock(){ InitRedstoneTorch(*this, true); }
block_t RedstoneWallTorch2::getID(){ return kExtendedBlockBase + 9; }

RedstoneWallTorch3::RedstoneWallTorch3():BaseBlock(){ InitRedstoneTorch(*this, true); }
block_t RedstoneWallTorch3::getID(){ return kExtendedBlockBase + 10; }

RedstoneWallTorch4::RedstoneWallTorch4():BaseBlock(){ InitRedstoneTorch(*this, true); }
block_t RedstoneWallTorch4::getID(){ return kExtendedBlockBase + 11; }

UnlitRedstoneTorch::UnlitRedstoneTorch():BaseBlock(){ InitRedstoneTorch(*this, false); }
block_t UnlitRedstoneTorch::getID(){ return kExtendedBlockBase + 12; }

UnlitRedstoneWallTorch1::UnlitRedstoneWallTorch1():BaseBlock(){ InitRedstoneTorch(*this, false); }
block_t UnlitRedstoneWallTorch1::getID(){ return kExtendedBlockBase + 13; }

UnlitRedstoneWallTorch2::UnlitRedstoneWallTorch2():BaseBlock(){ InitRedstoneTorch(*this, false); }
block_t UnlitRedstoneWallTorch2::getID(){ return kExtendedBlockBase + 14; }

UnlitRedstoneWallTorch3::UnlitRedstoneWallTorch3():BaseBlock(){ InitRedstoneTorch(*this, false); }
block_t UnlitRedstoneWallTorch3::getID(){ return kExtendedBlockBase + 15; }

UnlitRedstoneWallTorch4::UnlitRedstoneWallTorch4():BaseBlock(){ InitRedstoneTorch(*this, false); }
block_t UnlitRedstoneWallTorch4::getID(){ return kExtendedBlockBase + 16; }

// --- Piston (Java BlockPistonBase, plus BlockPistonExtension for the head) ------------------------
//
// Atlas: four new tiles, staged at (15,1) piston_top_normal, (15,2) piston_top_sticky, (15,3)
// piston_side and (15,14) piston_inner, in every pack and in both terrain.png and terrain_mips.png.
// The last of those is a *spare slot* -- one of the cells vanilla painted with flat grey filler and
// never gave a texture to, the same pattern that sits at (8,2) -- so nothing loses art by it. The
// first choice was (14,2), which is painted in all seven packs and is the classic dispenser front in
// the two that carry vanilla's own art; the port has no dispenser yet, but changes.md has one listed,
// so that cell belongs to it. See add-block-sprites.py's docstring.
// There is no fifth cell to stage: vanilla's piston_bottom.png is byte for byte furnace_top.png in the
// 1.7.3 assets (same md5), so the back of a piston reuses the furnace-top cell (14,3) that the furnace
// already owns.
//
// piston_inner is the face at the bottom of the recess an extended base leaves open -- a stone face
// with a bright ring and a 4x4 hole painted where the rod comes out. It is only ever seen 4 px behind
// the rod, so for a while the mesher painted it with the head tile and nobody could tell; the recess
// was being drawn 12 px too shallow at the time (see InitPistonBase's collideBox note), so the face
// was in the dark between two plates. With the vanilla 12 px body it is the middle of the piston and
// the one face the rod appears to come *out* of, which is what the ring is drawn for.
//
// Which field carries which tile matters, because the two models read them by *role* and not by
// direction: forPlane is the head face -- the 16x16 end that pushes, which is what vanilla calls
// piston_top -- and botPlane is the back. Naming them that way is deliberate even though forPlane is
// geometrically +Z. A piston is hand-emitted from its state nibble (model 17 for the base, 18 for the
// head, see CraftWorld_MeshBlocks.cpp), so the field names only have to line up with the roles the
// mesher asks for; and with forPlane = +Z, a piston pushing +Z is also correct if anything ever renders
// it down the plain cube path by mistake. The other four fields all hold the side tile.
static void InitPistonBase(BaseBlock& b, bool sticky, bool extended)
{
	b.forPlane_x = 15; b.forPlane_y = sticky ? 2 : 1;
	b.botPlane_x = 14; b.botPlane_y = 3;   // vanilla piston_bottom, the same art as furnace_top
	b.innerPlane_x = 15; b.innerPlane_y = 14;   // vanilla piston_inner, in one of vanilla's spare slots

	b.upPlane_x = 15;    b.upPlane_y = 3;
	b.downPlane_x = 15;  b.downPlane_y = 3;
	b.leftPlane_x = 15;  b.leftPlane_y = 3;
	b.rightPlane_x = 15; b.rightPlane_y = 3;

	b.editable = true;
	b.blockModel = 17;
	b.soundType = 2;        // stone, the sound vanilla's piston material reports
	b.lightSource = false;

	// The retracted base is a full opaque cube and so hides the faces of whatever it touches. The
	// extended base is a 12 px stone body with a 4x4 rod standing out of the recess it leaves, and air
	// around the rod, so it must not -- and `transparent` is the flag the mesher reads for that, which
	// is the reason the two states cannot share one block type even though they share a model id.
	// `solid` stays true in both: it is the *conductive* flag (IsRedstoneConductive reads exactly
	// this), and an extended piston is still an opaque block to the redstone rules, exactly as it is in
	// vanilla.
	b.transparent = extended;
	b.allowLight = extended;
	b.solid = true;

	if(extended)
	{
		// One axis-aligned box cannot describe "a 12 px body with the last 4 px open, plus a rod
		// through the middle of the opening" in all six directions at once, and every collision caller
		// goes through GetBlockCollideBox, which recomputes this from the state nibble. So what is
		// stored here is only the reference orientation: a piston pushing +Z, body against the -Z wall
		// of its cell -- the same 12 px the overridden PistonCellBox gives it in the other five.
		//
		// The box is the *body*, which is what makes it worth writing down: an earlier revision had it
		// 4 px against the back wall, on the reading that the plate was all an extended base drew. That
		// is the 4x4 rod's own cell, so the player could walk through eight twelfths of a solid piston
		// body -- and it is what PistonCellBox says too, so the two agreed on the wrong number.
		b.collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0.5f,0.25f));
	}
	else
	{
		b.collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0.5f,0.5f));
	}

	b.name = sticky ? (extended ? "Sticky Piston (extended)" : "Sticky Piston")
	                : (extended ? "Piston (extended)" : "Piston");
}

PistonOff::PistonOff():BaseBlock()
{
	InitPistonBase(*this, false, false);
	// Both states break back to the retracted id, the same way the lit lamp breaks back to the dark
	// one: a piston is a piston however far out its head happens to be.
	loot = PistonOff::getID();
}
block_t PistonOff::getID(){ return kExtendedBlockBase + 20; }

PistonOn::PistonOn():BaseBlock()
{
	InitPistonBase(*this, false, true);
	loot = PistonOff::getID();
}
block_t PistonOn::getID(){ return kExtendedBlockBase + 21; }

StickyPistonOff::StickyPistonOff():BaseBlock()
{
	InitPistonBase(*this, true, false);
	loot = StickyPistonOff::getID();
}
block_t StickyPistonOff::getID(){ return kExtendedBlockBase + 22; }

StickyPistonOn::StickyPistonOn():BaseBlock()
{
	InitPistonBase(*this, true, true);
	loot = StickyPistonOff::getID();
}
block_t StickyPistonOn::getID(){ return kExtendedBlockBase + 23; }

// The moving half: a 4 px plate across the whole face of its cell, at the far end of it, plus the 4x4
// rod that runs back through the other twelve. The base it left behind draws the rest of that rod --
// 4 more px, in the recess its 12 px body leaves open -- so the two halves together are one 16 px rod
// standing the full block a piston pushes, exactly the 16 px vanilla's piston_head model draws between
// the plate's back and the base's recessed face. Splitting it across the two cells is what keeps every
// chunk's mesh inside its own cell, which the per-chunk mesher needs and vanilla's renderer, drawing
// one block at a time, never had to care about.
//
// Only two tiles are used. The plate takes the head tile on both of its 16x16 faces and the arm band
// (the top four rows of piston_side) on its four 4 px flanks, as vanilla's extension does; the rod
// takes the arm band too, and vanilla stretches all four of those rows down the rod's whole length
// (its piston_head gives the rod's sides uv [0,0,16,4]). So the rod is the one box in this family whose
// art is stretched, and both halves stretch it the same way -- 1 row per 4 px -- which is what keeps
// the band continuous where the two cells meet.
static void InitPistonHead(BaseBlock& b, bool sticky)
{
	b.forPlane_x = 15; b.forPlane_y = sticky ? 2 : 1;

	b.upPlane_x = 15;    b.upPlane_y = 3;
	b.downPlane_x = 15;  b.downPlane_y = 3;
	b.leftPlane_x = 15;  b.leftPlane_y = 3;
	b.rightPlane_x = 15; b.rightPlane_y = 3;

	// The plate's back face -- the ring of it that would be left visible around the rod -- takes the
	// same head tile as its front, which is what vanilla's extension does and what `botPlane` cannot
	// mean for a model this one does not share with the base. The rod's own flanks come out of
	// `piston_side`'s arm band, which is why the side fields above point at the same cell: the mesher
	// reads one of them for the flanks and ignores the rest.
	b.botPlane_x = 15;   b.botPlane_y = sticky ? 2 : 1;

	b.editable = true;
	b.blockModel = 18;
	b.soundType = 2;
	b.transparent = true;   // the cell is a rod standing in air
	b.allowLight = true;
	b.solid = false;        // nothing can be built on a rod, and nothing conducts through it
	b.lightSource = false;

	// Java gives BlockPistonExtension the default bounds, i.e. the whole cell, even though the plate
	// only fills 4 px of it. Matching that keeps the player's collider and the pick ray agreeing with
	// vanilla about a block that is mostly air, and it is the same box in all six directions.
	b.collideBox = BoundingBox(Vector3(-0.5f,-0.5f,-0.5f),Vector3(0.5f,0.5f,0.5f));

	// Java drops nothing for the extension, and a broken head must not hand out a free piston.
	// DropThis ignores any id <= 0, so 0 is how "no drop" is spelled to it.
	b.loot = 0;

	b.name = sticky ? "Sticky Piston Head" : "Piston Head";
}

PistonHead::PistonHead():BaseBlock(){ InitPistonHead(*this, false); }
block_t PistonHead::getID(){ return kExtendedBlockBase + 24; }

StickyPistonHead::StickyPistonHead():BaseBlock(){ InitPistonHead(*this, true); }
block_t StickyPistonHead::getID(){ return kExtendedBlockBase + 25; }
