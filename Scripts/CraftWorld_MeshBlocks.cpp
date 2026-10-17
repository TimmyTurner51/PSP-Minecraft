#include "CraftWorld2.h"

void CraftWorld::GetSpecialBlock(int x,int y, int z,int &iVertex,SimpleMeshChunk* MeshChunk,block_t Block,bool transparent)
{
    BaseBlock *blockType = &blockTypes[Block];
    float pixel, texturePixel;

    pixel = (float)1/16.0f;
    texturePixel = (float)1/256.0f;

    bool canCreate = false;

    float up = 0.0f;
    float down = 0.0f;
    float left = 0.0f;
    float right = 0.0f;

    block_t Block1;

    Vector3 light1, light2, light3, light4;
    Vector3 BlockColorx1, BlockColorx2, BlockColory1, BlockColory2, BlockColorz1, BlockColorz2;
    if(blockType->blockModel == 5)
    {
        if(BlockIsWater(Block) || BlockIsLava(Block))
        {
            BuildFluidBlock(x,y,z,iVertex,MeshChunk,Block,transparent);
            return;
        }

        /// LEAVES
        if(Block == 9 || Block == 37 || Block == 38)
        {
			if (mainOptions.fastRendering == false)
			{
				BuildWorldBlockPlaneNoCheck(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 1, Vector3(0, 0, 16), Vector3(0, 16, 16), Vector3(0, 16, 0), Vector3(0, 0, 0));
				BuildWorldBlockPlaneNoCheck(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 2, Vector3(16, 0, 0), Vector3(16, 16, 0), Vector3(16, 16, 16), Vector3(16, 0, 16));

				BuildWorldBlockPlaneNoCheck(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 3, Vector3(0, 0, 0), Vector3(16, 0, 0), Vector3(16, 0, 16), Vector3(0, 0, 16));
				BuildWorldBlockPlaneNoCheck(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 4, Vector3(0, 16, 16), Vector3(16, 16, 16), Vector3(16, 16, 0), Vector3(0, 16, 0));

				BuildWorldBlockPlaneNoCheck(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 5, Vector3(0, 16, 0), Vector3(16, 16, 0), Vector3(16, 0, 0), Vector3(0, 0, 0));
				BuildWorldBlockPlaneNoCheck(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 6, Vector3(0, 0, 16), Vector3(16, 0, 16), Vector3(16, 16, 16), Vector3(0, 16, 16));
			}
			if (mainOptions.fastRendering == true)
			{
				BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 1, Vector3(0, 0, 16), Vector3(0, 16, 16), Vector3(0, 16, 0), Vector3(0, 0, 0));
				BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 2, Vector3(16, 0, 0), Vector3(16, 16, 0), Vector3(16, 16, 16), Vector3(16, 0, 16));

				BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 3, Vector3(0, 0, 0), Vector3(16, 0, 0), Vector3(16, 0, 16), Vector3(0, 0, 16));
				BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 4, Vector3(0, 16, 16), Vector3(16, 16, 16), Vector3(16, 16, 0), Vector3(0, 16, 0));

				BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 5, Vector3(0, 16, 0), Vector3(16, 16, 0), Vector3(16, 0, 0), Vector3(0, 0, 0));
				BuildWorldBlockPlane(blockType, x, y, z, iVertex, MeshChunk, Block, transparent, 6, Vector3(0, 0, 16), Vector3(16, 0, 16), Vector3(16, 16, 16), Vector3(0, 16, 16));
			}	
        }


        /// SNOW LAYER
        if(Block == Snow2::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,2,16),Vector3(0,2,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,2,0),Vector3(16,2,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,2,16),Vector3(16,2,16),Vector3(16,2,0),Vector3(0,2,0),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,2,0),Vector3(16,2,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,2,16),Vector3(0,2,16));

            return;
        }


        /// CACTI
        if(Block == 25)
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(1,0,16),Vector3(1,16,16),Vector3(1,16,0),Vector3(1,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(15,0,0),Vector3(15,16,0),Vector3(15,16,16),Vector3(15,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(1,0,1),Vector3(15,0,1),Vector3(15,0,15),Vector3(1,0,15));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(1,16,15),Vector3(15,16,15),Vector3(15,16,1),Vector3(1,16,1));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,1),Vector3(16,16,1),Vector3(16,0,1),Vector3(0,0,1));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,15),Vector3(16,0,15),Vector3(16,16,15),Vector3(0,16,15));
            return;
        }



        /// CARPETS
        if(Block >= BlackWoolCarpet::getID() && Block <= WhiteWoolCarpet::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,1,16),Vector3(0,1,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,1,0),Vector3(16,1,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,1,16),Vector3(16,1,16),Vector3(16,1,0),Vector3(0,1,0),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,1,0),Vector3(16,1,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,1,16),Vector3(0,1,16));

            return;
        }

        if(Block == ItemFrame1::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(1,2,2),Vector3(1,14,2),Vector3(1,14,14),Vector3(1,2,14),true,false,3);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(0.5f,2,2),Vector3(0.5f,14,2),Vector3(0.5f,14,14),Vector3(0.5f,2,14),true,false,2);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,2,2),Vector3(1,2,2),Vector3(1,2,14),Vector3(0,2,14),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,13,3),Vector3(1,13,3),Vector3(1,13,13),Vector3(0,13,13),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,14,14),Vector3(1,14,14),Vector3(1,14,2),Vector3(0,14,2),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,3,13),Vector3(1,3,13),Vector3(1,3,3),Vector3(0,3,3),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,14,2),Vector3(1,14,2),Vector3(1,2,2),Vector3(0,2,2),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,13,13),Vector3(1,13,13),Vector3(1,3,13),Vector3(0,3,13),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,2,14),Vector3(1,2,14),Vector3(1,14,14),Vector3(0,14,14),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,3,3),Vector3(1,3,3),Vector3(1,13,3),Vector3(0,13,3),true,false,0);

            return;
        }

        if(Block == ItemFrame2::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(15,2,14),Vector3(15,14,14),Vector3(15,14,2),Vector3(15,2,2),true,false,3);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(15.5f,2,14),Vector3(15.5f,14,14),Vector3(15.5f,14,2),Vector3(15.5f,2,2),true,false,2);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(15,2,2),Vector3(16,2,2),Vector3(15,2,14),Vector3(16,2,14),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(15,13,3),Vector3(16,13,3),Vector3(16,13,13),Vector3(15,13,13),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(15,14,14),Vector3(16,14,14),Vector3(16,14,2),Vector3(15,14,2),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(15,3,13),Vector3(16,3,13),Vector3(16,3,3),Vector3(15,3,3),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(15,14,2),Vector3(16,14,2),Vector3(16,2,2),Vector3(15,2,2),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(15,13,13),Vector3(16,13,13),Vector3(16,3,13),Vector3(15,3,13),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(15,2,14),Vector3(16,2,14),Vector3(16,14,14),Vector3(15,14,14),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(15,3,3),Vector3(16,3,3),Vector3(16,13,3),Vector3(15,13,3),true,false,0);

            return;
        }

        if(Block == ItemFrame3::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(2,2,1),Vector3(2,14,1),Vector3(2,14,0),Vector3(2,2,0),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(13,3,1),Vector3(13,13,1),Vector3(13,13,0),Vector3(13,3,0),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(14,2,0),Vector3(14,14,0),Vector3(14,14,1),Vector3(14,2,1),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(3,3,0),Vector3(3,13,0),Vector3(3,13,1),Vector3(3,3,1),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(2,2,0),Vector3(14,2,0),Vector3(14,2,1),Vector3(2,2,1),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(3,13,0),Vector3(13,13,0),Vector3(13,13,1),Vector3(3,13,1),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(2,14,1),Vector3(14,14,1),Vector3(14,14,0),Vector3(2,14,0),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(3,3,1),Vector3(13,3,1),Vector3(13,3,0),Vector3(3,3,0),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(2,2,1),Vector3(14,2,1),Vector3(14,14,1),Vector3(2,14,1),true,false,3);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(2,2,0.5f),Vector3(14,2,0.5f),Vector3(14,14,0.5f),Vector3(2,14,0.5f),true,false,2);

            return;
        }

        if(Block == ItemFrame4::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(2,2,16),Vector3(2,14,16),Vector3(2,14,15),Vector3(2,2,15),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(13,3,16),Vector3(13,13,16),Vector3(13,13,15),Vector3(13,3,15),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(14,2,15),Vector3(14,14,15),Vector3(14,14,16),Vector3(14,2,16),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(3,3,15),Vector3(3,13,15),Vector3(3,13,16),Vector3(3,3,16),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(2,2,15),Vector3(14,2,15),Vector3(14,2,16),Vector3(2,2,16),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(3,13,15),Vector3(13,13,15),Vector3(13,13,16),Vector3(3,13,16),true,false,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(2,14,16),Vector3(14,14,16),Vector3(14,14,15),Vector3(2,14,15),true,false,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(3,3,16),Vector3(13,3,16),Vector3(13,3,15),Vector3(3,3,15),true,false,0);


            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(2,14,15),Vector3(14,14,15),Vector3(14,2,15),Vector3(2,2,15),true,false,3);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(2,14,15.5f),Vector3(14,14,15.5f),Vector3(14,2,15.5f),Vector3(2,2,15.5f),true,false,2);

            return;
        }

        if(Block >= TrapdoorClosed1::getID() && Block <= TrapdoorClosed4::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,3,16),Vector3(0,3,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(16,0,16));

            if(BlockTransparentOrSpecial(x,y-1,z))
            {
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
            }
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,3,16),Vector3(16,3,16),Vector3(16,3,0),Vector3(0,3,0));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,3,16),Vector3(0,3,16));
            return;
        }

        if(Block == TrapdoorOpen1::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,16,16),Vector3(0,16,0),Vector3(0,0,0));
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(3,0,0),Vector3(3,16,0),Vector3(3,16,16),Vector3(3,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(3,0,0),Vector3(3,0,16),Vector3(0,0,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,16,16),Vector3(3,16,16),Vector3(3,16,0),Vector3(0,16,0));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,0),Vector3(3,16,0),Vector3(3,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(3,0,16),Vector3(3,16,16),Vector3(0,16,16));
            return;
        }

        if(Block == TrapdoorOpen2::getID())
        {
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(13,0,16),Vector3(13,16,16),Vector3(13,16,0),Vector3(13,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,16,0),Vector3(16,16,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(13,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(13,0,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(13,16,16),Vector3(16,16,16),Vector3(16,16,0),Vector3(13,16,0));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(13,16,0),Vector3(16,16,0),Vector3(16,0,0),Vector3(13,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(13,0,16),Vector3(16,0,16),Vector3(16,16,16),Vector3(13,16,16));
            return;
        }

        if(Block == TrapdoorOpen3::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,3),Vector3(0,16,3),Vector3(0,16,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,16,0),Vector3(16,16,3),Vector3(16,0,3));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,3),Vector3(0,0,3));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,16,3),Vector3(16,16,3),Vector3(16,16,0),Vector3(0,16,0));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,0),Vector3(16,16,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,3),Vector3(16,0,3),Vector3(16,16,3),Vector3(0,16,3));
            return;
        }

        if(Block == TrapdoorOpen4::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,16,16),Vector3(0,16,13),Vector3(0,0,13));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,13),Vector3(16,16,13),Vector3(16,16,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,13),Vector3(16,0,13),Vector3(16,0,16),Vector3(0,0,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,16,16),Vector3(16,16,16),Vector3(16,16,13),Vector3(0,16,13));

            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,13),Vector3(16,16,13),Vector3(16,0,13),Vector3(0,0,13));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,16,16),Vector3(0,16,16));
            return;
        }

        if(Block == Ladder1::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(0.5f,0,0),Vector3(0.5f,16,0),Vector3(0.5f,16,16),Vector3(0.5f,0,16),1,1);
            return;
        }

        if(Block == Ladder2::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(15.5f,0,16),Vector3(15.5f,16,16),Vector3(15.5f,16,0),Vector3(15.5f,0,0),1,1);
            return;
        }

        if(Block == Ladder3::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,0.5f),Vector3(16,0,0.5f),Vector3(16,16,0.5f),Vector3(0,16,0.5f),1,1);
            return;
        }

        if(Block == Ladder4::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,15.5f),Vector3(16,16,15.5f),Vector3(16,0,15.5f),Vector3(0,0,15.5f),1,1);
            return;
        }

        if(Block == 112)
        {
            BuildFluidBlock(x,y,z,iVertex,MeshChunk,Block,transparent);
            return;
        }

        if(Block == MelonPlant4::getID())
        {
            bool haveMelon = false;
            if(GetBlock(x+1,y,z) == MelonBlock::getID())
            {
                blockType = &blockTypes[Block];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,8),Vector3(16,16,8),Vector3(16,0,8),Vector3(0,0,8),1,1);
                blockType = &blockTypes[MelonPlant4Reverse::getID()];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,MelonPlant4Reverse::getID(),transparent,6,Vector3(0,0,8),Vector3(16,0,8),Vector3(16,16,8),Vector3(0,16,8),1,1);
                haveMelon = true;
            }
            if(GetBlock(x-1,y,z) == MelonBlock::getID() && haveMelon == false)
            {
                blockType = &blockTypes[MelonPlant4Reverse::getID()];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,MelonPlant4Reverse::getID(),transparent,5,Vector3(0,16,8),Vector3(16,16,8),Vector3(16,0,8),Vector3(0,0,8),1,1);
                blockType = &blockTypes[Block];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,8),Vector3(16,0,8),Vector3(16,16,8),Vector3(0,16,8),1,1);
                haveMelon = true;
            }
            if(GetBlock(x,y,z+1) == MelonBlock::getID() && haveMelon == false)
            {
                blockType = &blockTypes[MelonPlant4Reverse::getID()];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,MelonPlant4Reverse::getID(),transparent,1,Vector3(8,0,16),Vector3(8,16,16),Vector3(8,16,0),Vector3(8,0,0),1,1);
                blockType = &blockTypes[Block];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,0,0),Vector3(8,16,0),Vector3(8,16,16),Vector3(8,0,16),1,1);
                haveMelon = true;
            }
            if(GetBlock(x,y,z-1) == MelonBlock::getID() && haveMelon == false)
            {
                blockType = &blockTypes[Block];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,0,16),Vector3(8,16,16),Vector3(8,16,0),Vector3(8,0,0),1,1);
                blockType = &blockTypes[MelonPlant4Reverse::getID()];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,MelonPlant4Reverse::getID(),transparent,2,Vector3(8,0,0),Vector3(8,16,0),Vector3(8,16,16),Vector3(8,0,16),1,1);
                haveMelon = true;
            }

            if(haveMelon == false)
            {
                GetSpecialBlock(x,y,z,iVertex,MeshChunk,MelonPlant3::getID(),true);
            }
            else
            {
                GetSpecialBlock(x,y,z,iVertex,MeshChunk,MelonPlant2::getID(),true);
            }
        }

        if(Block == PumpkinPlant4::getID())
        {
            bool havePumpkin = false;
            if(GetBlock(x+1,y,z) == Pumpkin1::getID())
            {
                blockType = &blockTypes[Block];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,8),Vector3(16,16,8),Vector3(16,0,8),Vector3(0,0,8),1,1);
                blockType = &blockTypes[PumpkinPlant4Reverse::getID()];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,PumpkinPlant4Reverse::getID(),transparent,6,Vector3(0,0,8),Vector3(16,0,8),Vector3(16,16,8),Vector3(0,16,8),1,1);
                havePumpkin = true;
            }
            if(GetBlock(x-1,y,z) == Pumpkin1::getID() && havePumpkin == false)
            {
                blockType = &blockTypes[PumpkinPlant4Reverse::getID()];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,PumpkinPlant4Reverse::getID(),transparent,5,Vector3(0,16,8),Vector3(16,16,8),Vector3(16,0,8),Vector3(0,0,8),1,1);
                blockType = &blockTypes[Block];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,8),Vector3(16,0,8),Vector3(16,16,8),Vector3(0,16,8),1,1);
                havePumpkin = true;
            }
            if(GetBlock(x,y,z+1) == Pumpkin1::getID() && havePumpkin == false)
            {
                blockType = &blockTypes[PumpkinPlant4Reverse::getID()];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,PumpkinPlant4Reverse::getID(),transparent,1,Vector3(8,0,16),Vector3(8,16,16),Vector3(8,16,0),Vector3(8,0,0),1,1);
                blockType = &blockTypes[Block];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,0,0),Vector3(8,16,0),Vector3(8,16,16),Vector3(8,0,16),1,1);
                havePumpkin = true;
            }
            if(GetBlock(x,y,z-1) == Pumpkin1::getID() && havePumpkin == false)
            {
                blockType = &blockTypes[Block];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,0,16),Vector3(8,16,16),Vector3(8,16,0),Vector3(8,0,0),1,1);
                blockType = &blockTypes[PumpkinPlant4Reverse::getID()];
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,PumpkinPlant4Reverse::getID(),transparent,2,Vector3(8,0,0),Vector3(8,16,0),Vector3(8,16,16),Vector3(8,0,16),1,1);
                havePumpkin = true;
            }

            if(havePumpkin == false)
            {
                GetSpecialBlock(x,y,z,iVertex,MeshChunk,PumpkinPlant3::getID(),true);
            }
            else
            {
                GetSpecialBlock(x,y,z,iVertex,MeshChunk,PumpkinPlant2::getID(),true);
            }
        }

        /// DOOR 1 Z--
        if  (
            Block == 49 ||
            (Block == 56 && GetBlock(x,y,z-1) != 52 && GetBlock(x,y,z-1) != 56 && (GetBlock(x,y,z+1) == 56 || GetBlock(x,y,z+1) == 52)) ||
            (Block == 56 && GetBlock(x,y,z-1) != 52 && GetBlock(x,y,z-1) != 56 && GetBlock(x,y,z+1) != 52 && GetBlock(x,y,z+1) != 56) ||
            (Block == 55 && GetBlock(x,y,z-1) != 51 && GetBlock(x,y,z-1) != 55 && (GetBlock(x,y,z+1) == 55 || GetBlock(x,y,z+1) == 51))
            )
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,3),Vector3(0,32,3),Vector3(0,32,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,32,0),Vector3(16,32,3),Vector3(16,0,3));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,3),Vector3(0,0,3));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,32,3),Vector3(16,32,3),Vector3(16,32,0),Vector3(0,32,0));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,32,0),Vector3(16,32,0),Vector3(16,0,0),Vector3(0,0,0),0,1);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,3),Vector3(16,0,3),Vector3(16,32,3),Vector3(0,32,3),1,1);
            return;
        }

        /// DOOR Z++
        if  (
            Block == 50 ||
            (Block == 56 && GetBlock(x,y,z+1) != 52 && GetBlock(x,y,z+1) != 56 && (GetBlock(x,y,z-1) == 56 || GetBlock(x,y,z-1) == 52)) ||
            (Block == 55 && GetBlock(x,y,z+1) != 51 && GetBlock(x,y,z+1) != 55 && (GetBlock(x,y,z-1) == 55 || GetBlock(x,y,z-1) == 51)) ||
            (Block == 55 && GetBlock(x,y,z+1) != 51 && GetBlock(x,y,z+1) != 55 && GetBlock(x,y,z-1) != 51 && GetBlock(x,y,z-1) != 55)
            )
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,32,16),Vector3(0,32,13),Vector3(0,0,13));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,13),Vector3(16,32,13),Vector3(16,32,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,13),Vector3(16,0,13),Vector3(16,0,16),Vector3(0,0,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,32,16),Vector3(16,32,16),Vector3(16,32,13),Vector3(0,32,13));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,32,13),Vector3(16,32,13),Vector3(16,0,13),Vector3(0,0,13),1,1);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,32,16),Vector3(0,32,16),0,1);
            return;
        }

        /// DOOR X--
        if  (
            Block == 51 ||
            (Block == 53 && GetBlock(x-1,y,z) != 49 && GetBlock(x-1,y,z) != 53 && (GetBlock(x+1,y,z) == 53 || GetBlock(x+1,y,z) == 49)) ||
            (Block == 53 && GetBlock(x-1,y,z) != 49 && GetBlock(x-1,y,z) != 53 && GetBlock(x+1,y,z) != 49 && GetBlock(x+1,y,z) != 53) ||
            (Block == 54 && GetBlock(x-1,y,z) != 50 && GetBlock(x-1,y,z) != 54 && (GetBlock(x+1,y,z) == 54 || GetBlock(x+1,y,z) == 50))
            )
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,32,16),Vector3(0,32,0),Vector3(0,0,0),0,1);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(3,0,0),Vector3(3,32,0),Vector3(3,32,16),Vector3(3,0,16),1,1);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(3,0,0),Vector3(3,0,16),Vector3(0,0,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,32,16),Vector3(3,32,16),Vector3(3,32,0),Vector3(0,32,0));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,32,0),Vector3(3,32,0),Vector3(3,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(3,0,16),Vector3(3,32,16),Vector3(0,32,16));
            return;
        }

        /// DOOR X++
        if  (
            Block == 52 ||
            (Block == 53 && GetBlock(x+1,y,z) != 49 && GetBlock(x+1,y,z) != 53 && (GetBlock(x-1,y,z) == 53 || GetBlock(x-1,y,z) == 49)) ||
            (Block == 54 && GetBlock(x+1,y,z) != 50 && GetBlock(x+1,y,z) != 54 && (GetBlock(x-1,y,z) == 54 || GetBlock(x-1,y,z) == 50)) ||
            (Block == 54 && GetBlock(x+1,y,z) != 50 && GetBlock(x+1,y,z) != 54 && GetBlock(x-1,y,z) != 50 && GetBlock(x-1,y,z) != 54)
            )
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(13,0,16),Vector3(13,32,16),Vector3(13,32,0),Vector3(13,0,0),1,1);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,32,0),Vector3(16,32,16),Vector3(16,0,16),0,1);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(13,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(13,0,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(13,32,16),Vector3(16,32,16),Vector3(16,32,0),Vector3(13,32,0));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(13,32,0),Vector3(16,32,0),Vector3(16,0,0),Vector3(13,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(13,0,16),Vector3(16,0,16),Vector3(16,32,16),Vector3(13,32,16));
            return;
        }

        if(Block == 55 || Block == 56)
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,3),Vector3(0,32,3),Vector3(0,32,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,32,0),Vector3(16,32,3),Vector3(16,0,3));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,3),Vector3(0,0,3));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,32,3),Vector3(16,32,3),Vector3(16,32,0),Vector3(0,32,0));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,32,0),Vector3(16,32,0),Vector3(16,0,0),Vector3(0,0,0),0,1);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,3),Vector3(16,0,3),Vector3(16,32,3),Vector3(0,32,3),1,1);
            return;
        }

        if(Block == 53 || Block == 54)
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,32,16),Vector3(0,32,0),Vector3(0,0,0),0,1);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(3,0,0),Vector3(3,32,0),Vector3(3,32,16),Vector3(3,0,16),1,1);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(3,0,0),Vector3(3,0,16),Vector3(0,0,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,32,16),Vector3(3,32,16),Vector3(3,32,0),Vector3(0,32,0));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,32,0),Vector3(3,32,0),Vector3(3,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(3,0,16),Vector3(3,32,16),Vector3(0,32,16));
            return;
        }

        /// CHEST
        if(Block == 94)
        {
            // chest
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(1,0,15),Vector3(1,14,15),Vector3(1,14,1),Vector3(1,0,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(15,0,1),Vector3(15,14,1),Vector3(15,14,15),Vector3(15,0,15),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(1,0,1),Vector3(15,0,1),Vector3(15,0,15),Vector3(1,0,15),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(1,14,15),Vector3(15,14,15),Vector3(15,14,1),Vector3(1,14,1),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(1,14,1),Vector3(15,14,1),Vector3(15,0,1),Vector3(1,0,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(1,0,15),Vector3(15,0,15),Vector3(15,14,15),Vector3(1,14,15),1,0);

            // chest lock
            blockType = &blockTypes[98];
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,1,Vector3(0,7,9),Vector3(0,11,9),Vector3(0,11,7),Vector3(0,7,7),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,3,Vector3(0,7,7),Vector3(1,7,7),Vector3(1,7,9),Vector3(0,7,9),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,4,Vector3(0,11,9),Vector3(1,11,9),Vector3(1,11,7),Vector3(0,11,7),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,5,Vector3(0,11,7),Vector3(1,11,7),Vector3(1,7,7),Vector3(0,7,7),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,6,Vector3(0,7,9),Vector3(1,7,9),Vector3(1,11,9),Vector3(0,11,9),1,0);
            return;
        }

        if(Block == 95)
        {
            // chest
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(1,0,15),Vector3(1,14,15),Vector3(1,14,1),Vector3(1,0,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(15,0,1),Vector3(15,14,1),Vector3(15,14,15),Vector3(15,0,15),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(1,0,1),Vector3(15,0,1),Vector3(15,0,15),Vector3(1,0,15),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(1,14,15),Vector3(15,14,15),Vector3(15,14,1),Vector3(1,14,1),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(1,14,1),Vector3(15,14,1),Vector3(15,0,1),Vector3(1,0,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(1,0,15),Vector3(15,0,15),Vector3(15,14,15),Vector3(1,14,15),1,0);

            // chest lock
            blockType = &blockTypes[98];
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,2,Vector3(16,7,7),Vector3(16,11,7),Vector3(16,11,9),Vector3(16,7,9),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,3,Vector3(15,7,7),Vector3(16,7,7),Vector3(16,7,9),Vector3(15,7,9),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,4,Vector3(15,11,9),Vector3(16,11,9),Vector3(16,11,7),Vector3(15,11,7),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,5,Vector3(15,11,7),Vector3(16,11,7),Vector3(16,7,7),Vector3(15,7,7),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,6,Vector3(15,7,9),Vector3(16,7,9),Vector3(16,11,9),Vector3(15,11,9),1,0);
            return;
        }

        if(Block == 96)
        {
            // chest
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(1,0,15),Vector3(1,14,15),Vector3(1,14,1),Vector3(1,0,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(15,0,1),Vector3(15,14,1),Vector3(15,14,15),Vector3(15,0,15),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(1,0,1),Vector3(15,0,1),Vector3(15,0,15),Vector3(1,0,15),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(1,14,15),Vector3(15,14,15),Vector3(15,14,1),Vector3(1,14,1),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(1,14,1),Vector3(15,14,1),Vector3(15,0,1),Vector3(1,0,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(1,0,15),Vector3(15,0,15),Vector3(15,14,15),Vector3(1,14,15),1,0);

            // chest lock
            blockType = &blockTypes[98];
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,1,Vector3(7,7,1),Vector3(7,11,1),Vector3(7,11,0),Vector3(7,7,0),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,2,Vector3(9,7,0),Vector3(9,11,0),Vector3(9,11,1),Vector3(9,7,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,3,Vector3(7,7,0),Vector3(9,7,0),Vector3(9,7,1),Vector3(7,7,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,4,Vector3(7,11,1),Vector3(9,11,1),Vector3(9,11,0),Vector3(7,11,0),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,5,Vector3(7,11,0),Vector3(9,11,0),Vector3(9,7,0),Vector3(7,7,0),1,0);
            return;
        }

        if(Block == 97)
        {
            // chest
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(1,0,15),Vector3(1,14,15),Vector3(1,14,1),Vector3(1,0,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(15,0,1),Vector3(15,14,1),Vector3(15,14,15),Vector3(15,0,15),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(1,0,1),Vector3(15,0,1),Vector3(15,0,15),Vector3(1,0,15),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(1,14,15),Vector3(15,14,15),Vector3(15,14,1),Vector3(1,14,1),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(1,14,1),Vector3(15,14,1),Vector3(15,0,1),Vector3(1,0,1),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(1,0,15),Vector3(15,0,15),Vector3(15,14,15),Vector3(1,14,15),1,0);

            // chest lock
            blockType = &blockTypes[98];
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,1,Vector3(7,7,16),Vector3(7,11,16),Vector3(7,11,15),Vector3(7,7,15),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,2,Vector3(9,7,15),Vector3(9,11,15),Vector3(9,11,16),Vector3(9,7,16),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,3,Vector3(7,7,15),Vector3(9,7,15),Vector3(9,7,16),Vector3(7,7,16),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,4,Vector3(7,11,16),Vector3(9,11,16),Vector3(9,11,15),Vector3(7,11,15),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,66,transparent,6,Vector3(7,7,16),Vector3(9,7,16),Vector3(9,11,16),Vector3(7,11,16),1,0);
            return;
        }

        /// TORCH
        if(CraftWorld::IsTorchBlock(Block))
        {
            //texture stuff
            // A redstone torch shares this mesher with the plain torch but not its sprite layout: the
            // plain torch's artwork is a 4 texel wide slice (columns 6..10) and its window starts a row
            // higher (5), while the redstone torch's stick is 2 texels (columns 7..8) with alpha either
            // side. So a redstone torch gets its own column window and everything else keeps exactly what
            // it had. Left as a kind check rather than folded in, because the two windows are not two
            // points on one scale: the widths are 4 and 2, so there is no single number that interpolates
            // between them.
            //
            // The row is 6 for the redstone torch in *both* states even though the lit sprite carries an
            // extra row above its head -- measured, not assumed: the lit tile's artwork starts at row 5
            // and the unlit tile's at row 6, in this pack and four others, because the lit torch's sprite
            // includes its flame. Row 5 is the flame's red rim and Java never reads it (torch UVs start at
            // `getInterpolatedV(6)`), so reading it here is what showed a flat red square on top of a lit
            // torch where the head's bright centre should be. On the unlit sprite the same row is
            // transparent, which showed as a dead top pixel and a 2x1 cap.
            const bool rsTorch = CraftWorld::IsRedstoneTorchBlock(Block);
            float up = percent*blockType->leftPlane_y + texturePixel*(rsTorch ? 6.0f : 5.0f);
            float down = percent*blockType->leftPlane_y + percent;

            float left = percent * blockType->leftPlane_x + texturePixel*(rsTorch ? 7.0f : 6.0f);
            float right = left + texturePixel*(rsTorch ? 2.0f : 4.0f);

            pixel = 1.0f/16.0f;
            int torchFace = CraftWorld::TorchFaceFromBlock(Block);

            // A redstone torch gets its own shaft instead of the plain torch's four side quads. That mesh
            // puts all four faces on a 2 px footprint but draws each of them 4 px long, so every face pokes
            // a pixel out past the corners and the four cross each other. With the plain torch's 4 texel
            // wide sprite that reads as a chunky torch; the redstone torch's stick is only 2 texels wide and
            // sits inside a 2 px footprint, so the overlap shows the sprite twice in the middle of the
            // shaft. Java BlockTorch.setBlockBounds is 0.4375 .. 0.5625 across -- a 2 px square shaft -- so
            // the faces have to be exactly 2 px long. The `left`/`right` window above is the 2 texel column
            // range that fills them one texel per pixel, and `up`/`down` are what the plain torch uses.
            if(rsTorch && torchFace == 0)
            {
                const float lo = 0.4375f, hi = 0.5625f;   // 7/16 .. 9/16
                // Keep the plain torch's shaft height. Java's is 0.625, but the standing torch next to
                // this one is 11 px here, and a redstone torch a pixel shorter than a plain one would be
                // more noticeable than the 10% vertical stretch this costs.
                const float top = 0.6875f;                // 11/16
                const float capDn = up + texturePixel*2.0f;

                // -X
                MeshChunk->vert(x + lo, y       , z + hi, right, down,1,1,1);
                MeshChunk->vert(x + lo, y + top , z + hi, right, up  ,1,1,1);
                MeshChunk->vert(x + lo, y + top , z + lo, left , up  ,1,1,1);
                MeshChunk->vert(x + lo, y       , z + lo, left , down,1,1,1);
                MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
                iVertex += 4;

                // +X
                MeshChunk->vert(x + hi, y       , z + lo, right, down,1,1,1);
                MeshChunk->vert(x + hi, y + top , z + lo, right, up  ,1,1,1);
                MeshChunk->vert(x + hi, y + top , z + hi, left , up  ,1,1,1);
                MeshChunk->vert(x + hi, y       , z + hi, left , down,1,1,1);
                MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
                iVertex += 4;

                // -Z
                MeshChunk->vert(x + lo, y + top , z + lo, right, up  ,1,1,1);
                MeshChunk->vert(x + hi, y + top , z + lo, left , up  ,1,1,1);
                MeshChunk->vert(x + hi, y       , z + lo, left , down,1,1,1);
                MeshChunk->vert(x + lo, y       , z + lo, right, down,1,1,1);
                MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
                iVertex += 4;

                // +Z
                MeshChunk->vert(x + lo, y       , z + hi, left , down,1,1,1);
                MeshChunk->vert(x + hi, y       , z + hi, right, down,1,1,1);
                MeshChunk->vert(x + hi, y + top , z + hi, right, up  ,1,1,1);
                MeshChunk->vert(x + lo, y + top , z + hi, left , up  ,1,1,1);
                MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
                iVertex += 4;

                // +Y, flush with the top of the shaft. The plain torch puts its cap one pixel below the
                // top, which is what leaves a ring of side texture standing proud of it; flush does not.
                MeshChunk->vert(x + lo, y + top, z + hi, left , up   ,1,1,1);
                MeshChunk->vert(x + hi, y + top, z + hi, right, up   ,1,1,1);
                MeshChunk->vert(x + hi, y + top, z + lo, right, capDn,1,1,1);
                MeshChunk->vert(x + lo, y + top, z + lo, left , capDn,1,1,1);
                MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
                iVertex += 4;
            }
            else if(torchFace == 0) /////
            {
                //x-1
                MeshChunk->vert(x + 0.4375f,y			,z + 0.4375f + 0.0625*2+pixel	,right	, down,1,1,1);
                MeshChunk->vert(x + 0.4375f,y + 0.0625*11,z + 0.4375f + 0.0625*2+pixel	,right	, up	,1,1,1);
                MeshChunk->vert(x + 0.4375f,y + 0.0625*11,z + 0.4375f-pixel			,left	, up	,1,1,1);
                MeshChunk->vert(x + 0.4375f,y			,z + 0.4375f-pixel			,left	, down	,1,1,1);

                MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                iVertex += 4;

                //x+1
                MeshChunk->vert(x + 0.4375f + pixel*2,y		        ,z + 0.4375f-pixel            ,right, down,1,1,1);
                MeshChunk->vert(x + 0.4375f + pixel*2,y + pixel*11  ,z + 0.4375f-pixel            ,right, up,1,1,1);
                MeshChunk->vert(x + 0.4375f + pixel*2,y + pixel*11  ,z + 0.4375f + pixel*2+pixel  ,left, up,1,1,1);
                MeshChunk->vert(x + 0.4375f + pixel*2,y		        ,z + 0.4375f + pixel*2+pixel  ,left, down,1,1,1);

                MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                iVertex += 4;

                //z-1
                MeshChunk->vert(x + 0.4375f-pixel		        ,y + pixel*11   ,z + 0.4375f,right, up,1,1,1);
                MeshChunk->vert(x + 0.4375f + pixel*2+pixel   ,y + pixel*11   ,z + 0.4375f,left, up,1,1,1);
                MeshChunk->vert(x + 0.4375f + pixel*2+pixel   ,y			    ,z + 0.4375f,left, down,1,1,1);
                MeshChunk->vert(x + 0.4375f-pixel		        ,y			    ,z + 0.4375f,right, down,1,1,1);

                MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                iVertex += 4;

                //z+1
                MeshChunk->vert(x + 0.4375f-pixel		        ,y			    ,z + 0.4375f + pixel*2,left, down,1,1,1);
                MeshChunk->vert(x + 0.4375f + pixel*2+pixel   ,y			    ,z + 0.4375f + pixel*2,right, down,1,1,1);
                MeshChunk->vert(x + 0.4375f + pixel*2+pixel   ,y + pixel*11	    ,z + 0.4375f + pixel*2,right, up,1,1,1);
                MeshChunk->vert(x + 0.4375f-pixel		        ,y + pixel*11	,z + 0.4375f + pixel*2,left, up,1,1,1);

                MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                iVertex += 4;

                //y+1
                {
                    up = percent*blockType->leftPlane_y + texturePixel*6;
                    down = up + texturePixel*2;

                    left = percent * blockType->leftPlane_x + texturePixel*7;
                    right = left + texturePixel*2;

                    MeshChunk->vert(x + 0.4375f		        ,y + pixel*10   ,z + 0.4375f + pixel*2  ,left, up,1,1,1);
                    MeshChunk->vert(x + 0.4375f + pixel*2   ,y + pixel*10   ,z + 0.4375f + pixel*2  ,right, up,1,1,1);
                    MeshChunk->vert(x + 0.4375f + pixel*2   ,y + pixel*10   ,z + 0.4375f            ,right, down,1,1,1);
                    MeshChunk->vert(x + 0.4375f		        ,y + pixel*10   ,z + 0.4375f            ,left, down,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;
                }
            }
            else
            {

                bool canPlaseTorch = true;
                if(BlockSolid(x-1,y,z) && canPlaseTorch)
                {

                    //x-1
                    MeshChunk->vert(x - pixel   ,y + pixel*3    ,z + 0.4375f + pixel*2+pixel	,right	, down	,1,1,1);
                    MeshChunk->vert(x + pixel*4 ,y + pixel*14   ,z + 0.4375f + pixel*2+pixel	,right	, up	,1,1,1);
                    MeshChunk->vert(x + pixel*4 ,y + pixel*14   ,z + 0.4375f-pixel			,left	, up	,1,1,1);
                    MeshChunk->vert(x - pixel   ,y + pixel*3    ,z + 0.4375f-pixel			,left	, down	,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //x+1
                    MeshChunk->vert(x + pixel    ,y	+ pixel*3	,z + 0.4375f-pixel            ,right, down,1,1,1);
                    MeshChunk->vert(x + pixel*6  ,y + pixel*14  ,z + 0.4375f-pixel            ,right, up  ,1,1,1);
                    MeshChunk->vert(x + pixel*6  ,y + pixel*14  ,z + 0.4375f + pixel*2+pixel  ,left, up   ,1,1,1);
                    MeshChunk->vert(x + pixel    ,y + pixel*3   ,z + 0.4375f + pixel*2+pixel  ,left, down ,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //z-1
                    MeshChunk->vert(x + pixel*4-pixel   ,y + pixel*14   ,z + pixel*7,right, up  ,1,1,1);
                    MeshChunk->vert(x + pixel*6+pixel   ,y + pixel*14   ,z + pixel*7,left, up   ,1,1,1);
                    MeshChunk->vert(x + pixel+pixel     ,y + pixel*3	,z + pixel*7,left, down ,1,1,1);
                    MeshChunk->vert(x - pixel-pixel	    ,y + pixel*3	,z + pixel*7,right, down,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //z+1
                    MeshChunk->vert(x - pixel-pixel	        ,y + pixel*3    ,z + pixel*9,left, down,1,1,1);
                    MeshChunk->vert(x + pixel+pixel           ,y + pixel*3	,z + pixel*9,right, down,1,1,1);
                    MeshChunk->vert(x + pixel*6+pixel         ,y + pixel*14	,z + pixel*9,right, up,1,1,1);
                    MeshChunk->vert(x + pixel*4-pixel		    ,y + pixel*14	,z + pixel*9,left, up,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;


                    //y+1
                    up = percent*blockType->leftPlane_y + texturePixel*6;
                    down = up + texturePixel*2;

                    left = percent * blockType->leftPlane_x + texturePixel*7;
                    right = left + texturePixel*2;

                    MeshChunk->vert(x + pixel*4 - pixel/2.0f		    ,y + pixel*13   ,z + pixel*7 + pixel*2  ,left, up,1,1,1);
                    MeshChunk->vert(x + pixel*4 + pixel*2 - pixel/2.0f  ,y + pixel*13   ,z + pixel*7 + pixel*2  ,right, up,1,1,1);
                    MeshChunk->vert(x + pixel*4 + pixel*2 - pixel/2.0f  ,y + pixel*13   ,z + pixel*7            ,right, down,1,1,1);
                    MeshChunk->vert(x + pixel*4	- pixel/2.0f	        ,y + pixel*13   ,z + pixel*7            ,left, down,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;


                    //y-1
                    up = percent*blockType->leftPlane_y + texturePixel*14;
                    down = up + texturePixel*2;

                    left = percent * blockType->leftPlane_x + texturePixel*7;
                    right = left + texturePixel*2;

                    MeshChunk->vert(x - pixel   ,y + pixel*3   ,z + pixel*7            ,left, up,1,1,1);
                    MeshChunk->vert(x + pixel   ,y + pixel*3   ,z + pixel*7            ,right, up,1,1,1);
                    MeshChunk->vert(x + pixel   ,y + pixel*3   ,z + pixel*7 + pixel*2  ,right, down,1,1,1);
                    MeshChunk->vert(x - pixel	,y + pixel*3   ,z + pixel*7 + pixel*2  ,left, down,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;
                    canPlaseTorch = false;
                }
                if(torchFace == 2)
                if(BlockSolid(x+1,y,z) && canPlaseTorch)
                {
                    //x-1
                    MeshChunk->vert(x + pixel*15   ,y + pixel*3    ,z + 0.4375f + pixel*2+pixel	,right	, down	,1,1,1);
                    MeshChunk->vert(x + pixel*10 ,y + pixel*14   ,z + 0.4375f + pixel*2+pixel	,right	, up	,1,1,1);
                    MeshChunk->vert(x + pixel*10 ,y + pixel*14   ,z + 0.4375f-pixel			,left	, up	,1,1,1);
                    MeshChunk->vert(x + pixel*15  ,y + pixel*3    ,z + 0.4375f-pixel			,left	, down	,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //x+1
                    MeshChunk->vert(x + pixel*17    ,y	+ pixel*3	,z + 0.4375f-pixel            ,right, down,1,1,1);
                    MeshChunk->vert(x + pixel*12  ,y + pixel*14  ,z + 0.4375f-pixel            ,right, up  ,1,1,1);
                    MeshChunk->vert(x + pixel*12  ,y + pixel*14  ,z + 0.4375f + pixel*2+pixel  ,left, up   ,1,1,1);
                    MeshChunk->vert(x + pixel*17    ,y + pixel*3   ,z + 0.4375f + pixel*2+pixel  ,left, down ,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //z-1
                    MeshChunk->vert(x + pixel*12+pixel		,y + pixel*14  ,z + pixel*9,right, up  ,1,1,1);
                    MeshChunk->vert(x + pixel*10-pixel     ,y + pixel*14   ,z + pixel*9,left, up   ,1,1,1);
                    MeshChunk->vert(x + pixel*15-pixel       ,y + pixel*3	,z + pixel*9,left, down ,1,1,1);
                    MeshChunk->vert(x + pixel*17+pixel	    ,y + pixel*3	,z + pixel*9,right, down,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //z+1
                    MeshChunk->vert(x + pixel*17+pixel	        ,y + pixel*3    ,z + pixel*7,left, down,1,1,1);
                    MeshChunk->vert(x + pixel*15-pixel           ,y + pixel*3	,z + pixel*7,right, down,1,1,1);
                    MeshChunk->vert(x + pixel*10-pixel         ,y + pixel*14	,z + pixel*7,right, up,1,1,1);
                    MeshChunk->vert(x + pixel*12+pixel		    ,y + pixel*14	,z + pixel*7,left, up,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    {
                        up = percent*blockType->leftPlane_y + texturePixel*6;
                        down = up + texturePixel*2;

                        left = percent * blockType->leftPlane_x + texturePixel*7;
                        right = left + texturePixel*2;

                        MeshChunk->vert(x + pixel*10 + pixel/2.0f	,y + pixel*13   ,z + pixel*7 + pixel*2  ,left, up,1,1,1);
                        MeshChunk->vert(x + pixel*12 + pixel/2.0f   ,y + pixel*13   ,z + pixel*7 + pixel*2  ,right, up,1,1,1);
                        MeshChunk->vert(x + pixel*12 + pixel/2.0f   ,y + pixel*13   ,z + pixel*7            ,right, down,1,1,1);
                        MeshChunk->vert(x + pixel*10 + pixel/2.0f   ,y + pixel*13   ,z + pixel*7            ,left, down,1,1,1);

                        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                        iVertex += 4;
                    }

                    {
                        up = percent*blockType->leftPlane_y + texturePixel*14;
                        down = up + texturePixel*2;

                        left = percent * blockType->leftPlane_x + texturePixel*7;
                        right = left + texturePixel*2;

                        MeshChunk->vert(x + pixel * 15  ,y + pixel*3   ,z + pixel*7            ,left, up,1,1,1);
                        MeshChunk->vert(x + pixel * 17  ,y + pixel*3   ,z + pixel*7            ,right, up,1,1,1);
                        MeshChunk->vert(x + pixel * 17  ,y + pixel*3   ,z + pixel*7 + pixel*2  ,right, down,1,1,1);
                        MeshChunk->vert(x + pixel * 15 	,y + pixel*3   ,z + pixel*7 + pixel*2  ,left, down,1,1,1);

                        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                        iVertex += 4;
                    }
                }

                if(torchFace == 3)
                if(BlockSolid(x,y,z-1) && canPlaseTorch)
                {
                    //x-1
                    MeshChunk->vert(x + pixel*9,y + pixel*3		,z - pixel-pixel	    ,right	, down	,1,1,1);
                    MeshChunk->vert(x + pixel*9,y + pixel*14    ,z + pixel*4-pixel	,right	, up	,1,1,1);
                    MeshChunk->vert(x + pixel*9,y + pixel*14    ,z + pixel*6+pixel	,left	, up	,1,1,1);
                    MeshChunk->vert(x + pixel*9,y + pixel*3		,z + pixel+pixel	    ,left	, down	,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //x+1
                    MeshChunk->vert(x + pixel*7,y + pixel*3     ,z + pixel+pixel          ,right, down,1,1,1);
                    MeshChunk->vert(x + pixel*7,y + pixel*14    ,z + pixel*6+pixel           ,right, up,1,1,1);
                    MeshChunk->vert(x + pixel*7,y + pixel*14    ,z + pixel*4-pixel  ,left, up,1,1,1);
                    MeshChunk->vert(x + pixel*7,y + pixel*3	    ,z - pixel-pixel ,left, down,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //z-1
                    MeshChunk->vert(x + pixel*7	- pixel	        ,y + pixel*14   ,z + pixel*4,right, up,1,1,1);
                    MeshChunk->vert(x + pixel*7 + pixel*2 + pixel    ,y + pixel*14   ,z + pixel*4,left, up,1,1,1);
                    MeshChunk->vert(x + pixel*7 + pixel*2 + pixel   ,y + pixel*3    ,z - pixel,left, down,1,1,1);
                    MeshChunk->vert(x + pixel*7	- pixel	        ,y + pixel*3    ,z - pixel,right, down,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //z+1
                    MeshChunk->vert(x + pixel*7	-pixel	        ,y + pixel*3    ,z + pixel,left, down,1,1,1);
                    MeshChunk->vert(x + pixel*7 + pixel*2+pixel   ,y + pixel*3	,z + pixel,right, down,1,1,1);
                    MeshChunk->vert(x + pixel*7 + pixel*2+pixel  ,y + pixel*14	,z + pixel*6,right, up,1,1,1);
                    MeshChunk->vert(x + pixel*7	-pixel	        ,y + pixel*14	,z + pixel*6,left, up,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //y+1
                    {
                        up = percent*blockType->leftPlane_y + texturePixel*6;
                        down = up + texturePixel*2;

                        left = percent * blockType->leftPlane_x + texturePixel*7;
                        right = left + texturePixel*2;

                        MeshChunk->vert(x + pixel*7		        ,y + pixel*13   ,z + pixel*6 - pixel/2.0f  ,left, up,1,1,1);
                        MeshChunk->vert(x + pixel*7 + pixel*2   ,y + pixel*13   ,z + pixel*6 - pixel/2.0f ,right, up,1,1,1);
                        MeshChunk->vert(x + pixel*7 + pixel*2   ,y + pixel*13   ,z + pixel*4 - pixel/2.0f           ,right, down,1,1,1);
                        MeshChunk->vert(x + pixel*7		        ,y + pixel*13   ,z + pixel*4 - pixel/2.0f         ,left, down,1,1,1);

                        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                        iVertex += 4;
                    }

                    //y-1
                    {
                        up = percent*blockType->leftPlane_y + texturePixel*14;
                        down = up + texturePixel*2;

                        left = percent * blockType->leftPlane_x + texturePixel*7;
                        right = left + texturePixel*2;

                        MeshChunk->vert(x + pixel*7		        ,y + pixel*3   ,z + pixel  ,left, up,1,1,1);
                        MeshChunk->vert(x + pixel*7             ,y + pixel*3   ,z - pixel  ,right, up,1,1,1);
                        MeshChunk->vert(x + pixel*9             ,y + pixel*3   ,z - pixel  ,right, down,1,1,1);
                        MeshChunk->vert(x + pixel*9		        ,y + pixel*3   ,z + pixel  ,left, down,1,1,1);

                        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                        iVertex += 4;
                    }
                }

                if(torchFace == 4)
                if(BlockSolid(x,y,z+1) && canPlaseTorch)
                {

                    //x-1
                    MeshChunk->vert(x + pixel*9,y + pixel*3		,z + pixel*15-pixel	    ,right	, down	,1,1,1);
                    MeshChunk->vert(x + pixel*9,y + pixel*14    ,z + pixel*10-pixel	,right	, up	,1,1,1);
                    MeshChunk->vert(x + pixel*9,y + pixel*14    ,z + pixel*12+pixel	,left	, up	,1,1,1);
                    MeshChunk->vert(x + pixel*9,y + pixel*3		,z + pixel*17+pixel	    ,left	, down	,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //x+1
                    MeshChunk->vert(x + pixel*7,y + pixel*3     ,z + pixel*17+pixel         ,right, down,1,1,1);
                    MeshChunk->vert(x + pixel*7,y + pixel*14    ,z + pixel*12+pixel           ,right, up,1,1,1);
                    MeshChunk->vert(x + pixel*7,y + pixel*14    ,z + pixel*10-pixel  ,left, up,1,1,1);
                    MeshChunk->vert(x + pixel*7,y + pixel*3	    ,z + pixel*15-pixel ,left, down,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //z-1
                    MeshChunk->vert(x + pixel*7-pixel		        ,y + pixel*14   ,z + pixel*10,right, up,1,1,1);
                    MeshChunk->vert(x + pixel*7 + pixel*2+pixel  ,y + pixel*14   ,z + pixel*10,left, up,1,1,1);
                    MeshChunk->vert(x + pixel*7 + pixel*2+pixel   ,y + pixel*3    ,z + pixel*15,left, down,1,1,1);
                    MeshChunk->vert(x + pixel*7-pixel		        ,y + pixel*3    ,z + pixel*15,right, down,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //z+1
                    MeshChunk->vert(x + pixel*7-pixel		        ,y + pixel*3    ,z + pixel*17,left, down,1,1,1);
                    MeshChunk->vert(x + pixel*7 + pixel*2+pixel   ,y + pixel*3	,z + pixel*17,right, down,1,1,1);
                    MeshChunk->vert(x + pixel*7 + pixel*2+pixel   ,y + pixel*14	,z + pixel*12,right, up,1,1,1);
                    MeshChunk->vert(x + pixel*7-pixel		        ,y + pixel*14	,z + pixel*12,left, up,1,1,1);

                    MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                    MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                    iVertex += 4;

                    //y+1
                    {
                        up = percent*blockType->leftPlane_y + texturePixel*6;
                        down = up + texturePixel*2;

                        left = percent * blockType->leftPlane_x + texturePixel*7;
                        right = left + texturePixel*2;

                        MeshChunk->vert(x + pixel*7		        ,y + pixel*13   ,z + pixel*12 + pixel/2.0f ,left, up,1,1,1);
                        MeshChunk->vert(x + pixel*7 + pixel*2   ,y + pixel*13   ,z + pixel*12 + pixel/2.0f ,right, up,1,1,1);
                        MeshChunk->vert(x + pixel*7 + pixel*2   ,y + pixel*13   ,z + pixel*10 + pixel/2.0f ,right, down,1,1,1);
                        MeshChunk->vert(x + pixel*7		        ,y + pixel*13   ,z + pixel*10 + pixel/2.0f ,left, down,1,1,1);

                        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                        iVertex += 4;
                    }

                            //y+1
                    {
                        up = percent*blockType->leftPlane_y + texturePixel*14;
                        down = up + texturePixel*2;

                        left = percent * blockType->leftPlane_x + texturePixel*7;
                        right = left + texturePixel*2;

                        MeshChunk->vert(x + pixel*7		        ,y + pixel*3   ,z + pixel *17  ,left, up,1,1,1);
                        MeshChunk->vert(x + pixel*7             ,y + pixel*3   ,z + pixel *15 ,right, up,1,1,1);
                        MeshChunk->vert(x + pixel*9             ,y + pixel*3   ,z + pixel *15 ,right, down,1,1,1);
                        MeshChunk->vert(x + pixel*9		        ,y + pixel*3   ,z + pixel *17 ,left, down,1,1,1);

                        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
                        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

                        iVertex += 4;
                    }
                }
            }
            return;
        }
    }

    /// HALF BLOCKS
    if(blockType->blockModel == 1)
    {
        if(Block == BedBlock1::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,9,16),Vector3(0,9,0),Vector3(0,0,0));
            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,8,0),Vector3(16,8,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(0,3,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,9,16),Vector3(16,9,16),Vector3(16,9,0),Vector3(0,9,0),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,9,0),Vector3(16,9,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,9,16),Vector3(0,9,16));
            return;
        }
        if(Block == BedBlock5::getID())
        {
            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,9,16),Vector3(0,9,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,9,0),Vector3(16,9,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(0,3,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,9,16),Vector3(16,9,16),Vector3(16,9,0),Vector3(0,9,0),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,9,0),Vector3(16,9,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,9,16),Vector3(0,9,16));
            return;
        }

        if(Block == BedBlock2::getID())
        {
            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,9,16),Vector3(0,9,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,9,0),Vector3(16,9,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(0,3,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,9,16),Vector3(16,9,16),Vector3(16,9,0),Vector3(0,9,0),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,9,0),Vector3(16,9,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,9,16),Vector3(0,9,16));
            return;
        }
        if(Block == BedBlock6::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,9,16),Vector3(0,9,0),Vector3(0,0,0));
            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,9,0),Vector3(16,9,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(0,3,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,9,16),Vector3(16,9,16),Vector3(16,9,0),Vector3(0,9,0),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,9,0),Vector3(16,9,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,9,16),Vector3(0,9,16));
            return;
        }

        if(Block == BedBlock3::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,9,16),Vector3(0,9,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,9,0),Vector3(16,9,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(0,3,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,9,16),Vector3(16,9,16),Vector3(16,9,0),Vector3(0,9,0),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,9,0),Vector3(16,9,0),Vector3(16,0,0),Vector3(0,0,0));
            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,9,16),Vector3(0,9,16));
            return;
        }
        if(Block == BedBlock7::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,9,16),Vector3(0,9,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,9,0),Vector3(16,9,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(0,3,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,9,16),Vector3(16,9,16),Vector3(16,9,0),Vector3(0,9,0),1,0);

            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,9,0),Vector3(16,9,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,9,16),Vector3(0,9,16));
            return;
        }

        if(Block == BedBlock4::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,9,16),Vector3(0,9,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,9,0),Vector3(16,9,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(0,3,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,9,16),Vector3(16,9,16),Vector3(16,9,0),Vector3(0,9,0),1,0);

            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,9,0),Vector3(16,9,0),Vector3(16,0,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,9,16),Vector3(0,9,16));
            return;
        }
        if(Block == BedBlock8::getID())
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,9,16),Vector3(0,9,0),Vector3(0,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,9,0),Vector3(16,9,16),Vector3(16,0,16));

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,3,0),Vector3(16,3,0),Vector3(16,3,16),Vector3(0,3,16));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,9,16),Vector3(16,9,16),Vector3(16,9,0),Vector3(0,9,0),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,9,0),Vector3(16,9,0),Vector3(16,0,0),Vector3(0,0,0));
            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,9,16),Vector3(0,9,16));
            return;
        }

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,8,16),Vector3(0,8,0),Vector3(0,0,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,8,0),Vector3(16,8,16),Vector3(16,0,16));

        if(BlockTransparentOrSpecial(x,y-1,z))
        {
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));
        }
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,8,16),Vector3(16,8,16),Vector3(16,8,0),Vector3(0,8,0),1,0);

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,8,0),Vector3(16,8,0),Vector3(16,0,0),Vector3(0,0,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,8,16),Vector3(0,8,16));

        return;
    }


    /// FENCE
    if(blockType->blockModel == 2)
    {
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(6,0,10),Vector3(6,16,10),Vector3(6,16,6),Vector3(6,0,6),1,0);
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(10,0,6),Vector3(10,16,6),Vector3(10,16,10),Vector3(10,0,10),1,0);

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(6,0,6),Vector3(10,0,6),Vector3(10,0,10),Vector3(6,0,10));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(6,16,10),Vector3(10,16,10),Vector3(10,16,6),Vector3(6,16,6));

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(6,16,6),Vector3(10,16,6),Vector3(10,0,6),Vector3(6,0,6),1,0);
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(6,0,10),Vector3(10,0,10),Vector3(10,16,10),Vector3(6,16,10),1,0);

        if(BlockSolid(x,y,z-1) == true)
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(7,12,8),Vector3(7,15,8),Vector3(7,15,0),Vector3(7,12,0),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(7,6,8),Vector3(7,9,8),Vector3(7,9,0),Vector3(7,6,0),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(9,12,0),Vector3(9,15,0),Vector3(9,15,8),Vector3(9,12,8),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(9,6,0),Vector3(9,9,0),Vector3(9,9,8),Vector3(9,6,8),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,12,0),Vector3(9,12,0),Vector3(9,12,8),Vector3(7,12,8),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,6,0),Vector3(9,6,0),Vector3(9,6,8),Vector3(7,6,8),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,15,8),Vector3(9,15,8),Vector3(9,15,0),Vector3(7,15,0),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,9,8),Vector3(9,9,8),Vector3(9,9,0),Vector3(7,9,0),1,0);
        }

        if(BlockSolid(x,y,z+1) == true)
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(7,12,16),Vector3(7,15,16),Vector3(7,15,8),Vector3(7,12,8),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(7,6,16),Vector3(7,9,16),Vector3(7,9,8),Vector3(7,6,8),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(9,12,8),Vector3(9,15,8),Vector3(9,15,16),Vector3(9,12,16),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(9,6,8),Vector3(9,9,8),Vector3(9,9,16),Vector3(9,6,16),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,12,8),Vector3(9,12,8),Vector3(9,12,16),Vector3(7,12,16),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,6,8),Vector3(9,6,8),Vector3(9,6,16),Vector3(7,6,16),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,15,16),Vector3(9,15,16),Vector3(9,15,8),Vector3(7,15,8),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,9,16),Vector3(9,9,16),Vector3(9,9,8),Vector3(7,9,8),1,0);
        }

        if(BlockSolid(x-1,y,z) == true)
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,15,7),Vector3(8,15,7),Vector3(8,12,7),Vector3(0,12,7),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,9,7),Vector3(8,9,7),Vector3(8,6,7),Vector3(0,6,7),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,12,9),Vector3(8,12,9),Vector3(8,15,9),Vector3(0,15,9),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,6,9),Vector3(8,6,9),Vector3(8,9,9),Vector3(0,9,9),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,12,7),Vector3(8,12,7),Vector3(8,12,9),Vector3(0,12,9),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,6,7),Vector3(8,6,7),Vector3(8,6,9),Vector3(0,6,9),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,15,9),Vector3(8,15,9),Vector3(8,15,7),Vector3(0,15,7),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,9,9),Vector3(8,9,9),Vector3(8,9,7),Vector3(0,9,7),1,0);
        }

        if(BlockSolid(x+1,y,z) == true)
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(8,15,7),Vector3(16,15,7),Vector3(16,12,7),Vector3(8,12,7),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(8,9,7),Vector3(16,9,7),Vector3(16,6,7),Vector3(8,6,7),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(8,12,9),Vector3(16,12,9),Vector3(16,15,9),Vector3(8,15,9),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(8,6,9),Vector3(16,6,9),Vector3(16,9,9),Vector3(8,9,9),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(8,12,7),Vector3(16,12,7),Vector3(16,12,9),Vector3(8,12,9),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(8,6,7),Vector3(16,6,7),Vector3(16,6,9),Vector3(8,6,9),1,0);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(8,15,9),Vector3(16,15,9),Vector3(16,15,7),Vector3(8,15,7),1,0);
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(8,9,9),Vector3(16,9,9),Vector3(16,9,7),Vector3(8,9,7),1,0);
        }
        return;
    }

    /// FLOWERS, CANES & MOOSHROOMS
    if(blockType->blockModel == 3)
    {
        BlockColorx1 = Vector3(BlockFinalLight(x,y,z),BlockFinalLight(x,y,z),BlockFinalLight(x,y,z));
        BlockColorx2 = BlockColorx1*0.9f;

        up = blockType->leftPlane_y*percent;
        down = up+percent;

        left = blockType->leftPlane_x*percent;
        right = left+percent;

        canCreate = true;

        if (canCreate)
        {
            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

            light1 = light2 = light3 = light4 = BlockColorx1;

            MeshChunk->vert(x, y, z+1, right, down, light1.x,light1.y,light1.z);
            MeshChunk->vert(x, y+1, z+1, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+1, y+1, z, left, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+1, y, z, left, down, light3.x,light3.y,light3.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        //x+1
        canCreate = true;

        if (canCreate)
        {
            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

            light1 = light2 = light3 = light4 = BlockColorx2;

            MeshChunk->vert(x, y, z, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x, y+1, z, right, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+1, y+1, z+1, left, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+1, y, z+1, left, down, light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        canCreate = true;

        if (canCreate)
        {
            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

            light1 = light2 = light3 = light4 = BlockColorx2;

            MeshChunk->vert(x+1, y, z+1, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+1, y+1, z+1, right, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x, y+1, z, left, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x, y, z, left, down, light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        canCreate = true;

        if (canCreate)
        {
            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

            light1 = light2 = light3 = light4 = BlockColorx1;

            MeshChunk->vert(x+1, y, z, right, down, light1.x,light1.y,light1.z);
            MeshChunk->vert(x+1, y+1, z, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x, y+1, z+1, left, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x, y, z+1, left, down, light3.x,light3.y,light3.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        return;
    }

    /// WHEAT
    if(blockType->blockModel == 4)
    {
        //x-1
        canCreate = true;

        BlockColorx2 = Vector3(BlockFinalLight(x,y,z),BlockFinalLight(x,y-0.1f,z),BlockFinalLight(x,y,z));
        BlockColorz2 = BlockColorx2;

        up = blockType->leftPlane_y*percent;
        down = up+percent;

        left = blockType->leftPlane_x*percent;
        right = left+percent;

        if (canCreate)
        {
            light1 = light2 = light3 = light4 = BlockColorx2;

            MeshChunk->vert(x+pixel*4, y, z+1, right, down, light1.x,light1.y,light1.z);
            MeshChunk->vert(x+pixel*4, y+1, z+1, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+pixel*4, y+1, z, left, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+pixel*4, y, z, left, down, light3.x,light3.y,light3.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        if (canCreate)
        {
            light1 = light2 = light3 = light4 = BlockColorx2;

            MeshChunk->vert(x+pixel*4, y, z, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+pixel*4, y+1, z, right, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+pixel*4, y+1, z+1, left, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+pixel*4, y, z+1, left, down, light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        if (canCreate)
        {
            light1 = light2 = light3 = light4 = BlockColorx2;

            MeshChunk->vert(x+pixel*12, y, z, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+pixel*12, y+1, z, right, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+pixel*12, y+1, z+1, left, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+pixel*12, y, z+1, left, down, light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        if (canCreate)
        {
            light1 = light2 = light3 = light4 = BlockColorx2;

            MeshChunk->vert(x+pixel*12, y, z+1, right, down, light1.x,light1.y,light1.z);
            MeshChunk->vert(x+pixel*12, y+1, z+1, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+pixel*12, y+1, z, left, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+pixel*12, y, z, left, down, light3.x,light3.y,light3.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        if (canCreate)
        {
            light1 = light2 = light3 = light4 = BlockColorz2;

            MeshChunk->vert(x, y+1, z+pixel*4, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+1, y+1, z+pixel*4, left, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+1, y, z+pixel*4, left, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x, y, z+pixel*4, right, down, light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        if (canCreate == true)
        {
            light1 = light2 = light3 = light4 = BlockColorz2;

            MeshChunk->vert(x, y, z+pixel*4, left, down, light1.x,light1.y,light1.z);
            MeshChunk->vert(x+1, y, z+pixel*4, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+1, y+1, z+pixel*4, right, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x, y+1, z+pixel*4, left, up, light2.x,light2.y,light2.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        if (canCreate == true)
        {
            light1 = light2 = light3 = light4 = BlockColorz2;

            MeshChunk->vert(x, y, z+pixel*12, left, down, light1.x,light1.y,light1.z);
            MeshChunk->vert(x+1, y, z+pixel*12, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+1, y+1, z+pixel*12, right, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x, y+1, z+pixel*12, left, up, light2.x,light2.y,light2.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }

        if (canCreate)
        {
            light1 = light2 = light3 = light4 = BlockColorz2;

            MeshChunk->vert(x, y+1, z+pixel*12, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+1, y+1, z+pixel*12, left, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+1, y, z+pixel*12, left, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x, y, z+pixel*12, right, down, light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
        return;
    }

    /// STAIRS
    if(blockType->blockModel == 6)
    {
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,8,16),Vector3(0,8,0),Vector3(0,0,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,8,16),Vector3(0,16,16),Vector3(0,16,8),Vector3(0,8,8));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,8,0),Vector3(16,8,16),Vector3(16,0,16));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,8,8),Vector3(16,16,8),Vector3(16,16,16),Vector3(16,8,16));

        if(BlockTransparentOrSpecial(x,y-1,z) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,8,16),Vector3(16,8,16),Vector3(16,8,0),Vector3(0,8,0),1,0);
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,16,16),Vector3(16,16,16),Vector3(16,16,8),Vector3(0,16,8));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,8,0),Vector3(16,8,0),Vector3(16,0,0),Vector3(0,0,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,8),Vector3(16,16,8),Vector3(16,8,8),Vector3(0,8,8),1,0);

        if(BlockTransparentOrSpecial(x,y,z+1) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,16,16),Vector3(0,16,16));

        return;
    }

    if(blockType->blockModel == 7)
    {
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,8,16),Vector3(0,8,0),Vector3(0,0,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,8,8),Vector3(0,16,8),Vector3(0,16,0),Vector3(0,8,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,8,0),Vector3(16,8,16),Vector3(16,0,16));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,8,0),Vector3(16,16,0),Vector3(16,16,8),Vector3(16,8,8));

        if(BlockTransparentOrSpecial(x,y-1,z) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,8,16),Vector3(16,8,16),Vector3(16,8,0),Vector3(0,8,0),1,0);
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,16,8),Vector3(16,16,8),Vector3(16,16,0),Vector3(0,16,0));

        if(BlockTransparentOrSpecial(x,y,z-1) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,0),Vector3(16,16,0),Vector3(16,0,0),Vector3(0,0,0));

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,8,16),Vector3(0,8,16));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,8,8),Vector3(16,8,8),Vector3(16,16,8),Vector3(0,16,8),1,0);

        return;
    }

    if(blockType->blockModel == 8)
    {
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,8,16),Vector3(0,8,0),Vector3(0,0,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,8,16),Vector3(8,16,16),Vector3(8,16,0),Vector3(8,8,0),1,0);

        if(BlockTransparentOrSpecial(x+1,y,z) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,16,0),Vector3(16,16,16),Vector3(16,0,16));


        if(BlockTransparentOrSpecial(x,y-1,z) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,8,16),Vector3(16,8,16),Vector3(16,8,0),Vector3(0,8,0),1,0);
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(8,16,16),Vector3(16,16,16),Vector3(16,16,0),Vector3(8,16,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,8,0),Vector3(16,8,0),Vector3(16,0,0),Vector3(0,0,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(8,16,0),Vector3(16,16,0),Vector3(16,8,0),Vector3(8,8,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,8,16),Vector3(0,8,16));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(8,8,16),Vector3(16,8,16),Vector3(16,16,16),Vector3(8,16,16));

        return;
    }

    if(blockType->blockModel == 9)
    {
        if(BlockTransparentOrSpecial(x-1,y,z) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,16,16),Vector3(0,16,0),Vector3(0,0,0));

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,8,0),Vector3(16,8,16),Vector3(16,0,16));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,8,0),Vector3(8,16,0),Vector3(8,16,16),Vector3(8,8,16),1,0);

        if(BlockTransparentOrSpecial(x,y-1,z) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,8,16),Vector3(16,8,16),Vector3(16,8,0),Vector3(0,8,0),1,0);
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,16,16),Vector3(8,16,16),Vector3(8,16,0),Vector3(0,16,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,8,0),Vector3(16,8,0),Vector3(16,0,0),Vector3(0,0,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,0),Vector3(8,16,0),Vector3(8,8,0),Vector3(0,8,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,8,16),Vector3(0,8,16));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,8,16),Vector3(8,8,16),Vector3(8,16,16),Vector3(0,16,16));

        return;
    }

    /// PANELS
    if(blockType->blockModel == 10)
    {
        bool connectLeft, connectRight, connectFront, connectBack;
        connectLeft = (BlockSolid(x-1,y,z) && (!BlockTransparentOrSpecial(x-1,y,z) || GetBlock(x-1,y,z) == Block));
        connectRight = (BlockSolid(x+1,y,z) && (!BlockTransparentOrSpecial(x+1,y,z) || GetBlock(x+1,y,z) == Block));
        connectBack = (BlockSolid(x,y,z-1) && (!BlockTransparentOrSpecial(x,y,z-1) || GetBlock(x,y,z-1) == Block));
        connectFront = (BlockSolid(x,y,z+1) && (!BlockTransparentOrSpecial(x,y,z+1) || GetBlock(x,y,z+1) == Block));
        int connectsNum = 0;

        if(!connectFront && !connectBack && !connectLeft && !connectRight)
        {
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,0,16),Vector3(8,16,16),Vector3(8,16,0),Vector3(8,0,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,0,0),Vector3(8,16,0),Vector3(8,16,16),Vector3(8,0,16),true,false,0);

            if(BlockSolid(x,y-1,z) == false)
            {
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,7),Vector3(16,0,7),Vector3(16,0,9),Vector3(0,0,9));
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,0,0),Vector3(9,0,0),Vector3(9,0,16),Vector3(7,0,16));

                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,0,16),Vector3(9,0,16),Vector3(9,0,0),Vector3(7,0,0),1,0);
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,0,9),Vector3(16,0,9),Vector3(16,0,7),Vector3(0,0,7),1,0);
            }
            if(BlockSolid(x,y+1,z) == false)
            {
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,16,16),Vector3(9,16,16),Vector3(9,16,0),Vector3(7,16,0),1,0);
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,16,9),Vector3(16,16,9),Vector3(16,16,7),Vector3(0,16,7),1,0);

                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,16,7),Vector3(16,16,7),Vector3(16,16,9),Vector3(0,16,9));
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,16,0),Vector3(9,16,0),Vector3(9,16,16),Vector3(7,16,16));
            }

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,8),Vector3(16,16,8),Vector3(16,0,8),Vector3(0,0,8),true,false,5);
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,8),Vector3(16,0,8),Vector3(16,16,8),Vector3(0,16,8));
        }
        else
        {
            if(connectLeft)
            {
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,16,8),Vector3(8,16,8),Vector3(8,0,8),Vector3(0,0,8));
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,8),Vector3(8,0,8),Vector3(8,16,8),Vector3(0,16,8));

                if(BlockSolid(x,y-1,z) == false)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,7),Vector3(8,0,7),Vector3(8,0,9),Vector3(0,0,9));
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,0,9),Vector3(8,0,9),Vector3(8,0,7),Vector3(0,0,7),1,0);
                }
                if(BlockSolid(x,y+1,z) == false)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,16,9),Vector3(8,16,9),Vector3(8,16,7),Vector3(0,16,7),1,0);
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,16,7),Vector3(8,16,7),Vector3(8,16,9),Vector3(0,16,9));
                }
                connectsNum ++;
            }
            if(connectRight)
            {
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(8,16,8),Vector3(16,16,8),Vector3(16,0,8),Vector3(8,0,8));
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(8,0,8),Vector3(16,0,8),Vector3(16,16,8),Vector3(8,16,8));

                if(BlockSolid(x,y-1,z) == false)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(8,0,7),Vector3(16,0,7),Vector3(16,0,9),Vector3(8,0,9));
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(8,0,9),Vector3(16,0,9),Vector3(16,0,7),Vector3(8,0,7),1,0);
                }
                if(BlockSolid(x,y+1,z) == false)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(8,16,9),Vector3(16,16,9),Vector3(16,16,7),Vector3(8,16,7),1,0);
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(8,16,7),Vector3(16,16,7),Vector3(16,16,9),Vector3(8,16,9));
                }
                connectsNum ++;
            }
            if(connectBack)
            {
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,0,8),Vector3(8,16,8),Vector3(8,16,0),Vector3(8,0,0));
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,0,0),Vector3(8,16,0),Vector3(8,16,8),Vector3(8,0,8));

                if(BlockSolid(x,y-1,z) == false)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,0,0),Vector3(9,0,0),Vector3(9,0,8),Vector3(7,0,8));
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,0,8),Vector3(9,0,8),Vector3(9,0,0),Vector3(7,0,0));
                }
                if(BlockSolid(x,y+1,z) == false)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,16,8),Vector3(9,16,8),Vector3(9,16,0),Vector3(7,16,0));
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,16,0),Vector3(9,16,0),Vector3(9,16,8),Vector3(7,16,8));
                }
                connectsNum ++;
            }
            if(connectFront)
            {
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,0,16),Vector3(8,16,16),Vector3(8,16,8),Vector3(8,0,8));
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,0,8),Vector3(8,16,8),Vector3(8,16,16),Vector3(8,0,16));

                if(BlockSolid(x,y-1,z) == false)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,0,8),Vector3(9,0,8),Vector3(9,0,16),Vector3(7,0,16));
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,0,16),Vector3(9,0,16),Vector3(9,0,8),Vector3(7,0,8));
                }
                if(BlockSolid(x,y+1,z) == false)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(7,16,16),Vector3(9,16,16),Vector3(9,16,8),Vector3(7,16,8));
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(7,16,8),Vector3(9,16,8),Vector3(9,16,16),Vector3(7,16,16));
                }
                connectsNum ++;
            }

            if(connectsNum == 1)
            {
                if(connectLeft || connectRight)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,0,9),Vector3(8,16,9),Vector3(8,16,7),Vector3(8,0,7),true,false,0);
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,0,7),Vector3(8,16,7),Vector3(8,16,9),Vector3(8,0,9),true,false,0);
                }
                if(connectBack || connectFront)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(7,16,8),Vector3(9,16,8),Vector3(9,0,8),Vector3(7,0,8),true,false,0);
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(7,0,8),Vector3(9,0,8),Vector3(9,16,8),Vector3(7,16,8),true,false,0);
                }
            }
        }
    }

	/// Redstone
    if(blockType->blockModel == 11)
    {
        int redstoneVertStart = MeshChunk->currentVertexCount();
        bool connectLeft, connectRight, connectFront, connectBack;
        // Java's BlockRedstoneWire.canConnectTo, which is what decides whether the wire artwork grows a
        // strip toward a neighbour. The plateau is the easy part: wire, a block of redstone, TNT, and
        // (so a line feeding off a flat button does not look unwired from the thing lighting it) a
        // button. A lever answers to the same question for the same reason, which is why this asks
        // IsSwitchBlock rather than naming the two button ids. Two more neighbours answer "yes" but only
        // partially, and both are the reason a wire used to stop one cell short of a component:
        //
        //   * A repeater connects **along its own axis only** -- the cell behind it, which is the input
        //     it reads, and the cell in front, which is the one it drives. Its flanks stay bare, because
        //     a repeater genuinely does not exchange a signal sideways. So a line running into a
        //     repeater's back now joins up, and one butting against its side still conspicuously does
        //     not, which is the visual cue that it will not work.
        //   * A redstone torch connects from any side, since it powers every neighbour but its mount.
        auto wireConnectsTo = [&](int nx, int ny, int nz) -> bool
        {
            block_t nb = GetBlock(nx,ny,nz);
            if(nb == Block || nb == RedstoneBlock::getID() || nb == TNTBlock::getID() || IsSwitchBlock(nb))
            {
                return true;
            }
            if(IsRepeaterBlock(nb))
            {
                // Same nibble read as the repeater mesher: no-check, because the mesh build must not
                // materialise a column for a neighbour that has not generated.
                int f = RepeaterFacingFromState(GetBlockSettingsNoCheck(nx,ny,nz) & OpRedstonePowerMask);
                const int dxr = RepeaterDirX(f);
                const int dzr = RepeaterDirZ(f);
                return (nx + dxr == x && nz + dzr == z)      // the cell it drives, in front of it
                    || (nx - dxr == x && nz - dzr == z);     // the cell it reads, behind it
            }
            return IsRedstoneTorchBlock(nb);
        };
        connectLeft  = wireConnectsTo(x-1,y,z);
        connectRight = wireConnectsTo(x+1,y,z);
        connectBack  = wireConnectsTo(x,y,z-1);
        connectFront = wireConnectsTo(x,y,z+1);

        // Java staircase: a line also reaches dust one step up or down, unless an opaque
        // cube sits on the lower wire and cuts the climb. The flat strip still runs to
        // the edge; a vertical piece is emitted below when the neighbour is the upper one.
        auto dustAt = [&](int nx, int ny, int nz) -> bool
        {
            if(nx < 0 || ny < 0 || nz < 0 || nx >= worldLimit || ny >= WORLD_HEIGHT || nz >= worldLimit)
            {
                return false;
            }
            return GetBlock(nx,ny,nz) == Block;
        };
        auto climbCut = [&](int nx, int ny, int nz) -> bool
        {
            if(ny < 0 || ny >= WORLD_HEIGHT)
            {
                return true;
            }
            return BlockSolid(nx,ny,nz) && BlockTransparentOrSpecial(nx,ny,nz) == false;
        };
        const bool cutUp = climbCut(x, y+1, z);
        const bool climbLeft  = (cutUp == false && dustAt(x-1, y+1, z));
        const bool climbRight = (cutUp == false && dustAt(x+1, y+1, z));
        const bool climbBack  = (cutUp == false && dustAt(x, y+1, z-1));
        const bool climbFront = (cutUp == false && dustAt(x, y+1, z+1));
        const bool dropLeft   = dustAt(x-1, y-1, z) && climbCut(x-1, y, z) == false;
        const bool dropRight  = dustAt(x+1, y-1, z) && climbCut(x+1, y, z) == false;
        const bool dropBack   = dustAt(x, y-1, z-1) && climbCut(x, y, z-1) == false;
        const bool dropFront  = dustAt(x, y-1, z+1) && climbCut(x, y, z+1) == false;
        connectLeft  = connectLeft  || climbLeft  || dropLeft;
        connectRight = connectRight || climbRight || dropRight;
        connectBack  = connectBack  || climbBack  || dropBack;
        connectFront = connectFront || climbFront || dropFront;
        int connectsNum = 0;

        // One strip of wire: a flat plate at y = 1/16 covering the given rectangle in the XZ plane.
        // Java draws the wire with enough thickness to be seen from any angle; ours is a zero-thickness
        // quad pair, so the TOP copy is the one you actually see, and it has to be emitted
        // unconditionally. The plate sits 1/16 inside its own cell, so a full block above it cannot
        // really cover it -- but the old code skipped the top copy whenever BlockSolid(x,y+1,z), and
        // skipped the underside when there was ground below, so dust placed under a block was left with
        // no geometry at all and vanished. Only the underside is still skipped now, and only for solid
        // ground underneath, which genuinely hides it.
        auto emitWireStrip = [&](float x0, float z0, float x1, float z1)
        {
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,
                Vector3(x0,1,z1),Vector3(x1,1,z1),Vector3(x1,1,z0),Vector3(x0,1,z0),1,0);

            if(BlockSolid(x,y-1,z) == false)
            {
                BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,
                    Vector3(x0,1,z0),Vector3(x1,1,z0),Vector3(x1,1,z1),Vector3(x0,1,z1));
            }
        };

        if(!connectFront && !connectBack && !connectLeft && !connectRight)
        {
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,1,16),Vector3(8,1,16),Vector3(8,1,0),Vector3(8,1,0));
            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,1,0),Vector3(8,1,0),Vector3(8,1,16),Vector3(8,1,16),true,false,0);

            emitWireStrip(0.0f, 7.0f, 16.0f, 9.0f);
            emitWireStrip(7.0f, 0.0f, 9.0f, 16.0f);

            BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,1,8),Vector3(16,1,8),Vector3(16,1,8),Vector3(0,1,8),true,false,5);
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,1,8),Vector3(16,1,8),Vector3(16,1,8),Vector3(0,1,8));
        }
        else
        {
            if(connectLeft)
            {
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,1,8),Vector3(8,1,8),Vector3(8,1,8),Vector3(0,1,8));
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,1,8),Vector3(8,1,8),Vector3(8,1,8),Vector3(0,1,8));

                emitWireStrip(0.0f, 7.0f, 8.0f, 9.0f);
                connectsNum ++;
            }
            if(connectRight)
            {
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(8,1,8),Vector3(16,1,8),Vector3(16,1,8),Vector3(8,1,8));
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(8,1,8),Vector3(16,1,8),Vector3(16,1,8),Vector3(8,1,8));

                emitWireStrip(8.0f, 7.0f, 16.0f, 9.0f);
                connectsNum ++;
            }
            if(connectBack)
            {
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,1,8),Vector3(8,1,8),Vector3(8,1,0),Vector3(8,1,0));
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,1,0),Vector3(8,1,0),Vector3(8,1,8),Vector3(8,1,8));

                emitWireStrip(7.0f, 0.0f, 9.0f, 8.0f);
                connectsNum ++;
            }
            if(connectFront)
            {
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,1,16),Vector3(8,1,16),Vector3(8,1,8),Vector3(8,1,8));
                BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,1,8),Vector3(8,1,8),Vector3(8,1,16),Vector3(8,1,16));

                emitWireStrip(7.0f, 8.0f, 9.0f, 16.0f);
                connectsNum ++;
            }

            if(connectsNum == 1)
            {
                if(connectLeft || connectRight)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(8,1,9),Vector3(8,1,9),Vector3(8,1,7),Vector3(8,1,7),true,false,0);
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(8,1,7),Vector3(8,1,7),Vector3(8,1,9),Vector3(8,1,9),true,false,0);
                }
                if(connectBack || connectFront)
                {
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(7,1,8),Vector3(9,1,8),Vector3(9,1,8),Vector3(7,1,8),true,false,0);
                    BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(7,1,8),Vector3(9,1,8),Vector3(9,1,8),Vector3(7,1,8),true,false,0);
                }
            }
        }

        // The climb itself: a vertical strip on the face toward the upper neighbour, so the
        // line is seen running up the side of the block rather than stopping at the edge.
        if(climbLeft)
        {
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,
                Vector3(0,1,9),Vector3(0,16,9),Vector3(0,16,7),Vector3(0,1,7));
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,
                Vector3(0,1,7),Vector3(0,16,7),Vector3(0,16,9),Vector3(0,1,9));
        }
        if(climbRight)
        {
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,
                Vector3(16,1,7),Vector3(16,16,7),Vector3(16,16,9),Vector3(16,1,9));
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,
                Vector3(16,1,9),Vector3(16,16,9),Vector3(16,16,7),Vector3(16,1,7));
        }
        if(climbBack)
        {
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,
                Vector3(7,1,0),Vector3(9,1,0),Vector3(9,16,0),Vector3(7,16,0));
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,
                Vector3(9,1,0),Vector3(7,1,0),Vector3(7,16,0),Vector3(9,16,0));
        }
        if(climbFront)
        {
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,
                Vector3(9,1,16),Vector3(7,1,16),Vector3(7,16,16),Vector3(9,16,16));
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,
                Vector3(7,1,16),Vector3(9,1,16),Vector3(9,16,16),Vector3(7,16,16));
        }

        int rsPower = GetRedstoneDustPower(x,y,z);
        if(rsPower > 0)
        {
            float t = 0.18f + 0.82f * ((float)rsPower / 15.0f);
            MeshChunk->multiplyColourFrom(redstoneVertStart, 0.28f + (1.15f - 0.28f) * t, 0.04f + (0.32f - 0.04f) * t, 0.04f + (0.22f - 0.04f) * t);
        }
        else
        {
            MeshChunk->multiplyColourFrom(redstoneVertStart, 0.28f, 0.04f, 0.04f);
        }
    }

	if(blockType->blockModel == 12)
    {
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(0,0,16),Vector3(0,15,16),Vector3(0,15,0),Vector3(0,0,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(16,0,0),Vector3(16,15,0),Vector3(16,15,16),Vector3(16,0,16));

        if(BlockTransparentOrSpecial(x,y-0.1f,z) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(0,0,0),Vector3(16,0,0),Vector3(16,0,16),Vector3(0,0,16));

        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(0,15,16),Vector3(16,15,16),Vector3(16,15,0),Vector3(0,15,0));
        BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(0,15,0),Vector3(16,15,0),Vector3(16,0,0),Vector3(0,0,0));

        if(BlockTransparentOrSpecial(x,y,z+1) == true)
            BuildWorldBlockPlaneNoCheck(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(0,0,16),Vector3(16,0,16),Vector3(16,15,16),Vector3(0,15,16));

        return;
    }
	/// Button: a 6x4x2 box on the face it is mounted on, pushed one pixel deeper while pressed.
	/// Bounds come from ButtonCellBox() so the pick ray and the selection outline match exactly.
	if (blockType->blockModel == 14)
	{
		// Read the state nibble directly: this is the mesher, so use the no-check accessor rather than
		// GetButtonState(), which calls the bounds-checked GetBlock() and can create columns. That nibble
		// now holds only the button's facing/pressed state, because SetLightLevel skips button cells.
		int state = GetBlockSettingsNoCheck((int)x, (int)y, (int)z) & OpRedstonePowerMask;
		BoundingBox bb = ButtonCellBox(state);

		// ButtonCellBox is centred on the cell; the mesher works in 0..16 pixels from the cell corner.
		const float ax0 = (bb.min.x + 0.5f) * 16.0f, ax1 = (bb.max.x + 0.5f) * 16.0f;
		const float ay0 = (bb.min.y + 0.5f) * 16.0f, ay1 = (bb.max.y + 0.5f) * 16.0f;
		const float az0 = (bb.min.z + 0.5f) * 16.0f, az1 = (bb.max.z + 0.5f) * 16.0f;

		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,Vector3(ax0,ay0,az1),Vector3(ax0,ay1,az1),Vector3(ax0,ay1,az0),Vector3(ax0,ay0,az0));
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,Vector3(ax1,ay0,az0),Vector3(ax1,ay1,az0),Vector3(ax1,ay1,az1),Vector3(ax1,ay0,az1));
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,Vector3(ax0,ay0,az0),Vector3(ax1,ay0,az0),Vector3(ax1,ay0,az1),Vector3(ax0,ay0,az1));
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,4,Vector3(ax0,ay1,az1),Vector3(ax1,ay1,az1),Vector3(ax1,ay1,az0),Vector3(ax0,ay1,az0));
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,Vector3(ax0,ay1,az0),Vector3(ax1,ay1,az0),Vector3(ax1,ay0,az0),Vector3(ax0,ay0,az0));
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,Vector3(ax0,ay0,az1),Vector3(ax1,ay0,az1),Vector3(ax1,ay1,az1),Vector3(ax0,ay1,az1));
		return;
	}

	/// Redstone repeater: a 2 px slab whose top face carries the atlas repeater tile, so the torch row
	/// reads along the direction it drives. The lit variant is a separate block id (RepeaterOn), which
	/// means the tile is just blockType->upPlane and the mesher needs no per-state texture switch.
	if (blockType->blockModel == 15)
	{
		// Same nibble reuse as the button: read it no-check here, because GetRepeaterState() goes
		// through the bounds-checked GetBlock() and the mesher must not create columns.
		int state = GetBlockSettingsNoCheck((int)x, (int)y, (int)z) & OpRedstonePowerMask;
		int facing = RepeaterFacingFromState(state);
		int delay  = RepeaterDelayFromState(state);

		// Axis order matches the power code: 0 = -Z, 1 = +X, 2 = +Z, 3 = -X. Both the tile and the
		// torch row are laid out along this direction.
		static const int kTorchDirX[4] = { 0, 1, 0,-1};
		static const int kTorchDirZ[4] = {-1, 0, 1, 0};

		const float top = 2.0f;              // Java: a 2 px slab, in the same 1/16 units the other models use
		const float x0 = 0.0f,  x1 = 16.0f;
		const float z0 = 0.0f,  z1 = 16.0f;

		// The top face is emitted by hand instead of through BuildWorldBlockPlane, and that is not a
		// matter of taste. On a top face that helper derives its two `up`/`down` texture values from the
		// same coordinate its two `left`/`right` values come from -- both read point.x and point.z of the
		// *same* pair of points -- so every one of its textureRotation cases collapses the tile onto a
		// line, and cases 1 and 2 even collapse to the same permutation. The face still renders, which is
		// why this hid for so long: what you see is the whole sprite smeared along one axis, and a smear
		// of this tile looks enough like a bar to pass at a glance while pointing 90 degrees away from the
		// torch row. Drawing the quad here is what lets +U and +V land on two different world axes.
		//
		// +V runs along the repeater's axis, because the tile's torch row runs up the sprite along V, and +U
		// is that axis turned a quarter turn by `up x V`. Which *end* of the axis V points down is decided
		// by the tile's own artwork, which is not symmetric -- see the negation below. +U is the
		// perpendicular that keeps the tile unmirrored: the reference top-face map is +X -> +U, +Z -> +V
		// with determinant +1, and `up x V` is the only perpendicular choice that preserves that sign.
		{
			const float dx = -(float)kTorchDirX[facing];
			const float dz = -(float)kTorchDirZ[facing];
			// up x direction for up = +Y. With up = (0,1,0) and direction = (dx,0,dz) that product is
			// (dz, 0, -dx) -- note the unnegated dz. Getting that sign wrong lands on direction x up
			// instead, which is a mirror, and a mirrored tile is exactly as wrong as a rotated one for a
			// block whose whole job is to point somewhere.
			const float ux = dz, uz = -dx;

			// Both components are negated, which is a true 180 turn of the tile and not a mirror -- the
			// determinant stays +1. The tile's artwork is not symmetric end for end: its dark rail spans
			// sprite rows 6..13, so there are five stone rows above it and two below, and the wide end
			// reads as the input. With V on the drive direction that put the wide end at the output and
			// the whole base looked turned round -- the tile pointed back at whoever placed the repeater.
			// The 3D torches are placed from `facing` directly and are untouched by this: their fixed
			// torch stays on the output side, which is vanilla.

			const float u0 = percent * blockType->upPlane_x;
			const float u1 = u0 + percent;
			const float v0 = percent * blockType->upPlane_y;
			const float v1 = v0 + percent;

			const float tileLight = BlockFinalLight(x,y,z);

			// Corners in the same order the shared builder used -- (x0,z1), (x1,z1), (x1,z0), (x0,z0)
			// -- so the winding and the back-face culling are exactly as they were.
			const float cornerX[4] = { -8.0f,  8.0f,  8.0f, -8.0f};
			const float cornerZ[4] = {  8.0f,  8.0f, -8.0f, -8.0f};
			for(int c = 0; c < 4; c++)
			{
				const float along = cornerX[c]*dx + cornerZ[c]*dz;   // +8 at the input end, after the 180
				const float side  = cornerX[c]*ux + cornerZ[c]*uz;   // +8 on the +U side
				MeshChunk->vert(x + (8.0f + cornerX[c])*pixel, y + top*pixel, z + (8.0f + cornerZ[c])*pixel,
					(side > 0.0f) ? u1 : u0, (along > 0.0f) ? v1 : v0, tileLight, tileLight, tileLight);
			}
			MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
			MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
			iVertex += 4;
		}

		// The rim is a plain side face, where the shared builder's mapping is sound (u comes from the
		// point's z, v from its y), so these stay as they were.
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,3,
			Vector3(x0,0,z0),Vector3(x1,0,z0),Vector3(x1,0,z1),Vector3(x0,0,z1),false,false,0,0);
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,1,
			Vector3(x0,0,z1),Vector3(x0,top,z1),Vector3(x0,top,z0),Vector3(x0,0,z0),false,false,0,0);
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,2,
			Vector3(x1,0,z0),Vector3(x1,top,z0),Vector3(x1,top,z1),Vector3(x1,0,z1),false,false,0,0);
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,5,
			Vector3(x0,top,z0),Vector3(x1,top,z0),Vector3(x1,0,z0),Vector3(x0,0,z0),false,false,0,0);
		BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,6,
			Vector3(x0,0,z1),Vector3(x1,0,z1),Vector3(x1,top,z1),Vector3(x0,top,z1),false,false,0,0);

		// Locking (vanilla RenderBlockRedstoneDiode): a locked repeater draws a bedrock bar over its top in
		// addition to the torch row, and that bar is the only way to see a lock from outside -- the block
		// id and state are untouched. It runs perpendicular to the direction the repeater drives, so it
		// reads as "held" from any angle.
		//
		// alwaysBuild + constantColor are both deliberate. The bar lives inside the cell, so the usual
		// face test would look at the cell next door and drop faces that are plainly visible, and the
		// usual per-vertex light would take its light from that same wrong cell. The underside is not
		// emitted at all: it lies flush on the slab's top face and would z-fight with it.
		if(RepeaterIsLocked((int)x, (int)y, (int)z))
		{
			// The bar stops 2 px short of each edge rather than running the full 16. Two reasons: it is how
			// the lock reads as an object resting on the repeater instead of a stripe painted across the
			// block, and a bar that reached the cell boundary would z-fight with whatever sits beside the
			// repeater and would be clipped by the neighbour's mesh at the chunk edge. The 4 px width is
			// unchanged, so only the ends move.
			float bx0, bx1, bz0, bz1;
			if(facing == 1 || facing == 3)   // drives along X, so the bar spans Z
			{
				bx0 = 6.0f; bx1 = 10.0f; bz0 = 2.0f; bz1 = 14.0f;
			}
			else                             // drives along Z, so the bar spans X
			{
				bx0 = 2.0f; bx1 = 14.0f; bz0 = 6.0f; bz1 = 10.0f;
			}
			const float by0 = top;           // rests on the slab ...
			const float by1 = top + 2.0f;    // ... and stands 2 px proud of it
			BaseBlock* barTex = &blockTypes[bedrock::getID()]; // the tile vanilla textures the lock bar with

			// The bar's top face again has to be drawn by hand: it is a top face, so the shared builder's
			// rotation cases all smear it (see the slab's top face above). Bedrock is uniform noise, so
			// this was invisible rather than wrong-looking, but the reference map is one line to write
			// down: +X -> +U, +Z -> +V.
			{
				const float u0 = percent * barTex->upPlane_x;
				const float u1 = u0 + percent;
				const float v0 = percent * barTex->upPlane_y;
				const float v1 = v0 + percent;
				const float barLight = BlockFinalLight(x,y,z);
				const float barX[4] = { bx0, bx1, bx1, bx0};
				const float barZ[4] = { bz1, bz1, bz0, bz0};
				for(int c = 0; c < 4; c++)
				{
					MeshChunk->vert(x + barX[c]*pixel, y + by1*pixel, z + barZ[c]*pixel,
						(c == 0 || c == 3) ? u0 : u1, (c < 2) ? v1 : v0, barLight, barLight, barLight);
				}
				MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
				MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
				iVertex += 4;
			}
			BuildWorldBlockPlane(barTex,x,y,z,iVertex,MeshChunk,Block,transparent,1,
				Vector3(bx0,by0,bz1),Vector3(bx0,by1,bz1),Vector3(bx0,by1,bz0),Vector3(bx0,by0,bz0),true,true,0,0);
			BuildWorldBlockPlane(barTex,x,y,z,iVertex,MeshChunk,Block,transparent,2,
				Vector3(bx1,by0,bz0),Vector3(bx1,by1,bz0),Vector3(bx1,by1,bz1),Vector3(bx1,by0,bz1),true,true,0,0);
			BuildWorldBlockPlane(barTex,x,y,z,iVertex,MeshChunk,Block,transparent,5,
				Vector3(bx0,by1,bz0),Vector3(bx1,by1,bz0),Vector3(bx1,by0,bz0),Vector3(bx0,by0,bz0),true,true,0,0);
			BuildWorldBlockPlane(barTex,x,y,z,iVertex,MeshChunk,Block,transparent,6,
				Vector3(bx0,by0,bz1),Vector3(bx1,by0,bz1),Vector3(bx1,by1,bz1),Vector3(bx0,by1,bz1),true,true,0,0);
		}
		// The torch row is drawn even when the repeater is locked. Vanilla's locked look is the bedrock bar
		// laid *over* an otherwise untouched repeater, so skipping the torches here made a locked repeater
		// look like a broken one. Axis order matches the power code: 0 = -Z, 1 = +X, 2 = +Z, 3 = -X.
		{
			// The lit variant is a separate block id, so the sprite comes straight off whichever of the
			// two torch blocks matches this state -- no per-face texture switch needed.
			BaseBlock* torchType = &blockTypes[(Block == RepeaterOn::getID())
				? RedstoneTorch::getID() : UnlitRedstoneTorch::getID()];

			// Sample only the columns the torch artwork actually occupies. The plain-torch mesher reads a
			// 4 texel wide slice of the sprite (columns 6..10), but the artwork in every pack is only 2
			// texels wide below the tip -- columns 7 and 8 -- with alpha either side. This mesh is drawn
			// opaque, so those two texels of transparency came out as dead dark edges and the stick read
			// as half the width of the cap, which is the "sides do not match the top" report. Reading
			// 7..9 makes the stick fill the quad, so the sides and the cap are the same width.
			//
			// The lit sprite sits one texel row higher than the unlit one, and that is measured, not
			// assumed: in every pack's atlas the lit tile's artwork starts at row 5 and the unlit tile's
			// at row 6. It is not a pack quirk -- the lit redstone torch carries a flame above its head,
			// so its sprite is genuinely one row taller. That extra row is the flame's red rim, and it is
			// deliberately *not* used: the plain-torch mesher, whose head reads correctly, starts its
			// window at row 6 for the sides and rows 6..8 for the cap, and Java's own torch UVs are
			// `getInterpolatedV(6)` .. `(13)` on the sides and `(6)` .. `(8)` on the top face. Starting at
			// 5 for the lit state is what took the ball away: the cap spans the window's first two rows,
			// and rows 5..6 are the red rim over the top of the core, so the lit torch showed a flat red
			// square instead of the head's bright centre. Rows 6..8 are that centre -- `FFD800`/`FF8F00`
			// over `FFFF97`/`FFFFFF` -- which is the glowing ball seen from above. The unlit sprite needs
			// the same number for the opposite reason: its row 5 is transparent, so a window opening
			// there gave it a dead top row and a cap that was 2x1.
			const float rowStart = 6.0f;
			const float su0 = percent * torchType->leftPlane_x + texturePixel * 7.0f;
			const float su1 = su0 + texturePixel * 2.0f;
			const float sv0 = percent * torchType->leftPlane_y + texturePixel * rowStart;
			// Seven texel rows over the 7 px model, one row per pixel, so the five rows that stay outside a
			// 2 px slab are the artwork's first five and everything below them is buried in the base.
			const float sv1 = percent * torchType->leftPlane_y + texturePixel * (rowStart + 7.0f);
			// The cap samples the same two columns and the head, which is what the top face shows. It
			// starts on the same row the sides do, so the two agree at the corner, exactly as the plain
			// torch's do.
			const float cu0 = su0;
			const float cu1 = su1;
			const float cv0 = sv0;
			const float cv1 = cv0 + texturePixel * 2.0f;

			// Shade the torches with their own cell rather than leaving them fullbright, so an unlit
			// redstone torch does not glow in a dark room.
			const float tl = BlockFinalLight(x,y,z);

			// `along` runs from the input edge (-8) to the output edge (+8). Both torches sit on the
			// centre line, as Java's do. The quad winding is copied from the plain-torch mesher, which is
			// known to survive this engine's back-face culling.
			auto emitRepeaterTorch = [&](float along)
			{
				const float cxp = 8.0f + along * (float)kTorchDirX[facing];
				const float czp = 8.0f + along * (float)kTorchDirZ[facing];

			// The stick rises from the *block floor*, not from the top of the 2 px slab, and stops at 7 px.
			// Together with the sprite rows chosen above that puts the visible top five rows exactly in the
			// five pixels between the slab surface and the top of the torch, so the torches read as 5 px
			// tall from the outside with the rest of the artwork hidden inside the base.
			//
			// The cap is flush with the top of the stick (ycap == y1) rather than one pixel below it as the
			// plain torch does. That one pixel is what left a ring of side texture standing proud of the
			// cap; flush, the torch is a clean 2x2x7 box and the sides meet the top instead of overshooting.
			const float hh = 1.0f;                        // 2 px footprint
			const float y0 = 0.0f, y1 = 7.0f, ycap = 7.0f;
			const float xm = cxp - hh, xp = cxp + hh;
			const float zm = czp - hh, zp = czp + hh;

				// -X
				MeshChunk->vert(x + xm*pixel, y + y0*pixel, z + zp*pixel, su1, sv1, tl,tl,tl);
				MeshChunk->vert(x + xm*pixel, y + y1*pixel, z + zp*pixel, su1, sv0, tl,tl,tl);
				MeshChunk->vert(x + xm*pixel, y + y1*pixel, z + zm*pixel, su0, sv0, tl,tl,tl);
				MeshChunk->vert(x + xm*pixel, y + y0*pixel, z + zm*pixel, su0, sv1, tl,tl,tl);
				MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
				MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
				iVertex += 4;

				// +X
				MeshChunk->vert(x + xp*pixel, y + y0*pixel, z + zm*pixel, su1, sv1, tl,tl,tl);
				MeshChunk->vert(x + xp*pixel, y + y1*pixel, z + zm*pixel, su1, sv0, tl,tl,tl);
				MeshChunk->vert(x + xp*pixel, y + y1*pixel, z + zp*pixel, su0, sv0, tl,tl,tl);
				MeshChunk->vert(x + xp*pixel, y + y0*pixel, z + zp*pixel, su0, sv1, tl,tl,tl);
				MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
				MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
				iVertex += 4;

				// -Z
				MeshChunk->vert(x + xm*pixel, y + y1*pixel, z + zm*pixel, su1, sv0, tl,tl,tl);
				MeshChunk->vert(x + xp*pixel, y + y1*pixel, z + zm*pixel, su0, sv0, tl,tl,tl);
				MeshChunk->vert(x + xp*pixel, y + y0*pixel, z + zm*pixel, su0, sv1, tl,tl,tl);
				MeshChunk->vert(x + xm*pixel, y + y0*pixel, z + zm*pixel, su1, sv1, tl,tl,tl);
				MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
				MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
				iVertex += 4;

				// +Z
				MeshChunk->vert(x + xm*pixel, y + y0*pixel, z + zp*pixel, su0, sv1, tl,tl,tl);
				MeshChunk->vert(x + xp*pixel, y + y0*pixel, z + zp*pixel, su1, sv1, tl,tl,tl);
				MeshChunk->vert(x + xp*pixel, y + y1*pixel, z + zp*pixel, su1, sv0, tl,tl,tl);
				MeshChunk->vert(x + xm*pixel, y + y1*pixel, z + zp*pixel, su0, sv0, tl,tl,tl);
				MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
				MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
				iVertex += 4;

				// +Y (the flat cap, matching the plain-torch mesh)
				MeshChunk->vert(x + xm*pixel, y + ycap*pixel, z + zp*pixel, cu0, cv0, tl,tl,tl);
				MeshChunk->vert(x + xp*pixel, y + ycap*pixel, z + zp*pixel, cu1, cv0, tl,tl,tl);
				MeshChunk->vert(x + xp*pixel, y + ycap*pixel, z + zm*pixel, cu1, cv1, tl,tl,tl);
				MeshChunk->vert(x + xm*pixel, y + ycap*pixel, z + zm*pixel, cu0, cv1, tl,tl,tl);
				MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
				MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
				iVertex += 4;
			};

			// Both positions are Java's, not guesses. The fixed torch is 5 px out on the output side
			// (`Direction.offset * 0.3125`), and the delay torch walks one pixel per step from 1 px out
			// on the output side at delay 1 to 5 px out on the *input* side at delay 4, which is
			// `BlockRedstoneRepeater.repeaterTorchOffset = {-0.0625, 0.0625, 0.1875, 0.3125}` read in
			// the output-relative direction. At delay 1 the two torches are 4 px apart; at delay 4 they
			// are at opposite ends, 10 px apart.
			static const float kMovingTorchOffset[4] = { 1.0f, -1.0f, -3.0f, -5.0f };
			emitRepeaterTorch(5.0f);                                      // fixed torch
			emitRepeaterTorch(kMovingTorchOffset[(delay - 1) & 3]);       // moving "tick" torch
		}
		return;
	}

	/// Lever (Java BlockLever). Built the way vanilla builds it: a cobblestone base with the handle out
	/// of it, and the handle leaning one way while off and the other while on. The three sizes are Java's
	/// own -- a 6 x 8 x 3 px base and a 10 px handle on a 40 degree lean -- and they are stated where they
	/// are used below rather than here.
	///
	/// Two decisions here are worth writing down, because both look arbitrary from the outside.
	///
	/// **The base is cobblestone, not the lever tile.** The lever's own sprite is nothing but the handle
	/// -- a 2 px wide strip down columns 7-8, transparent everywhere else -- so a base drawn from it would
	/// be invisible. Vanilla's model has the same split (`base` = cobblestone, `lever` = lever.png), so
	/// the atlas already holds both tiles and no new art is needed.
	///
	/// **Everything is emitted by hand into a frame that is rebuilt per mounting face.** The button's
	/// mesher can lean on BuildWorldBlockPlane because its box always lies on the cell's own axes. A
	/// lever's handle does not: it is rotated 40 degrees inside the plane the mounting defines, and the
	/// shared builder derives each face's texture coordinates from the face's *own* axis-aligned
	/// coordinates, so a rotated box would come out with the sprite smeared along one axis -- the same
	/// trap the repeater's top face documents. Hand-emitting costs one frame and a quad helper, and the
	/// UV window is then explicit and correct on all six mountings.
	if (blockType->blockModel == 16)
	{
		int state = GetBlockSettingsNoCheck((int)x, (int)y, (int)z) & OpRedstonePowerMask;
		int facing = ButtonFacingFromState(state);
		bool on = ButtonPressedFromState(state);

		const float cx = x + 0.5f, cy = y + 0.5f, cz = z + 0.5f;
		const float tileLight = BlockFinalLight(x,y,z);

		// Same facing order as the button and the repeater: 0 mounts on the block below, 1 on the block
		// above, then -X, +X, -Z, +Z. Copied here rather than shared because the button's copy is file-local
		// to CraftWorld2.cpp, and a direction table is cheaper to duplicate than to expose.
		static const int kLeverDirX[6] = { 0, 0,-1, 1, 0, 0};
		static const int kLeverDirY[6] = {-1, 1, 0, 0, 0, 0};
		static const int kLeverDirZ[6] = { 0, 0, 0, 0,-1, 1};

		// The frame. `d` points at the block the lever is mounted on, so `n` points out of that face into
		// the cell -- the direction the base grows along and the handle rises from.
		const float dx = (float)kLeverDirX[facing];
		const float dy = (float)kLeverDirY[facing];
		const float dz = (float)kLeverDirZ[facing];
		const float nx = -dx, ny = -dy, nz = -dz;

		// The axis the handle falls along. Vanilla picks one of four horizontal axes for a floor lever
		// from the placer's yaw; this port has no room for that choice, because the state nibble's three
		// facing bits are already spent on the mounting and bit 3 is the latch. Floor and ceiling levers
		// therefore always lean along +X, and wall levers -- whose normal is horizontal -- along +Y, so
		// the handle always leans "upright" for the face it sits on, which is the half of vanilla's
		// behaviour a player would notice.
		float lx, ly, lz;
		if(ny != 0.0f)
		{
			lx = 1.0f; ly = 0.0f; lz = 0.0f;
		}
		else
		{
			lx = 0.0f; ly = 1.0f; lz = 0.0f;
		}

		// t2: the third axis of the frame, perpendicular to both n and the lean axis.
		const float t2x = ny*lz - nz*ly;
		const float t2y = nz*lx - nx*lz;
		const float t2z = nx*ly - ny*lx;

		// Java leans the handle 40 degrees: RenderBlocks.renderBlockLever rotates by `2*PI/9`, which is 40,
		// not the 45 a hand-built model reaches for. So these two numbers are the sine and cosine of that
		// angle, and `on` picks the side it falls to -- the latch bit's only visible job.
		const float SIN = 0.64278761f;   // sin 40, the lean
		const float COS = 0.76604444f;   // cos 40, how upright the stick stays
		const float s = on ? 1.0f : -1.0f;
		// Handle direction, and s1: perpendicular to it inside the n/lean plane. This pair works at any
		// angle, which is why the 45-degree diagonal could be dropped: hd . s1 = COS*SIN*s - SIN*s*COS = 0.
		const float hdx = nx*COS + lx*SIN*s;
		const float hdy = ny*COS + ly*SIN*s;
		const float hdz = nz*COS + lz*SIN*s;
		const float s1x = -s*SIN*nx + COS*lx;
		const float s1y = -s*SIN*ny + COS*ly;
		const float s1z = -s*SIN*nz + COS*lz;

		// One vertex. The position is a pixel offset from the cell centre; the two texture values are the
		// pixel inside the *tile* that this vertex shows, so every caller below states its own UV window
		// instead of inheriting one.
		struct LeverVert { float x, y, z, u, v; };
		auto makeVert = [&](float ox, float oy, float oz, int tileX, int tileY, float upx, float vpx)
		{
			LeverVert lv;
			lv.x = cx + ox * pixel;
			lv.y = cy + oy * pixel;
			lv.z = cz + oz * pixel;
			lv.u = percent * (tileX + upx / 16.0f);
			lv.v = percent * (tileY + vpx / 16.0f);
			return lv;
		};

		// Emit a quad, correcting the corner order against the direction the face looks. Necessary here
		// and not merely tidy: the frame is rotated differently for each of the six mountings, so a corner
		// order that winds correctly on a floor lever is a mirror of the one that winds correctly on a
		// wall lever. Reversing the four corners keeps the same quad and flips the winding.
		auto emitFace = [&](float outX, float outY, float outZ, const LeverVert q[4])
		{
			LeverVert a = q[0], b = q[1], c = q[2], dq = q[3];
			float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
			float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
			float fnx = e1y*e2z - e1z*e2y;
			float fny = e1z*e2x - e1x*e2z;
			float fnz = e1x*e2y - e1y*e2x;
			if(fnx*outX + fny*outY + fnz*outZ < 0.0f)
			{
				LeverVert tmp = b; b = dq; dq = tmp;
			}
			MeshChunk->vert(a.x, a.y, a.z, a.u, a.v, tileLight, tileLight, tileLight);
			MeshChunk->vert(b.x, b.y, b.z, b.u, b.v, tileLight, tileLight, tileLight);
			MeshChunk->vert(c.x, c.y, c.z, c.u, c.v, tileLight, tileLight, tileLight);
			MeshChunk->vert(dq.x, dq.y, dq.z, dq.u, dq.v, tileLight, tileLight, tileLight);
			MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
			MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
			iVertex += 4;
		};

		// Base: 6 px across x 8 px along the lean axis x 3 px deep, sitting on the mounting face. Java's
		// three numbers are `f` = 0.25 (a 4 px half-extent *along the lean axis*), `f1` = 0.1875 (a 3 px
		// across half-extent) and `f2` = 0.1875 (3 px thick) -- and that asymmetry is the point: the 8 px
		// axis is the one the handle falls across, so a square base lets a leaning handle overhang its own
		// pedestal. `along` counts pixels out of the mounting face, so 0 is flush with it and 8 is the far
		// side of the cell.
		const int COBBLE_X = 0, COBBLE_Y = 1;   // the cobblestone tile
		// Java paints the whole base with the cobblestone icon and lets the box stretch it; here it is a
		// stated window instead -- 8 x 6 px -- so the long faces keep the texture's scale rather than
		// smearing it along their length. U runs across the lean axis (columns 4..12), V runs across the
		// other one (rows 5..11). The 3 px faces reuse the same window because they are edge-on.
		auto baseVert = [&](float along, float a1, float a2, float upx, float vpx)
		{
			return makeVert(nx*(along - 8.0f) + lx*a1 + t2x*a2,
			                ny*(along - 8.0f) + ly*a1 + t2y*a2,
			                nz*(along - 8.0f) + lz*a1 + t2z*a2,
			                COBBLE_X, COBBLE_Y, upx, vpx);
		};
		const float HW = 3.0f;   // half of the 6 px across
		const float HL = 4.0f;   // half of the 8 px along the lean axis
		{
			// The two faces across the lean axis (a1 = +/-HL). U runs along a2, the 6 px direction.
			LeverVert q[4];
			q[0] = baseVert(0.0f,  HL,  HW, 11.0f, 11.0f);
			q[1] = baseVert(3.0f,  HL,  HW, 11.0f,  5.0f);
			q[2] = baseVert(3.0f,  HL, -HW,  5.0f,  5.0f);
			q[3] = baseVert(0.0f,  HL, -HW,  5.0f, 11.0f);
			emitFace(lx, ly, lz, q);
			q[0] = baseVert(0.0f, -HL, -HW,  5.0f, 11.0f);
			q[1] = baseVert(3.0f, -HL, -HW,  5.0f,  5.0f);
			q[2] = baseVert(3.0f, -HL,  HW, 11.0f,  5.0f);
			q[3] = baseVert(0.0f, -HL,  HW, 11.0f, 11.0f);
			emitFace(-lx, -ly, -lz, q);

			// The two long faces along the lean axis (a2 = +/-HW). U now spans the full 8 px window.
			q[0] = baseVert(0.0f, -HL,  HW,  4.0f, 11.0f);
			q[1] = baseVert(3.0f, -HL,  HW,  4.0f,  5.0f);
			q[2] = baseVert(3.0f,  HL,  HW, 12.0f,  5.0f);
			q[3] = baseVert(0.0f,  HL,  HW, 12.0f, 11.0f);
			emitFace( t2x,  t2y,  t2z, q);
			q[0] = baseVert(0.0f,  HL, -HW, 12.0f, 11.0f);
			q[1] = baseVert(3.0f,  HL, -HW, 12.0f,  5.0f);
			q[2] = baseVert(3.0f, -HL, -HW,  4.0f,  5.0f);
			q[3] = baseVert(0.0f, -HL, -HW,  4.0f, 11.0f);
			emitFace(-t2x, -t2y, -t2z, q);

			// The top of the base and the face against the mount. Both are normally hidden -- the first by
			// the handle, the second by the block the lever hangs on -- but they cost four vertices each and
			// without them a lever mounted on glass or water would show a hole straight through.
			q[0] = baseVert(3.0f, -HL,  HW,  4.0f,  5.0f);
			q[1] = baseVert(3.0f,  HL,  HW, 12.0f,  5.0f);
			q[2] = baseVert(3.0f,  HL, -HW, 12.0f, 11.0f);
			q[3] = baseVert(3.0f, -HL, -HW,  4.0f, 11.0f);
			emitFace(nx, ny, nz, q);
			q[0] = baseVert(0.0f, -HL, -HW,  4.0f,  5.0f);
			q[1] = baseVert(0.0f,  HL, -HW, 12.0f,  5.0f);
			q[2] = baseVert(0.0f,  HL,  HW, 12.0f, 11.0f);
			q[3] = baseVert(0.0f, -HL,  HW,  4.0f, 11.0f);
			emitFace(-nx, -ny, -nz, q);
		}

		// Handle: a 2 x 2 px box 10 px long (Java's `f5` = 0.625), hinged 2 px out of the mounting face.
		// That hinge sits *inside* the 3 px base rather than on top of it, which is what makes the stick look
		// planted in the pedestal; Java also slides the whole stick one pixel toward the side it falls (the
		// `zCoord -=/+ 0.0625` that runs before the tilt), so the hinge is deliberately off-centre. Ten
		// pixels is also exactly the length of the artwork's strip, so the texture maps at 1:1.
		//
		// Its UV window is the one place the sprite's own layout is load-bearing. The handle artwork is a
		// 2 px strip at columns 7-8 running from row 6 down to row 15, so the wide faces read columns 7..9
		// and map the length onto rows 6..15 with the artwork's bright end (the knob, rows 6-7) at the
		// *free* end of the handle. Mapping it the other way round is what makes a lever look upside down.
		const float LEN = 10.0f;        // px from the hinge to the tip
		const float HINGE_OUT = 6.0f;   // px out of the cell centre, i.e. 2 px out of the mounting face
		const float hingeX = nx*(-HINGE_OUT) + lx*(1.0f*s);
		const float hingeY = ny*(-HINGE_OUT) + ly*(1.0f*s);
		const float hingeZ = nz*(-HINGE_OUT) + lz*(1.0f*s);
		auto shaftVert = [&](float a, float c1, float c2, float upx, float vpx)
		{
			return makeVert(hingeX + hdx*a + s1x*c1 + t2x*c2,
			                hingeY + hdy*a + s1y*c1 + t2y*c2,
			                hingeZ + hdz*a + s1z*c1 + t2z*c2,
			                0, 6, upx, vpx);
		};
		// Distance from the hinge, turned into the sprite row the vertex should read.
		auto rowAt = [&](float a) { return 15.0f - a * (9.0f / LEN); };
		{
			LeverVert q[4];
			// The wide faces, c1 = +/-1. They span the handle's 2 px width in c2, which is the direction
			// the sprite's strip is wide -- so U runs across c2 (columns 7..9) and V runs along the handle
			// (rows 6..15, knob at the tip). The off-side face mirrors U so the artwork is not reversed
			// when it is seen from behind, which is the usual reason to write the back face out by hand.
			q[0] = shaftVert(0.0f,  1.0f,  1.0f,  9.0f, rowAt(0.0f));
			q[1] = shaftVert(LEN,   1.0f,  1.0f,  9.0f, rowAt(LEN));
			q[2] = shaftVert(LEN,   1.0f, -1.0f,  7.0f, rowAt(LEN));
			q[3] = shaftVert(0.0f,  1.0f, -1.0f,  7.0f, rowAt(0.0f));
			emitFace(s1x, s1y, s1z, q);
			q[0] = shaftVert(0.0f, -1.0f, -1.0f,  7.0f, rowAt(0.0f));
			q[1] = shaftVert(LEN,  -1.0f, -1.0f,  7.0f, rowAt(LEN));
			q[2] = shaftVert(LEN,  -1.0f,  1.0f,  9.0f, rowAt(LEN));
			q[3] = shaftVert(0.0f, -1.0f,  1.0f,  9.0f, rowAt(0.0f));
			emitFace(-s1x, -s1y, -s1z, q);

			// The two edges, c2 = +/-1, spanning the 2 px in c1. Narrower than the wide faces only in the
			// sense that the strip is 2 px across either way -- the two pairs are identical quads turned a
			// quarter turn, which is what a 2 x 2 cross-section means.
			q[0] = shaftVert(0.0f, -1.0f,  1.0f,  7.0f, rowAt(0.0f));
			q[1] = shaftVert(LEN,  -1.0f,  1.0f,  7.0f, rowAt(LEN));
			q[2] = shaftVert(LEN,   1.0f,  1.0f,  9.0f, rowAt(LEN));
			q[3] = shaftVert(0.0f,  1.0f,  1.0f,  9.0f, rowAt(0.0f));
			emitFace(t2x, t2y, t2z, q);
			q[0] = shaftVert(0.0f,  1.0f, -1.0f,  9.0f, rowAt(0.0f));
			q[1] = shaftVert(LEN,   1.0f, -1.0f,  9.0f, rowAt(LEN));
			q[2] = shaftVert(LEN,  -1.0f, -1.0f,  7.0f, rowAt(LEN));
			q[3] = shaftVert(0.0f, -1.0f, -1.0f,  7.0f, rowAt(0.0f));
			emitFace(-t2x, -t2y, -t2z, q);

			// The knob: the sprite's bright rows 6..8 seen end-on. This is the only face of the handle the
			// player reads as "which end is the grip" from directly above a floor lever, so it is worth the
			// four vertices. The hinge end is deliberately left open -- it is buried in the base.
			q[0] = shaftVert(LEN, -1.0f,  1.0f,  7.0f,  6.0f);
			q[1] = shaftVert(LEN,  1.0f,  1.0f,  9.0f,  6.0f);
			q[2] = shaftVert(LEN,  1.0f, -1.0f,  9.0f,  8.0f);
			q[3] = shaftVert(LEN, -1.0f, -1.0f,  7.0f,  8.0f);
			emitFace(hdx, hdy, hdz, q);
		}
		return;
	}

	/// Piston base and piston head (Java BlockPistonBase / BlockPistonExtension). Two ids share this
	/// block because "extended" is an id in this port rather than a state bit: the base is the full cube
	/// when retracted and a 4 px plate plus a 4x4 rod when its head is out, and the head is a 4 px plate
	/// plus the rod's other half. Vanilla splits the rod the same way, and splitting it is what keeps the
	/// whole piston inside its own two cells -- the per-chunk mesher could not draw a shape that reaches
	/// across a cell boundary, and then into the neighbouring chunk, anyway.
	///
	/// **Why this is hand-emitted.** `piston_side` is not a picture of a block face; it is a picture of
	/// the piston's *length*, read along the push direction. Its top four rows are the arm and the twelve
	/// under them are the body, and every shared plane builder in this file derives a face's texture
	/// coordinates from that face's own axis-aligned coordinates -- which puts the arm band along the
	/// *top* of a sideways piston's flank, where nothing is, instead of along the *front edge*, which is
	/// where the arm has to come out. So the family is built in an explicit frame.
	///
	/// **The frame.** `d` is the push direction and `(l, t, d)` is a right-handed signed permutation of
	/// the three axes (`l x t == d`), so a frame point (u, v, a) with all three in 0..16 lands in the
	/// cell's own 0..16 cube exactly once and nothing is mirrored -- provided the box's centre, frame
	/// (8, 8, 8), is anchored on the cell's centre, which makeVert does and says why. Each orientation
	/// takes its `(l, t)` from a six-row table rather than from a chain of sign tests, which is what
	/// keeps six cases from becoming six chances to swap two axes.
	///
	/// **The sprite window is the along-push range.** A box is a triple of ranges, and the along-push
	/// range carries two extra numbers: the sprite row shown at the box's *back* edge and the one shown
	/// at its *front* edge. A box says which slice of the sprite it wears by saying how deep it is, and
	/// on the body pieces those two numbers are 1:1 with world pixels, so nothing there stretches the
	/// art. That one rule is what puts the arm band on the front-most 4 px of the whole assembly and
	/// nowhere else:
	///
	///   retracted base   a  0..16   rows 16 -> 0    the cube; band across its front 4 px, body behind
	///   extended body    a  0..12   rows 16 ->  4    the 12 px the head leaves behind; body only
	///   extended rod     a 12..16   rows  4 -> 3    the 4 px of rod standing in the body's recess
	///   head rod         a  0..12   rows  3 -> 0    the rod's other 12 px, back to the base's stub
	///   head plate       a 12..16   rows  4 -> 0    the pushing plate; the arm band, where vanilla has it
	///
	/// The three boxes that do not read 1:1 are the two rods and the plate, and they are the three where
	/// vanilla does not either. A rod is 12 px long in a cell whose length is 16, so it cannot inherit
	/// the cell's window without dragging the body art down it; vanilla instead maps the *arm band* --
	/// uv [0,0,16,4] -- across the rod's whole length, one sprite row per four world pixels, and this
	/// port's two halves do the same (base rows 4 -> 3, head rows 3 -> 0, so one row per 4 px on each
	/// side of the cell boundary and the band meets itself at the seam). The plate's rows 4 -> 0 are
	/// 1:1, and are the arm band's own four rows.
	///
	/// **The rod's cross-section is 4x4 and is written as 6..10 everywhere.** Vanilla's piston_head rod
	/// is the element `from [6,6,4] to [10,10,16]`, and both halves here pass u and v as 6..10 on all
	/// four of their flanks. Worth stating plainly because the frame puts a box's *corner* at 0 and at
	/// 16, so a rod written 4..12 is the same shape at twice the diameter with nothing to show for it:
	/// the extents, the cap UV rectangles and the window ratios are all exactly as legal at 8 px across
	/// as at 4, and only the picture knows. tools/check-piston-mesh.py checks the diameter for that
	/// reason -- this was live until 2026-09-27 and no other check in the tree could see it.
	if (blockType->blockModel == 17 || blockType->blockModel == 18)
	{
		const bool isHead = (blockType->blockModel == 18);
		// Only the base has two shapes behind this model id, so this is only ever read for the base.
		const bool extended = IsPistonExtended(Block);
		const int facing = PistonFacingFromState(GetBlockSettingsNoCheck((int)x, (int)y, (int)z) & OpRedstonePowerMask);

		// Mid-stroke, if this cell belongs to a piston that is moving. A base reports its own stroke and
		// draws the head itself at the fraction of the way out it has reached; a head reports the stroke
		// that owns it and stands down, because the pair is one moving part and only the base cell can
		// draw the rod between them. See StartPistonStroke for why the animation is built on top of a
		// world whose ids have already moved.
		float strokeProgress = 0.0f;
		bool strokeExtending = false;
		const bool strokeHere = GetPistonStroke((int)x, (int)y, (int)z, isHead, facing, &strokeProgress, &strokeExtending);

		// l and t across the push, d along it, with l x t == d. Spelled out per facing because the compact
		// alternative -- take the next axis in the cycle, then correct for the sign of d -- is four lines
		// that have to be re-derived correctly every time they are read.
		//
		// Read as six unit vectors, in facing order, so the invariant can be checked by eye:
		//   d  = -Y +Y -X +X -Z +Z
		//   l  = +X +X +Z +Y +Y +X
		//   t  = +Z -Z +Y +Z +X +Y
		// and l x t == d holds in all six. The +X row is the one that is *not* the table's original
		// mirror of -X: it read l = (0,1,1) -- l x t = -d and |l| != 1, so the frame was neither
		// orthonormal nor right-handed, and a frame point (u,v,a) landed on y = u+v, i.e. a sheared slab
		// sixteen pixels taller than its own cell. Only +X was affected, which is a quarter of the
		// pistons a player can place, and it is the reason tools/check-piston-mesh.py exists.
		static const int kPistonLatX[6] = { 1, 1, 0, 0, 0, 1};
		static const int kPistonLatY[6] = { 0, 0, 0, 1, 1, 0};
		static const int kPistonLatZ[6] = { 0, 0, 1, 0, 0, 0};
		static const int kPistonTanX[6] = { 0, 0, 0, 0, 1, 0};
		static const int kPistonTanY[6] = { 0, 0, 1, 0, 0, 1};
		static const int kPistonTanZ[6] = { 1,-1, 0, 1, 0, 0};

		const float dx = (float)PistonDirX(facing), dy = (float)PistonDirY(facing), dz = (float)PistonDirZ(facing);
		const float lx = (float)kPistonLatX[facing], ly = (float)kPistonLatY[facing], lz = (float)kPistonLatZ[facing];
		const float tx = (float)kPistonTanX[facing], ty = (float)kPistonTanY[facing], tz = (float)kPistonTanZ[facing];

		// Four roles. All four fields that hold the side tile hold the same one and the mesher reads just
		// one of them, because the flanks are emitted from a single window rather than per world face.
		// The head tile is the 16x16 pushing face wherever it appears: the retracted cube's front cap and
		// both faces of the head's plate. `botPlane` is the base's back, against whatever the piston is
		// mounted on, which is vanilla's piston_bottom; `innerPlane` is the face at the bottom of an
		// extended base's recess -- vanilla's piston_inner, stone with the rod's hole drawn through it --
		// and it is the only one of the four the head does not have.
		const int sideTileX = blockType->upPlane_x,  sideTileY = blockType->upPlane_y;   // piston_side
		const int faceTileX = blockType->forPlane_x, faceTileY = blockType->forPlane_y;  // the head tile
		const int backTileX = blockType->botPlane_x, backTileY = blockType->botPlane_y;  // piston_bottom
		const int innerTileX = blockType->innerPlane_x, innerTileY = blockType->innerPlane_y;  // piston_inner

		// One vertex: a frame point and a pixel inside the tile in, a position and a texture coordinate
		// out. The two texture values are pixels inside the *tile*, 0..16, so every caller states its own
		// window instead of inheriting one -- the same contract the lever's makeVert has, and the reason
		// the along-push window can be stated per box at all.
		struct PistonVert { float x, y, z, u, v; };
		auto makeVert = [&](int tileX, int tileY, float pu, float pv, float pa, float su, float sv)
		{
			// The frame's box is 0..16 on all three axes, so its centre is frame (8, 8, 8), and
			// anchoring *that* on the cell's centre is what lands the box in this cell for every
			// orientation. The -8 belongs here rather than folded into the table: it is the same eight
			// on every axis whatever the signs are. Anchoring on the cell's corner instead -- x, y, z
			// with no shift -- is also correct while every axis of (l, t, d) is positive, which is
			// exactly three of the six facings, and emits the other three (down, west and north)
			// wholly inside the neighbouring block, the one the piston is mounted on.
			const float fu = pu - 8.0f, fv = pv - 8.0f, fa = pa - 8.0f;
			PistonVert vert;
			vert.x = x + 0.5f + (lx*fu + tx*fv + dx*fa) * pixel;
			vert.y = y + 0.5f + (ly*fu + ty*fv + dy*fa) * pixel;
			vert.z = z + 0.5f + (lz*fu + tz*fv + dz*fa) * pixel;
			vert.u = percent * ((float)tileX + su / 16.0f);
			vert.v = percent * ((float)tileY + sv / 16.0f);
			return vert;
		};

		// Emit a quad, correcting the corner order against the direction the face looks. Not tidy so much
		// as necessary: the frame is rotated differently for each orientation, so a corner order that
		// winds correctly on an upward piston is the mirror of the one that winds correctly on a sideways
		// one. Reversing the four corners keeps the same quad and flips the winding.
		//
		// The brightness is the same five constants the shared builder uses -- full on top, 0.6 beneath,
		// 0.65 on the X faces, 0.85 on the Z faces -- sampled from the cell the face looks into. Smooth
		// lighting and the corner term are deliberately not reproduced: those average eight neighbours
		// per vertex to round off the shading of a full cube, and the faces here that are ever seen up
		// close are the four narrow flanks of a 4 px plate, which a flat value is right for.
		auto emitFace = [&](float nx, float ny, float nz, const PistonVert q[4])
		{
			PistonVert a = q[0], b = q[1], c = q[2], d = q[3];
			const float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
			const float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
			if((e1y*e2z - e1z*e2y)*nx + (e1z*e2x - e1x*e2z)*ny + (e1x*e2y - e1y*e2x)*nz < 0.0f)
			{
				PistonVert tmp = b; b = d; d = tmp;
			}

			float light = BlockFinalLight(x + (int)nx, y + (int)ny, z + (int)nz);
			if(ny < 0.0f)       light *= 0.6f;
			else if(nx != 0.0f) light *= blockXConstLight;
			else if(nz != 0.0f) light *= blockZConstLight;

			MeshChunk->vert(a.x, a.y, a.z, a.u, a.v, light, light, light);
			MeshChunk->vert(b.x, b.y, b.z, b.u, b.v, light, light, light);
			MeshChunk->vert(c.x, c.y, c.z, c.u, c.v, light, light, light);
			MeshChunk->vert(d.x, d.y, d.z, d.u, d.v, light, light, light);
			MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
			MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);
			iVertex += 4;
		};

		// The four flanks of a box: the faces whose normal is across the push, and so the only ones that
		// can carry the sprite's along-push axis. `su` is the across coordinate of the axis the face
		// spans, running left to right *as seen from outside the cell* -- that is what stops the arm band
		// from being mirrored on half the orientations, which is the one mistake here a player would see
		// and report as "the piston is inside out". `sv` interpolates the two stated sprite rows so the
		// mapping stays 1:1 however deep the box is.
		//
		// This is for the three boxes that are 16 px across -- the retracted cube, the base's body and the
		// head's plate -- and it is the *body's* mapping: the sprite's rows run along the push, so the arm
		// band lies across the front 4 px and the body fills the twelve behind it. The rod is 4 px across
		// and wears the tile the other way round, which is what emitRodFlanks is for.
		//
		// The across window is the whole tile, hence the 0 and 16 constants below rather than something
		// read off v0 and v1: the sprite's 16 columns are not a length but the box's cross-section, drawn
		// with grey at both edges and grey again down the middle, which is why the tile is symmetric about
		// its centre. Reading the window off the geometry instead -- as this did until 2026-09-27 -- would
		// only ever show up on a box that is not 16 px across, and after that date there is no such box
		// here. It was not a harmless indirection: it is what a 4 px rod was reading them from.
		//
		// Every face is emitted unconditionally. Culling the ones flush with an opaque neighbour would
		// save a handful of vertices per piston and would also be the one place a slip shows up as a hole
		// in the world, so the depth buffer is left to do that work: a hidden face here is a face drawn
		// and discarded, not a face drawn and seen.
		auto emitFlanks = [&](float u0, float u1, float v0, float v1, float a0, float a1, float svBack, float svFront)
		{
			const float svStep = (a1 > a0) ? (svFront - svBack) / (a1 - a0) : 0.0f;
			auto svAt = [&](float a) { return svBack + (a - a0) * svStep; };
			PistonVert q[4];

			// +l. Across is t, and su counts down as t rises, so the sprite's column 0 sits at v0.
			q[0] = makeVert(sideTileX, sideTileY, u1, v0, a0, 16.0f, svAt(a0));
			q[1] = makeVert(sideTileX, sideTileY, u1, v0, a1, 16.0f, svAt(a1));
			q[2] = makeVert(sideTileX, sideTileY, u1, v1, a1,  0.0f, svAt(a1));
			q[3] = makeVert(sideTileX, sideTileY, u1, v1, a0,  0.0f, svAt(a0));
			emitFace(lx, ly, lz, q);

			// -l.
			q[0] = makeVert(sideTileX, sideTileY, u0, v0, a0,  0.0f, svAt(a0));
			q[1] = makeVert(sideTileX, sideTileY, u0, v0, a1,  0.0f, svAt(a1));
			q[2] = makeVert(sideTileX, sideTileY, u0, v1, a1, 16.0f, svAt(a1));
			q[3] = makeVert(sideTileX, sideTileY, u0, v1, a0, 16.0f, svAt(a0));
			emitFace(-lx, -ly, -lz, q);

			// +t. Across is l, and su counts up with it.
			q[0] = makeVert(sideTileX, sideTileY, u0, v1, a0,  0.0f, svAt(a0));
			q[1] = makeVert(sideTileX, sideTileY, u0, v1, a1,  0.0f, svAt(a1));
			q[2] = makeVert(sideTileX, sideTileY, u1, v1, a1, 16.0f, svAt(a1));
			q[3] = makeVert(sideTileX, sideTileY, u1, v1, a0, 16.0f, svAt(a0));
			emitFace(tx, ty, tz, q);

			// -t. The mirror of +l's arrangement, for the same reason.
			q[0] = makeVert(sideTileX, sideTileY, u0, v0, a0, 16.0f, svAt(a0));
			q[1] = makeVert(sideTileX, sideTileY, u0, v0, a1, 16.0f, svAt(a1));
			q[2] = makeVert(sideTileX, sideTileY, u1, v0, a1,  0.0f, svAt(a1));
			q[3] = makeVert(sideTileX, sideTileY, u1, v0, a0,  0.0f, svAt(a0));
			emitFace(-tx, -ty, -tz, q);
		};

		// The four flanks of the rod, which wear the tile the other way round from every other box here:
		// the sprite's 16 columns run *along* the arm and its 4 rows run *across* it. That is what
		// vanilla's piston_head says with rotation 90 on the rod's down face and 270 on its up face, and
		// the reason is what those 16 columns are: not a length but the arm's cross-section drawn as a
		// band -- grey, tan, grey, tan, grey, symmetric about the tile's centre. Reading them across a
		// 4 px rod, as emitFlanks does and as this did until 2026-09-27, puts three grey stripes down a
		// face the width of a fingernail; leaving the window at the tile's middle four columns, which is
		// what reading it off the geometry did, puts one down the middle. Vanilla's arm is two tan bands
		// with a grey band between them, running across the arm -- which is also what the retracted cube
		// shows on its front 4 px and the plate on its edge, since those two read the same band down
		// their length and this reads it around their width.
		//
		// `uBack` and `uFront` are the sprite columns at a0 and a1, and they are not free: the arm is
		// 16 px long and the sprite's 16 columns map onto it 1:1, column 0 at the plate's back face and
		// 16 at the far end, 4 px inside the base. Splitting that run at the cell boundary -- 12 px on
		// the head's rod, 4 on the base's stub -- is what keeps the bands continuous across the seam.
		//
		// The across direction is emitFlanks' rule applied to the other axis: row 0 sits at the edge that
		// comes first reading left to right from outside the cell, so the band's light rows land on the
		// same side of the rod on all four faces rather than spiralling round it.
		auto emitRodFlanks = [&](float l0, float l1, float t0, float t1, float a0, float a1,
		                         float uBack, float uFront)
		{
			const float uStep = (a1 > a0) ? (uFront - uBack) / (a1 - a0) : 0.0f;
			auto uAt = [&](float a) { return uBack + (a - a0) * uStep; };
			PistonVert q[4];

			// +l. Across is t, and viewed from outside, left to right runs t1 -> t0, so row 0 is at t1.
			q[0] = makeVert(sideTileX, sideTileY, l1, t0, a0, uAt(a0), 4.0f);
			q[1] = makeVert(sideTileX, sideTileY, l1, t0, a1, uAt(a1), 4.0f);
			q[2] = makeVert(sideTileX, sideTileY, l1, t1, a1, uAt(a1), 0.0f);
			q[3] = makeVert(sideTileX, sideTileY, l1, t1, a0, uAt(a0), 0.0f);
			emitFace(lx, ly, lz, q);

			// -l, where the same view reads t0 -> t1.
			q[0] = makeVert(sideTileX, sideTileY, l0, t0, a0, uAt(a0), 0.0f);
			q[1] = makeVert(sideTileX, sideTileY, l0, t0, a1, uAt(a1), 0.0f);
			q[2] = makeVert(sideTileX, sideTileY, l0, t1, a1, uAt(a1), 4.0f);
			q[3] = makeVert(sideTileX, sideTileY, l0, t1, a0, uAt(a0), 4.0f);
			emitFace(-lx, -ly, -lz, q);

			// +t. Across is l, and left to right runs l0 -> l1.
			q[0] = makeVert(sideTileX, sideTileY, l0, t1, a0, uAt(a0), 0.0f);
			q[1] = makeVert(sideTileX, sideTileY, l0, t1, a1, uAt(a1), 0.0f);
			q[2] = makeVert(sideTileX, sideTileY, l1, t1, a1, uAt(a1), 4.0f);
			q[3] = makeVert(sideTileX, sideTileY, l1, t1, a0, uAt(a0), 4.0f);
			emitFace(tx, ty, tz, q);

			// -t, where it runs l1 -> l0.
			q[0] = makeVert(sideTileX, sideTileY, l0, t0, a0, uAt(a0), 4.0f);
			q[1] = makeVert(sideTileX, sideTileY, l0, t0, a1, uAt(a1), 4.0f);
			q[2] = makeVert(sideTileX, sideTileY, l1, t0, a1, uAt(a1), 0.0f);
			q[3] = makeVert(sideTileX, sideTileY, l1, t0, a0, uAt(a0), 0.0f);
			emitFace(-tx, -ty, -tz, q);
		};

		// A cap: a box's face perpendicular to the push. Its plane is across the push, so the sprite
		// cannot run along it the way it runs along a flank, and it gets the plain "row 0 at the top of
		// the face, column 0 at the left, as seen from outside the cell" mapping instead -- derived from
		// the vertex's world position, so it holds for all six orientations without a table.
		//
		// The horizontal pair is the one case that needs care, and the reason is easy to miss: the two
		// horizontal coordinates are the same two the *vertical* faces spend on rows, so a face looking
		// up cannot take its row from y the way a face looking sideways does -- y does not vary across a
		// horizontal face at all. Doing it anyway samples one sprite row across the whole face, which is
		// what an up-facing piston's front cap looked like before this: a smear, on the one face a
		// player placing a piston on the ground looks at first. A face looking down and a face looking
		// up are the same face seen from opposite sides, so the up one is the down one under a half turn
		// about X: same su, and sv mirrored in z rather than taken from y.
		auto emitCap = [&](float u0, float u1, float v0, float v1, float a, float nx, float ny, float nz,
		                   int tileX, int tileY)
		{
			PistonVert q[4];
			const float cu[4] = { u0, u0, u1, u1 };
			const float cv[4] = { v0, v1, v1, v0 };
			for(int c = 0; c < 4; c++)
			{
				// The vertex's position within the cell, 0..16, which is what "derived from the vertex's
				// world position" has to mean here. The frame coordinates cu/cv/a are 0..16 too, but they
				// run along signed axes: on the three facings whose push, lateral or tangent axis is
				// negative, a raw frame coordinate is the *negation* of the world one, so reading a UV
				// straight off it lands outside the tile. Anchoring on the cell centre first -- the same
				// -8 that makeVert applies -- is what turns the frame coordinate into the world one, and
				// on the facings where all three axes are positive it is the identity, so nothing that
				// was right before moves.
				const float fu = cu[c] - 8.0f, fv = cv[c] - 8.0f, fa = a - 8.0f;
				const float Wx = 8.0f + (lx*fu + tx*fv + dx*fa);
				const float Wy = 8.0f + (ly*fu + ty*fv + dy*fa);
				const float Wz = 8.0f + (lz*fu + tz*fv + dz*fa);

				float su, sv;
				if(ny != 0.0f)
				{
					su = Wx;
					sv = (ny < 0.0f) ? Wz : (16.0f - Wz);
				}
				else
				{
					if(nx > 0.0f)       su = Wz;
					else if(nx < 0.0f)  su = 16.0f - Wz;
					else if(nz > 0.0f)  su = Wx;
					else                su = 16.0f - Wx;
					sv = 16.0f - Wy;
				}

				q[c] = makeVert(tileX, tileY, cu[c], cv[c], a, su, sv);
			}
			emitFace(nx, ny, nz, q);
		};

		// The head -- the 4 px plate and the 12 px rod behind it -- drawn in this cell's frame, with `off`
		// added to every along-push coordinate. `off` is 0 for a head standing in its own cell. A stroke
		// draws the same box pair from the *base* cell instead, and there `off` runs the whole way from 0
		// -- the plate over the base's recess, which is where the retracted cube's front 4 px are -- to a
		// full cell of 16 px, which is the plate at the far end of the head's own cell, one cell along the
		// push. One box pair, one slide of one cell: that is what makes the arm read as extending rather
		// than as a swap between two models, and the slide has to be the full cell because the two cells
		// are the two places the resting mesher draws the head. A slide of 16 minus something stops the
		// head short of its own cell and snaps the rest of the way when the stroke ends.
		//
		// `sliding` says which of the two callers this is, because the plate's back cap has one caller
		// it may not be emitted for: at the start of a slide the plate's back face lands exactly on the
		// base's recess floor, the one plane in this mesh it can z-fight with.
		auto emitHead = [&](float off, bool sliding)
		{
			// The plate: the arm band's own four rows across its edge, and the pushing face on the front.
			// Its caps take their window from the vertex's world position, which does not move when the
			// plate slides -- the plate is 16x16 across the push, so the tile travels with it and does
			// not smear -- so the derivation below is unchanged by `off`.
			//
			// The shading emitFace samples is the emitting cell's neighbours, and a stroke draws this from
			// the base cell, so a moving head is lit by the cells around its base rather than by its own.
			// It is a one-cell error on a part that is only ever lit by daylight or by a lamp that has to
			// already cover both cells to be seen at all, and it lasts a tenth of a second.
			emitFlanks(0.0f, 16.0f, 0.0f, 16.0f, 12.0f + off, 16.0f + off, 4.0f, 0.0f);
			emitCap(0.0f, 16.0f, 0.0f, 16.0f, 16.0f + off,  dx,  dy,  dz, faceTileX, faceTileY);

			// The back cap is dropped for as long as the plate's back face is on the base's recess floor,
			// which is only ever the first pose of a slide -- at `off` of 0 the floor's quad and this one
			// are one plane, and two quads on one plane is a z-fight. Nothing is lost by the drop: the
			// plate's back face is behind the plate itself from every direction it can be looked at, and
			// the floor is the piece worth keeping, because it is also what is there if the head is gone.
			if(sliding == false || off > 0.0f)
			{
				emitCap(0.0f, 16.0f, 0.0f, 16.0f, 12.0f + off, -dx, -dy, -dz, backTileX, backTileY);
			}

			// The rod, 4x4 px, centred in the cell: 12 px long and *rigid*, so it keeps its length and
			// travels with the plate, which is what vanilla's piston_head model does -- its rod is a
			// fixed 4x4x12 box and the whole model is the thing that moves.
			//
			// Telescoping it instead, so that it would span from the base's mouth to the plate at every
			// pose and be 28 px at the end, looks identical -- the 16 px it gains are inside the base's
			// body, in front of the mouth -- and costs two things: the columns past the end of the tile
			// that the sprite has to be sampled at, and a second set of flanks lying exactly on the
			// base's rod stub's for the middle of the stroke. Neither is worth a second way of stating
			// the arm's length, so there is one.
			//
			// Neither of the rod's own caps is emitted. The one at the back would be the arm's opening,
			// which is inside the base's cube at the start of a slide and behind the plate at the end of
			// it, and the one at the front would be coplanar with the plate's back face. Two quads on one
			// plane is a z-fight, and neither can be seen from outside the assembly.
			//
			// The columns run 1:1 with world pixels along the arm, counting *down* from the plate's back
			// face -- column 0 there, column 12 where the base's stub takes over -- so the arm's grey and
			// tan bands stay continuous across the cell boundary at the resting poses, and the sprite is
			// never sampled outside its own tile.
			emitRodFlanks(6.0f, 10.0f, 6.0f, 10.0f, off, 12.0f + off, 12.0f, 0.0f);
		};

		if(isHead)
		{
			// The head lives in the cell the piston pushes into. Its plate is at the far end of that cell,
			// which makes it the front-most 4 px of the whole assembly -- the slice of the sprite that
			// carries the arm band -- and its rod runs back to meet the base's.
			//
			// While its base is mid-stroke it draws nothing at all: the two halves are one moving part,
			// the base is already drawing both, and a head drawn here as well would be a second head left
			// standing in the cell the first one is leaving.
			if(strokeHere == false)
			{
				emitHead(0.0f, false);
			}
		}
		else if(extended)
		{
			// The base with its head out. What is left of the piston in its own cell is a 12 px body --
			// the whole 16x16 face, four pixels shy of the cell's far wall -- plus the 4 px of rod that
			// stands in the opening the body left. That opening is the recess: 4 px wide, 16 x 16 across,
			// with the rod through the middle of it, and the face at the bottom of it is the one vanilla
			// paints with piston_inner.
			//
			// This is the shape that has been wrong twice. It is worth stating what it is not: it is not
			// a 4 px plate against the back wall with the rod cantilevered out of it. That reading makes
			// the base a thin disc and hands two thirds of the piston's body to the rod, so an extended
			// piston reads as two plates on a stick rather than as a piston -- and it also puts the
			// collider (PistonCellBox) and the mesher on the wrong side of the cell together, which is
			// why the audit compares the two.
			//
			// The body's flanks wear the sprite's *body* rows at 1:1: rows 16 -> 4 over the 12 px, so the
			// arm band's four rows are exactly the four the body does not cover. Those four are the rod's,
			// and the head's, the same four the retracted cube shows across its front 4 px.
			//
			// This is also the shape an *extending* piston wears from the first frame of its stroke, which
			// is what Java does: the base has already become the extended block, and the head is drawn
			// sliding out of the recess the body leaves rather than out of a closed cube.
			emitFlanks(0.0f, 16.0f, 0.0f, 16.0f, 0.0f, 12.0f, 16.0f, 4.0f);
			emitCap(0.0f, 16.0f, 0.0f, 16.0f, 12.0f,  dx,  dy,  dz, innerTileX, innerTileY);
			emitCap(0.0f, 16.0f, 0.0f, 16.0f, 0.0f, -dx, -dy, -dz, backTileX, backTileY);

			// The rod's back cap is left out for the same coplanar reason as the head's, and this time its
			// front cap is emitted -- it sits on the cell boundary with nothing beyond it but the head's
			// rod, whose matching cap is the one that was dropped. Not while a stroke is running, though:
			// then the plate is somewhere between this cap and the far end of the next cell, and at the
			// start of the travel it lies exactly on it. The head's rod is in front of this cap for the
			// whole stroke, so dropping it costs nothing a player can see.
			emitRodFlanks(6.0f, 10.0f, 6.0f, 10.0f, 12.0f, 16.0f, 16.0f, 12.0f);
			if(strokeHere == false)
			{
				emitCap(6.0f, 10.0f, 6.0f, 10.0f, 16.0f, dx, dy, dz, sideTileX, sideTileY);
			}
		}
		else
		{
			// Retracted: the full cube, which is the state a piston stands in whenever it is idle and the
			// one the player reads as "piston". The sprite's rows run the length of the cube, so the arm
			// band lies across the front 4 px and the body fills the twelve behind it -- which is the
			// whole point of building the flanks in this frame, and what the shared builders cannot do.
			//
			// This is also what a *retracting* piston wears for its whole stroke, again as Java does:
			// the cube is back the moment the head lets go of its cell, and the arm is what travels.
			emitFlanks(0.0f, 16.0f, 0.0f, 16.0f, 0.0f, 16.0f, 16.0f, 0.0f);
			emitCap(0.0f, 16.0f, 0.0f, 16.0f, 16.0f,  dx,  dy,  dz, faceTileX, faceTileY);
			emitCap(0.0f, 16.0f, 0.0f, 16.0f, 0.0f, -dx, -dy, -dz, backTileX, backTileY);
		}

		// The moving half, drawn by the base. `progress` of 1 leaves the head where the resting mesher
		// puts it -- one cell out, plate at the far end of the head's cell -- and 0 leaves the plate
		// covering the recess, which makes the pair pixel-for-pixel the retracted cube. Everything
		// between is the slide, and the slide is a whole cell: the head is drawn from the base cell, so
		// it needs the cell's own 16 px of travel added to its offset before `progress` is applied. A
		// stroke only ever sets this on the base: `strokeHere` on a head has already returned.
		if(strokeHere && isHead == false)
		{
			emitHead(strokeProgress * 16.0f, true);
		}
		return;
	}

	/// Fire: Java-style crossed sprites, plus extra planes hugging ignitable neighbors.
	if (blockType->blockModel == 13)
	{
		const float u0 = blockType->leftPlane_x * percent;
		const float u1 = u0 + percent;
		const float v0 = blockType->leftPlane_y * percent;
		const float v1 = v0 + percent;
		const float inset = pixel;

		auto emit = [&](float x0, float y0, float z0, float x1, float y1, float z1,
		                float x2, float y2, float z2, float x3, float y3, float z3)
		{
			// Same V mapping as flowers: atlas top (v0) on world top, atlas bottom (v1) on world bottom.
			MeshChunk->vert(x0, y0, z0, u1, v1, 1, 1, 1);
			MeshChunk->vert(x1, y1, z1, u1, v0, 1, 1, 1);
			MeshChunk->vert(x2, y2, z2, u0, v0, 1, 1, 1);
			MeshChunk->vert(x3, y3, z3, u0, v1, 1, 1, 1);
			MeshChunk->triangle(iVertex, iVertex + 1, iVertex + 2);
			MeshChunk->triangle(iVertex + 2, iVertex + 3, iVertex);
			iVertex += 4;

			MeshChunk->vert(x0, y0, z0, u1, v1, 1, 1, 1);
			MeshChunk->vert(x3, y3, z3, u0, v1, 1, 1, 1);
			MeshChunk->vert(x2, y2, z2, u0, v0, 1, 1, 1);
			MeshChunk->vert(x1, y1, z1, u1, v0, 1, 1, 1);
			MeshChunk->triangle(iVertex, iVertex + 1, iVertex + 2);
			MeshChunk->triangle(iVertex + 2, iVertex + 3, iVertex);
			iVertex += 4;
		};

		const bool ignXm = BlockIsIgnitable(_GetBlockNoCheck(x - 1, y, z));
		const bool ignXp = BlockIsIgnitable(_GetBlockNoCheck(x + 1, y, z));
		const bool ignZm = BlockIsIgnitable(_GetBlockNoCheck(x, y, z - 1));
		const bool ignZp = BlockIsIgnitable(_GetBlockNoCheck(x, y, z + 1));
		const bool ignYp = BlockIsIgnitable(_GetBlockNoCheck(x, y + 1, z));
		const bool floorFire = BlockSolid(x, y - 1, z) || BlockIsIgnitable(_GetBlockNoCheck(x, y - 1, z));
		const bool anyClip = ignXm || ignXp || ignZm || ignZp || ignYp;

		if(floorFire || !anyClip)
		{
			emit(x,     y, z + 1, x,     y + 1, z + 1, x + 1, y + 1, z,     x + 1, y, z);
			emit(x,     y, z,     x,     y + 1, z,     x + 1, y + 1, z + 1, x + 1, y, z + 1);
		}
		if(ignXm)
			emit(x + inset, y, z,     x + inset, y + 1, z,     x + inset, y + 1, z + 1, x + inset, y, z + 1);
		if(ignXp)
			emit(x + 1 - inset, y, z + 1, x + 1 - inset, y + 1, z + 1, x + 1 - inset, y + 1, z, x + 1 - inset, y, z);
		if(ignZm)
			emit(x + 1, y, z + inset, x + 1, y + 1, z + inset, x,     y + 1, z + inset, x,     y, z + inset);
		if(ignZp)
			emit(x,     y, z + 1 - inset, x,     y + 1, z + 1 - inset, x + 1, y + 1, z + 1 - inset, x + 1, y, z + 1 - inset);
		if(ignYp)
			emit(x, y + 1 - inset, z, x + 1, y + 1 - inset, z, x + 1, y + 1 - inset, z + 1, x, y + 1 - inset, z + 1);
		return;
	}

	/// Nether portal: one flat quad in the middle of the cell, lying in the plane of its frame.
	///
	/// This replaces a plain cube (blockModel 0). A cube gave the portal six independently lit faces,
	/// so the interior read as a patchwork of 1x1, 2x2 and 3x3 block faces with a seam at every cell
	/// boundary where the tile repeated and the neighbour lighting changed -- "chunky", and precisely
	/// what the inside of a frame should not look like. One quad per cell, emitted unlit as (1,1,1),
	/// tiles seamlessly across the whole interior and reads as the continuous sheet vanilla draws.
	///
	/// Which way the quad faces comes from the block id and not from the neighbours: the mesher is
	/// handed an id and nothing else, and inferring the axis from adjacent cells would pick wrong on a
	/// portal lit flat against a wall or standing next to a second portal.
	if (blockType->blockModel == 30)
	{
		const float u0 = blockType->leftPlane_x * percent;
		const float u1 = u0 + percent;
		const float v0 = blockType->leftPlane_y * percent;
		const float v1 = v0 + percent;

		// Both windings, so the sheet is solid from either side of the frame. Same V mapping as fire,
		// where the atlas top is world top.
		auto emit = [&](float ax, float ay, float az,
		                float bx, float by, float bz,
		                float cx, float cy, float cz,
		                float dx, float dy, float dz)
		{
			MeshChunk->vert(ax, ay, az, u1, v1, 1, 1, 1);
			MeshChunk->vert(bx, by, bz, u1, v0, 1, 1, 1);
			MeshChunk->vert(cx, cy, cz, u0, v0, 1, 1, 1);
			MeshChunk->vert(dx, dy, dz, u0, v1, 1, 1, 1);
			MeshChunk->triangle(iVertex, iVertex + 1, iVertex + 2);
			MeshChunk->triangle(iVertex + 2, iVertex + 3, iVertex);
			iVertex += 4;

			MeshChunk->vert(ax, ay, az, u1, v1, 1, 1, 1);
			MeshChunk->vert(dx, dy, dz, u0, v1, 1, 1, 1);
			MeshChunk->vert(cx, cy, cz, u0, v0, 1, 1, 1);
			MeshChunk->vert(bx, by, bz, u1, v0, 1, 1, 1);
			MeshChunk->triangle(iVertex, iVertex + 1, iVertex + 2);
			MeshChunk->triangle(iVertex + 2, iVertex + 3, iVertex);
			iVertex += 4;
		};

		const float h = 0.5f;
		if(Block == PortalBlockZ::getID())
			// The frame runs along Z: the sheet spans Z and Y, centred in X.
			emit(x + h, y,     z,     x + h, y + 1, z,
			     x + h, y + 1, z + 1, x + h, y,     z + 1);
		else
			// The frame runs along X: the sheet spans X and Y, centred in Z.
			emit(x,     y,     z + h, x,     y + 1, z + h,
			     x + 1, y + 1, z + h, x + 1, y,     z + h);
		return;
	}
}

