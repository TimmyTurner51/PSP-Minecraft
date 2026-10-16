#include "CraftWorld2.h"

void CraftWorld::BuildWorldBlockPlaneNoCheck(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4)
{
    float pixel, texturePixel;

    pixel = (float)1/16.0f;
    texturePixel = (float)1/256.0f;

    bool canCreate = false;
    //bool DefaultBlock = false;
    //bool transparentBlock = false;

    float up = 0.0f;
    float down = 0.0f;
    float left = 0.0f;
    float right = 0.0f;

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

    if(side == 1)
    {
        Vector3 BlockColorx1;
        canCreate = true;

        if(canCreate == true)
        {
            float mainLight = BlockFinalLight(x-1,y,z);
            BlockColorx1 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;

            if(westLight != 0)
            {
                BlockColorx1.x += 0.3*westLight;
                BlockColorx1.y += 0.2*westLight;
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

            left = percent*blockType->leftPlane_x + point4.z*texturePixel;
            right = percent*blockType->leftPlane_x+percent - (16-point2.z)*texturePixel;

            up = percent*blockType->leftPlane_y + (16-point2.y)*texturePixel;
            down = percent*blockType->leftPlane_y + (16-point4.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 2)
    {

        Vector3 BlockColorx2;
        canCreate = true;

        if (canCreate)
        {
            float mainLight = BlockFinalLight(x+1,y,z);
            BlockColorx2 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;

            if(eastLight != 0)
            {
                BlockColorx2.x += 0.3*eastLight;
                BlockColorx2.y += 0.2*eastLight;
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

            left = percent*blockType->rightPlane_x + point1.z*texturePixel;
            right = percent*blockType->rightPlane_x+percent - (16-point3.z)*texturePixel;

            up = percent*blockType->rightPlane_y + (16-point3.y)*texturePixel;
            down = percent*blockType->rightPlane_y + (16-point1.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 3)
    {

        Vector3 BlockColory2;
        canCreate = true;

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

            left = percent*blockType->downPlane_x + point1.x*texturePixel;
            right = percent*blockType->downPlane_x+percent - (16-point3.x)*texturePixel;

            up = percent*blockType->downPlane_y + (16-point1.z)*texturePixel;
            down = percent*blockType->downPlane_y + (16-point3.z)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light4.x,light4.y,light4.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 4)
    {

        Vector3 BlockColory1;
        canCreate = true;

        if(canCreate)
        {
            float mainLight = BlockFinalLight(x,y+1,z);
            BlockColory1 = Vector3(mainLight,mainLight,mainLight);
            light1 = light2 = light3 = light4 = BlockColory1;

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

            left = percent*blockType->upPlane_x + point4.x*texturePixel;
            right = percent*blockType->upPlane_x+percent - (16-point2.x)*texturePixel;

            up = percent*blockType->upPlane_y + (16-point2.z)*texturePixel;
            down = percent*blockType->upPlane_y + (16-point4.z)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light4.x,light4.y,light4.z);


            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 5)
    {

        Vector3 BlockColorz1;
        canCreate = true;

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

            left = percent*blockType->botPlane_x + point4.x*texturePixel;
            right = percent*blockType->botPlane_x+percent - (16-point2.x)*texturePixel;

            up = percent*blockType->botPlane_y + (16-point2.y)*texturePixel;
            down = percent*blockType->botPlane_y + (16-point4.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 6)
    {

        Vector3 BlockColorz2;
        canCreate = true;


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

            left = percent*blockType->forPlane_x + point1.x*texturePixel;
            right = percent*blockType->forPlane_x+percent - (16-point3.x)*texturePixel;

            up = percent*blockType->forPlane_y + (16-point3.y)*texturePixel;
            down = percent*blockType->forPlane_y + (16-point1.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }
}


void CraftWorld::BuildWorldBlockPlane(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4)
{
    float pixel, texturePixel;

    pixel = (float)1/16.0f;
    texturePixel = (float)1/256.0f;

    bool canCreate = false;
    bool transparentBlock = false;
    bool specialBlock;

    float up = 0.0f;
    float down = 0.0f;
    float left = 0.0f;
    float right = 0.0f;

    block_t Block1 = 0;

    Vector3 light1, light2, light3, light4;
    Block = GetBlock(x,y,z);

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

    bool leavesBlock = false;
    if(Block == LeavesBlock::getID() || Block == BirchLeaves::getID() || Block == SpruceLeaves::getID())
    {
        leavesBlock = true;
    }

    if(side == 1)    {
        Vector3 BlockColorx1;

        onCorners == true ? Block1 = _GetBlock(x-1,y,z) : Block1 = _GetBlockNoCheck(x-1,y,z);
        BaseBlock *blockType1;
        blockType1 = &blockTypes[Block1];

        transparentBlock = blockType1->transparent;
        specialBlock = blockType1->blockModel;
        if(transparent == true)
        {
            if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112 && Block1 != 9 && Block1 != 37 && Block1 != 247 && Block1 != 38))
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

        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            if(mainOptions.fastRendering == true)
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
                BlockColorx1.x += 0.3*westLight;
                BlockColorx1.y += 0.2*westLight;
            }

            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

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

            left = percent*blockType->leftPlane_x + point4.z*texturePixel;
            right = percent*blockType->leftPlane_x+percent - (16-point2.z)*texturePixel;

            up = percent*blockType->leftPlane_y + (16-point2.y)*texturePixel;
            down = percent*blockType->leftPlane_y + (16-point4.y)*texturePixel;

            if(leavesBlock && mainOptions.fastRendering == true)
            {
                left += percent;
                right += percent;
            }

            MeshChunk->vert(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel, right, down, light1.x,light1.y,light1.z);
            MeshChunk->vert(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel, left, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel, left, down, light3.x,light3.y,light3.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 2)
    {

        Vector3 BlockColorx2;

        onCorners == true ? Block1 = _GetBlock(x+1,y,z) : Block1 = _GetBlockNoCheck(x+1,y,z);
        BaseBlock *blockType1;
        blockType1 = &blockTypes[Block1];

        transparentBlock = blockType1->transparent;
        specialBlock = blockType1->blockModel;
        if(transparent == true)
        {
            if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112 && Block1 != 9 && Block1 != 37 && Block1 != 38))
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

        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            if(mainOptions.fastRendering == true)
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
                BlockColorx2.x += 0.3*eastLight;
                BlockColorx2.y += 0.2*eastLight;
            }

            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

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

            left = percent*blockType->rightPlane_x + point1.z*texturePixel;
            right = percent*blockType->rightPlane_x+percent - (16-point3.z)*texturePixel;

            up = percent*blockType->rightPlane_y + (16-point3.y)*texturePixel;
            down = percent*blockType->rightPlane_y + (16-point1.y)*texturePixel;

            if(leavesBlock && mainOptions.fastRendering == true)
            {
                left += percent;
                right += percent;
            }

            MeshChunk->vert(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel, right, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel, left, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel, left, down, light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 3)
    {

        Vector3 BlockColory2;

        onCorners == true ? Block1 = _GetBlock(x,y-1,z) : Block1 = _GetBlockNoCheck(x,y-1,z);
        BaseBlock *blockType1;
        blockType1 = &blockTypes[Block1];

        transparentBlock = blockType1->transparent;
        specialBlock = blockType1->blockModel;
        if(transparent == true)
        {
            if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112 && Block1 != 9 && Block1 != 37 && Block1 != 38))
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

        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            if(mainOptions.fastRendering == true)
            {
                canCreate = false;
            }
        }

        if(Block == 25 && Block1 == 25)
        {
            canCreate = false;
        }

        if (canCreate)
        {
            float mainLight = BlockFinalLight(x,y-1,z);
            BlockColory2 = Vector3(mainLight,mainLight,mainLight) * 0.6f;

            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

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

            left = percent*blockType->downPlane_x + point1.x*texturePixel;
            right = percent*blockType->downPlane_x+percent - (16-point3.x)*texturePixel;

            up = percent*blockType->downPlane_y + (16-point1.z)*texturePixel;
            down = percent*blockType->downPlane_y + (16-point3.z)*texturePixel;

            if(leavesBlock && mainOptions.fastRendering == true)
            {
                left += percent;
                right += percent;
            }

            MeshChunk->vert(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel, left, up, light1.x,light1.y,light1.z);
            MeshChunk->vert(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel, left, down, light4.x,light4.y,light4.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 4)
    {

        Vector3 BlockColory1;

        onCorners == true ? Block1 = _GetBlock(x,y+1,z) : Block1 = _GetBlockNoCheck(x,y+1,z);
        BaseBlock *blockType1;
        blockType1 = &blockTypes[Block1];

        transparentBlock = blockType1->transparent;
        specialBlock = blockType1->blockModel;
        if(transparent == true)
        {
            if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112 && Block1 != 9 && Block1 != 37 && Block1 != 38))
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

        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            if(mainOptions.fastRendering == true)
            {
                canCreate = false;
            }
        }

        if(Block1 == 93)
        {
            canCreate = false;
        }

        if(Block == 25 && Block1 == 25)
        {
            canCreate = false;
        }

        if(canCreate)
        {
            float mainLight = BlockFinalLight(x,y+1,z);
            BlockColory1 = Vector3(mainLight,mainLight,mainLight);

            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

            light1 = light2 = light3 = light4 = BlockColory1;

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

            left = percent*blockType->upPlane_x + point4.x*texturePixel;
            right = percent*blockType->upPlane_x+percent - (16-point2.x)*texturePixel;

            up = percent*blockType->upPlane_y + (16-point2.z)*texturePixel;
            down = percent*blockType->upPlane_y + (16-point4.z)*texturePixel;

            if(leavesBlock && mainOptions.fastRendering == true)
            {
                left += percent;
                right += percent;
            }

            MeshChunk->vert(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel, left, up, light1.x,light1.y,light1.z);
            MeshChunk->vert(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel, left, down, light4.x,light4.y,light4.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 5)
    {

        Vector3 BlockColorz1;

        onCorners == true ? Block1 = _GetBlock(x,y,z-1) : Block1 = _GetBlockNoCheck(x,y,z-1);
        BaseBlock *blockType1;
        blockType1 = &blockTypes[Block1];

        transparentBlock = blockType1->transparent;
        specialBlock = blockType1->blockModel;
        if(transparent == true)
        {
            if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112 && Block1 != 9 && Block1 != 37 && Block1 != 38))
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

        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            if(mainOptions.fastRendering == true)
            {
                canCreate = false;
            }
        }

        if (canCreate)
        {
            float mainLight = BlockFinalLight(x,y,z-1);
            BlockColorz1 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;

            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

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

            left = percent*blockType->botPlane_x + point4.x*texturePixel;
            right = percent*blockType->botPlane_x+percent - (16-point2.x)*texturePixel;

            up = percent*blockType->botPlane_y + (16-point2.y)*texturePixel;
            down = percent*blockType->botPlane_y + (16-point4.y)*texturePixel;

            if(leavesBlock && mainOptions.fastRendering == true)
            {
                left += percent;
                right += percent;
            }

            MeshChunk->vert(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel, right, up, light2.x,light2.y,light2.z);
            MeshChunk->vert(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel, left, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel, left, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel, right, down, light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 6)
    {

        Vector3 BlockColorz2;

        onCorners == true ? Block1 = _GetBlock(x,y,z+1) : Block1 = _GetBlockNoCheck(x,y,z+1);
        BaseBlock *blockType1;
        blockType1 = &blockTypes[Block1];

        transparentBlock = blockType1->transparent;
        specialBlock = blockType1->blockModel;
        if(transparent == true)
        {
            if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (specialBlock > 0 && Block1 != 4 && Block1 != 112 && Block1 != 9 && Block1 != 37 && Block1 != 38))
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

        if(Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID())
        {
            if(mainOptions.fastRendering == true)
            {
                canCreate = false;
            }
        }

        if (canCreate == true)
        {
            float mainLight = BlockFinalLight(x,y,z+1);
            BlockColorz2 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;

            light1 = Vector3(1,1,1);
            light2 = Vector3(1,1,1);
            light3 = Vector3(1,1,1);
            light4 = Vector3(1,1,1);

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

            left = percent*blockType->forPlane_x + point1.x*texturePixel;
            right = percent*blockType->forPlane_x+percent - (16-point3.x)*texturePixel;

            up = percent*blockType->forPlane_y + (16-point3.y)*texturePixel;
            down = percent*blockType->forPlane_y + (16-point1.y)*texturePixel;

            if(leavesBlock && mainOptions.fastRendering == true)
            {
                left += percent;
                right += percent;
            }

            MeshChunk->vert(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel, left, down, light1.x,light1.y,light1.z);
            MeshChunk->vert(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel, right, down, light3.x,light3.y,light3.z);
            MeshChunk->vert(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel, right, up, light4.x,light4.y,light4.z);
            MeshChunk->vert(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel, left, up, light2.x,light2.y,light2.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }
}


void CraftWorld::BuildWorldBlockPlane(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4, bool alwaysBuild, bool constantColor)
{
    float pixel, texturePixel;

    pixel = (float)1/16.0f;
    texturePixel = (float)1/256.0f;

    bool canCreate = false;
    bool DefaultBlock = false;
    bool transparentBlock = false;

    float up = 0.0f;
    float down = 0.0f;
    float left = 0.0f;
    float right = 0.0f;

    block_t Block1;

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

    if(side == 1)
    {
        Vector3 BlockColorx1;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x-1,y,z);

            transparentBlock = DefaultBlock;
            canCreate = false;
            if(transparent)
            {
                Block1 = 1;
                if (x > 0)
                {
                    transparentBlock = BlockTransparent(x-1,y,z);
                    Block1 = GetBlock(x-1,y,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x-1,y,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (x > 0)
                {
                    Block1 = GetBlock(x-1,y,z);
                    transparentBlock = BlockTransparentOrSpecial(x-1,y,z);
                    if (transparentBlock == true)
                        canCreate = true;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }

        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x-1,y,z);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorx1 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorx1 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;
            }

            if(westLight != 0)
            {
                BlockColorx1.x += 0.3*westLight;
                BlockColorx1.y += 0.2*westLight;
            }

            light1 = light2 = light3 = light4 = BlockColorx1;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*blockType->leftPlane_x + point4.z*texturePixel;
            right = percent*blockType->leftPlane_x+percent - (16-point2.z)*texturePixel;

            up = percent*blockType->leftPlane_y + (16-point2.y)*texturePixel;
            down = percent*blockType->leftPlane_y + (16-point4.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 2)
    {

        Vector3 BlockColorx2;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x+1,y,z);

            transparentBlock = DefaultBlock;
            canCreate = false;
            if(transparent)
            {
                Block1 = 1;
                if (x < worldLimit - 1)
                {
                    transparentBlock = BlockTransparent(x+1,y,z);
                    Block1 = GetBlock(x+1,y,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x+1,y,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (x < worldLimit - 1)
                {
                    transparentBlock = BlockTransparentOrSpecial(x+1,y,z);
                    Block1 = GetBlock(x+1,y,z);
                    if (transparentBlock == true)
                        canCreate = true;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x+1,y,z);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorx2 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorx2 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;
            }

            if(eastLight != 0)
            {
                BlockColorx2.x += 0.3*eastLight;
                BlockColorx2.y += 0.2*eastLight;
            }

            light1 = light2 = light3 = light4 = BlockColorx2;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*blockType->rightPlane_x + point1.z*texturePixel;
            right = percent*blockType->rightPlane_x+percent - (16-point3.z)*texturePixel;

            up = percent*blockType->rightPlane_y + (16-point3.y)*texturePixel;
            down = percent*blockType->rightPlane_y + (16-point1.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 3)
    {

        Vector3 BlockColory2;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y-1,z);

            transparentBlock = DefaultBlock;
            canCreate = false;
            if(transparent)
            {
                Block1 = 1;
                if (y > 0)
                {
                    transparentBlock = BlockTransparent(x,y-1,z);
                    Block1 = GetBlock(x,y-1,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y-1,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (y > 0)
                {
                    Block1 = GetBlock(x,y-1,z);
                    transparentBlock = BlockTransparentOrSpecial(x,y-1,z);
                    if (transparentBlock == true)
                        canCreate = true;
                }
                else
                {
                    canCreate = false;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y-1,z);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColory2 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColory2 = Vector3(mainLight,mainLight,mainLight) * 0.6f;
            }
            light1 = light2 = light3 = light4 = BlockColory2;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*blockType->downPlane_x + point1.x*texturePixel;
            right = percent*blockType->downPlane_x+percent - (16-point3.x)*texturePixel;

            up = percent*blockType->downPlane_y + (16-point1.z)*texturePixel;
            down = percent*blockType->downPlane_y + (16-point3.z)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 4)
    {

        Vector3 BlockColory1;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y+1,z);

            if(transparent)
            {
                Block1 = 1;
                if (y < WORLD_HEIGHT - 1)
                {
                    transparentBlock = BlockTransparent(x,y+1,z);
                    Block1 = GetBlock(x,y+1,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y+1,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                    {
                        canCreate = true;
                    }
                }
            }
            else
            {
                if (y < WORLD_HEIGHT - 1)
                {
                    Block1 = GetBlock(x,y+1,z);
                    if (BlockTransparentOrSpecial(x,y+1,z) == true && GetBlock(x,y+1,z) != 93)
                    {
                        canCreate = true;
                    }
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if(canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y+1,z);
            BlockColory1 = Vector3(mainLight,mainLight,mainLight);
            light1 = light2 = light3 = light4 = BlockColory1;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }
            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,"top",Vector3(6,16,10),Vector3(10,16,10),Vector3(10,16,6),Vector3(6,16,6));
            left = percent*blockType->upPlane_x + point4.x*texturePixel;
            right = percent*blockType->upPlane_x + percent - ((16-point2.x)*texturePixel);

            up = percent*blockType->upPlane_y + (16-point2.z)*texturePixel;
            down = percent*blockType->upPlane_y + (16-point4.z)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);


            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 5)
    {

        Vector3 BlockColorz1;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y,z-1);

            if(transparent)
            {
                Block1 = 1;
                if (z > 0)
                {
                    transparentBlock = BlockTransparent(x,y,z-1);
                    Block1 = GetBlock(x,y,z-1);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y,z-1) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (z > 0)
                {
                    Block1 = GetBlock(x,y,z-1);
                    transparentBlock = BlockTransparentOrSpecial(x,y,z-1);
                    if (transparentBlock == true)
                        canCreate = true;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y,z-1);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorz1 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorz1 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;
            }
            light1 = light2 = light3 = light4 = BlockColorz1;

            if(mainOptions.smoothLighting == true && constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*blockType->botPlane_x + point4.x*texturePixel;
            right = percent*blockType->botPlane_x+percent - (16-point2.x)*texturePixel;

            up = percent*blockType->botPlane_y + (16-point2.y)*texturePixel;
            down = percent*blockType->botPlane_y + (16-point4.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 6)
    {

        Vector3 BlockColorz2;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y,z+1);

            if(transparent)
            {
                Block1 = 1;
                if (z < worldLimit - 1)
                {
                    transparentBlock = BlockTransparent(x,y,z+1);
                    Block1 = GetBlock(x,y,z+1);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y,z+1) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (z < worldLimit - 1)
                {
                    Block1 = GetBlock(x,y,z+1);
                    transparentBlock = BlockTransparentOrSpecial(x,y,z+1);
                    if (transparentBlock == true)
                        canCreate = true;
                }
                else
                {
                    canCreate = false;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate == true || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y,z+1);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorz2 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorz2 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;
            }

            light1 = light2 = light3 = light4 = BlockColorz2;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*blockType->forPlane_x + point1.x*texturePixel;
            right = percent*blockType->forPlane_x+percent - (16-point3.x)*texturePixel;

            up = percent*blockType->forPlane_y + (16-point3.y)*texturePixel;
            down = percent*blockType->forPlane_y + (16-point1.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }
}

void CraftWorld::BuildWorldBlockPlane(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4, bool alwaysBuild, bool constantColor, int texturePlane)
{
    float pixel, texturePixel;

    pixel = (float)1/16.0f;
    texturePixel = (float)1/256.0f;

    bool canCreate = false;
    bool DefaultBlock = false;
    bool transparentBlock = false;

    float up = 0.0f;
    float down = 0.0f;
    float left = 0.0f;
    float right = 0.0f;

    block_t Block1;

    int plane_x = 0, plane_y = 0;
    switch(texturePlane)
    {
        case 0 : plane_x = blockType->leftPlane_x; plane_y = blockType->leftPlane_y;
        break;
        case 1 : plane_x = blockType->rightPlane_x; plane_y = blockType->rightPlane_y;
        break;
        case 2 : plane_x = blockType->downPlane_x; plane_y = blockType->downPlane_y;
        break;
        case 3 : plane_x = blockType->upPlane_x; plane_y = blockType->upPlane_y;
        break;
        case 4 : plane_x = blockType->botPlane_x; plane_y = blockType->botPlane_y;
        break;
        case 5 : plane_x = blockType->forPlane_x; plane_y = blockType->forPlane_y;
        break;
    }

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

    if(side == 1)
    {
        Vector3 BlockColorx1;

        if(!alwaysBuild)
        {
            Block1 = GetBlock(x-1,y,z);

            transparentBlock = DefaultBlock;
            canCreate = false;
            if(transparent)
            {
                Block1 = 1;
                if (x > 0)
                {
                    transparentBlock = BlockTransparent(x-1,y,z);
                    Block1 = GetBlock(x-1,y,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x-1,y,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (x > 0)
                {
                    Block1 = GetBlock(x-1,y,z);
                    transparentBlock = BlockTransparentOrSpecial(x-1,y,z);
                    if (transparentBlock == true)
                        canCreate = true;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x-1,y,z);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorx1 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorx1 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;
            }

            if(westLight != 0)
            {
                BlockColorx1.x += 0.3*westLight;
                BlockColorx1.y += 0.2*westLight;
            }

            light1 = light2 = light3 = light4 = BlockColorx1;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point4.z*texturePixel;
            right = percent*plane_x+percent - (16-point2.z)*texturePixel;

            up = percent*plane_y + (16-point2.y)*texturePixel;
            down = percent*plane_y + (16-point4.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 2)
    {

        Vector3 BlockColorx2;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x+1,y,z);

            transparentBlock = DefaultBlock;
            canCreate = false;
            if(transparent)
            {
                Block1 = 1;
                if (x < worldLimit - 1)
                {
                    transparentBlock = BlockTransparent(x+1,y,z);
                    Block1 = GetBlock(x+1,y,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x+1,y,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (x < worldLimit - 1)
                {
                    transparentBlock = BlockTransparentOrSpecial(x+1,y,z);
                    Block1 = GetBlock(x+1,y,z);
                    if (transparentBlock == true)
                        canCreate = true;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x+1,y,z);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorx2 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorx2 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;
            }

            if(eastLight != 0)
            {
                BlockColorx2.x += 0.3*eastLight;
                BlockColorx2.y += 0.2*eastLight;
            }

            light1 = light2 = light3 = light4 = BlockColorx2;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point1.z*texturePixel;
            right = percent*plane_x+percent - (16-point3.z)*texturePixel;

            up = percent*plane_y + (16-point3.y)*texturePixel;
            down = percent*plane_y + (16-point1.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 3)
    {

        Vector3 BlockColory2;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y-1,z);

            transparentBlock = DefaultBlock;
            canCreate = false;
            if(transparent)
            {
                Block1 = 1;
                if (y > 0)
                {
                    transparentBlock = BlockTransparent(x,y-1,z);
                    Block1 = GetBlock(x,y-1,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y-1,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (y > 0)
                {
                    Block1 = GetBlock(x,y-1,z);
                    transparentBlock = BlockTransparentOrSpecial(x,y-1,z);
                    if (transparentBlock == true)
                        canCreate = true;
                }
                else
                {
                    canCreate = false;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y-1,z);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColory2 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColory2 = Vector3(mainLight,mainLight,mainLight) * 0.6f;
            }
            light1 = light2 = light3 = light4 = BlockColory2;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point1.x*texturePixel;
            right = percent*plane_x+percent - (16-point3.x)*texturePixel;

            up = percent*plane_y + (16-point1.z)*texturePixel;
            down = percent*plane_y + (16-point3.z)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 4)
    {

        Vector3 BlockColory1;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y+1,z);

            if(transparent)
            {
                Block1 = 1;
                if (y < WORLD_HEIGHT - 1)
                {
                    transparentBlock = BlockTransparent(x,y+1,z);
                    Block1 = GetBlock(x,y+1,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y+1,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                    {
                        canCreate = true;
                    }
                }
            }
            else
            {
                if (y < WORLD_HEIGHT - 1)
                {
                    Block1 = GetBlock(x,y+1,z);
                    if (BlockTransparentOrSpecial(x,y+1,z) == true && GetBlock(x,y+1,z) != 93)
                    {
                        canCreate = true;
                    }
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if(canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y+1,z);
            BlockColory1 = Vector3(mainLight,mainLight,mainLight);
            light1 = light2 = light3 = light4 = BlockColory1;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }
            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,"top",Vector3(6,16,10),Vector3(10,16,10),Vector3(10,16,6),Vector3(6,16,6));
            left = percent*plane_x + point4.x*texturePixel;
            right = percent*plane_x + percent - ((16-point2.x)*texturePixel);

            up = percent*plane_y + (16-point2.z)*texturePixel;
            down = percent*plane_y + (16-point4.z)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);


            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 5)
    {

        Vector3 BlockColorz1;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y,z-1);

            if(transparent)
            {
                Block1 = 1;
                if (z > 0)
                {
                    transparentBlock = BlockTransparent(x,y,z-1);
                    Block1 = GetBlock(x,y,z-1);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y,z-1) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (z > 0)
                {
                    Block1 = GetBlock(x,y,z-1);
                    transparentBlock = BlockTransparentOrSpecial(x,y,z-1);
                    if (transparentBlock == true)
                        canCreate = true;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y,z-1);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorz1 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorz1 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;
            }
            light1 = light2 = light3 = light4 = BlockColorz1;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point4.x*texturePixel;
            right = percent*plane_x+percent - (16-point2.x)*texturePixel;

            up = percent*plane_y + (16-point2.y)*texturePixel;
            down = percent*plane_y + (16-point4.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 6)
    {

        Vector3 BlockColorz2;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y,z+1);

            if(transparent)
            {
                Block1 = 1;
                if (z < worldLimit - 1)
                {
                    transparentBlock = BlockTransparent(x,y,z+1);
                    Block1 = GetBlock(x,y,z+1);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y,z+1) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (z < worldLimit - 1)
                {
                    Block1 = GetBlock(x,y,z+1);
                    transparentBlock = BlockTransparentOrSpecial(x,y,z+1);
                    if (transparentBlock == true)
                        canCreate = true;
                }
                else
                {
                    canCreate = false;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate == true || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y,z+1);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorz2 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorz2 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;
            }

            light1 = light2 = light3 = light4 = BlockColorz2;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point1.x*texturePixel;
            right = percent*plane_x+percent - (16-point3.x)*texturePixel;

            up = percent*plane_y + (16-point3.y)*texturePixel;
            down = percent*plane_y + (16-point1.y)*texturePixel;

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(left, down);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(right, down);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(right, up);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(left, up);
            MeshChunk->colour(light2.x,light2.y,light2.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }
}

void CraftWorld::BuildWorldBlockPlane(BaseBlock *blockType, int x,int y, int z, int &iVertex, SimpleMeshChunk* MeshChunk, block_t Block, bool transparent, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4, bool alwaysBuild, bool constantColor, int texturePlane, int textureRotation)
{
    float pixel, texturePixel;

    pixel = (float)1/16.0f;
    texturePixel = (float)1/256.0f;

    bool canCreate = false;
    bool DefaultBlock = false;
    bool transparentBlock = false;

    float up = 0.0f;
    float down = 0.0f;
    float left = 0.0f;
    float right = 0.0f;

    block_t Block1;

    int plane_x = 0, plane_y = 0;
    switch(texturePlane)
    {
        case 0 : plane_x = blockType->leftPlane_x; plane_y = blockType->leftPlane_y;
        break;
        case 1 : plane_x = blockType->rightPlane_x; plane_y = blockType->rightPlane_y;
        break;
        case 2 : plane_x = blockType->downPlane_x; plane_y = blockType->downPlane_y;
        break;
        case 3 : plane_x = blockType->upPlane_x; plane_y = blockType->upPlane_y;
        break;
        case 4 : plane_x = blockType->botPlane_x; plane_y = blockType->botPlane_y;
        break;
        case 5 : plane_x = blockType->forPlane_x; plane_y = blockType->forPlane_y;
        break;
    }

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

    if(side == 1)
    {
        Vector3 BlockColorx1;

        if(!alwaysBuild)
        {
            Block1 = GetBlock(x-1,y,z);

            transparentBlock = DefaultBlock;
            canCreate = false;
            if(transparent)
            {
                Block1 = 1;
                if (x > 0)
                {
                    transparentBlock = BlockTransparent(x-1,y,z);
                    Block1 = GetBlock(x-1,y,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x-1,y,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (x > 0)
                {
                    Block1 = GetBlock(x-1,y,z);
                    transparentBlock = BlockTransparentOrSpecial(x-1,y,z);
                    if (transparentBlock == true)
                        canCreate = true;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x-1,y,z);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorx1 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorx1 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;
            }

            if(westLight != 0)
            {
                BlockColorx1.x += 0.3*westLight;
                BlockColorx1.y += 0.2*westLight;
            }

            light1 = light2 = light3 = light4 = BlockColorx1;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point4.z*texturePixel;
            right = percent*plane_x+percent - (16-point2.z)*texturePixel;

            up = percent*plane_y + (16-point2.y)*texturePixel;
            down = percent*plane_y + (16-point4.y)*texturePixel;

            float x1,x2,x3,x4,y1,y2,y3,y4;
            float x1t,x2t,x3t,x4t,y1t,y2t,y3t,y4t;
            x1 = x1t = right;
            y1 = y1t = down;

            x2 = x2t = right;
            y2 = y2t = up;

            x3 = x3t = left;
            y3 = y3t = up;

            x4 = x4t = left;
            y4 = y4t = down;

            switch(textureRotation)
            {
                case 1:
                x1 = x2t;
                y1 = y2t;

                x2 = x1t;
                y2 = y1t;

                x3 = x4t;
                y3 = y4t;

                x4 = x3t;
                y4 = y3t;
                break;

                case 2:
                x1 = x3t;
                y1 = y3t;

                x2 = x4t;
                y2 = y4t;

                x3 = x1t;
                y3 = y1t;

                x4 = x2t;
                y4 = y2t;
                break;

                case 3:
                x1 = x4t;
                y1 = y4t;

                x2 = x1t;
                y2 = y1t;

                x3 = x2t;
                y3 = y2t;

                x4 = x3t;
                y4 = y3t;
                break;
            }

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(x1, y1);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(x2, y2);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(x3, y3);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(x4, y4);
            MeshChunk->colour(light3.x,light3.y,light3.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 2)
    {

        Vector3 BlockColorx2;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x+1,y,z);

            transparentBlock = DefaultBlock;
            canCreate = false;
            if(transparent)
            {
                Block1 = 1;
                if (x < worldLimit - 1)
                {
                    transparentBlock = BlockTransparent(x+1,y,z);
                    Block1 = GetBlock(x+1,y,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x+1,y,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (x < worldLimit - 1)
                {
                    transparentBlock = BlockTransparentOrSpecial(x+1,y,z);
                    Block1 = GetBlock(x+1,y,z);
                    if (transparentBlock == true)
                        canCreate = true;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x+1,y,z);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorx2 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorx2 = Vector3(mainLight,mainLight,mainLight) * blockXConstLight;
            }

            if(eastLight != 0)
            {
                BlockColorx2.x += 0.3*eastLight;
                BlockColorx2.y += 0.2*eastLight;
            }

            light1 = light2 = light3 = light4 = BlockColorx2;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point1.z*texturePixel;
            right = percent*plane_x+percent - (16-point3.z)*texturePixel;

            up = percent*plane_y + (16-point3.y)*texturePixel;
            down = percent*plane_y + (16-point1.y)*texturePixel;

            float x1,x2,x3,x4,y1,y2,y3,y4;
            float x1t,x2t,x3t,x4t,y1t,y2t,y3t,y4t;
            x1 = x1t = right;
            y1 = y1t = down ;

            x2 = x2t = right;
            y2 = y2t = up;

            x3 = x3t = left;
            y3 = y3t = up;

            x4 = x4t = left;
            y4 = y4t = down;

            switch(textureRotation)
            {
                case 1:
                x1 = x2t;
                y1 = y2t;

                x2 = x3t;
                y2 = y3t;

                x3 = x3t;
                y3 = y3t;

                x4 = x1t;
                y4 = y1t;
                break;

                case 2:
                x1 = x3t;
                y1 = y3t;

                x2 = x4t;
                y2 = y4t;

                x3 = x1t;
                y3 = y1t;

                x4 = x2t;
                y4 = y2t;
                break;

                case 3:
                x1 = x4t;
                y1 = y4t;

                x2 = x1t;
                y2 = y1t;

                x3 = x2t;
                y3 = y2t;

                x4 = x3t;
                y4 = y3t;
                break;
            }

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(x1, y1);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(x2, y2);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(x3, y3);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(x4, y4);
            MeshChunk->colour(light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 3)
    {

        Vector3 BlockColory2;


        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y-1,z);
            transparentBlock = DefaultBlock;
            canCreate = false;
            if(transparent)
            {
                Block1 = 1;
                if (y > 0)
                {
                    transparentBlock = BlockTransparent(x,y-1,z);
                    Block1 = GetBlock(x,y-1,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y-1,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (y > 0)
                {
                    Block1 = GetBlock(x,y-1,z);
                    transparentBlock = BlockTransparentOrSpecial(x,y-1,z);
                    if (transparentBlock == true)
                        canCreate = true;
                }
                else
                {
                    canCreate = false;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y-1,z);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColory2 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColory2 = Vector3(mainLight,mainLight,mainLight) * 0.6f;
            }
            light1 = light2 = light3 = light4 = BlockColory2;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point1.x*texturePixel;
            right = percent*plane_x+percent - (16-point3.x)*texturePixel;

            up = percent*plane_y + (16-point1.z)*texturePixel;
            down = percent*plane_y + (16-point3.z)*texturePixel;

            float x1,x2,x3,x4,y1,y2,y3,y4;
            float x1t,x2t,x3t,x4t,y1t,y2t,y3t,y4t;
            x1 = x1t = left;
            y1 = y1t = down;

            x2 = x2t = right;
            y2 = y2t = down;

            x3 = x3t = right;
            y3 = y3t = up;

            x4 = x4t = left;
            y4 = y4t = up;

            switch(textureRotation)
            {
                case 1:
                x1 = x2t;
                y1 = y2t;

                x2 = x3t;
                y2 = y3t;

                x3 = x3t;
                y3 = y3t;

                x4 = x1t;
                y4 = y1t;
                break;

                case 2:
                x1 = x3t;
                y1 = y3t;

                x2 = x4t;
                y2 = y4t;

                x3 = x1t;
                y3 = y1t;

                x4 = x2t;
                y4 = y2t;
                break;

                case 3:
                x1 = x4t;
                y1 = y4t;

                x2 = x1t;
                y2 = y1t;

                x3 = x2t;
                y3 = y2t;

                x4 = x3t;
                y4 = y3t;
                break;
            }

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(x1, y1);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(x2, y2);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(x3, y3);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(x4, y4);
            MeshChunk->colour(light4.x,light4.y,light4.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 4)
    {

        Vector3 BlockColory1;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y+1,z);

            if(transparent)
            {
                Block1 = 1;
                if (y < WORLD_HEIGHT - 1)
                {
                    transparentBlock = BlockTransparent(x,y+1,z);
                    Block1 = GetBlock(x,y+1,z);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y+1,z) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                    {
                        canCreate = true;
                    }
                }
            }
            else
            {
                if (y < WORLD_HEIGHT - 1)
                {
                    Block1 = GetBlock(x,y+1,z);
                    if (BlockTransparentOrSpecial(x,y+1,z) == true && GetBlock(x,y+1,z) != 93)
                    {
                        canCreate = true;
                    }
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if(canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y+1,z);
            BlockColory1 = Vector3(mainLight,mainLight,mainLight);
            light1 = light2 = light3 = light4 = BlockColory1;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }
            //BuildWorldBlockPlane(blockType,x,y,z,iVertex,MeshChunk,Block,transparent,"top",Vector3(6,16,10),Vector3(10,16,10),Vector3(10,16,6),Vector3(6,16,6));
            left = percent*plane_x + point4.x*texturePixel;
            right = percent*plane_x + percent - ((16-point2.x)*texturePixel);

            up = percent*plane_y + (16-point2.z)*texturePixel;
            down = percent*plane_y + (16-point4.z)*texturePixel;

            float x1,x2,x3,x4,y1,y2,y3,y4;
            float x1t,x2t,x3t,x4t,y1t,y2t,y3t,y4t;
            x1 = x1t = left;
            y1 = y1t = down;

            x2 = x2t = right;
            y2 = y2t = down ;

            x3 = x3t = right;
            y3 = y3t = up;

            x4 = x4t = left;
            y4 = y4t = up;

            // The four cases below are quarter turns of the tile about the face normal plus, for the top
            // face, one default that is a quarter turn of its own: the UV origin is taken from the
            // geometry, so "no rotation" is not the identity for every side. Only the top face (side 4)
            // has a case 4, because only the redstone repeater asks for it: case 4 is the missing true
            // 180 degrees, which the old 0/1 pair could not express (it could only flip 90 degrees and
            // mirror), so a rotated repeater used to flip its torch row onto the wrong axis.
            switch(textureRotation)
            {
                case 1:
                x1 = x2t;
                y1 = y2t;

                x2 = x1t;
                y2 = y1t;

                x3 = x4t;
                y3 = y4t;

                x4 = x3t;
                y4 = y3t;
                break;

                case 2:
                x1 = x3t;
                y1 = y3t;

                x2 = x4t;
                y2 = y4t;

                x3 = x1t;
                y3 = y1t;

                x4 = x2t;
                y4 = y2t;
                break;

                case 3:
                x1 = x4t;
                y1 = y4t;

                x2 = x1t;
                y2 = y1t;

                x3 = x2t;
                y3 = y2t;

                x4 = x3t;
                y4 = y3t;
                break;

                case 4: // 180 degrees: +U = -X, +V = -Z
                x1 = x2t;
                y1 = y2t;

                x2 = x3t;
                y2 = y3t;

                x3 = x4t;
                y3 = y4t;

                x4 = x1t;
                y4 = y1t;
                break;
            }

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(x1, y1);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(x2, y2);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(x3, y3);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(x4, y4);
            MeshChunk->colour(light4.x,light4.y,light4.z);


            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 5)
    {

        Vector3 BlockColorz1;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y,z-1);

            if(transparent)
            {
                Block1 = 1;
                if (z > 0)
                {
                    transparentBlock = BlockTransparent(x,y,z-1);
                    Block1 = GetBlock(x,y,z-1);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y,z-1) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (z > 0)
                {
                    Block1 = GetBlock(x,y,z-1);
                    transparentBlock = BlockTransparentOrSpecial(x,y,z-1);
                    if (transparentBlock == true)
                        canCreate = true;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }

        if (canCreate || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y,z-1);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorz1 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorz1 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;
            }
            light1 = light2 = light3 = light4 = BlockColorz1;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point4.x*texturePixel;
            right = percent*plane_x+percent - (16-point2.x)*texturePixel;

            up = percent*plane_y + (16-point2.y)*texturePixel;
            down = percent*plane_y + (16-point4.y)*texturePixel;

            float x1,x2,x3,x4,y1,y2,y3,y4;
            float x1t,x2t,x3t,x4t,y1t,y2t,y3t,y4t;
            x1 = x1t = right;
            y1 = y1t = up;

            x2 = x2t = left;
            y2 = y2t = up;

            x3 = x3t = left;
            y3 = y3t = down;

            x4 = x4t = right;
            y4 = y4t = down;

            switch(textureRotation)
            {
                case 1:
                x1 = x2t;
                y1 = y2t;

                x2 = x3t;
                y2 = y3t;

                x3 = x3t;
                y3 = y3t;

                x4 = x1t;
                y4 = y1t;
                break;

                case 2:
                x1 = x3t;
                y1 = y3t;

                x2 = x4t;
                y2 = y4t;

                x3 = x1t;
                y3 = y1t;

                x4 = x2t;
                y4 = y2t;
                break;

                case 3:
                x1 = x4t;
                y1 = y4t;

                x2 = x1t;
                y2 = y1t;

                x3 = x2t;
                y3 = y2t;

                x4 = x3t;
                y4 = y3t;
                break;
            }

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(x1, y1);
            MeshChunk->colour(light2.x,light2.y,light2.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(x2, y2);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(x3, y3);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(x4, y4);
            MeshChunk->colour(light1.x,light1.y,light1.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }

    if(side == 6)
    {

        Vector3 BlockColorz2;
        if(!alwaysBuild)
        {
            Block1 = GetBlock(x,y,z+1);

            if(transparent)
            {
                Block1 = 1;
                if (z < worldLimit - 1)
                {
                    transparentBlock = BlockTransparent(x,y,z+1);
                    Block1 = GetBlock(x,y,z+1);
                    if ((transparentBlock == true && Block1 != Block) || Block1 == 0 || (BlockSpecial(x,y,z+1) && Block1 != 9 && Block1 != 4 && Block1 != 112 && Block1 != 38))
                        canCreate = true;
                }
            }
            else
            {
                if (z < worldLimit - 1)
                {
                    Block1 = GetBlock(x,y,z+1);
                    transparentBlock = BlockTransparentOrSpecial(x,y,z+1);
                    if (transparentBlock == true)
                        canCreate = true;
                }
                else
                {
                    canCreate = false;
                }
            }
            if((Block1 == LeavesBlock::getID() || Block1 == BirchLeaves::getID() || Block1 == SpruceLeaves::getID()) && mainOptions.fastRendering == 1)
            {
                canCreate = false;
            }
        }
        if (canCreate == true || alwaysBuild)
        {
            float mainLight = BlockFinalLight(x,y,z+1);
            if(constantColor == true)
            {
                mainLight = BlockFinalLight(x,y,z);
                BlockColorz2 = Vector3(mainLight,mainLight,mainLight);
            }
            else
            {
                BlockColorz2 = Vector3(mainLight,mainLight,mainLight) * blockZConstLight;
            }

            light1 = light2 = light3 = light4 = BlockColorz2;

            if(mainOptions.smoothLighting == true && !constantColor)
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
            if(constantColor)
            {
                mainLight = BlockFinalLight(x,y,z);
                light1 = light2 = light3 = light4 = Vector3(mainLight,mainLight,mainLight);
            }

            left = percent*plane_x + point1.x*texturePixel;
            right = percent*plane_x+percent - (16-point3.x)*texturePixel;

            up = percent*plane_y + (16-point3.y)*texturePixel;
            down = percent*plane_y + (16-point1.y)*texturePixel;

            float x1,x2,x3,x4,y1,y2,y3,y4;
            float x1t,x2t,x3t,x4t,y1t,y2t,y3t,y4t;
            x1 = x1t = left;
            y1 = y1t = down;

            x2 = x2t = right;
            y2 = y2t = down;

            x3 = x3t = right;
            y3 = y3t = up;

            x4 = x4t = left;
            y4 = y4t = up;

            switch(textureRotation)
            {
                case 1:
                x1 = x2t;
                y1 = y2t;

                x2 = x3t;
                y2 = y3t;

                x3 = x3t;
                y3 = y3t;

                x4 = x1t;
                y4 = y1t;
                break;

                case 2:
                x1 = x3t;
                y1 = y3t;

                x2 = x4t;
                y2 = y4t;

                x3 = x1t;
                y3 = y1t;

                x4 = x2t;
                y4 = y2t;
                break;

                case 3:
                x1 = x4t;
                y1 = y4t;

                x2 = x1t;
                y2 = y1t;

                x3 = x2t;
                y3 = y2t;

                x4 = x3t;
                y4 = y3t;
                break;
            }

            MeshChunk->position(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel);
            MeshChunk->textureCoord(x1, y1);
            MeshChunk->colour(light1.x,light1.y,light1.z);
            MeshChunk->position(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel);
            MeshChunk->textureCoord(x2, y2);
            MeshChunk->colour(light3.x,light3.y,light3.z);
            MeshChunk->position(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel);
            MeshChunk->textureCoord(x3, y3);
            MeshChunk->colour(light4.x,light4.y,light4.z);
            MeshChunk->position(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel);
            MeshChunk->textureCoord(x4, y4);
            MeshChunk->colour(light2.x,light2.y,light2.z);

            MeshChunk->triangle(iVertex, iVertex+1, iVertex+2);
            MeshChunk->triangle(iVertex+2, iVertex+3, iVertex);

            iVertex += 4;
        }
    }
}

void CraftWorld::BuildGUIBlockPlane(BaseBlock* blockType, char side, Vector3 point1, Vector3 point2, Vector3 point3, Vector3 point4)
{
    float pixel, texturePixel;

    pixel = (float)1/16.0f;
    texturePixel = (float)1/256.0f;

    float x = -0.5f;
    float y = -0.5f;
    float z = -0.5f;

    float down = 0.0f;
    float up = 0.0f;

    float left = 0.0f;
    float right = 0.0f;

    if(side == 1)
    {

        left = blockType->leftPlane_x * percent + point4.z*texturePixel;
        right = blockType->leftPlane_x * percent + percent - (16-point2.z)*texturePixel;

        up = percent*blockType->leftPlane_y + (16-point2.y)*texturePixel;
        down = percent*blockType->leftPlane_y + (16-point4.y)*texturePixel;

        mPositionLeftRight.push_back(new Vector3(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel));
        mtexturesLeftRight.push_back(new Vector2(right, down));
        mPositionLeftRight.push_back(new Vector3(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel));
        mtexturesLeftRight.push_back(new Vector2(right, up));
        mPositionLeftRight.push_back(new Vector3(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel));
        mtexturesLeftRight.push_back(new Vector2(left, up));
        mPositionLeftRight.push_back(new Vector3(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel));
        mtexturesLeftRight.push_back(new Vector2(left, down));

        mTriangleLeftRight.push_back(new Vector3(iVertexLeftRight, iVertexLeftRight+1, iVertexLeftRight+2));
        mTriangleLeftRight.push_back(new Vector3(iVertexLeftRight+2, iVertexLeftRight+3, iVertexLeftRight));

        iVertexLeftRight += 4;
        pointsLeftRight += 6;
    }

    if(side == 2)
    {

        left = blockType->rightPlane_x*percent + point1.z*texturePixel;
        right = blockType->rightPlane_x*percent + percent - (16-point3.z)*texturePixel;

        up = percent*blockType->rightPlane_y + (16-point3.y)*texturePixel;
        down = percent*blockType->rightPlane_y + (16-point1.y)*texturePixel;

        mPositionLeftRight.push_back(new Vector3(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel));
        mtexturesLeftRight.push_back(new Vector2(right, down));
        mPositionLeftRight.push_back(new Vector3(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel));
        mtexturesLeftRight.push_back(new Vector2(right, up));
        mPositionLeftRight.push_back(new Vector3(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel));
        mtexturesLeftRight.push_back(new Vector2(left, up));
        mPositionLeftRight.push_back(new Vector3(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel));
        mtexturesLeftRight.push_back(new Vector2(left, down));

        mTriangleLeftRight.push_back(new Vector3(iVertexLeftRight, iVertexLeftRight+1, iVertexLeftRight+2));
        mTriangleLeftRight.push_back(new Vector3(iVertexLeftRight+2, iVertexLeftRight+3, iVertexLeftRight));

        iVertexLeftRight += 4;
        pointsLeftRight += 6;
    }

    if(side == 3)
    {

        left = blockType->downPlane_x*percent + point1.x*texturePixel;
        right = blockType->downPlane_x*percent + percent - (16-point3.x)*texturePixel;

        up = percent*blockType->downPlane_y + (16-point1.z)*texturePixel;
        down = percent*blockType->downPlane_y + (16-point3.z)*texturePixel;

        mPositionBottomTop.push_back(new Vector3(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel));
        mtexturesBottomTop.push_back(new Vector2(left, up));
        mPositionBottomTop.push_back(new Vector3(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel));
        mtexturesBottomTop.push_back(new Vector2(right, up));
        mPositionBottomTop.push_back(new Vector3(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel));
        mtexturesBottomTop.push_back(new Vector2(right, down));
        mPositionBottomTop.push_back(new Vector3(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel));
        mtexturesBottomTop.push_back(new Vector2(left, down));

        mTriangleBottomTop.push_back(new Vector3(iVertexBottomTop, iVertexBottomTop+1, iVertexBottomTop+2));
        mTriangleBottomTop.push_back(new Vector3(iVertexBottomTop+2, iVertexBottomTop+3, iVertexBottomTop));

        iVertexBottomTop += 4;
        pointsBottomTop += 6;
    }

    if(side == 4)
    {

        left = blockType->upPlane_x*percent + point4.x*texturePixel;
        right = blockType->upPlane_x*percent + percent - (16-point2.x)*texturePixel;

        up = percent*blockType->upPlane_y + (16-point2.z)*texturePixel;
        down = percent*blockType->upPlane_y + (16-point4.z)*texturePixel;

        mPositionBottomTop.push_back(new Vector3(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel));
        mtexturesBottomTop.push_back(new Vector2(left, up));
        mPositionBottomTop.push_back(new Vector3(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel));
        mtexturesBottomTop.push_back(new Vector2(right, up));
        mPositionBottomTop.push_back(new Vector3(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel));
        mtexturesBottomTop.push_back(new Vector2(right, down));
        mPositionBottomTop.push_back(new Vector3(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel));
        mtexturesBottomTop.push_back(new Vector2(left, down));

        mTriangleBottomTop.push_back(new Vector3(iVertexBottomTop, iVertexBottomTop+1, iVertexBottomTop+2));
        mTriangleBottomTop.push_back(new Vector3(iVertexBottomTop+2, iVertexBottomTop+3, iVertexBottomTop));

        iVertexBottomTop += 4;
        pointsBottomTop += 6;
    }

    if(side == 5)
    {

        left = blockType->botPlane_x*percent + point4.x*texturePixel;
        right = blockType->botPlane_x*percent + percent - (16-point2.x)*texturePixel;

        up = percent*blockType->botPlane_y + (16-point2.y)*texturePixel;
        down = percent*blockType->botPlane_y + (16-point4.y)*texturePixel;

        if(blockType->name == "Furnace")
        {
            left = 12*percent;
            right = left+percent;

            up = 2*percent;
            down = up+percent;
        }

        mPositionBackFront.push_back(new Vector3(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel));
        mtexturesBackFront.push_back(new Vector2(right, up));
        mPositionBackFront.push_back(new Vector3(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel));
        mtexturesBackFront.push_back(new Vector2(left, up));
        mPositionBackFront.push_back(new Vector3(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel));
        mtexturesBackFront.push_back(new Vector2(left, down));
        mPositionBackFront.push_back(new Vector3(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel));
        mtexturesBackFront.push_back(new Vector2(right, down));

        mTriangleBackFront.push_back(new Vector3(iVertexBackFront, iVertexBackFront+1, iVertexBackFront+2));
        mTriangleBackFront.push_back(new Vector3(iVertexBackFront+2, iVertexBackFront+3, iVertexBackFront));

        iVertexBackFront += 4;
        pointsBackFront += 6;
    }

    if(side == 6)
    {
        left = blockType->forPlane_x*percent + point1.x*texturePixel;
        right = blockType->forPlane_x*percent + percent - (16-point3.x)*texturePixel;

        up = percent*blockType->forPlane_y + (16-point3.y)*texturePixel;
        down = percent*blockType->forPlane_y + (16-point1.y)*texturePixel;

        mPositionBackFront.push_back(new Vector3(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel));
        mtexturesBackFront.push_back(new Vector2(left, down));
        mPositionBackFront.push_back(new Vector3(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel));
        mtexturesBackFront.push_back(new Vector2(right, down));
        mPositionBackFront.push_back(new Vector3(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel));
        mtexturesBackFront.push_back(new Vector2(right, up));
        mPositionBackFront.push_back(new Vector3(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel));
        mtexturesBackFront.push_back(new Vector2(left, up));

        mTriangleBackFront.push_back(new Vector3(iVertexBackFront, iVertexBackFront+1, iVertexBackFront+2));
        mTriangleBackFront.push_back(new Vector3(iVertexBackFront+2, iVertexBackFront+3, iVertexBackFront));

        iVertexBackFront += 4;
        pointsBackFront += 6;
    }

    // A flat inventory icon: one quad covering the whole cell, showing the block's top tile. Java draws
    // the repeater's item as its flat top sprite rather than a 3D slab, and that sprite is the one
    // players recognise -- a smooth slab with the torch row baked into it, seen face on. It needs its own
    // side because the world side quads tie their U to the quad's own corner coordinates, which mirrors
    // the tile; a top-tile icon wants the tile mapped straight across, its first row at the top.
    if(side == 7)
    {
        left  = blockType->upPlane_x * percent;
        right = left + percent;
        up    = blockType->upPlane_y * percent;
        down  = up + percent;

        mPositionBackFront.push_back(new Vector3(x+point1.x*pixel, y+point1.y*pixel, z+point1.z*pixel));
        mtexturesBackFront.push_back(new Vector2(left, up));
        mPositionBackFront.push_back(new Vector3(x+point2.x*pixel, y+point2.y*pixel, z+point2.z*pixel));
        mtexturesBackFront.push_back(new Vector2(right, up));
        mPositionBackFront.push_back(new Vector3(x+point3.x*pixel, y+point3.y*pixel, z+point3.z*pixel));
        mtexturesBackFront.push_back(new Vector2(right, down));
        mPositionBackFront.push_back(new Vector3(x+point4.x*pixel, y+point4.y*pixel, z+point4.z*pixel));
        mtexturesBackFront.push_back(new Vector2(left, down));

        mTriangleBackFront.push_back(new Vector3(iVertexBackFront, iVertexBackFront+1, iVertexBackFront+2));
        mTriangleBackFront.push_back(new Vector3(iVertexBackFront+2, iVertexBackFront+3, iVertexBackFront));

        iVertexBackFront += 4;
        pointsBackFront += 6;
    }
}
