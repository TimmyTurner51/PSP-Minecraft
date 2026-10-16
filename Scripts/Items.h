#ifndef ITEMS_H
#define ITEMS_H

#include "BaseItem.h"

class WoodenPickaxe : public BaseItem
{
public:

	WoodenPickaxe();

	static item_t getID();
};

class StonePickaxe : public BaseItem
{
public:

	StonePickaxe();

	static item_t getID();
};

class IronPickaxe : public BaseItem
{
public:

	IronPickaxe();

	static item_t getID();
};

class DiamondPickaxe : public BaseItem
{
public:

	DiamondPickaxe();

	static item_t getID();
};

class GoldenPickaxe : public BaseItem
{
public:

	GoldenPickaxe();

	static item_t getID();
};

class WoodenSword : public BaseItem
{
public:

	WoodenSword();

	static item_t getID();
};

class StoneSword : public BaseItem
{
public:

	StoneSword();

	static item_t getID();
};

class IronSword : public BaseItem
{
public:

	IronSword();

	static item_t getID();
};

class DiamondSword : public BaseItem
{
public:

	DiamondSword();

	static item_t getID();
};

class GoldenSword : public BaseItem
{
public:

	GoldenSword();

	static item_t getID();
};

class WoodenShovel : public BaseItem
{
public:

	WoodenShovel();

	static item_t getID();
};

class StoneShovel : public BaseItem
{
public:

	StoneShovel();

	static item_t getID();
};

class IronShovel : public BaseItem
{
public:

	IronShovel();

	static item_t getID();
};

class DiamondShovel : public BaseItem
{
public:

	DiamondShovel();

	static item_t getID();
};

class GoldenShovel : public BaseItem
{
public:

	GoldenShovel();

	static item_t getID();
};

class WoodenAxe : public BaseItem
{
public:

	WoodenAxe();

	static item_t getID();
};

class StoneAxe : public BaseItem
{
public:

	StoneAxe();

	static item_t getID();
};

class IronAxe : public BaseItem
{
public:

	IronAxe();

	static item_t getID();
};

class DiamondAxe : public BaseItem
{
public:

	DiamondAxe();

	static item_t getID();
};

class GoldenAxe : public BaseItem
{
public:

	GoldenAxe();

	static item_t getID();
};

class WoodenHoe : public BaseItem
{
public:

	WoodenHoe();

	static item_t getID();
};

class StoneHoe : public BaseItem
{
public:

	StoneHoe();

	static item_t getID();
};

class IronHoe : public BaseItem
{
public:

	IronHoe();

	static item_t getID();
};

class DiamondHoe : public BaseItem
{
public:

	DiamondHoe();

	static item_t getID();
};

class GoldenHoe : public BaseItem
{
public:

	GoldenHoe();

	static item_t getID();
};

class Scissors : public BaseItem
{
public:

	Scissors();

	static item_t getID();
};

class Stick : public BaseItem
{
public:

	Stick();

	static item_t getID();
};

// The debug stick. It is deliberately the stick's sprite and shape -- there is no new artwork, only a
// new name and a new id -- because its whole purpose is to be a testing handle, not an item a player
// is meant to find. The id is appended at 421 rather than slotted next to Stick's 276 because
// itemTypes is indexed by (id - 250), so any insertion but the end renumbers every later item and
// changes ids that existing saves already store.
class DebugStick : public BaseItem
{
public:

	DebugStick();

	static item_t getID();
};

class Coal : public BaseItem
{
public:

	Coal();

	static item_t getID();
};

class IronBar : public BaseItem
{
public:

	IronBar();

	static item_t getID();
};

class DiamondItem : public BaseItem
{
public:

	DiamondItem();

	static item_t getID();
};

class GoldenBar : public BaseItem
{
public:

	GoldenBar();

	static item_t getID();
};

class FireItem : public BaseItem
{
public:

	FireItem();

	static item_t getID();
};

