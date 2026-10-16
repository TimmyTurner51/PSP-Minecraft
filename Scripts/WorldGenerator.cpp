#include "WorldGenerator.h"
#include "CraftWorld2.h"
#include "LoadingScreen.h"
#include "DebugConfig.h"

#include <Aurora/Utils/Logger.h>
#include <psputils.h>
#include <pspkernel.h>

#define PI 3.1415926535897f

#define OLD 0 
#define SUPERFLAT 1 
#define Forest 2
#define DesertLakes 3
#define SnowyTaiga 4
//Flat
#define FlatDesertLakes 5
#define FlatSnowyTaiga 6
#define FlatOcean 7
// The nether world type. Defined in terms of CraftWorld's constant so the one place that has to
// recover a dimension from a saved world type and this generator cannot drift apart.
#define FlatNETHER CraftWorld::kWorldTypeNether

using namespace noisepp;

static int RandGenX(CraftWorld *world, int pad)
{
    int a = world->genMinX + pad;
    int b = world->genMaxX - pad;
    if(b <= a)
        return (world->genMinX + world->genMaxX) / 2;
    return a + rand() % (b - a);
}

static int RandGenZ(CraftWorld *world, int pad)
{
    int a = world->genMinZ + pad;
    int b = world->genMaxZ - pad;
    if(b <= a)
        return (world->genMinZ + world->genMaxZ) / 2;
    return a + rand() % (b - a);
}

static bool NearGenRim(CraftWorld *world, int x, int z, int rim)
{
    return x < world->genMinX + rim || x >= world->genMaxX - rim ||
           z < world->genMinZ + rim || z >= world->genMaxZ - rim;
}

static int ScaleGenCount(CraftWorld *world, int fullMapCount)
{
    int w = world->genMaxX - world->genMinX;
    int d = world->genMaxZ - world->genMinZ;
    if(w < 1) w = 1;
    if(d < 1) d = 1;
    int n = (int)(((long long)fullMapCount * w * d) / (256LL * 256LL));
    if(fullMapCount > 0 && n < 1)
        n = 1;
    return n;
}

static int CaveStartY(CraftWorld *world, int x, int z)
{
    int gh = world->groundHeight(x, z);
    if(gh < 40)
        return 12;
    return 6 + rand() % (gh - 32);
}

static void BounceCaveXZ(CraftWorld *world, int &x, int &z, bool &reverseX, bool &reverseZ)
{
    if(x > world->genMaxX - 5 || x < world->genMinX + 4)
    {
        if(x > (world->genMinX + world->genMaxX) / 2)
            x = world->genMaxX - 6;
        else
            x = world->genMinX + 5;
        reverseX = !reverseX;
    }
    if(z > world->genMaxZ - 5 || z < world->genMinZ + 4)
    {
        if(z > (world->genMinZ + world->genMaxZ) / 2)
            z = world->genMaxZ - 6;
        else
            z = world->genMinZ + 5;
        reverseZ = !reverseZ;
    }
}

