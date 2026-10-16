#include "CraftWorld2.h"

bool CraftWorld::BlockIsWater(block_t b)
{
    return b == WaterBlock::getID() || (b >= WaterBlock1::getID() && b <= WaterBlock4::getID()) ||
           (b >= WaterBlock5::getID() && b <= WaterBlock7::getID());
}

// Java 1.7 BlockLiquid: anything that does not block movement is replaced, except
// doors, ladders, sugar cane (and signs/portals, which this game lacks). Trapdoors
// are wood there, so they block flow; item frames are entities there, keep them.
bool CraftWorld::FluidWashesAway(block_t b)
{
    if(b == 0 || b >= blockTypes.size())
        return false;
    if(b >= 254 && b < kExtendedBlockBase)
        return false;
    if(BlockIsWater(b) || BlockIsLava(b))
        return false;
    if(blockTypes[b].solid)
        return false;
    if(b >= Door1::getID() && b <= Door10::getID())
        return false;
    if(b >= Ladder1::getID() && b <= Ladder4::getID())
        return false;
    if(b == CaneBlock::getID())
        return false;
    if(b >= TrapdoorOpen1::getID() && b <= TrapdoorClosed4::getID())
        return false;
    if(b >= ItemFrame1::getID() && b <= ItemFrame4::getID())
        return false;
    return true;
}

bool CraftWorld::BlockIsLava(block_t b)
{
    return b == Lava::getID() || b == LavaBlock1::getID();
}

bool CraftWorld::BlockIsIgnitable(block_t b)
{
    if(b == 0 || b >= blockTypes.size() || b == Fire::getID())
        return false;
    if(b == TNTBlock::getID())
        return true;
    if(b == LeavesBlock::getID() || b == BirchLeaves::getID() || b == SpruceLeaves::getID())
        return true;
    const BaseBlock& t = blockTypes[b];
    if(t.soundType == 3) // wood (logs, planks, fence, wooden slabs/stairs)
        return true;
    if(t.soundType == 6) // wool
        return true;
    return false;
}

// A water cell a real waterfall lands on spreads like a source (Java resets the flow distance on a
// fall). Only a genuine fall counts: the column above must be 2+ tall and this cell must be resting
// on a block. A one block step keeps its level, otherwise water creeps across bumpy ground forever.
int CraftWorld::WaterEffectiveLevel(int x, int y, int z, block_t b)
{
    if(y + 2 >= WORLD_HEIGHT)
        return FluidSpreadLevel(b);
    if(!BlockIsWater(GetBlockNoCheck(x, y + 1, z)) || !BlockIsWater(GetBlockNoCheck(x, y + 2, z)))
        return FluidSpreadLevel(b);
    if(y > 0 && BlockIsWater(GetBlockNoCheck(x, y - 1, z)))
        return FluidSpreadLevel(b); // inside the falling column, not resting on anything
    return 0;
}

Vector3 CraftWorld::WaterFlowVector(int x, int y, int z)
{
    Vector3 v(0.0f, 0.0f, 0.0f);
    if(x < 0 || y < 0 || z < 0 || x >= worldLimit || y >= WORLD_HEIGHT || z >= worldLimit)
        return v;
    block_t here = GetBlockNoCheck(x, y, z);
    if(!BlockIsWater(here))
        return v;
    int my = WaterEffectiveLevel(x, y, z, here);
    const int dx[4] = {1, -1, 0, 0};
    const int dz[4] = {0, 0, 1, -1};
    for(int d = 0; d < 4; d++)
    {
        int nx = x + dx[d];
        int nz = z + dz[d];
        if(nx < 0 || nz < 0 || nx >= worldLimit || nz >= worldLimit)
            continue;
        block_t n = GetBlockNoCheck(nx, y, nz);
        int nl = -1;
        if(BlockIsWater(n))
            nl = WaterEffectiveLevel(nx, y, nz, n);
        else if(n == 0 || (n < blockTypes.size() && !blockTypes[n].solid))
        {
            // Open side over lower water pulls toward the drop, one full step (8) further down
            if(y > 0 && BlockIsWater(GetBlockNoCheck(nx, y - 1, nz)))
                nl = WaterEffectiveLevel(nx, y - 1, nz, GetBlockNoCheck(nx, y - 1, nz)) + 8;
        }
        if(nl < 0)
            continue;
        float diff = (float)(nl - my);
        v.x += dx[d] * diff;
        v.z += dz[d] * diff;
    }
    float len = sqrtf(v.x * v.x + v.z * v.z);
    if(len > 0.0001f)
    {
        v.x /= len;
        v.z /= len;
    }
    else
        v = Vector3(0.0f, 0.0f, 0.0f);
    return v;
}