class LigniteCoal : public BaseItem
{
public:

	LigniteCoal();

	static item_t getID();
};

class Clay : public BaseItem
{
public:

	Clay();

	static item_t getID();
};

class Apple : public BaseItem
{
public:

	Apple();

	static item_t getID();
};

class GoldenApple : public BaseItem
{
public:

	GoldenApple();

	static item_t getID();
};

class WheatSeeds : public BaseItem
{
public:

	WheatSeeds();

	static item_t getID();
};

class Wheat : public BaseItem
{
public:

	Wheat();

	static item_t getID();
};

class WhiteBread : public BaseItem
{
public:

	WhiteBread();

	static item_t getID();
};

class Brick : public BaseItem
{
public:

	Brick();

	static item_t getID();
};

class Busket : public BaseItem
{
public:

	Busket();

	static item_t getID();
};

class WaterBusket : public BaseItem
{
public:

	WaterBusket();

	static item_t getID();
};

class LavaBusket : public BaseItem
{
public:

	LavaBusket();

	static item_t getID();
};

class CaneItem : public BaseItem
{
public:

	CaneItem();

	static item_t getID();
};

class Flower1Item : public BaseItem
{
public:

	Flower1Item();

	static item_t getID();
};

class Flower2Item : public BaseItem
{
public:

	Flower2Item();

	static item_t getID();
};

class OakSaplingItem : public BaseItem
{
public:

	OakSaplingItem();

	static item_t getID();
};

class Paper : public BaseItem
{
public:

	Paper();

	static item_t getID();
};

class Book : public BaseItem
{
public:

	Book();

	static item_t getID();
};

class SnowBallItem : public BaseItem
{
public:

	SnowBallItem();

	static item_t getID();
};

class MooshroomItem1 : public BaseItem
{
public:

	MooshroomItem1();

	static item_t getID();
};

class MooshroomItem2 : public BaseItem
{
public:

	MooshroomItem2();

	static item_t getID();
};

class Bowl : public BaseItem
{
public:

	Bowl();

	static item_t getID();
};

class MooshroomBowl : public BaseItem
{
public:

	MooshroomBowl();

	static item_t getID();
};

class IronWateringCan : public BaseItem
{
public:

	IronWateringCan();

	static item_t getID();
};

class DiamondWateringCan : public BaseItem
{
public:

	DiamondWateringCan();

	static item_t getID();
};

class DoorItem : public BaseItem
{
public:

	DoorItem();

	static item_t getID();
};

class LadderItem : public BaseItem
{
public:

	LadderItem();

	static item_t getID();
};

class TorchItem : public BaseItem
{
public:

	TorchItem();

	static item_t getID();
};

/// Redstone torches ride the same terrain-atlas sprite path the plain torch does, so the item icon is
/// just the lit tile at (3,6): plane 3, textureRow 15 - 6 (rows count up from the bottom of the atlas).
class RedstoneTorchItem : public BaseItem
{
public:

	RedstoneTorchItem();

	static item_t getID();
};

/// The repeater's inventory icon is its own sprite, not a 3D render of the block it places. Java draws
/// the item form as the flat top tile, so the icon is terrain (3,8) -- the dark repeater -- at plane 3,
/// textureRow 15 - 8, rows counting up from the bottom of the atlas like every other terrain-backed
/// item. This is also why the repeater needs an item at all: the inventory sends block ids through
/// `drawHudCubes`, which renders the block's model under a tilted camera, and only an item id reaches
/// `drawHudItems`, the flat-sprite path the redstone torch already uses.
class RepeaterItem : public BaseItem
{
public:

	RepeaterItem();

	static item_t getID();
};

class SpruceSaplingItem : public BaseItem
{
public:

	SpruceSaplingItem();

	static item_t getID();
};

class BirchSaplingItem : public BaseItem
{
public:

	BirchSaplingItem();

	static item_t getID();
};

class MelonItem : public BaseItem
{
public:

	MelonItem();

