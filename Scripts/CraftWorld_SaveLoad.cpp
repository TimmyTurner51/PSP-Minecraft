#include "CraftWorld2.h"
#include "WorldGenerator.h"
#include "DebugConfig.h"
#include "TextureHelper.h"
#include <Aurora/Graphics/Effects/Arrow2.h>
#include <stdio.h>
#include <zlib.h>
void CraftWorld::SaveCompressedWorld(std::string  filename)
{
    MC_DebugHeapCheckpoint("save begin");
    //first save version and name on normal file
    FILE * pFile;
    pFile = fopen(filename.c_str(),"wb");

    if(pFile == NULL)
        return;

    // A pressed button is transient; its press timer lives only in memory. Drop them all back to rest
    // so a save can never contain a button that would stay stuck pressed after loading.
    ReleaseAllButtons();
    // A repeater's pending delay tick is transient in the same way. Its *output* is a block id and so
    // is saved, which is what matters: a repeater caught mid-delay simply resumes from the state it is
    // in, and the first later event around it re-times it.
    ClearRepeaters();
    // A piston's stroke is transient for the same reason and one more: it names cells, and it is over in
    // a tenth of a second. Saved it would be meaningless in another world; dropped, the piston is simply
    // drawn at rest, which is the shape its block ids are already in.
    ClearPistonStrokes();

    //version
    fwrite(&worldVersion,sizeof(int),1,pFile);

    //game mode
    fwrite(&gameModeWorld,sizeof(char),1,pFile);

    //game mode
    fwrite(&gameLocked,sizeof(bool),1,pFile);

    //name
    fwrite(worldName,sizeof(char),50,pFile);

    //close file
    fclose(pFile);

    //now save compressed map info
    filename = filename + "c";

    //open file
    gzFile saveFile;
    saveFile = gzopen(filename.c_str(),"wb");
    if(saveFile == 0)
        return;

    //player pos
    gzwrite(saveFile, &playerPos.x,sizeof(float));
    gzwrite(saveFile, &playerPos.y,sizeof(float));
    gzwrite(saveFile, &playerPos.z,sizeof(float));

    //player spawn pos
    gzwrite(saveFile, &playerSpawnPointPosition.x,sizeof(float));
    gzwrite(saveFile, &playerSpawnPointPosition.y,sizeof(float));
    gzwrite(saveFile, &playerSpawnPointPosition.z,sizeof(float));

    //snow biome pos
    gzwrite(saveFile, &snowBiomePosition.x,sizeof(float));
    gzwrite(saveFile, &snowBiomePosition.z,sizeof(float));
    gzwrite(saveFile, &snowBiomeRadius,sizeof(float));

    //desert biome pos
    gzwrite(saveFile, &desertBiomePosition.x,sizeof(float));
    gzwrite(saveFile, &desertBiomePosition.z,sizeof(float));
    gzwrite(saveFile, &desertBiomeRadius,sizeof(float));

    //size
    gzwrite(saveFile, &WORLD_SIZE,sizeof(int));

    //size
    gzwrite(saveFile, &WORLD_HEIGHT,sizeof(int));

    //chunksize
    gzwrite(saveFile, &CHUNK_SIZE,sizeof(int));

    worldSavePath = filename.substr(0, filename.size() - 1);
    if(worldSavePath.empty())
        worldSavePath = filename;
    SaveLoadedColumns();

    //world time
    gzwrite(saveFile, &worldDayTime,sizeof(float));
    //sun time
    //gzwrite(saveFile, &sunTime,sizeof(float));

    gzwrite(saveFile, &mainStatistics,sizeof(st));

    gzwrite(saveFile, &mainOptions,sizeof(opt));

    gzwrite(saveFile, &invId,sizeof(short)*(36));

    gzwrite(saveFile, &invAm,sizeof(short)*(36));

    gzwrite(saveFile, &invSt,sizeof(bool)*(36));

    gzwrite(saveFile, &armorId,sizeof(short)*(4));

    gzwrite(saveFile, &armorAm,sizeof(short)*(4));

    gzwrite(saveFile, &armorSt,sizeof(bool)*(4));

    gzwrite(saveFile, &HP,sizeof(unsigned int));

    gzwrite(saveFile, &HG,sizeof(unsigned int));

    gzwrite(saveFile, &OS,sizeof(unsigned int));

    gzwrite(saveFile, &brightFactor,sizeof(float));

    gzwrite(saveFile, &starsFactor,sizeof(float));

    gzwrite(saveFile, &worldSeed,sizeof(int));

    gzwrite(saveFile, &skyTime,sizeof(float));

    int savedWeatherType = worldWeather->GetWeatherType();
    gzwrite(saveFile, &savedWeatherType,sizeof(int));

    float savedWeatherDuration = worldWeather->GetWeatherDuration();
    gzwrite(saveFile, &savedWeatherDuration,sizeof(float));

    mChestsize = mChests.size();
    gzwrite(saveFile, &mChestsize,sizeof(int));

    if(mChestsize != 0)
    {
        int* data1 = new int[27*mChestsize];
        int* data2 = new int[27*mChestsize];
        bool* data3 = new bool[27*mChestsize];

        int* data4 = new int[mChestsize];
        int* data5 = new int[mChestsize];
        int* data6 = new int[mChestsize];

        for(int k = 0; k < mChestsize; k++)
        {
            Chest* SaveChest = mChests[k];
            for(int l = 0; l <= 26; l++)
            {
                data1[k*27+l] = SaveChest->chestSlotId[l];
                data2[k*27+l] = SaveChest->chestSlotAm[l];
                data3[k*27+l] = SaveChest->chestSlotSt[l];
            }

            data4[k] = SaveChest->chestX;
            data5[k] = SaveChest->chestY;
            data6[k] = SaveChest->chestZ;
        }

        gzwrite(saveFile, data1,sizeof(int) * (mChestsize*27));
        gzwrite(saveFile, data2,sizeof(int) * (mChestsize*27));
        gzwrite(saveFile, data3,sizeof(bool) * (mChestsize*27));

        gzwrite(saveFile, data4,sizeof(int)*(mChestsize));
        gzwrite(saveFile, data5,sizeof(int)*(mChestsize));
        gzwrite(saveFile, data6,sizeof(int)*(mChestsize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
    }

    mFurnacesize = mFurnaces.size();
    gzwrite(saveFile, &mFurnacesize,sizeof(int));

    if(mFurnacesize != 0)
    {
        int* data1 = new int[3*mFurnacesize];
        int* data2 = new int[3*mFurnacesize];
        bool* data3 = new bool[3*mFurnacesize];

        int* data4 = new int[mFurnacesize];
        int* data5 = new int[mFurnacesize];
        int* data6 = new int[mFurnacesize];

        float* data7 = new float[mFurnacesize];
        float* data8 = new float[mFurnacesize];
        int* data10 = new int[mFurnacesize];

        char* data11 = new char[mFurnacesize];

        for(int k = 0; k < mFurnacesize; k++)
        {
            Furnace* SaveFurnace = mFurnaces[k];
            for(int l = 0; l <= 2; l++)
            {
                data1[k*3+l] = SaveFurnace->furnaceSlotId[l];
                data2[k*3+l] = SaveFurnace->furnaceSlotAm[l];
                data3[k*3+l] = SaveFurnace->furnaceSlotSt[l];
            }
            data4[k] = SaveFurnace->furnaceX;
            data5[k] = SaveFurnace->furnaceY;
            data6[k] = SaveFurnace->furnaceZ;

            data7[k] = SaveFurnace->fuelTime;
            data8[k] = SaveFurnace->meltingTime;
            data10[k] = SaveFurnace->smeltItemTemp;
            data11[k] = SaveFurnace->GetSide();
        }

        gzwrite(saveFile, data1,sizeof(int) * (mFurnacesize*3));
        gzwrite(saveFile, data2,sizeof(int) * (mFurnacesize*3));
        gzwrite(saveFile, data3,sizeof(bool) *(mFurnacesize*3));

        gzwrite(saveFile, data4,sizeof(int)*(mFurnacesize));
        gzwrite(saveFile, data5,sizeof(int)*(mFurnacesize));
        gzwrite(saveFile, data6,sizeof(int)*(mFurnacesize));

        gzwrite(saveFile, data7,sizeof(float)*(mFurnacesize));
        gzwrite(saveFile, data8,sizeof(float)*(mFurnacesize));
        gzwrite(saveFile, data10,sizeof(int)*(mFurnacesize));
        gzwrite(saveFile, data11,sizeof(char)*(mFurnacesize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
        delete[] data7;
        delete[] data8;
        delete[] data10;
        delete[] data11;
    }

    mZombiesize = mZombies.size();
    gzwrite(saveFile, &mZombiesize,sizeof(int));

    if(mZombiesize != 0)
    {
        float* data1 = new float[mZombiesize]; // x
        float* data2 = new float[mZombiesize]; // y
        float* data3 = new float[mZombiesize]; // z

        float* data4 = new float[mZombiesize]; // mainAngle
        float* data5 = new float[mZombiesize]; // health
        int* data6 = new int[mZombiesize]; // id

        int* data7 = new int[mZombiesize]; // helmet id
        int* data8 = new int[mZombiesize]; // chestplate id
        int* data9 = new int[mZombiesize]; // leggings id
        int* data10 = new int[mZombiesize]; // boots id

        for(int k = 0; k < mZombiesize; k++)
        {
            Zombie* SaveZombie = mZombies[k];

            data1[k] = SaveZombie->position.x;
            data2[k] = SaveZombie->position.y;
            data3[k] = SaveZombie->position.z;

            data4[k] = SaveZombie->mainAngle;
            data5[k] = SaveZombie->HP;
            data6[k] = SaveZombie->number;

            data7[k] = SaveZombie->helmetId;
            data8[k] = SaveZombie->chestplateId;
            data9[k] = SaveZombie->leggingsId;
            data10[k] = SaveZombie->bootsId;
        }

        gzwrite(saveFile, data1,sizeof(float) * (mZombiesize));
        gzwrite(saveFile, data2,sizeof(float) * (mZombiesize));
        gzwrite(saveFile, data3,sizeof(float) *(mZombiesize));

        gzwrite(saveFile, data4,sizeof(float)*(mZombiesize));
        gzwrite(saveFile, data5,sizeof(float)*(mZombiesize));
        gzwrite(saveFile, data6,sizeof(int)*(mZombiesize));

        gzwrite(saveFile, data7,sizeof(int)*(mZombiesize));
        gzwrite(saveFile, data8,sizeof(int)*(mZombiesize));
        gzwrite(saveFile, data9,sizeof(int)*(mZombiesize));
        gzwrite(saveFile, data10,sizeof(int)*(mZombiesize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
        delete[] data7;
        delete[] data8;
        delete[] data9;
        delete[] data10;
    }


    mCowsize = mCows.size();
    gzwrite(saveFile, &mCowsize,sizeof(int));

    if(mCowsize != 0)
    {
        float* data1 = new float[mCowsize]; // x
        float* data2 = new float[mCowsize]; // y
        float* data3 = new float[mCowsize]; // z

        float* data4 = new float[mCowsize]; // mainAngle
        float* data5 = new float[mCowsize]; // health
        int* data6 = new int[mCowsize]; // id

        for(int k = 0; k < mCowsize; k++)
        {
            Cow* SaveCow = mCows[k];

            data1[k] = SaveCow->position.x;
            data2[k] = SaveCow->position.y;
            data3[k] = SaveCow->position.z;

            data4[k] = SaveCow->mainAngle;
            data5[k] = SaveCow->HP;
            data6[k] = SaveCow->number;
        }

        gzwrite(saveFile, data1,sizeof(float) * (mCowsize));
        gzwrite(saveFile, data2,sizeof(float) * (mCowsize));
        gzwrite(saveFile, data3,sizeof(float) *(mCowsize));

        gzwrite(saveFile, data4,sizeof(float)*(mCowsize));
        gzwrite(saveFile, data5,sizeof(float)*(mCowsize));
        gzwrite(saveFile, data6,sizeof(int)*(mCowsize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
    }

    mSheepsize = mSheeps.size();
    gzwrite(saveFile, &mSheepsize,sizeof(int));

    if(mSheepsize != 0)
    {
        float* data1 = new float[mSheepsize]; // x
        float* data2 = new float[mSheepsize]; // y
        float* data3 = new float[mSheepsize]; // z

        float* data4 = new float[mSheepsize]; // mainAngle
        float* data5 = new float[mSheepsize]; // health
        int* data6 = new int[mSheepsize]; // id

        int* data7 = new int[mSheepsize]; // color
        bool* data8 = new bool[mSheepsize]; // sheared

        for(int k = 0; k < mSheepsize; k++)
        {
            Sheep* SaveSheep = mSheeps[k];

            data1[k] = SaveSheep->position.x;
            data2[k] = SaveSheep->position.y;
            data3[k] = SaveSheep->position.z;

            data4[k] = SaveSheep->mainAngle;
            data5[k] = SaveSheep->HP;
            data6[k] = SaveSheep->number;

            data7[k] = SaveSheep->GetFurColor();
            data8[k] = SaveSheep->sheared;
        }

        gzwrite(saveFile, data1,sizeof(float) * (mSheepsize));
        gzwrite(saveFile, data2,sizeof(float) * (mSheepsize));
        gzwrite(saveFile, data3,sizeof(float) *(mSheepsize));

        gzwrite(saveFile, data4,sizeof(float)*(mSheepsize));
        gzwrite(saveFile, data5,sizeof(float)*(mSheepsize));
        gzwrite(saveFile, data6,sizeof(int)*(mSheepsize));

        gzwrite(saveFile, data7,sizeof(int)*(mSheepsize));
        gzwrite(saveFile, data8,sizeof(bool)*(mSheepsize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
        delete[] data7;
        delete[] data8;
    }


    mCreepersize = mCreepers.size();
    gzwrite(saveFile, &mCreepersize,sizeof(int));

    if(mCreepersize != 0)
    {
        float* data1 = new float[mCreepersize]; // x
        float* data2 = new float[mCreepersize]; // y
        float* data3 = new float[mCreepersize]; // z

        float* data4 = new float[mCreepersize]; // mainAngle
        float* data5 = new float[mCreepersize]; // health
        int* data6 = new int[mCreepersize]; // id

        float* data7 = new float[mCreepersize]; // freezedTimerMax

        for(int k = 0; k < mCreepersize; k++)
        {
            Creeper* SaveCreeper = mCreepers[k];

            data1[k] = SaveCreeper->position.x;
            data2[k] = SaveCreeper->position.y;
            data3[k] = SaveCreeper->position.z;

            data4[k] = SaveCreeper->mainAngle;
            data5[k] = SaveCreeper->HP;
            data6[k] = SaveCreeper->number;

            data7[k] = SaveCreeper->freezedTimerMax;
        }

        gzwrite(saveFile, data1,sizeof(float) * (mCreepersize));
        gzwrite(saveFile, data2,sizeof(float) * (mCreepersize));
        gzwrite(saveFile, data3,sizeof(float) *(mCreepersize));

        gzwrite(saveFile, data4,sizeof(float)*(mCreepersize));
        gzwrite(saveFile, data5,sizeof(float)*(mCreepersize));
        gzwrite(saveFile, data6,sizeof(int)*(mCreepersize));

        gzwrite(saveFile, data7,sizeof(float)*(mCreepersize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
        delete[] data7;
    }

	/*mPigsize = mPigs.size();
    gzwrite(saveFile, &mPigsize,sizeof(int));

    if(mPigsize != 0)
    {
        float* data1 = new float[mPigsize]; // x
        float* data2 = new float[mPigsize]; // y
        float* data3 = new float[mPigsize]; // z

        float* data4 = new float[mPigsize]; // mainAngle
        float* data5 = new float[mPigsize]; // health
        int* data6 = new int[mPigsize]; // id

        float* data7 = new float[mPigsize]; // freezedTimerMax

        for(int k = 0; k < mPigsize; k++)
        {
            Pig* SavePig = mPigs[k];

            data1[k] = SavePig->position.x;
            data2[k] = SavePig->position.y;
            data3[k] = SavePig->position.z;

            data4[k] = SavePig->mainAngle;
            data5[k] = SavePig->HP;
            data6[k] = SavePig->number;

            data7[k] = SavePig->freezedTimerMax;
        }

        gzwrite(saveFile, data1,sizeof(float) * (mPigsize));
        gzwrite(saveFile, data2,sizeof(float) * (mPigsize));
        gzwrite(saveFile, data3,sizeof(float) *(mPigsize));

        gzwrite(saveFile, data4,sizeof(float)*(mPigsize));
        gzwrite(saveFile, data5,sizeof(float)*(mPigsize));
        gzwrite(saveFile, data6,sizeof(int)*(mPigsize));

        gzwrite(saveFile, data7,sizeof(float)*(mPigsize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
        delete[] data7;
    }

    gzwrite(saveFile, &zombieNum,sizeof(int));
	*/

    // TNT data saving
    mTNTsize = mTNTs.size();
    gzwrite(saveFile, &mTNTsize,sizeof(int));

    if(mTNTsize != 0)
    {
        float* data1 = new float[mTNTsize]; // x
        float* data2 = new float[mTNTsize]; // y
        float* data3 = new float[mTNTsize]; // z

        float* data4 = new float[mTNTsize]; // timeToExplode

        for(int k = 0; k < mTNTsize; k++)
        {
            TNT* SaveTNT = mTNTs[k];

            data1[k] = SaveTNT->position.x;
            data2[k] = SaveTNT->position.y;
            data3[k] = SaveTNT->position.z;

            data4[k] = SaveTNT->timeToExplode;
        }

        gzwrite(saveFile, data1,sizeof(float) * (mTNTsize));
        gzwrite(saveFile, data2,sizeof(float) * (mTNTsize));
        gzwrite(saveFile, data3,sizeof(float) *(mTNTsize));

        gzwrite(saveFile, data4,sizeof(float)*(mTNTsize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
    }

    mJukeboxsize = mJukeboxes.size();
    gzwrite(saveFile, &mJukeboxsize,sizeof(int));

    if(mJukeboxsize != 0)
    {
        int* data1 = new int[mJukeboxsize]; // x
        int* data2 = new int[mJukeboxsize]; // y
        int* data3 = new int[mJukeboxsize]; // z
        int* data4 = new int[mJukeboxsize]; // record

        for(int k = 0; k < mJukeboxsize; k++)
        {
            JukeboxEntity* SaveJukebox = mJukeboxes[k];

            data1[k] = SaveJukebox->GetX();
            data2[k] = SaveJukebox->GetY();
            data3[k] = SaveJukebox->GetZ();
            data4[k] = SaveJukebox->GetRecord();
        }

        gzwrite(saveFile, data1,sizeof(int) * (mJukeboxsize));
        gzwrite(saveFile, data2,sizeof(int) * (mJukeboxsize));
        gzwrite(saveFile, data3,sizeof(int) * (mJukeboxsize));

        gzwrite(saveFile, data4,sizeof(int) * (mJukeboxsize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
    }

    mMonsterSpawnersize = mMonsterSpawners.size();
    gzwrite(saveFile, &mMonsterSpawnersize,sizeof(int));

    if(mMonsterSpawnersize != 0)
    {
        int* data1 = new int[mMonsterSpawnersize]; // x
        int* data2 = new int[mMonsterSpawnersize]; // y
        int* data3 = new int[mMonsterSpawnersize]; // z
        int* data4 = new int[mMonsterSpawnersize]; // mob to spawn

        for(int k = 0; k < mMonsterSpawnersize; k++)
        {
            MonsterSpawnerEntity* SaveMS = mMonsterSpawners[k];

            data1[k] = SaveMS->GetX();
            data2[k] = SaveMS->GetY();
            data3[k] = SaveMS->GetZ();

            if(SaveMS->MobToSpawn() == "zombie")
            {
                data4[k] = 1;
            }
            else
            {
                data4[k] = 0;
            }
        }

        gzwrite(saveFile, data1,sizeof(int) * (mMonsterSpawnersize));
        gzwrite(saveFile, data2,sizeof(int) * (mMonsterSpawnersize));
        gzwrite(saveFile, data3,sizeof(int) * (mMonsterSpawnersize));

        gzwrite(saveFile, data4,sizeof(int)*(mMonsterSpawnersize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
    }


    mItemFramesize = mItemFrames.size();
    gzwrite(saveFile, &mItemFramesize,sizeof(int));

    if(mItemFramesize != 0)
    {
        int* data1 = new int[mItemFramesize]; // x
        int* data2 = new int[mItemFramesize]; // y
        int* data3 = new int[mItemFramesize]; // z
        int* data4 = new int[mItemFramesize]; // facing
        int* data5 = new int[mItemFramesize]; // get item id
        int* data6 = new int[mItemFramesize]; // get item amount

        for(int k = 0; k < mItemFramesize; k++)
        {
            ItemFrameEntity* SaveIF = mItemFrames[k];

            data1[k] = SaveIF->GetX();
            data2[k] = SaveIF->GetY();
            data3[k] = SaveIF->GetZ();
            data4[k] = SaveIF->GetFacing();
            data5[k] = SaveIF->GetItem();
            data6[k] = SaveIF->GetAmount();
        }

        gzwrite(saveFile, data1,sizeof(int) * (mItemFramesize));
        gzwrite(saveFile, data2,sizeof(int) * (mItemFramesize));
        gzwrite(saveFile, data3,sizeof(int) * (mItemFramesize));
        gzwrite(saveFile, data4,sizeof(int) * (mItemFramesize));
        gzwrite(saveFile, data5,sizeof(int) * (mItemFramesize));
        gzwrite(saveFile, data6,sizeof(int) * (mItemFramesize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
    }
    gzwrite(saveFile, &worldType,sizeof(int));
    char savedTrees = makeTrees ? 1 : 0;
    char savedWater = makeWater ? 1 : 0;
    char savedCaves = makeCaves ? 1 : 0;
    gzwrite(saveFile, &savedTrees,sizeof(char));
    gzwrite(saveFile, &savedWater,sizeof(char));
    gzwrite(saveFile, &savedCaves,sizeof(char));
    gzwrite(saveFile, &waterLevel,sizeof(int));
    gzwrite(saveFile, &worldStamp,sizeof(int));

    // Optional trailing pig blob (old saves omit this; load is tolerant).
    mPigsize = mPigs.size();
    gzwrite(saveFile, &mPigsize,sizeof(int));
    if(mPigsize != 0)
    {
        float* data1 = new float[mPigsize];
        float* data2 = new float[mPigsize];
        float* data3 = new float[mPigsize];
        float* data4 = new float[mPigsize];
        float* data5 = new float[mPigsize];
        int* data6 = new int[mPigsize];
        float* data7 = new float[mPigsize];

        for(int k = 0; k < mPigsize; k++)
        {
            Pig* SavePig = mPigs[k];
            data1[k] = SavePig->position.x;
            data2[k] = SavePig->position.y;
            data3[k] = SavePig->position.z;
            data4[k] = SavePig->mainAngle;
            data5[k] = SavePig->HP;
            data6[k] = SavePig->number;
            data7[k] = SavePig->freezedTimerMax;
        }

        gzwrite(saveFile, data1,sizeof(float) * (mPigsize));
        gzwrite(saveFile, data2,sizeof(float) * (mPigsize));
        gzwrite(saveFile, data3,sizeof(float) *(mPigsize));
        gzwrite(saveFile, data4,sizeof(float)*(mPigsize));
        gzwrite(saveFile, data5,sizeof(float)*(mPigsize));
        gzwrite(saveFile, data6,sizeof(int)*(mPigsize));
        gzwrite(saveFile, data7,sizeof(float)*(mPigsize));

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
        delete[] data7;
    }

    // Optional passive-mob age blob (babies / breeding cooldown), missing on older saves.
    int ageMagic = 0x41474531;
    gzwrite(saveFile, &ageMagic,sizeof(int));
    int ageCount = mCows.size();
    gzwrite(saveFile, &ageCount,sizeof(int));
    for(int k = 0; k < ageCount; k++)
        gzwrite(saveFile, &mCows[k]->age,sizeof(float));
    ageCount = mSheeps.size();
    gzwrite(saveFile, &ageCount,sizeof(int));
    for(int k = 0; k < ageCount; k++)
        gzwrite(saveFile, &mSheeps[k]->age,sizeof(float));
    ageCount = mPigs.size();
    gzwrite(saveFile, &ageCount,sizeof(int));
    for(int k = 0; k < ageCount; k++)
        gzwrite(saveFile, &mPigs[k]->age,sizeof(float));

    // Optional chicken blob at the very tail of the file; older saves end after the age blob.
    int chickenMagic = 0x43484B31; // 'CHK1'
    gzwrite(saveFile, &chickenMagic,sizeof(int));
    int chickenCount = mChickens.size();
    gzwrite(saveFile, &chickenCount,sizeof(int));
    for(int k = 0; k < chickenCount; k++)
    {
        Chicken* SaveChicken = mChickens[k];

        float chickenData[9];
        chickenData[0] = SaveChicken->position.x;
        chickenData[1] = SaveChicken->position.y;
        chickenData[2] = SaveChicken->position.z;
        chickenData[3] = SaveChicken->mainAngle;
        chickenData[4] = SaveChicken->HP;
        chickenData[5] = (float)SaveChicken->number;
        chickenData[6] = SaveChicken->freezedTimerMax;
        chickenData[7] = SaveChicken->age;
        chickenData[8] = SaveChicken->loveTimer;

        gzwrite(saveFile, chickenData,sizeof(float)*9);
    }

    // Optional spider blob, appended the same way and for the same reason: a save written before
    // the spider existed ends after the chicken blob and is still readable.
    int spiderMagic = 0x53504431; // 'SPD1'
    gzwrite(saveFile, &spiderMagic,sizeof(int));
    int spiderCount = mSpiders.size();
    gzwrite(saveFile, &spiderCount,sizeof(int));
    for(int k = 0; k < spiderCount; k++)
    {
        Spider* SaveSpider = mSpiders[k];

        float spiderData[8];
        spiderData[0] = SaveSpider->position.x;
        spiderData[1] = SaveSpider->position.y;
        spiderData[2] = SaveSpider->position.z;
        spiderData[3] = SaveSpider->mainAngle;
        spiderData[4] = SaveSpider->HP;
        spiderData[5] = (float)SaveSpider->number;
        spiderData[6] = SaveSpider->freezedTimerMax;
        spiderData[7] = SaveSpider->age;

        gzwrite(saveFile, spiderData,sizeof(float)*8);
    }

    // Optional skeleton blob, appended for the same reason as the spider's: a save written before the
    // skeleton existed ends after the spider blob and is still readable.
    int skeletonMagic = 0x534B4C31; // 'SKL1'
    gzwrite(saveFile, &skeletonMagic,sizeof(int));
    int skeletonCount = mSkeletons.size();
    gzwrite(saveFile, &skeletonCount,sizeof(int));
    for(int k = 0; k < skeletonCount; k++)
    {
        Skeleton* SaveSkeleton = mSkeletons[k];

        float skeletonData[8];
        skeletonData[0] = SaveSkeleton->position.x;
        skeletonData[1] = SaveSkeleton->position.y;
        skeletonData[2] = SaveSkeleton->position.z;
        skeletonData[3] = SaveSkeleton->mainAngle;
        skeletonData[4] = SaveSkeleton->HP;
        skeletonData[5] = (float)SaveSkeleton->number;
        skeletonData[6] = SaveSkeleton->freezedTimerMax;
        skeletonData[7] = SaveSkeleton->age;

        gzwrite(saveFile, skeletonData,sizeof(float)*8);
    }

    int villagerMagic = 0x56494C31; // 'VIL1'
    gzwrite(saveFile, &villagerMagic,sizeof(int));
    int villagerCount = 0;
    gzwrite(saveFile, &villagerCount,sizeof(int));

    // Optional landed-arrow blob, at the tail after the skeleton blob and for the same reason: a
    // save written before arrows stuck in the ground ended after the skeleton blob and still loads.
    //
    // Only landed arrows are written. A shot still in flight is a fraction of a second of state with
    // nothing to show for it -- reloading one would leave the player looking at an arrow hanging in
    // mid-air, aimed at whatever it was aimed at when the world was left -- so flying arrows are
    // dropped, exactly as they are when any world is closed.
    int arrowMagic = 0x41525731; // 'ARW1'
    gzwrite(saveFile, &arrowMagic,sizeof(int));
    int arrowCount = 0;
    for(unsigned int i = 0; i < mArrows.size(); i++)
    {
        if(mArrows[i]->stuck == true)
            arrowCount ++;
    }
    gzwrite(saveFile, &arrowCount,sizeof(int));
    for(unsigned int i = 0; i < mArrows.size(); i++)
    {
        Arrow2* SaveArrow = mArrows[i];
        if(SaveArrow->stuck == false)
            continue;

        // Position, the heading it landed on, and the two flags that decide whether the player may
        // pick it up. Ten floats: the two booleans ride as 1/0 rather than being written as their
        // own types, so the record has one stride and the reader below cannot desync from it.
        const Vector3 dir = SaveArrow->FlightDir();
        float arrowData[10];
        arrowData[0] = SaveArrow->position.x;
        arrowData[1] = SaveArrow->position.y;
        arrowData[2] = SaveArrow->position.z;
        arrowData[3] = dir.x;
        arrowData[4] = dir.y;
        arrowData[5] = dir.z;
        arrowData[6] = (SaveArrow->fromMob == true) ? 1.0f : 0.0f;
        arrowData[7] = SaveArrow->pickupDelay;
        // Spare pair, left at zero so a future field can use them without changing the stride.
        arrowData[8] = 0.0f;
        arrowData[9] = 0.0f;

        gzwrite(saveFile, arrowData,sizeof(float)*10);
    }

    // Animals that walked out of the loaded ring. Older saves end at the arrow blob and still load.
    int parkMagic = 0x50415231; // 'PAR1'
    gzwrite(saveFile, &parkMagic,sizeof(int));
    gzwrite(saveFile, &parkedPassiveCount,sizeof(int));
    for(int i = 0; i < parkedPassiveCount; i++)
    {
        int kind = parkedPassive[i].kind;
        int extra = (int)parkedPassive[i].sheared | ((int)parkedPassive[i].color << 8);
        float parkData[6];
        parkData[0] = parkedPassive[i].x;
        parkData[1] = parkedPassive[i].y;
        parkData[2] = parkedPassive[i].z;
        parkData[3] = parkedPassive[i].angle;
        parkData[4] = parkedPassive[i].hp;
        parkData[5] = parkedPassive[i].age;
        gzwrite(saveFile, &kind,sizeof(int));
        gzwrite(saveFile, &extra,sizeof(int));
        gzwrite(saveFile, parkData,sizeof(float)*6);
    }

    //close file
    gzclose(saveFile);
    saveFile=0;
    MC_DebugHeapCheckpoint("save end");
}

void CraftWorld::LoadWorld(const char *filename)
{
    FILE * pFile;
    pFile = fopen(filename,"rb");

    if(pFile == NULL)
        return;

    //version
    fread(&worldVersion,sizeof(int),1,pFile);

    //name
    fread(worldName,sizeof(char),50,pFile);

    //size
    fread(&WORLD_SIZE,sizeof(int),1,pFile);

    //chunksize
    fread(&CHUNK_SIZE,sizeof(int),1,pFile);

    //cubes
    WORLD_HEIGHT = 112;
    unsigned char* flatBlocks = new unsigned char[WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE];
    fread(flatBlocks,sizeof(unsigned char),(WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE),pFile);
    worldGenerating = true;
    int cs = CHUNK_SIZE > 0 ? CHUNK_SIZE : 16;
    for(int cz = 0; cz * cs < WORLD_SIZE; cz++)
    {
        for(int cx = 0; cx * cs < WORLD_SIZE; cx++)
        {
            ChunkColumn* col = EnsureColumn(cx, cz, false);
            if(!col) continue;
            for(int lz = 0; lz < cs; lz++)
            {
                for(int y = 0; y < WORLD_HEIGHT; y++)
                {
                    for(int lx = 0; lx < cs; lx++)
                    {
                        int x = cx * cs + lx;
                        int z = cz * cs + lz;
                        if(x >= WORLD_SIZE || z >= WORLD_SIZE) continue;
                        int src = x + y * WORLD_SIZE + z * WORLD_SIZE * WORLD_HEIGHT;
                        col->blocks[col->Index(lx,y,lz)] = flatBlocks[src];
                    }
                }
            }
            col->generated = true;
        }
    }
    worldGenerating = false;
    delete[] flatBlocks;

    //close file
    fclose(pFile);
}

void CraftWorld::LoadCompressedWorld(std::string  filename) // Загрузка мира
{
    //load normal save info
    FILE * pFile;
    pFile = fopen(filename.c_str(),"rb");

    if(pFile == NULL)
        return;

    //version
    fread(&worldVersion,sizeof(int),1,pFile);

    fread(&gameModeWorld,sizeof(char),1,pFile);

    fread(&gameLocked,sizeof(bool),1,pFile);
    //name
    fread(worldName,sizeof(char),50,pFile);
    //close file
    fclose(pFile);

    worldSavePath = filename;

    //load compressed map data
    filename = filename + "c";

    //open file
    gzFile saveFile;
    saveFile = gzopen(filename.c_str(),"rb");
    if(saveFile == 0)
        return;

    //player pos
    gzread(saveFile, &playerPos.x,sizeof(float));
    gzread(saveFile, &playerPos.y,sizeof(float));
    gzread(saveFile, &playerPos.z,sizeof(float));

    //player spawn pos
    gzread(saveFile, &playerSpawnPointPosition.x,sizeof(float));
    gzread(saveFile, &playerSpawnPointPosition.y,sizeof(float));
    gzread(saveFile, &playerSpawnPointPosition.z,sizeof(float));

    //snow biome data
    gzread(saveFile, &snowBiomePosition.x,sizeof(float));
    gzread(saveFile, &snowBiomePosition.z,sizeof(float));
    gzread(saveFile, &snowBiomeRadius,sizeof(float));

    //desert biome data
    gzread(saveFile, &desertBiomePosition.x,sizeof(float));
    gzread(saveFile, &desertBiomePosition.z,sizeof(float));
    gzread(saveFile, &desertBiomeRadius,sizeof(float));

    //size
    gzread(saveFile, &WORLD_SIZE,sizeof(int));

    //size
    gzread(saveFile, &WORLD_HEIGHT,sizeof(int));

    //chunksize
    gzread(saveFile, &CHUNK_SIZE,sizeof(int));
    if(CHUNK_SIZE <= 0)
        CHUNK_SIZE = 16;
    if(WORLD_HEIGHT <= 0)
        WORLD_HEIGHT = 112;
    worldLimit = 4096;
    if(worldLimit < WORLD_SIZE)
        worldLimit = WORLD_SIZE;

    if(worldVersion < 151)
    {
        unsigned char* flatBlocks = new unsigned char[WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE];
        unsigned char* flatSettings = new unsigned char[WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE];
        memset(flatSettings, 0, sizeof(unsigned char) * WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE);
        gzread(saveFile, flatBlocks,sizeof(unsigned char)*(WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE));
        if(worldVersion >= 3)
            gzread(saveFile, flatSettings,sizeof(unsigned char)*(WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE));

        worldGenerating = true;
        int cs = CHUNK_SIZE;
        for(int cz = 0; cz * cs < WORLD_SIZE; cz++)
        {
            for(int cx = 0; cx * cs < WORLD_SIZE; cx++)
            {
                ChunkColumn* col = EnsureColumn(cx, cz, false);
                if(!col) continue;
                for(int lz = 0; lz < cs; lz++)
                {
                    for(int y = 0; y < WORLD_HEIGHT; y++)
                    {
                        for(int lx = 0; lx < cs; lx++)
                        {
                            int x = cx * cs + lx;
                            int z = cz * cs + lz;
                            if(x >= WORLD_SIZE || z >= WORLD_SIZE) continue;
                            int src = x + y * WORLD_SIZE + z * WORLD_SIZE * WORLD_HEIGHT;
                            col->blocks[col->Index(lx,y,lz)] = flatBlocks[src];
                            col->settings[col->Index(lx,y,lz)] = flatSettings[src];
                        }
                    }
                }
                col->generated = true;
            }
        }
        worldGenerating = false;
        delete[] flatBlocks;
        delete[] flatSettings;
    }

    if(worldVersion >= 3)
    {
        //world time
        gzread(saveFile, &worldDayTime,sizeof(float));

        //sun time
        //gzread(saveFile, &sunTime,sizeof(float));

        gzread(saveFile, &mainStatistics,sizeof(st));

        gzread(saveFile, &mainOptions,sizeof(opt));

        gzread(saveFile, &invId,sizeof(short)*(36));

        gzread(saveFile, &invAm,sizeof(short)*(36));

        gzread(saveFile, &invSt,sizeof(bool)*(36));

        gzread(saveFile, &armorId,sizeof(short)*(4));

        gzread(saveFile, &armorAm,sizeof(short)*(4));

        gzread(saveFile, &armorSt,sizeof(bool)*(4));

        gzread(saveFile, &HP,sizeof(unsigned int));

        gzread(saveFile, &HG,sizeof(unsigned int));

        gzread(saveFile, &OS,sizeof(unsigned int));

        gzread(saveFile, &brightFactor,sizeof(float));

        gzread(saveFile, &starsFactor,sizeof(float));

        gzread(saveFile, &worldSeed,sizeof(int));

        gzread(saveFile, &skyTime,sizeof(float));

        int loadedWeatherType = 0;
        gzread(saveFile, &loadedWeatherType,sizeof(int));
        worldWeather->SetWeatherType(loadedWeatherType);

        float loadedWeatherDuration = 0.0f;
        gzread(saveFile, &loadedWeatherDuration,sizeof(float));
        worldWeather->SetWeatherDuration(loadedWeatherDuration);

        gzread(saveFile, &mChestsize,sizeof(int));

        mChests.clear();

        if(mChestsize != 0)
        {
            int* data1 = new int[27*mChestsize];
            int* data2 = new int[27*mChestsize];
            bool* data3 = new bool[27*mChestsize];
            int* data4 = new int[mChestsize];
            int* data5 = new int[mChestsize];
            int* data6 = new int[mChestsize];


            gzread(saveFile, data1,sizeof(int)*(mChestsize*27));
            gzread(saveFile, data2,sizeof(int)*(mChestsize*27));
            gzread(saveFile, data3,sizeof(bool)*(mChestsize*27));

            gzread(saveFile, data4,sizeof(int)*(mChestsize));
            if(worldVersion >= 151)
            {
                gzread(saveFile, data5,sizeof(int)*(mChestsize));
                gzread(saveFile, data6,sizeof(int)*(mChestsize));
            }

            for(int k = 0; k < mChestsize; k++)
            {
                Chest* SaveChest = new Chest(-1,-1,-1);

                for(int l = 0; l <= 26; l++)
                {
                    SaveChest->chestSlotId[l] = data1[k * 27 + l];
                    SaveChest->chestSlotAm[l] = data2[k * 27 + l];
                    SaveChest->chestSlotSt[l] = data3[k * 27 + l];
                }
                if(worldVersion >= 151)
                {
                    SaveChest->chestX = data4[k];
                    SaveChest->chestY = data5[k];
                    SaveChest->chestZ = data6[k];
                }
                else
                {
                    int address = data4[k];
                    int size = WORLD_SIZE > 0 ? WORLD_SIZE : 252;
                    int height = WORLD_HEIGHT > 0 ? WORLD_HEIGHT : 112;
                    SaveChest->chestX = address % size;
                    int rest = address / size;
                    SaveChest->chestY = rest % height;
                    SaveChest->chestZ = rest / height;
                    SaveChest->chestAddress = address;
                }
                mChests.push_back(SaveChest);
            }
            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
            delete[] data5;
            delete[] data6;
        }


        gzread(saveFile, &mFurnacesize,sizeof(int));
        mFurnaces.clear();

        if(mFurnacesize != 0)
        {
            int* data1 = new int[3*mFurnacesize];
            int* data2 = new int[3*mFurnacesize];
            bool* data3 = new bool[3*mFurnacesize];

            int* data4 = new int[mFurnacesize];
            int* data5 = new int[mFurnacesize];
            int* data6 = new int[mFurnacesize];

            float* data7 = new float[mFurnacesize];
            float* data8 = new float[mFurnacesize];
            int* data10 = new int[mFurnacesize];

            char* data11 = new char[mFurnacesize];

            gzread(saveFile, data1,sizeof(int)*(mFurnacesize*3));
            gzread(saveFile, data2,sizeof(int)*(mFurnacesize*3));
            gzread(saveFile, data3,sizeof(bool)*(mFurnacesize*3));

            gzread(saveFile, data4,sizeof(int)*(mFurnacesize));
            gzread(saveFile, data5,sizeof(int)*(mFurnacesize));
            gzread(saveFile, data6,sizeof(int)*(mFurnacesize));

            gzread(saveFile, data7,sizeof(float)*(mFurnacesize));
            gzread(saveFile, data8,sizeof(float)*(mFurnacesize));
            gzread(saveFile, data10,sizeof(int)*(mFurnacesize));

            gzread(saveFile, data11,sizeof(char)*(mFurnacesize));

            for(int k = 0; k < mFurnacesize; k++)
            {
                Furnace* SaveFurnace = new Furnace(-1,-1,-1);

                for(int l = 0; l <= 2; l++)
                {
                    SaveFurnace->furnaceSlotId[l] = data1[k * 3 + l];
                    SaveFurnace->furnaceSlotAm[l] = data2[k * 3 + l];
                    SaveFurnace->furnaceSlotSt[l] = data3[k * 3 + l];
                }
                SaveFurnace->furnaceX = data4[k];
                SaveFurnace->furnaceY = data5[k];
                SaveFurnace->furnaceZ = data6[k];
                SaveFurnace->fuelTime = data7[k];
                SaveFurnace->meltingTime = data8[k];
                SaveFurnace->smeltItemTemp = data10[k];
                SaveFurnace->SetSide(data11[k]);
                mFurnaces.push_back(SaveFurnace);
            }
            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
            delete[] data5;
            delete[] data6;
            delete[] data7;
            delete[] data8;
            delete[] data10;
            delete[] data11;
        }

        mZombiesize = 0;
        gzread(saveFile, &mZombiesize,sizeof(int));

        mZombies.clear();

        if(mZombiesize != 0)
        {
            float* data1 = new float[mZombiesize];
            float* data2 = new float[mZombiesize];
            float* data3 = new float[mZombiesize];

            float* data4 = new float[mZombiesize];
            float* data5 = new float[mZombiesize];
            int* data6 = new int[mZombiesize];

            int* data7 = new int[mZombiesize];
            int* data8 = new int[mZombiesize];
            int* data9 = new int[mZombiesize];
            int* data10 = new int[mZombiesize];

            gzread(saveFile, data1,sizeof(float)*(mZombiesize));
            gzread(saveFile, data2,sizeof(float)*(mZombiesize));
            gzread(saveFile, data3,sizeof(float)*(mZombiesize));

            gzread(saveFile, data4,sizeof(float)*(mZombiesize));
            gzread(saveFile, data5,sizeof(float)*(mZombiesize));
            gzread(saveFile, data6,sizeof(int)*(mZombiesize));

            gzread(saveFile, data7,sizeof(int)*(mZombiesize));
            gzread(saveFile, data8,sizeof(int)*(mZombiesize));
            gzread(saveFile, data9,sizeof(int)*(mZombiesize));
            gzread(saveFile, data10,sizeof(int)*(mZombiesize));

            for(int g = 0; g < mZombiesize; g++)
            {
                Zombie* SaveZombie = new Zombie(Vector3(data1[g],data2[g],data3[g]), data6[g]);

                SaveZombie->mainAngle = data4[g];
                SaveZombie->HP = data5[g];
                SaveZombie->helmetId = data7[g];
                SaveZombie->chestplateId = data8[g];
                SaveZombie->leggingsId = data9[g];
                SaveZombie->bootsId = data10[g];

                mZombies.push_back(SaveZombie);
            }
            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
            delete[] data5;
            delete[] data6;
            delete[] data7;
            delete[] data8;
            delete[] data9;
            delete[] data10;
        }

        mCowsize = 0;
        gzread(saveFile, &mCowsize,sizeof(int));

        mCows.clear();

        if(mCowsize != 0)
        {
            float* data1 = new float[mCowsize];
            float* data2 = new float[mCowsize];
            float* data3 = new float[mCowsize];

            float* data4 = new float[mCowsize];
            float* data5 = new float[mCowsize];
            int* data6 = new int[mCowsize];


            gzread(saveFile, data1,sizeof(float)*(mCowsize));
            gzread(saveFile, data2,sizeof(float)*(mCowsize));
            gzread(saveFile, data3,sizeof(float)*(mCowsize));

            gzread(saveFile, data4,sizeof(float)*(mCowsize));
            gzread(saveFile, data5,sizeof(float)*(mCowsize));
            gzread(saveFile, data6,sizeof(int)*(mCowsize));


            for(int g = 0; g < mCowsize; g++)
            {
                Cow* SaveCow = new Cow(Vector3(data1[g],data2[g],data3[g]), data6[g]);

                SaveCow->mainAngle = data4[g];
                SaveCow->HP = data5[g];

                mCows.push_back(SaveCow);
            }
            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
            delete[] data5;
            delete[] data6;
        }


        mSheepsize = 0;
        gzread(saveFile, &mSheepsize,sizeof(int));

        mSheeps.clear();

        if(mSheepsize != 0)
        {
            float* data1 = new float[mSheepsize];
            float* data2 = new float[mSheepsize];
            float* data3 = new float[mSheepsize];

            float* data4 = new float[mSheepsize];
            float* data5 = new float[mSheepsize];
            int* data6 = new int[mSheepsize];

            int* data7 = new int[mSheepsize];
            bool* data8 = new bool[mSheepsize];

            gzread(saveFile, data1,sizeof(float)*(mSheepsize));
            gzread(saveFile, data2,sizeof(float)*(mSheepsize));
            gzread(saveFile, data3,sizeof(float)*(mSheepsize));

            gzread(saveFile, data4,sizeof(float)*(mSheepsize));
            gzread(saveFile, data5,sizeof(float)*(mSheepsize));
            gzread(saveFile, data6,sizeof(int)*(mSheepsize));

            gzread(saveFile, data7,sizeof(int)*(mSheepsize));
            gzread(saveFile, data8,sizeof(bool)*(mSheepsize));


            for(int g = 0; g < mSheepsize; g++)
            {
                Sheep* SaveSheep = new Sheep(Vector3(data1[g],data2[g],data3[g]), data6[g]);

                SaveSheep->mainAngle = data4[g];
                SaveSheep->HP = data5[g];
                SaveSheep->SetFurColor(data7[g]);
                SaveSheep->sheared = data8[g];

                mSheeps.push_back(SaveSheep);
            }
            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
            delete[] data5;
            delete[] data6;
            delete[] data7;
            delete[] data8;
        }



        mCreepersize = 0;
        gzread(saveFile, &mCreepersize,sizeof(int));

        mCreepers.clear();

        if(mCreepersize != 0)
        {
            float* data1 = new float[mCreepersize];
            float* data2 = new float[mCreepersize];
            float* data3 = new float[mCreepersize];

            float* data4 = new float[mCreepersize];
            float* data5 = new float[mCreepersize];
            int* data6 = new int[mCreepersize];

            float* data7 = new float[mCreepersize];

            gzread(saveFile, data1,sizeof(float)*(mCreepersize));
            gzread(saveFile, data2,sizeof(float)*(mCreepersize));
            gzread(saveFile, data3,sizeof(float)*(mCreepersize));

            gzread(saveFile, data4,sizeof(float)*(mCreepersize));
            gzread(saveFile, data5,sizeof(float)*(mCreepersize));
            gzread(saveFile, data6,sizeof(int)*(mCreepersize));

            gzread(saveFile, data7,sizeof(float)*(mCreepersize));


            for(int g = 0; g < mCreepersize; g++)
            {
                Creeper* SaveCreeper = new Creeper(Vector3(data1[g],data2[g],data3[g]), data6[g]);

                SaveCreeper->mainAngle = data4[g];
                SaveCreeper->HP = data5[g];
                SaveCreeper->freezedTimerMax = data7[g];

                mCreepers.push_back(SaveCreeper);
            }
            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
            delete[] data5;
            delete[] data6;
            delete[] data7;
        }

		/*mPigsize = 0;
        gzread(saveFile, &mPigsize,sizeof(int));

        mPigs.clear();

        if(mPigsize != 0)
        {
            float* data1 = new float[mPigsize];
            float* data2 = new float[mPigsize];
            float* data3 = new float[mPigsize];

            float* data4 = new float[mPigsize];
            float* data5 = new float[mPigsize];
            int* data6 = new int[mPigsize];

            float* data7 = new float[mPigsize];

            gzread(saveFile, data1,sizeof(float)*(mPigsize));
            gzread(saveFile, data2,sizeof(float)*(mPigsize));
            gzread(saveFile, data3,sizeof(float)*(mPigsize));

            gzread(saveFile, data4,sizeof(float)*(mPigsize));
            gzread(saveFile, data5,sizeof(float)*(mPigsize));
            gzread(saveFile, data6,sizeof(int)*(mPigsize));

            gzread(saveFile, data7,sizeof(float)*(mPigsize));


            for(int g = 0; g < mPigsize; g++)
            {
                Pig* SavePig = new Pig(Vector3(data1[g],data2[g],data3[g]), data6[g]);

                SavePig->mainAngle = data4[g];
                SavePig->HP = data5[g];
                SavePig->freezedTimerMax = data7[g];

                mPigs.push_back(SavePig);
            }
            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
            delete[] data5;
            delete[] data6;
            delete[] data7;
        }

        gzread(saveFile, &zombieNum,sizeof(int));*/

        // loading TNT data
        mTNTsize = 0;
        gzread(saveFile, &mTNTsize,sizeof(int));

        mTNTs.clear();

        if(mTNTsize != 0)
        {
            float* data1 = new float[mTNTsize];
            float* data2 = new float[mTNTsize];
            float* data3 = new float[mTNTsize];

            float* data4 = new float[mTNTsize];


            gzread(saveFile, data1,sizeof(float)*(mTNTsize));
            gzread(saveFile, data2,sizeof(float)*(mTNTsize));
            gzread(saveFile, data3,sizeof(float)*(mTNTsize));

            gzread(saveFile, data4,sizeof(float)*(mTNTsize));

            for(int g = 0; g < mTNTsize; g++)
            {
                TNT* SaveTNT = new TNT(this, Vector3(data1[g],data2[g],data3[g]), Vector3(0,0,0));

                SaveTNT->timeToExplode = data4[g];

                mTNTs.push_back(SaveTNT);
            }
            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
        }

        // loading NoteBlocks data
        mNoteBlocksize = 0;
        gzread(saveFile, &mNoteBlocksize,sizeof(int));

        mNoteBlocks.clear();

        if(mNoteBlocksize != 0)
        {
            int* data1 = new int[mNoteBlocksize];
            int* data2 = new int[mNoteBlocksize];
            int* data3 = new int[mNoteBlocksize];
            int* data4 = new int[mNoteBlocksize];

            gzread(saveFile, data1,sizeof(int)*(mNoteBlocksize));
            gzread(saveFile, data2,sizeof(int)*(mNoteBlocksize));
            gzread(saveFile, data3,sizeof(int)*(mNoteBlocksize));
            gzread(saveFile, data4,sizeof(int)*(mNoteBlocksize));

            for(int k = 0; k < mNoteBlocksize; k++)
            {
                NoteBlockEntity* SaveNoteBlock = new NoteBlockEntity(data1[k],data2[k],data3[k],data4[k]);
                mNoteBlocks.push_back(SaveNoteBlock);
            }

            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
        }

        // loading Jukebox data
        mJukeboxsize = 0;
        gzread(saveFile, &mJukeboxsize,sizeof(int));

        mJukeboxes.clear();

        if(mJukeboxsize != 0)
        {
            int* data1 = new int[mJukeboxsize];
            int* data2 = new int[mJukeboxsize];
            int* data3 = new int[mJukeboxsize];
            int* data4 = new int[mJukeboxsize];

            gzread(saveFile, data1,sizeof(int)*(mJukeboxsize));
            gzread(saveFile, data2,sizeof(int)*(mJukeboxsize));
            gzread(saveFile, data3,sizeof(int)*(mJukeboxsize));
            gzread(saveFile, data4,sizeof(int)*(mJukeboxsize));

            for(int k = 0; k < mJukeboxsize; k++)
            {
                JukeboxEntity* SaveJukebox = new JukeboxEntity(data1[k],data2[k],data3[k],data4[k]);
                mJukeboxes.push_back(SaveJukebox);
            }

            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
        }


        // loading monstaer Spawners data
        mMonsterSpawnersize = 0;
        gzread(saveFile, &mMonsterSpawnersize,sizeof(int));

        mMonsterSpawners.clear();

        if(mMonsterSpawnersize != 0)
        {
            int* data1 = new int[mMonsterSpawnersize];
            int* data2 = new int[mMonsterSpawnersize];
            int* data3 = new int[mMonsterSpawnersize];
            int* data4 = new int[mMonsterSpawnersize];

            gzread(saveFile, data1,sizeof(int)*(mMonsterSpawnersize));
            gzread(saveFile, data2,sizeof(int)*(mMonsterSpawnersize));
            gzread(saveFile, data3,sizeof(int)*(mMonsterSpawnersize));
            gzread(saveFile, data4,sizeof(int)*(mMonsterSpawnersize));

            for(int k = 0; k < mMonsterSpawnersize; k++)
            {
                MonsterSpawnerEntity* SaveMS;

                if(data4[k] == 1)
                {
                    SaveMS = new MonsterSpawnerEntity(data1[k],data2[k],data3[k],"zombie");
                }
                else
                {
                    SaveMS = new MonsterSpawnerEntity(data1[k],data2[k],data3[k]," ");
                }

                mMonsterSpawners.push_back(SaveMS);
            }

            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
        }


        // loading item frames data
        mItemFramesize = 0;
        gzread(saveFile, &mItemFramesize,sizeof(int));

        mItemFrames.clear();

        if(mItemFramesize != 0)
        {
            int* data1 = new int[mItemFramesize];
            int* data2 = new int[mItemFramesize];
            int* data3 = new int[mItemFramesize];
            int* data4 = new int[mItemFramesize];
            int* data5 = new int[mItemFramesize];
            int* data6 = new int[mItemFramesize];

            gzread(saveFile, data1,sizeof(int)*(mItemFramesize));
            gzread(saveFile, data2,sizeof(int)*(mItemFramesize));
            gzread(saveFile, data3,sizeof(int)*(mItemFramesize));
            gzread(saveFile, data4,sizeof(int)*(mItemFramesize));
            gzread(saveFile, data5,sizeof(int)*(mItemFramesize));
            gzread(saveFile, data6,sizeof(int)*(mItemFramesize));

            for(int k = 0; k < mItemFramesize; k++)
            {
                ItemFrameEntity* SaveIF;
                SaveIF = new ItemFrameEntity(data1[k],data2[k],data3[k],data4[k]);
                SaveIF->SetItem(data5[k]);
                SaveIF->SetAmount(data6[k]);

                mItemFrames.push_back(SaveIF);
            }

            delete[] data1;
            delete[] data2;
            delete[] data3;
            delete[] data4;
            delete[] data5;
            delete[] data6;
        }
    }
    else
    {
    }

    if(worldVersion >= 151)
    {
        gzread(saveFile, &worldType,sizeof(int));
        // The dimension is not stored; it follows from the world type. Without this a loaded nether
        // would generate as the nether but light as a cave, which is a black screen.
        dimension = (worldType == kWorldTypeNether) ? 1 : 0;
        char loadedTrees = 1;
        char loadedWater = 1;
        char loadedCaves = 1;
        gzread(saveFile, &loadedTrees,sizeof(char));
        gzread(saveFile, &loadedWater,sizeof(char));
        gzread(saveFile, &loadedCaves,sizeof(char));
        makeTrees = loadedTrees != 0;
        makeWater = loadedWater != 0;
        makeCaves = loadedCaves != 0;
        int loadedWaterLevel = 0;
        if(gzread(saveFile, &loadedWaterLevel,sizeof(int)) == (int)sizeof(int) && loadedWaterLevel >= 8 && loadedWaterLevel < WORLD_HEIGHT)
            waterLevel = loadedWaterLevel;
        int loadedStamp = 0;
        if(gzread(saveFile, &loadedStamp,sizeof(int)) == (int)sizeof(int))
            worldStamp = loadedStamp;
    }

    // Optional pig blob appended after worldStamp (missing on older saves).
    mPigs.clear();
    mPigsize = 0;
    if(gzread(saveFile, &mPigsize,sizeof(int)) == (int)sizeof(int) && mPigsize > 0 && mPigsize < 256)
    {
        float* data1 = new float[mPigsize];
        float* data2 = new float[mPigsize];
        float* data3 = new float[mPigsize];
        float* data4 = new float[mPigsize];
        float* data5 = new float[mPigsize];
        int* data6 = new int[mPigsize];
        float* data7 = new float[mPigsize];

        gzread(saveFile, data1,sizeof(float)*(mPigsize));
        gzread(saveFile, data2,sizeof(float)*(mPigsize));
        gzread(saveFile, data3,sizeof(float)*(mPigsize));
        gzread(saveFile, data4,sizeof(float)*(mPigsize));
        gzread(saveFile, data5,sizeof(float)*(mPigsize));
        gzread(saveFile, data6,sizeof(int)*(mPigsize));
        gzread(saveFile, data7,sizeof(float)*(mPigsize));

        for(int g = 0; g < mPigsize; g++)
        {
            Pig* SavePig = new Pig(Vector3(data1[g],data2[g],data3[g]), data6[g]);
            SavePig->world = this;
            SavePig->mainAngle = data4[g];
            SavePig->HP = data5[g];
            SavePig->freezedTimerMax = data7[g];
            mPigs.push_back(SavePig);
            if(data6[g] > zombieNum)
                zombieNum = data6[g];
        }

        delete[] data1;
        delete[] data2;
        delete[] data3;
        delete[] data4;
        delete[] data5;
        delete[] data6;
        delete[] data7;
    }
    else
    {
        mPigsize = 0;
    }

    int ageMagic = 0;
    if(gzread(saveFile, &ageMagic,sizeof(int)) == (int)sizeof(int) && ageMagic == 0x41474531)
    {
        for(int group = 0; group < 3; group++)
        {
            int ageCount = 0;
            if(gzread(saveFile, &ageCount,sizeof(int)) != (int)sizeof(int) || ageCount < 0 || ageCount > 1024)
                break;

            int mobCount = group == 0 ? (int)mCows.size() : (group == 1 ? (int)mSheeps.size() : (int)mPigs.size());
            for(int k = 0; k < ageCount; k++)
            {
                float savedAge = 0.0f;
                if(gzread(saveFile, &savedAge,sizeof(float)) != (int)sizeof(float))
                    break;
                if(ageCount != mobCount)
                    continue;
                if(group == 0) mCows[k]->SetAge(savedAge);
                else if(group == 1) mSheeps[k]->SetAge(savedAge);
                else mPigs[k]->SetAge(savedAge);
            }
        }
    }

    // Optional chicken blob at the tail (older saves end after the age blob).
    int chickenMagic = 0;
    if(gzread(saveFile, &chickenMagic,sizeof(int)) == (int)sizeof(int) && chickenMagic == 0x43484B31)
    {
        int chickenCount = 0;
        if(gzread(saveFile, &chickenCount,sizeof(int)) == (int)sizeof(int) && chickenCount >= 0 && chickenCount < 256)
        {
            DestroyAllChickens();

            for(int k = 0; k < chickenCount; k++)
            {
                float chickenData[9];
                if(gzread(saveFile, chickenData,sizeof(float)*9) != (int)sizeof(float)*9)
                    break;

                SpawnChicken(chickenData[0], chickenData[1], chickenData[2], chickenData[6]);

                if(mChickens.empty() == true)
                    continue;

                Chicken* SaveChicken = mChickens.back();
                SaveChicken->mainAngle = chickenData[3];
                SaveChicken->HP = chickenData[4];
                SaveChicken->freezedTimerMax = chickenData[6];
                SaveChicken->SetAge(chickenData[7]);
                SaveChicken->loveTimer = chickenData[8];

                int savedNumber = (int)chickenData[5];
                SaveChicken->number = savedNumber;
                if(savedNumber > zombieNum)
                    zombieNum = savedNumber;
            }
        }
    }

    // Optional spider blob, at the tail after the chicken blob.
    int spiderMagic = 0;
    if(gzread(saveFile, &spiderMagic,sizeof(int)) == (int)sizeof(int) && spiderMagic == 0x53504431)
    {
        int spiderCount = 0;
        if(gzread(saveFile, &spiderCount,sizeof(int)) == (int)sizeof(int) && spiderCount >= 0 && spiderCount < 256)
        {
            DestroyAllSpiders();

            for(int k = 0; k < spiderCount; k++)
            {
                float spiderData[8];
                if(gzread(saveFile, spiderData,sizeof(float)*8) != (int)sizeof(float)*8)
                    break;

                SpawnSpider(spiderData[0], spiderData[1], spiderData[2], spiderData[6]);

                if(mSpiders.empty() == true)
                    continue;

                Spider* SaveSpider = mSpiders.back();
                SaveSpider->mainAngle = spiderData[3];
                SaveSpider->HP = spiderData[4];
                SaveSpider->freezedTimerMax = spiderData[6];
                SaveSpider->age = spiderData[7];

                int savedNumber = (int)spiderData[5];
                SaveSpider->number = savedNumber;
                if(savedNumber > zombieNum)
                    zombieNum = savedNumber;
            }
        }
    }

    // Optional skeleton blob, at the tail after the spider blob.
    int skeletonMagic = 0;
    if(gzread(saveFile, &skeletonMagic,sizeof(int)) == (int)sizeof(int) && skeletonMagic == 0x534B4C31)
    {
        int skeletonCount = 0;
        if(gzread(saveFile, &skeletonCount,sizeof(int)) == (int)sizeof(int) && skeletonCount >= 0 && skeletonCount < 256)
        {
            DestroyAllSkeletons();

            for(int k = 0; k < skeletonCount; k++)
            {
                float skeletonData[8];
                if(gzread(saveFile, skeletonData,sizeof(float)*8) != (int)sizeof(float)*8)
                    break;

                SpawnSkeleton(skeletonData[0], skeletonData[1], skeletonData[2], skeletonData[6]);

                if(mSkeletons.empty() == true)
                    continue;

                Skeleton* SaveSkeleton = mSkeletons.back();
                SaveSkeleton->mainAngle = skeletonData[3];
                SaveSkeleton->HP = skeletonData[4];
                SaveSkeleton->freezedTimerMax = skeletonData[6];
                SaveSkeleton->age = skeletonData[7];

                int savedNumber = (int)skeletonData[5];
                SaveSkeleton->number = savedNumber;
                if(savedNumber > zombieNum)
                    zombieNum = savedNumber;
            }
        }
    }

    int villagerMagic = 0;
    if(gzread(saveFile, &villagerMagic,sizeof(int)) == (int)sizeof(int) && villagerMagic == 0x56494C31)
    {
        int villagerCount = 0;
        if(gzread(saveFile, &villagerCount,sizeof(int)) == (int)sizeof(int) && villagerCount >= 0 && villagerCount < 256)
        {
            for(int k = 0; k < villagerCount; k++)
            {
                float villagerData[9];
                if(gzread(saveFile, villagerData,sizeof(float)*9) != (int)sizeof(float)*9)
                    break;
            }
        }
    }

    // Optional landed-arrow blob, at the tail after the skeleton blob. Nothing clears the arrows on
    // load the way DestroyAllSkeletons does for the mobs, and nothing needs to: a world that was just
    // loaded starts with an empty mArrows, because LoadCompressedWorld is what builds the world in
    // the first place. The clear is here anyway so that loading a save *over* a live session -- which
    // the converter and the in-game reload both do -- cannot leave the previous world's arrows in the
    // air of the new one.
    int arrowMagic = 0;
    if(gzread(saveFile, &arrowMagic,sizeof(int)) == (int)sizeof(int) && arrowMagic == 0x41525731)
    {
        int arrowCount = 0;
        if(gzread(saveFile, &arrowCount,sizeof(int)) == (int)sizeof(int) && arrowCount >= 0 && arrowCount <= kMaxLandedArrows)
        {
            for(int i = 0; i < (int)mArrows.size(); i++)
            {
                delete mArrows[i];
            }
            mArrows.clear();

            const int arrowTexture = TextureHelper::Instance()->GetTexture(TextureHelper::arrowProjectileTexture);

            for(int k = 0; k < arrowCount; k++)
            {
                float arrowData[10];
                if(gzread(saveFile, arrowData,sizeof(float)*10) != (int)sizeof(float)*10)
                    break;

                Arrow2* SaveArrow = new Arrow2(arrowData[0], arrowData[1], arrowData[2]);
                SaveArrow->SetTexture(arrowTexture);
                SaveArrow->RestoreLanded(Vector3(arrowData[0],arrowData[1],arrowData[2]),
                                         Vector3(arrowData[3],arrowData[4],arrowData[5]),
                                         arrowData[6] != 0.0f,
                                         arrowData[7]);
                mArrows.push_back(SaveArrow);
            }
        }
    }

    parkedPassiveCount = 0;
    int parkMagic = 0;
    if(gzread(saveFile, &parkMagic,sizeof(int)) == (int)sizeof(int) && parkMagic == 0x50415231)
    {
        int parkCount = 0;
        if(gzread(saveFile, &parkCount,sizeof(int)) == (int)sizeof(int) && parkCount > 0 && parkCount <= kParkedPassiveMax)
        {
            for(int i = 0; i < parkCount; i++)
            {
                int kind = 0;
                int extra = 0;
                float parkData[6];
                if(gzread(saveFile, &kind,sizeof(int)) != (int)sizeof(int))
                    break;
                if(gzread(saveFile, &extra,sizeof(int)) != (int)sizeof(int))
                    break;
                if(gzread(saveFile, parkData,sizeof(float)*6) != (int)sizeof(float)*6)
                    break;
                if(kind < 0 || kind > 3)
                    continue;
                ParkedPassive& p = parkedPassive[parkedPassiveCount++];
                p.kind = (unsigned char)kind;
                p.sheared = (unsigned char)(extra & 0xff);
                p.color = (unsigned char)((extra >> 8) & 0xff);
                p.pad = 0;
                p.x = parkData[0];
                p.y = parkData[1];
                p.z = parkData[2];
                p.angle = parkData[3];
                p.hp = parkData[4];
                p.age = parkData[5];
            }
        }
    }

    //close file
    gzclose(saveFile);
    saveFile=0;
    MC_DebugHeapCheckpoint("load columns end");

    if(!worldGen)
    {
        worldGen = new WorldGenerator();
        worldGen->SetSeed(worldSeed);
    }
    // load the 3x3 columns around the player before the first frame so collision
    // (which treats missing columns as solid) does not pin the player in place
    for(int i = 0; i < 9; i++)
        UpdateStreaming();
}