int CraftWorld::FluidSpreadLevel(block_t b)
{
    if(b == WaterBlock::getID() || b == Lava::getID())
        return 0;
    if(b == WaterBlock1::getID() || b == LavaBlock1::getID())
        return 1;
    if(b == WaterBlock2::getID())
        return 2;
    if(b == WaterBlock3::getID())
        return 3;
    if(b == WaterBlock4::getID())
        return 4;
    if(b == WaterBlock5::getID())
        return 5;
    if(b == WaterBlock6::getID())
        return 6;
    if(b == WaterBlock7::getID())
        return 7;
    return -1;
}

block_t CraftWorld::FluidBlockFromLevel(bool lava, int level)
{
    if(lava)
    {
        if(level <= 0)
            return Lava::getID();
        return LavaBlock1::getID();
    }
    if(level <= 0)
        return WaterBlock::getID();
    if(level == 1)
        return WaterBlock1::getID();
    if(level == 2)
        return WaterBlock2::getID();
    if(level == 3)
        return WaterBlock3::getID();
    if(level == 4)
        return WaterBlock4::getID();
    if(level == 5)
        return WaterBlock5::getID();
    if(level == 6)
        return WaterBlock6::getID();
    return WaterBlock7::getID();
}

int CraftWorld::LavaDistanceToSource(int x, int y, int z)
{
    int best = 99;
    for(int dy = -1; dy <= 1; dy++)
    {
        for(int dx = -3; dx <= 3; dx++)
        {
            for(int dz = -3; dz <= 3; dz++)
            {
                int d = abs(dx) + abs(dy) + abs(dz);
                if(d > 3 || d >= best)
                    continue;
                int nx = x + dx;
                int ny = y + dy;
                int nz = z + dz;
                if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
                    continue;
                if(GetBlockNoCheck(nx, ny, nz) == Lava::getID())
                    best = d;
            }
        }
    }
    return best;
}

