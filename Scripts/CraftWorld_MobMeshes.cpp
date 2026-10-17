#include "CraftWorld2.h"
#include <malloc.h>

void CraftWorld::buildmobVerts()
{
    std::vector<Vector3*> mPosition;
    std::vector<Vector2*> mtextures;
    std::vector<Vector3*> mTriangle;

    int iVertex = 0;
    int points = 0;
    unsigned int size = 0;

    float pixel = 1 / 16.0f;
    float textureYpixel;
    float textureXpixel;

    float x = pixel * -2;
    float y = pixel * -6;
    float z = pixel * -4;

    float down = 0.0f;
    float up = 0.0f;

    float left = 0.0f;
    float right = 0.0f;

    int vert = 0;

    /// * --- BODY --- *

    left = 0.5f;
    right = left + 0.125f;

    down = 0.625f;
    up = down + 0.375f;

    mPosition.push_back(new Vector3(x, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.3125f;
    right = left + 0.125f;

    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.4375f;
    right = left + 0.125f;

    down = 0.5f;
    up = down + 0.125f;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y, z+pixel*8));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.3125f;
    right = left + 0.125f;

    down = 0.5f;
    up = down + 0.125f;

    mPosition.push_back(new Vector3(x,   y+pixel*12, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.25f;
    right = left + 0.0625f;

    down = 0.625f;
    up = down + 0.375f;

    mPosition.push_back(new Vector3(x,   y+pixel*12, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.4375f;
    right = left + 0.0625f;

    down = 0.625f;
    up = down + 0.375f;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*12, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    bodyVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices

    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        bodyVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        bodyVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        bodyVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        bodyVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        bodyVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        bodyVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        bodyVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        bodyVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        bodyVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        bodyVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        bodyVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        bodyVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        bodyVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        bodyVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        bodyVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(bodyVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- HEAD --- *

    x = pixel * -4;
    y = pixel * -4;
    z = pixel * -4;

    vert = 0;
    iVertex = 0;
    points = 0;
    size = 0;

    left = 0.375f;
    right = left + 0.125f;

    down = 0.25;
    up = down + 0.25;

    mPosition.push_back(new Vector3(x, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.125f;
    right = left + 0.125f;

    mPosition.push_back(new Vector3(x+pixel*8, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.25f;
    right = left + 0.125f;

    down = 0;
    up = down + 0.25f;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y, z+pixel*8));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.125f;
    right = left + 0.125f;

    down = 0;
    up = down + 0.25f;

    mPosition.push_back(new Vector3(x,   y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*8, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.125;
    right = 0;

    down = 0.25;
    up = down + 0.25;

    mPosition.push_back(new Vector3(x,   y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.375;
    right = 0.25;

    down = 0.25;
    up = down + 0.25;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y,   z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    headVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        headVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        headVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        headVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        headVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        headVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        headVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        headVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        headVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        headVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        headVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        headVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        headVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        headVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        headVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        headVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(headVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();



    /// * --- HAT --- *

    x = pixel * -4.5f;
    y = pixel * -4.5f;
    z = pixel * -4.5f;

    vert = 0;
    iVertex = 0;
    points = 0;
    size = 0;

    left = 0.875f;
    right = left + 0.125f;

    down = 0.25;
    up = down + 0.25;

    mPosition.push_back(new Vector3(x, y, z+pixel*9));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*9, z+pixel*9));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*9, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.625f;
    right = left + 0.125f;

    mPosition.push_back(new Vector3(x+pixel*9, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*9, y+pixel*9, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*9, y+pixel*9, z+pixel*9));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*9, y,   z+pixel*9));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f;
    right = left + 0.125f;

    down = 0;
    up = down + 0.25f;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*9, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*9, y, z+pixel*9));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y, z+pixel*9));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.625f;
    right = left + 0.125f;

    down = 0;
    up = down + 0.25f;

    mPosition.push_back(new Vector3(x,   y+pixel*9, z+pixel*9));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*9, y+pixel*9, z+pixel*9));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*9, y+pixel*9, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*9, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.625;
    right = 0.5;

    down = 0.25;
    up = down + 0.25;

    mPosition.push_back(new Vector3(x,   y+pixel*9, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*9, y+pixel*9, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*9, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.875;
    right = 0.75;

    down = 0.25;
    up = down + 0.25;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*9));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*9, y,   z+pixel*9));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*9, y+pixel*9, z+pixel*9));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*9, z+pixel*9));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    hatVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        hatVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        hatVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        hatVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        hatVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        hatVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        hatVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        hatVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        hatVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        hatVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        hatVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        hatVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        hatVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        hatVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        hatVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        hatVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(hatVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();



    /// * --- HAND --- *

    x = pixel * -2;
    y = pixel * -2;
    z = pixel * -2;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = 0.8125f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.6875f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.6875f;
    right = left + 0.0625f;

    down = 0.5;
    up = down + 0.125;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f;
    right = left + 0.0625f;

    down = 0.5;
    up = down + 0.125;

    mPosition.push_back(new Vector3(x,   y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*12, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.625f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    handVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        handVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        handVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        handVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        handVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        handVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        handVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        handVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        handVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        handVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        handVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        handVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        handVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        handVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        handVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        handVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(handVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();




    /// * --- LEG --- *

    x = pixel * -2;
    y = pixel * -2;
    z = pixel * -2;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = 0.8125f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.6875 - 0.625f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.6875f - 0.625;
    right = left + 0.0625f;

    down = 0.5;
    up = down + 0.125;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f - 0.625;
    right = left + 0.0625f;

    down = 0.5;
    up = down + 0.125;

    mPosition.push_back(new Vector3(x,   y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*12, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.625f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    legVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        legVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        legVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        legVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        legVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        legVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        legVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        legVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        legVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        legVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        legVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        legVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        legVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        legVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        legVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        legVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(legVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();



    /// * --- cowBodyVertices --- *

    x = pixel * -9;
    y = pixel * -5;
    z = pixel * -6;

    vert = 0;
    iVertex = 0;
    points = 0;
    size = 0;

    left = 40.0f/64.0f;
    right = left + 12.0f/64.0f;

    down = 4.0f/32.0f;
    up = down + 10.0f/32.0f;

    mPosition.push_back(new Vector3(x, y, z+pixel*12));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*10, z+pixel*12));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*10, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    //face
    left = 28.0f/64.0f;
    right = left + 12.0f/64.0f;

    down = 4.0f/32.0f;
    up = down + 10.0f/32.0f;

    mPosition.push_back(new Vector3(x+pixel*18, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*18, y+pixel*10, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*18, y+pixel*10, z+pixel*12));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*18, y,   z+pixel*12));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    // bottom
    left = 28.0f/64.0f;
    right = left + 12.0f/64.0f;

    down = 14.0f/32.0f;
    up = down + 18.0f/32.0f;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*18, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*18, y, z+pixel*12));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*12));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    // top
    left = 50.0f/64.0f;
    right = left + 12.0f/64.0f;

    down = 14.0f/32.0f;
    up = down + 18.0f/32.0f;

    mPosition.push_back(new Vector3(x,   y+pixel*10, z+pixel*12));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*18, y+pixel*10, z+pixel*12));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*18, y+pixel*10, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y+pixel*10, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    // left
    left = 18.0f/64.0f;
    right = left + 10.0f/64.0f;

    down = 14.0f/32.0f;
    up = down + 18.0f/32.0f;

    mPosition.push_back(new Vector3(x,   y+pixel*10, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*18, y+pixel*10, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*18, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    // right
    left = 40.0f/64.0f;
    right = left + 10.0f/64.0f;

    down = 14.0f/32.0f;
    up = down + 18.0f/32.0f;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*12));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*18, y,   z+pixel*12));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*18, y+pixel*10, z+pixel*12));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*10, z+pixel*12));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    cowBodyVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices

    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        cowBodyVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        cowBodyVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        cowBodyVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        cowBodyVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        cowBodyVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        cowBodyVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        cowBodyVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        cowBodyVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        cowBodyVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        cowBodyVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        cowBodyVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        cowBodyVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        cowBodyVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        cowBodyVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        cowBodyVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(cowBodyVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();



    /// * --- cowHeadVertices --- *

    x = pixel * -3;
    y = pixel * -4;
    z = pixel * -4;

    vert = 0;
    iVertex = 0;
    points = 0;
    size = 0;

    left = 20.0f/64.0f;
    right = left + 8.0f/64.0f;

    down = 6.0f/32.0f;
    up = down + 8.0f/32.0f;

    mPosition.push_back(new Vector3(x, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    //face
    left = 6.0f/64.0f;
    right = left + 8.0f/64.0f;

    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 14.0f/64.0f;
    right = left + 8.0f/64.0f;

    down = 0;
    up = down + 6.0f/32.0f;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y, z+pixel*8));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 6.0f/64.0f;
    right = left + 8.0f/64.0f;

    down = 0;
    up = down + 6.0f/32.0f;

    mPosition.push_back(new Vector3(x,   y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*8, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    left = 6.0f/64.0f;
    right = 0;

    down = 6.0f/32.0f;
    up = down + 8.0f/32.0f;

    mPosition.push_back(new Vector3(x,   y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    left = 14.0f/64.0f;
    right = 20.0f/64.0f;

    down = 6.0f/32.0f;
    up = down + 8.0f/32.0f;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    cowHeadVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        cowHeadVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        cowHeadVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        cowHeadVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        cowHeadVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        cowHeadVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        cowHeadVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        cowHeadVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        cowHeadVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        cowHeadVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        cowHeadVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        cowHeadVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        cowHeadVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        cowHeadVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        cowHeadVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        cowHeadVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(cowHeadVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();



    /// * --- cowHornsVertices --- *

    x = pixel * -0.5f;
    y = pixel * -2.0f;
    z = pixel * -0.5f;

    vert = 0;
    iVertex = 0;
    points = 0;
    size = 0;

    textureXpixel = 1/64.0f;
    textureYpixel = 1/32.0f;

    left = textureXpixel*22;
    right = left + textureXpixel;

    down = textureYpixel;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x, y, z+pixel*1));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*3, z+pixel*1));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*3, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*23;
    right = left + textureXpixel;

    mPosition.push_back(new Vector3(x+pixel*1, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z+pixel*1));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*1, y,   z+pixel*1));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*24;
    right = left + textureXpixel;

    down = 0;
    up = down + textureYpixel;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*1, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*1, y, z+pixel*1));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y, z+pixel*1));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*23;
    right = left + textureXpixel;

    down = 0;
    up = down + textureYpixel;

    mPosition.push_back(new Vector3(x,   y+pixel*3, z+pixel*1));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z+pixel*1));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*3, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    left = textureXpixel*24;
    right = left + textureXpixel;

    down = textureYpixel;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x,   y+pixel*3, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*1, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    left = textureXpixel*25;
    right = left + textureXpixel;

    down = textureYpixel;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*1));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*1, y,   z+pixel*1));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z+pixel*1));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*3, z+pixel*1));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    cowHornsVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        cowHornsVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        cowHornsVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        cowHornsVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        cowHornsVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        cowHornsVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        cowHornsVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        cowHornsVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        cowHornsVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        cowHornsVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        cowHornsVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        cowHornsVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        cowHornsVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        cowHornsVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        cowHornsVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        cowHornsVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(cowHornsVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();


    /// * --- cowLegVertices --- *


    x = pixel * -2;
    y = pixel * -2;
    z = pixel * -2;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = 0.8125f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.6875 - 0.625f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f - 0.625;
    right = left + 0.0625f;

    down = 0.5;
    up = down + 0.125;

    mPosition.push_back(new Vector3(x,   y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*12, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.625f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*12, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    left = 0.6875f - 0.625;
    right = left + 0.0625f;

    down = 0.5;
    up = down + 0.125;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    cowLegVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        cowLegVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        cowLegVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        cowLegVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        cowLegVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        cowLegVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        cowLegVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        cowLegVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        cowLegVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        cowLegVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        cowLegVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        cowLegVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        cowLegVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        cowLegVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        cowLegVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        cowLegVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(cowLegVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- creeperLegVertices --- *

    textureXpixel = 1/64.0f;
    textureYpixel = 1/32.0f;

    x = pixel * -2;
    y = pixel * 0;
    z = pixel * -2;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = textureXpixel*12;
    right = left + textureXpixel*4;

    down = textureYpixel*20;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*4;
    right = left + textureXpixel*4;

    down = textureYpixel*20;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*4;
    right = left + textureXpixel*4;

    down = textureYpixel*16;
    up = down + textureYpixel*4;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*8;
    right = left + textureXpixel*4;

    down = textureYpixel*16;
    up = down + textureYpixel*4;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0;
    right = left + textureXpixel*4;

    down = textureYpixel*20;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*8;
    right = left + textureXpixel*4;

    down = textureYpixel*20;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    creeperLegVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        creeperLegVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        creeperLegVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        creeperLegVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        creeperLegVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        creeperLegVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        creeperLegVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        creeperLegVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        creeperLegVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        creeperLegVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        creeperLegVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        creeperLegVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        creeperLegVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        creeperLegVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        creeperLegVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        creeperLegVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(creeperLegVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();


    /// * --- pigBodyVertices --- *  (classic ModelPig body 10x16x8 @28,8, baked length=16 height=8 width=10)

    textureXpixel = 1/64.0f;
    textureYpixel = 1/32.0f;

    x = pixel * -8;
    y = pixel * -4;
    z = pixel * -5;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // -X rear (ModelBox down face 10x8)
    left = textureXpixel*36;
    right = left + textureXpixel*10;
    down = textureYpixel*8;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X front (ModelBox up face 10x8)
    left = textureXpixel*46;
    right = left + textureXpixel*10;
    down = textureYpixel*8;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x+pixel*16, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*16, y,   z+pixel*10));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // bottom (north 10x16)
    left = textureXpixel*36;
    right = left + textureXpixel*10;
    down = textureYpixel*16;
    up = down + textureYpixel*16;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*16, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*16, y, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*10));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // top (south 10x16)
    left = textureXpixel*54;
    right = left + textureXpixel*10;
    down = textureYpixel*16;
    up = down + textureYpixel*16;

    mPosition.push_back(new Vector3(x,   y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*8, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z (west 8x16)
    left = textureXpixel*28;
    right = left + textureXpixel*8;
    down = textureYpixel*16;
    up = down + textureYpixel*16;

    mPosition.push_back(new Vector3(x,   y+pixel*8, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*16, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z (east 8x16)
    left = textureXpixel*46;
    right = left + textureXpixel*8;
    down = textureYpixel*16;
    up = down + textureYpixel*16;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*10));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*16, y,   z+pixel*10));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    pigBodyVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        pigBodyVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        pigBodyVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        pigBodyVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        pigBodyVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        pigBodyVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        pigBodyVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        pigBodyVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        pigBodyVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        pigBodyVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        pigBodyVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        pigBodyVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        pigBodyVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        pigBodyVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        pigBodyVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        pigBodyVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(pigBodyVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
        delete mTriangle[aa];
    mTriangle.clear();


    /// * --- pigHeadVertices --- *  (8x8x8 @0,0)

    x = pixel * -4;
    y = pixel * -4;
    z = pixel * -4;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // -X back
    left = textureXpixel*24;
    right = left + textureXpixel*8;
    down = textureYpixel*8;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X face
    left = textureXpixel*8;
    right = left + textureXpixel*8;
    down = textureYpixel*8;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x+pixel*8, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // bottom
    left = textureXpixel*16;
    right = left + textureXpixel*8;
    down = 0;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y, z+pixel*8));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // top
    left = textureXpixel*8;
    right = left + textureXpixel*8;
    down = 0;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x,   y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*8, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z
    left = 0;
    right = textureXpixel*8;
    down = textureYpixel*8;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x,   y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z
    left = textureXpixel*16;
    right = textureXpixel*24;
    down = textureYpixel*8;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y,   z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    pigHeadVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        pigHeadVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        pigHeadVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        pigHeadVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        pigHeadVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        pigHeadVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        pigHeadVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        pigHeadVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        pigHeadVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        pigHeadVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        pigHeadVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        pigHeadVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        pigHeadVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        pigHeadVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        pigHeadVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        pigHeadVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(pigHeadVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
        delete mTriangle[aa];
    mTriangle.clear();


    /// * --- pigSnoutVertices --- *  (ModelBox 4x3x1 @16,16; +X = forward)
    // UV: top(17,16) 4x1, bottom(21,16) 4x1, west(16,17) 1x3, front(17,17) 4x3,
    //     east(21,17) 1x3, back(22,17) 4x3
    // Y is mirrored vs Java ModelPig (0..3): our head face maps eyes upper, so snout sits at -3..0.

    x = 0;
    y = pixel * -3;
    z = pixel * -2;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // -X back (against head) @22,17 4x3
    left = textureXpixel*22;
    right = left + textureXpixel*4;
    down = textureYpixel*17;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*3, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*3, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X front (nostrils) @17,17 4x3
    left = textureXpixel*17;
    right = left + textureXpixel*4;
    down = textureYpixel*17;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x+pixel*1, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*1, y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // bottom @21,16 4x1
    left = textureXpixel*21;
    right = left + textureXpixel*4;
    down = textureYpixel*16;
    up = down + textureYpixel*1;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*1, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*1, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // top @17,16 4x1
    left = textureXpixel*17;
    right = left + textureXpixel*4;
    down = textureYpixel*16;
    up = down + textureYpixel*1;

    mPosition.push_back(new Vector3(x,   y+pixel*3, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*3, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z west @16,17 1x3
    left = textureXpixel*16;
    right = left + textureXpixel*1;
    down = textureYpixel*17;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x,   y+pixel*3, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*1, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z east @21,17 1x3
    left = textureXpixel*21;
    right = left + textureXpixel*1;
    down = textureYpixel*17;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*1, y,   z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*1, y+pixel*3, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*3, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    pigSnoutVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        pigSnoutVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        pigSnoutVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        pigSnoutVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        pigSnoutVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        pigSnoutVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        pigSnoutVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        pigSnoutVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        pigSnoutVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        pigSnoutVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        pigSnoutVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        pigSnoutVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        pigSnoutVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        pigSnoutVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        pigSnoutVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        pigSnoutVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(pigSnoutVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
        delete mTriangle[aa];
    mTriangle.clear();

    /// * --- pigLegVertices --- *  (4x6x4 @0,16)

    x = pixel * -2;
    y = pixel * 0;
    z = pixel * -2;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = textureXpixel*12;
    right = left + textureXpixel*4;
    down = textureYpixel*20;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    left = textureXpixel*4;
    right = left + textureXpixel*4;
    down = textureYpixel*20;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // foot sole @4,16 4x4 — UV rotated 90° (prior orientation was correct)
    left = textureXpixel*4;
    right = left + textureXpixel*4;
    down = textureYpixel*16;
    up = down + textureYpixel*4;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // top of leg @8,16 4x4
    left = textureXpixel*8;
    right = left + textureXpixel*4;
    down = textureYpixel*16;
    up = down + textureYpixel*4;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    left = 0;
    right = left + textureXpixel*4;
    down = textureYpixel*20;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    left = textureXpixel*8;
    right = left + textureXpixel*4;
    down = textureYpixel*20;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y,   z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    pigLegVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        pigLegVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        pigLegVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        pigLegVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        pigLegVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        pigLegVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        pigLegVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        pigLegVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        pigLegVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        pigLegVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        pigLegVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        pigLegVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        pigLegVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        pigLegVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        pigLegVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        pigLegVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(pigLegVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
        delete mTriangle[aa];
    mTriangle.clear();

/// * --- sheepBodyVertices --- *

    textureXpixel = 1/64.0f;
    textureYpixel = 1/32.0f;

    x = pixel * -8;
    y = pixel * -3;
    z = pixel * -4;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = textureXpixel*34;
    right = left + textureXpixel*8;

    down = textureYpixel*8;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*42;
    right = left + textureXpixel*8;

    down = textureYpixel*8;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x+pixel*16, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*6, z+pixel*8));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*16, y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*28;
    right = left + textureXpixel*8;

    down = textureYpixel*14;
    up = down + textureYpixel*16;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*16, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*16, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*42;
    right = left + textureXpixel*8;

    down = textureYpixel*14;
    up = down + textureYpixel*16;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*8));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*6, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*36;
    right = left + textureXpixel*6;

    down = textureYpixel*14;
    up = down + textureYpixel*16;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*16, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*50;
    right = left + textureXpixel*6;

    down = textureYpixel*14;
    up = down + textureYpixel*16;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*16, y,   z+pixel*8));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*16, y+pixel*6, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    sheepBodyVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        sheepBodyVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        sheepBodyVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        sheepBodyVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        sheepBodyVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        sheepBodyVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        sheepBodyVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        sheepBodyVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        sheepBodyVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        sheepBodyVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        sheepBodyVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        sheepBodyVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        sheepBodyVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        sheepBodyVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        sheepBodyVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        sheepBodyVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(sheepBodyVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();




    /// * --- sheepHeadFurVertices --- *

    textureXpixel = 1/64.0f;
    textureYpixel = 1/32.0f;

    x = pixel * -3;
    y = pixel * -3;
    z = pixel * -3;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = textureXpixel*18;
    right = left + textureXpixel*6;

    down = textureYpixel*12;
    up = down - textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*6;
    right = left + textureXpixel*6;

    down = textureYpixel*12;
    up = down - textureYpixel*6;

    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*12;
    right = left + textureXpixel*6;

    down = textureYpixel*0;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*6;
    right = left + textureXpixel*6;

    down = textureYpixel*0;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*6;
    right = 0;

    down = textureYpixel*6;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*18;
    right = left - textureXpixel*6;

    down = textureYpixel*6;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*6));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    sheepHeadFurVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        sheepHeadFurVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        sheepHeadFurVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        sheepHeadFurVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        sheepHeadFurVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        sheepHeadFurVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        sheepHeadFurVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        sheepHeadFurVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        sheepHeadFurVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        sheepHeadFurVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        sheepHeadFurVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        sheepHeadFurVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        sheepHeadFurVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        sheepHeadFurVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        sheepHeadFurVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        sheepHeadFurVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(sheepHeadFurVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();




    /// * --- sheepHeadVertices --- *

    textureXpixel = 1/64.0f;
    textureYpixel = 1/32.0f;

    x = pixel * -4;
    y = pixel * -3;
    z = pixel * -3;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = textureXpixel*8;
    right = left + textureXpixel*6;

    down = textureYpixel*14;
    up = down - textureYpixel*6;

    mPosition.push_back(new Vector3(x+pixel*8, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y,   z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*14;
    right = left + textureXpixel*6;

    down = textureYpixel*0;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*8;
    right = left + textureXpixel*6;

    down = textureYpixel*0;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*8;
    right = 0;

    down = textureYpixel*8;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*22;
    right = left - textureXpixel*8;

    down = textureYpixel*8;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*6));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y,   z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = textureXpixel*22;
    right = left + textureXpixel*6;

    down = textureYpixel*14;
    up = down - textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    sheepHeadVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        sheepHeadVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        sheepHeadVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        sheepHeadVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        sheepHeadVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        sheepHeadVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        sheepHeadVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        sheepHeadVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        sheepHeadVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        sheepHeadVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        sheepHeadVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        sheepHeadVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        sheepHeadVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        sheepHeadVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        sheepHeadVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        sheepHeadVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(sheepHeadVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();



    /// * --- sheepLegFurVertices --- *


    x = pixel * -2;
    y = pixel * -2;
    z = pixel * -2;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = 0.8125f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.1875f;
    down += 0.03125*0.4f;

    mPosition.push_back(new Vector3(x, y+pixel*0.4f, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y+pixel*0.4f,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.6875 - 0.625f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.1875f;
    down += 0.03125*0.4f;

    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*0.4f,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*0.4f,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f - 0.625;
    right = left + 0.0625f;

    down = 0.5;
    up = down + 0.125;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.625f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.1875f;
    down += 0.03125*0.4f;

    mPosition.push_back(new Vector3(x,   y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*0.4f,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y+pixel*0.4f,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.1875f;
    down += 0.03125*0.4f;

    mPosition.push_back(new Vector3(x,   y+pixel*0.4f,   z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*0.4f,   z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*4, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    sheepLegFurVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        sheepLegFurVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        sheepLegFurVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        sheepLegFurVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        sheepLegFurVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        sheepLegFurVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        sheepLegFurVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        sheepLegFurVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        sheepLegFurVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        sheepLegFurVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        sheepLegFurVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        sheepLegFurVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        sheepLegFurVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        sheepLegFurVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        sheepLegFurVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        sheepLegFurVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(sheepLegFurVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();



    /// * --- helmetVertices --- *

    x = pixel * -5;
    y = pixel * -5;
    z = pixel * -5;

    vert = 0;
    iVertex = 0;
    points = 0;
    size = 0;

    left = 0.375f;
    right = left + 0.125f;

    down = 0.25;
    up = down + 0.25;

    mPosition.push_back(new Vector3(x, y, z+pixel*10));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*10, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*10, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.125f;
    right = left + 0.125f;

    mPosition.push_back(new Vector3(x+pixel*10, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*10, y+pixel*10, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*10, y+pixel*10, z+pixel*10));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*10, y,   z+pixel*10));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;



    left = 0.125f;
    right = left + 0.125f;

    down = 0;
    up = down + 0.25f;

    mPosition.push_back(new Vector3(x,   y+pixel*10, z+pixel*10));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*10, y+pixel*10, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*10, y+pixel*10, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*10, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;



    left = 0.125;
    right = 0;

    down = 0.25;
    up = down + 0.25;

    mPosition.push_back(new Vector3(x,   y+pixel*10, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*10, y+pixel*10, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*10, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;



    left = 0.375;
    right = 0.25;

    down = 0.25;
    up = down + 0.25;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*10));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*10, y,   z+pixel*10));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*10, y+pixel*10, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*10, z+pixel*10));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    helmetVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        helmetVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        helmetVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        helmetVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        helmetVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        helmetVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        helmetVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        helmetVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        helmetVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        helmetVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        helmetVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        helmetVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        helmetVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        helmetVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        helmetVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        helmetVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(helmetVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();





    /// * --- shoulderVertices --- *

    x = pixel * -3;
    y = pixel * -3;
    z = pixel * -3;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = 0.8125f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*14, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*14, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.6875f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.6875f;
    right = left + 0.0625f;

    down = 0.5;
    up = down + 0.125;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    left = 0.625f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y+pixel*14, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*6));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*14, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    shoulderVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        shoulderVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        shoulderVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        shoulderVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        shoulderVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        shoulderVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        shoulderVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        shoulderVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        shoulderVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        shoulderVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        shoulderVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        shoulderVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        shoulderVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        shoulderVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        shoulderVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        shoulderVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(shoulderVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();






    /// * --- chestplateVertices --- *

    x = pixel * -3;
    y = pixel * -7;
    z = pixel * -5;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = 0.5f;
    right = left + 0.125f;

    down = 0.625f;
    up = down + 0.375f;

    mPosition.push_back(new Vector3(x, y, z+pixel*10));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*14, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*14, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.3125f;
    right = left + 0.125f;

    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z+pixel*10));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*10));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    left = 0.25f;
    right = left + 0.0625f;

    down = 0.625f;
    up = down + 0.375f;

    mPosition.push_back(new Vector3(x,   y+pixel*14, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.4375f;
    right = left + 0.0625f;

    down = 0.625f;
    up = down + 0.375f;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*10));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*10));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*14, z+pixel*10));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    chestplateVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices

    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        chestplateVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        chestplateVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        chestplateVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        chestplateVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        chestplateVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        chestplateVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        chestplateVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        chestplateVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        chestplateVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        chestplateVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        chestplateVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        chestplateVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        chestplateVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        chestplateVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        chestplateVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(chestplateVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();




    /// * --- bellyVertices --- *

    x = pixel * -2.5f;
    y = pixel * -6;
    z = pixel * -4.5f;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = 0.5f;
    right = left + 0.125f;

    down = 0.625f;
    up = down + 0.375f;

    mPosition.push_back(new Vector3(x, y, z+pixel*9));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*9));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.3125f;
    right = left + 0.125f;

    mPosition.push_back(new Vector3(x+pixel*5, y,   z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*5, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*5, y+pixel*12, z+pixel*9));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*5, y,   z+pixel*9));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.25f;
    right = left + 0.0625f;

    down = 0.625f;
    up = down + 0.375f;

    mPosition.push_back(new Vector3(x,   y+pixel*12, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*5, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*5, y,   z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.4375f;
    right = left + 0.0625f;

    down = 0.625f;
    up = down + 0.375f;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*9));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*5, y,   z+pixel*9));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*5, y+pixel*12, z+pixel*9));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y+pixel*12, z+pixel*9));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    bellyVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices

    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        bellyVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        bellyVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        bellyVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        bellyVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        bellyVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        bellyVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        bellyVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        bellyVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        bellyVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        bellyVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        bellyVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        bellyVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        bellyVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        bellyVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        bellyVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(bellyVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();



    /// * --- leggingVertices --- *

    x = pixel * -2.5f;
    y = pixel * -2.5f;
    z = pixel * -2.5f;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = 0.8125f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x, y, z+pixel*5));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*13, z+pixel*5));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*13, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.6875 - 0.625f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x+pixel*5, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*5, y+pixel*13, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*5, y+pixel*13, z+pixel*5));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*5, y,   z+pixel*5));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    left = 0.625f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y+pixel*13, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*5, y+pixel*13, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*5, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*5));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*5, y,   z+pixel*5));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*5, y+pixel*13, z+pixel*5));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*13, z+pixel*5));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;



    leggingVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        leggingVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        leggingVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        leggingVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        leggingVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        leggingVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        leggingVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        leggingVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        leggingVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        leggingVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        leggingVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        leggingVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        leggingVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        leggingVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        leggingVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        leggingVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(leggingVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();




    /// * --- bootVertices --- *

    x = pixel * -3;
    y = pixel * -3;
    z = pixel * -3;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    left = 0.8125f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*14, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*14, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y,   z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;



    left = 0.6875 - 0.625f;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;



    left = 0.6875f - 0.625;
    right = left + 0.0625f;

    down = 0.5;
    up = down + 0.125;

    mPosition.push_back(new Vector3(x,   y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x,   y, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;



    left = 0.625f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y+pixel*14, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x,   y,   z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;

    left = 0.75f - 0.625;
    right = left + 0.0625f;

    down = 0.625;
    up = down + 0.375;

    mPosition.push_back(new Vector3(x,   y,   z+pixel*6));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y,   z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*14, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x,   y+pixel*14, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));

    iVertex += 4;
    points += 6;


    bootVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //build verts
    //vertices
    size =  mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        bootVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        bootVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        bootVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        bootVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        bootVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;

        bootVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        bootVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        bootVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        bootVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        bootVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;

        bootVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        bootVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        bootVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        bootVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        bootVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }

    //clear the cache or there will be some errors
    sceKernelDcacheWritebackInvalidateRange(bootVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    //sceKernelDcacheWritebackInvalidateAll();

    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();

    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete 	mTriangle[aa];
    }
    mTriangle.clear();


















    // ---- chicken ------------------------------------------------------------
    // Generated by _tmp_atlas/chicken_gen.py from vanilla ModelChicken (see that file
    // for the box table). The body is vanilla's 6x8x6 with its 90 degree render
    // rotation baked in, so it is 8 px fore-aft, 6 px tall and 6 px wide.

    textureXpixel = 1/64.0f;
    textureYpixel = 1/32.0f;

    /// * --- chickenBodyVertices --- *  (body)

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // body  (vanilla 6x8x6 @0,9 -> 8x6x6 px)
    x = pixel*-4; y = pixel*-3; z = pixel*-3;

    // -X  (ModelBox top 6x6)
    left = textureXpixel*6;
    right = left + textureXpixel*6;
    down = textureYpixel*9;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox bottom 6x6)
    left = textureXpixel*12;
    right = left + textureXpixel*6;
    down = textureYpixel*9;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x+pixel*8, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox front 6x8)
    left = textureXpixel*6;
    right = left + textureXpixel*6;
    down = textureYpixel*15;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox back 6x8)
    left = textureXpixel*18;
    right = left + textureXpixel*6;
    down = textureYpixel*15;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 6x8)
    left = textureXpixel*0;
    right = left + textureXpixel*6;
    down = textureYpixel*15;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 6x8)
    left = textureXpixel*12;
    right = left + textureXpixel*6;
    down = textureYpixel*15;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    chickenBodyVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        chickenBodyVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        chickenBodyVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        chickenBodyVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        chickenBodyVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        chickenBodyVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        chickenBodyVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        chickenBodyVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        chickenBodyVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        chickenBodyVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        chickenBodyVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        chickenBodyVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        chickenBodyVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        chickenBodyVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        chickenBodyVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        chickenBodyVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(chickenBodyVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- chickenHeadVertices --- *  (head + beak + wattle, pivoting on the neck)

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // head  (vanilla 4x6x3 @0,0 -> 3x6x4 px)
    x = pixel*-1; y = pixel*0; z = pixel*-2;

    // -X  (ModelBox back 4x6)
    left = textureXpixel*10;
    right = left + textureXpixel*4;
    down = textureYpixel*3;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 4x6)
    left = textureXpixel*3;
    right = left + textureXpixel*4;
    down = textureYpixel*3;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x+pixel*3, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*3, y, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 4x3)
    left = textureXpixel*7;
    right = left + textureXpixel*4;
    down = textureYpixel*0;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*3, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*3, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 4x3)
    left = textureXpixel*3;
    right = left + textureXpixel*4;
    down = textureYpixel*0;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 3x6)
    left = textureXpixel*0;
    right = left + textureXpixel*3;
    down = textureYpixel*3;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*3, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 3x6)
    left = textureXpixel*7;
    right = left + textureXpixel*3;
    down = textureYpixel*3;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*3, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // beak  (vanilla 4x2x2 @14,0 -> 2x2x4 px)
    x = pixel*2; y = pixel*2; z = pixel*-2;

    // -X  (ModelBox back 4x2)
    left = textureXpixel*22;
    right = left + textureXpixel*4;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 4x2)
    left = textureXpixel*16;
    right = left + textureXpixel*4;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 4x2)
    left = textureXpixel*20;
    right = left + textureXpixel*4;
    down = textureYpixel*0;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 4x2)
    left = textureXpixel*16;
    right = left + textureXpixel*4;
    down = textureYpixel*0;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 2x2)
    left = textureXpixel*14;
    right = left + textureXpixel*2;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 2x2)
    left = textureXpixel*20;
    right = left + textureXpixel*2;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z+pixel*4));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*4));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*4));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*4));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // wattle  (vanilla 2x2x2 @14,4 -> 2x2x2 px)
    x = pixel*1; y = pixel*0; z = pixel*-1;

    // -X  (ModelBox back 2x2)
    left = textureXpixel*20;
    right = left + textureXpixel*2;
    down = textureYpixel*6;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*2));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 2x2)
    left = textureXpixel*16;
    right = left + textureXpixel*2;
    down = textureYpixel*6;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*2));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*2));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 2x2)
    left = textureXpixel*18;
    right = left + textureXpixel*2;
    down = textureYpixel*4;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 2x2)
    left = textureXpixel*16;
    right = left + textureXpixel*2;
    down = textureYpixel*4;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*2));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*2));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 2x2)
    left = textureXpixel*14;
    right = left + textureXpixel*2;
    down = textureYpixel*6;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 2x2)
    left = textureXpixel*18;
    right = left + textureXpixel*2;
    down = textureYpixel*6;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z+pixel*2));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*2));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*2));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    chickenHeadVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        chickenHeadVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        chickenHeadVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        chickenHeadVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        chickenHeadVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        chickenHeadVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        chickenHeadVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        chickenHeadVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        chickenHeadVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        chickenHeadVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        chickenHeadVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        chickenHeadVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        chickenHeadVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        chickenHeadVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        chickenHeadVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        chickenHeadVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(chickenHeadVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- chickenLegVertices --- *  (leg, reused on both sides)

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // leg, pivot at the knee  (vanilla 3x5x3 @26,0 -> 3x5x3 px)
    x = pixel*-1.5; y = pixel*-5; z = pixel*-1.5;

    // -X  (ModelBox back 3x5)
    left = textureXpixel*35;
    right = left + textureXpixel*3;
    down = textureYpixel*3;
    up = down + textureYpixel*5;

    mPosition.push_back(new Vector3(x, y, z+pixel*3));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*5, z+pixel*3));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*5, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 3x5)
    left = textureXpixel*29;
    right = left + textureXpixel*3;
    down = textureYpixel*3;
    up = down + textureYpixel*5;

    mPosition.push_back(new Vector3(x+pixel*3, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*5, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*5, z+pixel*3));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*3, y, z+pixel*3));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 3x3)
    left = textureXpixel*32;
    right = left + textureXpixel*3;
    down = textureYpixel*0;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*3, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*3, y, z+pixel*3));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*3));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 3x3)
    left = textureXpixel*29;
    right = left + textureXpixel*3;
    down = textureYpixel*0;
    up = down + textureYpixel*3;

    mPosition.push_back(new Vector3(x, y+pixel*5, z+pixel*3));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*5, z+pixel*3));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*5, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*5, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 3x5)
    left = textureXpixel*26;
    right = left + textureXpixel*3;
    down = textureYpixel*3;
    up = down + textureYpixel*5;

    mPosition.push_back(new Vector3(x, y+pixel*5, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*5, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*3, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 3x5)
    left = textureXpixel*32;
    right = left + textureXpixel*3;
    down = textureYpixel*3;
    up = down + textureYpixel*5;

    mPosition.push_back(new Vector3(x, y, z+pixel*3));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*3, y, z+pixel*3));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*3, y+pixel*5, z+pixel*3));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*5, z+pixel*3));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    chickenLegVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        chickenLegVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        chickenLegVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        chickenLegVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        chickenLegVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        chickenLegVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        chickenLegVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        chickenLegVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        chickenLegVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        chickenLegVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        chickenLegVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        chickenLegVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        chickenLegVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        chickenLegVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        chickenLegVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        chickenLegVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(chickenLegVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- chickenWingVertices --- *  (wing, reused on both sides)

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // wing, pivot at the shoulder  (vanilla 1x4x6 @24,13 -> 6x4x1 px)
    x = pixel*-3; y = pixel*-4; z = pixel*-0.5;

    // -X  (ModelBox back 1x4)
    left = textureXpixel*37;
    right = left + textureXpixel*1;
    down = textureYpixel*19;
    up = down + textureYpixel*4;

    mPosition.push_back(new Vector3(x, y, z+pixel*1));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*4, z+pixel*1));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*4, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 1x4)
    left = textureXpixel*30;
    right = left + textureXpixel*1;
    down = textureYpixel*19;
    up = down + textureYpixel*4;

    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*4, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*4, z+pixel*1));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*1));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 1x6)
    left = textureXpixel*31;
    right = left + textureXpixel*1;
    down = textureYpixel*13;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*1));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*1));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 1x6)
    left = textureXpixel*30;
    right = left + textureXpixel*1;
    down = textureYpixel*13;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y+pixel*4, z+pixel*1));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*4, z+pixel*1));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*4, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*4, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 6x4)
    left = textureXpixel*24;
    right = left + textureXpixel*6;
    down = textureYpixel*19;
    up = down + textureYpixel*4;

    mPosition.push_back(new Vector3(x, y+pixel*4, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*4, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 6x4)
    left = textureXpixel*31;
    right = left + textureXpixel*6;
    down = textureYpixel*19;
    up = down + textureYpixel*4;

    mPosition.push_back(new Vector3(x, y, z+pixel*1));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*1));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*4, z+pixel*1));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*4, z+pixel*1));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    chickenWingVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        chickenWingVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        chickenWingVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        chickenWingVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        chickenWingVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        chickenWingVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        chickenWingVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        chickenWingVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        chickenWingVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        chickenWingVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        chickenWingVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        chickenWingVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        chickenWingVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        chickenWingVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        chickenWingVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        chickenWingVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(chickenWingVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();




    // ---- spider -----------------------------------------------------------
    // Generated by _tmp_atlas/spider_gen.py from vanilla ModelSpider (1.7.3 class
    // `bjb`); the box table and the pivots are read out of the jar, not transcribed.
    // Head, thorax and abdomen are three bones and the legs are one box used on eight of
    // them, four growing -Z and four +Z so neither side shows mirrored art.

    textureXpixel = 1/64.0f;
    textureYpixel = 1/32.0f;

    /// * --- spiderBodyVertices --- *  (abdomen)

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // abdomen  (vanilla 10x8x12 @0,12 -> 12x8x10 px, pivot-relative)
    x = pixel*-6; y = pixel*-4; z = pixel*-5;

    // -X  (ModelBox back 10x8)
    left = textureXpixel*34;
    right = left + textureXpixel*10;
    down = textureYpixel*24;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y, z+pixel*10));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 10x8)
    left = textureXpixel*12;
    right = left + textureXpixel*10;
    down = textureYpixel*24;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x+pixel*12, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*12, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*12, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*12, y, z+pixel*10));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 10x12)
    left = textureXpixel*22;
    right = left + textureXpixel*10;
    down = textureYpixel*12;
    up = down + textureYpixel*12;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*12, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*12, y, z+pixel*10));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 10x12)
    left = textureXpixel*12;
    right = left + textureXpixel*10;
    down = textureYpixel*12;
    up = down + textureYpixel*12;

    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*12, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*12, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 12x8)
    left = textureXpixel*0;
    right = left + textureXpixel*12;
    down = textureYpixel*24;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*12, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*12, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 12x8)
    left = textureXpixel*22;
    right = left + textureXpixel*12;
    down = textureYpixel*24;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y, z+pixel*10));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*12, y, z+pixel*10));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*12, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*10));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    spiderBodyVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        spiderBodyVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        spiderBodyVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        spiderBodyVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        spiderBodyVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        spiderBodyVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        spiderBodyVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        spiderBodyVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        spiderBodyVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        spiderBodyVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        spiderBodyVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        spiderBodyVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        spiderBodyVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        spiderBodyVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        spiderBodyVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        spiderBodyVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(spiderBodyVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- spiderNeckVertices --- *  (thorax)

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // thorax  (vanilla 6x6x6 @0,0 -> 6x6x6 px, pivot-relative)
    x = pixel*-3; y = pixel*-3; z = pixel*-3;

    // -X  (ModelBox back 6x6)
    left = textureXpixel*18;
    right = left + textureXpixel*6;
    down = textureYpixel*6;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 6x6)
    left = textureXpixel*6;
    right = left + textureXpixel*6;
    down = textureYpixel*6;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 6x6)
    left = textureXpixel*12;
    right = left + textureXpixel*6;
    down = textureYpixel*0;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 6x6)
    left = textureXpixel*6;
    right = left + textureXpixel*6;
    down = textureYpixel*0;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 6x6)
    left = textureXpixel*0;
    right = left + textureXpixel*6;
    down = textureYpixel*6;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y+pixel*6, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*6, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 6x6)
    left = textureXpixel*12;
    right = left + textureXpixel*6;
    down = textureYpixel*6;
    up = down + textureYpixel*6;

    mPosition.push_back(new Vector3(x, y, z+pixel*6));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*6, y, z+pixel*6));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*6, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*6, z+pixel*6));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    spiderNeckVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        spiderNeckVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        spiderNeckVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        spiderNeckVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        spiderNeckVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        spiderNeckVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        spiderNeckVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        spiderNeckVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        spiderNeckVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        spiderNeckVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        spiderNeckVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        spiderNeckVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        spiderNeckVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        spiderNeckVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        spiderNeckVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        spiderNeckVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(spiderNeckVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- spiderHeadVertices --- *  (head)

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // head  (vanilla 8x8x8 @32,4 -> 8x8x8 px, pivot-relative)
    x = pixel*0; y = pixel*-4; z = pixel*-4;

    // -X  (ModelBox back 8x8)
    left = textureXpixel*56;
    right = left + textureXpixel*8;
    down = textureYpixel*12;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 8x8)
    left = textureXpixel*40;
    right = left + textureXpixel*8;
    down = textureYpixel*12;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x+pixel*8, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z+pixel*8));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 8x8)
    left = textureXpixel*48;
    right = left + textureXpixel*8;
    down = textureYpixel*4;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 8x8)
    left = textureXpixel*40;
    right = left + textureXpixel*8;
    down = textureYpixel*4;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 8x8). These two side tiles were swapped: 32 is the
    // left of the unwrap and 48 is the right, so the head's cheeks were mirrored.
    left = textureXpixel*48;
    right = left + textureXpixel*8;
    down = textureYpixel*12;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y+pixel*8, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*8, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 8x8)
    left = textureXpixel*32;
    right = left + textureXpixel*8;
    down = textureYpixel*12;
    up = down + textureYpixel*8;

    mPosition.push_back(new Vector3(x, y, z+pixel*8));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*8, y, z+pixel*8));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*8, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*8, z+pixel*8));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    spiderHeadVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        spiderHeadVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        spiderHeadVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        spiderHeadVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        spiderHeadVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        spiderHeadVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        spiderHeadVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        spiderHeadVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        spiderHeadVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        spiderHeadVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        spiderHeadVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        spiderHeadVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        spiderHeadVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        spiderHeadVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        spiderHeadVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        spiderHeadVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(spiderHeadVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- spiderLegVertices --- *  (leg, grows toward -Z (four of them))

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // leg, grows toward -Z  (vanilla 16x2x2 @18,0 -> 2x2x16 px, pivot-relative)
    x = pixel*-1; y = pixel*-1; z = pixel*-15;

    // -X  (ModelBox back 16x2)
    left = textureXpixel*38;
    right = left + textureXpixel*16;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z+pixel*16));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 16x2)
    left = textureXpixel*20;
    right = left + textureXpixel*16;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*16));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 16x2)
    left = textureXpixel*36;
    right = left + textureXpixel*16;
    down = textureYpixel*0;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*16));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*16));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 16x2)
    left = textureXpixel*20;
    right = left + textureXpixel*16;
    down = textureYpixel*0;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 2x2)
    left = textureXpixel*18;
    right = left + textureXpixel*2;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 2x2)
    left = textureXpixel*36;
    right = left + textureXpixel*2;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z+pixel*16));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*16));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    spiderLegVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        spiderLegVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        spiderLegVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        spiderLegVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        spiderLegVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        spiderLegVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        spiderLegVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        spiderLegVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        spiderLegVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        spiderLegVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        spiderLegVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        spiderLegVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        spiderLegVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        spiderLegVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        spiderLegVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        spiderLegVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(spiderLegVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- spiderLegLeftVertices --- *  (leg, grows toward +Z (four of them))

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // leg, grows toward +Z  (vanilla 16x2x2 @18,0 -> 2x2x16 px, pivot-relative)
    x = pixel*-1; y = pixel*-1; z = pixel*-1;

    // -X  (ModelBox back 16x2)
    left = textureXpixel*38;
    right = left + textureXpixel*16;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z+pixel*16));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 16x2)
    left = textureXpixel*20;
    right = left + textureXpixel*16;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*16));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 16x2)
    left = textureXpixel*36;
    right = left + textureXpixel*16;
    down = textureYpixel*0;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*16));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y, z+pixel*16));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 16x2)
    left = textureXpixel*20;
    right = left + textureXpixel*16;
    down = textureYpixel*0;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 2x2)
    left = textureXpixel*18;
    right = left + textureXpixel*2;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y+pixel*2, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 2x2)
    left = textureXpixel*36;
    right = left + textureXpixel*2;
    down = textureYpixel*2;
    up = down + textureYpixel*2;

    mPosition.push_back(new Vector3(x, y, z+pixel*16));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*16));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*2, z+pixel*16));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    spiderLegLeftVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        spiderLegLeftVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        spiderLegLeftVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        spiderLegLeftVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        spiderLegLeftVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        spiderLegLeftVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        spiderLegLeftVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        spiderLegLeftVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        spiderLegLeftVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        spiderLegLeftVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        spiderLegLeftVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        spiderLegLeftVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        spiderLegLeftVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        spiderLegLeftVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        spiderLegLeftVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        spiderLegLeftVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(spiderLegLeftVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();




    // ---- skeleton -----------------------------------------------------------
    // Generated by _tmp_atlas/skeleton_gen.py from the zombie's limb sections -- ModelSkeleton
    // only shrinks the limbs (2x12x2) and moves their texture windows; head and body are
    // inherited, so the skeleton draws world->headVertices / world->bodyVertices.

    /// * --- skeletonHandVertices --- *  (skeleton limb)

    // Derived from the HAND section by _tmp_atlas/skeleton_gen.py: same faces,
    // same winding, box shrunk to 2x12x2 and each window re-read from the 2x12x2 net.
    x = pixel * -1;
    y = pixel * -2;
    z = pixel * -1;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // -X  (ModelBox back 2x12 @46,18)
    left = 0.71875f;
    right = 0.75f;
    down = 0.5625f;
    up = 0.9375f;

    mPosition.push_back(new Vector3(x, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 2x12 @42,18)
    left = 0.65625f;
    right = 0.6875f;
    down = 0.5625f;
    up = 0.9375f;

    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*2));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 2x2 @44,16)
    left = 0.6875f;
    right = 0.71875f;
    down = 0.5f;
    up = 0.5625f;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y, z+pixel*2));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 2x2 @42,16)
    left = 0.65625f;
    right = 0.6875f;
    down = 0.5f;
    up = 0.5625f;

    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 2x12 @40,18)
    left = 0.625f;
    right = 0.65625f;
    down = 0.5625f;
    up = 0.9375f;

    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 2x12 @44,18)
    left = 0.6875f;
    right = 0.71875f;
    down = 0.5625f;
    up = 0.9375f;

    mPosition.push_back(new Vector3(x, y, z+pixel*2));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    skeletonHandVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        skeletonHandVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        skeletonHandVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        skeletonHandVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        skeletonHandVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        skeletonHandVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        skeletonHandVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        skeletonHandVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        skeletonHandVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        skeletonHandVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        skeletonHandVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        skeletonHandVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        skeletonHandVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        skeletonHandVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        skeletonHandVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        skeletonHandVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(skeletonHandVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();

    /// * --- skeletonLegVertices --- *  (skeleton limb)

    // Derived from the LEG section by _tmp_atlas/skeleton_gen.py: same faces,
    // same winding, box shrunk to 2x12x2 and each window re-read from the 2x12x2 net.
    x = pixel * -1;
    y = pixel * -2;
    z = pixel * -1;

    iVertex = 0;
    points = 0;
    vert = 0;
    size = 0;

    // -X  (ModelBox back 2x12 @6,18)
    left = 0.09375f;
    right = 0.125f;
    down = 0.5625f;
    up = 0.9375f;

    mPosition.push_back(new Vector3(x, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +X  (ModelBox front 2x12 @2,18)
    left = 0.03125f;
    right = 0.0625f;
    down = 0.5625f;
    up = 0.9375f;

    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*2));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Y  (ModelBox bottom 2x2 @4,16)
    left = 0.0625f;
    right = 0.09375f;
    down = 0.5f;
    up = 0.5625f;

    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y, z+pixel*2));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Y  (ModelBox top 2x2 @2,16)
    left = 0.03125f;
    right = 0.0625f;
    down = 0.5f;
    up = 0.5625f;

    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // -Z  (ModelBox right 2x12 @0,18)
    left = 0.0f;
    right = 0.03125f;
    down = 0.5625f;
    up = 0.9375f;

    mPosition.push_back(new Vector3(x, y+pixel*12, z));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z));
    mtextures.push_back(new Vector2(left, up));
    mPosition.push_back(new Vector3(x+pixel*2, y, z));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x, y, z));
    mtextures.push_back(new Vector2(right, down));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    // +Z  (ModelBox left 2x12 @4,18)
    left = 0.0625f;
    right = 0.09375f;
    down = 0.5625f;
    up = 0.9375f;

    mPosition.push_back(new Vector3(x, y, z+pixel*2));
    mtextures.push_back(new Vector2(left, down));
    mPosition.push_back(new Vector3(x+pixel*2, y, z+pixel*2));
    mtextures.push_back(new Vector2(right, down));
    mPosition.push_back(new Vector3(x+pixel*2, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(right, up));
    mPosition.push_back(new Vector3(x, y+pixel*12, z+pixel*2));
    mtextures.push_back(new Vector2(left, up));

    mTriangle.push_back(new Vector3(iVertex, iVertex+1, iVertex+2));
    mTriangle.push_back(new Vector3(iVertex+2, iVertex+3, iVertex));
    iVertex += 4;
    points += 6;

    skeletonLegVertices = (TexturesPSPVertex*)memalign(16,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    size = mTriangle.size();
    for(unsigned int j = 0; j < size; j++)
    {
        skeletonLegVertices[vert].u = mtextures[mTriangle[j]->x]->x;
        skeletonLegVertices[vert].v = mtextures[mTriangle[j]->x]->y;
        skeletonLegVertices[vert].x = mPosition[mTriangle[j]->x]->x;
        skeletonLegVertices[vert].y = mPosition[mTriangle[j]->x]->y;
        skeletonLegVertices[vert].z = mPosition[mTriangle[j]->x]->z;
        vert++;
        skeletonLegVertices[vert].u = mtextures[mTriangle[j]->y]->x;
        skeletonLegVertices[vert].v = mtextures[mTriangle[j]->y]->y;
        skeletonLegVertices[vert].x = mPosition[mTriangle[j]->y]->x;
        skeletonLegVertices[vert].y = mPosition[mTriangle[j]->y]->y;
        skeletonLegVertices[vert].z = mPosition[mTriangle[j]->y]->z;
        vert++;
        skeletonLegVertices[vert].u = mtextures[mTriangle[j]->z]->x;
        skeletonLegVertices[vert].v = mtextures[mTriangle[j]->z]->y;
        skeletonLegVertices[vert].x = mPosition[mTriangle[j]->z]->x;
        skeletonLegVertices[vert].y = mPosition[mTriangle[j]->z]->y;
        skeletonLegVertices[vert].z = mPosition[mTriangle[j]->z]->z;
        vert++;
    }
    sceKernelDcacheWritebackInvalidateRange(skeletonLegVertices,( mTriangle.size() * 3) * sizeof(TexturesPSPVertex));
    for(unsigned int aa = 0; aa < mPosition.size(); aa++)
    {
        delete mPosition[aa];
    }
    for(unsigned int aa = 0; aa < mtextures.size(); aa++)
    {
        delete mtextures[aa];
    }
    mPosition.clear();
    mtextures.clear();
    for(unsigned int aa = 0; aa < mTriangle.size(); aa++)
    {
        delete mTriangle[aa];
    }
    mTriangle.clear();


}
