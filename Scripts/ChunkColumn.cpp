#include "ChunkColumn.h"
#include <string.h>

ChunkColumn::ChunkColumn(int createCx, int createCz, int createChunkSize, int createHeight)
{
    cx = createCx;
    cz = createCz;
    chunkSize = createChunkSize;
    height = createHeight;
    generated = false;
    surfaceReady = false;
    surfaceY = 0;
    surfaceMinY = 0;
    surfaceKnown = false;
    hashNext = 0;
    blocks = new block_t[Volume()];
    settings = new block_settings_t[Volume()];
    memset(blocks, 0, Volume() * sizeof(block_t));
    memset(settings, 0, Volume() * sizeof(block_settings_t));
    for(int i = 0; i < 16; i++)
        meshId[i] = -1;
}

ChunkColumn::~ChunkColumn()
{
    delete[] blocks;
    delete[] settings;
    blocks = 0;
    settings = 0;
}

int ChunkColumn::Volume() const
{
    return chunkSize * height * chunkSize;
}

int ChunkColumn::Index(int lx, int y, int lz) const
{
    return lx + y * chunkSize + lz * chunkSize * height;
}