int CraftWorld::FluidCornerHeightPx(int cx, int cy, int cz, bool lava)
{
    int n = 0;
    float empty = 0.0f;
    for(int j = 0; j < 4; j++)
    {
        int bx = cx - (j & 1);
        int bz = cz - ((j >> 1) & 1);
        if(bx < 0 || bz < 0 || cy < 0 || bx >= worldLimit || bz >= worldLimit || cy >= WORLD_HEIGHT)
            continue;
        block_t b = GetBlockNoCheck(bx, cy, bz);
        // The next chunk is still empty. Counting it as air pulls this corner down and, together with
        // the side face below, draws a wall through the middle of a lake. Skip it; the shore is drawn
        // when that chunk's surface exists.
        ChunkColumn* ncol = ColumnForBlock(bx, bz);
        if(!ncol || (!ncol->generated && !ncol->surfaceReady))
            continue;
        block_t above = 0;
        if(cy + 1 < WORLD_HEIGHT)
            above = GetBlockNoCheck(bx, cy + 1, bz);

        // A solid neighbour is neither fluid nor air, so it used to fall through every branch and
        // count for nothing. For lava that left the sea at 14/16, two pixels under the netherrack,
        // and the gap read as a slot through the wall. Lava still rises to meet the rock.
        //
        // Water does not. A source is 8/9 of a block (14 px), including against the bank, which is
        // the lip of sand above the water. Raising water to 16 made a lake the same height as the
        // grass beside it.
        if(b != 0 && b < (block_t)blockTypes.size() && blockTypes[b].solid)
        {
            if(lava)
                return 16;
            continue;
        }

        bool same = lava ? BlockIsLava(b) : BlockIsWater(b);
        bool sameUp = lava ? BlockIsLava(above) : (BlockIsWater(above) || above == IceBlock::getID());
        if(same && sameUp)
            return 16;
        if(same)
        {
            int lvl = FluidSpreadLevel(b);
            if(lava && b == LavaBlock1::getID())
            {
                // Overworld lava decays 2 per block (levels 2, 4, 6), same scale as water's 0..7
                int dist = LavaDistanceToSource(bx, cy, bz);
                if(dist < 1)
                    dist = 1;
                if(dist > 3)
                    dist = 3;
                lvl = dist * 2;
            }
            float p = (float)(lvl + 1) / 9.0f;
            if(lvl == 0)
            {
                empty += p * 10.0f;
                n += 10;
            }
            else
            {
                empty += p;
                n += 1;
            }
        }
        else if(b == 0 || (b < blockTypes.size() && blockTypes[b].solid == false && BlockIsWater(b) == false && BlockIsLava(b) == false))
        {
            empty += 1.0f;
            n += 1;
        }
    }
    if(n <= 0)
        return 1;
    float h = 1.0f - empty / (float)n;
    int px = (int)(h * 16.0f + 0.5f);
    if(px < 1)
        px = 1;
    if(px > 16)
        px = 16;
    return px;
}

void CraftWorld::BuildFluidBlock(int x, int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent)
{
    BaseBlock *blockType = &blockTypes[Block];
    bool lava = BlockIsLava(Block);
    bool sameAbove = false;
    if(y + 1 < WORLD_HEIGHT)
    {
        block_t up = GetBlock(x, y + 1, z);
        if(lava)
            sameAbove = BlockIsLava(up);
        else
            sameAbove = BlockIsWater(up) || up == IceBlock::getID();
    }

    int h00, h10, h11, h01;
    if(sameAbove)
    {
        h00 = 16;
        h10 = 16;
        h11 = 16;
        h01 = 16;
    }
    else
    {
        h00 = FluidCornerHeightPx(x, y, z, lava);
        h10 = FluidCornerHeightPx(x + 1, y, z, lava);
        h11 = FluidCornerHeightPx(x + 1, y, z + 1, lava);
        h01 = FluidCornerHeightPx(x, y, z + 1, lava);
    }

    auto sameAt = [this, lava](int nx, int ny, int nz) -> bool
    {
        if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
            return false;
        // A chunk that has not reached its surface yet is not air. A side built against it stays in
        // the lake as an inner wall. Treat it as the same fluid until its blocks exist; the rebuild
        // when it arrives adds the shore.
        ChunkColumn* ncol = ColumnForBlock(nx, nz);
        if(!ncol || (!ncol->generated && !ncol->surfaceReady))
            return true;
        block_t n = GetBlock(nx, ny, nz);
        if(lava)
            return BlockIsLava(n);
        return BlockIsWater(n) || n == IceBlock::getID();
    };

    auto drawSide = [&](int nx, int ny, int nz) -> bool
    {
        if(sameAt(nx, ny, nz))
            return false;
        // 1-block drop: air here, same fluid below. Skip the wall so the next
        // surface shows instead of a shaded face between flow cells.
        if(nx >= 0 && ny > 0 && nz >= 0 && nx < worldLimit && ny < WORLD_HEIGHT && nz < worldLimit)
        {
            if(GetBlock(nx, ny, nz) == 0 && sameAt(nx, ny - 1, nz))
                return false;
        }
        return true;
    };

    // Uniform cell light on every face: no east/west dimming and no smooth-lighting
    // AO seams between adjacent flow cells (those only show from one glancing angle).
    if(drawSide(x - 1, y, z))
        BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 1, Vector3(0, 0, 16), Vector3(0, (float)h01, 16), Vector3(0, (float)h00, 0), Vector3(0, 0, 0), true, true);
    if(drawSide(x + 1, y, z))
        BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 2, Vector3(16, 0, 0), Vector3(16, (float)h10, 0), Vector3(16, (float)h11, 16), Vector3(16, 0, 16), true, true);
    if(sameAt(x, y - 1, z) == false)
        BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 3, Vector3(0, 0, 0), Vector3(16, 0, 0), Vector3(16, 0, 16), Vector3(0, 0, 16), true, true);
    if(sameAbove == false)
        BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 4, Vector3(0, (float)h01, 16), Vector3(16, (float)h11, 16), Vector3(16, (float)h10, 0), Vector3(0, (float)h00, 0), true, true);
    if(drawSide(x, y, z - 1))
        BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 5, Vector3(0, (float)h00, 0), Vector3(16, (float)h10, 0), Vector3(16, 0, 0), Vector3(0, 0, 0), true, true);
    if(drawSide(x, y, z + 1))
        BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 6, Vector3(0, 0, 16), Vector3(16, 0, 16), Vector3(16, (float)h11, 16), Vector3(0, (float)h01, 16), true, true);
}

