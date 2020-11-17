#include "WorldGenerator.h"
#include "CraftWorld2.h"
#include "LoadingScreen.h"

#include <Aurora/Utils/Logger.h>

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
#define FlatNETHER 8

using namespace noisepp;

void WorldGenerator::initRandompMap(int worldSize, int worldHeight, int chunkSize, CraftWorld *world, bool makeTrees,bool makeWater,bool makeCaves, unsigned int seedII, int worldType)
{
    LoadingScreen* loading = new LoadingScreen();

    loading->readiness = 1;
    loading->stateName = 1;

    int WORLD_SIZE = worldSize;
    int CHUNK_SIZE = chunkSize;
    int WORLD_HEIGHT = 98;

    if (seedII == 0)
    {
        seedII = rand() % 1000000;
    }
    unsigned int seed = seedII;
    srand(seed);

    world->worldSeed = seed;
    SetSeed(seed);

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

        PerlinModule* perlin = new PerlinModule();
        perlin->setQuality(noisepp::NOISE_QUALITY_HIGH);

        float *dataBeach = new float[WORLD_SIZE * WORLD_SIZE];
        perlin->setSeed(seed+10);
        perlin->setOctaveCount(4);
        perlin->setPersistence(0.17f);
        perlin->setFrequency(0.8f);
        perlin->setScale(4.5f);
        perlin->setLacunarity(3.5f);

        noisepp::utils::PlaneBuilder2D builderTrees;
        builderTrees.setModule(perlin);
        builderTrees.setSize(WORLD_SIZE, WORLD_SIZE);
        builderTrees.setBounds(0, 0, 4, 5.5f);
        builderTrees.setDestination(dataBeach);
        builderTrees.build ();


        loading->stateName = 1; // terrain generation
        for (int z = 0; z < WORLD_SIZE; z++)
        {
            for (int x = 0; x < WORLD_SIZE; x++)
            {
                //int distanceToCenter = world->FastDistance2d(abs(x-WORLD_SIZE/2),abs(z-WORLD_SIZE/2));
                loading->readiness = ((float)(x+z*WORLD_SIZE)/(66503.0f))*100;

                //int Height  = data[x + z*WORLD_SIZE]* 128/12 + middleHeight;
                float HillFrequency = (GetValue2D(x,z,1,0.05f,1)+1.0f)/2.0f;

                int Height = (InterpolateElevationNoise(x,z) + InterpolateRoughnessNoise(x,z)*GetValue2D3(x,z,6,0.1f,1))*64*HillFrequency+62;

                if(Height <= 48)
                {
                    Height = 48-(48-Height)/2;
                }
                if(Height >= 68)
                {
                   Height = 68+(Height-68)/2;
                }

                float perlinHeight = dataBeach[x + z*WORLD_SIZE]* 1048576.0f/12.0f + 69;

                for (int y = Height; y >= 0; y--)
                {
                    if(y == Height)
                    {
                        if(world->GetBlock(x,y+1,z) == 0)
                        {
                            if(Height <= waterLevel+2)
                            {
                                world->GetBlock(x, y, z) = SandBlock::getID();
                                if(perlinHeight > 75.0f)
                                {
                                    world->GetBlock(x, y, z) = GrassBlock::getID();
									if(y < waterLevel)
                                    {
                                        world->GetBlock(x, y, z) = DirtBlock::getID();
                                    }
                                }
                            }
                            else
                            {
                                world->GetBlock(x, y, z) = GrassBlock::getID();
                            }
                        }
                    }
                    else if(y < Height-4)
                    {
                        if(world->GetBlock(x,y,z) == RockBlock::getID())
                        {
                            break;
                        }
                        world->GetBlock(x, y, z) = RockBlock::getID();
                    }
                    else
                    {
                        if(Height <= waterLevel+3)
                        {
                            world->GetBlock(x, y, z) = SandBlock::getID();
                            if(perlinHeight > 75.0f)
                            {
                                world->GetBlock(x, y, z) = DirtBlock::getID();
                            }

							world->GetBlock(x, y, z) = RockBlock::getID();
                            if(perlinHeight > 75.0f)
                            {
                                world->GetBlock(x, y, z) = DirtBlock::getID();
                            }
                        }
                        else
                        {
							world->GetBlock(x, y, z) = DirtBlock::getID();
                        }       
                    }
                }

                if(Height < waterLevel)
                {
                    for (int y = waterLevel; y > Height; y--)
                    {
                        world->GetBlock(x, y, z) = WaterBlock::getID();
                    }
                }
            }
        }

       // delete []data;
        delete []dataBeach;

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

            int numWorms = 100 + rand() % 6;
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
                    x = 10 + rand() % (WORLD_SIZE-10);
                    z = 10 + rand() % (WORLD_SIZE-10);
                    y = 6 + rand() % (world->groundHeight(x,z)-32);
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

                    if(x > WORLD_SIZE-5 || x <= 4)
                    {
                        if(x > 30)
                        {
                            x = WORLD_SIZE-6;
                        }
                        else
                        {
                            x = 5;
                        }
                        reverseX = !reverseX;
                    }

                    if(z > WORLD_SIZE-5 || z <= 4)
                    {
                        if(z > 30)
                        {
                            z = WORLD_SIZE-6;
                        }
                        else
                        {
                            z = 5;
                        }
                        reverseZ = !reverseZ;
                    }
                }
            }
            delete [] dataVer;
            delete [] dataHor;
        }

        initDirt(WORLD_SIZE, world);
        initGravel(WORLD_SIZE, world);
        //initErosion(WORLD_HEIGHT, world);
        initBiome(WORLD_SIZE,world,perlin,seed);

        initPumpkins(WORLD_SIZE,world);
        initCanes(WORLD_SIZE,world,waterLevel);

        loading->readiness = 80;
        loading->stateName = 4; // trees planting

        initFlowers(WORLD_SIZE, world);

        if(makeTrees)
        {
            initRandomTrees(WORLD_SIZE, world);
        }

        loading->readiness = 85;
        loading->stateName = 5; // ore placing

        initBedrock(WORLD_SIZE, world);
        initClay(WORLD_SIZE, world);

        loading->readiness = 90;
        loading->stateName = 6;

        if(genGrass)
        {
            initGrass(WORLD_SIZE, world);
        }

        //initOtherVegetation(WORLD_SIZE, world);
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

        for(int i = 0; i <= 1; i++)
        {
            int spawnX,spawnY,spawnZ;

            spawnX = 20 + rand() % (WORLD_SIZE-20);
            spawnZ = 20 + rand() % (WORLD_SIZE-20);
            spawnY = world->groundHeightWater(spawnX,spawnZ);

            world->SpawnCow(spawnX+0.5f,spawnY+2.0f,spawnZ+0.5f);
            world->SpawnCow(spawnX+2.0f+0.5f,world->groundHeightWater(spawnX+2.0f,spawnZ+2.0f)+2.0f,spawnZ+2.0f+0.5f);
        }

        for(int i = 0; i < 1; i++)
        {
            int spawnX,spawnY,spawnZ;

            spawnX = 30 + rand() % (WORLD_SIZE-30);
            spawnZ = 30 + rand() % (WORLD_SIZE-30);
            spawnY = world->groundHeightWater(spawnX,spawnZ);

            world->SpawnSheep(spawnX+0.5f,spawnY+2.0f,spawnZ+0.5f);
            world->SpawnSheep(spawnX+2.5f,world->groundHeightWater(spawnX+2.0f,spawnZ+2.0f)+2.0f,spawnZ+2.5f);
        }

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

            newPlayerSpawnPosition = Vector3(border+rand()%(WORLD_SIZE-border-1),0,border+rand()%(WORLD_SIZE-border-1));
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
                world->SetPlayerSpawnPosition(Vector3(WORLD_SIZE/2+0.5f,world->groundHeightWater(WORLD_SIZE/2,WORLD_SIZE/2)+world->playerGrowth-0.05f-0.5f,WORLD_SIZE/2+0.5f));
            }
            tries ++;
        }

        delete perlin;
    }

    int spawnX = 128;
    int spawnZ = 128;
    int spawnY = world->groundHeightWater(spawnX,spawnZ);
    world->SpawnSheep(spawnX,spawnY+12.0f/16.0f,spawnZ);
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

		PerlinModule* perlin = new PerlinModule();
		perlin->setQuality(noisepp::NOISE_QUALITY_HIGH);

		float* dataBeach = new float[WORLD_SIZE * WORLD_SIZE];
		perlin->setSeed(seed + 10);
		perlin->setOctaveCount(4);
		perlin->setPersistence(0.17f);
		perlin->setFrequency(0.8f);
		perlin->setScale(4.5f);
		perlin->setLacunarity(3.5f);

		noisepp::utils::PlaneBuilder2D builderTrees;
		builderTrees.setModule(perlin);
		builderTrees.setSize(WORLD_SIZE, WORLD_SIZE);
		builderTrees.setBounds(0, 0, 4, 5.5f);
		builderTrees.setDestination(dataBeach);
		builderTrees.build();


		loading->stateName = 1; // terrain generation
		for (int z = 0; z < WORLD_SIZE; z++)
		{
			for (int x = 0; x < WORLD_SIZE; x++)
			{
				//int distanceToCenter = world->FastDistance2d(abs(x-WORLD_SIZE/2),abs(z-WORLD_SIZE/2));
				loading->readiness = ((float)(x + z * WORLD_SIZE) / (66503.0f)) * 100;

				//int Height  = data[x + z*WORLD_SIZE]* 128/12 + middleHeight;
				float HillFrequency = (GetValue2D(x, z, 1, 0.05f, 1) + 1.0f) / 2.0f;

				int Height = (InterpolateElevationNoise(x, z) + InterpolateRoughnessNoise(x, z) * GetValue2D3(x, z, 6, 0.1f, 1)) * 64 * HillFrequency + 62;

				if (Height <= 48)
				{
					Height = 48 - (48 - Height) / 2;
				}
				if (Height >= 68)
				{
					Height = 68 + (Height - 68) / 2;
				}

				float perlinHeight = dataBeach[x + z * WORLD_SIZE] * 1048576.0f / 12.0f + 69;

				for (int y = Height; y >= 0; y--)
				{
					if (y == Height)
					{
						if (world->GetBlock(x, y + 1, z) == 0)
						{
							if (Height <= waterLevel + 2)
							{
								world->GetBlock(x, y, z) = DirtBlock::getID();
								if (perlinHeight > 75.0f)
								{
									world->GetBlock(x, y, z) = SnowSoil::getID();
									if (y < waterLevel)
									{
										world->GetBlock(x, y, z) = DirtBlock::getID();
									}
								}
							}
							else
							{
								world->GetBlock(x, y, z) = SnowSoil::getID();
							}
						}
					}
					else if (y < Height - 4)
					{
						if (world->GetBlock(x, y, z) == RockBlock::getID())
						{
							break;
						}
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					else
					{
						if (Height <= waterLevel + 3)
						{
							world->GetBlock(x, y, z) = DirtBlock::getID();
							if (perlinHeight > 75.0f)
							{
								world->GetBlock(x, y, z) = DirtBlock::getID();
							}

							world->GetBlock(x, y, z) = RockBlock::getID();
							if (perlinHeight > 75.0f)
							{
								world->GetBlock(x, y, z) = DirtBlock::getID();
							}
						}
						else
						{
							world->GetBlock(x, y, z) = DirtBlock::getID();
						}
					}
				}

				if (Height < waterLevel)
				{
					for (int y = waterLevel; y > Height; y--)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
				}
			}
		}

		// delete []data;
		delete[]dataBeach;

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

			int numWorms = 100 + rand() % 6;
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
					x = 10 + rand() % (WORLD_SIZE - 10);
					z = 10 + rand() % (WORLD_SIZE - 10);
					y = 6 + rand() % (world->groundHeight(x, z) - 32);
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
								if (world->GetBlock(xx, yy, zz) == world->GetBlock(xx, yy, zz) == WaterBlock::getID())
								{
									haveWater = true;
								}
							}
						}
					}

					if (haveWater == false)
					{
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

					if (x > WORLD_SIZE - 5 || x <= 4)
					{
						if (x > 30)
						{
							x = WORLD_SIZE - 6;
						}
						else
						{
							x = 5;
						}
						reverseX = !reverseX;
					}

					if (z > WORLD_SIZE - 5 || z <= 4)
					{
						if (z > 30)
						{
							z = WORLD_SIZE - 6;
						}
						else
						{
							z = 5;
						}
						reverseZ = !reverseZ;
					}
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

		initFlowers(WORLD_SIZE, world);

		if (makeTrees)
		{
			initRandomTrees(WORLD_SIZE, world);
		}

		loading->readiness = 85;
		loading->stateName = 5; // ore placing

		initBedrock(WORLD_SIZE, world);
		initClay(WORLD_SIZE, world);

		loading->readiness = 90;
		loading->stateName = 6;

		if (genGrass)
		{
			initGrass(WORLD_SIZE, world);
		}

		//initOtherVegetation(WORLD_SIZE, world);
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

		for (int i = 0; i <= 1; i++)
		{
			int spawnX, spawnY, spawnZ;

			spawnX = 20 + rand() % (WORLD_SIZE - 20);
			spawnZ = 20 + rand() % (WORLD_SIZE - 20);
			spawnY = world->groundHeightWater(spawnX, spawnZ);

			world->SpawnCow(spawnX + 0.5f, spawnY + 2.0f, spawnZ + 0.5f);
			world->SpawnCow(spawnX + 2.0f + 0.5f, world->groundHeightWater(spawnX + 2.0f, spawnZ + 2.0f) + 2.0f, spawnZ + 2.0f + 0.5f);
		}

		for (int i = 0; i < 1; i++)
		{
			int spawnX, spawnY, spawnZ;

			spawnX = 30 + rand() % (WORLD_SIZE - 30);
			spawnZ = 30 + rand() % (WORLD_SIZE - 30);
			spawnY = world->groundHeightWater(spawnX, spawnZ);

			world->SpawnSheep(spawnX + 0.5f, spawnY + 2.0f, spawnZ + 0.5f);
			world->SpawnSheep(spawnX + 2.5f, world->groundHeightWater(spawnX + 2.0f, spawnZ + 2.0f) + 2.0f, spawnZ + 2.5f);
		}

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

			newPlayerSpawnPosition = Vector3(border + rand() % (WORLD_SIZE - border - 1), 0, border + rand() % (WORLD_SIZE - border - 1));
			int groundBlock = world->GetBlock(newPlayerSpawnPosition.x, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z), newPlayerSpawnPosition.z);
			if (groundBlock != WaterBlock::getID())
			{
				newPlayerSpawnPosition = Vector3(newPlayerSpawnPosition.x + 0.5f, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z) + world->playerGrowth - 0.05f + 1, newPlayerSpawnPosition.z + 0.5f);
				world->SetPlayerSpawnPosition(newPlayerSpawnPosition);
				break;
			}
			if (tries == 3071)
			{
				world->SetPlayerSpawnPosition(Vector3(WORLD_SIZE / 2 + 0.5f, world->groundHeightWater(WORLD_SIZE / 2, WORLD_SIZE / 2) + world->playerGrowth - 0.05f - 0.5f, WORLD_SIZE / 2 + 0.5f));
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

		PerlinModule* perlin = new PerlinModule();
		perlin->setQuality(noisepp::NOISE_QUALITY_HIGH);

		float* dataBeach = new float[WORLD_SIZE * WORLD_SIZE];
		perlin->setSeed(seed + 10);
		perlin->setOctaveCount(4);
		perlin->setPersistence(0.17f);
		perlin->setFrequency(0.8f);
		perlin->setScale(4.5f);
		perlin->setLacunarity(3.5f);

		noisepp::utils::PlaneBuilder2D builderTrees;
		builderTrees.setModule(perlin);
		builderTrees.setSize(WORLD_SIZE, WORLD_SIZE);
		builderTrees.setBounds(0, 0, 4, 5.5f);
		builderTrees.setDestination(dataBeach);
		builderTrees.build();


		loading->stateName = 1; // terrain generation
		for (int z = 0; z < WORLD_SIZE; z++)
		{
			for (int x = 0; x < WORLD_SIZE; x++)
			{
				//int distanceToCenter = world->FastDistance2d(abs(x-WORLD_SIZE/2),abs(z-WORLD_SIZE/2));
				loading->readiness = ((float)(x + z * WORLD_SIZE) / (66503.0f)) * 100;

				//int Height  = data[x + z*WORLD_SIZE]* 128/12 + middleHeight;
				float HillFrequency = (GetValue2D(x, z, 1, 0.05f, 1) + 1.0f) / 2.0f;

				int Height = (InterpolateElevationNoise(x, z) + InterpolateRoughnessNoise(x, z) * GetValue2D3(x, z, 6, 0.1f, 1)) * 64 * HillFrequency + 62;

				if (Height <= 48)
				{
					Height = 48 - (48 - Height) / 2;
				}
				if (Height >= 68)
				{
					Height = 68 + (Height - 68) / 2;
				}

				float perlinHeight = dataBeach[x + z * WORLD_SIZE] * 1048576.0f / 12.0f + 69;

				for (int y = Height; y >= 0; y--)
				{
					if (y == Height)
					{
						if (world->GetBlock(x, y + 1, z) == 0)
						{
							if (Height <= waterLevel + 2)
							{
								world->GetBlock(x, y, z) = DirtBlock::getID();
								if (perlinHeight > 75.0f)
								{
									world->GetBlock(x, y, z) = GrassBlock::getID();
									if (y < waterLevel)
									{
										world->GetBlock(x, y, z) = DirtBlock::getID();
									}
								}
							}
							else
							{
								world->GetBlock(x, y, z) = GrassBlock::getID();
							}
						}
					}
					else if (y < Height - 4)
					{
						if (world->GetBlock(x, y, z) == RockBlock::getID())
						{
							break;
						}
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					else
					{
						if (Height <= waterLevel + 3)
						{
							world->GetBlock(x, y, z) = DirtBlock::getID();
							if (perlinHeight > 75.0f)
							{
								world->GetBlock(x, y, z) = DirtBlock::getID();
							}

							world->GetBlock(x, y, z) = RockBlock::getID();
							if (perlinHeight > 75.0f)
							{
								world->GetBlock(x, y, z) = DirtBlock::getID();
							}
						}
						else
						{
							world->GetBlock(x, y, z) = DirtBlock::getID();
						}
					}
				}

				if (Height < waterLevel)
				{
					for (int y = waterLevel; y > Height; y--)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
				}
			}
		}

		// delete []data;
		delete[]dataBeach;

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

			int numWorms = 100 + rand() % 6;
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
					x = 10 + rand() % (WORLD_SIZE - 10);
					z = 10 + rand() % (WORLD_SIZE - 10);
					y = 6 + rand() % (world->groundHeight(x, z) - 32);
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
								if (world->GetBlock(xx, yy, zz) == world->GetBlock(xx, yy, zz) == WaterBlock::getID())
								{
									haveWater = true;
								}
							}
						}
					}

					if (haveWater == false)
					{
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

					if (x > WORLD_SIZE - 5 || x <= 4)
					{
						if (x > 30)
						{
							x = WORLD_SIZE - 6;
						}
						else
						{
							x = 5;
						}
						reverseX = !reverseX;
					}

					if (z > WORLD_SIZE - 5 || z <= 4)
					{
						if (z > 30)
						{
							z = WORLD_SIZE - 6;
						}
						else
						{
							z = 5;
						}
						reverseZ = !reverseZ;
					}
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

		initFlowers(WORLD_SIZE, world);

		if (makeTrees)
		{
			initRandomTrees(WORLD_SIZE, world);
		}

		loading->readiness = 85;
		loading->stateName = 5; // ore placing

		initBedrock(WORLD_SIZE, world);
		initClay(WORLD_SIZE, world);

		loading->readiness = 90;
		loading->stateName = 6;

		if (genGrass)
		{
			initGrass(WORLD_SIZE, world);
		}

		//initOtherVegetation(WORLD_SIZE, world);
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

		for (int i = 0; i <= 1; i++)
		{
			int spawnX, spawnY, spawnZ;

			spawnX = 20 + rand() % (WORLD_SIZE - 20);
			spawnZ = 20 + rand() % (WORLD_SIZE - 20);
			spawnY = world->groundHeightWater(spawnX, spawnZ);

			world->SpawnCow(spawnX + 0.5f, spawnY + 2.0f, spawnZ + 0.5f);
			world->SpawnCow(spawnX + 2.0f + 0.5f, world->groundHeightWater(spawnX + 2.0f, spawnZ + 2.0f) + 2.0f, spawnZ + 2.0f + 0.5f);
		}

		for (int i = 0; i < 1; i++)
		{
			int spawnX, spawnY, spawnZ;

			spawnX = 30 + rand() % (WORLD_SIZE - 30);
			spawnZ = 30 + rand() % (WORLD_SIZE - 30);
			spawnY = world->groundHeightWater(spawnX, spawnZ);

			world->SpawnSheep(spawnX + 0.5f, spawnY + 2.0f, spawnZ + 0.5f);
			world->SpawnSheep(spawnX + 2.5f, world->groundHeightWater(spawnX + 2.0f, spawnZ + 2.0f) + 2.0f, spawnZ + 2.5f);
		}

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

			newPlayerSpawnPosition = Vector3(border + rand() % (WORLD_SIZE - border - 1), 0, border + rand() % (WORLD_SIZE - border - 1));
			int groundBlock = world->GetBlock(newPlayerSpawnPosition.x, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z), newPlayerSpawnPosition.z);
			if (groundBlock != WaterBlock::getID())
			{
				newPlayerSpawnPosition = Vector3(newPlayerSpawnPosition.x + 0.5f, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z) + world->playerGrowth - 0.05f + 1, newPlayerSpawnPosition.z + 0.5f);
				world->SetPlayerSpawnPosition(newPlayerSpawnPosition);
				break;
			}
			if (tries == 3071)
			{
				world->SetPlayerSpawnPosition(Vector3(WORLD_SIZE / 2 + 0.5f, world->groundHeightWater(WORLD_SIZE / 2, WORLD_SIZE / 2) + world->playerGrowth - 0.05f - 0.5f, WORLD_SIZE / 2 + 0.5f));
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

		PerlinModule* perlin = new PerlinModule();
		perlin->setQuality(noisepp::NOISE_QUALITY_HIGH);

		float* dataBeach = new float[WORLD_SIZE * WORLD_SIZE];
		perlin->setSeed(seed + 10);
		perlin->setOctaveCount(4);
		perlin->setPersistence(0.17f);
		perlin->setFrequency(0.8f);
		perlin->setScale(4.5f);
		perlin->setLacunarity(3.5f);

		noisepp::utils::PlaneBuilder2D builderTrees;
		builderTrees.setModule(perlin);
		builderTrees.setSize(WORLD_SIZE, WORLD_SIZE);
		builderTrees.setBounds(0, 0, 4, 5.5f);
		builderTrees.setDestination(dataBeach);
		builderTrees.build();


		loading->stateName = 1; // terrain generation
		for (int z = 0; z < WORLD_SIZE; z++)
		{
			for (int x = 0; x < WORLD_SIZE; x++)
			{
				//int distanceToCenter = world->FastDistance2d(abs(x-WORLD_SIZE/2),abs(z-WORLD_SIZE/2));
				loading->readiness = ((float)(x + z * WORLD_SIZE) / (66503.0f)) * 100;

				//int Height  = data[x + z*WORLD_SIZE]* 128/12 + middleHeight;
				float HillFrequency = (GetValue2D(x, z, 1, 0.05f, 1) + 1.0f) / 2.0f;

				int Height = (InterpolateElevationNoise(x, z) + InterpolateRoughnessNoise(x, z) * GetValue2D3(x, z, 6, 0.1f, 1)) * 64 * HillFrequency + 62;

				if (Height <= 48)
				{
					Height = 48 - (48 - Height) / 2;
				}
				if (Height >= 68)
				{
					Height = 68 + (Height - 68) / 2;
				}

				float perlinHeight = dataBeach[x + z * WORLD_SIZE] * 1048576.0f / 12.0f + 69;

				for (int y = Height; y >= 0; y--)
				{
					if (y == Height)
					{
						if (world->GetBlock(x, y + 1, z) == 0)
						{
							if (Height <= waterLevel + 2)
							{
								world->GetBlock(x, y, z) = SandBlock::getID();
								if (perlinHeight > 75.0f)
								{
									if (y < waterLevel)
									{
										world->GetBlock(x, y, z) = SandStone::getID();
									}
								}
							}
							else
							{
								world->GetBlock(x, y, z) = SandBlock::getID();
							}
						}
					}
					else if (y < Height - 4)
					{
						if (world->GetBlock(x, y, z) == RockBlock::getID())
						{
							break;
						}
						world->GetBlock(x, y, z) = RockBlock::getID();
					}
					else
					{
						if (Height <= waterLevel + 3)
						{
							world->GetBlock(x, y, z) = SandBlock::getID();
							if (perlinHeight > 75.0f)
							{
								world->GetBlock(x, y, z) = SandStone::getID();
							}

							world->GetBlock(x, y, z) = RockBlock::getID();
							if (perlinHeight > 75.0f)
							{
								world->GetBlock(x, y, z) = SandStone::getID();
							}
						}
						else
						{
							world->GetBlock(x, y, z) = SandStone::getID();
						}
					}
				}

				if (Height < waterLevel)
				{
					for (int y = waterLevel; y > Height; y--)
					{
						world->GetBlock(x, y, z) = WaterBlock::getID();
					}
				}
			}
		}

		// delete []data;
		delete[]dataBeach;

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

			int numWorms = 100 + rand() % 6;
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
					x = 10 + rand() % (WORLD_SIZE - 10);
					z = 10 + rand() % (WORLD_SIZE - 10);
					y = 6 + rand() % (world->groundHeight(x, z) - 32);
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

					if (x > WORLD_SIZE - 5 || x <= 4)
					{
						if (x > 30)
						{
							x = WORLD_SIZE - 6;
						}
						else
						{
							x = 5;
						}
						reverseX = !reverseX;
					}

					if (z > WORLD_SIZE - 5 || z <= 4)
					{
						if (z > 30)
						{
							z = WORLD_SIZE - 6;
						}
						else
						{
							z = 5;
						}
						reverseZ = !reverseZ;
					}
				}
			}
			delete[] dataVer;
			delete[] dataHor;
		}

		initDirt(WORLD_SIZE, world);
		initGravel(WORLD_SIZE, world);
		//initErosion(WORLD_HEIGHT, world);

		initPumpkins(WORLD_SIZE, world);
		initCanes(WORLD_SIZE, world, waterLevel);

		loading->readiness = 80;
		loading->stateName = 4; // trees planting

		initFlowers(WORLD_SIZE, world);

		if (makeTrees)
		{

		}

		loading->readiness = 85;
		loading->stateName = 5; // ore placing

		initBedrock(WORLD_SIZE, world);
		initClay(WORLD_SIZE, world);

		loading->readiness = 90;
		loading->stateName = 6;

		if (genGrass)
		{
			initGrass(WORLD_SIZE, world);
		}

		//initOtherVegetation(WORLD_SIZE, world);
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

		for (int i = 0; i <= 1; i++)
		{
			int spawnX, spawnY, spawnZ;

			spawnX = 20 + rand() % (WORLD_SIZE - 20);
			spawnZ = 20 + rand() % (WORLD_SIZE - 20);
			spawnY = world->groundHeightWater(spawnX, spawnZ);

			world->SpawnCow(spawnX + 0.5f, spawnY + 2.0f, spawnZ + 0.5f);
			world->SpawnCow(spawnX + 2.0f + 0.5f, world->groundHeightWater(spawnX + 2.0f, spawnZ + 2.0f) + 2.0f, spawnZ + 2.0f + 0.5f);
		}

		for (int i = 0; i < 1; i++)
		{
			int spawnX, spawnY, spawnZ;

			spawnX = 30 + rand() % (WORLD_SIZE - 30);
			spawnZ = 30 + rand() % (WORLD_SIZE - 30);
			spawnY = world->groundHeightWater(spawnX, spawnZ);

			world->SpawnSheep(spawnX + 0.5f, spawnY + 2.0f, spawnZ + 0.5f);
			world->SpawnSheep(spawnX + 2.5f, world->groundHeightWater(spawnX + 2.0f, spawnZ + 2.0f) + 2.0f, spawnZ + 2.5f);
		}

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

			newPlayerSpawnPosition = Vector3(border + rand() % (WORLD_SIZE - border - 1), 0, border + rand() % (WORLD_SIZE - border - 1));
			int groundBlock = world->GetBlock(newPlayerSpawnPosition.x, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z), newPlayerSpawnPosition.z);
			if (groundBlock != WaterBlock::getID())
			{
				newPlayerSpawnPosition = Vector3(newPlayerSpawnPosition.x + 0.5f, world->groundHeightWater(newPlayerSpawnPosition.x, newPlayerSpawnPosition.z) + world->playerGrowth - 0.05f + 1, newPlayerSpawnPosition.z + 0.5f);
				world->SetPlayerSpawnPosition(newPlayerSpawnPosition);
				break;
			}
			if (tries == 3071)
			{
				world->SetPlayerSpawnPosition(Vector3(WORLD_SIZE / 2 + 0.5f, world->groundHeightWater(WORLD_SIZE / 2, WORLD_SIZE / 2) + world->playerGrowth - 0.05f - 0.5f, WORLD_SIZE / 2 + 0.5f));
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
        for(int x = 0; x < WORLD_SIZE; x++)
        {
            loading->readiness = ((float)x/WORLD_SIZE)*50 + 1;
            for(int z = 0; z < WORLD_SIZE; z++)
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
        world->SetPlayerSpawnPosition(Vector3(WORLD_SIZE/2,world->groundHeight(WORLD_SIZE/2,WORLD_SIZE/2)+world->playerGrowth-0.05f+1,WORLD_SIZE/2));
    }
	if (worldType == FlatNETHER)
	{
		loading->readiness = 1;
		loading->stateName = 1;
		int superflatWorldHeight = 8;
		for (int x = 0; x < WORLD_SIZE; x++)
		{
			loading->readiness = ((float)x / WORLD_SIZE) * 50 + 1;
			for (int z = 0; z < WORLD_SIZE; z++)
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
						if (rand() % 384 == 2)
						{
							//world->GetBlock(x,y,z) = 41 + rand() % 5; // stuff for survival mod
						}

						if (y >= superflatWorldHeight - 2)
						{
							world->GetBlock(x, y, z) = Netherrack::getID();
							if (rand() % 384 == 0)
							{
								//world->GetBlock(x,y,z) = WoodBlock::getID(); // stuff for survival mod
							}
						}
						if (y >= superflatWorldHeight - 20)
						{
							world->GetBlock(x, y, z) = Netherrack::getID();
							if (rand() % 384 == 0)
							{
								//world->GetBlock(x,y,z) = WoodBlock::getID(); // stuff for survival mod
							}
						}
					}
				}
			}
		}
		loading->stateName = 7;
		loading->stateName = 90;
		world->initWorldBlocksLight();
		world->SetPlayerSpawnPosition(Vector3(WORLD_SIZE / 2, world->groundHeight(WORLD_SIZE / 2, WORLD_SIZE / 2) + world->playerGrowth - 0.05f + 1, WORLD_SIZE / 2));
	}
	if (worldType == FlatSnowyTaiga)
	{
		bool snowBiomeIncluded = true;
		bool snowBiomeSuperLarge = true;
		loading->readiness = 1;
		loading->stateName = 1;
		int superflatWorldHeight = 46;
		for (int x = 0; x < WORLD_SIZE; x++)
		{
			loading->readiness = ((float)x / WORLD_SIZE) * 50 + 1;
			for (int z = 0; z < WORLD_SIZE; z++)
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
		world->SetPlayerSpawnPosition(Vector3(WORLD_SIZE / 2, world->groundHeight(WORLD_SIZE / 2, WORLD_SIZE / 2) + world->playerGrowth - 0.05f + 1, WORLD_SIZE / 2));
	}
	if (worldType == FlatDesertLakes)
	{
		loading->readiness = 1;
		loading->stateName = 1;
		int superflatWorldHeight = 46;
		for (int x = 0; x < WORLD_SIZE; x++)
		{
			loading->readiness = ((float)x / WORLD_SIZE) * 50 + 1;
			for (int z = 0; z < WORLD_SIZE; z++)
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
		world->SetPlayerSpawnPosition(Vector3(WORLD_SIZE / 2, world->groundHeight(WORLD_SIZE / 2, WORLD_SIZE / 2) + world->playerGrowth - 0.05f + 1, WORLD_SIZE / 2));
	}
	if (worldType == FlatOcean)
	{
		loading->readiness = 1;
		loading->stateName = 1;
		int superflatWorldHeight = 32;
		for (int x = 0; x < WORLD_SIZE; x++)
		{
			loading->readiness = ((float)x / WORLD_SIZE) * 50 + 1;
			for (int z = 0; z < WORLD_SIZE; z++)
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
		world->SetPlayerSpawnPosition(Vector3(WORLD_SIZE / 2, world->groundHeight(WORLD_SIZE / 2, WORLD_SIZE / 2) + world->playerGrowth - 0.05f + 1, WORLD_SIZE / 2));
	}
    loading->KillLoadingScreen();
    delete loading;
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

float WorldGenerator::InterpolateElevationNoise(int wx, int wz)
{
	float q1, q2, q3, q4, xinterp, yinterp, r1, r2, r3;
	int minx, maxx, minz, maxz;

	minx = floor(wx/4.0f);
	maxx = ceil(wx/4.0f);

	minz = floor(wz/4.0f);
	maxz = ceil(wz/4.0f);

	q1 = GetValue2D(minx,minz,1,0.1f,1.0f);
	q2 = GetValue2D(maxx,minz,1,0.1f,1.0f);
	q3 = GetValue2D(minx,maxz,1,0.1f,1.0f);
	q4 = GetValue2D(maxx,maxz,1,0.1f,1.0f);

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

	q1 = GetValue2D(minx,minz,3,0.1f,1.0f);
	q2 = GetValue2D(maxx,minz,3,0.1f,1.0f);
	q3 = GetValue2D(minx,maxz,3,0.1f,1.0f);
	q4 = GetValue2D(maxx,maxz,3,0.1f,1.0f);

	xinterp = (wx/2.0f - minx);
	yinterp = (wz/2.0f - minz);

	r1 = func1.Linear(q1,q2,xinterp);
	r2 = func1.Linear(q3,q4,xinterp);

	r3 = func1.Linear(r1,r2,yinterp);

	return r3;
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
    int num = 13 + rand() % 8;

    for(int i = 0; i < num; i++)
    {
        int x = 6 + rand() % WORLD_SIZE-9;
        int z = 6 + rand() % WORLD_SIZE-9;
        int y = world->groundHeight(x,z);

        if(y <= 60)
        {
            i--;
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
    int num = 2 + rand() % 2;

    for(int i = 0; i < num; i++)
    {
        int x = 9 + rand() % WORLD_SIZE-12;
        int z = 9 + rand() % WORLD_SIZE-12;
        int y = 0;

        while(world->groundHeight(x,z) < 70)
        {
            x = 9 + rand() % WORLD_SIZE-12;
            z = 9 + rand() % WORLD_SIZE-12;
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
    char lavaLakesNum = 7 + rand() % 4;

    for(int i = 0; i <= lavaLakesNum; i++)
    {
        int x = 8 + rand() % WORLD_SIZE-9;
        int z = 8 + rand() % WORLD_SIZE-9;
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
            i--;
            continue;
        }
    }
}

void WorldGenerator::initWaterLakes(int WORLD_SIZE, CraftWorld *world)
{
    char waterLakesNum = 10 + rand() % 7;

    for(int i = 0; i <= waterLakesNum; i++)
    {
        int x = 5 + rand() % WORLD_SIZE-9;
        int z = 5 + rand() % WORLD_SIZE-9;
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
            i--;
            continue;
        }
    }
}

void WorldGenerator::initBiome(int WORLD_SIZE, CraftWorld *world, PerlinModule* perlin, int seed)
{
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
    int biomeCenterX = WORLD_SIZE/2+sinf(angle)*(126);
    int biomeCenterZ = WORLD_SIZE/2+cosf(angle)*(126);
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

        biomeCenterX = WORLD_SIZE/2+sinf(angle)*(126+rad);
        biomeCenterZ = WORLD_SIZE/2+cosf(angle)*(126+rad);
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
    for (int z = 1; z < WORLD_SIZE-1; z++)
    {
        for (int x = 1; x < WORLD_SIZE-1; x++)
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
	for (int z = 1; z < WORLD_SIZE; ++z)
    {
        for (int x = 1; x < WORLD_SIZE; ++x)
        {
            world->GetBlock(x, 2, z) = bedrock::getID();//last one in set
        }
    }

    for (int z = 1; z < WORLD_SIZE; ++z)
    {
        for (int x = 1; x < WORLD_SIZE; ++x)
        {
            short r = rand() % 2;
            if (r == 1)
            {
                world->GetBlock(x, 3, z) = bedrock::getID();//last one in set
            }
        }
    }

    for (int z = 1; z < WORLD_SIZE; ++z)
    {
        for (int x = 1; x < WORLD_SIZE; ++x)
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
    for (int z = 0; z < WORLD_SIZE; ++z)
    {
        for (int x = 0; x < WORLD_SIZE; ++x)
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

    bool mooshroomTree = 1*(rand() % 12 == 0);
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
    int treesNum = 256+rand()%51;


    for (int i = 0; i < treesNum; i ++)
    {
        int x = 3+rand()%(WORLD_SIZE-4);
        int z = 3+rand()%(WORLD_SIZE-4);
        int y = world->groundHeight(x,z);

        if(y >= 85)
        {
            i--;
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
    char woodBlock;
    char leavesBlock;
    bool snowTree = false;

    int treesNum = 1700;

    for (int i = 0; i < treesNum; i ++)
    {
        int x = 5+rand()%(WORLD_SIZE-6);
        int z = 5+rand()%(WORLD_SIZE-6);
        int y = world->groundHeight(x,z);

        if(y >= 85)
        {
            i--;
            continue;
        }

        perlinHeight = dataTrees[x + z*WORLD_SIZE]* 128.0f/12.0f + 69;
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
    int NumPumpkinsSpawns = 12 + rand() % 4;
    int Frequency = 4; // 1 - 100

    for (int i = 0; i < NumPumpkinsSpawns; ++i)
    {
        //Choose the tpumpkin position
        int x = rand() % WORLD_SIZE;
        int z = rand() % WORLD_SIZE;
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
    int num = 32 + rand() % 10;

    for (int i = 0; i < num; ++i)
    {
        int centerX = 10 + rand() % (WORLD_SIZE-10);
        int centerZ = 10 + rand() % (WORLD_SIZE-10);

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

    int NumTypes = 600;

    for (int i = 0; i < NumTypes; ++i)
    {
        //Choose the tpumpkin position
        int x2 = rand() % WORLD_SIZE;
        int z2 = rand() % WORLD_SIZE;


        if(world->GetBlock(x2, world->groundHeight(x2, z2)+1, z2 ) == 0 && world->GetBlock(x2, world->groundHeight(x2, z2), z2 ) == 1)
        {
            world->GetBlock(x2, world->groundHeight(x2, z2)+1, z2 ) = 116;
        }
    }
}

void WorldGenerator::initOtherVegetation(int WORLD_SIZE, CraftWorld *world)
{

    int NumTypes = 100 + rand() % 100;

    for (int i = 0; i < NumTypes; ++i)
    {
        //Choose the tpumpkin position
        int x2 = rand() % WORLD_SIZE;
        int z2 = rand() % WORLD_SIZE;


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
    // oreConcentration - var between 0 and 100

    oreConcentration = oreConcentration / 100.0f; // percents
    int oreAmount = oreConcentration*(WORLD_SIZE*WORLD_SIZE*(oreHighestBoundary-oreLowestBoundary));

    switch(oreID)
    {
        case 41: world->mainWorldVariables.worldIronOre = oreAmount;
        case 42: world->mainWorldVariables.worldCoalOre = oreAmount;
        case 43: world->mainWorldVariables.worldDiamondOre = oreAmount;
        case 44: world->mainWorldVariables.worldRedstoneOre = oreAmount;
        case 45: world->mainWorldVariables.worldRedstoneOre = oreAmount;
        case 5: world->mainWorldVariables.worldGoldenOre = oreAmount;
    }

    while(oreAmount > 0)
    {
        int x = 1+rand()%(WORLD_SIZE-2);
        int y = oreLowestBoundary+rand()%(oreHighestBoundary-oreLowestBoundary+1);
        int z = 1+rand()%(WORLD_SIZE-2);

        oreAmount -= GenerateOreVine(WORLD_SIZE,world,x,y,z,oreID,rand()%variations);
    }
}

void WorldGenerator::initClay(int WORLD_SIZE, CraftWorld *world)
{
    int NumOres = 40 + rand() % 20;

    for (int i = 0; i < NumOres; ++i)
    {
        //Choose the tpumpkin positionh
        int x = rand() % WORLD_SIZE;
        int z = rand() % WORLD_SIZE;
        int y = world->groundHeight(x,z);

        if(y >= 63)
        {
            i--;
            continue;
        }
        float radius = 1.5+(rand()%12)/10.0f;

        world->BuildBlockSphere(radius,ClayBlock::getID(),x,y+1,z,SandBlock::getID());
    }
}



void WorldGenerator::initDirt(int WORLD_SIZE, CraftWorld *world)
{
    int NumBlocks = 1500;
    float radius = 2.0f + (rand() % 35)/10.0f;

    short PlaceBlock = DirtBlock::getID();

    for (int i = 0; i < NumBlocks; ++i)
    {
        int x = rand() % WORLD_SIZE;
        int z = rand() % WORLD_SIZE;
        int y = 4 + rand() % (world->groundHeight(x,z)-1);

        world->BuildBlockSphere(radius, PlaceBlock, x, y, z, RockBlock::getID());

    }
}

void WorldGenerator::initGravel(int WORLD_SIZE, CraftWorld *world)
{
    int NumBlocks = 400;
    float radius = 2.0f + (rand() % 30)/10.0f;

    short PlaceBlock = RockBlock::getID();

    for (int i = 0; i < NumBlocks; ++i)
    {
        int x = rand() % WORLD_SIZE;
        int z = rand() % WORLD_SIZE;
        int y = 4 + rand() % (world->groundHeight(x,z)-1);

        world->BuildBlockSphere(radius, PlaceBlock, x, y, z, RockBlock::getID());
    }
}


void WorldGenerator::initDungeons(int WORLD_SIZE, CraftWorld *world)
{
    int xSize, ySize, zSize;
    int x, y, z;

    ySize = 5;

    for(int t = 0; t <= 4; t++)
    {
        xSize = 5+rand()%3;
        zSize = 5+rand()%3;

        x = 1 + rand() % (WORLD_SIZE-8);
        z = 1 + rand() % (WORLD_SIZE-8);

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