	static item_t getID();
};

class MelonSeeds : public BaseItem
{
public:

	MelonSeeds();

	static item_t getID();
};

class Sugar : public BaseItem
{
public:

	Sugar();

	static item_t getID();
};

class CocoaBeans : public BaseItem
{
public:

	CocoaBeans();

	static item_t getID();
};

class Cookie : public BaseItem
{
public:

	Cookie();

	static item_t getID();
};

class RawBeef : public BaseItem
{
public:

	RawBeef();

	static item_t getID();
};

class Steak : public BaseItem
{
public:

	Steak();

	static item_t getID();
};

class RawChicken : public BaseItem
{
public:

	RawChicken();

	static item_t getID();
};

class CookedChicken : public BaseItem
{
public:

	CookedChicken();

	static item_t getID();
};

class Feather : public BaseItem
{
public:

	Feather();

	static item_t getID();
};

class Egg : public BaseItem
{
public:

	Egg();

	static item_t getID();
};

// Spider drops (Java 1.7.3 EntitySpider.dropFewItems: 0-2 string, and 0-1 spider eye).
class String : public BaseItem
{
public:

	String();

	static item_t getID();
};

class SpiderEye : public BaseItem
{
public:

	SpiderEye();

	static item_t getID();
};


// Arrow is a material like string or a feather -- it stacks -- and it is what a bow fires and what
// a skeleton drops. Bow is a tool: it does not stack and it wears out, so it carries durability.
class Arrow : public BaseItem
{
public:

	Arrow();

	static item_t getID();
};


class Bow : public BaseItem
{
public:

	Bow();

	static item_t getID();
};

// The bow's three draw frames. An item's atlas cell is baked into its mesh when the world builds its
// meshes (CraftWorld::GetItemVerts), and there is no way to ask for a different cell at draw time, so
// each frame the bow can be shown with has to exist as an item type of its own. They are appended
// after the debug stick as ids 422..424 and nothing can obtain them: they are in no tab, no recipe and
// no drop table, and only the drawing helpers below ever name them.
//
// Appended rather than slotted in beside the bow for the same reason as every other late arrival here:
// itemTypes is indexed by (id - 250), so inserting anywhere but the end renumbers every later item.
class BowPulling0 : public BaseItem
{
public:

	BowPulling0();

	static item_t getID();
};

class BowPulling1 : public BaseItem
{
public:

	BowPulling1();

	static item_t getID();
};

class BowPulling2 : public BaseItem
{
public:

	BowPulling2();

	static item_t getID();
};

// The mesh to draw a bow with at a given draw stage: 0 is at rest (bow_standby), 1, 2 and 3 are the
// string pulled further and further back, which is vanilla's bow_pulling_0/1/2. Anything outside 0..3
// is treated as 0, so a caller can hand this an unclamped value.
int BowHeldMeshId(int stage);

// A draw amount from 0 to 1 as one of those stages. Vanilla splits its 20-tick draw into three roughly
// equal parts when it picks which icon to show, so thirds are the faithful split; a charge of exactly
// 1 gives stage 3 rather than 4.
	int BowDrawStage(float charge);

// True for the bow itself and for any of its draw frames. While the string is pulled the bow is drawn
// as a frame, so "is this a bow?" has to answer yes for them as well -- which is what the held-item
// scale below is keyed on.
bool IsBowItem(int itemId);

// The scale a bow is drawn at when held. The generic held item is 0.42, which is sized for a 16x16
// block face; a bow at that size reads as a toy next to the mob holding it. Every site that draws one
// -- the skeleton, the player's first person and the player's third person -- reads this, so the three
// cannot drift apart, and it is the one number to turn to make a held bow bigger or smaller.
extern const float kHeldBowScale;

