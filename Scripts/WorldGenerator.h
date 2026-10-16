#ifndef WorldGenerator_H
#define WorldGenerator_H

#include <Aurora/Math/Frustum.h>
#include <math.h>
#include <psptypes.h>

#include <noisepp/Noise.h>
#include <noisepp/NoiseUtils.h>
#include <noisepp/NoiseBuilders.h>

#include <pspiofilemgr.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <zlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#include "SimplexNoise.h"
#include "Chest2.h"
#include "MonsterSpawnerEntity.h"



#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <psppower.h>
#include <psptypes.h>
#include <pspge.h>
#include <psputils.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include <stdarg.h>

class CraftWorld;
using namespace noisepp;

class WorldGenerator
{
	public:
		void initRandompMap(int worldSize, int worldHeight, int chunkSize, CraftWorld *world, bool makeTrees,bool makeWater,bool makeCaves, unsigned int seedII, int worldType);
		void GenerateColumn(CraftWorld *world, int cx, int cz);
		// One slice of GenerateColumn. `step` is how far this column has already been built; a budget of
		// 0 runs until the column is finished. Returns true when nothing is left.
		bool GenerateColumnSlice(CraftWorld *world, int cx, int cz, int &step, unsigned int startUs, unsigned int budgetUs);
		void initTrees(int WORLD_SIZE,  CraftWorld *world, PerlinModule* perlin, int seed);
		void initRandomTrees(int WORLD_SIZE,  CraftWorld *world);

		void initPumpkins(int WORLD_SIZE,  CraftWorld *world);

		void initFlowers(int WORLD_SIZE, CraftWorld *world);
		void initGrass(int WORLD_SIZE, CraftWorld *world);
		void initOtherVegetation(int WORLD_SIZE, CraftWorld *world);

		void initLavaPools(int WORLD_SIZE, CraftWorld *world);
		void initBiome(int WORLD_SIZE, CraftWorld *world, PerlinModule* perlin, int seed);

		void initCanes(int WORLD_SIZE, CraftWorld *world, int waterLevel);
		void initBedrock(int WORLD_SIZE, CraftWorld *world);
        void initBeachSand(int WORLD_SIZE, CraftWorld *world);

        void initErosion(int WORLD_SIZE, CraftWorld *world);

        void initOre(int WORLD_SIZE, CraftWorld *world, int oreID, float oreConcentration, int oreLowestBoundary, int oreHighestBoundary, int variations);

		void initDungeons(int WORLD_SIZE, CraftWorld *world);

        void initClay(int WORLD_SIZE, CraftWorld *world);
		void initDirt(int WORLD_SIZE, CraftWorld *world);
		void initGravel(int WORLD_SIZE, CraftWorld *world);
		void initLavaLakes(int WORLD_SIZE, CraftWorld *world);
        void initWaterLakes(int WORLD_SIZE, CraftWorld *world);

        int GenerateOreVine(int WORLD_SIZE, CraftWorld *world, int x, int y, int z, int oreID, int type);

        void GeneratePileOfLeaves(CraftWorld *world, int x, int y, int z);
        void GenerateClassicTree(int WORLD_SIZE, CraftWorld *world, int x, int y, int z, int trunkBlock, int leavesBlock);
        void GenerateSpruceTree(int WORLD_SIZE, CraftWorld *world, int x, int y, int z);

        float InterpolateBiomeNoise(int wx, int wz);
        float InterpolateMoistureNoise(int wx, int wz);
        float InterpolateElevationNoise(int wx, int wz);
        float InterpolateRoughnessNoise(int wx, int wz);
        // Filled for the column being generated. Elevation and roughness sample a coarse grid and
        // interpolate; doing that per block repeated the same noise calls across the whole 16x16.
        void PrepareColumnNoise(int x0, int z0, int chunkSize);
        float SampleElevation(int ix, int iz);
        float SampleRoughness(int ix, int iz);
        int DefaultTerrainHeight(int x, int z, int worldHeight);
        void PlaceDefaultTerrainColumn(CraftWorld *world, int x, int z, int worldType, int waterLevel, int worldHeight);
        void PlaceTerrainColumn(CraftWorld *world, int x, int z, int Height, int biome, int waterLevel, int worldHeight);

        // Biomes are picked per block from temperature / moisture noise, so spawn and
        // streamed columns agree and biome edges are jagged instead of circles.
        enum Biome { BIOME_PLAINS = 0, BIOME_FOREST = 1, BIOME_DESERT = 2, BIOME_SNOW = 3 };
        int BiomeAt(int x, int z, int worldType);
        // useCache: colHeights/colBiomes were just filled by GenerateColumn for this column
        void DecorateColumn(CraftWorld *world, int cx, int cz, bool useCache = false);
        void DecorateSpawnRegion(CraftWorld *world);
        // The nether: one solid netherrack mass with caverns carved out of it, a lava sea at
        // NETHER_LAVA_LEVEL, and bedrock floor and ceiling. Used both for a streamed column and for
        // the spawn region, so the two cannot disagree.
        void GenerateNetherColumn(CraftWorld *world, int cx, int cz);
        // Nearest netherrack floor with headroom above it, above the lava sea. Returns false if
        // there is none anywhere near the middle of the world.
        bool FindNetherSpawn(CraftWorld *world, int &outX, int &outY, int &outZ);
        int colHeights[16][16];
        int colBiomes[16][16];
        bool noiseGridOn;
        int elevX0, elevZ0, elevNX, elevNZ;
        float elevGrid[8][8];
        int roughX0, roughZ0, roughNX, roughNZ;
        float roughGrid[12][12];

        SimplexNoise func1,func2,func3;

        float GetValue3D(int wx, int wy, int wz, int octaves, float startFrequency, float startAmplitude);
        float GetValue2D(int wx, int wz, int octaves, float startFrequency, float startAmplitude);
        float GetValue2D2(int wx, int wz, int octaves, float startFrequency, float startAmplitude);
        float GetValue2D3(int wx, int wz, int octaves, float startFrequency, float startAmplitude);
        void SetSeed(int seed);
};

#endif