void WorldGenerator::initRandompMap(int worldSize, int worldHeight, int chunkSize, CraftWorld *world, bool makeTrees,bool makeWater,bool makeCaves, unsigned int seedII, int worldType)
{
    LoadingScreen* loading = new LoadingScreen();

    loading->readiness = 1;
    loading->stateName = 1;

    int WORLD_SIZE = worldSize;

    if (seedII == 0)
    {
        seedII = rand() % 1000000;
    }
    unsigned int seed = seedII;
    srand(seed);

    noiseGridOn = false;
    world->worldSeed = seed;
    // per-world id: same seed twice still gets different column files
    world->worldStamp = (int)sceKernelLibcTime(0) ^ (int)(seed * 2654435761u);
    if(world->worldStamp == 0)
        world->worldStamp = 1;
    SetSeed(seed);
    world->worldType = worldType;
    // A world created with the nether biome *is* the nether, so record it on the world rather than
    // leaving every later reader to work it out from the biome number: the lighting needs to know,
    // and it must not disagree with the generator about which world this is.
    world->dimension = (worldType == FlatNETHER) ? 1 : 0;
    world->makeTrees = makeTrees;
    world->makeWater = makeWater;
    world->makeCaves = makeCaves;
    world->waterLevel = 58;
    world->worldGenerating = true;
    world->SetupSpawnGeneration();
    const int WORLD_MID = (world->genMinX + world->genMaxX) / 2;

    int grass_;
    bool genGrass = true;

    if(worldType == OLD)
    {
        int octave_count = 2;
        int waterLevel = 58;
        int middleHeight = 64;

        Real persistence = 0.05;
        Real frequency = 1.4;
        Real scale = 2.12;
        Real lacunarity = 2.0;

        {
            std::string path = "Terrain/options.txt";
            std::string str;
            std::string file_contents;
            std::string flag = "";

            std::ifstream file(path.c_str());

            if (file.is_open())
            {
                while (std::getline(file, str))
                {
                    if(flag.compare("<water_level>") == 0)
                    {
                        file_contents = "";
                        file_contents += str;
                        file_contents.push_back('\n');

                        waterLevel = std::atoi(file_contents.c_str());
                        flag = "";

                        continue;
                    }
                    if(flag.compare("<grass>") == 0)
                    {
                        file_contents = "";
                        file_contents += str;
                        file_contents.push_back('\n');

                        grass_ = std::atoi(file_contents.c_str());
                        flag = "";

                        continue;
                    }

                    flag.clear();
                    flag += str;
                    flag.erase(flag.end()-1);
                }
            }

            file.close();
        }

        if(grass_ == 0)
        {
            genGrass = false;
        }

        world->waterLevel = waterLevel;
        PerlinModule* perlin = new PerlinModule();
        perlin->setQuality(noisepp::NOISE_QUALITY_HIGH);

        loading->stateName = 1; // terrain generation
        for (int z = world->genMinZ; z < world->genMaxZ; z++)
        {
            for (int x = world->genMinX; x < world->genMaxX; x++)
            {
                loading->readiness = (int)(((float)((x - world->genMinX) + (z - world->genMinZ) * (world->genMaxX - world->genMinX)) / (float)((world->genMaxX - world->genMinX) * (world->genMaxZ - world->genMinZ) + 1)) * 60.0f);
                PlaceDefaultTerrainColumn(world, x, z, worldType, waterLevel, world->WORLD_HEIGHT);
            }
        }

        loading->readiness = 60;
        loading->stateName = 2; // cave generation

        if(makeCaves)
        {
            float *dataVer = new float[150];
            float *dataHor = new float[150];
            noisepp::utils::PlaneBuilder2D wormBuilder;

            int probX = -1;
            int probY = -1;
            int probZ = -1;

            int numWorms = ScaleGenCount(world, 100) + rand() % 3;
            for(int i = 0; i < numWorms; i++)
            {
                int steps = 50 + rand() % 100;
                int radius = 2.5f;

                perlin->setSeed(seed+1030+i);
                perlin->setOctaveCount(6);
                perlin->setPersistence(0.6f);
                perlin->setFrequency(1.2f);
                perlin->setScale(3);
                perlin->setLacunarity(1.8f);

                wormBuilder.setModule(perlin);
                wormBuilder.setSize(1, 150);
                wormBuilder.setBounds(-3, -3, 3, 3);

                wormBuilder.setDestination(dataVer);
                wormBuilder.build();


                perlin->setSeed(seed+2030+i);
                perlin->setOctaveCount(6);
                perlin->setPersistence(0.6f);
                perlin->setFrequency(8.2f);
                perlin->setScale(3);
                perlin->setLacunarity(1.8f);

                wormBuilder.setModule(perlin);
                wormBuilder.setSize(1, 150);
                wormBuilder.setBounds(-3, -3, 3, 3);

                wormBuilder.setDestination(dataHor);
                wormBuilder.build();

                int x,y,z;

                if(probX == -1)
                {
                    x = RandGenX(world, 10);
                    z = RandGenZ(world, 10);
                    y = CaveStartY(world, x, z);
                }
                else
                {
                    x = probX;
                    y = probY;
                    z = probZ;

                    probX = -1;
                    probY = -1;
                    probZ = -1;
                }

                if(rand() % 3 == 0) // ground cave
                {
                    y = world->groundHeight(x,z)+2;
                }

                bool reverseX = false;
                bool reverseY = false;
                bool reverseZ = false;

                for(int j = 0; j < steps; j++)
                {


                    radius = 1.5f+(rand()%14)/10.0f;

                    if(j == (int)(steps/2) && rand() % 3 == 0)
                    {
                        probX = x;
                        probY = y;
                        probZ = z;
                    }

                    if(rand()%32 == 0)
                    {
                        continue;
                    }

                    bool haveWater = false;
                    for(int xx = x-radius; xx <= x+radius; xx ++)
                    {
                        for(int zz = z-radius; zz <= z+radius; zz ++)
                        {
                            for(int yy = y-radius; yy <= y+radius; yy ++)
                            {
                                if(world->GetBlock(xx,yy,zz) == IceBlock::getID() || world->GetBlock(xx,yy,zz) == WaterBlock::getID())
                                {
                                    haveWater = true;
                                }
                            }
                        }
                    }

                    if(haveWater == false)
                    {
                        if(!NearGenRim(world, x, z, world->CHUNK_SIZE + 4))
                            world->BuildBlockSphere(radius,0,x,y,z);

                        if(rand()%14 == 0)
                        {
                            radius = 1.5f+(rand()%10)/10.0f;
                            world->BuildBlockSphere(radius,0,x+radius*1.5f,y,z,RockBlock::getID());
                            world->BuildBlockSphere(radius,0,x+radius*1.5f,y,z+radius*1.5f,RockBlock::getID());
                            world->BuildBlockSphere(radius,0,x+radius*1.5f,y+radius*1.5f,z+radius*1.5f,RockBlock::getID());
                            world->BuildBlockSphere(radius,0,x,y-radius*1.5f,z+radius*1.5f,RockBlock::getID());
                            world->BuildBlockSphere(radius,0,x,y+radius*1.5f,z+radius*1.5f,RockBlock::getID());
                        }
                    }

                    reverseX == false ? x += world->LengthDirX((dataHor[j]),(dataVer[j]))*radius*1.5f : x -= world->LengthDirX((dataHor[j]),(dataVer[j]))*radius*1.5f;
                    reverseY == false ? y += world->LengthDirY((dataHor[j]),(dataVer[j]))*radius*1.5f : y -= world->LengthDirY((dataHor[j]),(dataVer[j]))*radius*1.5f;
                    reverseZ == false ? z += world->LengthDirZ((dataHor[j]),(dataVer[j]))*radius*1.5f : z -= world->LengthDirZ((dataHor[j]),(dataVer[j]))*radius*1.5f;

                    if(y <= 6)
                    {
                        y = 7;
                        reverseY = !reverseY;
                    }

                    BounceCaveXZ(world, x, z, reverseX, reverseZ);
                }
            }
            delete [] dataVer;
            delete [] dataHor;
        }

        initDirt(WORLD_SIZE, world);
        initGravel(WORLD_SIZE, world);
        //initErosion(WORLD_HEIGHT, world);

        loading->readiness = 80;
        loading->stateName = 4; // trees planting

        // trees, plants, canes, cactus, pumpkins, pockets: same code streamed columns use
        DecorateSpawnRegion(world);

        loading->readiness = 85;
        loading->stateName = 5; // ore placing

        initBedrock(WORLD_SIZE, world);

        loading->readiness = 90;
        loading->stateName = 6;

        world->initWorldBlocksLight();

        loading->readiness = 100;
        loading->stateName = 7;

        initLavaLakes(WORLD_SIZE, world);
        initWaterLakes(WORLD_SIZE, world);
        initOre(WORLD_SIZE,world,CoalOre::getID(),0.83,2,80,3);
        initOre(WORLD_SIZE,world,IronOre::getID(),0.57,2,61,3);
        initOre(WORLD_SIZE,world,GoldBlock::getID(),0.25,2,30,2);
        initOre(WORLD_SIZE,world,DiamondOre::getID(),0.094,1,15,2);
        initOre(WORLD_SIZE,world,LapisOre::getID(),0.13,1,34,3);
        initOre(WORLD_SIZE,world,RedstoneOre::getID(),0.6,1,14,3);

        initDungeons(WORLD_SIZE, world);

        world->SpawnStartAnimals(2, 1);

        int tries = 0;
        Vector3 newPlayerSpawnPosition = Vector3(1,0,1);
        while(tries < 3072)
        {
            int border = 48;
            if(tries > 512)
            {
                border -= 16;
            }
            if(tries > 1024)
            {
                border -= 16;
            }
            if(tries > 1536)
            {
                border -= 8;
            }

            newPlayerSpawnPosition = Vector3(RandGenX(world, border),0,RandGenZ(world, border));
            int groundBlock = world->GetBlock(newPlayerSpawnPosition.x,world->groundHeightWater(newPlayerSpawnPosition.x,newPlayerSpawnPosition.z),newPlayerSpawnPosition.z);
            if(groundBlock != WaterBlock::getID())
            {
                if(groundBlock == Snow2::getID())
                {
                    newPlayerSpawnPosition = Vector3(newPlayerSpawnPosition.x+0.5f,world->groundHeightWater(newPlayerSpawnPosition.x,newPlayerSpawnPosition.z)+world->playerGrowth-0.05f,newPlayerSpawnPosition.z+0.5f);
                }
                else
                {
                    newPlayerSpawnPosition = Vector3(newPlayerSpawnPosition.x+0.5f,world->groundHeightWater(newPlayerSpawnPosition.x,newPlayerSpawnPosition.z)+world->playerGrowth-0.05f+1,newPlayerSpawnPosition.z+0.5f);
                }
                world->SetPlayerSpawnPosition(newPlayerSpawnPosition);
                break;
            }
            if(tries == 3071)
            {
                world->SetPlayerSpawnPosition(Vector3(WORLD_MID+0.5f,world->groundHeightWater(WORLD_MID,WORLD_MID)+world->playerGrowth-0.05f-0.5f,WORLD_MID+0.5f));
            }
            tries ++;
        }

        delete perlin;
    }

	if (worldType == SnowyTaiga)
	{
		bool snowBiomeIncluded = true;
		bool snowBiomeSuperLarge = true;
		bool desertBiomeIncluded = false;
		bool desertBiomeSuperLarge = false;
		int octave_count = 2;
		int waterLevel = 58;
		int middleHeight = 64;

		Real persistence = 0.05;
		Real frequency = 1.4;
		Real scale = 2.12;
		Real lacunarity = 2.0;

		{
			std::string path = "Terrain/options.txt";
			std::string str;
			std::string file_contents;
			std::string flag = "";

			std::ifstream file(path.c_str());

			if (file.is_open())
			{
				while (std::getline(file, str))
				{
					if (flag.compare("<water_level>") == 0)
					{
						file_contents = "";
						file_contents += str;
						file_contents.push_back('\n');

						waterLevel = std::atoi(file_contents.c_str());
						flag = "";

						continue;
					}
					if (flag.compare("<grass>") == 0)
					{
						file_contents = "";
						file_contents += str;
						file_contents.push_back('\n');

						grass_ = std::atoi(file_contents.c_str());
						flag = "";

						continue;
					}

					flag.clear();
					flag += str;
					flag.erase(flag.end() - 1);
				}
			}

			file.close();
		}

		if (grass_ == 0)
		{
			genGrass = false;
		}

		world->waterLevel = waterLevel;
		PerlinModule* perlin = new PerlinModule();
		perlin->setQuality(noisepp::NOISE_QUALITY_HIGH);

		loading->stateName = 1; // terrain generation
		for (int z = world->genMinZ; z < world->genMaxZ; z++)
		{
			for (int x = world->genMinX; x < world->genMaxX; x++)
			{
				loading->readiness = ((float)(x + z * WORLD_SIZE) / (66503.0f)) * 100;
				PlaceDefaultTerrainColumn(world, x, z, worldType, waterLevel, world->WORLD_HEIGHT);
			}
		}

		loading->readiness = 60;
		loading->stateName = 2; // cave generation

		if (makeCaves)
		{
			float* dataVer = new float[150];
			float* dataHor = new float[150];
			noisepp::utils::PlaneBuilder2D wormBuilder;

			int probX = -1;
			int probY = -1;
			int probZ = -1;

			int numWorms = ScaleGenCount(world, 100) + rand() % 3;
			for (int i = 0; i < numWorms; i++)
			{
				int steps = 50 + rand() % 100;
				int radius = 2.5f;

				perlin->setSeed(seed + 1030 + i);
				perlin->setOctaveCount(6);
				perlin->setPersistence(0.6f);
				perlin->setFrequency(1.2f);
				perlin->setScale(3);
				perlin->setLacunarity(1.8f);

				wormBuilder.setModule(perlin);
				wormBuilder.setSize(1, 150);
				wormBuilder.setBounds(-3, -3, 3, 3);

				wormBuilder.setDestination(dataVer);
				wormBuilder.build();


				perlin->setSeed(seed + 2030 + i);
				perlin->setOctaveCount(6);
				perlin->setPersistence(0.6f);
				perlin->setFrequency(8.2f);
				perlin->setScale(3);
				perlin->setLacunarity(1.8f);

				wormBuilder.setModule(perlin);
				wormBuilder.setSize(1, 150);
				wormBuilder.setBounds(-3, -3, 3, 3);

				wormBuilder.setDestination(dataHor);
				wormBuilder.build();

				int x, y, z;

				if (probX == -1)
				{
					x = RandGenX(world, 10);
					z = RandGenZ(world, 10);
                    y = CaveStartY(world, x, z);
				}
				else
				{
					x = probX;
					y = probY;
					z = probZ;

					probX = -1;
					probY = -1;
					probZ = -1;
				}

				if (rand() % 3 == 0) // ground cave
				{
					y = world->groundHeight(x, z) + 2;
				}

				bool reverseX = false;
				bool reverseY = false;
				bool reverseZ = false;

				for (int j = 0; j < steps; j++)
				{


					radius = 1.5f + (rand() % 14) / 10.0f;

					if (j == (int)(steps / 2) && rand() % 3 == 0)
					{
						probX = x;
						probY = y;
						probZ = z;
					}

					if (rand() % 32 == 0)
					{
						continue;
					}

					bool haveWater = false;
					for (int xx = x - radius; xx <= x + radius; xx++)
					{
						for (int zz = z - radius; zz <= z + radius; zz++)
						{
							for (int yy = y - radius; yy <= y + radius; yy++)
							{
								if (world->GetBlock(xx, yy, zz) == WaterBlock::getID())
								{
									haveWater = true;
								}
							}
						}
					}

					if (haveWater == false)
					{
						if(!NearGenRim(world, x, z, world->CHUNK_SIZE + 4))
							world->BuildBlockSphere(radius, 0, x, y, z);

						if (rand() % 14 == 0)
						{
							radius = 1.5f + (rand() % 10) / 10.0f;
							world->BuildBlockSphere(radius, 0, x + radius * 1.5f, y, z, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x + radius * 1.5f, y, z + radius * 1.5f, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x + radius * 1.5f, y + radius * 1.5f, z + radius * 1.5f, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x, y - radius * 1.5f, z + radius * 1.5f, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x, y + radius * 1.5f, z + radius * 1.5f, RockBlock::getID());
						}
					}

					reverseX == false ? x += world->LengthDirX((dataHor[j]), (dataVer[j])) * radius * 1.5f : x -= world->LengthDirX((dataHor[j]), (dataVer[j])) * radius * 1.5f;
					reverseY == false ? y += world->LengthDirY((dataHor[j]), (dataVer[j])) * radius * 1.5f : y -= world->LengthDirY((dataHor[j]), (dataVer[j])) * radius * 1.5f;
					reverseZ == false ? z += world->LengthDirZ((dataHor[j]), (dataVer[j])) * radius * 1.5f : z -= world->LengthDirZ((dataHor[j]), (dataVer[j])) * radius * 1.5f;

					if (y <= 6)
					{
						y = 7;
						reverseY = !reverseY;
					}

					BounceCaveXZ(world, x, z, reverseX, reverseZ);
				}
			}
			delete[] dataVer;
			delete[] dataHor;
		}

		initDirt(WORLD_SIZE, world);
		initGravel(WORLD_SIZE, world);
		//initErosion(WORLD_HEIGHT, world);

		loading->readiness = 80;
		loading->stateName = 4; // trees planting

		DecorateSpawnRegion(world);

		loading->readiness = 85;
		loading->stateName = 5; // ore placing

		initBedrock(WORLD_SIZE, world);

		loading->readiness = 90;
		loading->stateName = 6;

		world->initWorldBlocksLight();

		loading->readiness = 100;
		loading->stateName = 7;

		initLavaLakes(WORLD_SIZE, world);
		initWaterLakes(WORLD_SIZE, world);
		initOre(WORLD_SIZE, world, CoalOre::getID(), 0.83, 2, 80, 3);
		initOre(WORLD_SIZE, world, IronOre::getID(), 0.57, 2, 61, 3);
		initOre(WORLD_SIZE, world, GoldBlock::getID(), 0.25, 2, 30, 2);
		initOre(WORLD_SIZE, world, DiamondOre::getID(), 0.094, 1, 15, 2);
		initOre(WORLD_SIZE, world, LapisOre::getID(), 0.13, 1, 34, 3);
		initOre(WORLD_SIZE, world, RedstoneOre::getID(), 0.6, 1, 14, 3);

		initDungeons(WORLD_SIZE, world);

		world->SpawnStartAnimals(2, 1);

		int tries = 0;
		Vector3 newPlayerSpawnPosition = Vector3(1, 0, 1);
		while (tries < 3072)
		{
			int border = 48;
			if (tries > 512)
			{
				border -= 16;
			}
			if (tries > 1024)
			{
				border -= 16;
			}
			if (tries > 1536)
			{
				border -= 8;
			}

			newPlayerSpawnPosition = Vector3(RandGenX(world, border), 0, RandGenZ(world, border));
			int groundBlock = world->GetBlock(newPlayerSpawnPosition.x, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z), newPlayerSpawnPosition.z);
			if (groundBlock != WaterBlock::getID())
			{
				newPlayerSpawnPosition = Vector3(newPlayerSpawnPosition.x + 0.5f, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z) + world->playerGrowth - 0.05f + 1, newPlayerSpawnPosition.z + 0.5f);
				world->SetPlayerSpawnPosition(newPlayerSpawnPosition);
				break;
			}
			if (tries == 3071)
			{
				world->SetPlayerSpawnPosition(Vector3(WORLD_MID + 0.5f, world->groundHeightWater(WORLD_MID, WORLD_MID) + world->playerGrowth - 0.05f - 0.5f, WORLD_MID + 0.5f));
			}
			tries++;
		}

		delete perlin;
	}
	if (worldType == Forest)
	{
		bool snowBiomeIncluded = false;
		bool snowBiomeSuperLarge = false;
		bool desertBiomeIncluded = false;
		bool desertBiomeSuperLarge = false;
		int octave_count = 2;
		int waterLevel = 58;
		int middleHeight = 64;

		Real persistence = 0.05;
		Real frequency = 1.4;
		Real scale = 2.12;
		Real lacunarity = 2.0;

		{
			std::string path = "Terrain/options.txt";
			std::string str;
			std::string file_contents;
			std::string flag = "";

			std::ifstream file(path.c_str());

			if (file.is_open())
			{
				while (std::getline(file, str))
				{
					if (flag.compare("<water_level>") == 0)
					{
						file_contents = "";
						file_contents += str;
						file_contents.push_back('\n');

						waterLevel = std::atoi(file_contents.c_str());
						flag = "";

						continue;
					}
					if (flag.compare("<grass>") == 0)
					{
						file_contents = "";
						file_contents += str;
						file_contents.push_back('\n');

						grass_ = std::atoi(file_contents.c_str());
						flag = "";

						continue;
					}

					flag.clear();
					flag += str;
					flag.erase(flag.end() - 1);
				}
			}

			file.close();
		}

		if (grass_ == 0)
		{
			genGrass = false;
		}

		world->waterLevel = waterLevel;
		PerlinModule* perlin = new PerlinModule();
		perlin->setQuality(noisepp::NOISE_QUALITY_HIGH);

		loading->stateName = 1; // terrain generation
		for (int z = world->genMinZ; z < world->genMaxZ; z++)
		{
			for (int x = world->genMinX; x < world->genMaxX; x++)
			{
				loading->readiness = ((float)(x + z * WORLD_SIZE) / (66503.0f)) * 100;
				PlaceDefaultTerrainColumn(world, x, z, worldType, waterLevel, world->WORLD_HEIGHT);
			}
		}

		loading->readiness = 60;
		loading->stateName = 2; // cave generation

		if (makeCaves)
		{
			float* dataVer = new float[150];
			float* dataHor = new float[150];
			noisepp::utils::PlaneBuilder2D wormBuilder;

			int probX = -1;
			int probY = -1;
			int probZ = -1;

			int numWorms = ScaleGenCount(world, 100) + rand() % 3;
			for (int i = 0; i < numWorms; i++)
			{
				int steps = 50 + rand() % 100;
				int radius = 2.5f;

				perlin->setSeed(seed + 1030 + i);
				perlin->setOctaveCount(6);
				perlin->setPersistence(0.6f);
				perlin->setFrequency(1.2f);
				perlin->setScale(3);
				perlin->setLacunarity(1.8f);

				wormBuilder.setModule(perlin);
				wormBuilder.setSize(1, 150);
				wormBuilder.setBounds(-3, -3, 3, 3);

				wormBuilder.setDestination(dataVer);
				wormBuilder.build();


				perlin->setSeed(seed + 2030 + i);
				perlin->setOctaveCount(6);
				perlin->setPersistence(0.6f);
				perlin->setFrequency(8.2f);
				perlin->setScale(3);
				perlin->setLacunarity(1.8f);

				wormBuilder.setModule(perlin);
				wormBuilder.setSize(1, 150);
				wormBuilder.setBounds(-3, -3, 3, 3);

				wormBuilder.setDestination(dataHor);
				wormBuilder.build();

				int x, y, z;

				if (probX == -1)
				{
					x = RandGenX(world, 10);
					z = RandGenZ(world, 10);
                    y = CaveStartY(world, x, z);
				}
				else
				{
					x = probX;
					y = probY;
					z = probZ;

					probX = -1;
					probY = -1;
					probZ = -1;
				}

				if (rand() % 3 == 0) // ground cave
				{
					y = world->groundHeight(x, z) + 2;
				}

				bool reverseX = false;
				bool reverseY = false;
				bool reverseZ = false;

				for (int j = 0; j < steps; j++)
				{


					radius = 1.5f + (rand() % 14) / 10.0f;

					if (j == (int)(steps / 2) && rand() % 3 == 0)
					{
						probX = x;
						probY = y;
						probZ = z;
					}

					if (rand() % 32 == 0)
					{
						continue;
					}

					bool haveWater = false;
					for (int xx = x - radius; xx <= x + radius; xx++)
					{
						for (int zz = z - radius; zz <= z + radius; zz++)
						{
							for (int yy = y - radius; yy <= y + radius; yy++)
							{
								if (world->GetBlock(xx, yy, zz) == WaterBlock::getID())
								{
									haveWater = true;
								}
							}
						}
					}

					if (haveWater == false)
					{
						if(!NearGenRim(world, x, z, world->CHUNK_SIZE + 4))
							world->BuildBlockSphere(radius, 0, x, y, z);

						if (rand() % 14 == 0)
						{
							radius = 1.5f + (rand() % 10) / 10.0f;
							world->BuildBlockSphere(radius, 0, x + radius * 1.5f, y, z, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x + radius * 1.5f, y, z + radius * 1.5f, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x + radius * 1.5f, y + radius * 1.5f, z + radius * 1.5f, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x, y - radius * 1.5f, z + radius * 1.5f, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x, y + radius * 1.5f, z + radius * 1.5f, RockBlock::getID());
						}
					}

					reverseX == false ? x += world->LengthDirX((dataHor[j]), (dataVer[j])) * radius * 1.5f : x -= world->LengthDirX((dataHor[j]), (dataVer[j])) * radius * 1.5f;
					reverseY == false ? y += world->LengthDirY((dataHor[j]), (dataVer[j])) * radius * 1.5f : y -= world->LengthDirY((dataHor[j]), (dataVer[j])) * radius * 1.5f;
					reverseZ == false ? z += world->LengthDirZ((dataHor[j]), (dataVer[j])) * radius * 1.5f : z -= world->LengthDirZ((dataHor[j]), (dataVer[j])) * radius * 1.5f;

					if (y <= 6)
					{
						y = 7;
						reverseY = !reverseY;
					}

					BounceCaveXZ(world, x, z, reverseX, reverseZ);
				}
			}
			delete[] dataVer;
			delete[] dataHor;
		}

		initDirt(WORLD_SIZE, world);
		initGravel(WORLD_SIZE, world);
		//initErosion(WORLD_HEIGHT, world);

		loading->readiness = 80;
		loading->stateName = 4; // trees planting

		DecorateSpawnRegion(world);

		loading->readiness = 85;
		loading->stateName = 5; // ore placing

		initBedrock(WORLD_SIZE, world);

		loading->readiness = 90;
		loading->stateName = 6;

		world->initWorldBlocksLight();

		loading->readiness = 100;
		loading->stateName = 7;

		initLavaLakes(WORLD_SIZE, world);
		initWaterLakes(WORLD_SIZE, world);
		initOre(WORLD_SIZE, world, CoalOre::getID(), 0.83, 2, 80, 3);
		initOre(WORLD_SIZE, world, IronOre::getID(), 0.57, 2, 61, 3);
		initOre(WORLD_SIZE, world, GoldBlock::getID(), 0.25, 2, 30, 2);
		initOre(WORLD_SIZE, world, DiamondOre::getID(), 0.094, 1, 15, 2);
		initOre(WORLD_SIZE, world, LapisOre::getID(), 0.13, 1, 34, 3);
		initOre(WORLD_SIZE, world, RedstoneOre::getID(), 0.6, 1, 14, 3);

		initDungeons(WORLD_SIZE, world);

		world->SpawnStartAnimals(2, 1);

		int tries = 0;
		Vector3 newPlayerSpawnPosition = Vector3(1, 0, 1);
		while (tries < 3072)
		{
			int border = 48;
			if (tries > 512)
			{
				border -= 16;
			}
			if (tries > 1024)
			{
				border -= 16;
			}
			if (tries > 1536)
			{
				border -= 8;
			}

			newPlayerSpawnPosition = Vector3(RandGenX(world, border), 0, RandGenZ(world, border));
			int groundBlock = world->GetBlock(newPlayerSpawnPosition.x, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z), newPlayerSpawnPosition.z);
			if (groundBlock != WaterBlock::getID())
			{
				newPlayerSpawnPosition = Vector3(newPlayerSpawnPosition.x + 0.5f, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z) + world->playerGrowth - 0.05f + 1, newPlayerSpawnPosition.z + 0.5f);
				world->SetPlayerSpawnPosition(newPlayerSpawnPosition);
				break;
			}
			if (tries == 3071)
			{
				world->SetPlayerSpawnPosition(Vector3(WORLD_MID + 0.5f, world->groundHeightWater(WORLD_MID, WORLD_MID) + world->playerGrowth - 0.05f - 0.5f, WORLD_MID + 0.5f));
			}
			tries++;
		}

		delete perlin;
	}
	if (worldType == DesertLakes)
	{
		bool snowBiomeIncluded = false;
		bool snowBiomeSuperLarge = false;
		bool desertBiomeIncluded = true;
		bool desertBiomeSuperLarge = true;
		int octave_count = 2;
		int waterLevel = 58;
		int middleHeight = 64;

		Real persistence = 0.05;
		Real frequency = 1.4;
		Real scale = 2.12;
		Real lacunarity = 2.0;

		{
			std::string path = "Terrain/options.txt";
			std::string str;
			std::string file_contents;
			std::string flag = "";

			std::ifstream file(path.c_str());

			if (file.is_open())
			{
				while (std::getline(file, str))
				{
					if (flag.compare("<water_level>") == 0)
					{
						file_contents = "";
						file_contents += str;
						file_contents.push_back('\n');

						waterLevel = std::atoi(file_contents.c_str());
						flag = "";

						continue;
					}
					if (flag.compare("<grass>") == 0)
					{
						file_contents = "";
						file_contents += str;
						file_contents.push_back('\n');

						grass_ = std::atoi(file_contents.c_str());
						flag = "";

						continue;
					}

					flag.clear();
					flag += str;
					flag.erase(flag.end() - 1);
				}
			}

			file.close();
		}

		if (grass_ == 0)
		{
			genGrass = false;
		}

		world->waterLevel = waterLevel;
		PerlinModule* perlin = new PerlinModule();
		perlin->setQuality(noisepp::NOISE_QUALITY_HIGH);

		loading->stateName = 1; // terrain generation
		for (int z = world->genMinZ; z < world->genMaxZ; z++)
		{
			for (int x = world->genMinX; x < world->genMaxX; x++)
			{
				loading->readiness = ((float)(x + z * WORLD_SIZE) / (66503.0f)) * 100;
				PlaceDefaultTerrainColumn(world, x, z, worldType, waterLevel, world->WORLD_HEIGHT);
			}
		}

		loading->readiness = 60;
		loading->stateName = 2; // cave generation

		if (makeCaves)
		{
			float* dataVer = new float[150];
			float* dataHor = new float[150];
			noisepp::utils::PlaneBuilder2D wormBuilder;

			int probX = -1;
			int probY = -1;
			int probZ = -1;

			int numWorms = ScaleGenCount(world, 100) + rand() % 3;
			for (int i = 0; i < numWorms; i++)
			{
				int steps = 50 + rand() % 100;
				int radius = 2.5f;

				perlin->setSeed(seed + 1030 + i);
				perlin->setOctaveCount(6);
				perlin->setPersistence(0.6f);
				perlin->setFrequency(1.2f);
				perlin->setScale(3);
				perlin->setLacunarity(1.8f);

				wormBuilder.setModule(perlin);
				wormBuilder.setSize(1, 150);
				wormBuilder.setBounds(-3, -3, 3, 3);

				wormBuilder.setDestination(dataVer);
				wormBuilder.build();


				perlin->setSeed(seed + 2030 + i);
				perlin->setOctaveCount(6);
				perlin->setPersistence(0.6f);
				perlin->setFrequency(8.2f);
				perlin->setScale(3);
				perlin->setLacunarity(1.8f);

				wormBuilder.setModule(perlin);
				wormBuilder.setSize(1, 150);
				wormBuilder.setBounds(-3, -3, 3, 3);

				wormBuilder.setDestination(dataHor);
				wormBuilder.build();

				int x, y, z;

				if (probX == -1)
				{
					x = RandGenX(world, 10);
					z = RandGenZ(world, 10);
                    y = CaveStartY(world, x, z);
				}
				else
				{
					x = probX;
					y = probY;
					z = probZ;

					probX = -1;
					probY = -1;
					probZ = -1;
				}

				if (rand() % 3 == 0) // ground cave
				{
					y = world->groundHeight(x, z) + 2;
				}

				bool reverseX = false;
				bool reverseY = false;
				bool reverseZ = false;

				for (int j = 0; j < steps; j++)
				{


					radius = 1.5f + (rand() % 14) / 10.0f;

					if (j == (int)(steps / 2) && rand() % 3 == 0)
					{
						probX = x;
						probY = y;
						probZ = z;
					}

					if (rand() % 32 == 0)
					{
						continue;
					}

					bool haveWater = false;
					for (int xx = x - radius; xx <= x + radius; xx++)
					{
						for (int zz = z - radius; zz <= z + radius; zz++)
						{
							for (int yy = y - radius; yy <= y + radius; yy++)
							{
								if (world->GetBlock(xx, yy, zz) == IceBlock::getID() || world->GetBlock(xx, yy, zz) == WaterBlock::getID())
								{
									haveWater = true;
								}
							}
						}
					}

					if (haveWater == false)
					{
						if(!NearGenRim(world, x, z, world->CHUNK_SIZE + 4))
							world->BuildBlockSphere(radius, 0, x, y, z);

						if (rand() % 14 == 0)
						{
							radius = 1.5f + (rand() % 10) / 10.0f;
							world->BuildBlockSphere(radius, 0, x + radius * 1.5f, y, z, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x + radius * 1.5f, y, z + radius * 1.5f, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x + radius * 1.5f, y + radius * 1.5f, z + radius * 1.5f, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x, y - radius * 1.5f, z + radius * 1.5f, RockBlock::getID());
							world->BuildBlockSphere(radius, 0, x, y + radius * 1.5f, z + radius * 1.5f, RockBlock::getID());
						}
					}

					reverseX == false ? x += world->LengthDirX((dataHor[j]), (dataVer[j])) * radius * 1.5f : x -= world->LengthDirX((dataHor[j]), (dataVer[j])) * radius * 1.5f;
					reverseY == false ? y += world->LengthDirY((dataHor[j]), (dataVer[j])) * radius * 1.5f : y -= world->LengthDirY((dataHor[j]), (dataVer[j])) * radius * 1.5f;
					reverseZ == false ? z += world->LengthDirZ((dataHor[j]), (dataVer[j])) * radius * 1.5f : z -= world->LengthDirZ((dataHor[j]), (dataVer[j])) * radius * 1.5f;

					if (y <= 6)
					{
						y = 7;
						reverseY = !reverseY;
					}

					BounceCaveXZ(world, x, z, reverseX, reverseZ);
				}
			}
			delete[] dataVer;
			delete[] dataHor;
		}

		initDirt(WORLD_SIZE, world);
		initGravel(WORLD_SIZE, world);
		//initErosion(WORLD_HEIGHT, world);

		loading->readiness = 80;
		loading->stateName = 4; // trees planting

		DecorateSpawnRegion(world);

		loading->readiness = 85;
		loading->stateName = 5; // ore placing

		initBedrock(WORLD_SIZE, world);

		loading->readiness = 90;
		loading->stateName = 6;

		world->initWorldBlocksLight();

		loading->readiness = 100;
		loading->stateName = 7;

		initLavaLakes(WORLD_SIZE, world);
		initWaterLakes(WORLD_SIZE, world);
		initOre(WORLD_SIZE, world, CoalOre::getID(), 0.83, 2, 80, 3);
		initOre(WORLD_SIZE, world, IronOre::getID(), 0.57, 2, 61, 3);
		initOre(WORLD_SIZE, world, GoldBlock::getID(), 0.25, 2, 30, 2);
		initOre(WORLD_SIZE, world, DiamondOre::getID(), 0.094, 1, 15, 2);
		initOre(WORLD_SIZE, world, LapisOre::getID(), 0.13, 1, 34, 3);
		initOre(WORLD_SIZE, world, RedstoneOre::getID(), 0.6, 1, 14, 3);

		initDungeons(WORLD_SIZE, world);

		world->SpawnStartAnimals(2, 1);

		int tries = 0;
		Vector3 newPlayerSpawnPosition = Vector3(1, 0, 1);
		while (tries < 3072)
		{
			int border = 48;
			if (tries > 512)
			{
				border -= 16;
			}
			if (tries > 1024)
			{
				border -= 16;
			}
			if (tries > 1536)
			{
				border -= 8;
			}

			newPlayerSpawnPosition = Vector3(RandGenX(world, border), 0, RandGenZ(world, border));
			int groundBlock = world->GetBlock(newPlayerSpawnPosition.x, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z), newPlayerSpawnPosition.z);
			if (groundBlock != WaterBlock::getID())
			{
				newPlayerSpawnPosition = Vector3(newPlayerSpawnPosition.x + 0.5f, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z) + world->playerGrowth - 0.05f + 1, newPlayerSpawnPosition.z + 0.5f);
				world->SetPlayerSpawnPosition(newPlayerSpawnPosition);
				break;
			}
			if (tries == 3071)
			{
				world->SetPlayerSpawnPosition(Vector3(WORLD_MID + 0.5f, world->groundHeightWater(WORLD_MID, WORLD_MID) + world->playerGrowth - 0.05f - 0.5f, WORLD_MID + 0.5f));
			}
			tries++;
		}

		delete perlin;
	}
    if(worldType == SUPERFLAT)
    {
        loading->readiness = 1;
        loading->stateName = 1;
        int superflatWorldHeight = 46;
        for(int x = world->genMinX; x < world->genMaxX; x++)
        {
            loading->readiness = ((float)x/WORLD_SIZE)*50 + 1;
            for(int z = world->genMinZ; z < world->genMaxZ; z++)
            {
                for(int y = 0; y <= superflatWorldHeight; y++)
                {
                    if(y == 2)
                    {
                        world->GetBlock(x,y,z) = bedrock::getID(); // bedrock
                    }
                    else
                    {
                        world->GetBlock(x,y,z) = bedrock::getID(); 
                        if(rand() % 384 == 44)
                        {
                            //world->GetBlock(x,y,z) = 41 + rand() % 5; // stuff for survival mod
                        }

                        if(y >= superflatWorldHeight-2)
                        {
                            world->GetBlock(x,y,z) = DirtBlock::getID(); // Dirt
                            if(rand() % 384 == 0)
                            {
                                //world->GetBlock(x,y,z) = WoodBlock::getID(); // stuff for survival mod
                            }
                        }
                        if(y == superflatWorldHeight)
                        {
                            world->GetBlock(x,y,z) = GrassBlock::getID();
                        }
                    }
                }
            }
        }
        loading->stateName = 7;
        loading->stateName = 90;
        world->initWorldBlocksLight();
        world->SetPlayerSpawnPosition(Vector3(WORLD_MID,world->groundHeight(WORLD_MID,WORLD_MID)+world->playerGrowth-0.05f+1,WORLD_MID));
    }
	if (worldType == FlatNETHER)
	{
		loading->readiness = 1;
		loading->stateName = 1;
		// A column at a time, through the same function the streamed columns use, so the spawn
		// region and everything beyond it are the same nether. This branch used to build a flat
		// slab eight blocks thick here while GenerateColumn built a full column out there.
		int cs = world->CHUNK_SIZE;
		for (int cx = world->genMinX / cs; cx < world->genMaxX / cs; cx++)
		{
			loading->readiness = ((float)(cx * cs) / WORLD_SIZE) * 50 + 1;
			for (int cz = world->genMinZ / cs; cz < world->genMaxZ / cs; cz++)
				GenerateNetherColumn(world, cx, cz);
		}
		loading->stateName = 7;
		loading->stateName = 90;
		world->initWorldBlocksLight();
		// Deliberately not groundHeight(): that scans down from the top of the world, so under a
		// bedrock ceiling it returns the ceiling and the player is placed on top of the world.
		int spawnX = WORLD_MID, spawnY = 64, spawnZ = WORLD_MID;
		if (!FindNetherSpawn(world, spawnX, spawnY, spawnZ))
		{
			// No real room anywhere near the middle: make one, rather than dropping the player into
			// whatever happens to be there. Sized like the one the finder looks for.
			for (int dx = -1; dx <= 1; dx++)
			{
				for (int dz = -1; dz <= 1; dz++)
				{
					world->GetBlock(WORLD_MID + dx, spawnY - 1, WORLD_MID + dz) = Netherrack::getID();
					for (int dy = 0; dy < 4; dy++)
						world->GetBlock(WORLD_MID + dx, spawnY + dy, WORLD_MID + dz) = 0;
				}
			}
		}
		world->SetPlayerSpawnPosition(Vector3(spawnX, spawnY + world->playerGrowth - 0.05f, spawnZ));
#if MC_NETHER_PROBE
		// What the generator actually produced for the spawn region. "It looks like the nether" would
		// not catch an all-solid world, one that is all lava, or a spawn inside the lava sea; these
		// numbers would. Temporary -- see MC_NETHER_PROBE in DebugConfig.h.
		{
			int air = 0, netherrackCount = 0, lava = 0, glowstone = 0, soulsand = 0, other = 0;
			int bandTotal[8] = {0};
			int bandOpen[8] = {0};
			int h = world->WORLD_HEIGHT;
			for (int x = world->genMinX; x < world->genMaxX; x++)
			{
				for (int z = world->genMinZ; z < world->genMaxZ; z++)
				{
					for (int y = 1; y < h; y++)
					{
						block_t b = world->GetBlock(x, y, z);
						if (b == 0) air++;
						else if (b == Netherrack::getID()) netherrackCount++;
						else if (b == Lava::getID()) lava++;
						else if (b == GlowstoneBlock::getID()) glowstone++;
						else if (b == Soulsand::getID()) soulsand++;
						else other++;
						int band = (y * 8) / h;
						if (band > 7) band = 7;
						bandTotal[band]++;
						if (b == 0 || b == Lava::getID()) bandOpen[band]++;
					}
				}
			}
			Aurora::Utils::Logger::Instance()->LogMessage("[netherprobe] region x%d..%d z%d..%d y1..%d\n",
				world->genMinX, world->genMaxX, world->genMinZ, world->genMaxZ, h - 1);
			Aurora::Utils::Logger::Instance()->LogMessage(
				"[netherprobe] air=%d netherrack=%d lava=%d glowstone=%d soulsand=%d other=%d\n",
				air, netherrackCount, lava, glowstone, soulsand, other);
			for (int b = 0; b < 8; b++)
				Aurora::Utils::Logger::Instance()->LogMessage("[netherprobe] band y%3d..%3d open=%d%%\n",
					(b * h) / 8, ((b + 1) * h) / 8 - 1, bandTotal[b] ? (bandOpen[b] * 100) / bandTotal[b] : 0);
			Aurora::Utils::Logger::Instance()->LogMessage("[netherprobe] spawn %d %d %d\n", spawnX, spawnY, spawnZ);
			// The pocket the player wakes up in, as a 5x5 map five levels around the spawn: '#' solid,
			// '.' air. A one-wide shaft is the failure mode this exists to catch -- it passes any block
			// count and is unplayable to stand in.
			for (int lvl = spawnY - 1; lvl <= spawnY + 3; lvl++)
			{
				char row[64];
				int p = 0;
				for (int dz = -2; dz <= 2; dz++)
				{
					for (int dx = -2; dx <= 2; dx++)
						row[p++] = (world->GetBlock(spawnX + dx, lvl, spawnZ + dz) == 0) ? '.' : '#';
					row[p++] = ' ';
				}
				row[p] = 0;
				Aurora::Utils::Logger::Instance()->LogMessage("[netherprobe] y=%3d (%+d) %s\n", lvl, lvl - spawnY, row);
			}
		}
#endif
	}
	if (worldType == FlatSnowyTaiga)
	{
		bool snowBiomeIncluded = true;
		bool snowBiomeSuperLarge = true;
		loading->readiness = 1;
		loading->stateName = 1;
		int superflatWorldHeight = 46;
		for (int x = world->genMinX; x < world->genMaxX; x++)
		{
			loading->readiness = ((float)x / WORLD_SIZE) * 50 + 1;
			for (int z = world->genMinZ; z < world->genMaxZ; z++)
			{
				for (int y = 0; y <= superflatWorldHeight; y++)
				{
					if (y == 2)
					{
						world->GetBlock(x, y, z) = bedrock::getID(); // bedrock
					}
					else
					{
						world->GetBlock(x, y, z) = bedrock::getID();
						if (rand() % 384 == 44)
						{
							//world->GetBlock(x,y,z) = 41 + rand() % 5; // stuff for survival mod
						}

						if (y >= superflatWorldHeight - 2)
						{
							world->GetBlock(x, y, z) = DirtBlock::getID(); // Dirt
							if (rand() % 384 == 0)
							{
								//world->GetBlock(x,y,z) = WoodBlock::getID(); // stuff for survival mod
							}
						}
						if (y == superflatWorldHeight)
						{
							world->GetBlock(x, y, z) = SnowSoil::getID();
						}
					}
				}
			}
		}
		loading->stateName = 7;
		loading->stateName = 90;
		world->initWorldBlocksLight();
		world->SetPlayerSpawnPosition(Vector3(WORLD_MID, world->groundHeight(WORLD_MID, WORLD_MID) + world->playerGrowth - 0.05f + 1, WORLD_MID));
	}
	if (worldType == FlatDesertLakes)
	{
		loading->readiness = 1;
		loading->stateName = 1;
		int superflatWorldHeight = 46;
		for (int x = world->genMinX; x < world->genMaxX; x++)
		{
			loading->readiness = ((float)x / WORLD_SIZE) * 50 + 1;
			for (int z = world->genMinZ; z < world->genMaxZ; z++)
			{
				for (int y = 0; y <= superflatWorldHeight; y++)
				{
					if (y == 2)
					{
						world->GetBlock(x, y, z) = bedrock::getID(); // bedrock
					}
					else
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
						if (rand() % 384 == 44)
						{
							//world->GetBlock(x,y,z) = 41 + rand() % 5; // stuff for survival mod
						}

						if (y >= superflatWorldHeight - 2)
						{
							world->GetBlock(x, y, z) = SandStone::getID(); // Dirt
							if (rand() % 384 == 0)
							{
								//world->GetBlock(x,y,z) = WoodBlock::getID(); // stuff for survival mod
							}
						}
						if (y == superflatWorldHeight)
						{
							world->GetBlock(x, y, z) = SandBlock::getID();
						}
					}
				}
			}
		}
		loading->stateName = 7;
		loading->stateName = 90;
		world->initWorldBlocksLight();
		world->SetPlayerSpawnPosition(Vector3(WORLD_MID, world->groundHeight(WORLD_MID, WORLD_MID) + world->playerGrowth - 0.05f + 1, WORLD_MID));
	}
	if (worldType == FlatOcean)
	{
		loading->readiness = 1;
		loading->stateName = 1;
		int superflatWorldHeight = 32;
		for (int x = world->genMinX; x < world->genMaxX; x++)
		{
			loading->readiness = ((float)x / WORLD_SIZE) * 50 + 1;
			for (int z = world->genMinZ; z < world->genMaxZ; z++)
			{
				for (int y = 0; y <= superflatWorldHeight; y++)
				{
					if (y == 2)
					{
						world->GetBlock(x, y, z) = bedrock::getID();
					}
					if (y == 3)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 4)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 5)
					{
						world->GetBlock(x, y, z) = RockBlock::getID(); 
					}
					if (y == 6)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 7)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 8)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 9)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 10)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 11)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 12)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 13)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 14)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 15)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 16)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 17)
					{
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					if (y == 18)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 19)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 20)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 21)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 22)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 23)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 24)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 25)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 26)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 27)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 28)
					{
						world->GetBlock(x, y, z) = DirtBlock::getID();
					}
					if (y == 29)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
					if (y == 30)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
					if (y == 31)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
					if (y == 32)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
					if (y == 33)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
					if (y == 34)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
					if (y == 35)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
					if (y == 36)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
				}
			}
		}
		loading->stateName = 7;
		loading->stateName = 90;
		world->initWorldBlocksLight();
		world->SetPlayerSpawnPosition(Vector3(WORLD_MID, world->groundHeight(WORLD_MID, WORLD_MID) + world->playerGrowth - 0.05f + 1, WORLD_MID));
	}
    loading->KillLoadingScreen();
    delete loading;
    world->MarkAllColumnsGenerated();
}

