#include "Items.h"

#define PI 3.1415926535897f

//items id begin from 250

WoodenPickaxe::WoodenPickaxe():BaseItem()
{
	textureRow = 9;

	plane = 0;
	durabilityPoints = 60;
	stackable = false;

	name = "Wooden Pickaxe";
}

item_t WoodenPickaxe::getID(){ return 250;}

StonePickaxe::StonePickaxe():BaseItem()
{
	textureRow = 9;

	plane = 1;
	durabilityPoints = 132;
	stackable = false;

	name = "Stone Pickaxe";
}

item_t StonePickaxe::getID(){ return 251;}

IronPickaxe::IronPickaxe():BaseItem()
{
	textureRow = 9;

	plane = 2;
	durabilityPoints = 251;
	stackable = false;

	name = "Iron Pickaxe";
}

item_t IronPickaxe::getID(){ return 252;}

DiamondPickaxe::DiamondPickaxe():BaseItem()
{
	textureRow = 9;

	plane = 3;
	durabilityPoints = 1562;
	stackable = false;

	name = "Diamond Pickaxe";
}

item_t DiamondPickaxe::getID(){ return 253;}

GoldenPickaxe::GoldenPickaxe():BaseItem()
{
	textureRow = 9;

	plane = 4;
	durabilityPoints = 33;
	stackable = false;

	name = "Golden Pickaxe";
}

item_t GoldenPickaxe::getID(){ return 254;}

WoodenSword::WoodenSword():BaseItem()
{
	textureRow = 11;

	plane = 0;
	durabilityPoints = 60;
	stackable = false;

	name = "Wooden Sword";
}

item_t WoodenSword::getID(){ return 255;}

StoneSword::StoneSword():BaseItem()
{
	textureRow = 11;

	plane = 1;
	durabilityPoints = 132;
	stackable = false;

	name = "Stone Sword";
}

item_t StoneSword::getID(){ return 256;}

IronSword::IronSword():BaseItem()
{
	textureRow = 11;

	plane = 2;
	durabilityPoints = 251;
	stackable = false;

	name = "Iron Sword";
}

item_t IronSword::getID(){ return 257;}

DiamondSword::DiamondSword():BaseItem()
{
	textureRow = 11;

	plane = 3;
	durabilityPoints = 1562;
	stackable = false;

	name = "Diamond Sword";
}

item_t DiamondSword::getID(){ return 258;}

GoldenSword::GoldenSword():BaseItem()
{
	textureRow = 11;

	plane = 4;
	durabilityPoints = 33;
	stackable = false;

	name = "Golden Sword";
}

item_t GoldenSword::getID(){ return 259;}

WoodenShovel::WoodenShovel():BaseItem()
{
	textureRow = 10;

	plane = 0;
	durabilityPoints = 60;
	stackable = false;

	name = "Wooden Shovel";
}

item_t WoodenShovel::getID(){ return 260;}

StoneShovel::StoneShovel():BaseItem()
{
	textureRow = 10;

	plane = 1;
	durabilityPoints = 132;
	stackable = false;

	name = "Stone Shovel";
}

item_t StoneShovel::getID(){ return 261;}

IronShovel::IronShovel():BaseItem()
{
	textureRow = 10;

	plane = 2;
	durabilityPoints = 251;
	stackable = false;

	name = "Iron Shovel";
}

item_t IronShovel::getID(){ return 262;}

DiamondShovel::DiamondShovel():BaseItem()
{
	textureRow = 10;

	plane = 3;
	durabilityPoints = 1562;
	stackable = false;

	name = "Diamond Shovel";
}

item_t DiamondShovel::getID(){ return 263;}

GoldenShovel::GoldenShovel():BaseItem()
{
	textureRow = 10;

	plane = 4;
	durabilityPoints = 33;
	stackable = false;

	name = "Golden Shovel";
}

item_t GoldenShovel::getID(){ return 264;}

WoodenAxe::WoodenAxe():BaseItem()
{
	textureRow = 8;

	plane = 0;
	durabilityPoints = 60;
	stackable = false;

	name = "Wooden Axe";
}

item_t WoodenAxe::getID(){ return 265;}

StoneAxe::StoneAxe():BaseItem()
{
	textureRow = 8;

	plane = 1;
	durabilityPoints = 132;
	stackable = false;

	name = "Stone Axe";
}

item_t StoneAxe::getID(){ return 266;}

IronAxe::IronAxe():BaseItem()
{
	textureRow = 8;

	plane = 2;
	durabilityPoints = 251;
	stackable = false;

	name = "Iron Axe";
}

item_t IronAxe::getID(){ return 267;}

DiamondAxe::DiamondAxe():BaseItem()
{
	textureRow = 8;

	plane = 3;
	durabilityPoints = 1562;
	stackable = false;

	name = "Diamond Axe";
}

item_t DiamondAxe::getID(){ return 268;}

GoldenAxe::GoldenAxe():BaseItem()
{
	textureRow = 8;

	plane = 4;
	durabilityPoints = 33;
	stackable = false;

	name = "Golden Axe";
}

item_t GoldenAxe::getID(){ return 269;}

WoodenHoe::WoodenHoe():BaseItem()
{
	textureRow = 7;

	plane = 0;
	durabilityPoints = 60;
	stackable = false;

	name = "Wooden Hoe";
}

item_t WoodenHoe::getID(){ return 270;}

StoneHoe::StoneHoe():BaseItem()
{
	textureRow = 7;

	plane = 1;
	durabilityPoints = 132;
	stackable = false;

	name = "Stone Hoe";
}

item_t StoneHoe::getID(){ return 271;}

IronHoe::IronHoe():BaseItem()
{
	textureRow = 7;

	plane = 2;
	durabilityPoints = 251;
	stackable = false;

	name = "Iron Hoe";
}

item_t IronHoe::getID(){ return 272;}