void CraftWorld::UpdateBlockPhysics(int id)
{
    if(!MeshIdValid(id))
        return;
    UpdateBlockPhysicsBox(mChunks[id]->chunkStartX, mChunks[id]->chunkStartY, mChunks[id]->chunkStartZ);
}

void CraftWorld::UpdateBlockPhysicsBox(int StartX, int StartY, int StartZ)
{
    std::vector<int> flowX;
    std::vector<int> flowY;
    std::vector<int> flowZ;
    std::vector<block_t> flowId;

    auto fluidPriority = [this](block_t id) -> int
    {
        if(id == 0)
            return 0;
        if(id == Obsidian::getID())
            return 100;
        if(id == CobbleStone::getID())
            return 90;
        int lvl = FluidSpreadLevel(id);
        if(lvl < 0)
            return 50;
        return 80 - lvl;
    };

    auto queueFluid = [&](int tx, int ty, int tz, block_t put)
    {
        if(tx < 0 || ty < 0 || tz < 0 || tx >= worldLimit || ty >= WORLD_HEIGHT || tz >= worldLimit)
            return;
        for(unsigned int i = 0; i < flowX.size(); i++)
        {
            if(flowX[i] == tx && flowY[i] == ty && flowZ[i] == tz)
            {
                if(fluidPriority(put) > fluidPriority(flowId[i]))
                    flowId[i] = put;
                return;
            }
        }
        if(flowX.size() >= 48)
            return;
        flowX.push_back(tx);
        flowY.push_back(ty);
        flowZ.push_back(tz);
        flowId.push_back(put);
    };

    auto mixResult = [this](block_t src, block_t dest) -> block_t
    {
        if(BlockIsLava(src) && BlockIsWater(dest))
            return (src == Lava::getID()) ? Obsidian::getID() : CobbleStone::getID();
        if(BlockIsWater(src) && BlockIsLava(dest))
            return (dest == Lava::getID()) ? Obsidian::getID() : CobbleStone::getID();
        return 0;
    };

    int x0 = StartX - 4;
    int y0 = StartY - 4;
    int z0 = StartZ - 4;
    int x1 = StartX + CHUNK_SIZE + 4;
    int y1 = StartY + CHUNK_SIZE + 4;
    int z1 = StartZ + CHUNK_SIZE + 4;
    if(x0 < 0) x0 = 0;
    if(y0 < 0) y0 = 0;
    if(z0 < 0) z0 = 0;
    if(x1 > worldLimit) x1 = worldLimit;
    if(y1 > WORLD_HEIGHT) y1 = WORLD_HEIGHT;
    if(z1 > worldLimit) z1 = worldLimit;
    // Water with a real fall above it spreads like a source: Java spreads it as level 0 when it
    // lands, so a waterfall still reaches 7 blocks across the floor.
    auto waterSpreadLevel = [this](int wx, int wy, int wz, block_t b) -> int
    {
        return WaterEffectiveLevel(wx, wy, wz, b);
    };


    if(physicsBoxesThisFrame > 0)
        return;
    physicsBoxesThisFrame++;

    bool gridWasActive = meshGridActive;
    if(!gridWasActive)
        BeginColumnGrid(StartX, StartZ);

    for (int z = z0; z < z1; ++z)
    {
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
				block_t here = GetBlockNoCheck(x,y,z);
				if(here == 0)
					continue;
				bool lava = BlockIsLava(here);
				bool water = BlockIsWater(here);
				if(lava || water)
				{
					int level = FluidSpreadLevel(here);
					int spreadLevel = water ? waterSpreadLevel(x, y, z, here) : level;
					int maxSpread = lava ? 3 : 7;
					int dx[4] = {1, -1, 0, 0};
					int dz[4] = {0, 0, 1, -1};
					bool fell = false;
					if(y > 0)
					{
						block_t below = GetBlockNoCheck(x, y - 1, z);
						if(below == 0 || FluidWashesAway(below))
						{
							block_t downId = lava ? LavaBlock1::getID() : FluidBlockFromLevel(false, level < 1 ? 1 : level);
							queueFluid(x, y - 1, z, downId);
							fell = true;
						}
						else
						{
							block_t mixed = mixResult(here, below);
							if(mixed != 0)
							{
								queueFluid(x, y - 1, z, mixed);
								fell = true;
							}
						}
					}
					// Water inside a falling column (water above and below) never spreads sideways
					bool inColumn = water && y > 0 && y + 1 < WORLD_HEIGHT &&
						BlockIsWater(GetBlockNoCheck(x, y + 1, z)) &&
						BlockIsWater(GetBlockNoCheck(x, y - 1, z));
					if(fell == false && inColumn == false && (lava || spreadLevel < maxSpread))
					{
						block_t nextId;
						if(lava)
							nextId = LavaBlock1::getID();
						else
							nextId = FluidBlockFromLevel(false, spreadLevel + 1);
						for(int d = 0; d < 4; d++)
						{
							int tx = x + dx[d];
							int tz = z + dz[d];
							if(tx < 0 || tz < 0 || tx >= worldLimit || tz >= worldLimit)
								continue;
							block_t dest = GetBlockNoCheck(tx, y, tz);
							block_t mixed = mixResult(here, dest);
							if(mixed != 0)
							{
								queueFluid(tx, y, tz, mixed);
								continue;
							}
							bool open = dest == 0 || FluidWashesAway(dest);
							if(lava)
							{
								if(open && LavaDistanceToSource(tx, y, tz) <= 3)
									queueFluid(tx, y, tz, nextId);
							}
							else
							{
								if(open || (BlockIsWater(dest) && waterSpreadLevel(tx, y, tz, dest) > spreadLevel + 1))
									queueFluid(tx, y, tz, nextId);
							}
						}
					}
					if(level > 0)
					{
						bool supported = false;
						if(y + 1 < WORLD_HEIGHT)
						{
							block_t above = GetBlockNoCheck(x, y + 1, z);
							if((lava && BlockIsLava(above)) || (water && BlockIsWater(above)))
								supported = true;
						}
						if(supported == false && y > 0 && GetBlockNoCheck(x, y - 1, z) == 0)
							supported = true;
						if(supported == false)
						{
							if(lava)
							{
								if(LavaDistanceToSource(x, y, z) <= 3)
									supported = true;
							}
							else
							{
								for(int d = 0; d < 4; d++)
								{
									int tx = x + dx[d];
									int tz = z + dz[d];
									if(tx < 0 || tz < 0 || tx >= worldLimit || tz >= worldLimit)
										continue;
									block_t n = GetBlockNoCheck(tx, y, tz);
									if(BlockIsWater(n) && waterSpreadLevel(tx, y, tz, n) < level)
									{
										supported = true;
										break;
									}
								}
							}
						}
						if(supported == false)
							queueFluid(x, y, z, 0);
					}
				}
                 if(here == GravitySandBlock::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = GravitySandBlock ::getID();
					}
                }
                //falling gravel
                if(here == Gravel::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = Gravel::getID();
					}
                }
				//falling white_concrete_powder
                if(here == white_concrete_powder::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = white_concrete_powder::getID();
					}
                }
                /*/falling orange_concrete_powder
                if(GetBlock(x,y,z) == orange_concrete_powder::getID())
                {
					if(GetBlock(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = orange_concrete_powder::getID();
					}
                }
				//falling magenta_concrete_powder
                if(GetBlock(x,y,z) == magenta_concrete_powder::getID())
                {
					if(GetBlock(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = magenta_concrete_powder::getID();
					}
                }
                //falling light_blue_concrete_powder
                if(GetBlock(x,y,z) == light_blue_concrete_powder::getID())
                {
					if(GetBlock(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = light_blue_concrete_powder::getID();
					}
                }*/
				//falling yellow_concrete_powder
                if(here == yellow_concrete_powder::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = yellow_concrete_powder::getID();
					}
                }
				/*/falling lime_concrete_powder
                if(GetBlock(x,y,z) == lime_concrete_powder::getID())
                {
					if(GetBlock(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = lime_concrete_powder::getID();
					}
                }*/
				//falling pink_concrete_powder
                if(here == pink_concrete_powder::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = pink_concrete_powder::getID();
					}
                }
				//falling gray_concrete_powder
                if(here == gray_concrete_powder::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = gray_concrete_powder::getID();
					}
                }
				/*/falling light_gray_concrete_powder
                if(GetBlock(x,y,z) == light_gray_concrete_powder::getID())
                {
					if(GetBlock(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = light_gray_concrete_powder::getID();
					}
                }*/
				/*/falling cyan_concrete_powder
                if(GetBlock(x,y,z) == cyan_concrete_powder::getID())
                {
					if(GetBlock(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = cyan_concrete_powder::getID();
					}
                }*/
				//falling purple_concrete_powder
                if(here == purple_concrete_powder::getID())
                {	
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = purple_concrete_powder::getID();
					}
                }
				//falling blue_concrete_powder
                if(here == blue_concrete_powder::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = blue_concrete_powder::getID();
					}
                }
				/*/falling brown_concrete_powder
                if(GetBlock(x,y,z) == brown_concrete_powder::getID())
                {
					if(GetBlock(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = brown_concrete_powder::getID();
					}
                }*/
				//falling green_concrete_powder
                if(here == green_concrete_powder::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = green_concrete_powder::getID();
					}
                }
				//falling red_concrete_powder
                if(here == red_concrete_powder::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = red_concrete_powder::getID();
					}
                }
				//falling black_concrete_powder
                if(here == black_concrete_powder::getID())
                {
					if(GetBlockNoCheck(x,y-1,z) == 0)
                    {
						GetBlock(x,y,z) = 0;
						GetBlock(x,y-1,z) = black_concrete_powder::getID();
					}
                }
				//fire: stay in this cell, hug/burn ignitable neighbors, do not teleport into them
				if (here == Fire::getID())
				{
					auto extinguish = [&]()
					{
						GetBlock(x, y, z) = 0;
						RemoveLigtSourceAtPosition(x, y, z, Fire::getID());
						AddChunkToUpdate(x, y, z);
						QueueLightAreaIn(Vector3((float)x, (float)y, (float)z));
					};

					const int nOff[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
					bool wet = false;
					bool supported = BlockSolid(x, y - 1, z);
					bool touchingIgnitable = false;
					for(int d = 0; d < 6; d++)
					{
						int nx = x + nOff[d][0];
						int ny = y + nOff[d][1];
						int nz = z + nOff[d][2];
						block_t nb = _GetBlock(nx, ny, nz);
						if(BlockIsWater(nb))
							wet = true;
						if(BlockIsIgnitable(nb))
						{
							touchingIgnitable = true;
							supported = true;
						}
					}

					if(wet || supported == false)
					{
						extinguish();
					}
					else
					{
						int roll = rand() % 20;
						if(roll == 0 && touchingIgnitable)
						{
							int d = rand() % 6;
							int nx = x + nOff[d][0];
							int ny = y + nOff[d][1];
							int nz = z + nOff[d][2];
							if(ny >= 0 && ny < WORLD_HEIGHT)
							{
								block_t nb = _GetBlock(nx, ny, nz);
								if(BlockIsIgnitable(nb))
								{
									GetBlock(nx, ny, nz) = Fire::getID();
									SetLigtSourcePosition(nx, ny, nz, Fire::getID());
									AddChunkToUpdate(nx, ny, nz);
									QueueLightAreaIn(Vector3((float)nx, (float)ny, (float)nz));
								}
								else if(nb == 0)
								{
									bool nextToFuel = false;
									for(int e = 0; e < 6; e++)
									{
										if(BlockIsIgnitable(_GetBlock(nx + nOff[e][0], ny + nOff[e][1], nz + nOff[e][2])))
										{
											nextToFuel = true;
											break;
										}
									}
									if(nextToFuel)
									{
										GetBlock(nx, ny, nz) = Fire::getID();
										SetLigtSourcePosition(nx, ny, nz, Fire::getID());
										AddChunkToUpdate(nx, ny, nz);
										QueueLightAreaIn(Vector3((float)nx, (float)ny, (float)nz));
									}
								}
							}
						}
						else if(roll == 1 && touchingIgnitable == false)
						{
							extinguish();
						}
					}
				}
            }
        }
    }

    if(flowX.empty() == false)
    {
        std::vector<int> fluidChunks;
        std::vector<int> fluidChunkX;
        std::vector<int> fluidChunkY;
        std::vector<int> fluidChunkZ;
        for(unsigned int i = 0; i < flowX.size(); i++)
        {
            block_t oldId = GetBlock(flowX[i], flowY[i], flowZ[i]);
            block_t newId = flowId[i];
            if(oldId == newId)
                continue;
            bool oldLava = BlockIsLava(oldId);
            bool newLava = BlockIsLava(newId);
            bool washed = oldId != 0 && FluidWashesAway(oldId) && (BlockIsWater(newId) || newLava);
            Vector3 cellPos((float)flowX[i], (float)flowY[i], (float)flowZ[i]);
            if(washed)
            {
                if(LightSourceBlock(oldId))
                {
                    RemoveLigtSourceAtPosition(flowX[i], flowY[i], flowZ[i], oldId);
                    QueueLightAreaIn(cellPos);
                }
                // Water pops the block off as an item; lava burns it
                if(newLava == false)
                    DropThis(LootBlock(oldId), cellPos);
            }
            if(oldLava && newLava == false)
                RemoveLigtSourceAtPosition(flowX[i], flowY[i], flowZ[i], oldId);
            GetBlock(flowX[i], flowY[i], flowZ[i]) = newId;
            if(newLava && oldLava == false)
                SetLigtSourcePosition(flowX[i], flowY[i], flowZ[i], newId);
            if(washed && oldId == Redstoneoff::getID())
                UpdateRedstonePower(flowX[i], flowY[i], flowZ[i]);
            int cid = getChunkId(Vector3((float)flowX[i], (float)flowY[i], (float)flowZ[i]));
            if(cid < 0)
                continue;
            bool seen = false;
            for(unsigned int c = 0; c < fluidChunks.size(); c++)
            {
                if(fluidChunks[c] == cid)
                {
                    seen = true;
                    break;
                }
            }
            if(seen == false)
            {
                fluidChunks.push_back(cid);
                fluidChunkX.push_back(flowX[i]);
                fluidChunkY.push_back(flowY[i]);
                fluidChunkZ.push_back(flowZ[i]);
            }
        }
        for(unsigned int c = 0; c < fluidChunks.size(); c++)
        {
            AddChunkToUpdate(fluidChunks[c]);
            rebuildNearestChunks(fluidChunks[c], Vector3((float)fluidChunkX[c], (float)fluidChunkY[c], (float)fluidChunkZ[c]));
        }
    }
    if(!gridWasActive)
        EndMeshCaches();
}

