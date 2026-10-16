#ifndef CHUNKCOLUMN_H
#define CHUNKCOLUMN_H

#include "BaseBlock.h"

inline int ChunkFloorDiv(int a, int b)
{
    if(b <= 0)
        return 0;
    int q = a / b;
    int r = a % b;
    if(r != 0 && a < 0)
        --q;
    return q;
}

class ChunkColumn
{
public:
    int cx;
    int cz;
    int height;
    int chunkSize;
    bool generated;
    // Terrain (and the water on it) exists. Caves and trees may still be running. The mesher may build
    // the surface sections from here; it leaves the deeper ones until `generated`.
    bool surfaceReady;
    // The ground's band: surfaceY is the y of the column's highest ground cell, surfaceMinY of its
    // lowest. Set by the generator when the terrain rows land, or scanned from the blocks for a
    // column read from a file (CraftWorld::EnsureSurfaceBand). surfaceKnown says which has happened.
    int surfaceY;
    int surfaceMinY;
    bool surfaceKnown;
    ChunkColumn* hashNext;
    int meshId[16];
    block_t* blocks;
    block_settings_t* settings;

    ChunkColumn(int createCx, int createCz, int createChunkSize, int createHeight);
    ~ChunkColumn();

    int Index(int lx, int y, int lz) const;
    int Volume() const;
};

#endif