DiamondHoe::DiamondHoe():BaseItem()
{
	textureRow = 7;

	plane = 3;
	durabilityPoints = 1562;
	stackable = false;

	name = "Diamond Hoe";
}

item_t DiamondHoe::getID(){ return 273;}

GoldenHoe::GoldenHoe():BaseItem()
{
	textureRow = 7;

	plane = 4;
	durabilityPoints = 33;
	stackable = false;

	name = "Golden Hoe";
}

item_t GoldenHoe::getID(){ return 274;}

Scissors::Scissors():BaseItem()
{
	textureRow = 10;

	plane = 13;
	durabilityPoints = 239;
	stackable = false;

	name = "Shears";
}

item_t Scissors::getID(){ return 275;}

Stick::Stick():BaseItem()
{
	textureRow = 12;

	plane = 5;

	name = "Stick";
}

item_t Stick::getID(){ return 276;}

DebugStick::DebugStick():BaseItem()
{
	// The stick's cell, in every texture pack. Its appearance is intentionally unchanged; only the
	// name differs, which is what the hover label and the inventory read out.
	textureRow = 12;

	plane = 5;

	name = "Debug Stick";
}

item_t DebugStick::getID(){ return 421;}

Coal::Coal():BaseItem()
{
	textureRow = 15;

	plane = 7;

	name = "Coal";
}

item_t Coal::getID(){ return 277;}

IronBar::IronBar():BaseItem()
{
	textureRow = 14;

	plane = 7;

	name = "Iron Ingot";
}

item_t IronBar::getID(){ return 278;}

DiamondItem::DiamondItem():BaseItem()
{
	textureRow = 12;

	plane = 7;

	name = "Diamond";
}

item_t DiamondItem::getID(){ return 279;}

GoldenBar::GoldenBar():BaseItem()
{
	textureRow = 13;

	plane = 7;

	name = "Gold Ingot";
}

item_t GoldenBar::getID(){ return 280;}

FireItem::FireItem():BaseItem()
{
	textureRow = 0;

	plane = 0;
}

item_t FireItem::getID(){ return 281;}

LigniteCoal::LigniteCoal():BaseItem()
{
	textureRow = 14;

	plane = 9;
}

item_t LigniteCoal::getID(){ return 282;}

Clay::Clay():BaseItem()
{
	textureRow = 12;

	plane = 9;

	furnItem = 289;

	name = "Clay";
}

item_t Clay::getID(){ return 283;}


Apple::Apple():BaseItem()
{
	textureRow = 15;

	plane = 10;

	name = "Apple";
}

item_t Apple::getID(){ return 284;}


GoldenApple::GoldenApple():BaseItem()
{
	textureRow = 15;

	plane = 11;

	name = "Golden Apple";
}

item_t GoldenApple::getID(){ return 285;}

WheatSeeds::WheatSeeds():BaseItem()
{
	textureRow = 15;

	plane = 9;

	name = "Seeds";
}

item_t WheatSeeds::getID(){ return 286;}

Wheat::Wheat():BaseItem()
{
	textureRow = 14;

	plane = 9;

	name = "Wheat";
}

item_t Wheat::getID(){ return 287;}


WhiteBread::WhiteBread():BaseItem()
{
	textureRow = 13;

	plane = 9;

	name = "Bread";
}

item_t WhiteBread::getID(){ return 288;}

Brick::Brick():BaseItem()
{
	textureRow = 14;

	plane = 6;

	name = "Brick";
}

item_t Brick::getID(){ return 289;}

Busket::Busket():BaseItem()
{
	textureRow = 11;

	plane = 10;
	stackable = false;

	name = "Bucket";
}

item_t Busket::getID(){ return 290;}

WaterBusket::WaterBusket():BaseItem()
{
	textureRow = 11;

	plane = 11;
	stackable = false;

	name = "Water Bucket";
}

item_t WaterBusket::getID(){ return 291;}

LavaBusket::LavaBusket():BaseItem()
{
	textureRow = 11;

	plane = 12;
	stackable = false;

	name = "Lava Bucket";
}

item_t LavaBusket::getID(){ return 292;}

CaneItem::CaneItem():BaseItem()
{
	textureRow = 14;

	plane = 11;

	name = "Sugar Canes";
}

item_t CaneItem::getID(){ return 293;}

Flower1Item::Flower1Item():BaseItem()
{
	textureRow = 15;

	plane = 12;

	name = "Rose";
	terrainTexture = true;
}

item_t Flower1Item::getID(){ return 294;}

Flower2Item::Flower2Item():BaseItem()
{
	textureRow = 15;

	plane = 13;

	name = "Dandelion";
	terrainTexture = true;
}

item_t Flower2Item::getID(){ return 295;}

OakSaplingItem::OakSaplingItem():BaseItem()
{
	textureRow = 15;

	plane = 15;

	name = "Oak Sapling";
	terrainTexture = true;
}

item_t OakSaplingItem::getID(){ return 296;}

Paper::Paper():BaseItem()
{
	textureRow = 12;

	plane = 10;

	name = "Paper";
}

item_t Paper::getID(){ return 297;}

Book::Book():BaseItem()
{
	textureRow = 12;

	plane = 11;

	name = "Book";
}

item_t Book::getID(){ return 298;}

SnowBallItem::SnowBallItem():BaseItem()
{
	textureRow = 15;

	plane = 14;

	name = "Snowball";
}

item_t SnowBallItem::getID(){ return 299;}

MooshroomItem1::MooshroomItem1():BaseItem()
{
	textureRow = 14;

	plane = 12;

	name = "Mushroom";
	terrainTexture = true;
}

item_t MooshroomItem1::getID(){ return 300;}


MooshroomItem2::MooshroomItem2():BaseItem()
{
	textureRow = 14;

	plane = 13;

	name = "Mushroom";
	terrainTexture = true;
}

item_t MooshroomItem2::getID(){ return 301;}

Bowl::Bowl():BaseItem()
{
	textureRow = 11;

	plane = 7;

	name = "Bowl";
}

