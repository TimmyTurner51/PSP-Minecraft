#ifndef BLOCKS_H
#define BLOCKS_H

#include "BaseBlock.h"

class GrassBlock : public BaseBlock
{
public:

	GrassBlock();

	static block_t getID();
};

class DirtBlock : public BaseBlock
{
public:

	DirtBlock();
	static block_t getID();
};

class RockBlock : public BaseBlock
{
public:

	RockBlock();
	static block_t getID();
};

class WaterBlock : public BaseBlock
{
public:

	WaterBlock();
	static block_t getID();
};

class GoldBlock : public BaseBlock
{
public:

	GoldBlock();
	static block_t getID();
};

class LadderBlock : public BaseBlock
{
public:

	LadderBlock();
	static block_t getID();
};

class SandBlock : public BaseBlock
{
public:

	SandBlock();
	static block_t getID();
};

class WoodBlock : public BaseBlock
{
public:

	WoodBlock();
	static block_t getID();
};

class LeavesBlock : public BaseBlock
{
public:

	LeavesBlock();
	static block_t getID();
};

class BlackWoolBlock : public BaseBlock
{
public:

	BlackWoolBlock();
	static block_t getID();
};

class GrayWoolBlock : public BaseBlock
{
public:

	GrayWoolBlock();
	static block_t getID();
};

class RedWoolBlock : public BaseBlock
{
public:

	RedWoolBlock();
	static block_t getID();
};

class PinkWoolBlock : public BaseBlock
{
public:

	PinkWoolBlock();
	static block_t getID();
};

class DarkGreenWoolBlock : public BaseBlock
{
public:

	DarkGreenWoolBlock();
	static block_t getID();
};

class GreenWoolBlock : public BaseBlock
{
public:

	GreenWoolBlock();
	static block_t getID();
};

class BrownWoolBlock : public BaseBlock
{
public:

	BrownWoolBlock();
	static block_t getID();
};

class YellowWoolBlock : public BaseBlock
{
public:

	YellowWoolBlock();
	static block_t getID();
};

class BlueWoolBlock : public BaseBlock
{
public:

	BlueWoolBlock();
	static block_t getID();
};

class LightBlueWoolBlock : public BaseBlock
{
public:

	LightBlueWoolBlock();
	static block_t getID();
};

class VioletWoolBlock : public BaseBlock
{
public:

	VioletWoolBlock();
	static block_t getID();
};

class PastelWoolBlock : public BaseBlock
{
public:

	PastelWoolBlock();
	static block_t getID();
};

class OrangeWoolBlock : public BaseBlock
{
public:

	OrangeWoolBlock();
	static block_t getID();
};

class LightGrayWoolBlock : public BaseBlock
{
public:

	LightGrayWoolBlock();
	static block_t getID();
};

class WhiteWoolBlock : public BaseBlock
{
public:

	WhiteWoolBlock();
	static block_t getID();
};

class CactusBlock : public BaseBlock
{
public:

	CactusBlock();
	static block_t getID();
};

class SandStone : public BaseBlock
{
public:

	SandStone();
	static block_t getID();
};

class BrickBlock : public BaseBlock
{
public:

	BrickBlock();
	static block_t getID();
};

class CageBlock : public BaseBlock
{
public:

	CageBlock();
	static block_t getID();
};

class SnowSoil : public BaseBlock
{
public:

	SnowSoil();
	static block_t getID();
};

class DarkWoodBlock : public BaseBlock
{
public:

	DarkWoodBlock();
	static block_t getID();
};

class WhiteWoodBlock : public BaseBlock
{
public:

	WhiteWoodBlock();
	static block_t getID();
};

class IceBlock : public BaseBlock
{
public:

	IceBlock();
	static block_t getID();
};

class CaneBlock : public BaseBlock
{
public:

	CaneBlock();
	static block_t getID();
};

class OakPlanks : public BaseBlock
{
public:

	OakPlanks();
	static block_t getID();
};

class ShelfBlock : public BaseBlock
{
public:

	ShelfBlock();
	static block_t getID();
};

class CobbleStone : public BaseBlock
{
public:

	CobbleStone();
	static block_t getID();
};

//
class SpruceLeaves : public BaseBlock
{
public:

	SpruceLeaves();
	static block_t getID();
};

class BirchLeaves : public BaseBlock
{
public:

	BirchLeaves();
	static block_t getID();
};

class Sponge : public BaseBlock
{
public:

	Sponge();
	static block_t getID();
};

class GlassBlock : public BaseBlock
{
public:

	GlassBlock();
	static block_t getID();
};

class IronOre : public BaseBlock
{
public:

	IronOre();
	static block_t getID();
};

class CoalOre : public BaseBlock
{
public:

	CoalOre();
	static block_t getID();
};

class DiamondOre : public BaseBlock
{
public:

	DiamondOre();
	static block_t getID();
};

class RedstoneOre : public BaseBlock
{
public:

	RedstoneOre();
	static block_t getID();
};