// How far a held bow is tipped out of the vertical, in radians, counted towards whatever the holder
// is facing: one number per hand, because the three hands are three different chains of rotations.
// Each is derived, not tuned -- the artwork's own angle and one equation per chain, all in Items.cpp
// and all reproducible with `python _tmp_atlas/solve_bow_parallel.py`, which measures the art off the
// atlas and prints the angle each hand is off by. Applied by the hand that names it.
extern const float kHeldBowAimTilt;             // third person (and the carried bow)
extern const float kHeldBowFirstPersonTilt;     // the player's own hand, in view space
extern const float kHeldBowSkeletonTilt;        // the skeleton's bow arm

class LapisLazuli : public BaseItem
{
public:

	LapisLazuli();

	static item_t getID();
};

class MilkBusket : public BaseItem
{
public:

	MilkBusket();

	static item_t getID();
};

class CakeItem : public BaseItem
{
public:

	CakeItem();

	static item_t getID();
};

class Bone : public BaseItem
{
public:

	Bone();

	static item_t getID();
};

class BoneMeal : public BaseItem
{
public:

	BoneMeal();

	static item_t getID();
};

class Leather : public BaseItem
{
public:

	Leather();

	static item_t getID();
};

class SampleHelmet : public BaseItem
{
public:

	SampleHelmet();

	static item_t getID();
};

class SampleChestplate : public BaseItem
{
public:

	SampleChestplate();

	static item_t getID();
};

class SampleLeggings : public BaseItem
{
public:

	SampleLeggings();

	static item_t getID();
};

class SampleBoots : public BaseItem
{
public:

	SampleBoots();

	static item_t getID();
};

class LeatherHelmet : public BaseItem
{
public:

	LeatherHelmet();

	static item_t getID();
};

class LeatherChestplate : public BaseItem
{
public:

	LeatherChestplate();

	static item_t getID();
};

class LeatherLeggings : public BaseItem
{
public:

	LeatherLeggings();

	static item_t getID();
};

class LeatherBoots : public BaseItem
{
public:

	LeatherBoots();

	static item_t getID();
};

class ChainmailHelmet : public BaseItem
{
public:

	ChainmailHelmet();

	static item_t getID();
};

class ChainmailChestplate : public BaseItem
{
public:

	ChainmailChestplate();

	static item_t getID();
};

class ChainmailLeggings : public BaseItem
{
public:

	ChainmailLeggings();

	static item_t getID();
};

class ChainmailBoots : public BaseItem
{
public:

	ChainmailBoots();

	static item_t getID();
};

class IronHelmet : public BaseItem
{
public:

	IronHelmet();

	static item_t getID();
};

class IronChestplate : public BaseItem
{
public:

	IronChestplate();

	static item_t getID();
};

class IronLeggings : public BaseItem
{
public:

	IronLeggings();

	static item_t getID();
};

class IronBoots : public BaseItem
{
public:

	IronBoots();

	static item_t getID();
};

class DiamondHelmet : public BaseItem
{
public:

	DiamondHelmet();

	static item_t getID();
};

class DiamondChestplate : public BaseItem
{
public:

	DiamondChestplate();

	static item_t getID();
};

class DiamondLeggings : public BaseItem
{
public:

	DiamondLeggings();

	static item_t getID();
};

class DiamondBoots : public BaseItem
{
public:

	DiamondBoots();

	static item_t getID();
};

class GoldenHelmet : public BaseItem
{
public:

	GoldenHelmet();

	static item_t getID();
};

class GoldenChestplate : public BaseItem
{
public:

	GoldenChestplate();

	static item_t getID();
};

class GoldenLeggings : public BaseItem
{
public:

	GoldenLeggings();

	static item_t getID();
};

class GoldenBoots : public BaseItem
{
public:

	GoldenBoots();

	static item_t getID();
};

class RottenFlesh : public BaseItem
{
public:

	RottenFlesh();

	static item_t getID();
};

class GlassPanelItem : public BaseItem
{
public:

	GlassPanelItem();

	static item_t getID();
};

class IronBarsItem : public BaseItem
{
public:

	IronBarsItem();

	static item_t getID();
};

