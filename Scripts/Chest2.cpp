#include "Chest2.h"

Chest::Chest(int x, int y,  int z)
{
    for(int i = 0; i < 27; i++)
    {
        chestSlotId[i] = -1;
        chestSlotAm[i] = -1;
        chestSlotSt[i] = 0;
    }

    chestX = x;
    chestY = y;
    chestZ = z;
    chestAddress = 0;

    id = 0;
}

Chest::~Chest()
{

}