item_t Bowl::getID(){ return 302;}

MooshroomBowl::MooshroomBowl():BaseItem()
{
	textureRow = 11;

	plane = 8;

	name = "Mushroom Stew";
}

item_t MooshroomBowl::getID(){ return 303;}

IronWateringCan::IronWateringCan():BaseItem()
{
	textureRow = 1;

	plane = 3;
}

item_t IronWateringCan::getID(){ return 304;}

DiamondWateringCan::DiamondWateringCan():BaseItem()
{
	textureRow = 1;

	plane = 4;
}

item_t DiamondWateringCan::getID(){ return 305;}

DoorItem::DoorItem():BaseItem()
{
	textureRow = 13;

	plane = 11;
	stackable = false;

	name = "Door";
}

item_t DoorItem::getID(){ return 306;}

LadderItem::LadderItem():BaseItem()
{
	textureRow = 10;

	plane = 3;

	name = "Ladder";
	terrainTexture = true;
}

item_t LadderItem::getID(){ return 307;}

TorchItem::TorchItem():BaseItem()
{
	textureRow = 10;

	plane = 0;

	name = "Torch";
	terrainTexture = true;
}

item_t TorchItem::getID(){ return 308;}

RedstoneTorchItem::RedstoneTorchItem():BaseItem()
{
	textureRow = 9;      // terrain row 6 = the lit redstone torch tile
	plane = 3;

	name = "Redstone Torch";
	terrainTexture = true;
}

item_t RedstoneTorchItem::getID(){ return 408;}

RepeaterItem::RepeaterItem():BaseItem()
{
	// Its own sprite off the item sheet, not the block's terrain tiles. `plane 9`, `textureRow 7` is
	// items.png cell (9,8), which nothing else on the sheet uses. The art there is the authentic vanilla
	// 16x16 sprite (`assets/minecraft/textures/items/repeater.png` out of the 1.7.3 client jar -- see the
	// extracted reference set under `_mc173_extract/`), copied in at 1:1 so it matches the other icons'
	// ~13x14 footprint instead of reading oversized. `terrainTexture = false` is what routes the inventory
	// through the item sheet; the block's own terrain tiles are only used to draw it in the world.
	textureRow = 7;
	plane = 9;

	name = "Redstone Repeater";
	terrainTexture = false;
}

item_t RepeaterItem::getID(){ return 409;}

SpruceSaplingItem::SpruceSaplingItem():BaseItem()
{
	textureRow = 15;

	plane = 14;

	name = "Spruce Sapling";
	terrainTexture = true;
}

item_t SpruceSaplingItem::getID(){ return 309;}

BirchSaplingItem::BirchSaplingItem():BaseItem()
{
	textureRow = 14;

	plane = 14;

	name = "Birch Sapling";
	terrainTexture = true;
}

item_t BirchSaplingItem::getID(){ return 310;}

MelonItem::MelonItem():BaseItem()
{
	textureRow = 9;

	plane = 13;

	name = "Melon";
}

item_t MelonItem::getID(){ return 311;}

MelonSeeds::MelonSeeds():BaseItem()
{
	textureRow = 12;

	plane = 14;

	name = "Melon Seeds";
}

item_t MelonSeeds::getID(){ return 312;}

Sugar::Sugar():BaseItem()
{
	textureRow = 15;

	plane = 13;

	name = "Sugar";
}

item_t Sugar::getID(){ return 313;}

CocoaBeans::CocoaBeans():BaseItem()
{
	textureRow = 8;
	plane = 14;
	itemType = 'Y';

	name = "Cocoa Beans";
}

item_t CocoaBeans::getID(){ return 314;}

Cookie::Cookie():BaseItem()
{
	textureRow = 10;

	plane = 12;

	name = "Cookie";
}

item_t Cookie::getID(){ return 315;}

RawBeef::RawBeef():BaseItem()
{
	textureRow = 9;

	plane = 9;
	furnItem = 317;

	name = "Raw Beef";
}

item_t RawBeef::getID(){ return 316;}

Steak::Steak():BaseItem()
{
	textureRow = 9;

	plane = 10;

	name = "Steak";
}

item_t Steak::getID(){ return 317;}


RawChicken::RawChicken():BaseItem()
{
	textureRow = 6;

	plane = 10;
	furnItem = 413; // CookedChicken

	name = "Raw Chicken";
}

item_t RawChicken::getID(){ return 412;}

CookedChicken::CookedChicken():BaseItem()
{
	textureRow = 6;

	plane = 11;

	name = "Cooked Chicken";
}

item_t CookedChicken::getID(){ return 413;}

Feather::Feather():BaseItem()
{
	textureRow = 2;

	plane = 7;

	name = "Feather";
}

item_t Feather::getID(){ return 411;}

Egg::Egg():BaseItem()
{
	textureRow = 2;

	plane = 6;

	name = "Egg";
}

item_t Egg::getID(){ return 414;}

String::String():BaseItem()
{
	textureRow = 2;

	plane = 5;

	name = "String";
}

item_t String::getID(){ return 415;}

SpiderEye::SpiderEye():BaseItem()
{
	textureRow = 2;

	plane = 8;

	name = "Spider Eye";
}

item_t SpiderEye::getID(){ return 416;}

Arrow::Arrow():BaseItem()
{
	textureRow = 2;

	plane = 3;

	name = "Arrow";
}

item_t Arrow::getID(){ return 418;}

Bow::Bow():BaseItem()
{
	textureRow = 2;

	plane = 4;

	// vanilla's bow is 384 uses and does not stack. The art is bow_standby, which is what vanilla
	// shows in the inventory and in hand when the bow is not drawn.
	durabilityPoints = 384;
	stackable = false;

	name = "Bow";
}

item_t Bow::getID(){ return 419;}

