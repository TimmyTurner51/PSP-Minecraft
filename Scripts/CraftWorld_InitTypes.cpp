#include "CraftWorld2.h"
#include "DebugConfig.h"

void CraftWorld::InitBlockVector()
{
	if(blockTypes.empty() == false)
		return;

	// Reserved as ordinary hygiene, not for safety. It used to be load bearing: the mesh buffers inside
	// BaseBlock were owned by BaseBlock, its copy is shallow, so a vector reallocation past this reserve
	// would have freed a buffer twice. The buffers are owned by CraftWorld now (see
	// FreeBlockTypeMeshes), which makes a reallocation merely wasteful instead of fatal -- so if you
	// register past kExtendedBlockBase + 64 you will leak nothing, but you will pay a copy of every
	// built block, and you should raise this number anyway.
	blockTypes.reserve(kExtendedBlockBase + 64);

	//Air
	blockTypes.push_back(BaseBlock());
	//Classic blocks
	blockTypes.push_back(GrassBlock());
	blockTypes.push_back(DirtBlock());
	blockTypes.push_back(RockBlock());
	blockTypes.push_back(WaterBlock());
	blockTypes.push_back(GoldBlock());
	blockTypes.push_back(LadderBlock());
	blockTypes.push_back(SandBlock());
	blockTypes.push_back(WoodBlock());
	blockTypes.push_back(LeavesBlock());

	blockTypes.push_back(BlackWoolBlock());
	blockTypes.push_back(GrayWoolBlock());
	blockTypes.push_back(RedWoolBlock());
	blockTypes.push_back(PinkWoolBlock());
	blockTypes.push_back(DarkGreenWoolBlock());
	blockTypes.push_back(GreenWoolBlock());
	blockTypes.push_back(BrownWoolBlock());
	blockTypes.push_back(YellowWoolBlock());
	blockTypes.push_back(BlueWoolBlock());

	blockTypes.push_back(LightBlueWoolBlock());
	blockTypes.push_back(VioletWoolBlock());
	blockTypes.push_back(PastelWoolBlock());
	blockTypes.push_back(OrangeWoolBlock());
	blockTypes.push_back(LightGrayWoolBlock());
	blockTypes.push_back(WhiteWoolBlock());
	blockTypes.push_back(CactusBlock());
	blockTypes.push_back(SandStone());
	blockTypes.push_back(BrickBlock());

	blockTypes.push_back(CageBlock());
	blockTypes.push_back(SnowSoil());
	blockTypes.push_back(DarkWoodBlock());
	blockTypes.push_back(WhiteWoodBlock());
	blockTypes.push_back(IceBlock());
	blockTypes.push_back(CaneBlock());
	blockTypes.push_back(OakPlanks());
	blockTypes.push_back(ShelfBlock());
	blockTypes.push_back(CobbleStone());

	blockTypes.push_back(SpruceLeaves());
	blockTypes.push_back(BirchLeaves());
	blockTypes.push_back(Sponge());
	blockTypes.push_back(GlassBlock());
	blockTypes.push_back(IronOre());
	blockTypes.push_back(CoalOre());
	blockTypes.push_back(DiamondOre());
	blockTypes.push_back(RedstoneOre());
	blockTypes.push_back(LapisOre());

	blockTypes.push_back(Obsidian());
	blockTypes.push_back(SnowBlock());
	blockTypes.push_back(Torch());
	blockTypes.push_back(Door1());
	blockTypes.push_back(Door2());
	blockTypes.push_back(Door3());
	blockTypes.push_back(Door4());
	blockTypes.push_back(Door5());
	blockTypes.push_back(Door6());

	blockTypes.push_back(Door7());
	blockTypes.push_back(Door8());
	blockTypes.push_back(Door9());
	blockTypes.push_back(Door10());
	blockTypes.push_back(WoodenFence());
	blockTypes.push_back(CobbleStair1());
	blockTypes.push_back(CobbleStair2());
	blockTypes.push_back(CobbleStair3());
	blockTypes.push_back(CobbleStair4());

	blockTypes.push_back(Diamond());
	blockTypes.push_back(Gold());
	blockTypes.push_back(Iron());
	blockTypes.push_back(BrickStair1());
	blockTypes.push_back(BrickStair2());
	blockTypes.push_back(BrickStair3());
	blockTypes.push_back(BrickStair4());
	blockTypes.push_back(NoteBlock());
	blockTypes.push_back(JukeBox());

	blockTypes.push_back(Pumpkin1());
	blockTypes.push_back(Pumpkin2());
	blockTypes.push_back(Pumpkin3());
	blockTypes.push_back(Pumpkin4());
	blockTypes.push_back(Mooshroom1());
	blockTypes.push_back(Mooshroom2());
	blockTypes.push_back(Soil());
	blockTypes.push_back(WateredSoil());
	blockTypes.push_back(MelonBlock());

	blockTypes.push_back(OakHalfBlock());
	blockTypes.push_back(CobbleHalfBlock());
	blockTypes.push_back(BrickHalfBlock());
	blockTypes.push_back(Slab());
	blockTypes.push_back(StoneSandHalfBlock());
	blockTypes.push_back(Ladder1());
	blockTypes.push_back(Ladder2());
	blockTypes.push_back(Ladder3());
	blockTypes.push_back(Ladder4());

	blockTypes.push_back(DoubleSlab());
	blockTypes.push_back(MossyCobblestone());
	blockTypes.push_back(Snow2());
	blockTypes.push_back(ChestBlock1());
	blockTypes.push_back(ChestBlock2());
	blockTypes.push_back(ChestBlock3());
	blockTypes.push_back(ChestBlock4());
	blockTypes.push_back(ChestLock());
	blockTypes.push_back(ClayBlock());

	blockTypes.push_back(Chest5());
	blockTypes.push_back(LapisBlock());
	blockTypes.push_back(StoneBrick());
	blockTypes.push_back(CrackedStoneBrick());
	blockTypes.push_back(CarvedStoneBrick());
	blockTypes.push_back(CraftingTable());
	blockTypes.push_back(FurnaceOff());
	blockTypes.push_back(FurnaceOn());
	blockTypes.push_back(HalfStoneBrick());

	blockTypes.push_back(Spawner());
	blockTypes.push_back(TNTBlock());
	blockTypes.push_back(Nuclear());
	blockTypes.push_back(Lava());
	blockTypes.push_back(Gravel());
	blockTypes.push_back(GlassPanel());
	blockTypes.push_back(IronBars());
	blockTypes.push_back(TallGrass());
	blockTypes.push_back(Fern());

	blockTypes.push_back(DrySapling());
	blockTypes.push_back(Flower1());
	blockTypes.push_back(Flower2());
	blockTypes.push_back(OakSapling());
	blockTypes.push_back(SpruceSapling());
	blockTypes.push_back(BirchSapling());
	blockTypes.push_back(FreeBlock5());
	blockTypes.push_back(WoodenStair1());
	blockTypes.push_back(WoodenStair2());

	blockTypes.push_back(WoodenStair3());
	blockTypes.push_back(WoodenStair4());
	blockTypes.push_back(MelonPlant4Reverse());
	blockTypes.push_back(MelonPlant1());
	blockTypes.push_back(MelonPlant2());
	blockTypes.push_back(MelonPlant3());
	blockTypes.push_back(MelonPlant4());

	blockTypes.push_back(WheatBlock1());
	blockTypes.push_back(WheatBlock2());
	blockTypes.push_back(WheatBlock3());
	blockTypes.push_back(WheatBlock4());

	blockTypes.push_back(WheatBlock5());
	blockTypes.push_back(WheatBlock6());
	blockTypes.push_back(WorldSky());
	blockTypes.push_back(Cloud());
	blockTypes.push_back(bedrock());

	blockTypes.push_back(JackOLantern1());
	blockTypes.push_back(JackOLantern2());
	blockTypes.push_back(JackOLantern3());
	blockTypes.push_back(JackOLantern4());

	blockTypes.push_back(TrapdoorOpen1());
	blockTypes.push_back(TrapdoorOpen2());
	blockTypes.push_back(TrapdoorOpen3());
	blockTypes.push_back(TrapdoorOpen4());
	blockTypes.push_back(TrapdoorClosed1());
	blockTypes.push_back(TrapdoorClosed2());
	blockTypes.push_back(TrapdoorClosed3());
	blockTypes.push_back(TrapdoorClosed4());

	blockTypes.push_back(BirchPlanks());
	blockTypes.push_back(SprucePlanks());
	blockTypes.push_back(BirchHalfBlock());
	blockTypes.push_back(SpruceHalfBlock());
	blockTypes.push_back(BirchFence());
	blockTypes.push_back(SpruceFence());

	blockTypes.push_back(BirchStair1());
	blockTypes.push_back(BirchStair2());
	blockTypes.push_back(BirchStair3());
	blockTypes.push_back(BirchStair4());

	blockTypes.push_back(SpruceStair1());
	blockTypes.push_back(SpruceStair2());
	blockTypes.push_back(SpruceStair3());
	blockTypes.push_back(SpruceStair4());

	blockTypes.push_back(WaterBlock1());
	blockTypes.push_back(WaterBlock2());
	blockTypes.push_back(WaterBlock3());
	blockTypes.push_back(WaterBlock4());

	blockTypes.push_back(ItemFrame1());
	blockTypes.push_back(ItemFrame2());
	blockTypes.push_back(ItemFrame3());
	blockTypes.push_back(ItemFrame4());

	blockTypes.push_back(PumpkinPlant4Reverse());
	blockTypes.push_back(PumpkinPlant1());
	blockTypes.push_back(PumpkinPlant2());
	blockTypes.push_back(PumpkinPlant3());
	blockTypes.push_back(PumpkinPlant4());

	blockTypes.push_back(PotatoPlant1());
	blockTypes.push_back(PotatoPlant2());
	blockTypes.push_back(PotatoPlant3());
	blockTypes.push_back(PotatoPlant4());

	blockTypes.push_back(CarrotPlant1());
	blockTypes.push_back(CarrotPlant2());
	blockTypes.push_back(CarrotPlant3());
	blockTypes.push_back(CarrotPlant4());

	blockTypes.push_back(BedBlock1());
	blockTypes.push_back(BedBlock2());
	blockTypes.push_back(BedBlock3());
	blockTypes.push_back(BedBlock4());

	blockTypes.push_back(BedBlock5());
	blockTypes.push_back(BedBlock6());
	blockTypes.push_back(BedBlock7());
	blockTypes.push_back(BedBlock8());

	blockTypes.push_back(BlackWoolCarpet());
	blockTypes.push_back(GrayWoolCarpet());
	blockTypes.push_back(RedWoolCarpet());
	blockTypes.push_back(PinkWoolCarpet());
	blockTypes.push_back(DarkGreenWoolCarpet());
	blockTypes.push_back(GreenWoolCarpet());
	blockTypes.push_back(BrownWoolCarpet());
	blockTypes.push_back(YellowWoolCarpet());
	blockTypes.push_back(BlueWoolCarpet());
	blockTypes.push_back(LightBlueWoolCarpet());
	blockTypes.push_back(VioletWoolCarpet());
	blockTypes.push_back(PastelWoolCarpet());
	blockTypes.push_back(OrangeWoolCarpet());
	blockTypes.push_back(LightGrayWoolCarpet());
	blockTypes.push_back(WhiteWoolCarpet());

	blockTypes.push_back(HayBale());
	blockTypes.push_back(CoalBlock());
	blockTypes.push_back(RedstoneBlock());

	blockTypes.push_back(YellowGlassBlock());
	blockTypes.push_back(GreenGlassBlock());
	blockTypes.push_back(PinkGlassBlock());
	blockTypes.push_back(PurpleGlassBlock());
	blockTypes.push_back(RedGlassBlock());
	blockTypes.push_back(BlackGlassBlock());
	blockTypes.push_back(BlueGlassBlock());
	blockTypes.push_back(GlowstoneBlock());
	blockTypes.push_back(Soulsand());
	blockTypes.push_back(Netherrack());
	blockTypes.push_back(MagmaBlock());
	blockTypes.push_back(Redstoneoff());
	blockTypes.push_back(BlockofQuartz());
	blockTypes.push_back(ChiseledQuartzBlock());
	blockTypes.push_back(PillarQuartzBlock());
	blockTypes.push_back(white_concrete_powder());
	blockTypes.push_back(yellow_concrete_powder());
	blockTypes.push_back(pink_concrete_powder());
	blockTypes.push_back(gray_concrete_powder());
	blockTypes.push_back(purple_concrete_powder());
	blockTypes.push_back(blue_concrete_powder());
	blockTypes.push_back(green_concrete_powder());
	blockTypes.push_back(red_concrete_powder());
	blockTypes.push_back(black_concrete_powder());
	blockTypes.push_back(YellowGlassBlockPannel());
	blockTypes.push_back(GreenGlassBlockPannel());
	blockTypes.push_back(PinkGlassBlockPannel());
	blockTypes.push_back(PurpleGlassBlockPannel());
	blockTypes.push_back(RedGlassBlockPannel());
	blockTypes.push_back(BlackGlassBlockPannel());
	blockTypes.push_back(BlueGlassBlockPannel());
	blockTypes.push_back(Fire());
	blockTypes.push_back(GravitySandBlock());
	blockTypes.push_back(LavaBlock1());
	blockTypes.push_back(WallTorch1());
	blockTypes.push_back(WallTorch2());
	blockTypes.push_back(WallTorch3());
	blockTypes.push_back(WallTorch4());
	// Ids 254..2047 belong to items. Further blocks go through AddBlockType
	// at kExtendedBlockBase (2048) and up, which block_t can store through 65535.
	// Must run here, before buildblocksVerts: BaseBlock copies are shallow and
	// resizing after vertex buffers exist would free them twice.
	//
	// `legacyBlockCount` is the boundary between the flat table above and everything registered by
	// id below. The tile audit at the end of this function needs it to tell a real block from the
	// default-filled gap: AddBlockType resizes blockTypes up to 2048, so indices 254..2047 exist as
	// default-constructed BaseBlocks whose every face is (0,0). Without this marker the audit reports
	// all 1794 of them as grass-topped blocks and buries the handful that are real.
	const unsigned int legacyBlockCount = (unsigned int)blockTypes.size();
	AddBlockType(WaterBlock5::getID(), WaterBlock5());
	AddBlockType(WaterBlock6::getID(), WaterBlock6());
	AddBlockType(WaterBlock7::getID(), WaterBlock7());
	AddBlockType(ButtonStone::getID(), ButtonStone());
	AddBlockType(ButtonWood::getID(), ButtonWood());
	AddBlockType(RepeaterOff::getID(), RepeaterOff());
	AddBlockType(RepeaterOn::getID(), RepeaterOn());
	AddBlockType(RedstoneTorch::getID(), RedstoneTorch());
	AddBlockType(RedstoneWallTorch1::getID(), RedstoneWallTorch1());
	AddBlockType(RedstoneWallTorch2::getID(), RedstoneWallTorch2());
	AddBlockType(RedstoneWallTorch3::getID(), RedstoneWallTorch3());
	AddBlockType(RedstoneWallTorch4::getID(), RedstoneWallTorch4());
	AddBlockType(UnlitRedstoneTorch::getID(), UnlitRedstoneTorch());
	AddBlockType(UnlitRedstoneWallTorch1::getID(), UnlitRedstoneWallTorch1());
	AddBlockType(UnlitRedstoneWallTorch2::getID(), UnlitRedstoneWallTorch2());
	AddBlockType(UnlitRedstoneWallTorch3::getID(), UnlitRedstoneWallTorch3());
	AddBlockType(UnlitRedstoneWallTorch4::getID(), UnlitRedstoneWallTorch4());
	AddBlockType(Lever::getID(), Lever());
	AddBlockType(RedstoneLampOff::getID(), RedstoneLampOff());
	AddBlockType(RedstoneLampOn::getID(), RedstoneLampOn());
	AddBlockType(PistonOff::getID(), PistonOff());
	AddBlockType(PistonOn::getID(), PistonOn());
	AddBlockType(StickyPistonOff::getID(), StickyPistonOff());
	AddBlockType(StickyPistonOn::getID(), StickyPistonOn());
	AddBlockType(PistonHead::getID(), PistonHead());
	AddBlockType(StickyPistonHead::getID(), StickyPistonHead());
	// The nether portal, at kExtendedBlockBase + 26 and + 27: one id per plane the frame can lie in.
	// Last of the extended blocks, and they must be registered here rather than push_back'd: ids from
	// kItemIdBase (250) up to kExtendedBlockBase belong to items, so a push_back would land on top of
	// one.
	AddBlockType(PortalBlock::getID(), PortalBlock());
	AddBlockType(PortalBlockZ::getID(), PortalBlockZ());

	// ---------------------------------------------------------------------------------------------
	// Unset-tile audit.
	//
	// BaseBlock's constructor leaves all twelve plane fields at 0, and terrain atlas cell (0,0) is
	// GRASS TOP. So a block whose init forgets a face does not fail and does not look obviously
	// broken -- it silently paints that face with grass, which is exactly the "placing some blocks
	// gives it a grass top texture" report. Nothing in the build catches it: the zero field is a
	// legal value and the cell it names is a real tile.
	//
	// This names the offenders at boot so they can be fixed one by one. Two lists, because the two
	// cases mean different things:
	//   * every face at (0,0) -- no tile was set at all; almost certainly a block missing its init.
	//   * top face at (0,0)   -- only the top is unset; the sides were set, so the block renders
	//                            correctly everywhere except its roof, which is grass.
	// GrassBlock and the soil blocks use (0,0) on purpose, so both lists are printed whole and read
	// with that in mind rather than filtered by name -- a name filter would hide the next real one.
	// ---------------------------------------------------------------------------------------------
	{
		int allFaces = 0, topFace = 0;
		for(unsigned int i = 1; i < blockTypes.size(); i++)
		{
			// Skip the id-space gap. AddBlockType resizes the vector to 2048, so indices between the
			// flat table and kExtendedBlockBase hold default-constructed BaseBlocks -- every face at
			// (0,0) by construction, and not blocks at all. Reporting them is what made the first run
			// of this audit read "1796 blocks with no tiles set" when the real count is three.
			if(i >= legacyBlockCount && i < (unsigned int)kExtendedBlockBase)
				continue;

			const BaseBlock& b = blockTypes[i];
			const bool topAtGrass = (b.upPlane_x == 0 && b.upPlane_y == 0);
			const bool everyFaceAtGrass =
			    topAtGrass &&
			    b.downPlane_x == 0 && b.downPlane_y == 0 &&
			    b.leftPlane_x == 0 && b.leftPlane_y == 0 &&
			    b.rightPlane_x == 0 && b.rightPlane_y == 0 &&
			    b.botPlane_x == 0 && b.botPlane_y == 0 &&
			    b.forPlane_x == 0 && b.forPlane_y == 0;
			if(everyFaceAtGrass)
			{
				allFaces++;
				MC_DEBUG_LOG("TILE AUDIT id %d '%s': every face is atlas (0,0) = grass top (no tiles set)\n",
				             (int)i, b.name.c_str());
			}
			else if(topAtGrass)
			{
				topFace++;
				MC_DEBUG_LOG("TILE AUDIT id %d '%s': top face is atlas (0,0) = grass top\n",
				             (int)i, b.name.c_str());
			}
		}
		MC_DEBUG_LOG("TILE AUDIT: %d block(s) with no tiles set, %d with a grass-top roof\n",
		             allFaces, topFace);
	}
}