float WorldGenerator::InterpolateBiomeNoise(int wx, int wz)
{
	float q1, q2, q3, q4, xinterp, yinterp, r1, r2, r3;
	int minx, maxx, minz, maxz;

	minx = floor(wx/8.0f);
	maxx = ceil(wx/8.0f);

	minz = floor(wz/8.0f);
	maxz = ceil(wz/8.0f);

	q1 = (GetValue2D3(minx,minz,1,0.1f,1.0f)+1.0f)/2.0f;
	q2 = (GetValue2D3(maxx,minz,1,0.1f,1.0f)+1.0f)/2.0f;
	q3 = (GetValue2D3(minx,maxz,1,0.1f,1.0f)+1.0f)/2.0f;
	q4 = (GetValue2D3(maxx,maxz,1,0.1f,1.0f)+1.0f)/2.0f;

	xinterp = (wx/8.0f - minx);
	yinterp = (wz/8.0f - minz);

	r1 = func1.Linear(q1,q2,xinterp);
	r2 = func1.Linear(q3,q4,xinterp);

	r3 = func1.Linear(r1,r2,yinterp);

    if(r3 > 1)
    {
        r3 = 1.0f;
    }

    if(r3 < 0)
    {
        r3 = 0.0f;
    }

	return r3;
}

float WorldGenerator::InterpolateMoistureNoise(int wx, int wz)
{
	float q1, q2, q3, q4, xinterp, yinterp, r1, r2, r3;
	int minx, maxx, minz, maxz;

	minx = floor(wx/6.0f);
	maxx = ceil(wx/6.0f);

	minz = floor(wz/6.0f);
	maxz = ceil(wz/6.0f);

    q1 = (GetValue2D2(minx,minz,1,0.1f,1.0f)*2)+0.3f;
	q2 = (GetValue2D2(maxx,minz,1,0.1f,1.0f)*2)+0.3f;
	q3 = (GetValue2D2(minx,maxz,1,0.1f,1.0f)*2)+0.3f;
	q4 = (GetValue2D2(maxx,maxz,1,0.1f,1.0f)*2)+0.3f;


	xinterp = (wx/6.0f - minx);
	yinterp = (wz/6.0f - minz);

	r1 = func1.Linear(q1,q2,xinterp);
	r2 = func1.Linear(q3,q4,xinterp);

	r3 = func1.Linear(r1,r2,yinterp);

    if(r3 > 1)
    {
        r3 = 1.0f;
    }

    if(r3 < 0)
    {
        r3 = 0.0f;
    }

	return r3;
}