// The draw frames. All three live in atlas column 5 and the row numbering here counts from the bottom
// of the sheet (GetItemVerts: down = 1 - percent3 * (textureRow + 1)), so the sprite that is drawn
// *least* -- bow_pulling_0, the first frame of the draw -- has the *highest* row number. They are
// consecutive cells so the string reads as one continuous pull rather than three poses.
BowPulling0::BowPulling0():BaseItem()
{
	textureRow = 9;
	plane = 5;

	// Carried over from the bow so the frames are exchangeable with it in the hand. Nothing can
	// obtain one, so the only figure that matters is that they are not a tool and do not stack.
	durabilityPoints = 384;
	stackable = false;

	name = "bow_pulling_0";
}

item_t BowPulling0::getID(){ return 422;}

BowPulling1::BowPulling1():BaseItem()
{
	textureRow = 8;
	plane = 5;

	durabilityPoints = 384;
	stackable = false;

	name = "bow_pulling_1";
}

item_t BowPulling1::getID(){ return 423;}

BowPulling2::BowPulling2():BaseItem()
{
	textureRow = 7;
	plane = 5;

	durabilityPoints = 384;
	stackable = false;

	name = "bow_pulling_2";
}

item_t BowPulling2::getID(){ return 424;}

// 0.42 -- the scale every other held item uses -- is sized for a 16x16 block face held as a cube. A
// bow's sprite fills its whole cell, so at 0.42 the drawn bow is about a third of a skeleton's height
// and reads as a toy. This is vanilla-ish weapon proportions instead, and it is deliberately one
// shared number: the skeleton, the first-person hand and the third-person player all draw from it.
const float kHeldBowScale = 0.65f;

// The nocked arrow inside every bow sprite, measured off bow_pulling_0/1/2 rather than estimated.
//
// In each of the three pulling frames the arrow is a straight run of ten opaque pixels at *exactly*
// 45 degrees across the cell, its head at the upper-left end and its tail sitting in the string's
// nock -- so in the quad's own frame the arrow points along 225 degrees, and an upright sprite has it
// aimed a quarter turn above the horizon. `python _tmp_atlas/solve_bow_parallel.py` finds that run in
// the atlas, prints the angle and fails loudly if it is not 45, so this is read off the art.
const float kBowSpriteArrowAngle = PI / 4.0f;

// The tilt each hand has to add to put that arrow on the arm. This used to be one number shared by
// all three hands, which cannot be right: the hands do not apply the same rotations to the sprite, so
// the same artwork needs a different correction in each. The shared number that shipped (-PI/4) was
// solved below to be exactly right in the third person, and 22.5 degrees low on the skeleton and 18
// degrees low in the first person -- which is the "the bow is tipped back, tip it up" report.
//
// Third person. The item's own RotZ is headPitch - armPitch (so the bow's lean is the camera's and
// nothing else -- see kBowArmRaise) and the arm's RotZ multiplies *outside* it. Both are turns about
// the same Z axis, so they add, and the arm's own rotation therefore cancels out of the comparison:
//
//      the arrow, in the world = 180 + kBowSpriteArrowAngle (its own direction) - PI/2 + tilt + armZ
//      the arm,   in the world = 90 + armZ                  (the arm mesh is authored along +Y)
//
// Parallel is then 180 + art - PI/2 + tilt = 90, i.e. tilt = -art, with the quarter turns cancelling
// out of both sides. That is the neat part of stating it this way: the third person's number is read
// straight off the sprite, and it stays right if the artwork is ever redrawn.
const float kHeldBowAimTilt = -kBowSpriteArrowAngle;                        // = -PI/4, -45 degrees

// Skeleton. Its bow is drawn inside the arm's own RotZ (rHandAngle - animHandAngle + the draw terms),
// so the arm's walk and draw swings rotate the bow with the arm and drop out of the difference exactly
// as headPitch does above. The bow's own RotZ is 1.375*PI (the lean that leaves the artwork upright on
// the arm -- see Skeleton2.cpp), and the arrow has to come out on the arm's axis, which for a skeleton
// is rHandAngle = 1.5*PI turned by the artwork's angle. Solve 1.375PI + tilt = 1.5PI - art:
const float kHeldBowSkeletonTilt = (1.5f * PI - 1.375f * PI) - kBowSpriteArrowAngle;  // = -PI/8, -22.5 deg

// First person. No arm is drawn here -- the sprite is a flat quad held across the screen -- so
// "parallel to the arm" is taken as the same thing it means in the other two hands, where the arm is
// level at rest: the artwork's arrow ends up in the aim's own horizontal plane, so the bow points
// where the shot goes instead of below it. StatePlay's chain is RotX(-0.33) RotY(-1.47 + PI) followed
// by this RotZ, which is a three-dimensional chain and has no closed form as tidy as the two above, so
// it is solved numerically by the script named above. The value it prints, for animDest 0:
const float kHeldBowFirstPersonTilt = -0.46847f;                            // -26.85 degrees

int BowHeldMeshId(int stage)
{
	switch(stage)
	{
		case 1: return BowPulling0::getID();
		case 2: return BowPulling1::getID();
		case 3: return BowPulling2::getID();
		default: return Bow::getID();
	}
}

int BowDrawStage(float charge)
{
	if(charge <= 0.0f)
		return 0;
	int stage = 1 + (int)(charge * 3.0f);
	if(stage > 3)
		stage = 3;
	return stage;
}

bool IsBowItem(int itemId)
{
	return itemId == Bow::getID() || itemId == BowPulling0::getID() ||
	       itemId == BowPulling1::getID() || itemId == BowPulling2::getID();
}



LapisLazuli::LapisLazuli():BaseItem()
{
	textureRow = 7;
	plane = 14;
	itemType = 'Y';

	name = "Lapis Lazuli";
}

item_t LapisLazuli::getID(){ return 318;}

MilkBusket::MilkBusket():BaseItem()
{
	textureRow = 11;

	plane = 13;

	name = "Milk";
	stackable = false;
}

item_t MilkBusket::getID(){ return 319;}