class LapisOre : public BaseBlock
{
public:

	LapisOre();
	static block_t getID();
};

class Obsidian : public BaseBlock
{
public:

	Obsidian();
	static block_t getID();
};

class SnowBlock : public BaseBlock
{
public:

	SnowBlock();
	static block_t getID();
};

class Torch : public BaseBlock
{
public:

	Torch();
	static block_t getID();
};

/// Wall torches (ladder-style IDs; settings bits are block light and cannot store face)
class WallTorch1 : public BaseBlock
{
public:
	WallTorch1();
	static block_t getID(); // attach -X
};

class WallTorch2 : public BaseBlock
{
public:
	WallTorch2();
	static block_t getID(); // attach +X
};

class WallTorch3 : public BaseBlock
{
public:
	WallTorch3();
	static block_t getID(); // attach -Z
};

class WallTorch4 : public BaseBlock
{
public:
	WallTorch4();
	static block_t getID(); // attach +Z
};

class Door1 : public BaseBlock
{
public:

	Door1();
	static block_t getID();
};

class Door2 : public BaseBlock
{
public:

	Door2();
	static block_t getID();
};

class Door3 : public BaseBlock
{
public:

	Door3();
	static block_t getID();
};

class Door4 : public BaseBlock
{
public:

	Door4();
	static block_t getID();
};

class Door5 : public BaseBlock
{
public:

	Door5();
	static block_t getID();
};

class Door6 : public BaseBlock
{
public:

	Door6();
	static block_t getID();
};

class Door7 : public BaseBlock
{
public:

	Door7();
	static block_t getID();
};

class Door8 : public BaseBlock
{
public:

    Door8();
    static block_t getID();
};

class Door9 : public BaseBlock
{
public:

    Door9();
    static block_t getID();
};

class Door10 : public BaseBlock
{
public:

    Door10();
    static block_t getID();
};

class WoodenFence : public BaseBlock
{
public:

    WoodenFence();
    static block_t getID();
};

class CobbleStair1 : public BaseBlock
{
public:

    CobbleStair1();
    static block_t getID();
};

class CobbleStair2 : public BaseBlock
{
public:

    CobbleStair2();
    static block_t getID();
};

class CobbleStair3 : public BaseBlock
{
public:

    CobbleStair3();
    static block_t getID();
};

class CobbleStair4 : public BaseBlock
{
public:

    CobbleStair4();
    static block_t getID();
};

class Diamond : public BaseBlock
{
public:

    Diamond();
    static block_t getID();
};

class Gold : public BaseBlock
{
public:

    Gold();
    static block_t getID();
};

class Iron : public BaseBlock
{
public:

    Iron();
    static block_t getID();
};

class BrickStair1 : public BaseBlock
{
public:

    BrickStair1();
    static block_t getID();
};

class BrickStair2 : public BaseBlock
{
public:

    BrickStair2();
    static block_t getID();
};

class BrickStair3 : public BaseBlock
{
public:

    BrickStair3();
    static block_t getID();
};

class BrickStair4 : public BaseBlock
{
public:

    BrickStair4();
    static block_t getID();
};

class NoteBlock : public BaseBlock
{
public:

    NoteBlock();
    static block_t getID();
};

class Grass : public BaseBlock
{
public:

    Grass();
    static block_t getID();
};

class JukeBox : public BaseBlock
{
public:

    JukeBox();
    static block_t getID();
};

class Pumpkin1 : public BaseBlock
{
public:

    Pumpkin1();
    static block_t getID();
};

class Pumpkin2 : public BaseBlock
{
public:

    Pumpkin2();
    static block_t getID();
};

class Pumpkin3 : public BaseBlock
{
public:

    Pumpkin3();
    static block_t getID();
};

class Pumpkin4 : public BaseBlock
{
public:

    Pumpkin4();
    static block_t getID();
};

class Mooshroom1 : public BaseBlock
{
public:

    Mooshroom1();
    static block_t getID();
};

class Mooshroom2 : public BaseBlock
{
public:

    Mooshroom2();
    static block_t getID();
};

class Soil : public BaseBlock
{
public:

    Soil();
    static block_t getID();
};

class WateredSoil : public BaseBlock
{
public:

    WateredSoil();
    static block_t getID();
};

class MelonBlock : public BaseBlock
{
public:

    MelonBlock();
    static block_t getID();
};

class OakHalfBlock : public BaseBlock
{
public:

    OakHalfBlock();
    static block_t getID();
};

class CobbleHalfBlock : public BaseBlock
{
public:

    CobbleHalfBlock();
    static block_t getID();
};

class BrickHalfBlock : public BaseBlock
{
public:

    BrickHalfBlock();
    static block_t getID();
};

class Slab : public BaseBlock
{
public:

    Slab();
    static block_t getID();
};

class StoneSandHalfBlock : public BaseBlock
{
public:

    StoneSandHalfBlock();
    static block_t getID();
};

class Ladder1 : public BaseBlock
{
public:

    Ladder1();
    static block_t getID();
};