void WorldGenerator::PrepareColumnNoise(int x0, int z0, int chunkSize)
{
    noiseGridOn = false;
    if(chunkSize <= 0 || chunkSize > 16)
        return;
    const int x1 = x0 + chunkSize - 1;
    const int z1 = z0 + chunkSize - 1;

    elevX0 = (int)floor(x0 / 4.0f);
    elevZ0 = (int)floor(z0 / 4.0f);
    elevNX = (int)ceil(x1 / 4.0f) - elevX0 + 1;
    elevNZ = (int)ceil(z1 / 4.0f) - elevZ0 + 1;
    roughX0 = (int)floor(x0 / 2.0f);
    roughZ0 = (int)floor(z0 / 2.0f);
    roughNX = (int)ceil(x1 / 2.0f) - roughX0 + 1;
    roughNZ = (int)ceil(z1 / 2.0f) - roughZ0 + 1;
    if(elevNX <= 0 || elevNZ <= 0 || elevNX > 8 || elevNZ > 8)
        return;
    if(roughNX <= 0 || roughNZ <= 0 || roughNX > 12 || roughNZ > 12)
        return;

    for(int iz = 0; iz < elevNZ; iz++)
        for(int ix = 0; ix < elevNX; ix++)
            elevGrid[ix][iz] = GetValue2D(elevX0 + ix, elevZ0 + iz, 1, 0.1f, 1.0f);
    for(int iz = 0; iz < roughNZ; iz++)
        for(int ix = 0; ix < roughNX; ix++)
            roughGrid[ix][iz] = GetValue2D(roughX0 + ix, roughZ0 + iz, 3, 0.1f, 1.0f);
    noiseGridOn = true;
}

float WorldGenerator::SampleElevation(int ix, int iz)
{
    if(noiseGridOn)
    {
        const int dx = ix - elevX0;
        const int dz = iz - elevZ0;
        if((unsigned)dx < (unsigned)elevNX && (unsigned)dz < (unsigned)elevNZ)
            return elevGrid[dx][dz];
    }
    return GetValue2D(ix, iz, 1, 0.1f, 1.0f);
}

float WorldGenerator::SampleRoughness(int ix, int iz)
{
    if(noiseGridOn)
    {
        const int dx = ix - roughX0;
        const int dz = iz - roughZ0;
        if((unsigned)dx < (unsigned)roughNX && (unsigned)dz < (unsigned)roughNZ)
            return roughGrid[dx][dz];
    }
    return GetValue2D(ix, iz, 3, 0.1f, 1.0f);
}

float WorldGenerator::InterpolateElevationNoise(int wx, int wz)
{
	float q1, q2, q3, q4, xinterp, yinterp, r1, r2, r3;
	int minx, maxx, minz, maxz;

	minx = floor(wx/4.0f);
	maxx = ceil(wx/4.0f);

	minz = floor(wz/4.0f);
	maxz = ceil(wz/4.0f);

	q1 = SampleElevation(minx,minz);
	q2 = SampleElevation(maxx,minz);
	q3 = SampleElevation(minx,maxz);
	q4 = SampleElevation(maxx,maxz);

	xinterp = (wx/4.0f - minx);
	yinterp = (wz/4.0f - minz);

	r1 = func1.Linear(q1,q2,xinterp);
	r2 = func1.Linear(q3,q4,xinterp);

	r3 = func1.Linear(r1,r2,yinterp);

	return r3;
}

float WorldGenerator::InterpolateRoughnessNoise(int wx, int wz)
{
	float q1, q2, q3, q4, xinterp, yinterp, r1, r2, r3;
	int minx, maxx, minz, maxz;

	minx = floor(wx/2.0f);
	maxx = ceil(wx/2.0f);

	minz = floor(wz/2.0f);
	maxz = ceil(wz/2.0f);

	q1 = SampleRoughness(minx,minz);
	q2 = SampleRoughness(maxx,minz);
	q3 = SampleRoughness(minx,maxz);
	q4 = SampleRoughness(maxx,maxz);

	xinterp = (wx/2.0f - minx);
	yinterp = (wz/2.0f - minz);

	r1 = func1.Linear(q1,q2,xinterp);
	r2 = func1.Linear(q3,q4,xinterp);

	r3 = func1.Linear(r1,r2,yinterp);

	return r3;
}

int WorldGenerator::DefaultTerrainHeight(int x, int z, int worldHeight)
{
    float HillFrequency = (GetValue2D(x,z,1,0.05f,1)+1.0f)/2.0f;
    int Height = (int)((InterpolateElevationNoise(x,z) + InterpolateRoughnessNoise(x,z)*GetValue2D3(x,z,6,0.1f,1))*64*HillFrequency+62);
    if(Height <= 48)
        Height = 48-(48-Height)/2;
    if(Height >= 68)
        Height = 68+(Height-68)/2;

    // Large scale "continent" noise: pulls wide regions down into seas and lakes.
    float ocean = GetValue2D2(x, z, 1, 0.028f, 1.0f);
    if(ocean < -0.22f)
        Height -= (int)((-0.22f - ocean) * 46.0f);
    // Small scale dips give ponds without needing a separate lake pass.
    float pond = GetValue2D3(x, z, 1, 0.45f, 1.0f);
    if(pond > 0.62f)
        Height -= (int)((pond - 0.62f) * 14.0f);

    if(Height < 4)
        Height = 4;
    if(Height >= worldHeight - 2)
        Height = worldHeight - 3;
    return Height;
}

int WorldGenerator::BiomeAt(int x, int z, int worldType)
{
    if(worldType == SnowyTaiga || worldType == FlatSnowyTaiga)
        return BIOME_SNOW;
    if(worldType == DesertLakes || worldType == FlatDesertLakes)
        return BIOME_DESERT;

    float temp = GetValue2D3(x, z, 1, 0.022f, 1.0f);      // ~700 block wavelength
    float wet  = GetValue2D2(x, z, 1, 0.031f, 1.0f);
    float edge = GetValue2D(x, z, 2, 0.55f, 0.16f);       // jagged biome borders
    temp += edge;
    wet  += edge * 0.5f;

    if(worldType == Forest)
    {
        if(temp < -0.55f)
            return BIOME_SNOW;
        return BIOME_FOREST;
    }

    if(temp < -0.34f)
        return BIOME_SNOW;
    if(temp > 0.36f && wet < 0.12f)
        return BIOME_DESERT;
    if(wet > 0.02f)
        return BIOME_FOREST;
    return BIOME_PLAINS;
}

void WorldGenerator::PlaceDefaultTerrainColumn(CraftWorld *world, int x, int z, int worldType, int waterLevel, int worldHeight)
{
    int Height = DefaultTerrainHeight(x, z, worldHeight);
    int biome = BiomeAt(x, z, worldType);
    PlaceTerrainColumn(world, x, z, Height, biome, waterLevel, worldHeight);
}

void WorldGenerator::PlaceTerrainColumn(CraftWorld *world, int x, int z, int Height, int biome, int waterLevel, int worldHeight)
{
    // patch noise breaks up shorelines / river beds so they are not one material
    float patch = GetValue2D2(x, z, 2, 0.35f, 1.0f);
    bool underWater = Height < waterLevel;
    bool shore = Height >= waterLevel - 1 && Height <= waterLevel + 1;

    world->GetBlock(x, 0, z) = bedrock::getID();
    world->GetBlock(x, 1, z) = bedrock::getID();
    if(rand() % 2 == 0)
        world->GetBlock(x, 2, z) = bedrock::getID();

    for(int y = Height; y >= 2; y--)
    {
        block_t put = RockBlock::getID();
        int depth = Height - y;   // 0 = surface block

        if(biome == BIOME_DESERT)
        {
            if(depth < 4)
                put = SandBlock::getID();
            else if(depth < 8)
                put = SandStone::getID();
            if(underWater && depth == 0 && patch > 0.3f)
                put = ClayBlock::getID();
        }
        else if(depth == 0)
        {
            if(underWater)
            {
                // sea / lake floor
                if(patch > 0.45f)
                    put = ClayBlock::getID();
                else if(patch < -0.35f)
                    put = Gravel::getID();
                else if(Height < waterLevel - 6)
                    put = Gravel::getID();
                else
                    put = SandBlock::getID();
            }
            else if(shore && patch > -0.25f)
                put = SandBlock::getID();
            else if(biome == BIOME_SNOW)
                put = SnowSoil::getID();
            else
                put = GrassBlock::getID();
        }
        else if(depth < 4)
        {
            if((underWater || shore) && patch > -0.25f && depth < 3)
                put = SandBlock::getID();
            else
                put = DirtBlock::getID();
        }

        world->GetBlock(x, y, z) = put;
    }

    if(underWater)
    {
        for(int y = waterLevel; y > Height; y--)
        {
            if(y <= 0 || y >= worldHeight)
                continue;
            world->GetBlock(x, y, z) = WaterBlock::getID();
        }
        if(biome == BIOME_SNOW && waterLevel > 0 && waterLevel < worldHeight)
            world->GetBlock(x, waterLevel, z) = IceBlock::getID();
    }
    else if(biome == BIOME_SNOW && Height + 1 < worldHeight)
    {
        block_t top = world->GetBlock(x, Height, z);
        if(top != IceBlock::getID())
            world->GetBlock(x, Height + 1, z) = Snow2::getID();
    }
}

void WorldGenerator::SetSeed(int seed)
{
	func1.setSeed(seed);
	func2.setSeed(seed+500);
	func3.setSeed(seed+1000);
}

float WorldGenerator::GetValue3D(int wx, int wy, int wz, int octaves, float startFrequency, float startAmplitude)
{
	float DIVISOR = 1.0f / 16.0f;

	float x = wx * DIVISOR;
	float y = wy * DIVISOR;
	float z = wz * DIVISOR;

	float frequency = startFrequency;
	float amplitude = startAmplitude;

	float sum = 0.0f;

	for (int octave = 0; octave < octaves; octave++)
	{
		float noise = func1.sdnoise3(x * frequency, y * frequency, z * frequency,0,0,0);

		sum += amplitude * noise;
		frequency *= 2;
		amplitude *= 0.5;
	}

	return sum;
}

float WorldGenerator::GetValue2D(int wx, int wz, int octaves, float startFrequency, float startAmplitude)
{
	float DIVISOR = 1.0f / 16.0f;

	float x = wx * DIVISOR;
	float z = wz * DIVISOR;

	float frequency = startFrequency;
	float amplitude = startAmplitude;

	float sum = 0.0f;

	for (int octave = 0; octave < octaves; octave++)
	{
		float noise = func1.sdnoise2(x * frequency, z * frequency,0,0);

		sum += amplitude * noise;
		frequency *= 2;
		amplitude *= 0.5;
	}

	return sum;
}

float WorldGenerator::GetValue2D2(int wx, int wz, int octaves, float startFrequency, float startAmplitude)
{
	float DIVISOR = 1.0f / 16.0f;

	float x = wx * DIVISOR;
	float z = wz * DIVISOR;

	float frequency = startFrequency;
	float amplitude = startAmplitude;

	float sum = 0.0f;

	for (int octave = 0; octave < octaves; octave++)
	{
		float noise = func2.sdnoise2(x * frequency, z * frequency,0,0);

		sum += amplitude * noise;
		frequency *= 2;
		amplitude *= 0.5;
	}

	return sum;
}

float WorldGenerator::GetValue2D3(int wx, int wz, int octaves, float startFrequency, float startAmplitude)
{
	float DIVISOR = 1.0f / 16.0f;

	float x = wx * DIVISOR;
	float z = wz * DIVISOR;

	float frequency = startFrequency;
	float amplitude = startAmplitude;

	float sum = 0.0f;

	for (int octave = 0; octave < octaves; octave++)
	{
		float noise = func3.sdnoise2(x * frequency, z * frequency,0,0);

		sum += amplitude * noise;
		frequency *= 2;
		amplitude *= 0.5;
	}

	return sum;
}


void WorldGenerator::initErosion(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    int num = 13 + rand() % 8;

    for(int i = 0; i < num; i++)
    {
        int x = RandGenX(world, 6);
        int z = RandGenZ(world, 6);
        if(NearGenRim(world, x, z, world->CHUNK_SIZE + 4))
            continue;
        int y = world->groundHeight(x,z);

        if(y <= 60)
        {
            continue;
        }

        world->BuildBlockSphere(2+rand()%3,0,x,y+rand()%2,z);

        int xx, yy, zz;
        xx = x;
        yy = y+1;
        zz = z;

        for(int j = 0; j <= 10; j++)
        {
            world->GetBlock(xx,yy,zz) = 0;
            world->GetBlock(xx,yy-2,zz) = 0;
            world->GetBlock(xx,yy-1,zz) = 0;
            world->GetBlock(xx,yy+1,zz) = 0;

            world->GetBlock(xx+1,yy,zz) = 0;
            world->GetBlock(xx+1,yy-2,zz) = 0;
            world->GetBlock(xx+1,yy-1,zz) = 0;
            world->GetBlock(xx+1,yy+1,zz) = 0;

            world->GetBlock(xx-1,yy,zz) = 0;
            world->GetBlock(xx-1,yy-2,zz) = 0;
            world->GetBlock(xx-1,yy-1,zz) = 0;
            world->GetBlock(xx-1,yy+1,zz) = 0;

            world->GetBlock(xx,yy,zz+1) = 0;
            world->GetBlock(xx,yy-2,zz+1) = 0;
            world->GetBlock(xx,yy-1,zz+1) = 0;
            world->GetBlock(xx,yy+1,zz+1) = 0;

            world->GetBlock(xx,yy,zz-1) = 0;
            world->GetBlock(xx,yy-2,zz-1) = 0;
            world->GetBlock(xx,yy-1,zz-1) = 0;
            world->GetBlock(xx,yy+1,zz-1) = 0;



            switch(rand()%2)
            {
                case 0: xx += -1+rand()%3; break;
                case 1: zz += -1+rand()%3; break;
            }
        }

        for(int x2 = x-4; x2 <= x+4; x2++)
        {
            for(int z2 = z-4; z2 <= z+4; z2++)
            {
                for(int y2 = y-5; y2 <= y+5; y2++)
                {
                    if(world->GetBlock(x2,y2,z2) == 0)
                    {
                        if(y2 <= 60)
                        {
                            world->GetBlock(x2,y2,z2) = WaterBlock::getID();
                        }
                    }
                    if(world->GetBlock(x2,y2,z2) == DirtBlock::getID())
                    {
                        if(world->GetBlock(x2,y2+1,z2) == 0)
                        {
                            world->GetBlock(x2,y2,z2) = GrassBlock::getID();
                            if(y2 <= 62)
                            {
                                world->GetBlock(x2,y2,z2) = SandBlock::getID();
                            }
                        }
                        continue;
                    }
                        if(world->GetBlock(x2,y2,z2) == RockBlock::getID())
                        {
                            if(world->GetBlock(x2,y2+1,z2) == 0)
                            {
                                if(y > 62)
                                {
                                    world->GetBlock(x2,y2+1,z2) = GrassBlock::getID();
                                }
                            }
                            continue;
                        }
                }
            }
        }
    }
}

void WorldGenerator::initLavaPools(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    int num = 2 + rand() % 2;

    for(int i = 0; i < num; i++)
    {
        int x = RandGenX(world, 9);
        int z = RandGenZ(world, 9);
        int y = 0;

        while(world->groundHeight(x,z) < 70)
        {
            x = RandGenX(world, 9);
            z = RandGenZ(world, 9);
        }
        y = world->groundHeight(x,z);

        if(rand() % 100 > 40)
        {
            world->BuildBlockSphere(3,0,x,y+2,z);
            world->BuildBlockSphere(3,112,x,y,z,1);
            world->BuildBlockSphere(3,112,x,y,z,2);
            world->BuildBlockSphere(3,112,x,y,z,3);
            world->BuildBlockSphere(3,112,x,y,z,7);
        }
        else
        {
            world->BuildBlockSphere(4,0,x,y+4,z);
            world->BuildBlockSphere(4,112,x,y,z,1);
            world->BuildBlockSphere(4,112,x,y,z,2);
            world->BuildBlockSphere(5,112,x,y,z,3);
            world->BuildBlockSphere(5,112,x,y,z,7);
        }
    }
}

void WorldGenerator::initLavaLakes(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    char lavaLakesNum = 7 + rand() % 4;

    for(int i = 0; i <= lavaLakesNum; i++)
    {
        int x = RandGenX(world, 8);
        int z = RandGenZ(world, 8);
        if(NearGenRim(world, x, z, world->CHUNK_SIZE + 8))
            continue;
        int y = 9 + rand() % 16;

        float radius = 3 + (rand() % 21)/10.0f;
        bool flag1 = true;

        for(int x2 = x - radius; x2 < x + radius; x2++)
        {
            for(int y2 = y - radius; y2 < y + radius; y2++)
            {
                for(int z2 = z - radius; z2 < z + radius; z2++)
                {
                    if(world->GetBlock(x2,y2,z2) == 0)
                    {
                        flag1 = false;
                    }
                }
            }
        }

        if(flag1)
        {
            world->BuildBlockSphere(radius,Lava::getID(),x,y,z);

            for(int x2 = x - radius; x2 < x + radius; x2++)
            {
                for(int y2 = y-1; y2 < y + radius+1; y2++)
                {
                    for(int z2 = z - radius; z2 < z + radius; z2++)
                    {
                        if(world->GetBlock(x2,y2,z2) == Lava::getID())
                        {
                            world->GetBlock(x2,y2,z2) = 0;
                        }
                    }
                }
            }

            for(int x2 = x - radius; x2 < x + radius; x2++)
            {
                for(int y2 = y - radius - 3 ; y2 < y - radius; y2++)
                {
                    for(int z2 = z - radius; z2 < z + radius; z2++)
                    {
                        if(world->GetBlock(x2,y2,z2) == RockBlock::getID())
                        {
                            if(y2 <= 13 && rand() % 100 == 0)
                            {
                                world->GetBlock(x2,y2,z2) = DiamondOre::getID();
                            }
                        }
                    }
                }
            }

            world->UpdateLightAreaIn(Vector3(x,y,z));
        }
        else
        {
            continue;
        }
    }
}