CakeItem::CakeItem():BaseItem()
{
	textureRow = 14;

	plane = 13;

	name = "Cake";
	stackable = false;
}

item_t CakeItem::getID(){ return 320;}

Bone::Bone():BaseItem()
{
	textureRow = 14;

	plane = 12;

	name = "Bone";
}

item_t Bone::getID(){ return 321;}

BoneMeal::BoneMeal():BaseItem()
{
	textureRow = 4;
	plane = 15;
	itemType = 'Y';

	name = "Bone Meal";
}

item_t BoneMeal::getID(){ return 322;}

Leather::Leather():BaseItem()
{
	textureRow = 9;

	plane = 7;

	name = "Leather";
}

item_t Leather::getID(){ return 323;}

SampleHelmet::SampleHelmet():BaseItem()
{
	textureRow = 15;

	plane = 15;
	stackable = false;
}

item_t SampleHelmet::getID(){ return 324;}

SampleChestplate::SampleChestplate():BaseItem()
{
	textureRow = 14;

	plane = 15;
	stackable = false;
}

item_t SampleChestplate::getID(){ return 325;}

SampleLeggings::SampleLeggings():BaseItem()
{
	textureRow = 13;

	plane = 15;
	stackable = false;
}

item_t SampleLeggings::getID(){ return 326;}

SampleBoots::SampleBoots():BaseItem()
{
	textureRow = 12;

	plane = 15;
	stackable = false;
}

item_t SampleBoots::getID(){ return 327;}

LeatherHelmet::LeatherHelmet():BaseItem()
{
	textureRow = 15;

	plane = 0;
	itemType = 'H';
	durabilityPoints = 55;
	stackable = false;

	name = "Leather Helmet";
}

item_t LeatherHelmet::getID(){ return 328;}

LeatherChestplate::LeatherChestplate():BaseItem()
{
	textureRow = 14;

	plane = 0;
	itemType = 'C';
	durabilityPoints = 80;
	stackable = false;

	name = "Leather Chestplate";
}

item_t LeatherChestplate::getID(){ return 329;}

LeatherLeggings::LeatherLeggings():BaseItem()
{
	textureRow = 13;

	plane = 0;
	itemType = 'L';
	durabilityPoints = 75;
	stackable = false;

	name = "Leather Leggings";
}

item_t LeatherLeggings::getID(){ return 330;}

LeatherBoots::LeatherBoots():BaseItem()
{
	textureRow = 12;

	plane = 0;
	itemType = 'B';
	durabilityPoints = 65;
	stackable = false;

	name = "Leather Boots";
}

item_t LeatherBoots::getID(){ return 331;}

ChainmailHelmet::ChainmailHelmet():BaseItem()
{
	textureRow = 15;

	plane = 1;
	itemType = 'H';
	durabilityPoints = 165;
	stackable = false;

	name = "Chainmail Helmet";
}

item_t ChainmailHelmet::getID(){ return 332;}

ChainmailChestplate::ChainmailChestplate():BaseItem()
{
	textureRow = 14;

	plane = 1;
	itemType = 'C';
	durabilityPoints = 240;
	stackable = false;

	name = "Chainmail Chestplate";
}

item_t ChainmailChestplate::getID(){ return 333;}

ChainmailLeggings::ChainmailLeggings():BaseItem()
{
	textureRow = 13;

	plane = 1;
	itemType = 'L';
	durabilityPoints = 225;
	stackable = false;

	name = "Chainmail Leggings";
}

item_t ChainmailLeggings::getID(){ return 334;}

ChainmailBoots::ChainmailBoots():BaseItem()
{
	textureRow = 12;

	plane = 1;
	itemType = 'B';
	durabilityPoints = 195;
	stackable = false;

	name = "Chainmail Boots";
}

item_t ChainmailBoots::getID(){ return 335;}

IronHelmet::IronHelmet():BaseItem()
{
	textureRow = 15;

	plane = 2;
	itemType = 'H';
	durabilityPoints = 165;
	stackable = false;

	name = "Iron Helmet";
}

item_t IronHelmet::getID(){ return 336;}

IronChestplate::IronChestplate():BaseItem()
{
	textureRow = 14;

	plane = 2;
	itemType = 'C';
	durabilityPoints = 240;
	stackable = false;

	name = "Iron Chestplate";
}

item_t IronChestplate::getID(){ return 337;}

IronLeggings::IronLeggings():BaseItem()
{
	textureRow = 13;

	plane = 2;
	itemType = 'L';
	durabilityPoints = 225;
	stackable = false;

	name = "Iron Leggings";
}

item_t IronLeggings::getID(){ return 338;}

IronBoots::IronBoots():BaseItem()
{
	textureRow = 12;

	plane = 2;
	itemType = 'B';
	durabilityPoints = 195;
	stackable = false;

	name = "Iron Boots";
}

item_t IronBoots::getID(){ return 339;}

DiamondHelmet::DiamondHelmet():BaseItem()
{
	textureRow = 15;

	plane = 3;
	itemType = 'H';
	durabilityPoints = 363;
	stackable = false;

	name = "Diamond Helmet";
}

item_t DiamondHelmet::getID(){ return 340;}

DiamondChestplate::DiamondChestplate():BaseItem()
{
	textureRow = 14;

	plane = 3;
	itemType = 'C';
	durabilityPoints = 528;
	stackable = false;

	name = "Diamond Chestplate";
}

item_t DiamondChestplate::getID(){ return 341;}

DiamondLeggings::DiamondLeggings():BaseItem()
{
	textureRow = 13;

	plane = 3;
	itemType = 'L';
	durabilityPoints = 495;
	stackable = false;

	name = "Diamond Leggings";
}

item_t DiamondLeggings::getID(){ return 342;}

DiamondBoots::DiamondBoots():BaseItem()
{
	textureRow = 12;

	plane = 3;
	itemType = 'B';
	durabilityPoints = 429;
	stackable = false;

	name = "Diamond Boots";
}