class Ladder2 : public BaseBlock
{
public:

    Ladder2();
    static block_t getID();
};

class Ladder3 : public BaseBlock
{
public:

    Ladder3();
    static block_t getID();
};

class Ladder4 : public BaseBlock
{
public:

    Ladder4();
    static block_t getID();
};

class DoubleSlab : public BaseBlock
{
public:

    DoubleSlab();
    static block_t getID();
};

class MossyCobblestone : public BaseBlock
{
public:

    MossyCobblestone();
    static block_t getID();
};

class Snow2 : public BaseBlock
{
public:

    Snow2();
    static block_t getID();
};

class ChestBlock1 : public BaseBlock
{
public:

    ChestBlock1();
    static block_t getID();
};

class ChestBlock2 : public BaseBlock
{
public:

    ChestBlock2();
    static block_t getID();
};

class ChestBlock3 : public BaseBlock
{
public:

    ChestBlock3();
    static block_t getID();
};

class ChestBlock4 : public BaseBlock
{
public:

    ChestBlock4();
    static block_t getID();
};

class ChestLock : public BaseBlock
{
public:

    ChestLock();
    static block_t getID();
};


class ClayBlock : public BaseBlock
{
public:

    ClayBlock();
    static block_t getID();
};

class Chest5 : public BaseBlock
{
public:

    Chest5();
    static block_t getID();
};

class LapisBlock : public BaseBlock
{
public:

    LapisBlock();
    static block_t getID();
};

class StoneBrick : public BaseBlock
{
public:

    StoneBrick();
    static block_t getID();
};

class CrackedStoneBrick : public BaseBlock
{
public:

    CrackedStoneBrick();
    static block_t getID();
};

class CarvedStoneBrick : public BaseBlock
{
public:

    CarvedStoneBrick();
    static block_t getID();
};

class CraftingTable : public BaseBlock
{
public:

    CraftingTable();
    static block_t getID();
};

class FurnaceOff : public BaseBlock
{
public:

    FurnaceOff();
    static block_t getID();
};

class FurnaceOn : public BaseBlock
{
public:

    FurnaceOn();
    static block_t getID();
};

class HalfStoneBrick : public BaseBlock
{
public:

    HalfStoneBrick();
    static block_t getID();
};

class Spawner : public BaseBlock
{
public:

    Spawner();
    static block_t getID();
};

class TNTBlock : public BaseBlock
{
public:

    TNTBlock();
    static block_t getID();
};

class Nuclear : public BaseBlock
{
public:

    Nuclear();
    static block_t getID();
};

class Lava : public BaseBlock
{
public:

    Lava();
    static block_t getID();
};

class Gravel : public BaseBlock
{
public:

    Gravel();
    static block_t getID();
};

class GlassPanel : public BaseBlock
{
public:

    GlassPanel();
    static block_t getID();
};

class IronBars : public BaseBlock
{
public:

    IronBars();
    static block_t getID();
};

class TallGrass : public BaseBlock
{
public:

    TallGrass();
    static block_t getID();
};

class Fern : public BaseBlock
{
public:

    Fern();
    static block_t getID();
};

class DrySapling : public BaseBlock
{
public:

    DrySapling();
    static block_t getID();
};

class Flower1 : public BaseBlock
{
public:

    Flower1();
    static block_t getID();
};

class Flower2 : public BaseBlock
{
public:

    Flower2();
    static block_t getID();
};

class OakSapling : public BaseBlock
{
public:

    OakSapling();
    static block_t getID();
};


class SpruceSapling : public BaseBlock
{
public:

    SpruceSapling();
    static block_t getID();
};

class BirchSapling : public BaseBlock
{
public:

    BirchSapling();
    static block_t getID();
};

class FreeBlock5 : public BaseBlock
{
public:

    FreeBlock5();
    static block_t getID();
};

class WoodenStair1 : public BaseBlock
{
public:

    WoodenStair1();
    static block_t getID();
};

class WoodenStair2 : public BaseBlock
{
public:

    WoodenStair2();
    static block_t getID();
};

class WoodenStair3 : public BaseBlock
{
public:

    WoodenStair3();
    static block_t getID();
};

class WoodenStair4 : public BaseBlock
{
public:

    WoodenStair4();
    static block_t getID();
};

class MelonPlant4Reverse : public BaseBlock
{
public:

    MelonPlant4Reverse();
    static block_t getID();
};

class MelonPlant1 : public BaseBlock
{
public:

    MelonPlant1();
    static block_t getID();
};

class MelonPlant2 : public BaseBlock
{
public:

    MelonPlant2();
    static block_t getID();
};

class MelonPlant3 : public BaseBlock
{
public:

    MelonPlant3();
    static block_t getID();
};

class MelonPlant4 : public BaseBlock
{
public:

    MelonPlant4();
    static block_t getID();
};

class WheatBlock1 : public BaseBlock
{
public:

    WheatBlock1();
    static block_t getID();
};

class WheatBlock2 : public BaseBlock
{
public:

    WheatBlock2();
    static block_t getID();
};

class WheatBlock3 : public BaseBlock
{
public:

    WheatBlock3();
    static block_t getID();
};

class WheatBlock4 : public BaseBlock
{
public:

    WheatBlock4();
    static block_t getID();
};

class WheatBlock5 : public BaseBlock
{
public:

	WheatBlock5();
	static block_t getID();
};

class WheatBlock6 : public BaseBlock
{
public:

	WheatBlock6();
	static block_t getID();
};

class WorldSky : public BaseBlock
{
public:

    WorldSky();
    static block_t getID();
};

class Cloud : public BaseBlock
{
public:

    Cloud();
    static block_t getID();
};

class bedrock : public BaseBlock
{
public:

	bedrock();
	static block_t getID();
};

class JackOLantern1 : public BaseBlock
{
public:

	JackOLantern1();
	static block_t getID();
};

class JackOLantern2 : public BaseBlock
{
public:

	JackOLantern2();
	static block_t getID();
};

class JackOLantern3 : public BaseBlock
{
public:

	JackOLantern3();
	static block_t getID();
};

class JackOLantern4 : public BaseBlock
{
public:

	JackOLantern4();
	static block_t getID();
};

class TrapdoorOpen1 : public BaseBlock
{
public:

	TrapdoorOpen1();
	static block_t getID();
};

class TrapdoorOpen2 : public BaseBlock
{
public:

	TrapdoorOpen2();
	static block_t getID();
};

class TrapdoorOpen3 : public BaseBlock
{
public:

	TrapdoorOpen3();
	static block_t getID();
};

class TrapdoorOpen4 : public BaseBlock
{
public:

	TrapdoorOpen4();
	static block_t getID();
};

class TrapdoorClosed1 : public BaseBlock
{
public:

	TrapdoorClosed1();
	static block_t getID();
};

class TrapdoorClosed2 : public BaseBlock
{
public:

	TrapdoorClosed2();
	static block_t getID();
};

class TrapdoorClosed3 : public BaseBlock
{
public:

	TrapdoorClosed3();
	static block_t getID();
};

class TrapdoorClosed4 : public BaseBlock
{
public:

	TrapdoorClosed4();
	static block_t getID();
};

class BirchPlanks : public BaseBlock
{
public:

	BirchPlanks();
	static block_t getID();
};

class SprucePlanks : public BaseBlock
{
public:

	SprucePlanks();
	static block_t getID();
};

class BirchHalfBlock : public BaseBlock
{
public:

	BirchHalfBlock();
	static block_t getID();
};

class SpruceHalfBlock : public BaseBlock
{
public:

	SpruceHalfBlock();
	static block_t getID();
};

class BirchFence : public BaseBlock
{
public:

	BirchFence();
	static block_t getID();
};

class SpruceFence : public BaseBlock
{
public:

	SpruceFence();
	static block_t getID();
};

class BirchStair1 : public BaseBlock
{
public:

    BirchStair1();
    static block_t getID();
};

class BirchStair2 : public BaseBlock
{
public:

    BirchStair2();
    static block_t getID();
};

class BirchStair3 : public BaseBlock
{
public:

    BirchStair3();
    static block_t getID();
};

class BirchStair4 : public BaseBlock
{
public:

    BirchStair4();
    static block_t getID();
};

class SpruceStair1 : public BaseBlock
{
public:

    SpruceStair1();
    static block_t getID();
};

class SpruceStair2 : public BaseBlock
{
public:

    SpruceStair2();
    static block_t getID();
};

class SpruceStair3 : public BaseBlock
{
public:

    SpruceStair3();
    static block_t getID();
};

class SpruceStair4 : public BaseBlock
{
public:

    SpruceStair4();
    static block_t getID();
};

class WaterBlock1 : public BaseBlock
{
public:

    WaterBlock1();
    static block_t getID();
};


class WaterBlock2 : public BaseBlock
{
public:

    WaterBlock2();
    static block_t getID();
};

class WaterBlock3 : public BaseBlock
{
public:

    WaterBlock3();
    static block_t getID();
};

class WaterBlock4 : public BaseBlock
{
public:

    WaterBlock4();
    static block_t getID();
};

// Flow levels 5..7 (Java water spreads 7 blocks). Extended ids, see AddBlockType.
class WaterBlock5 : public WaterBlock4
{
public:
    static block_t getID();
};

class WaterBlock6 : public WaterBlock4
{
public:
    static block_t getID();
};

class WaterBlock7 : public WaterBlock4
{
public:
    static block_t getID();
};

class ItemFrame1 : public BaseBlock
{
public:

    ItemFrame1();
    static block_t getID();
};

class ItemFrame2 : public BaseBlock
{
public:

    ItemFrame2();
    static block_t getID();
};

class ItemFrame3 : public BaseBlock
{
public:

    ItemFrame3();
    static block_t getID();
};

class ItemFrame4 : public BaseBlock
{
public:

    ItemFrame4();
    static block_t getID();
};