void WorldGenerator::initWaterLakes(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    char waterLakesNum = 10 + rand() % 7;

    for(int i = 0; i <= waterLakesNum; i++)
    {
        int x = RandGenX(world, 5);
        int z = RandGenZ(world, 5);
        if(NearGenRim(world, x, z, world->CHUNK_SIZE + 8))
            continue;
        int y = 14 + rand() % 32;

        float radius = 2.5 + (rand() % 18)/10.0f;
        bool flag1 = true;

        for(int x2 = x - radius; x2 < x + radius; x2++)
        {
            for(int y2 = y - radius; y2 < y + radius; y2++)
            {
                for(int z2 = z - radius; z2 < z + radius; z2++)
                {
                    if(world->GetBlock(x2,y2,z2) == 0)
                    {
                        flag1 = false;
                    }
                }
            }
        }

        if(flag1)
        {
            world->BuildBlockSphere(radius,WaterBlock::getID(),x,y,z);

            for(int x2 = x - radius; x2 < x + radius; x2++)
            {
                for(int y2 = y; y2 < y + radius+1; y2++)
                {
                    for(int z2 = z - radius; z2 < z + radius; z2++)
                    {
                        if(world->GetBlock(x2,y2,z2) == WaterBlock::getID())
                        {
                            world->GetBlock(x2,y2,z2) = 0;
                        }
                    }
                }
            }

            for(int x2 = x - radius; x2 < x + radius; x2++)
            {
                for(int y2 = y - radius - 3 ; y2 < y - radius; y2++)
                {
                    for(int z2 = z - radius; z2 < z + radius; z2++)
                    {
                        if(world->GetBlock(x2,y2,z2) == RockBlock::getID())
                        {
                            if(y2 <= 13 && rand() % 69 == 0)
                            {
                                world->GetBlock(x2,y2,z2) = CoalOre::getID();
                            }
                        }
                    }
                }
            }
        }
        else
        {
            continue;
        }
    }
}