class Flint : public BaseItem
{
public:

	Flint();

	static item_t getID();
};

class Gunpowder : public BaseItem
{
public:

	Gunpowder();

	static item_t getID();
};

class Redstone : public BaseItem
{
public:

	Redstone();

	static item_t getID();
};

class Clock : public BaseItem
{
public:

	Clock();

	static item_t getID();
};

class Compass : public BaseItem
{
public:

	Compass();

	static item_t getID();
};

class Saddle : public BaseItem
{
public:

	Saddle();

	static item_t getID();
};

class Sign : public BaseItem
{
public:

	Sign();

	static item_t getID();
};

class ItemFrameItem : public BaseItem
{
public:

	ItemFrameItem ();

	static item_t getID();
};

class FlowerPotItem : public BaseItem
{
public:

	FlowerPotItem ();

	static item_t getID();
};

class PaintningItem : public BaseItem
{
public:

	PaintningItem ();

	static item_t getID();
};

class BedItem : public BaseItem
{
public:

	BedItem ();

	static item_t getID();
};

class Disk1 : public BaseItem
{
public:

	Disk1 ();

	static item_t getID();
};

class Disk2 : public BaseItem
{
public:

	Disk2 ();

	static item_t getID();
};

class Disk3 : public BaseItem
{
public:

	Disk3 ();

	static item_t getID();
};

class LightGrayDye : public BaseItem
{
public:

	LightGrayDye ();

	static item_t getID();
};

class GrayDye : public BaseItem
{
public:

	GrayDye ();

	static item_t getID();
};

class InkSac : public BaseItem
{
public:

	InkSac ();

	static item_t getID();
};

class PinkDye : public BaseItem
{
public:

	PinkDye ();

	static item_t getID();
};

class RoseRed : public BaseItem
{
public:

	RoseRed ();

	static item_t getID();
};

class OrangeDye : public BaseItem
{
public:

	OrangeDye ();

	static item_t getID();
};

class DandelionYellow : public BaseItem
{
public:

	DandelionYellow ();

	static item_t getID();
};

class LimeDye : public BaseItem
{
public:

	LimeDye ();

	static item_t getID();
};

class CactusGreen : public BaseItem
{
public:

	CactusGreen ();

	static item_t getID();
};

class LightBlueDye : public BaseItem
{
public:

	LightBlueDye ();

	static item_t getID();
};

class PurpleDye : public BaseItem
{
public:

	PurpleDye ();

	static item_t getID();
};

class MagnetaDye : public BaseItem
{
public:

	MagnetaDye ();

	static item_t getID();
};

class PumpkinSeeds : public BaseItem
{
public:

	PumpkinSeeds ();

	static item_t getID();
};

class FlintAndSteel : public BaseItem
{
public:

	FlintAndSteel ();

	static item_t getID();
};

class Potato : public BaseItem
{
public:

	Potato ();

	static item_t getID();
};

class BakedPotato : public BaseItem
{
public:

	BakedPotato ();

	static item_t getID();
};

class PoisonedPotato : public BaseItem
{
public:

	PoisonedPotato ();

	static item_t getID();
};

class Carrot : public BaseItem
{
public:

	Carrot ();

	static item_t getID();
};

class Disk4 : public BaseItem
{
public:

	Disk4 ();

	static item_t getID();
};

class Disk5 : public BaseItem
{
public:

	Disk5 ();

	static item_t getID();
};

class Disk6 : public BaseItem
{
public:

	Disk6 ();

	static item_t getID();
};

class Disk7 : public BaseItem
{
public:

	Disk7 ();

	static item_t getID();
};

class Disk8 : public BaseItem
{
public:

	Disk8 ();

	static item_t getID();
};

class Disk9 : public BaseItem
{
public:

	Disk9 ();

	static item_t getID();
};

class Disk10 : public BaseItem
{
public:

	Disk10 ();

	static item_t getID();
};