class PumpkinPlant4Reverse : public BaseBlock
{
public:

    PumpkinPlant4Reverse();
    static block_t getID();
};

class PumpkinPlant1 : public BaseBlock
{
public:

    PumpkinPlant1();
    static block_t getID();
};

class PumpkinPlant2 : public BaseBlock
{
public:

    PumpkinPlant2();
    static block_t getID();
};

class PumpkinPlant3 : public BaseBlock
{
public:

    PumpkinPlant3();
    static block_t getID();
};

class PumpkinPlant4 : public BaseBlock
{
public:

    PumpkinPlant4();
    static block_t getID();
};

class PotatoPlant1 : public BaseBlock
{
public:

    PotatoPlant1();
    static block_t getID();
};

class PotatoPlant2 : public BaseBlock
{
public:

    PotatoPlant2();
    static block_t getID();
};

class PotatoPlant3 : public BaseBlock
{
public:

    PotatoPlant3();
    static block_t getID();
};

class PotatoPlant4 : public BaseBlock
{
public:

    PotatoPlant4();
    static block_t getID();
};

class CarrotPlant1 : public BaseBlock
{
public:

    CarrotPlant1();
    static block_t getID();
};

class CarrotPlant2 : public BaseBlock
{
public:

    CarrotPlant2();
    static block_t getID();
};

class CarrotPlant3 : public BaseBlock
{
public:

    CarrotPlant3();
    static block_t getID();
};

class CarrotPlant4 : public BaseBlock
{
public:

    CarrotPlant4();
    static block_t getID();
};

class BedBlock1 : public BaseBlock
{
public:

    BedBlock1();
    static block_t getID();
};

class BedBlock2 : public BaseBlock
{
public:

    BedBlock2();
    static block_t getID();
};

class BedBlock3 : public BaseBlock
{
public:

    BedBlock3();
    static block_t getID();
};

class BedBlock4 : public BaseBlock
{
public:

    BedBlock4();
    static block_t getID();
};

class BedBlock5 : public BaseBlock
{
public:

    BedBlock5();
    static block_t getID();
};

class BedBlock6 : public BaseBlock
{
public:

    BedBlock6();
    static block_t getID();
};

class BedBlock7 : public BaseBlock
{
public:

    BedBlock7();
    static block_t getID();
};

class BedBlock8 : public BaseBlock
{
public:

    BedBlock8();
    static block_t getID();
};

class BlackWoolCarpet : public BaseBlock
{
public:

	BlackWoolCarpet();
	static block_t getID();
};

class GrayWoolCarpet : public BaseBlock
{
public:

	GrayWoolCarpet();
	static block_t getID();
};

class RedWoolCarpet : public BaseBlock
{
public:

	RedWoolCarpet();
	static block_t getID();
};

class PinkWoolCarpet : public BaseBlock
{
public:

	PinkWoolCarpet();
	static block_t getID();
};

class DarkGreenWoolCarpet : public BaseBlock
{
public:

	DarkGreenWoolCarpet();
	static block_t getID();
};

class GreenWoolCarpet : public BaseBlock
{
public:

	GreenWoolCarpet();
	static block_t getID();
};

class BrownWoolCarpet : public BaseBlock
{
public:

	BrownWoolCarpet();
	static block_t getID();
};

class YellowWoolCarpet : public BaseBlock
{
public:

	YellowWoolCarpet();
	static block_t getID();
};

class BlueWoolCarpet : public BaseBlock
{
public:

	BlueWoolCarpet();
	static block_t getID();
};

class LightBlueWoolCarpet : public BaseBlock
{
public:

	LightBlueWoolCarpet();
	static block_t getID();
};

class VioletWoolCarpet : public BaseBlock
{
public:

	VioletWoolCarpet();
	static block_t getID();
};

class PastelWoolCarpet : public BaseBlock
{
public:

	PastelWoolCarpet();
	static block_t getID();
};

class OrangeWoolCarpet : public BaseBlock
{
public:

	OrangeWoolCarpet();
	static block_t getID();
};

class LightGrayWoolCarpet : public BaseBlock
{
public:

	LightGrayWoolCarpet();
	static block_t getID();
};

class WhiteWoolCarpet : public BaseBlock
{
public:

	WhiteWoolCarpet();
	static block_t getID();
};

class HayBale : public BaseBlock
{
public:

	HayBale();
	static block_t getID();
};

class CoalBlock : public BaseBlock
{
public:

	CoalBlock();
	static block_t getID();
};

class RedstoneBlock : public BaseBlock
{
public:

	RedstoneBlock();
	static block_t getID();
};

class YellowGlassBlock : public BaseBlock
{
public:

	YellowGlassBlock();
	static block_t getID();
};

class GreenGlassBlock : public BaseBlock
{
public:

	GreenGlassBlock();
	static block_t getID();
};

class PinkGlassBlock : public BaseBlock
{
public:

	PinkGlassBlock();
	static block_t getID();
};