void WorldGenerator::initBiome(int WORLD_SIZE, CraftWorld *world, PerlinModule* perlin, int seed)
{
    WORLD_SIZE = world->worldLimit;
    const int WORLD_MID = (world->genMinX + world->genMaxX) / 2;
    /// ANOMALIES SECTION
    bool snowBiomeIncluded = true;
    bool snowBiomeSuperLarge = false;
    bool desertBiomeIncluded = true;
    bool desertBiomeSuperLarge = false;

    if(rand() % 15 == 0)
    {
        snowBiomeIncluded = false;
    }
    if(rand() % 15 == 0)
    {
        desertBiomeIncluded = false;
    }
    if(rand() % 15 == 0)
    {
        snowBiomeSuperLarge = true;
    }
    if(rand() % 100 == 0)
    {
        desertBiomeSuperLarge = true;
    }

    /// SNOW BIOME
    float angle = (rand() % (629))/100.0f;
    int biomeCenterX = WORLD_MID+sinf(angle)*(126);
    int biomeCenterZ = WORLD_MID+cosf(angle)*(126);
    int biomeRadius = 75+rand()%11+snowBiomeSuperLarge*(25+rand()%140);

    if(snowBiomeIncluded)
    {
        world->snowBiomePosition = Vector3(biomeCenterX,0,biomeCenterZ);
        world->snowBiomeRadius = biomeRadius;

        for (int z = biomeCenterZ-biomeRadius; z <= biomeCenterZ+biomeRadius; z++)
        {
            for (int x = biomeCenterX-biomeRadius; x <= biomeCenterX+biomeRadius; x++)
            {
                if(world->FastDistance2d(abs(x-biomeCenterX),abs(z-biomeCenterZ)) < biomeRadius+5)
                {
                    if(x == biomeCenterX-biomeRadius || x == biomeCenterX+biomeRadius || z == biomeCenterZ-biomeRadius || z == biomeCenterZ+biomeRadius)
                    {
                        if(rand() % 4 == 1)
                        {
                            continue;
                        }
                    }
                    if(world->GetBlock(x, world->groundHeight(x, z), z) == 0) continue;
                    if(world->GetBlock(x, world->groundHeightExcept(x, z, 9), z) == 1)
                    {
                        world->GetBlock(x, world->groundHeightExcept(x, z, 9), z) = 29; // snow
                    }
                    else if(world->GetBlock(x, world->groundHeightWater(x, z), z) == 4)
                    {
                        world->GetBlock(x, world->groundHeightWater(x, z), z) = 32; // ice
                        continue;
                    }

                    if(world->GetBlock(x, world->groundHeight(x, z), z) != 93 && world->GetBlock(x, world->groundHeight(x, z), z) != 32)
                    {
                        if(world->GetBlock(x, world->groundHeight(x, z)+1, z) == 0)
                        {
                            world->GetBlock(x, world->groundHeight(x, z)+1, z) = 93;
                        }
                    }
                }
            }
        }
    }


    /// DESERT BIOME
    if(desertBiomeIncluded && (!snowBiomeSuperLarge || !snowBiomeIncluded))
    {
        angle += PI + (-20+rand()%41)/100.0f;

        int rad = rand() % 15;

        biomeCenterX = WORLD_MID+sinf(angle)*(126+rad);
        biomeCenterZ = WORLD_MID+cosf(angle)*(126+rad);
        biomeRadius = 75+rand()%11+snowBiomeSuperLarge*(25+rand()%140);

        world->desertBiomePosition = Vector3(biomeCenterX,0,biomeCenterZ);
        world->desertBiomeRadius = biomeRadius;

        for (int z = biomeCenterZ-biomeRadius; z <= biomeCenterZ+biomeRadius; z++)
        {
            for (int x = biomeCenterX-biomeRadius; x <= biomeCenterX+biomeRadius; x++)
            {
                if(world->FastDistance2d(abs(x-biomeCenterX),abs(z-biomeCenterZ)) < biomeRadius+5)
                {
                    if(x == biomeCenterX-biomeRadius || x == biomeCenterX+biomeRadius || z == biomeCenterZ-biomeRadius || z == biomeCenterZ+biomeRadius)
                    {
                        if(rand() % 4 == 1)
                        {
                            continue;
                        }
                    }
                    if(world->GetBlock(x, world->groundHeight(x, z), z) == 0) continue;
                    if(world->GetBlock(x, world->groundHeight(x, z), z) == 1 || world->GetBlock(x, world->groundHeight(x, z), z) == 2)
                    {
                        for(int Y = world->groundHeight(x, z); Y >= world->groundHeight(x, z) - 4; Y--)
                        {
                            if(world->GetBlock(x, Y, z) == 3)
                            {
                                world->GetBlock(x, Y, z) = 26;
                            }
                            if(world->GetBlock(x, Y, z) == 1 || world->GetBlock(x, Y, z) == 2)
                            {
                                world->GetBlock(x, Y, z) = 7;
                                if(world->GetBlock(x, Y+1, z) == 0)
                                {
                                    if(rand() % 275 == 1)
                                    {
                                        int cactusHeight = 2+rand()%2;
                                        for(int YY = Y+1; YY <= Y+cactusHeight; YY ++)
                                        {
                                            if(world->GetBlock(x+1, YY, z) != 0 ||
                                               world->GetBlock(x-1, YY, z) != 0 ||
                                               world->GetBlock(x, YY, z+1) != 0 ||
                                               world->GetBlock(x, YY, z-1) != 0)
                                            {
                                                break;
                                            }
                                            world->GetBlock(x, YY, z) = CactusBlock::getID();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void WorldGenerator::initCanes(int WORLD_SIZE, CraftWorld *world, int waterLevel)
{
    WORLD_SIZE = world->worldLimit;
    for (int z = world->genMinZ + 1; z < world->genMaxZ - 1; z++)
    {
        for (int x = world->genMinX + 1; x < world->genMaxX - 1; x++)
        {
            if (world->GetBlock(x,waterLevel,z) == SandBlock::getID())
            {
                if (world->GetBlock(x-1,waterLevel,z) == WaterBlock::getID() || world->GetBlock(x+1,waterLevel,z) == WaterBlock::getID() || world->GetBlock(x,waterLevel,z-1) == WaterBlock::getID() || world->GetBlock(x,waterLevel,z+1) == WaterBlock::getID())
                {
                    if(rand() % 48 == 0)
                    {
                        int height = 1+rand()%3;
                        for(int y = waterLevel+1; y <= waterLevel+1+height; y++)
                        {
                            world->GetBlock(x,y,z) = CaneBlock::getID();
                        }
                    }
                }
            }
        }
    }
}

void WorldGenerator::initBedrock(int WORLD_SIZE, CraftWorld *world)
{	
	for (int z = world->genMinZ; z < world->genMaxZ; ++z)
    {
        for (int x = world->genMinX; x < world->genMaxX; ++x)
        {
            world->GetBlock(x, 2, z) = bedrock::getID();//last one in set
        }
    }

    for (int z = world->genMinZ; z < world->genMaxZ; ++z)
    {
        for (int x = world->genMinX; x < world->genMaxX; ++x)
        {
            short r = rand() % 2;
            if (r == 1)
            {
                world->GetBlock(x, 3, z) = bedrock::getID();//last one in set
            }
        }
    }

    for (int z = world->genMinZ; z < world->genMaxZ; ++z)
    {
        for (int x = world->genMinX; x < world->genMaxX; ++x)
        {
            short r = rand() % 3;
            if (r == 1)
            {
                world->GetBlock(x, 4, z) = bedrock::getID();//last one in set
            }
        }
    }
}

void WorldGenerator::initBeachSand(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    for (int z = world->genMinZ; z < world->genMaxZ; ++z)
    {
        for (int x = world->genMinX; x < world->genMaxX; ++x)
        {
            if (world->GetBlock(x,62,z) != 0 && world->GetBlock(x,63,z) == 0)
            {
                world->GetBlock(x,62,z) = 7;
                world->GetBlock(x,61,z) = 7;
            }
        }
    }
}

void WorldGenerator::GeneratePileOfLeaves(CraftWorld *world, int x, int y, int z)
{
    for(int x1 = x-2; x1 <= x+2; x1++)
    {
        for(int z1 = z-1; z1 <= z+1; z1++)
        {
            for(int y1 = y-1; y1 <= y+1; y1++)
            {
                if(world->GetBlock(x1,y1,z1) == 0)
                {
                    world->GetBlock(x1,y1,z1) = LeavesBlock::getID();
                }
            }
        }
    }
    for(int x1 = x-1; x1 <= x+1; x1++)
    {
        for(int z1 = z-2; z1 <= z+2; z1++)
        {
            for(int y1 = y-1; y1 <= y+1; y1++)
            {
                if(world->GetBlock(x1,y1,z1) == 0)
                {
                    world->GetBlock(x1,y1,z1) = LeavesBlock::getID();
                }
            }
        }
    }
    for(int x1 = x-1; x1 <= x+1; x1++)
    {
        if(world->GetBlock(x1,y+2,z) == 0)
        {
            world->GetBlock(x1,y+2,z) = LeavesBlock::getID();
        }
        if(world->GetBlock(x1,y-2,z) == 0)
        {
            world->GetBlock(x1,y-2,z) = LeavesBlock::getID();
        }
    }
    for(int z1 = z-1; z1 <= z+1; z1++)
    {
        if(world->GetBlock(x,y+2,z1) == 0)
        {
            world->GetBlock(x,y+2,z1) = LeavesBlock::getID();
        }
        if(world->GetBlock(x,y-2,z1) == 0)
        {
            world->GetBlock(x,y-2,z1) = LeavesBlock::getID();
        }
    }
}

void WorldGenerator::GenerateClassicTree(int WORLD_SIZE, CraftWorld *world, int x, int y, int z, int trunkBlock, int leavesBlock)
{
    WORLD_SIZE = world->worldLimit;
    int trunkHeight = 3 + rand() % 3;
    if(trunkBlock == WhiteWoodBlock::getID()) // if it is oak
    {
        trunkHeight = 4 + rand() % 2;
    }
    bool flag = false;

    if(world->GetBlock(x, y, z) == GrassBlock::getID())
    {
        flag = true;
    }

    for(int x1 = x-1; x1 <= x+1; x1++)
    {
        for(int z1 = z-1; z1 <= z+1; z1++)
        {
            for (int y1 = y+1; y1 < y + 1 + trunkHeight && y1 < world->WORLD_HEIGHT; ++y1)
            {
                if(world->GetBlock(x1, y1, z1) != 0 && world->GetBlock(x1, y1, z1) != 93)
                {
                    flag = false;
                    break;
                }
            }
        }
    }

    if(flag == false) return;

    if(y + 2 + trunkHeight >= world->WORLD_HEIGHT-1)
    {
        return;
    }

    // gen tree trunk
    for (int y1 = y+1; y1 < y + 2 + trunkHeight && y1 < world->WORLD_HEIGHT; ++y1)
    {
        block_t& Block = world->GetBlock(x, y1, z);
        Block = trunkBlock;
    }

    bool mooshroomTree = (rand() % 12 == 0);
    if(trunkBlock == WoodBlock::getID() && mooshroomTree)
    {
        for(int xx = x-2; xx <= x+2; xx++)
        {
            for(int zz = z-2; zz <= z+2; zz++)
            {
                if(world->GetBlock(xx,world->groundHeight(xx,zz),zz) == GrassBlock::getID())
                {
                    if(rand() % 5 == 0)
                    {
                        world->GetBlock(xx,world->groundHeight(xx,zz)+1,zz) = Mooshroom1::getID() + rand()%2;
                    }
                }
            }
        }
    }
    // remove grass under trunk
    world->GetBlock(x, y, z) = DirtBlock::getID();

    bool baloonTree = false;
    if(trunkBlock == WoodBlock::getID())
    {
        if(trunkHeight >= 4)
        {
            if(rand() % 12 == 0)
            {
                baloonTree = true;
            }
        }
    }


    //create my leaves
    if(!baloonTree)
    {
        for(int yy = 0; yy < 2; yy++)
        {
            for(int xx = 0; xx < 5; xx++)
            {
                for(int zz = 0; zz < 3; zz++)
                {
                    int x1 = xx + x - 2;
                    int y1 = yy + y + trunkHeight - 1;
                    int z1 = zz + z - 1;

                    if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                    if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                    block_t& Block = world->GetBlock(x1, y1, z1);
                    if (Block == 0) Block = leavesBlock;
                }
            }
        }

        for(int yy = 0; yy < 2; yy++)
        {
            for(int xx = 0; xx < 3; xx++)
            {
                    int x1 = xx + x - 1;
                    int y1 = yy + y + trunkHeight +1;
                    int z1 = z;

                    if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                    if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                    block_t& Block = world->GetBlock(x1, y1, z1);
                    if (Block == 0) Block = leavesBlock;
            }
        }

        for(int xx = 0; xx < 3; xx++)
        {
            for(int zz = 0; zz < 3; zz++)
            {
                int x1 = xx + x - 1;
                int y1 = y + trunkHeight + 1;
                int z1 = zz + z - 1;
                int rand1 = rand() % 10;

                if (rand1 > 4)
                {
                    if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                    if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                    block_t& Block = world->GetBlock(x1, y1, z1);
                    if (Block == 0) Block = leavesBlock;
                }
            }
        }

        for(int yy = 0; yy < 2; yy++)
        {
            for(int zz = 0; zz < 3; zz++)
            {

                    int x1 = x;
                    int y1 = yy + y + trunkHeight + 1;
                    int z1 = zz + z - 1;

                    if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                    if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                    block_t& Block = world->GetBlock(x1, y1, z1);
                    if (Block == 0) Block = leavesBlock;
            }
        }

        for(int yy = 0; yy < 2; yy++)
        {
            for(int xx = 0; xx < 3; xx++)
            {
                for(int zz = 0; zz < 5; zz++)
                {
                    int x1 = xx + x - 1;
                    int y1 = yy + y + trunkHeight - 1;
                    int z1 = zz + z - 2;

                    if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                    if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                    block_t& Block = world->GetBlock(x1, y1, z1);
                    if (Block == 0) Block = leavesBlock;
                }
            }
        }

        for(int yy = 0; yy < 2; yy++)
        {
            int x1 = x - 2;
            int y1 = yy + y + trunkHeight - 1;
            int z1 = z - 2;
            int rand1 = rand() % 3;

            if (rand1 != 1)
            {
                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = leavesBlock;
            }
        }

        for(int yy = 0; yy < 2; yy++)
        {
            int x1 = x + 2;
            int y1 = yy + y + trunkHeight - 1;
            int z1 = z - 2;
            int rand1 = rand() % 3;

            if (rand1 != 1)
            {
                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = leavesBlock;
            }
        }

        for(int yy = 0; yy < 2; yy++)
        {
            int x1 = x - 2;
            int y1 = yy + y + trunkHeight - 1;
            int z1 = z + 2;
            int rand1 = rand() % 2;

            if (rand1 != 1)
            {
                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = leavesBlock;
            }
        }

        for(int yy = 0; yy < 2; yy++)
        {
            int x1 = x + 2;
            int y1 = yy + y + trunkHeight - 1;
            int z1 = z + 2;
            int rand1 = rand() % 2;

            if (rand1 != 1)
            {
                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = leavesBlock;
            }
        }
    }
    else
    {
        GeneratePileOfLeaves(world,x,y+trunkHeight,z);
    }
}

void WorldGenerator::GenerateSpruceTree(int WORLD_SIZE, CraftWorld *world, int x, int y, int z)
{
    WORLD_SIZE = world->worldLimit;
    int trunkBlock = DarkWoodBlock::getID();

    int trunkHeight = 6+rand()%2;
    int type = 0;

    //check if there is an water on there or another tree
    bool flag = true;

    for (int y1 = y+1; y1 <= y + trunkHeight ; y1++)
    {
        if(world->GetBlock(x, y1, z) != 0)
        {
            flag = false;
            break;
        }
    }
    if (flag == false) return;;

    if(rand() % 7 == 1)
    {
        type = 1;
        trunkHeight = 6 + rand() % 3;
    }
    else
    {
        type = 0;
    }

    //Create the tree trunk
    for (int y1 = y; y1 <= y + trunkHeight ; y1++)
    {
        block_t& Block = world->GetBlock(x, y1, z);
        Block = trunkBlock;
    }
    // remove grass under trunk
    world->GetBlock(x, y-1, z) = DirtBlock::getID();

    if(type == 0)
    {
        for(int xx = -1; xx <= 1; xx++)
        {
            for(int zz = -2; zz <= 2; zz++)
            {
                int x1 = xx + x;
                int y1 = y + 1 + (trunkHeight-6);
                int z1 = zz + z;

                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = 37;
            }
        }

        for(int xx = -2; xx <= 2; xx++)
        {
            for(int zz = -1; zz <= 1; zz++)
            {
                int x1 = xx + x;
                int y1 = y + 1 + (trunkHeight-6);
                int z1 = zz + z;

                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = 37;
            }
        }

        for(int xx = -1; xx <= 1; xx++)
        {
            for(int zz = -2; zz <= 2; zz++)
            {
                int x1 = xx + x;
                int y1 = y + 3 + (trunkHeight-6);
                int z1 = zz + z;

                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = 37;
            }
        }

        for(int xx = -2; xx <= 2; xx++)
        {
            for(int zz = -1; zz <= 1; zz++)
            {
                int x1 = xx + x;
                int y1 = y + 3 + (trunkHeight-6);
                int z1 = zz + z;

                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = 37;
            }
        }

        for(int xx = -1; xx <= 1; xx++)
        {
                int x1 = xx + x;
                int y1 = y + 2 + (trunkHeight-6);
                int z1 = z;

                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = 37;
        }


        for(int zz = -1; zz <= 1; zz++)
        {
            int x1 = x;
            int y1 = y + 2 + (trunkHeight-6);
            int z1 = zz + z;

            if (x1 < 0 || y1 < 0 || z1 < 0) continue;
            if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

            block_t& Block = world->GetBlock(x1, y1, z1);
            if (Block == 0) Block = 37;
        }

        for(int xx = -1; xx <= 1; xx++)
        {
                int x1 = xx + x;
                int y1 = y + 4 + (trunkHeight-6);
                int z1 = z;

                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = 37;
        }


        for(int zz = -1; zz <= 1; zz++)
        {
            int x1 = x;
            int y1 = y + 4 + (trunkHeight-6);
            int z1 = zz + z;

            if (x1 < 0 || y1 < 0 || z1 < 0) continue;
            if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

            block_t& Block = world->GetBlock(x1, y1, z1);
            if (Block == 0) Block = 37;
        }

        for(int xx = -1; xx <= 1; xx++)
        {
                int x1 = xx + x;
                int y1 = y + 6 + (trunkHeight-6);
                int z1 = z;

                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = 37;
        }

        for(int zz = -1; zz <= 1; zz++)
        {
            int x1 = x;
            int y1 = y + 6 + (trunkHeight-6);
            int z1 = zz + z;

            if (x1 < 0 || y1 < 0 || z1 < 0) continue;
            if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

            block_t& Block = world->GetBlock(x1, y1, z1);
            if (Block == 0) Block = 37;
        }

        block_t& Block = world->GetBlock(x, y+7+(trunkHeight-6), z);
        if (Block == 0) Block = 37;
    }
    else // type == 1
    {
        block_t& Block = world->GetBlock(x, y+trunkHeight+1, z);
        if (Block == 0) Block = 37;

        for(int xx = -1; xx <= 1; xx++)
        {
                int x1 = xx + x;
                int y1 = y + trunkHeight;
                int z1 = z;

                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = 37;
        }

        for(int zz = -1; zz <= 1; zz++)
        {
            int x1 = x;
            int y1 = y + trunkHeight;
            int z1 = zz + z;

            if (x1 < 0 || y1 < 0 || z1 < 0) continue;
            if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

            block_t& Block = world->GetBlock(x1, y1, z1);
            if (Block == 0) Block = 37;
        }

        for(int xx = -1; xx <= 1; xx++)
        {
                int x1 = xx + x;
                int y1 = y + trunkHeight-1;
                int z1 = z;

                if (x1 < 0 || y1 < 0 || z1 < 0) continue;
                if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

                block_t& Block = world->GetBlock(x1, y1, z1);
                if (Block == 0) Block = 37;
        }

        for(int zz = -1; zz <= 1; zz++)
        {
            int x1 = x;
            int y1 = y + trunkHeight-1;
            int z1 = zz + z;

            if (x1 < 0 || y1 < 0 || z1 < 0) continue;
            if (x1 >= WORLD_SIZE || y1 >= world->WORLD_HEIGHT || z1 >= WORLD_SIZE) continue;

            block_t& Block = world->GetBlock(x1, y1, z1);
            if (Block == 0) Block = 37;
        }
    }
}


void WorldGenerator::initRandomTrees(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    int treesNum = ScaleGenCount(world, 256)+rand()%13;


    for (int i = 0; i < treesNum; i ++)
    {
        int x = RandGenX(world, 3);
        int z = RandGenZ(world, 3);
        int y = world->groundHeight(x,z);

        if(y >= 85)
        {
            continue;
        }

        bool snowTree = false;
        if(world->GetBlock(x,y,z) == Snow2::getID() && (world->GetBlock(x,y-1,z) == SnowSoil::getID() || world->GetBlock(x,y-1,z) == GrassBlock::getID() || world->GetBlock(x,y-1,z) == DirtBlock::getID()))
        {
            snowTree = true;
        }
        else
        {
            snowTree = false;
        }

        if(!snowTree)
        {
            if(rand() % 100 > 75)
            {
                GenerateClassicTree(WORLD_SIZE,world,x,y,z,WhiteWoodBlock::getID(),BirchLeaves::getID());
            }
            else
            {
                GenerateClassicTree(WORLD_SIZE,world,x,y,z,WoodBlock::getID(),LeavesBlock::getID());
            }
        }
        else
        {
            if(rand() % 3 == 1)
            {
                GenerateSpruceTree(WORLD_SIZE,world,x,y,z);
            }
        }
    }
}

void WorldGenerator::initTrees(int WORLD_SIZE, CraftWorld *world, PerlinModule* perlin, int seed)
{
    // only the spawn region is generated up front; a worldLimit² buffer would be 64 MB
    WORLD_SIZE = world->genMaxX - world->genMinX;
    if(WORLD_SIZE <= 0)
        return;
    float *dataTrees = new float[WORLD_SIZE * WORLD_SIZE];
    perlin->setSeed(seed+10);
    perlin->setOctaveCount(8);
    perlin->setPersistence(0.17f);
    perlin->setFrequency(0.8f);
    perlin->setScale(4.5f);
    perlin->setLacunarity(3.5f);

    noisepp::utils::PlaneBuilder2D builderTrees;
    builderTrees.setModule(perlin);
    builderTrees.setSize(WORLD_SIZE, WORLD_SIZE);
    builderTrees.setBounds(0, 0, 3, 5.5f);
    builderTrees.setDestination(dataTrees);
    builderTrees.build ();

    float perlinHeight;
    block_t woodBlock;
    block_t leavesBlock;
    bool snowTree = false;

    int treesNum = 1700;

    for (int i = 0; i < treesNum; i ++)
    {
        int x = RandGenX(world, 5);
        int z = RandGenZ(world, 5);
        int y = world->groundHeight(x,z);

        if(y >= 85)
        {
            continue;
        }

        perlinHeight = dataTrees[(x - world->genMinX) + (z - world->genMinZ)*WORLD_SIZE]* 128.0f/12.0f + 69;
        if(perlinHeight >=  72)
        {
            bool snowTree = false;
            if(world->GetBlock(x,y,z) == Snow2::getID() && (world->GetBlock(x,y-1,z) == SnowSoil::getID() || world->GetBlock(x,y-1,z) == GrassBlock::getID() || world->GetBlock(x,y-1,z) == DirtBlock::getID()))
            {
                snowTree = true;
            }
            else
            {
                snowTree = false;
            }

            if(!snowTree)
            {
                if(rand() % 100 > 80)
                {
                    GenerateClassicTree(WORLD_SIZE,world,x,y,z,WhiteWoodBlock::getID(),BirchLeaves::getID());
                }
                else
                {
                    GenerateClassicTree(WORLD_SIZE,world,x,y,z,WoodBlock::getID(),LeavesBlock::getID());
                }
            }
            else
            {
                if(rand() % 3 == 1)
                {
                    GenerateSpruceTree(WORLD_SIZE,world,x,y,z);
                }
            }
        }
        else
        {
            if(world->GetBlock(x,y,z) == GrassBlock::getID())
            {
                world->GetBlock(x,y+1,z) = TallGrass::getID();
            }
        }
    }

    delete[] dataTrees;
}

void WorldGenerator::initPumpkins(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    int NumPumpkinsSpawns = 12 + rand() % 4;
    int Frequency = 4; // 1 - 100

    for (int i = 0; i < NumPumpkinsSpawns; ++i)
    {
        //Choose the tpumpkin position
        int x = RandGenX(world, 0);
        int z = RandGenZ(world, 0);
        //check if there is an water on there or another tree
        int rad = 0;

        for(int x2 = x-3; x2 < x+3; ++x2)
        {
            for(int z2 = z-3; z2 < z+3; ++z2)
            {
                if (x2 > 0 && x2 < WORLD_SIZE && z2 > 0 && z2 < WORLD_SIZE)
                {

                    rad = rand() % 100;

                    if (rad > Frequency)
                        continue;

                    if(world->GetBlock(x2, world->groundHeight(x2, z2), z2 ) == GrassBlock::getID() || world->GetBlock(x2, world->groundHeight(x2, z2), z2 ) == DirtBlock::getID() || world->GetBlock(x2, world->groundHeight(x2, z2), z2 ) == SnowSoil::getID())
                    {
                        if(world->GetBlock(x2, world->groundHeight(x2, z2)+1, z2 ) == 0)
                        {
                            int rad2 = rand() % 4;

                            world->GetBlock(x2, world->groundHeight(x2, z2), z2) = DirtBlock::getID();
                            world->GetBlock(x2, world->groundHeight(x2, z2)+1, z2) = 73+rad2;
                        }
                    }
                }
            }
        }
    }
}

void WorldGenerator::initFlowers(int WORLD_SIZE, CraftWorld *world) //generate flowers
{
    WORLD_SIZE = world->worldLimit;
    int num = 32 + rand() % 10;

    for (int i = 0; i < num; ++i)
    {
        int centerX = RandGenX(world, 10);
        int centerZ = RandGenZ(world, 10);

        int size = 2 + rand() % 3;
        int typeProcent = 40 + rand() % 20;
		for(int x = centerX-size; x <= centerX+size; x++)
        {
            for(int z = centerZ-size; z <= centerZ+size; z++)
            {
                int y = world->groundHeight(x, z);
                if(world->GetBlock(x,y,z) == 1)
                {
                    if(x == centerX-size || x == centerX+size || z == centerZ-size || z == centerZ+size) // check for corner flower
                    {
                        if(rand() % 8 == 1) // 1/8
                        {
                            if(rand() % 100 > typeProcent)
                            {
								world->GetBlock(x,y+1,z) = TallGrass::getID();
                            }
                        }
                    }
                    else
                    {
                        if(rand() % 5 == 1) // 1/5
                        {
                            if(rand() % 100 > typeProcent)
                            {
                                world->GetBlock(x,y+1,z) = TallGrass::getID();
                            }
                        }
                    }
                }
            }
        }
        for(int x = centerX-size; x <= centerX+size; x++)
        {
            for(int z = centerZ-size; z <= centerZ+size; z++)
            {
                int y = world->groundHeight(x, z);
                if(world->GetBlock(x,y,z) == 1)
                {
                    if(x == centerX-size || x == centerX+size || z == centerZ-size || z == centerZ+size) // check for corner flower
                    {
                        if(rand() % 8 == 1) // 1/8
                        {
                            if(rand() % 100 > typeProcent)
                            {
                                world->GetBlock(x,y+1,z) = Flower1::getID();
                            }
                            else
                            {
                                world->GetBlock(x,y+1,z) = Flower2::getID();
                            }
                        }
                    }
                    else
                    {
                        if(rand() % 5 == 1) // 1/5
                        {
                            if(rand() % 100 > typeProcent)
                            {
                                world->GetBlock(x,y+1,z) = Flower1::getID();
                            }
                            else
                            {
                                world->GetBlock(x,y+1,z) = Flower2::getID();
                            }
                        }
                    }
                }
            }
        }
    }
}

void WorldGenerator::initGrass(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;

    int NumTypes = 600;

    for (int i = 0; i < NumTypes; ++i)
    {
        //Choose the tpumpkin position
        int x2 = RandGenX(world, 0);
        int z2 = RandGenZ(world, 0);


        if(world->GetBlock(x2, world->groundHeight(x2, z2)+1, z2 ) == 0 && world->GetBlock(x2, world->groundHeight(x2, z2), z2 ) == 1)
        {
            world->GetBlock(x2, world->groundHeight(x2, z2)+1, z2 ) = 116;
        }
    }
}

void WorldGenerator::initOtherVegetation(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;

    int NumTypes = 100 + rand() % 100;

    for (int i = 0; i < NumTypes; ++i)
    {
        //Choose the tpumpkin position
        int x2 = RandGenX(world, 0);
        int z2 = RandGenZ(world, 0);


        if(world->GetBlock(x2, world->groundHeight(x2, z2)+1, z2 ) == 0)
        {
            if(world->GetBlock(x2, world->groundHeight(x2, z2), z2 ) == 1)
            {
                world->GetBlock(x2, world->groundHeight(x2, z2)+1, z2 ) = 117;
            }
        }
    }
}

int WorldGenerator::GenerateOreVine(int WORLD_SIZE, CraftWorld *world, int x, int y, int z, int oreID, int type)
{
    WORLD_SIZE = world->worldLimit;
    // type = {0 - sm, 1 - med, 2 - big}
    int orePlaced = 0;
    int m = rand() % 3;
    switch(type)
    {
        // small
        case 0:

        if(m == 0)
        {
            for(int z2 = z; z2 <= z+1; z2++)
            {
                if(world->GetBlock(x,y,z2) == RockBlock::getID())
                {
                    world->GetBlock(x,y,z2) = oreID;
                    orePlaced ++;
                }
            }
        }
        else if(m == 1)
        {
            for(int x2 = x; x2 <= x+1; x2++)
            {
                if(world->GetBlock(x2,y,z) == RockBlock::getID())
                {
                    world->GetBlock(x2,y,z) = oreID;
                    orePlaced ++;
                }
            }
        }
        else
        {
            for(int y2 = y; y2 <= y+1; y2++)
            {
                if(world->GetBlock(x,y2,z) == RockBlock::getID())
                {
                    world->GetBlock(x,y2,z) = oreID;
                    orePlaced ++;
                }
            }
        }

        break;
        // mid-size
        case 1:


        if(m == 0)
        {
            for(int y2 = y; y2 <= y+1; y2++)
            {
                for(int z2 = z; z2 <= z+1; z2++)
                {
                    if(world->GetBlock(x,y2,z2) == RockBlock::getID())
                    {
                        world->GetBlock(x,y2,z2) = oreID;
                        orePlaced ++;
                    }
                }
            }
        }
        else if(m == 1)
        {
            for(int x2 = x; x2 <= x+1; x2++)
            {
                for(int z2 = z; z2 <= z+1; z2++)
                {
                    if(world->GetBlock(x2,y,z2) == RockBlock::getID())
                    {
                        world->GetBlock(x2,y,z2) = oreID;
                        orePlaced ++;
                    }
                }
            }
        }
        else
        {
            for(int x2 = x; x2 <= x+1; x2++)
            {
                for(int y2 = y; y2 <= y+1; y2++)
                {
                    if(world->GetBlock(x2,y2,z) == RockBlock::getID())
                    {
                        world->GetBlock(x2,y2,z) = oreID;
                        orePlaced ++;
                    }
                }
            }
        }

        break;
        // large
        case 2:

        for(int x2 = x; x2 <= x+1; x2++)
        {
            for(int y2 = y; y2 <= y+1; y2++)
            {
                for(int z2 = z; z2 <= z+1; z2++)
                {
                    if(world->GetBlock(x2,y2,z2) == RockBlock::getID())
                    {
                        world->GetBlock(x2,y2,z2) = oreID;
                        orePlaced ++;
                    }
                }
            }
        }

        break;
    }

    return orePlaced;
}

void WorldGenerator::initOre(int WORLD_SIZE, CraftWorld *world, int oreID, float oreConcentration, int oreLowestBoundary, int oreHighestBoundary, int variations) // brief and sweet function
{
    WORLD_SIZE = world->worldLimit;
    // oreConcentration - var between 0 and 100

    oreConcentration = oreConcentration / 100.0f; // percents
    int gw = world->genMaxX - world->genMinX;
    int gd = world->genMaxZ - world->genMinZ;
    if(gw < 4) gw = 4;
    if(gd < 4) gd = 4;
    int oreAmount = oreConcentration*(gw*gd*(oreHighestBoundary-oreLowestBoundary));

    switch(oreID)
    {
        case 41: world->mainWorldVariables.worldIronOre = oreAmount;
        case 42: world->mainWorldVariables.worldCoalOre = oreAmount;
        case 43: world->mainWorldVariables.worldDiamondOre = oreAmount;
        case 44: world->mainWorldVariables.worldRedstoneOre = oreAmount;
        case 45: world->mainWorldVariables.worldRedstoneOre = oreAmount;
        case 5: world->mainWorldVariables.worldGoldenOre = oreAmount;
    }

    int attempts = 0;
    int maxAttempts = oreAmount * 4 + 32;
    while(oreAmount > 0 && attempts < maxAttempts)
    {
        attempts++;
        int x = RandGenX(world, 1);
        int y = oreLowestBoundary+rand()%(oreHighestBoundary-oreLowestBoundary+1);
        int z = RandGenZ(world, 1);

        int placed = GenerateOreVine(WORLD_SIZE,world,x,y,z,oreID,rand()%variations);
        if(placed > 0)
            oreAmount -= placed;
        else
            oreAmount--;
    }
}

void WorldGenerator::initClay(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    int NumOres = 40 + rand() % 20;

    for (int i = 0; i < NumOres; ++i)
    {
        //Choose the tpumpkin positionh
        int x = RandGenX(world, 0);
        int z = RandGenZ(world, 0);
        int y = world->groundHeight(x,z);

        if(y >= 63 || y < 0)
        {
            continue;
        }
        float radius = 1.5+(rand()%12)/10.0f;

        world->BuildBlockSphere(radius,ClayBlock::getID(),x,y+1,z,SandBlock::getID());
    }
}



void WorldGenerator::initDirt(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    int NumBlocks = ScaleGenCount(world, 1500);
    float radius = 2.0f + (rand() % 35)/10.0f;

    short PlaceBlock = DirtBlock::getID();

    for (int i = 0; i < NumBlocks; ++i)
    {
        int x = RandGenX(world, 0);
        int z = RandGenZ(world, 0);
        int gh = world->groundHeight(x,z);
        if(gh < 6)
            continue;
        int y = 4 + rand() % (gh-1);

        world->BuildBlockSphere(radius, PlaceBlock, x, y, z, RockBlock::getID());

    }
}

void WorldGenerator::initGravel(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    int NumBlocks = ScaleGenCount(world, 400);
    float radius = 2.0f + (rand() % 30)/10.0f;

    short PlaceBlock = RockBlock::getID();

    for (int i = 0; i < NumBlocks; ++i)
    {
        int x = RandGenX(world, 0);
        int z = RandGenZ(world, 0);
        int gh = world->groundHeight(x,z);
        if(gh < 6)
            continue;
        int y = 4 + rand() % (gh-1);

        world->BuildBlockSphere(radius, PlaceBlock, x, y, z, RockBlock::getID());
    }
}


void WorldGenerator::initDungeons(int WORLD_SIZE, CraftWorld *world)
{
    WORLD_SIZE = world->worldLimit;
    int xSize, ySize, zSize;
    int x, y, z;

    ySize = 5;
    int dungeonTries = 0;

    for(int t = 0; t <= 4 && dungeonTries < 24; t++)
    {
        xSize = 5+rand()%3;
        zSize = 5+rand()%3;

        x = RandGenX(world, 1);
        z = RandGenZ(world, 1);

        y = 42-(t*12) + rand() % 12;
        if(t == 4)
        {
            y = 42-(3*12) + rand() % 12;
        }

        if(t == 0)
        {
            world->spawner_x = x;
            world->spawner_z = z;
        }

        bool no = false;

        for(int xx = x; xx <= x+xSize; xx++)
        {
            for(int yy = y; yy <= y+ySize; yy++)
            {
                for(int zz = z; zz <= z+zSize; zz++)
                {
                    if(world->GetBlock(xx,yy,zz) == 0)
                    {
                        no = true;
                    }
                    if(rand() % 100 > 80)
                    {
                        world->GetBlock(xx,yy,zz) = CobbleStone::getID();
                    }
                    else
                    {
                        world->GetBlock(xx,yy,zz) = MossyCobblestone::getID();
                    }
                }
            }
        }

        if(no)
        {
            dungeonTries++;
            t--;
            continue;
        }

        for(int xx = x+1; xx <= x+xSize-1; xx++)
        {
            for(int yy = y+1; yy <= y+ySize-1; yy++)
            {
                for(int zz = z+1; zz <= z+zSize-1; zz++)
                {
                    world->GetBlock(xx,yy,zz) = 0;
                }
            }
        }

        MonsterSpawnerEntity* NewObject = new MonsterSpawnerEntity(x+xSize/2,y+1,z+zSize/2,"zombie");
        world->GetBlock(x+xSize/2,y+1,z+zSize/2) = Spawner::getID();

        world->mMonsterSpawners.push_back(NewObject);

        for(int j = 0; j <= 1; j++)
        {
            Chest *NewChest;
            if(j == 0)
            {
                world->GetBlock(x+1,y+1,z+1) = 97;
                NewChest = new Chest(x+1,y+1,z+1);
            }
            else
            {
                world->GetBlock(x+xSize-1,y+1,z+zSize-1) = 94;
                NewChest = new Chest(x+xSize-1,y+1,z+zSize-1);
            }

            for(int i = 0; i <= 7; i++)
            {
                int slot = rand() % 27;
                int item = -1;
                int am = 0;
                bool st = true;

                int rand1 = rand() % 100;

                if(rand1 < 80)
                {
                    switch(rand() % 7)
                    {
                    case 0:
                        item = 284;
                        am = 1 + rand() % 2;
                    break;
                    case 1:
                        item = 286;
                        am = 2 + rand() % 3;
                    break;
                    case 2:
                        item = 288;
                        am = 2;
                    break;
                    case 3:
                        item = 322;
                        am = 5+rand()%5;
                    break;
                    case 4:
                        item = 311;
                        am = 1+rand()%4;
                    break;
                    case 5:
                        item = 314;
                        am = 6+rand()%8;
                    break;
                    case 6:
                        item = InkSac::getID();
                        am = 12+rand()%12;
                    break;
                    }
                }
                else if(rand1 >= 80 && rand1 < 95)
                {
                    switch(rand() % 4)
                    {
                    case 0:
                        item = 315;
                        am = 8;
                    break;
                    case 1:
                        item = 291;
                        am = 1;
                        st = false;
                    break;
                    case 2:
                        item = 278;
                        am = 2+rand()%4;
                    break;
                    case 3:
                        item = 280;
                        am = 2+rand()%4;
                    break;
                    }
                }
                else
                {
                    switch(rand() % 2)
                    {
                    case 0:
                        item = 285;
                        am = 1;
                    break;
                    case 1:
                        item = 279;
                        am = 1+rand()%2;
                    break;
                    }
                }

                NewChest->chestSlotId[slot] = item;
                NewChest->chestSlotAm[slot] = am;
                NewChest->chestSlotSt[slot] = st;
            }
            NewChest->chestSlotId[26] = 312;
            NewChest->chestSlotAm[26] = 3;
            NewChest->chestSlotSt[26] = true;
            world->mChests.push_back(NewChest);
        }
    }
}

// ============================== the Nether ==============================
//
// The nether is not terrain built up from a surface: it is one solid mass with caverns carved out
// of it. Bedrock floor, netherrack body, bedrock ceiling, and then a single 3D noise field whose
// threshold slides with height, so the middle of the column opens into a connected cavern layer
// while the floor and ceiling stay whole.
//
// The thresholds come from measuring this exact field offline (probe in _tmp_nether). fBm over
// SimplexNoise is centred on zero, so a threshold of t leaves about (50 - 214*t) per cent of a band
// open -- that is how the control points below were placed.
static const int NETHER_LAVA_LEVEL = 31;

static float NetherCarveThreshold(int y)
{
    // Interpolated rather than stepped: a hard step would terrace the carving into flat bands.
    static const float pts[][2] = {
        {   0, 0.62f },   // floor: no carve at all
        {   8, 0.35f },
        {  20, 0.10f },
        {  31, -0.02f },  // the lava sea level, and the most open band
        {  55, -0.02f },
        {  70, 0.10f },
        {  90, 0.28f },
        { 104, 0.62f },
        { 111, 0.95f },   // ceiling: no carve at all again
    };
    const int n = (int)(sizeof(pts) / sizeof(pts[0]));
    if(y <= (int)pts[0][0])
        return pts[0][1];
    if(y >= (int)pts[n - 1][0])
        return pts[n - 1][1];
    for(int i = 0; i + 1 < n; i++)
    {
        if(y <= (int)pts[i + 1][0])
        {
            float f = (y - pts[i][0]) / (pts[i + 1][0] - pts[i][0]);
            return pts[i][1] + f * (pts[i + 1][1] - pts[i][1]);
        }
    }
    return pts[n - 1][1];
}

void WorldGenerator::GenerateNetherColumn(CraftWorld *world, int cx, int cz)
{
    int CHUNK_SIZE = world->CHUNK_SIZE;
    int WORLD_HEIGHT = world->WORLD_HEIGHT;
    int worldLimit = world->worldLimit;
    int x0 = cx * CHUNK_SIZE;
    int z0 = cz * CHUNK_SIZE;
    if(x0 < 0 || z0 < 0 || x0 >= worldLimit || z0 >= worldLimit)
        return;

    const int ceiling = WORLD_HEIGHT - 1;
    const block_t netherrack = Netherrack::getID();
    const block_t bedrockId = bedrock::getID();
    const block_t lava = Lava::getID();

    // ---- 1. the solid mass ----
    for(int lz = 0; lz < CHUNK_SIZE; lz++)
    {
        int z = z0 + lz;
        if(z < 0 || z >= worldLimit)
            continue;
        for(int lx = 0; lx < CHUNK_SIZE; lx++)
        {
            int x = x0 + lx;
            if(x < 0 || x >= worldLimit)
                continue;
            world->GetBlock(x, 0, z) = bedrockId;
            for(int y = 1; y < ceiling; y++)
                world->GetBlock(x, y, z) = netherrack;
            world->GetBlock(x, ceiling, z) = bedrockId;
            // a ragged floor: bedrock climbs a few blocks here and there, so the bottom of the
            // world does not read as a flat sheet
            for(int y = 1; y <= 5; y++)
                if((rand() % 8) < (6 - y))
                    world->GetBlock(x, y, z) = bedrockId;
        }
    }

    // ---- 2. carve the caverns ----
    //
    // The field is sampled every 2 blocks and the blocks between the samples are filled in by
    // interpolation. The previous version thresholded the 2-block samples directly, so every carve
    // boundary landed on the 2-block lattice and the cavern walls came out stepped in 2x2 and 3x3
    // patches -- "chunky", and the reason the nether did not read as clean.
    //
    // Interpolating is nearly free: 81 samples per 2-block band instead of 64, so almost no extra
    // noise, and the interpolation itself is a handful of multiplies per block. Sampling at every
    // block instead would have been four times the noise calls, which on the PSP is generation time
    // paid while the player is waiting to see the world.
    const int kBandSamples = CHUNK_SIZE / 2 + 1;   // 9: the far edge is needed as the basis for the last block
    // Fixed-size rather than CHUNK_SIZE-derived: the bounds are used as array dimensions and as a
    // reference type in the lambdas below, and a variable-length array there is not something the PSP
    // toolchain is comfortable with. 17 covers a CHUNK_SIZE of 32, twice what the game uses.
    const int kBandGridMax = 17;
    float bandNear[kBandGridMax][kBandGridMax];
    float bandFar[kBandGridMax][kBandGridMax];
    float bandMid[kBandGridMax][kBandGridMax];

    auto sampleBand = [&](float (&grid)[kBandGridMax][kBandGridMax], int wy)
    {
        for(int i = 0; i < kBandSamples; i++)
            for(int j = 0; j < kBandSamples; j++)
                grid[i][j] = GetValue3D(x0 + i * 2, wy, z0 + j * 2, 3, 0.45f, 1.0f);
    };

    // Carve the cells of one y layer, reading the field from grid by bilinear interpolation in x and z.
    auto carveLayer = [&](float (&grid)[kBandGridMax][kBandGridMax], int yy)
    {
        if(yy <= 1 || yy >= ceiling)
            return;
        const float threshold = NetherCarveThreshold(yy);
        for(int lz = 0; lz < CHUNK_SIZE; lz++)
        {
            const int z = z0 + lz;
            if(z < 0 || z >= worldLimit)
                continue;
            const int j = lz >> 1;
            const float fz = (lz & 1) ? 0.5f : 0.0f;
            for(int lx = 0; lx < CHUNK_SIZE; lx++)
            {
                const int x = x0 + lx;
                if(x < 0 || x >= worldLimit)
                    continue;
                const int i = lx >> 1;
                const float fx = (lx & 1) ? 0.5f : 0.0f;
                const float near = grid[i][j] * (1.0f - fx) + grid[i + 1][j] * fx;
                const float far  = grid[i][j + 1] * (1.0f - fx) + grid[i + 1][j + 1] * fx;
                if(near * (1.0f - fz) + far * fz <= threshold)
                    continue;
                block_t& b = world->GetBlock(x, yy, z);
                if(b == netherrack)
                    // everything below the lava level is the sea itself
                    b = (yy <= NETHER_LAVA_LEVEL) ? lava : 0;
            }
        }
    };

    sampleBand(bandNear, 2);
    for(int y = 2; y < ceiling - 1; y += 2)
    {
        sampleBand(bandFar, y + 2);
        // bandNear is the field at y and bandFar at y + 2, so y takes bandNear unchanged and y + 1 is
        // halfway between the two. That is the whole of the vertical interpolation.
        for(int i = 0; i < kBandSamples; i++)
            for(int j = 0; j < kBandSamples; j++)
                bandMid[i][j] = 0.5f * (bandNear[i][j] + bandFar[i][j]);

        carveLayer(bandNear, y);
        carveLayer(bandMid, y + 1);

        // This band becomes the near side of the next pair.
        for(int i = 0; i < kBandSamples; i++)
            for(int j = 0; j < kBandSamples; j++)
                bandNear[i][j] = bandFar[i][j];
    }

    // ---- 3. the surfaces the caverns expose ----
    for(int lz = 0; lz < CHUNK_SIZE; lz++)
    {
        int z = z0 + lz;
        if(z < 0 || z >= worldLimit)
            continue;
        for(int lx = 0; lx < CHUNK_SIZE; lx++)
        {
            int x = x0 + lx;
            if(x < 0 || x >= worldLimit)
                continue;
            for(int y = 1; y < ceiling; y++)
            {
                if(world->GetBlock(x, y, z) != netherrack)
                    continue;
                block_t above = world->GetBlock(x, y + 1, z);
                if(above != 0 && above != lava && above != Fire::getID())
                    continue;

                if(y >= NETHER_LAVA_LEVEL - 2 && y <= NETHER_LAVA_LEVEL + 8)
                {
                    // soul sand: shallow floor patches beside the lava, the nether's beach
                    if(GetValue2D3(x, z, 2, 0.25f, 1.0f) > 0.18f && (rand() % 5) < 3)
                    {
                        world->GetBlock(x, y, z) = Soulsand::getID();
                        continue;
                    }
                    if((rand() % 55) == 0)
                    {
                        world->GetBlock(x, y, z) = MagmaBlock::getID();
                        continue;
                    }
                    // fire on the exposed netherrack at the lava's edge. Fire needs a block to sit
                    // on, which is this one; it is placed above, not in place of.
                    if(above == 0 && y <= NETHER_LAVA_LEVEL + 4 && (rand() % 40) == 0)
                        world->GetBlock(x, y + 1, z) = Fire::getID();
                    continue;
                }

                // glowstone: clusters hanging off the underside of the mass, in the upper half of
                // the column, where they light the caverns rather than sitting in the lava
                if(above == 0 && y > 45 && y < ceiling - 3 && (rand() % 190) == 0)
                {
                    int r = 1 + rand() % 2;
                    for(int dx = -r; dx <= r; dx++)
                    {
                        for(int dz = -r; dz <= r; dz++)
                        {
                            for(int dy = -r; dy <= 0; dy++)
                            {
                                if(abs(dx) + abs(dz) + abs(dy) > r + 1)
                                    continue;
                                int gx = x + dx, gy = y + dy, gz = z + dz;
                                if(gy <= 1 || gy >= ceiling)
                                    continue;
                                if(gx < 0 || gz < 0 || gx >= worldLimit || gz >= worldLimit)
                                    continue;
                                if(world->GetBlock(gx, gy, gz) == 0)
                                {
                                    world->GetBlock(gx, gy, gz) = GlowstoneBlock::getID();
                                    // Placed light, not just a placed block: the nether runs no light pass of
                                    // its own (see the note at the end of this function), so a source the
                                    // generation writes is a source that never glows until something seeds
                                    // it. Once per cluster, at its origin, rather than once per cell.
                                    if(gx == x && gy == y && gz == z)
                                        world->SetLigtSourcePosition(gx, gy, gz, GlowstoneBlock::getID());
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- 4. glowstone ----
    //
    // Glowstone is seeded where it is placed, above: it is a genuine light source whose glow spreads into
    // the cavern, and one flood per cluster at the cluster's origin is the shape SetLigtSourcePosition
    // wants.
    //
    // Lava is deliberately NOT seeded. It is a light source too, but LightTravel stops dead at a
    // light-source block once its level drops below 11, so a seed inside the sea lights its own cell and
    // the air above it and can never reach the next lava block along -- which lit isolated cells and left
    // the rest of the surface at the 0.4 ambient, the bright squares the sea was covered in. Lava is
    // drawn at full brightness instead, from the block itself; see BlockFinalLightNoCheck. There is
    // nothing to seed and nothing to keep in step with the light data.
}

// Is the player-sized volume above this floor actually open? A three-block shaft in one column is not
// enough: the player lands in a one-wide hole in the middle of solid netherrack, the camera ends up
// inside the walls, and the view is netherrack seen from inside with the faces culled -- which reads
// as having spawned inside the ground, looking straight through it.
static bool NetherVolumeOpen(CraftWorld *world, int x, int y, int z)
{
    // y is the first air block; the player stands here and their head is one block up, so a 3x3
    // footprint at both levels has to be clear.
    for(int dy = 0; dy < 2; dy++)
        for(int dz = -1; dz <= 1; dz++)
            for(int dx = -1; dx <= 1; dx++)
                if(world->GetBlock(x + dx, y + dy, z + dz) != 0)
                    return false;
    return true;
}

// The nether cannot use groundHeight() for its spawn: that scans down from the top of the world, so
// in a dimension with a bedrock ceiling it returns the ceiling and the player would be placed on
// top of the world. This looks for a netherrack floor with a room above it instead, nearest the
// middle first, and above the lava sea so the player does not land in it.
bool WorldGenerator::FindNetherSpawn(CraftWorld *world, int &outX, int &outY, int &outZ)
{
    const block_t netherrack = Netherrack::getID();
    int worldLimit = world->worldLimit;
    int mid = worldLimit / 2;
    for(int r = 0; r <= 24; r++)
    {
        for(int dz = -r; dz <= r; dz++)
        {
            for(int dx = -r; dx <= r; dx++)
            {
                if(r > 0 && abs(dx) != r && abs(dz) != r)
                    continue;   // ring only, so nearer ground wins
                int x = mid + dx;
                int z = mid + dz;
                if(x < 3 || z < 3 || x >= worldLimit - 3 || z >= worldLimit - 3)
                    continue;
                // Highest floor first: the top of a cavern, so the player is under the ceiling
                // rather than on a ledge under an open shaft.
                for(int y = 70; y >= NETHER_LAVA_LEVEL + 2; y--)
                {
                    if(world->GetBlock(x, y, z) != netherrack)
                        continue;
                    if(!NetherVolumeOpen(world, x, y + 1, z))
                        continue;
                    outX = x;
                    outY = y + 1;
                    outZ = z;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool SliceBudgetSpent(unsigned int startUs, unsigned int budgetUs)
{
    if(budgetUs == 0)
        return false;
    unsigned int now = sceKernelGetSystemTimeLow();
    return (unsigned int)((int)now - (int)startUs) >= budgetUs;
}

void WorldGenerator::GenerateColumn(CraftWorld *world, int cx, int cz)
{
    int step = 0;
    GenerateColumnSlice(world, cx, cz, step, 0, 0);
}

bool WorldGenerator::GenerateColumnSlice(CraftWorld *world, int cx, int cz, int &step, unsigned int startUs, unsigned int budgetUs)
{
    int CHUNK_SIZE = world->CHUNK_SIZE;
    int WORLD_HEIGHT = world->WORLD_HEIGHT;
    int worldType = world->worldType;
    int waterLevel = world->waterLevel;
    if(waterLevel < 8)
        waterLevel = 58;
    int x0 = cx * CHUNK_SIZE;
    int z0 = cz * CHUNK_SIZE;

    // A dimension is generated by what the world *is*, not by the biome option that happened to be
    // picked when it was created, so this is checked before worldType. FlatNETHER routes here too:
    // it used to be a hardcoded slab in the spawn region and a full column when streamed, which is
    // why that world looked like one thing where it spawned and another thing past the spawn ring.
    // The nether column is still one piece. Its carve is a single noise field with state between
    // every y band, and splitting it would be a different generator. The overworld, which is what
    // a sprint through new ground actually pays for, yields between rows.
    if(world->dimension == 1 || worldType == FlatNETHER)
    {
        if(step == 0)
            GenerateNetherColumn(world, cx, cz);
        step = 1;
        return true;
    }

    // One column at a time, and the noise grid is that column's. Cleared when the terrain rows
    // finish so a later height sample cannot read a stale grid. The pin stays for the caves and
    // the trees, which write the same column.
    world->PinLastColumn(world->FindColumn(cx, cz));
    if(step == 0 || (step < CHUNK_SIZE && !noiseGridOn))
        PrepareColumnNoise(x0, z0, CHUNK_SIZE);
    while(step < CHUNK_SIZE)
    {
        int lz = step;
        for(int lx = 0; lx < CHUNK_SIZE; lx++)
        {
            int x = x0 + lx;
            int z = z0 + lz;
            if(x < 0 || z < 0 || x >= world->worldLimit || z >= world->worldLimit)
                continue;

            if(worldType == SUPERFLAT || worldType == FlatDesertLakes || worldType == FlatSnowyTaiga || worldType == FlatOcean)
            {
                world->GetBlock(x, 0, z) = bedrock::getID();
                world->GetBlock(x, 1, z) = DirtBlock::getID();
                world->GetBlock(x, 2, z) = DirtBlock::getID();
                world->GetBlock(x, 3, z) = DirtBlock::getID();
                if(worldType == FlatOcean)
                {
                    world->GetBlock(x, 4, z) = SandBlock::getID();
                    for(int y = 5; y <= waterLevel && y < WORLD_HEIGHT; y++)
                        world->GetBlock(x, y, z) = WaterBlock::getID();
                }
                else
                    world->GetBlock(x, 4, z) = GrassBlock::getID();
                if(lx < 16 && lz < 16)
                    colHeights[lx][lz] = (worldType == FlatOcean) ? waterLevel : 4;
                continue;
            }

            int Height = DefaultTerrainHeight(x, z, WORLD_HEIGHT);
            int biome = BiomeAt(x, z, worldType);
            if(lx < 16 && lz < 16)
            {
                colHeights[lx][lz] = Height;
                colBiomes[lx][lz] = biome;
            }
            PlaceTerrainColumn(world, x, z, Height, biome, waterLevel, WORLD_HEIGHT);

            for(int y = 5; y < Height-4; y++)
            {
                if(world->GetBlock(x, y, z) != RockBlock::getID())
                    continue;
                int r = rand() % 400;
                if(r < 2)
                    world->GetBlock(x, y, z) = CoalOre::getID();
                else if(r == 2 && y < 56)
                    world->GetBlock(x, y, z) = IronOre::getID();
                else if(r == 3 && y < 30)
                    world->GetBlock(x, y, z) = GoldBlock::getID();
                else if(r == 4 && y < 15)
                    world->GetBlock(x, y, z) = RedstoneOre::getID();
                else if(r == 5 && y < 34 && rand() % 2 == 0)
                    world->GetBlock(x, y, z) = LapisOre::getID();
                else if(r == 6 && y < 14 && rand() % 3 == 0)
                    world->GetBlock(x, y, z) = DiamondOre::getID();
            }
        }
        step++;
        if(SliceBudgetSpent(startUs, budgetUs))
            return false;
    }

    // Hand the streamed column back once, with the surface and its water in place and the caves not
    // started. The caller lights that surface and the mesher draws it while the slices below carve.
    // A sync caller (budget 0) never stops here, so spawn terrain is unchanged.
    if(budgetUs != 0 && step == CHUNK_SIZE && world->pauseAfterTerrain)
    {
        world->pauseAfterTerrain = false;
        ChunkColumn* col = world->FindColumn(cx, cz);
        if(col)
        {
            int maxH = 0;
            int minH = 0x7fffffff;
            int n = CHUNK_SIZE < 16 ? CHUNK_SIZE : 16;
            for(int lz = 0; lz < n; lz++)
                for(int lx = 0; lx < n; lx++)
                {
                    if(colHeights[lx][lz] > maxH)
                        maxH = colHeights[lx][lz];
                    if(colHeights[lx][lz] < minH)
                        minH = colHeights[lx][lz];
                }
            if(minH > maxH)
                minH = maxH;
            // The whole band, not just the summit: the mesher draws every section the ground passes
            // through, so a hillside's lower half does not wait for the caves under it.
            col->surfaceY = maxH;
            col->surfaceMinY = minH;
            col->surfaceKnown = true;
        }
        noiseGridOn = false;
        return false;
    }

    int caveSteps = 0;
    for(int lz = 0; lz + 1 < CHUNK_SIZE && lz + 1 < 16; lz += 2)
        caveSteps++;
    int decorateAt = CHUNK_SIZE + caveSteps;
    bool carve = world->makeCaves && worldType != SUPERFLAT && worldType != FlatOcean && worldType != FlatNETHER;
    if(!carve)
        step = decorateAt;

    // Caves: one 3D noise sample is shared by a 2x2 block footprint (4x fewer sdnoise3
    // calls, this was the most expensive part of a streamed column) and nothing is
    // carved under the sea floor. One 2-block strip per resume, so a strip that runs
    // long is the frame, not the whole column.
    while(step < decorateAt)
    {
        int lz = (step - CHUNK_SIZE) * 2;
        for(int lx = 0; lx + 1 < CHUNK_SIZE && lx + 1 < 16; lx += 2)
        {
            int x = x0 + lx;
            int z = z0 + lz;
            if(x < 0 || z < 0 || x + 1 >= world->worldLimit || z + 1 >= world->worldLimit)
                continue;
            int h = colHeights[lx][lz];
            int h2 = colHeights[lx + 1][lz];
            if(h2 < h) h = h2;
            h2 = colHeights[lx][lz + 1];
            if(h2 < h) h = h2;
            h2 = colHeights[lx + 1][lz + 1];
            if(h2 < h) h = h2;
            if(h < waterLevel - 1)
                continue;
            int yEnd = h - 8;
            if(yEnd > WORLD_HEIGHT - 2)
                yEnd = WORLD_HEIGHT - 2;
            for(int y = 8; y < yEnd; y++)
            {
                if(GetValue3D(x, y, z, 1, 0.09f, 1.0f) <= 0.52f)
                    continue;
                for(int dz = 0; dz < 2; dz++)
                {
                    for(int dx = 0; dx < 2; dx++)
                    {
                        block_t& b = world->GetBlock(x + dx, y, z + dz);
                        if(b == RockBlock::getID() || b == DirtBlock::getID() || b == CoalOre::getID() || b == IronOre::getID())
                            b = 0;
                    }
                }
            }
        }
        step++;
        if(SliceBudgetSpent(startUs, budgetUs))
            return false;
    }

    if(step == decorateAt)
    {
        if(SliceBudgetSpent(startUs, budgetUs))
            return false;
        if(worldType != SUPERFLAT && worldType != FlatOcean && worldType != FlatNETHER && worldType != FlatDesertLakes && worldType != FlatSnowyTaiga)
            DecorateColumn(world, cx, cz, true);
        step++;
    }
    noiseGridOn = false;
    // A finished overworld column may grow one animal pair. A column read back from the card
    // never reaches here, so a saved herd is not joined by a second one.
    world->TryAnimalPack(cx, cz);
    return true;
}

// Surface / underground features for one 16x16 column. Used for spawn columns and
// streamed columns alike so both look the same. Everything here stays inside the
// column except tree canopies and pockets, which may spill into loaded neighbours.
void WorldGenerator::DecorateColumn(CraftWorld *world, int cx, int cz, bool useCache)
{
    int CHUNK_SIZE = world->CHUNK_SIZE;
    int WORLD_HEIGHT = world->WORLD_HEIGHT;
    int worldType = world->worldType;
    int waterLevel = world->waterLevel;
    if(waterLevel < 8)
        waterLevel = 58;
    int x0 = cx * CHUNK_SIZE;
    int z0 = cz * CHUNK_SIZE;
    if(x0 < 0 || z0 < 0 || x0 >= world->worldLimit || z0 >= world->worldLimit)
        return;
    if(CHUNK_SIZE > 16)
        useCache = false;

    int biome = useCache ? colBiomes[CHUNK_SIZE / 2][CHUNK_SIZE / 2] : BiomeAt(x0 + CHUNK_SIZE / 2, z0 + CHUNK_SIZE / 2, worldType);
    // forest density varies inside a biome so woods have clearings and thickets
    float forestNoise = GetValue2D3(x0 + 8, z0 + 8, 1, 0.12f, 1.0f);

    // ---- underground pockets: dirt, gravel, lava, water ----
    for(int p = 0; p < 2; p++)
    {
        int px = x0 + rand() % CHUNK_SIZE;
        int pz = z0 + rand() % CHUNK_SIZE;
        int py = 6 + rand() % 50;
        if(world->GetBlock(px, py, pz) == RockBlock::getID())
            world->BuildBlockSphere(1.5f + (rand() % 10) / 10.0f, (p == 0) ? DirtBlock::getID() : Gravel::getID(), px, py, pz, RockBlock::getID());
    }
    if(rand() % 9 == 0)
    {
        int px = x0 + 4 + rand() % (CHUNK_SIZE - 8);
        int pz = z0 + 4 + rand() % (CHUNK_SIZE - 8);
        int py = 9 + rand() % 14;
        bool solid = true;
        for(int dx = -3; dx <= 3 && solid; dx++)
            for(int dy = -3; dy <= 4 && solid; dy++)
                for(int dz = -3; dz <= 3 && solid; dz++)
                    if(world->GetBlock(px + dx, py + dy, pz + dz) == 0)
                        solid = false;
        if(solid)
        {
            float r = 2.0f + (rand() % 10) / 10.0f;
            world->BuildBlockSphere(r, Lava::getID(), px, py, pz);
            for(int dx = -3; dx <= 3; dx++)
                for(int dy = 0; dy <= 4; dy++)
                    for(int dz = -3; dz <= 3; dz++)
                        if(world->GetBlock(px + dx, py + dy, pz + dz) == Lava::getID())
                            world->GetBlock(px + dx, py + dy, pz + dz) = 0;
        }
    }

    // ---- surface pass ----
    for(int lz = 0; lz < CHUNK_SIZE; lz++)
    {
        for(int lx = 0; lx < CHUNK_SIZE; lx++)
        {
            int x = x0 + lx;
            int z = z0 + lz;
            int y;
            if(useCache)
            {
                // terrain top is known; only a tree/pocket could have changed it, so a
                // short scan from a few blocks above is enough (no 112-block walk)
                int top = colHeights[lx][lz] + 3;
                if(top >= WORLD_HEIGHT) top = WORLD_HEIGHT - 1;
                y = -1;
                for(int yy = top; yy >= 0; yy--)
                {
                    block_t b = world->GetBlock(x, yy, z);
                    if(b != 0 && b != Snow2::getID())
                    {
                        y = yy;
                        break;
                    }
                }
            }
            else
                y = world->groundHeightExcept(x, z, Snow2::getID());
            if(y < 3 || y + 3 >= WORLD_HEIGHT)
                continue;
            block_t ground = world->GetBlock(x, y, z);
            block_t above = world->GetBlock(x, y + 1, z);
            bool open = (above == 0 || above == Snow2::getID());
            if(!open)
                continue;
            bool onGrass = (ground == GrassBlock::getID());
            bool onDirt = (ground == DirtBlock::getID());
            bool onSnow = (ground == SnowSoil::getID());
            bool onSand = (ground == SandBlock::getID());
            bool inner = (lx >= 2 && lx < CHUNK_SIZE - 2 && lz >= 2 && lz < CHUNK_SIZE - 2);
            int localBiome = biome;
            if(lx == 0 || lz == 0 || lx == CHUNK_SIZE - 1 || lz == CHUNK_SIZE - 1)
                localBiome = useCache ? colBiomes[lx][lz] : BiomeAt(x, z, worldType);

            // sugar cane on sand next to water at sea level
            if(onSand && y == waterLevel && rand() % 14 == 0)
            {
                if(world->GetBlock(x - 1, y, z) == WaterBlock::getID() || world->GetBlock(x + 1, y, z) == WaterBlock::getID() ||
                   world->GetBlock(x, y, z - 1) == WaterBlock::getID() || world->GetBlock(x, y, z + 1) == WaterBlock::getID())
                {
                    int h = 1 + rand() % 3;
                    for(int cy = y + 1; cy <= y + h && cy < WORLD_HEIGHT - 1; cy++)
                        world->GetBlock(x, cy, z) = CaneBlock::getID();
                    continue;
                }
            }

            if(localBiome == BIOME_DESERT)
            {
                if(onSand && y > waterLevel && rand() % 90 == 0 && world->GetBlock(x, y + 2, z) == 0)
                {
                    int h = 1 + rand() % 3;
                    bool room = true;
                    for(int cy = y + 1; cy <= y + h && room; cy++)
                        if(world->GetBlock(x - 1, cy, z) != 0 || world->GetBlock(x + 1, cy, z) != 0 || world->GetBlock(x, cy, z - 1) != 0 || world->GetBlock(x, cy, z + 1) != 0)
                            room = false;
                    if(room)
                        for(int cy = y + 1; cy <= y + h && cy < WORLD_HEIGHT - 1; cy++)
                            world->GetBlock(x, cy, z) = CactusBlock::getID();
                }
                continue;
            }

            if(localBiome == BIOME_SNOW)
            {
                if(!world->makeTrees || !onSnow || !inner || y <= waterLevel)
                    continue;
                int chance = 70 - (int)(forestNoise * 40.0f);
                if(chance < 24) chance = 24;
                if(rand() % chance == 0)
                {
                    // spruce trunk starts at the block passed in; ground below becomes dirt
                    world->GetBlock(x, y + 1, z) = 0;
                    GenerateSpruceTree(world->worldLimit, world, x, y + 1, z);
                    if(world->GetBlock(x, y + 1, z) == 0)
                        world->GetBlock(x, y + 1, z) = Snow2::getID();
                }
                continue;
            }

            // plains / forest
            if(!(onGrass || onDirt))
                continue;
            if(y <= waterLevel)
                continue;

            if(world->makeTrees && inner)
            {
                int chance;
                if(localBiome == BIOME_FOREST)
                    chance = 26 - (int)(forestNoise * 14.0f);
                else
                    chance = 190 - (int)(forestNoise * 90.0f);
                if(chance < 12) chance = 12;
                if(rand() % chance == 0)
                {
                    if(onDirt)
                        world->GetBlock(x, y, z) = GrassBlock::getID();
                    int kind = rand() % 10;
                    if(kind < 2)
                        GenerateClassicTree(world->worldLimit, world, x, y, z, WhiteWoodBlock::getID(), BirchLeaves::getID());
                    else if(kind == 2 && localBiome == BIOME_FOREST)
                        GenerateSpruceTree(world->worldLimit, world, x, y + 1, z);
                    else
                        GenerateClassicTree(world->worldLimit, world, x, y, z, WoodBlock::getID(), LeavesBlock::getID());
                    continue;
                }
            }

            if(!onGrass)
                continue;

            // pumpkin patches (rare, clustered)
            if(rand() % 700 == 0)
            {
                for(int dx = -1; dx <= 1; dx++)
                    for(int dz = -1; dz <= 1; dz++)
                    {
                        if(rand() % 3 != 0)
                            continue;
                        int gy = world->groundHeight(x + dx, z + dz);
                        if(gy <= 0 || gy + 1 >= WORLD_HEIGHT)
                            continue;
                        block_t g = world->GetBlock(x + dx, gy, z + dz);
                        if((g == GrassBlock::getID() || g == DirtBlock::getID()) && world->GetBlock(x + dx, gy + 1, z + dz) == 0)
                        {
                            world->GetBlock(x + dx, gy, z + dz) = DirtBlock::getID();
                            world->GetBlock(x + dx, gy + 1, z + dz) = 73 + rand() % 4;
                        }
                    }
                continue;
            }

            int plant = rand() % 100;
            int grassChance = (localBiome == BIOME_FOREST) ? 14 : 9;
            if(plant < grassChance)
                world->GetBlock(x, y + 1, z) = TallGrass::getID();
            else if(plant < grassChance + 2)
                world->GetBlock(x, y + 1, z) = (rand() % 2 == 0) ? Flower1::getID() : Flower2::getID();
            else if(plant == 99 && localBiome == BIOME_FOREST)
                world->GetBlock(x, y + 1, z) = Fern::getID();
            else if(plant == 98 && localBiome == BIOME_FOREST && rand() % 4 == 0)
            {
                // mushrooms hide in darker forest
                world->GetBlock(x, y + 1, z) = (rand() % 2 == 0) ? Mooshroom1::getID() : Mooshroom2::getID();
            }
        }
    }
}

void WorldGenerator::DecorateSpawnRegion(CraftWorld *world)
{
    int cs = world->CHUNK_SIZE;
    if(cs <= 0)
        return;
    for(int z = world->genMinZ; z < world->genMaxZ; z += cs)
        for(int x = world->genMinX; x < world->genMaxX; x += cs)
            DecorateColumn(world, x / cs, z / cs);
}