void CraftWorld::InitItemVector()
{
	itemTypes.push_back(WoodenPickaxe());
	itemTypes.push_back(StonePickaxe());
	itemTypes.push_back(IronPickaxe());
	itemTypes.push_back(DiamondPickaxe());
	itemTypes.push_back(GoldenPickaxe());

	itemTypes.push_back(WoodenSword());
	itemTypes.push_back(StoneSword());
	itemTypes.push_back(IronSword());
	itemTypes.push_back(DiamondSword());
	itemTypes.push_back(GoldenSword());

	itemTypes.push_back(WoodenShovel());
	itemTypes.push_back(StoneShovel());
	itemTypes.push_back(IronShovel());
	itemTypes.push_back(DiamondShovel());
	itemTypes.push_back(GoldenShovel());

	itemTypes.push_back(WoodenAxe());
	itemTypes.push_back(StoneAxe());
	itemTypes.push_back(IronAxe());
	itemTypes.push_back(DiamondAxe());
	itemTypes.push_back(GoldenAxe());

	itemTypes.push_back(WoodenHoe());
	itemTypes.push_back(StoneHoe());
	itemTypes.push_back(IronHoe());
	itemTypes.push_back(DiamondHoe());
	itemTypes.push_back(GoldenHoe());

	itemTypes.push_back(Scissors());
	itemTypes.push_back(Stick());
	itemTypes.push_back(Coal());
	itemTypes.push_back(IronBar());
	itemTypes.push_back(DiamondItem());

	itemTypes.push_back(GoldenBar());
	itemTypes.push_back(FireItem());
	itemTypes.push_back(LigniteCoal());
	itemTypes.push_back(Clay());
	itemTypes.push_back(Apple());
	itemTypes.push_back(GoldenApple());

	itemTypes.push_back(WheatSeeds());
	itemTypes.push_back(Wheat());
	itemTypes.push_back(WhiteBread());
	itemTypes.push_back(Brick());
	itemTypes.push_back(Busket());
	itemTypes.push_back(WaterBusket());
	itemTypes.push_back(LavaBusket());

	itemTypes.push_back(CaneItem());
	itemTypes.push_back(Flower1Item());
	itemTypes.push_back(Flower2Item());
	itemTypes.push_back(OakSaplingItem());
	itemTypes.push_back(Paper());
	itemTypes.push_back(Book());
	itemTypes.push_back(SnowBallItem());

	itemTypes.push_back(MooshroomItem1());
	itemTypes.push_back(MooshroomItem2());
	itemTypes.push_back(Bowl());
	itemTypes.push_back(MooshroomBowl());

	itemTypes.push_back(IronWateringCan());
	itemTypes.push_back(DiamondWateringCan());
	itemTypes.push_back(DoorItem());
	itemTypes.push_back(LadderItem());
	itemTypes.push_back(TorchItem());
	itemTypes.push_back(SpruceSaplingItem());
	itemTypes.push_back(BirchSaplingItem());

	itemTypes.push_back(MelonItem());
	itemTypes.push_back(MelonSeeds());
	itemTypes.push_back(Sugar());
	itemTypes.push_back(CocoaBeans());
	itemTypes.push_back(Cookie());
	itemTypes.push_back(RawBeef());
	itemTypes.push_back(Steak());
	itemTypes.push_back(LapisLazuli());
	itemTypes.push_back(MilkBusket());
	itemTypes.push_back(CakeItem());
	itemTypes.push_back(Bone());
	itemTypes.push_back(BoneMeal());
	itemTypes.push_back(Leather());

	itemTypes.push_back(SampleHelmet());
	itemTypes.push_back(SampleChestplate());
	itemTypes.push_back(SampleLeggings());
	itemTypes.push_back(SampleBoots());

	itemTypes.push_back(LeatherHelmet());
	itemTypes.push_back(LeatherChestplate());
	itemTypes.push_back(LeatherLeggings());
	itemTypes.push_back(LeatherBoots());

	itemTypes.push_back(ChainmailHelmet());
	itemTypes.push_back(ChainmailChestplate());
	itemTypes.push_back(ChainmailLeggings());
	itemTypes.push_back(ChainmailBoots());

	itemTypes.push_back(IronHelmet());
	itemTypes.push_back(IronChestplate());
	itemTypes.push_back(IronLeggings());
	itemTypes.push_back(IronBoots());

	itemTypes.push_back(DiamondHelmet());
	itemTypes.push_back(DiamondChestplate());
	itemTypes.push_back(DiamondLeggings());
	itemTypes.push_back(DiamondBoots());

	itemTypes.push_back(GoldenHelmet());
	itemTypes.push_back(GoldenChestplate());
	itemTypes.push_back(GoldenLeggings());
	itemTypes.push_back(GoldenBoots());

	itemTypes.push_back(RottenFlesh());
	itemTypes.push_back(GlassPanelItem());
	itemTypes.push_back(IronBarsItem());

	itemTypes.push_back(Flint());
	itemTypes.push_back(Gunpowder());
	itemTypes.push_back(Redstone());
	itemTypes.push_back(Clock());
	itemTypes.push_back(Compass());
	itemTypes.push_back(Saddle());
	itemTypes.push_back(Sign());
	itemTypes.push_back(ItemFrameItem());
	itemTypes.push_back(FlowerPotItem());
	itemTypes.push_back(PaintningItem());
	itemTypes.push_back(BedItem());
	itemTypes.push_back(Disk1());
	itemTypes.push_back(Disk2());
	itemTypes.push_back(Disk3());

	itemTypes.push_back(LightGrayDye());
	itemTypes.push_back(GrayDye());
	itemTypes.push_back(InkSac());
	itemTypes.push_back(PinkDye());
	itemTypes.push_back(RoseRed());
	itemTypes.push_back(OrangeDye());
	itemTypes.push_back(DandelionYellow());
	itemTypes.push_back(LimeDye());
	itemTypes.push_back(CactusGreen());
	itemTypes.push_back(LightBlueDye());
	itemTypes.push_back(PurpleDye());
	itemTypes.push_back(MagnetaDye());

	itemTypes.push_back(PumpkinSeeds());
	itemTypes.push_back(FlintAndSteel());

	itemTypes.push_back(Potato());
	itemTypes.push_back(BakedPotato());
	itemTypes.push_back(PoisonedPotato());
	itemTypes.push_back(Carrot());

	itemTypes.push_back(Disk4());
	itemTypes.push_back(Disk5());
	itemTypes.push_back(Disk6());
	itemTypes.push_back(Disk7());
	itemTypes.push_back(Disk8());
	itemTypes.push_back(Disk9());
	itemTypes.push_back(Disk10());
	itemTypes.push_back(Disk11());
	itemTypes.push_back(Disk12());

	itemTypes.push_back(RawMutton());
	itemTypes.push_back(CookedMutton());
	
	itemTypes.push_back(YellowGlassBlockPannelItem());
	itemTypes.push_back(GreenGlassBlockPannelItem());
	itemTypes.push_back(PinkGlassBlockPannelItem());
	itemTypes.push_back(PurpleGlassBlockPannelItem());
	itemTypes.push_back(RedGlassBlockPannelItem());
	itemTypes.push_back(BlackGlassBlockPannelItem());
	itemTypes.push_back(BlueGlassBlockPannelItem());

	itemTypes.push_back(RawPorkchop());
	itemTypes.push_back(CookedPorkchop());

	itemTypes.push_back(CowSpawnEgg());
	itemTypes.push_back(SheepSpawnEgg());
	itemTypes.push_back(ZombieSpawnEgg());
	itemTypes.push_back(CreeperSpawnEgg());
	itemTypes.push_back(PigSpawnEgg());

	// id 408. Must stay last: itemTypes is indexed by `id - 250`, so the push order is the id order.
	itemTypes.push_back(RedstoneTorchItem());

	// id 409.
	itemTypes.push_back(RepeaterItem());

	// id 410. Appended rather than slotted in beside the other spawn eggs: 408 and 409 are taken by
	// the torch and repeater, and moving those would change item ids that existing saves store.
	itemTypes.push_back(ChickenSpawnEgg());

	// ids 411-414. The chicken's derived items. Appended for the same reason as the spawn egg above:
	// the vector index is (id - 250), so inserting anywhere but the end would renumber every later
	// item and change ids that existing saves already store.
	itemTypes.push_back(Feather());       // id 411.
	itemTypes.push_back(RawChicken());    // id 412.
	itemTypes.push_back(CookedChicken()); // id 413.
	itemTypes.push_back(Egg());           // id 414.

	// ids 415-417. The spider and its derived items, appended for the same reason again.
	itemTypes.push_back(String());        // id 415.
	itemTypes.push_back(SpiderEye());     // id 416.
	itemTypes.push_back(SpiderSpawnEgg()); // id 417.
itemTypes.push_back(Arrow());          // id 418.
itemTypes.push_back(Bow());            // id 419.
itemTypes.push_back(SkeletonSpawnEgg()); // id 420.

// id 421. The debug stick, appended for the same reason as everything above: the vector index is
// (id - 250), so inserting anywhere but the end would renumber every later item.
itemTypes.push_back(DebugStick());

// ids 422-424. The bow's three draw frames, appended so the bow can be drawn mid-draw: an item's
// atlas cell is baked into its mesh here, so each frame needs an entry of its own. Nothing can obtain
// them; see the classes in Items.h.
itemTypes.push_back(BowPulling0());
itemTypes.push_back(BowPulling1());
itemTypes.push_back(BowPulling2());
}