class PurpleGlassBlock : public BaseBlock
{
public:

	PurpleGlassBlock();
	static block_t getID();
};

class OrangeGlassBlock : public BaseBlock
{
public:

	OrangeGlassBlock();
	static block_t getID();
};

class RedGlassBlock : public BaseBlock
{
public:

	RedGlassBlock();
	static block_t getID();
};

/*class DGreenGlassBlock : public BaseBlock
{
public:

	DGreenGlassBlock();
	static block_t getID();
};*/

class BlackGlassBlock : public BaseBlock
{
public:

	BlackGlassBlock();
	static block_t getID();
};

/*class GrayGlassBlock : public BaseBlock
{
public:

	GrayGlassBlock();
	static block_t getID();
};*/

/*class LGrayGlassBlock : public BaseBlock
{
public:

	LGrayGlassBlock();
	static block_t getID();
};*/

/*class WhiteGlassBlock : public BaseBlock
{
public:

	WhiteGlassBlock();
	static block_t getID();
};

class BrownGlassBlock : public BaseBlock
{
public:

	BrownGlassBlock();
	static block_t getID();
};

class DBlueGlassBlock : public BaseBlock
{
public:

	DBlueGlassBlock();
	static block_t getID();
};

class LBlueGlassBlock : public BaseBlock
{
public:

	LBlueGlassBlock();
	static block_t getID();
};*/

class BlueGlassBlock : public BaseBlock
{
public:

	BlueGlassBlock();
	static block_t getID();
};

/*class DPurpleGlassBlock : public BaseBlock
{
public:

	DPurpleGlassBlock();
	static block_t getID();
};*/

class GlowstoneBlock : public BaseBlock
{
public:

	GlowstoneBlock();
	static block_t getID();
};

class Soulsand : public BaseBlock
{
public:

	Soulsand();
	static block_t getID();
};

class Netherrack : public BaseBlock
{
public:

	Netherrack();
	static block_t getID();
};
class MagmaBlock : public BaseBlock
{
public:

	MagmaBlock();
	static block_t getID();
};

class Redstoneoff : public BaseBlock
{
public:

	Redstoneoff();
	static block_t getID();
};

// Button (Java BlockButton): a small mechanism mounted on the side/floor of a block. Which face it is
// mounted on, and whether it is currently pressed, live in the 4-bit block-state nibble
// (OpRedstonePowerMask) that the lighting code preserves; the mesh and the power code read them back.
// A pressed button strongly powers the one block it is mounted on, then pops back out after 1 s
// (stone) or 1.5 s (wood).
class ButtonStone : public BaseBlock
{
public:

	ButtonStone();
	static block_t getID();
};

class ButtonWood : public BaseBlock
{
public:

	ButtonWood();
	static block_t getID();
};

// Lever (Java BlockLever). Shares the button's cell-state nibble -- bits 0-2 the mounting face, in the
// same order -- but bit 3 is a *latch* instead of a press: use flips it and it stays flipped, with no
// press timer and no entry in mPressedButtons. That shared encoding is the whole reason a lever gets
// wire, torch, TNT and lamp behaviour for free: every reader of "is a switch on and where does it point"
// asks the same nibble question a button answers.
class Lever : public BaseBlock
{
public:

	Lever();
	static block_t getID();
};

// Redstone lamp (Java BlockRedstoneLight). The lit state is a second block id for the same reason the
// furnace and the repeater use one: "is this cell a light source" stays an id lookup, and the mesher
// picks the lit tile with no per-state texture switch. Unlike every other component here the lamp
// reacts to power with no event of its own, so it is evaluated from the redstone event window -- see
// CraftWorld::EvaluateRedstoneLamp.
class RedstoneLampOff : public BaseBlock
{
public:

	RedstoneLampOff();
	static block_t getID();
};

class RedstoneLampOn : public BaseBlock
{
public:

	RedstoneLampOn();
	static block_t getID();
};

// Redstone repeater (Java BlockRedstoneRepeater). A flat 2 px slab that takes its input only from the
// block it faces away from and drives only the block it faces, always at full strength -- which is what
// lets a signal travel past the 15-cell limit of plain dust. Its facing and delay live in the 4-bit
// block-state nibble (the lighting code skips repeater cells, exactly as it does buttons).
// The powered state is a second block id rather than another state bit, which keeps "is this repeater
// driving me" an id comparison and matches the furnace off/on pair this codebase already uses.
class RepeaterOff : public BaseBlock
{
public:

	RepeaterOff();
	static block_t getID();
};

class RepeaterOn : public BaseBlock
{
public:

	RepeaterOn();
	static block_t getID();
};