void CraftWorld::GetNormalBlock(int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent)
{
    Block = _GetBlockNoCheck(x,y,z);
    BaseBlock *blockType = &blockTypes[Block];

    int yy = y;
    int zz = z;

    bool anotherLight = false;
    int side = -1;
    if(Block == FurnaceOff::getID() || Block == FurnaceOn::getID())
    {
        int o;
        o = -1;
        for(unsigned int i = 0; i < mFurnaces.size(); i++)
        {
            Furnace* NewFurnace = mFurnaces[i];
            if(NewFurnace->furnaceX == x && NewFurnace->furnaceY == y && NewFurnace->furnaceZ == z)
            {
                side = NewFurnace->GetSide();
                break;
            }
        }
    }

    bool canCreate = false;
    bool DefaultBlock = false;
    bool transparentBlock = false;
    char specialBlock = 0;

    float up = 0.0f;
    float down = 0.0f;
    float left = 0.0f;
    float right = 0.0f;

    block_t Block1 = 0;
    BaseBlock *blockType1;

    float light[20];
    for(int o = 0; o <= 19; o++)
    {
        light[o] = -1;
    }
    bool onCorners = false;
    if(x == 0 || y == 0 || z == 0 || x == worldLimit-1 || y == WORLD_HEIGHT-1 || z == worldLimit-1)
    {
        onCorners = true;
    }

    Vector3 light1, light2, light3, light4;

    Vector3 BlockColorx1;
    onCorners == true ? Block1 = _GetBlock(x-1,y,z) : Block1 = _GetBlockNoCheck(x-1,y,z);
    blockType1 = &blockTypes[Block1];

    transparentBlock = blockType1->transparent;
    specialBlock = blockType1->blockModel;
    if(transparent == true)
    {
        if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112))
        {
            canCreate = true;
        }
        if (transparentBlock == false && specialBlock == 0)
        {
            canCreate = false;
        }
    }
    else
    {
        if (specialBlock > 0 && Block1 != Block)
        {
            canCreate = true;
        }
        if (transparentBlock == true || Block1 == 0)
        {
            canCreate = true;
        }
    }

    if(mainOptions.fastRendering)
    {
        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            canCreate = false;
        }
    }

    if(canCreate == true)
    {
        float mainLight = BlockFinalLight(x-1,y,z);
        BlockColorx1 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;
        if(westLight != 0)
        {
            BlockColorx1.x += 0.3f*westLight;
            BlockColorx1.y += 0.2f*westLight;
        }

        if (BlockColorx1.x > 1.0f)
        {
            BlockColorx1.x = 1.0f;
        }
        if (BlockColorx1.y > 1.0f)
        {
            BlockColorx1.y = 1.0f;
        }
        light1 = light2 = light3 = light4 = BlockColorx1;

        if(mainOptions.smoothLighting == true)
        {
            if(onCorners)
            {
                light[0] = BlockFinalLight(x-1,y+1,z) ;
                light[1] = BlockFinalLight(x-1,y,z+1) ;
                light[2] = BlockFinalLight(x-1,y+1,z+1) ;
                light[3] = BlockFinalLight(x-1,y,z-1) ;
                light[4] = BlockFinalLight(x-1,y+1,z-1) ;
                light[5] = BlockFinalLight(x-1,y-1,z) ;
                light[6] = BlockFinalLight(x-1,y-1,z+1) ;
                light[7] = BlockFinalLight(x-1,y-1,z-1) ;
            }
            else
            {
                light[0] = BlockFinalLightNoCheck(x-1,y+1,z) ;
                light[1] = BlockFinalLightNoCheck(x-1,y,z+1) ;
                light[2] = BlockFinalLightNoCheck(x-1,y+1,z+1) ;
                light[3] = BlockFinalLightNoCheck(x-1,y,z-1) ;
                light[4] = BlockFinalLightNoCheck(x-1,y+1,z-1) ;
                light[5] = BlockFinalLightNoCheck(x-1,y-1,z) ;
                light[6] = BlockFinalLightNoCheck(x-1,y-1,z+1) ;
                light[7] = BlockFinalLightNoCheck(x-1,y-1,z-1) ;
            }

            float light1noV = (light[0]+light[1]+light[2]+mainLight)/4.0f;
            float light2noV = (light[0]+light[3]+light[4]+mainLight)/4.0f;
            float light3noV = (light[5]+light[1]+light[6]+mainLight)/4.0f;
            float light4noV = (light[5]+light[3]+light[7]+mainLight)/4.0f;

            float lightFactor1 = light1noV * lightShadowFactor;
            float lightFactor2 = light2noV * lightShadowFactor;
            float lightFactor3 = light3noV * lightShadowFactor;
            float lightFactor4 = light4noV * lightShadowFactor;

            if(onCorners)
            {
                if(!BlockTransparentOrLightSource(x-1,y+1,z) || !BlockTransparentOrLightSource(x-1,y,z+1) || !BlockTransparentOrLightSource(x-1,y+1,z+1))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSource(x-1,y+1,z) || !BlockTransparentOrLightSource(x-1,y,z-1) || !BlockTransparentOrLightSource(x-1,y+1,z-1))
                {
                    light2noV-=lightFactor2;
                }
                if(!BlockTransparentOrLightSource(x-1,y-1,z) || !BlockTransparentOrLightSource(x-1,y,z+1) || !BlockTransparentOrLightSource(x-1,y-1,z+1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSource(x-1,y-1,z) || !BlockTransparentOrLightSource(x-1,y,z-1) || !BlockTransparentOrLightSource(x-1,y-1,z-1))
                {
                    light4noV-=lightFactor4;
                }
            }
            else
            {
                if(!BlockTransparentOrLightSourceNoCheck(x-1,y+1,z) || !BlockTransparentOrLightSourceNoCheck(x-1,y,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y+1,z+1))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x-1,y+1,z) || !BlockTransparentOrLightSourceNoCheck(x-1,y,z-1) || !BlockTransparentOrLightSourceNoCheck(x-1,y+1,z-1))
                {
                    light2noV-=lightFactor2;
                }
                if(!BlockTransparentOrLightSourceNoCheck(x-1,y-1,z) || !BlockTransparentOrLightSourceNoCheck(x-1,y,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y-1,z+1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x-1,y-1,z) || !BlockTransparentOrLightSourceNoCheck(x-1,y,z-1) || !BlockTransparentOrLightSourceNoCheck(x-1,y-1,z-1))
                {
                    light4noV-=lightFactor4;
                }
            }

            light1 = Vector3(light3noV,light3noV,light3noV)*blockXConstLight;
            light2 = Vector3(light1noV,light1noV,light1noV)*blockXConstLight;
            light3 = Vector3(light4noV,light4noV,light4noV)*blockXConstLight;
            light4 = Vector3(light2noV,light2noV,light2noV)*blockXConstLight;
        }

        left = percent*blockType->leftPlane_x;
        right = percent*blockType->leftPlane_x+percent;

        up = percent*blockType->leftPlane_y;
        down = percent*blockType->leftPlane_y+percent;

        if(side != -1)
        {
            int dif = Block - FurnaceOff::getID();

            if(side == 2)
            {
                left = percent*(12+dif);
                right = percent*(12+dif)+percent;

                up = percent*(2+dif);
                down = percent*(2+dif)+percent;
            }
        }

        MeshChunk->vert(x, y, z+1, right, down, light1.x,light1.y,light1.z);
        MeshChunk->vert(x, y+1, z+1, right, up, light2.x,light2.y,light2.z);
        MeshChunk->vert(x, y+1, z, left, up, light4.x,light4.y,light4.z);
        MeshChunk->vert(x, y, z, left, down,light3.x,light3.y,light3.z);

        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

        iVertex += 4;
    }


    Vector3 BlockColorx2;
    canCreate = false;
    onCorners == true ? Block1 = _GetBlock(x+1,y,z) : Block1 = _GetBlockNoCheck(x+1,y,z);
    blockType1 = &blockTypes[Block1];

    transparentBlock = blockType1->transparent;
    specialBlock = blockType1->blockModel;
    if(transparent == true)
    {
        if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112))
        {
            canCreate = true;
        }
        if (transparentBlock == false && specialBlock == 0)
        {
            canCreate = false;
        }
    }
    else
    {
        if (specialBlock > 0 && Block1 != Block)
        {
            canCreate = true;
        }
        if (transparentBlock == true || Block1 == 0)
        {
            canCreate = true;
        }
    }

    if(mainOptions.fastRendering)
    {
        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            canCreate = false;
        }
    }

    if (canCreate)
    {

        float mainLight = BlockFinalLight(x+1,y,z);
        BlockColorx2 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;

        if(eastLight != 0)
        {
            BlockColorx2.x += 0.3f*eastLight;
            BlockColorx2.y += 0.2f*eastLight;
        }

        if (BlockColorx2.x > 1.0f)
        {
            BlockColorx2.x = 1.0f;
        }
        if (BlockColorx2.y > 1.0f)
        {
            BlockColorx2.y = 1.0f;
        }

        light1 = light2 = light3 = light4 = BlockColorx2;

        if(mainOptions.smoothLighting == true)
        {
            if(onCorners)
            {
                light[8] = BlockFinalLight(x+1,y+1,z) ;
                light[9] = BlockFinalLight(x+1,y,z+1) ;
                light[10] = BlockFinalLight(x+1,y+1,z+1) ;
                light[11] = BlockFinalLight(x+1,y,z-1) ;
                light[12] = BlockFinalLight(x+1,y+1,z-1) ;
                light[13] = BlockFinalLight(x+1,y-1,z) ;
                light[14] = BlockFinalLight(x+1,y-1,z+1) ;
                light[15] = BlockFinalLight(x+1,y-1,z-1) ;
            }
            else
            {
                light[8] = BlockFinalLightNoCheck(x+1,y+1,z) ;
                light[9] = BlockFinalLightNoCheck(x+1,y,z+1) ;
                light[10] = BlockFinalLightNoCheck(x+1,y+1,z+1) ;
                light[11] = BlockFinalLightNoCheck(x+1,y,z-1) ;
                light[12] = BlockFinalLightNoCheck(x+1,y+1,z-1) ;
                light[13] = BlockFinalLightNoCheck(x+1,y-1,z) ;
                light[14] = BlockFinalLightNoCheck(x+1,y-1,z+1) ;
                light[15] = BlockFinalLightNoCheck(x+1,y-1,z-1) ;
            }

            float light1noV = (light[8]+light[9]+light[10]+mainLight)/4.0f;
            float light2noV = (light[8]+light[11]+light[12]+mainLight)/4.0f;
            float light3noV = (light[13]+light[9]+light[14]+mainLight)/4.0f;
            float light4noV = (light[13]+light[11]+light[15]+mainLight)/4.0f;

            float lightFactor1 = light1noV * lightShadowFactor;
            float lightFactor2 = light2noV * lightShadowFactor;
            float lightFactor3 = light3noV * lightShadowFactor;
            float lightFactor4 = light4noV * lightShadowFactor;

            if(onCorners)
            {
                if(!BlockTransparentOrLightSource(x+1,y+1,z) || !BlockTransparentOrLightSource(x+1,y,z+1) || !BlockTransparentOrLightSource(x+1,y+1,z+1))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSource(x+1,y+1,z) || !BlockTransparentOrLightSource(x+1,y,z-1) || !BlockTransparentOrLightSource(x+1,y+1,z-1))
                {
                    light2noV-=lightFactor2;
                }
                if(!BlockTransparentOrLightSource(x+1,y-1,z) || !BlockTransparentOrLightSource(x+1,y,z+1) || !BlockTransparentOrLightSource(x+1,y-1,z+1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSource(x+1,y-1,z) || !BlockTransparentOrLightSource(x+1,y,z-1) || !BlockTransparentOrLightSource(x+1,y-1,z-1))
                {
                    light4noV-=lightFactor4;
                }
            }
            else
            {
                if(!BlockTransparentOrLightSourceNoCheck(x+1,y+1,z) || !BlockTransparentOrLightSourceNoCheck(x+1,y,z+1) || !BlockTransparentOrLightSourceNoCheck(x+1,y+1,z+1))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x+1,y+1,z) || !BlockTransparentOrLightSourceNoCheck(x+1,y,z-1) || !BlockTransparentOrLightSourceNoCheck(x+1,y+1,z-1))
                {
                    light2noV-=lightFactor2;
                }
                if(!BlockTransparentOrLightSourceNoCheck(x+1,y-1,z) || !BlockTransparentOrLightSourceNoCheck(x+1,y,z+1) || !BlockTransparentOrLightSourceNoCheck(x+1,y-1,z+1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x+1,y-1,z) || !BlockTransparentOrLightSourceNoCheck(x+1,y,z-1) || !BlockTransparentOrLightSourceNoCheck(x+1,y-1,z-1))
                {
                    light4noV-=lightFactor4;
                }
            }

            light1 = Vector3(light3noV,light3noV,light3noV)*blockXConstLight;
            light2 = Vector3(light1noV,light1noV,light1noV)*blockXConstLight;
            light3 = Vector3(light4noV,light4noV,light4noV)*blockXConstLight;
            light4 = Vector3(light2noV,light2noV,light2noV)*blockXConstLight;
        }

        left = percent*blockType->rightPlane_x;
        right = percent*blockType->rightPlane_x+percent;

        up = percent*blockType->rightPlane_y;
        down = percent*blockType->rightPlane_y+percent;

        if(side != -1)
        {
            int dif = Block - FurnaceOff::getID();

            if(side == 1)
            {
                left = percent*(12+dif);
                right = percent*(12+dif)+percent;

                up = percent*(2+dif);
                down = percent*(2+dif)+percent;
            }
        }

        MeshChunk->vert(x+1, y, z, right, down, light3.x,light3.y,light3.z);
        MeshChunk->vert(x+1, y+1, z, right, up, light4.x,light4.y,light4.z);
        MeshChunk->vert(x+1, y+1, z+1, left, up, light2.x,light2.y,light2.z);
        MeshChunk->vert(x+1, y, z+1, left, down, light1.x,light1.y,light1.z);

        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

        iVertex += 4;
    }



    canCreate = false;
    Vector3 BlockColory2;
    onCorners == true ? Block1 = _GetBlock(x,y-1,z) : Block1 = _GetBlockNoCheck(x,y-1,z);
    blockType1 = &blockTypes[Block1];

    transparentBlock = blockType1->transparent;
    specialBlock = blockType1->blockModel;
    if(transparent == true)
    {
        if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112))
        {
            canCreate = true;
        }
        if (transparentBlock == false && specialBlock == 0)
        {
            canCreate = false;
        }
    }
    else
    {
        if (specialBlock > 0 && Block1 != Block)
        {
            canCreate = true;
        }
        if (transparentBlock == true || Block1 == 0)
        {
            canCreate = true;
        }
    }

    if(mainOptions.fastRendering)
    {
        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            canCreate = false;
        }
    }

    if (canCreate)
    {
        float mainLight = BlockFinalLight(x,y-1,z);
        BlockColory2 = Vector3(mainLight,mainLight,mainLight) * 0.6f;
        light1 = light2 = light3 = light4 = BlockColory2;

        if(mainOptions.smoothLighting == true)
        {
            if(onCorners)
            {
                if(light[5] == -1)
                {
                    light[5] = BlockFinalLight(x-1,y-1,z) ;
                }

                if(light[7] == -1)
                {
                    light[7] = BlockFinalLight(x-1,y-1,z-1) ;
                }

                light[16] = BlockFinalLight(x,y-1,z-1) ;

                if(light[15] == -1)
                {
                    light[15] = BlockFinalLight(x+1,y-1,z-1) ;
                }

                if(light[13] == -1)
                {
                    light[13] = BlockFinalLight(x+1,y-1,z) ;
                }

                if(light[14] == -1)
                {
                    light[14] = BlockFinalLight(x+1,y-1,z+1) ;
                }

                if(light[6] == -1)
                {
                    light[6] = BlockFinalLight(x-1,y-1,z+1) ;
                }

                light[17] = BlockFinalLight(x,y-1,z+1) ;
            }
            else
            {
                if(light[5] == -1)
                {
                    light[5] = BlockFinalLightNoCheck(x-1,y-1,z) ;
                }

                if(light[7] == -1)
                {
                    light[7] = BlockFinalLightNoCheck(x-1,y-1,z-1) ;
                }

                light[16] = BlockFinalLightNoCheck(x,y-1,z-1) ;

                if(light[15] == -1)
                {
                    light[15] = BlockFinalLightNoCheck(x+1,y-1,z-1) ;
                }

                if(light[13] == -1)
                {
                    light[13] = BlockFinalLightNoCheck(x+1,y-1,z) ;
                }

                if(light[14] == -1)
                {
                    light[14] = BlockFinalLightNoCheck(x+1,y-1,z+1) ;
                }

                if(light[6] == -1)
                {
                    light[6] = BlockFinalLightNoCheck(x-1,y-1,z+1) ;
                }

                light[17] = BlockFinalLightNoCheck(x,y-1,z+1) ;
            }

            float light1noV = (light[5]+light[7]+light[16]+mainLight)/4.0f;
            float light2noV = (light[16]+light[15]+light[13]+mainLight)/4.0f;
            float light3noV = (light[13]+light[14]+light[17]+mainLight)/4.0f;
            float light4noV = (light[17]+light[6]+light[5]+mainLight)/4.0f;

            float lightFactor1 = light1noV * lightShadowFactor;
            float lightFactor2 = light2noV * lightShadowFactor;
            float lightFactor3 = light3noV * lightShadowFactor;
            float lightFactor4 = light4noV * lightShadowFactor;

            if(onCorners)
            {
                if(!BlockTransparentOrLightSource(x-1,y-1,z) || !BlockTransparentOrLightSource(x-1,y-1,z-1) || !BlockTransparentOrLightSource(x,y-1,z-1))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSource(x,y-1,z-1) || !BlockTransparentOrLightSource(x+1,y-1,z-1) || !BlockTransparentOrLightSource(x+1,y-1,z))
                {
                    light2noV-=lightFactor2;
                }

                if(!BlockTransparentOrLightSource(x+1,y-1,z) || !BlockTransparentOrLightSource(x+1,y-1,z+1) || !BlockTransparentOrLightSource(x,y-1,z+1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSource(x,y-1,z+1) || !BlockTransparentOrLightSource(x-1,y-1,z+1) || !BlockTransparentOrLightSource(x-1,y-1,z))
                {
                    light4noV-=lightFactor4;
                }
            }
            else
            {
                if(!BlockTransparentOrLightSourceNoCheck(x-1,y-1,z) || !BlockTransparentOrLightSourceNoCheck(x-1,y-1,z-1) || !BlockTransparentOrLightSourceNoCheck(x,y-1,z-1))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x,y-1,z-1) || !BlockTransparentOrLightSourceNoCheck(x+1,y-1,z-1) || !BlockTransparentOrLightSourceNoCheck(x+1,y-1,z))
                {
                    light2noV-=lightFactor2;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x+1,y-1,z) || !BlockTransparentOrLightSourceNoCheck(x+1,y-1,z+1) || !BlockTransparentOrLightSourceNoCheck(x,y-1,z+1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x,y-1,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y-1,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y-1,z))
                {
                    light4noV-=lightFactor4;
                }
            }

            light1 = Vector3(light1noV,light1noV,light1noV)*0.6f;
            light2 = Vector3(light2noV,light2noV,light2noV)*0.6f;
            light3 = Vector3(light3noV,light3noV,light3noV)*0.6f;
            light4 = Vector3(light4noV,light4noV,light4noV)*0.6f;
        }

        left = percent*blockType->downPlane_x;
        right = percent*blockType->downPlane_x+percent;

        up = percent*blockType->downPlane_y;
        down = percent*blockType->downPlane_y+percent;

        MeshChunk->vert(x, y, z, left, up, light1.x,light1.y,light1.z);
        MeshChunk->vert(x+1, y, z, right, up, light2.x,light2.y,light2.z);
        MeshChunk->vert(x+1, y, z+1, right, down, light3.x,light3.y,light3.z);
        MeshChunk->vert(x, y, z+1, left, down, light4.x,light4.y,light4.z);

        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

        iVertex += 4;
    }



    canCreate = false;
    Vector3 BlockColory1;
    onCorners == true ? Block1 = _GetBlock(x,y+1,z) : Block1 = _GetBlockNoCheck(x,y+1,z);
    blockType1 = &blockTypes[Block1];

    transparentBlock = blockType1->transparent;
    specialBlock = blockType1->blockModel;
    if(transparent == true)
    {
        if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112))
        {
            canCreate = true;
        }
        if (transparentBlock == false && specialBlock == 0)
        {
            canCreate = false;
        }
    }
    else
    {
        if (specialBlock > 0 && Block1 != Block)
        {
            canCreate = true;
        }
        if (transparentBlock == true || Block1 == 0)
        {
            canCreate = true;
        }
    }

    if(Block1 == 93)
    {
        canCreate = false;
    }

    if(mainOptions.fastRendering)
    {
        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            canCreate = false;
        }
    }

    if(canCreate)
    {
        float mainLight = BlockFinalLight(x,y+1,z);
        BlockColory1 = Vector3(mainLight,mainLight,mainLight);

        light1 = Vector3(1,1,1);
        light2 = Vector3(1,1,1);
        light3 = Vector3(1,1,1);
        light4 = Vector3(1,1,1);

        light1 *= BlockColory1;
        light2 *= BlockColory1;
        light3 *= BlockColory1;
        light4 *= BlockColory1;

        if(mainOptions.smoothLighting == true)
        {
            if(onCorners)
            {
                light[18] = BlockFinalLight(x,y+1,z+1) ;

                if(light[2] == -1)
                {
                    light[2] = BlockFinalLight(x-1,y+1,z+1) ;
                }

                if(light[0] == -1)
                {
                    light[0] = BlockFinalLight(x-1,y+1,z) ;
                }

                if(light[10] == -1)
                {
                    light[10] = BlockFinalLight(x+1,y+1,z+1) ;
                }

                if(light[8] == -1)
                {
                    light[8] = BlockFinalLight(x+1,y+1,z) ;
                }

                if(light[12] == -1)
                {
                    light[12] = BlockFinalLight(x+1,y+1,z-1) ;
                }

                light[19] = BlockFinalLight(x,y+1,z-1) ;

                if(light[4] == -1)
                {
                    light[4] = BlockFinalLight(x-1,y+1,z-1) ;
                }
            }
            else
            {
                light[18] = BlockFinalLightNoCheck(x,y+1,z+1) ;

                if(light[2] == -1)
                {
                    light[2] = BlockFinalLightNoCheck(x-1,y+1,z+1) ;
                }

                if(light[0] == -1)
                {
                    light[0] = BlockFinalLightNoCheck(x-1,y+1,z) ;
                }

                if(light[10] == -1)
                {
                    light[10] = BlockFinalLightNoCheck(x+1,y+1,z+1) ;
                }

                if(light[8] == -1)
                {
                    light[8] = BlockFinalLightNoCheck(x+1,y+1,z) ;
                }

                if(light[12] == -1)
                {
                    light[12] = BlockFinalLightNoCheck(x+1,y+1,z-1) ;
                }

                light[19] = BlockFinalLightNoCheck(x,y+1,z-1) ;

                if(light[4] == -1)
                {
                    light[4] = BlockFinalLightNoCheck(x-1,y+1,z-1) ;
                }
            }

            float light1noV = (light[18]+light[2]+light[0]+mainLight)/4.0f;
            float light2noV = (light[18]+light[10]+light[8]+mainLight)/4.0f;
            float light3noV = (light[8]+light[12]+light[19]+mainLight)/4.0f;
            float light4noV = (light[19]+light[4]+light[0]+mainLight)/4.0f;

            float lightFactor1 = light1noV * lightShadowFactor;
            float lightFactor2 = light2noV * lightShadowFactor;
            float lightFactor3 = light3noV * lightShadowFactor;
            float lightFactor4 = light4noV * lightShadowFactor;

            if(onCorners)
            {
                if(!BlockTransparentOrLightSource(x,y+1,z+1) || !BlockTransparentOrLightSource(x-1,y+1,z+1) || !BlockTransparentOrLightSource(x-1,y+1,z))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSource(x,y+1,z+1) || !BlockTransparentOrLightSource(x+1,y+1,z+1) || !BlockTransparentOrLightSource(x+1,y+1,z))
                {
                    light2noV-=lightFactor2;
                }

                if(!BlockTransparentOrLightSource(x+1,y+1,z) || !BlockTransparentOrLightSource(x+1,y+1,z-1) || !BlockTransparentOrLightSource(x,y+1,z-1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSource(x,y+1,z-1) || !BlockTransparentOrLightSource(x-1,y+1,z-1) || !BlockTransparentOrLightSource(x-1,y+1,z))
                {
                    light4noV-=lightFactor4;
                }
            }
            else
            {
                if(!BlockTransparentOrLightSourceNoCheck(x,y+1,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y+1,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y+1,z))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x,y+1,z+1) || !BlockTransparentOrLightSourceNoCheck(x+1,y+1,z+1) || !BlockTransparentOrLightSourceNoCheck(x+1,y+1,z))
                {
                    light2noV-=lightFactor2;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x+1,y+1,z) || !BlockTransparentOrLightSourceNoCheck(x+1,y+1,z-1) || !BlockTransparentOrLightSourceNoCheck(x,y+1,z-1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x,y+1,z-1) || !BlockTransparentOrLightSourceNoCheck(x-1,y+1,z-1) || !BlockTransparentOrLightSourceNoCheck(x-1,y+1,z))
                {
                    light4noV-=lightFactor4;
                }
            }

            light1 = Vector3(light1noV,light1noV,light1noV);
            light2 = Vector3(light2noV,light2noV,light2noV);
            light3 = Vector3(light3noV,light3noV,light3noV);
            light4 = Vector3(light4noV,light4noV,light4noV);
        }

        left = percent*blockType->upPlane_x;
        right = percent*blockType->upPlane_x+percent;

        up = percent*blockType->upPlane_y;
        down = percent*blockType->upPlane_y+percent;

        MeshChunk->vert(x, y+1, z+1, left, up, light1.x,light1.y,light1.z);
        MeshChunk->vert(x+1, y+1, z+1, right, up, light2.x,light2.y,light2.z);
        MeshChunk->vert(x+1, y+1, z, right, down, light3.x,light3.y,light3.z);
        MeshChunk->vert(x, y+1, z, left, down, light4.x,light4.y,light4.z);

        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

        iVertex += 4;
    }


    Vector3 BlockColorz1;
    canCreate = false;
    onCorners == true ? Block1 = _GetBlock(x,y,z-1) : Block1 = _GetBlockNoCheck(x,y,z-1);
    blockType1 = &blockTypes[Block1];

    transparentBlock = blockType1->transparent;
    specialBlock = blockType1->blockModel;
    if(transparent == true)
    {
        if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112))
        {
            canCreate = true;
        }
        if (transparentBlock == false && specialBlock == 0)
        {
            canCreate = false;
        }
    }
    else
    {
        if (specialBlock > 0 && Block1 != Block)
        {
            canCreate = true;
        }
        if (transparentBlock == true || Block1 == 0)
        {
            canCreate = true;
        }
    }

    if(mainOptions.fastRendering)
    {
        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            canCreate = false;
        }
    }

    if (canCreate)
    {
        float mainLight = BlockFinalLight(x,y,z-1);
        BlockColorz1 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;
        light1 = light2 = light3 = light4 = BlockColorz1;

        if(mainOptions.smoothLighting == true)
        {
            if(onCorners)
            {
                if(light[19] == -1)
                {
                    light[19] = BlockFinalLight(x,y+1,z-1) ;
                }

                if(light[3] == -1)
                {
                    light[3] = BlockFinalLight(x-1,y,z-1) ;
                }

                if(light[4] == -1)
                {
                    light[4] = BlockFinalLight(x-1,y+1,z-1) ;
                }

                if(light[11] == -1)
                {
                    light[11] = BlockFinalLight(x+1,y,z-1) ;
                }

                if(light[12] == -1)
                {
                    light[12] = BlockFinalLight(x+1,y+1,z-1) ;
                }

                if(light[16] == -1)
                {
                    light[16] = BlockFinalLight(x,y-1,z-1) ;
                }

                if(light[7] == -1)
                {
                    light[7] = BlockFinalLight(x-1,y-1,z-1) ;
                }

                if(light[15] == -1)
                {
                    light[15] = BlockFinalLight(x+1,y-1,z-1) ;
                }
            }
            else
            {
                if(light[19] == -1)
                {
                    light[19] = BlockFinalLightNoCheck(x,y+1,z-1) ;
                }

                if(light[3] == -1)
                {
                    light[3] = BlockFinalLightNoCheck(x-1,y,z-1) ;
                }

                if(light[4] == -1)
                {
                    light[4] = BlockFinalLightNoCheck(x-1,y+1,z-1) ;
                }

                if(light[11] == -1)
                {
                    light[11] = BlockFinalLightNoCheck(x+1,y,z-1) ;
                }

                if(light[12] == -1)
                {
                    light[12] = BlockFinalLightNoCheck(x+1,y+1,z-1) ;
                }

                if(light[16] == -1)
                {
                    light[16] = BlockFinalLightNoCheck(x,y-1,z-1) ;
                }

                if(light[7] == -1)
                {
                    light[7] = BlockFinalLightNoCheck(x-1,y-1,z-1) ;
                }

                if(light[15] == -1)
                {
                    light[15] = BlockFinalLightNoCheck(x+1,y-1,z-1) ;
                }
            }

            float light1noV = (light[19]+light[3]+light[4]+mainLight)/4.0f;
            float light2noV = (light[19]+light[11]+light[12]+mainLight)/4.0f;
            float light3noV = (light[16]+light[3]+light[7]+mainLight)/4.0f;
            float light4noV = (light[16]+light[11]+light[15]+mainLight)/4.0f;

            float lightFactor1 = light1noV * lightShadowFactor;
            float lightFactor2 = light2noV * lightShadowFactor;
            float lightFactor3 = light3noV * lightShadowFactor;
            float lightFactor4 = light4noV * lightShadowFactor;

            if(onCorners)
            {
                if(!BlockTransparentOrLightSource(x,y+1,z-1) || !BlockTransparentOrLightSource(x-1,y,z-1) || !BlockTransparentOrLightSource(x-1,y+1,z-1))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSource(x,y+1,z-1) || !BlockTransparentOrLightSource(x+1,y,z-1) || !BlockTransparentOrLightSource(x+1,y+1,z-1))
                {
                    light2noV-=lightFactor2;
                }
                if(!BlockTransparentOrLightSource(x,y-1,z-1) || !BlockTransparentOrLightSource(x-1,y,z-1) || !BlockTransparentOrLightSource(x-1,y-1,z-1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSource(x,y-1,z-1) || !BlockTransparentOrLightSource(x+1,y,z-1) || !BlockTransparentOrLightSource(x+1,y-1,z-1))
                {
                    light4noV-=lightFactor4;
                }
            }
            else
            {
                if(!BlockTransparentOrLightSourceNoCheck(x,y+1,z-1) || !BlockTransparentOrLightSourceNoCheck(x-1,y,z-1) || !BlockTransparentOrLightSourceNoCheck(x-1,y+1,z-1))
                {
                    light1noV-=lightFactor1;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x,y+1,z-1) || !BlockTransparentOrLightSourceNoCheck(x+1,y,z-1) || !BlockTransparentOrLightSourceNoCheck(x+1,y+1,z-1))
                {
                    light2noV-=lightFactor2;
                }
                if(!BlockTransparentOrLightSourceNoCheck(x,y-1,z-1) || !BlockTransparentOrLightSourceNoCheck(x-1,y,z-1) || !BlockTransparentOrLightSourceNoCheck(x-1,y-1,z-1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x,y-1,z-1) || !BlockTransparentOrLightSourceNoCheck(x+1,y,z-1) || !BlockTransparentOrLightSourceNoCheck(x+1,y-1,z-1))
                {
                    light4noV-=lightFactor4;
                }
            }

            light1 = Vector3(light3noV,light3noV,light3noV)*blockZConstLight;
            light2 = Vector3(light1noV,light1noV,light1noV)*blockZConstLight;
            light3 = Vector3(light4noV,light4noV,light4noV)*blockZConstLight;
            light4 = Vector3(light2noV,light2noV,light2noV)*blockZConstLight;
        }

        left = percent*blockType->botPlane_x;
        right = percent*blockType->botPlane_x+percent;

        up = percent*blockType->botPlane_y;
        down = percent*blockType->botPlane_y+percent;

        if(side != -1)
        {
            int dif = Block - FurnaceOff::getID();

            if(side == 3)
            {
                left = percent*(12+dif);
                right = percent*(12+dif)+percent;

                up = percent*(2+dif);
                down = percent*(2+dif)+percent;
            }
        }

        MeshChunk->vert(x, y+1, z, right, up, light2.x,light2.y,light2.z);
        MeshChunk->vert(x+1, y+1, z, left, up, light4.x,light4.y,light4.z);
        MeshChunk->vert(x+1, y, z, left, down, light3.x,light3.y,light3.z);
        MeshChunk->vert(x, y, z, right, down, light1.x,light1.y,light1.z);

        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

        iVertex += 4;
    }


    Vector3 BlockColorz2;
    canCreate = false;
    onCorners == true ? Block1 = _GetBlock(x,y,z+1) : Block1 = _GetBlockNoCheck(x,y,z+1);
    blockType1 = &blockTypes[Block1];

    transparentBlock = blockType1->transparent;
    specialBlock = blockType1->blockModel;
    if(transparent == true)
    {
        if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112))
        {
            canCreate = true;
        }
        if (transparentBlock == false && specialBlock == 0)
        {
            canCreate = false;
        }
    }
    else
    {
        if (specialBlock > 0 && Block1 != Block)
        {
            canCreate = true;
        }
        if (transparentBlock == true || Block1 == 0)
        {
            canCreate = true;
        }
    }

    if(mainOptions.fastRendering)
    {
        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            canCreate = false;
        }
    }

    if (canCreate == true)
    {
        float mainLight = BlockFinalLight(x,y,z+1);
        BlockColorz2 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;
        light1 = light2 = light3 = light4 = BlockColorz2;

        if(mainOptions.smoothLighting == true)
        {
            if(onCorners)
            {
                if(light[18] == -1)
                {
                    light[18] = BlockFinalLight(x,y+1,z+1) ;
                }

                if(light[1] == -1)
                {
                    light[1] = BlockFinalLight(x-1,y,z+1) ;
                }

                if(light[2] == -1)
                {
                    light[2] = BlockFinalLight(x-1,y+1,z+1) ;
                }

                if(light[9] == -1)
                {
                    light[9] = BlockFinalLight(x+1,y,z+1) ;
                }

                if(light[10] == -1)
                {
                    light[10] = BlockFinalLight(x+1,y+1,z+1) ;
                }

                if(light[17] == -1)
                {
                    light[17] = BlockFinalLight(x,y-1,z+1) ;
                }

                if(light[6] == -1)
                {
                    light[6] = BlockFinalLight(x-1,y-1,z+1) ;
                }

                if(light[14] == -1)
                {
                    light[14] = BlockFinalLight(x+1,y-1,z+1) ;
                }
            }
            else
            {
                if(light[18] == -1)
                {
                    light[18] = BlockFinalLightNoCheck(x,y+1,z+1) ;
                }

                if(light[1] == -1)
                {
                    light[1] = BlockFinalLightNoCheck(x-1,y,z+1) ;
                }

                if(light[2] == -1)
                {
                    light[2] = BlockFinalLightNoCheck(x-1,y+1,z+1) ;
                }

                if(light[9] == -1)
                {
                    light[9] = BlockFinalLightNoCheck(x+1,y,z+1) ;
                }

                if(light[10] == -1)
                {
                    light[10] = BlockFinalLightNoCheck(x+1,y+1,z+1) ;
                }

                if(light[17] == -1)
                {
                    light[17] = BlockFinalLightNoCheck(x,y-1,z+1) ;
                }

                if(light[6] == -1)
                {
                    light[6] = BlockFinalLightNoCheck(x-1,y-1,z+1) ;
                }

                if(light[14] == -1)
                {
                    light[14] = BlockFinalLightNoCheck(x+1,y-1,z+1) ;
                }
            }

            float light1noV = (light[18]+light[1]+light[2]+mainLight)/4.0f;
            float light2noV = (light[18]+light[9]+light[10]+mainLight)/4.0f;
            float light3noV = (light[17]+light[1]+light[6]+mainLight)/4.0f;
            float light4noV = (light[17]+light[9]+light[14]+mainLight)/4.0f;

            float lightFactor1 = light1noV * lightShadowFactor;
            float lightFactor2 = light2noV * lightShadowFactor;
            float lightFactor3 = light3noV * lightShadowFactor;
            float lightFactor4 = light4noV * lightShadowFactor;

            if(onCorners)
            {
                if(!BlockTransparentOrLightSource(x,y+1,z+1) || !BlockTransparentOrLightSource(x-1,y,z+1) || !BlockTransparentOrLightSource(x-1,y+1,z+1))
                {
                    light1noV-=lightFactor1;
                }
                if(!BlockTransparentOrLightSource(x,y+1,z+1) || !BlockTransparentOrLightSource(x+1,y,z+1) || !BlockTransparentOrLightSource(x+1,y+1,z+1))
                {
                    light2noV-=lightFactor2;
                }
                if(!BlockTransparentOrLightSource(x,y-1,z+1) || !BlockTransparentOrLightSource(x-1,y,z+1) || !BlockTransparentOrLightSource(x-1,y-1,z+1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSource(x,y-1,z+1) || !BlockTransparentOrLightSource(x+1,y,z+1) || !BlockTransparentOrLightSource(x+1,y-1,z+1))
                {
                    light4noV-=lightFactor4;
                }
            }
            else
            {
                if(!BlockTransparentOrLightSourceNoCheck(x,y+1,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y+1,z+1))
                {
                    light1noV-=lightFactor1;
                }
                if(!BlockTransparentOrLightSourceNoCheck(x,y+1,z+1) || !BlockTransparentOrLightSourceNoCheck(x+1,y,z+1) || !BlockTransparentOrLightSourceNoCheck(x+1,y+1,z+1))
                {
                    light2noV-=lightFactor2;
                }
                if(!BlockTransparentOrLightSourceNoCheck(x,y-1,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y,z+1) || !BlockTransparentOrLightSourceNoCheck(x-1,y-1,z+1))
                {
                    light3noV-=lightFactor3;
                }

                if(!BlockTransparentOrLightSourceNoCheck(x,y-1,z+1) || !BlockTransparentOrLightSourceNoCheck(x+1,y,z+1) || !BlockTransparentOrLightSourceNoCheck(x+1,y-1,z+1))
                {
                    light4noV-=lightFactor4;
                }
            }

            light1 = Vector3(light3noV,light3noV,light3noV)*blockZConstLight;
            light2 = Vector3(light1noV,light1noV,light1noV)*blockZConstLight;
            light3 = Vector3(light4noV,light4noV,light4noV)*blockZConstLight;
            light4 = Vector3(light2noV,light2noV,light2noV)*blockZConstLight;
        }

        left = percent*blockType->forPlane_x;
        right = percent*blockType->forPlane_x+percent;

        up = percent*blockType->forPlane_y;
        down = percent*blockType->forPlane_y+percent;

        if(side != -1)
        {
            int dif = Block - FurnaceOff::getID();

            if(side == 0)
            {
                left = percent*(12+dif);
                right = percent*(12+dif)+percent;

                up = percent*(2+dif);
                down = percent*(2+dif)+percent;
            }
        }

        MeshChunk->vert(x, y, z+1, left, down, light1.x,light1.y,light1.z);
        MeshChunk->vert(x+1, y, z+1, right, down, light3.x,light3.y,light3.z);
        MeshChunk->vert(x+1, y+1, z+1, right, up, light4.x,light4.y,light4.z);
        MeshChunk->vert(x, y+1, z+1, left, up, light2.x,light2.y,light2.z);

        MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
        MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

        iVertex += 4;
    }
}