class Disk11 : public BaseItem
{
public:

	Disk11 ();

	static item_t getID();
};

class Disk12 : public BaseItem
{
public:

	Disk12 ();

	static item_t getID();
};

class RawMutton : public BaseItem
{
public:

	RawMutton ();

	static item_t getID();
};

class CookedMutton : public BaseItem
{
public:

	CookedMutton ();

	static item_t getID();
};

class YellowGlassBlockPannelItem : public BaseItem
{
public:

	YellowGlassBlockPannelItem();

	static item_t getID();
};

class GreenGlassBlockPannelItem : public BaseItem
{
public:

	GreenGlassBlockPannelItem();
	static item_t getID();
};

class PinkGlassBlockPannelItem : public BaseItem
{
public:

	PinkGlassBlockPannelItem();
	static item_t getID();
};

class PurpleGlassBlockPannelItem : public BaseItem
{
public:

	PurpleGlassBlockPannelItem();
	static item_t getID();
};

/*class OrangeGlassBlockPannelItem : public BaseItem
{
public:

	OrangeGlassBlockPannelItem();
	static item_t getID();
};*/

class RedGlassBlockPannelItem : public BaseItem
{
public:

	RedGlassBlockPannelItem();
	static item_t getID();
};

/*class DGreenGlassBlockPannelItem : public BaseItem
{
public:

	DGreenGlassBlockPannelItem();
	static item_t getID();
};*/

class BlackGlassBlockPannelItem : public BaseItem
{
public:

	BlackGlassBlockPannelItem();
	static item_t getID();
};

/*class GrayGlassBlockPannelItem : public BaseItem
{
public:

	GrayGlassBlockPannelItem();
	static item_t getID();
};

class LGrayGlassBlockPannelItem : public BaseItem
{
public:

	LGrayGlassBlockPannelItem();
	static item_t getID();
};

class WhiteGlassBlockPannelItem : public BaseItem
{
public:

	WhiteGlassBlockPannelItem();
	static item_t getID();
};

class BrownGlassBlockPannelItem : public BaseItem
{
public:

	BrownGlassBlockPannelItem();
	static item_t getID();
};

class DBlueGlassBlockPannelItem : public BaseItem
{
public:

	DBlueGlassBlockPannelItem();
	static item_t getID();
};

class LBlueGlassBlockPannelItem : public BaseItem
{
public:

	LBlueGlassBlockPannelItem();
	static item_t getID();
};*/

class BlueGlassBlockPannelItem : public BaseItem
{
public:

	BlueGlassBlockPannelItem();
	static item_t getID();
};

/*class DPurpleGlassBlockPannelItem : public BaseItem
{
public:

	DPurpleGlassBlockPannelItem();
	static item_t getID();
};*/

class RawPorkchop : public BaseItem
{
public:

	RawPorkchop();
	static item_t getID();
};

class CookedPorkchop : public BaseItem
{
public:

	CookedPorkchop();
	static item_t getID();
};

class CowSpawnEgg : public BaseItem
{
public:

	CowSpawnEgg();
	static item_t getID();
};

class SheepSpawnEgg : public BaseItem
{
public:

	SheepSpawnEgg();
	static item_t getID();
};

class ZombieSpawnEgg : public BaseItem
{
public:

	ZombieSpawnEgg();
	static item_t getID();
};

class CreeperSpawnEgg : public BaseItem
{
public:

	CreeperSpawnEgg();
	static item_t getID();
};

class PigSpawnEgg : public BaseItem
{
public:

	PigSpawnEgg();
	static item_t getID();
};

class ChickenSpawnEgg : public BaseItem
{
public:

	ChickenSpawnEgg();
	static item_t getID();
};

class SpiderSpawnEgg : public BaseItem
{
public:

	SpiderSpawnEgg();
	static item_t getID();
};

class SkeletonSpawnEgg : public BaseItem
{
public:

	SkeletonSpawnEgg();
	static item_t getID();
};

#endif