/// Redstone torches (Java BlockRedstoneTorch, both the lit block 76 and the unlit block 75).
///
/// Like the plain torch family this codebase uses one block id per mounting face -- floor plus four walls
/// -- because a torch's face has to survive in the world and the block settings nibble is block light.
/// Unlike the plain torch there is also a lit/unlit pair, so lit and unlit ids are two contiguous ranges
/// five apart: toggling is always `id +/- 5` and never touches the nibble.
class RedstoneTorch : public BaseBlock          // lit, standing on the block below
{
public:
	RedstoneTorch();
	static block_t getID();
};
class RedstoneWallTorch1 : public BaseBlock     // lit, attached to the block at -X
{
public:
	RedstoneWallTorch1();
	static block_t getID();
};
class RedstoneWallTorch2 : public BaseBlock     // lit, attached to the block at +X
{
public:
	RedstoneWallTorch2();
	static block_t getID();
};
class RedstoneWallTorch3 : public BaseBlock     // lit, attached to the block at -Z
{
public:
	RedstoneWallTorch3();
	static block_t getID();
};
class RedstoneWallTorch4 : public BaseBlock     // lit, attached to the block at +Z
{
public:
	RedstoneWallTorch4();
	static block_t getID();
};
class UnlitRedstoneTorch : public BaseBlock     // switched off by its attachment being powered
{
public:
	UnlitRedstoneTorch();
	static block_t getID();
};
class UnlitRedstoneWallTorch1 : public BaseBlock
{
public:
	UnlitRedstoneWallTorch1();
	static block_t getID();
};
class UnlitRedstoneWallTorch2 : public BaseBlock
{
public:
	UnlitRedstoneWallTorch2();
	static block_t getID();
};
class UnlitRedstoneWallTorch3 : public BaseBlock
{
public:
	UnlitRedstoneWallTorch3();
	static block_t getID();
};
class UnlitRedstoneWallTorch4 : public BaseBlock
{
public:
	UnlitRedstoneWallTorch4();
	static block_t getID();
};

/// Piston (Java BlockPistonBase, plus BlockPistonExtension for the head).
///
/// Four base ids and two head ids, for the same reason the repeater and the lamp each have a pair: the
/// sticky/normal difference is a *tile* difference (piston_top vs piston_top_sticky) and the extended
/// flag has to be visible to the world without reading the state nibble, so between them they take two
/// id bits rather than a state bit. Sticky is the id, extended is the id, and the nibble is left holding
/// nothing but the direction it pushes.
///
/// That is the one place this differs from the repeater. A repeater is 4 px of art and its nibble has
/// room to spare, so its "on" state became an id for the *tile*. A piston's nibble has to carry six
/// directions, and it is the direction -- not the state -- that the mesher, the push code and the
/// placement code all need first.
///
/// State nibble (bits 0-2, the same 6-neighbour order the button and lever mountings use so the two
/// direction tables cannot drift apart): 0 = pushes down (-Y), 1 = up (+Y), 2 = -X, 3 = +X, 4 = -Z,
/// 5 = +Z. Bit 3 of an id that needs no more is free here because the extended flag is the id.
class PistonOff : public BaseBlock               // retracted, normal
{
public:
	PistonOff();
	static block_t getID();
};
class PistonOn : public BaseBlock                // extended: a head block sits in the cell in front
{
public:
	PistonOn();
	static block_t getID();
};
class StickyPistonOff : public BaseBlock         // retracted, sticky: also pulls one block back
{
public:
	StickyPistonOff();
	static block_t getID();
};
class StickyPistonOn : public BaseBlock
{
public:
	StickyPistonOn();
	static block_t getID();
};
/// The moving half. Java keeps this as one id (34) with a sticky bit in its metadata; this codebase
/// already spends metadata bits on nothing else for the extension, so the pair is spelled out as ids
/// the way the rest of the file does it.
class PistonHead : public BaseBlock
{
public:
	PistonHead();
	static block_t getID();
};
class StickyPistonHead : public BaseBlock
{
public:
	StickyPistonHead();
	static block_t getID();
};

class BlockofQuartz : public BaseBlock
{
public:

	BlockofQuartz();
	static block_t getID();
};

class ChiseledQuartzBlock : public BaseBlock
{
public:

	ChiseledQuartzBlock();
	static block_t getID();
};

class PillarQuartzBlock : public BaseBlock
{
public:

	PillarQuartzBlock();
	static block_t getID();
};

class white_concrete_powder : public BaseBlock
{
public:

	white_concrete_powder();
	static block_t getID();
};

/*class orange_concrete_powder : public BaseBlock
{
public:

	orange_concrete_powder();
	static block_t getID();
};

class magenta_concrete_powder : public BaseBlock
{
public:

	magenta_concrete_powder();
	static block_t getID();
};

light_blue_concrete_powder : public BaseBlock
{
public:

	light_blue_concrete_powder();
	static block_t getID();
};*/

class yellow_concrete_powder : public BaseBlock
{
public:

	yellow_concrete_powder();
	static block_t getID();
};

/*class lime_concrete_powder : public BaseBlock
{
public:

	lime_concrete_powder();
	static block_t getID();
};*/

class pink_concrete_powder : public BaseBlock
{
public:

	pink_concrete_powder();
	static block_t getID();
};

class gray_concrete_powder : public BaseBlock
{
public:

	gray_concrete_powder();
	static block_t getID();
};

/*class light_gray_concrete_powder : public BaseBlock
{
public:

	light_gray_concrete_powder();
	static block_t getID();
};*/

/*class cyan_concrete_powder : public BaseBlock
{
public:

	cyan_concrete_powder();
	static block_t getID();
};*/

class purple_concrete_powder : public BaseBlock
{
public:

	purple_concrete_powder();
	static block_t getID();
};

class blue_concrete_powder : public BaseBlock
{
public:

	blue_concrete_powder();
	static block_t getID();
};

/*class brown_concrete_powder : public BaseBlock
{
public:

	brown_concrete_powder();
	static block_t getID();
};*/

class green_concrete_powder : public BaseBlock
{
public:

	green_concrete_powder();
	static block_t getID();
};

class red_concrete_powder : public BaseBlock
{
public:

	red_concrete_powder();
	static block_t getID();
};

class black_concrete_powder : public BaseBlock
{
public:

	black_concrete_powder();
	static block_t getID();
};

class YellowGlassBlockPannel : public BaseBlock
{
public:

	YellowGlassBlockPannel();
	static block_t getID();
};

class GreenGlassBlockPannel : public BaseBlock
{
public:

	GreenGlassBlockPannel();
	static block_t getID();
};

class PinkGlassBlockPannel : public BaseBlock
{
public:

	PinkGlassBlockPannel();
	static block_t getID();
};

class PurpleGlassBlockPannel : public BaseBlock
{
public:

	PurpleGlassBlockPannel();
	static block_t getID();
};

class OrangeGlassBlockPannel : public BaseBlock
{
public:

	OrangeGlassBlockPannel();
	static block_t getID();
};

class RedGlassBlockPannel : public BaseBlock
{
public:

	RedGlassBlockPannel();
	static block_t getID();
};

/*class DGreenGlassBlockPannel : public BaseBlock
{
public:

	DGreenGlassBlockPannel();
	static block_t getID();
};*/

class BlackGlassBlockPannel : public BaseBlock
{
public:

	BlackGlassBlockPannel();
	static block_t getID();
};

/*class GrayGlassBlockPannel : public BaseBlock
{
public:

	GrayGlassBlockPannel();
	static block_t getID();
};

class LGrayGlassBlockPannel : public BaseBlock
{
public:

	LGrayGlassBlockPannel();
	static block_t getID();
};*/

/*class WhiteGlassBlockPannel : public BaseBlock
{
public:

	WhiteGlassBlockPannel();
	static block_t getID();
};

class BrownGlassBlockPannel : public BaseBlock
{
public:

	BrownGlassBlockPannel();
	static block_t getID();
};

class DBlueGlassBlockPannel : public BaseBlock
{
public:

	DBlueGlassBlockPannel();
	static block_t getID();
};

class LBlueGlassBlockPannel : public BaseBlock
{
public:

	LBlueGlassBlockPannel();
	static block_t getID();
};*/

class BlueGlassBlockPannel : public BaseBlock
{
public:

	BlueGlassBlockPannel();
	static block_t getID();
};

/*class DPurpleGlassBlockPannel : public BaseBlock
{
public:

	DPurpleGlassBlockPannel();
	static block_t getID();
};*/

/*class redstone_lamp : public BaseBlock
{
public:

	redstone_lamp();
	static block_t getID();
};

class lit_redstone_lamp : public BaseBlock
{
public:

	lit_redstone_lamp();
	static block_t getID();
};*/

class Fire : public BaseBlock
{
public:

	Fire();

	static block_t getID();
};

// The nether portal. Not solid, not editable, not breakable: it exists only as the lit interior of an
// obsidian frame, and it is what the player stands in to travel.
//
// The lit interior is a flat plane in the middle of each cell, not a cube, so the surface reads as one
// continuous sheet rather than a grid of separately lit block faces. There are two ids because the
// plane has to know which way the frame runs and the mesher is handed nothing but an id:
//   PortalBlock   -- the frame runs along X, so the sheet spans X and Y and is centred in Z
//   PortalBlockZ  -- the frame runs along Z, so the sheet spans Z and Y and is centred in X
// Both draw with blockModel 30 (CraftWorld_MeshBlocks.cpp) and share the animated atlas tile (15,8),
// which UpdateWorldAnimation repaints every tick.
class PortalBlock : public BaseBlock
{
public:

	PortalBlock();

	static block_t getID();
};

class PortalBlockZ : public BaseBlock
{
public:

	PortalBlockZ();

	static block_t getID();
};

// There are two portal ids now, so every "is this the portal?" test has to ask about both. One place
// to get that right instead of five.
inline bool IsPortalBlock(block_t b)
{
	return b == PortalBlock::getID() || b == PortalBlockZ::getID();
}

class GravitySandBlock : public BaseBlock
{
public:

	GravitySandBlock();

	static block_t getID();
};

class LavaBlock1 : public BaseBlock
{
public:

	LavaBlock1();
	static block_t getID();
};

#endif