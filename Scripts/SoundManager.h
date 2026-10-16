#ifndef SOUNDMANAGER_H_
#define SOUNDMANAGER_H_

extern "C"
{
    #include "sound_utils/pgeWav.h"
    #include "sound_utils/oslib.h"
}

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Aurora/Utils/Logger.h>

#include<fstream>
#include<iostream>


class SoundManager
{
public:
	SoundManager();
	~SoundManager();

	static SoundManager m_SoundManager;
	static SoundManager* Instance();

	void Init();
	void AudioSync();

    void PlayBreakSound();
    void PlayHitSound();
    void PlayEatSound();

    void PlayZombieHurtSound(float distanceToPlayer);
    void PlayZombieDieSound(float distanceToPlayer);
    void PlayZombieSaySound(float distanceToPlayer);

    void PlayCowHurtSound(float distanceToPlayer);
    void PlayCowSaySound(float distanceToPlayer);

    void PlayPigHurtSound(float distanceToPlayer);
    void PlayPigSaySound(float distanceToPlayer);

    void PlayCreeperHurtSound(float distanceToPlayer);
    void PlaySheepHurtSound(float distanceToPlayer);

    void PlayChickenHurtSound(float distanceToPlayer);
    void PlayChickenSaySound(float distanceToPlayer);
    void PlayChickenStepSound(float distanceToPlayer);
    void PlayChickenPlopSound(float distanceToPlayer);

    // Spider: four idles, four footfalls and a death rattle. 1.7.3 gives the spider no hurt clip --
    // getHurtSound() points at mob.spider.say -- so the hurt path picks one of the say clips.
void PlaySpiderSaySound(float distanceToPlayer);
void PlaySpiderHurtSound(float distanceToPlayer);
void PlaySpiderStepSound(float distanceToPlayer);
void PlaySpiderDeathSound(float distanceToPlayer);

// Skeleton: three idles, four hurts, four footfalls and a death rattle. Unlike the spider this mob
// does have its own hurt clips -- 1.7.3 getHurtSound() is mob.skeleton.hurt -- so the hurt path uses
// them rather than borrowing the idle clips.
void PlaySkeletonSaySound(float distanceToPlayer);
void PlaySkeletonHurtSound(float distanceToPlayer);
void PlaySkeletonStepSound(float distanceToPlayer);
void PlaySkeletonDeathSound(float distanceToPlayer);


    void PlayRainSound(int volume);
    void StopRainSound();
    bool RainSoundPlaying();

    void PlayFissSound();
    void PlaySplashSound();
    void PlayFuseSound();
    void PlayShearSound();
	void PlayFlintSound();
    void PlayBowSound();
    void PlayBowSoundAt(float distanceToPlayer);

    // Arrow impact. Vanilla gives the two cases different samples, so these are two calls rather
    // than one: a block hit is the dry rattle of one of the four random/bowhit clips, and a hit on
    // a body -- a mob the player shot, or the player taking a skeleton's arrow -- is
    // random/successful_hit. Both take the distance to the player, like the mob sounds do, so a
    // shot that lands across the valley is quiet instead of being played at the player's ear.
    void PlayArrowHitSound(float distanceToPlayer);
    void PlayArrowFleshSound(float distanceToPlayer);

	void PlayMenuSound();
	void PlayMenuSound2();
	void PlayMenuSound3();
	void PlayPlopSound();
	// Redstone button press/spring-back. Stone uses vanilla random/click, wood uses
	// vanilla random/wood_click (both pulled from the 1.7.3 assets); wood = true selects wood.
	void PlayButtonSound(bool wood);
	void PlayWalkSound(int type);
	void PlayDigSound(int type);
	void PlayEndDigSound(int type);
	void TNTSound();
	void PlayDoorSound(bool open);
	// Piston extend/retract. Java plays tile.piston.out when a piston extends and tile.piston.in
	// when it retracts, both at volume 0.5 and pitch ~0.6; extend = true selects the former.
	void PlayPistonSound(bool extend);
	void PlayFallSound(float i);
	void PlayGlassSound();
    void PlayNoteSound(unsigned int noteType, float pitch);

    void PlayDiskTrack(int diskNumber);
    void StopDiskTrack();

	int PlayRandomAmbient();

	void StopAmbient();
	void PlayAmbient(std::string name);

	bool playerSounds;
	bool compactSounds;   // true on 32 MB PSPs: SFX come from Assets/SoundsLow (22 kHz mono)
	bool ambientSoundsEnabled;

	int currentAmbientSound;
	int lastAmbientSound;

	bool delayAmbient;

private:

	int currentWalkSound;
	int lastWalkSound;

	//sounds
	pgeWav *buttonSound;      // GUI / menu click (random.click as the menu uses it)
	pgeWav *buttonSound2;
	pgeWav *buttonSound3;
	pgeWav *buttonStone;      // redstone button: vanilla random/click
	pgeWav *buttonWood;       // redstone button: vanilla random/wood_click
	//plop sound
	pgeWav *plopSound;
	//grass sounds
	pgeWav *grassSounds[4];
	pgeWav *grawelSounds[4];
	pgeWav *stoneSounds[4];
	pgeWav *woodSounds[4];
	pgeWav *sandSounds[4];
	pgeWav *snowSounds[4];

	pgeWav *eatSounds[3];

	pgeWav *hitSounds[3];
    pgeWav *fallSounds[2];

    pgeWav *zombieHurtSounds[2];
    pgeWav *zombieSaySound;
    pgeWav *zombieDieSound;

    pgeWav *cowHurtSounds[2];
    pgeWav *cowSaySound;

    pgeWav *pigHurtSounds[2];
    pgeWav *pigSaySound;

    pgeWav *creeperHurtSounds[2];

    pgeWav *sheepHurtSounds[2];

    pgeWav *chickenHurtSounds[2];
    pgeWav *chickenSaySounds[3];
    pgeWav *chickenStepSounds[2];
    pgeWav *chickenPlopSound;

    pgeWav *spiderSaySounds[4];
    pgeWav *spiderStepSounds[4];
    pgeWav *spiderDeathSound;

    pgeWav *skeletonSaySounds[3];
    pgeWav *skeletonHurtSounds[4];
    pgeWav *skeletonStepSounds[4];
    pgeWav *skeletonDeathSound;


	pgeWav *doorOpen;
	pgeWav *doorClose;
	// Piston actuation: vanilla tile.piston.out / tile.piston.in.
	pgeWav *pistonOut;
	pgeWav *pistonIn;
	// Alternates the piston's pitch between two of vanilla's values -- see PlayPistonSound.
	bool pistonPitchFlip;
	pgeWav *tnt;
	pgeWav *bow;
	// Arrow impact. Four block-impact clips, picked at random, plus the single body-impact sample.
	pgeWav *bowHitSounds[4];
	pgeWav *bowHitFleshSound;
	pgeWav *breakSound;
	pgeWav *fuse;
	pgeWav *fiss;
	pgeWav *splash;
	pgeWav *glass;
	pgeWav *shear;
	pgeWav *flint;

	pgeWav *rainSound;

	pgeWav *digSounds[3];

	pgeWav *noteSounds[2]; // bass and pling

	// this lib can stream
	OSL_SOUND *ambientTrack;
	std::string ambientTrackName;

	OSL_SOUND *diskTrack;
	std::string diskTrackName;

};

#endif