item_t DiamondBoots::getID(){ return 343;}

GoldenHelmet::GoldenHelmet():BaseItem()
{
	textureRow = 15;

	plane = 4;
	itemType = 'H';
	durabilityPoints = 77;
	stackable = false;

	name = "Golden Helmet";
}

item_t GoldenHelmet::getID(){ return 344;}

GoldenChestplate::GoldenChestplate():BaseItem()
{
	textureRow = 14;

	plane = 4;
	itemType = 'C';
	durabilityPoints = 112;
	stackable = false;

	name = "Golden Chestplate";
}

item_t GoldenChestplate::getID(){ return 345;}

GoldenLeggings::GoldenLeggings():BaseItem()
{
	textureRow = 13;

	plane = 4;
	itemType = 'L';
	durabilityPoints = 105;
	stackable = false;

	name = "Golden Leggings";
}

item_t GoldenLeggings::getID(){ return 346;}

GoldenBoots::GoldenBoots():BaseItem()
{
	textureRow = 12;

	plane = 4;
	itemType = 'B';
	durabilityPoints = 91;
	stackable = false;

	name = "Golden Boots";
}

item_t GoldenBoots::getID(){ return 347;}

RottenFlesh::RottenFlesh():BaseItem()
{
	textureRow = 10;

	plane = 11;

	name = "Rotten Flesh";
}

item_t RottenFlesh::getID(){ return 348;}

GlassPanelItem::GlassPanelItem():BaseItem()
{
	textureRow = 12;
	plane = 1;

	terrainTexture = true;

	name = "Glass Panel";
}

item_t GlassPanelItem::getID(){ return 349;}

IronBarsItem::IronBarsItem():BaseItem()
{
	textureRow = 10;
	plane = 5;

	terrainTexture = true;

	name = "Iron Bars";
}

item_t IronBarsItem::getID(){ return 350;}

Flint::Flint():BaseItem()
{
	textureRow = 15;
	plane = 6;

	name = "Flint";
}

item_t Flint::getID(){ return 351;}

Gunpowder::Gunpowder():BaseItem()
{
	textureRow = 13;
	plane = 8;

	name = "Gunpowder";
}

item_t Gunpowder::getID(){ return 352;}

Redstone::Redstone():BaseItem()
{
	textureRow = 12;
	plane = 8;

	name = "Redstone";
}

item_t Redstone::getID(){ return 353;}

Clock::Clock():BaseItem()
{
	textureRow = 11;
	plane = 6;
	stackable = false;

	name = "Clock";
}

item_t Clock::getID(){ return 354;}

Compass::Compass():BaseItem()
{
	textureRow = 12;
	plane = 6;
	stackable = false;

	name = "Compass";
}

item_t Compass::getID(){ return 355;}

Saddle::Saddle():BaseItem()
{
	textureRow = 9;
	plane = 8;
	stackable = false;

	name = "Saddle";
}

item_t Saddle::getID(){ return 356;}

Sign::Sign():BaseItem()
{
	textureRow = 13;
	plane = 10;
	stackable = false;

	name = "Sign";
}

item_t Sign::getID(){ return 357;}

ItemFrameItem::ItemFrameItem():BaseItem()
{
	textureRow = 3;
	plane = 14;

	name = "Item Frame";
}

item_t ItemFrameItem::getID(){ return 358;}

FlowerPotItem::FlowerPotItem():BaseItem()
{
	textureRow = 3;
	plane = 15;

	name = "Flower Pot";
}

item_t FlowerPotItem::getID(){ return 359;}

PaintningItem::PaintningItem():BaseItem()
{
	textureRow = 14;
	plane = 10;

	name = "Paintning";
}

item_t PaintningItem::getID(){ return 360;}

BedItem::BedItem():BaseItem()
{
	textureRow = 13;
	plane = 13;
	stackable = false;

	name = "Bed";
}

item_t BedItem::getID(){ return 361;}


Disk1::Disk1():BaseItem()
{
	textureRow = 0;
	plane = 0;
	stackable = false;

	name = "Music Disk";
}

item_t Disk1::getID(){ return 362;}

Disk2::Disk2():BaseItem()
{
	textureRow = 0;
	plane = 1;
	stackable = false;

	name = "Music Disk";
}

item_t Disk2::getID(){ return 363;}

Disk3::Disk3():BaseItem()
{
	textureRow = 0;
	plane = 2;
	stackable = false;

	name = "Music Disk";
}

item_t Disk3::getID(){ return 364;}


LightGrayDye::LightGrayDye():BaseItem()
{
	textureRow = 4;
	plane = 14;
	itemType = 'Y';

	name = "Light Gray Dye";
}

item_t LightGrayDye::getID(){ return 365;}

GrayDye::GrayDye():BaseItem()
{
	textureRow = 11;
	plane = 15;
	itemType = 'Y';

	name = "Gray Dye";
}

item_t GrayDye::getID(){ return 366;}

InkSac::InkSac():BaseItem()
{
	textureRow = 11;
	plane = 14;
	itemType = 'Y';

	name = "Ink Sac";
}

item_t InkSac::getID(){ return 367;}

PinkDye::PinkDye():BaseItem()
{
	textureRow = 10;
	plane = 15;
	itemType = 'Y';

	name = "Pink Dye";
}

item_t PinkDye::getID(){ return 368;}

RoseRed::RoseRed():BaseItem()
{
	textureRow = 10;
	plane = 14;
	itemType = 'Y';

	name = "Rose Red";
}

item_t RoseRed::getID(){ return 369;}

OrangeDye::OrangeDye():BaseItem()
{
	textureRow = 5;
	plane = 15;
	itemType = 'Y';

	name = "Orange Dye";
}

item_t OrangeDye::getID(){ return 370;}

DandelionYellow::DandelionYellow():BaseItem()
{
	textureRow = 8;
	plane = 15;
	itemType = 'Y';

	name = "Dandelion Yellow";
}

item_t DandelionYellow::getID(){ return 371;}

LimeDye::LimeDye():BaseItem()
{
	textureRow = 9;
	plane = 15;
	itemType = 'Y';

	name = "Lime Dye";
}

item_t LimeDye::getID(){ return 372;}

CactusGreen::CactusGreen():BaseItem()
{
	textureRow = 9;
	plane = 14;
	itemType = 'Y';

	name = "Cactus Green";
}

item_t CactusGreen::getID(){ return 373;}

LightBlueDye::LightBlueDye():BaseItem()
{
	textureRow = 7;
	plane = 15;
	itemType = 'Y';

	name = "Light Blue Dye";
}

item_t LightBlueDye::getID(){ return 374;}

PurpleDye::PurpleDye():BaseItem()
{
	textureRow = 6;
	plane = 14;
	itemType = 'Y';

	name = "Purple Dye";
}

item_t PurpleDye::getID(){ return 375;}

MagnetaDye::MagnetaDye():BaseItem()
{
	textureRow = 6;
	plane = 15;
	itemType = 'Y';

	name = "Magneta Dye";
}

item_t MagnetaDye::getID(){ return 376;}


PumpkinSeeds::PumpkinSeeds():BaseItem()
{
	textureRow = 12;
	plane = 13;

	name = "Pumpkin Seeds";
}

item_t PumpkinSeeds::getID(){ return 377;}

FlintAndSteel::FlintAndSteel():BaseItem()
{
	textureRow = 15;
	plane = 5;
	stackable = false;

	durabilityPoints = 65;

	name = "Flint and Steel";
}

item_t FlintAndSteel::getID(){ return 378;}

Potato::Potato():BaseItem()
{
	textureRow = 8;
	plane = 7;
	furnItem = 380;

	name = "Potato";
}

item_t Potato::getID(){ return 379;}

BakedPotato::BakedPotato():BaseItem()
{
	textureRow = 8;
	plane = 6;

	name = "Baked Potato";
}

item_t BakedPotato::getID(){ return 380;}

PoisonedPotato::PoisonedPotato():BaseItem()
{
	textureRow = 7;
	plane = 6;

	name = "Poisioned Potato";
}

item_t PoisonedPotato::getID(){ return 381;}

Carrot::Carrot():BaseItem()
{
	textureRow = 8;
	plane = 8;

	name = "Carrot";
}

item_t Carrot::getID(){ return 382;}

Disk4::Disk4():BaseItem()
{
	textureRow = 0;
	plane = 3;
	stackable = false;

	name = "Music Disk";
}

item_t Disk4::getID(){ return 383;}

Disk5::Disk5():BaseItem()
{
	textureRow = 0;
	plane = 4;
	stackable = false;

	name = "Music Disk";
}

item_t Disk5::getID(){ return 384;}

Disk6::Disk6():BaseItem()
{
	textureRow = 0;
	plane = 5;
	stackable = false;

	name = "Music Disk";
}

item_t Disk6::getID(){ return 385;}

Disk7::Disk7():BaseItem()
{
	textureRow = 0;
	plane = 6;
	stackable = false;

	name = "Music Disk";
}

item_t Disk7::getID(){ return 386;}

Disk8::Disk8():BaseItem()
{
	textureRow = 0;
	plane = 7;
	stackable = false;

	name = "Music Disk";
}

item_t Disk8::getID(){ return 387;}

Disk9::Disk9():BaseItem()
{
	textureRow = 0;
	plane = 8;
	stackable = false;

	name = "Music Disk";
}

item_t Disk9::getID(){ return 388;}

Disk10::Disk10():BaseItem()
{
	textureRow = 0;
	plane = 9;
	stackable = false;

	name = "Music Disk";
}

item_t Disk10::getID(){ return 389;}

Disk11::Disk11():BaseItem()
{
	textureRow = 0;
	plane = 10;
	stackable = false;

	name = "Music Disk";
}

item_t Disk11::getID(){ return 390;}

Disk12::Disk12():BaseItem()
{
	textureRow = 0;
	plane = 11;
	stackable = false;

	name = "Music Disk";
}
item_t Disk12::getID(){ return 391;}

RawMutton::RawMutton():BaseItem()
{
	textureRow = 9;

	plane = 11;
	furnItem = 393;

	name = "Raw Mutton";
}

item_t RawMutton::getID(){ return 392;}

CookedMutton::CookedMutton():BaseItem()
{
	textureRow = 9;

	plane = 12;

	name = "Cooked Mutton";
}

item_t CookedMutton::getID(){ return 393;}

YellowGlassBlockPannelItem::YellowGlassBlockPannelItem():BaseItem()
{
	textureRow = 2;
	plane = 9;

	name = "Yellow Stained Glass Pane";
}

item_t YellowGlassBlockPannelItem::getID(){ return 394;}

GreenGlassBlockPannelItem::GreenGlassBlockPannelItem():BaseItem()
{
    textureRow = 2;
	plane = 10;

    name = "Green Stained Glass Pane";
}
item_t GreenGlassBlockPannelItem::getID(){ return 395;}

PinkGlassBlockPannelItem::PinkGlassBlockPannelItem():BaseItem()
{
    textureRow = 2;
	plane = 11;

    name = "Pink Stained Glass Pane";
}
item_t PinkGlassBlockPannelItem::getID(){ return 396;}

PurpleGlassBlockPannelItem::PurpleGlassBlockPannelItem():BaseItem()
{
    textureRow = 2;
	plane = 12;

    name = "Purple Stained Glass Pane";
}
item_t PurpleGlassBlockPannelItem::getID(){ return 397;}

/*OrangeGlassBlockPannelItem::OrangeGlassBlockPannelItem():BaseItem()
{
    textureRow = 2;
	plane = 13;

    name = "Orange Stained Glass Pane";
}
item_t OrangeGlassBlockPannelItem::getID(){ return 398;}*/

RedGlassBlockPannelItem::RedGlassBlockPannelItem():BaseItem()
{
    textureRow = 2;
	plane = 14;

    name = "Red Stained Glass Pane";
}
item_t RedGlassBlockPannelItem::getID(){ return 398;}

/*DGreenGlassBlockPannelItem::DGreenGlassBlockPannelItem():BaseItem()
{
    textureRow = 2;
	plane = 15;

    name = "Dark Green Stained Glass Pane";
}
item_t DGreenGlassBlockPannelItem::getID(){ return 400;}*/

BlackGlassBlockPannelItem::BlackGlassBlockPannelItem():BaseItem()
{
    textureRow = 1;
	plane = 10;

    name = "Black Stained Glass Pane";
}
item_t BlackGlassBlockPannelItem::getID(){ return 399;}

/*LGrayGlassBlockPannelItem::LGrayGlassBlockPannelItem():BaseItem()
{
    textureRow = 1;
	plane = 10;

    name = "Light Gray Stained Glass Pane";
}
item_t LGrayGlassBlockPannelItem::getID(){ return 402;}

GrayGlassBlockPannelItem::GrayGlassBlockPannelItem():BaseItem()
{
    textureRow = 1;
	plane = 10;

    name = "Gray Stained Glass Pane";
}
item_t GrayGlassBlockPannelItem::getID(){ return 403;}*/

/*WhiteGlassBlockPannelItem::WhiteGlassBlockPannelItem():BaseItem()
{
    textureRow = 1;
	plane = 12;

    name = "White Stained Glass Pane";
}
item_t WhiteGlassBlockPannelItem::getID(){ return 404;}*/

/*BrownGlassBlockPannelItem::BrownGlassBlockPannelItem():BaseItem()
{
    textureRow = 1;
	plane = 13;

    name = "Brown Stained Glass Pane";
}
item_t BrownGlassBlockPannelItem::getID(){ return 405;}

DBlueGlassBlockPannelItem::DBlueGlassBlockPannelItem():BaseItem()
{
   textureRow = 1;
	plane = 14;

    name = "Dark Blue Stained Glass Pane";
}
item_t DBlueGlassBlockPannelItem::getID(){ return 406;}

LBlueGlassBlockPannelItem::LBlueGlassBlockPannelItem():BaseItem()
{
    textureRow = 1;
	plane = 15;

    name = "Light Blue Stained Glass Pane";
}
item_t LBlueGlassBlockPannelItem::getID(){ return 407;}*/

BlueGlassBlockPannelItem::BlueGlassBlockPannelItem():BaseItem()
{
    textureRow = 0;
	plane = 14;

    name = "Blue Stained Glass Pane";
}
item_t BlueGlassBlockPannelItem::getID(){ return 400;}

/*DPurpleGlassBlockPannelItem::DPurpleGlassBlockPannelItem():BaseItem()
{
    textureRow = 0;
	plane = 15;

    name = "Dark Purple Stained Glass Pane";
}
item_t DPurpleGlassBlockPannelItem::getID(){ return 409;}*/

RawPorkchop::RawPorkchop():BaseItem()
{
    textureRow = 10;
	furnItem = 402; // CookedPorkchop
	plane = 7;

    name = "Raw Porkchop";
}
item_t RawPorkchop::getID(){ return 401;}

CookedPorkchop::CookedPorkchop():BaseItem()
{
    textureRow = 10;
	plane = 8;

    name = "Cooked Porkchop";
}
item_t CookedPorkchop::getID(){ return 402;}

CowSpawnEgg::CowSpawnEgg():BaseItem()
{
    textureRow = 6;
    plane = 0;
    name = "Spawn Cow";
}
item_t CowSpawnEgg::getID(){ return 403;}

SheepSpawnEgg::SheepSpawnEgg():BaseItem()
{
    textureRow = 6;
    plane = 1;
    name = "Spawn Sheep";
}
item_t SheepSpawnEgg::getID(){ return 404;}

ZombieSpawnEgg::ZombieSpawnEgg():BaseItem()
{
    textureRow = 6;
    plane = 2;
    name = "Spawn Zombie";
}
item_t ZombieSpawnEgg::getID(){ return 405;}

CreeperSpawnEgg::CreeperSpawnEgg():BaseItem()
{
    textureRow = 6;
    plane = 3;
    name = "Spawn Creeper";
}
item_t CreeperSpawnEgg::getID(){ return 406;}
PigSpawnEgg::PigSpawnEgg():BaseItem()
{
    textureRow = 6;
    plane = 4;
    name = "Spawn Pig";
}
item_t PigSpawnEgg::getID(){ return 407;}
ChickenSpawnEgg::ChickenSpawnEgg():BaseItem()
{
    textureRow = 6;
    plane = 9;   // row 6 is the spawn-egg row; planes 4..8 are taken, 9..12 were free
    name = "Spawn Chicken";
}
item_t ChickenSpawnEgg::getID(){ return 410;}

SpiderSpawnEgg::SpiderSpawnEgg():BaseItem()
{
    textureRow = 6;
    plane = 12;   // row 6 is the spawn-egg row; 12 is the free cell after the chicken's 9
    name = "Spawn Spider";
}
item_t SpiderSpawnEgg::getID(){ return 417;}

SkeletonSpawnEgg::SkeletonSpawnEgg():BaseItem()
{
    textureRow = 6;
    // Row 6 is the spawn-egg row and plane 5 was the one cell in it nothing else declared. It held ten
    // stray pixels (teal 73,161,148) which _tmp_atlas/make_chicken_egg.py clears before drawing, and
    // the art is generated the same way as the rest: the pack's own sheep tile recoloured with Java's
    // EntityList pair for the skeleton, 0xC1C1C1 shell and 0x494949 spots (entity id 51).
    plane = 5;
    name = "Spawn Skeleton";
}
item_t SkeletonSpawnEgg::getID(){ return 420;}
