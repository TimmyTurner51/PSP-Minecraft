#include "SoundManager.h"
#include "CraftWorld2.h"
#include "DebugConfig.h"
#include "InitTrace.h"
#include <string>

using namespace Aurora;

SoundManager SoundManager::m_SoundManager;

// All SFX stay resident in RAM as raw PCM (6.6 MB at 44.1 kHz stereo). That is a
// third of a PSP-1000's heap, so on low-memory hardware the same clips are loaded
// from Assets/SoundsLow/ (22 kHz mono, ~1.6 MB). 2000/3000 keep the full-quality set.
static bool gCompactSounds = false;

// Java plays a button at volume 0.3 (playSoundEffect "random.click"/"random.wood_click").
// pgeWav takes 0-100, so this keeps the same loudness ratio the block-break hits (played
// at full volume) have in Java.
static const unsigned char kButtonClickVolume = 30;

// Java plays the piston at volume 0.5 (BlockPistonBase.onBlockEvent), which on the same 0-1 scale
// the button's 0.3 is quoted from is 50 here.
static const unsigned char kPistonSoundVolume = 50;

static pgeWav* LoadSfx(const char* path)
{
    if(gCompactSounds)
    {
        std::string p(path);
        size_t pos = p.find("Assets/Sounds/");
        if(pos != std::string::npos)
        {
            std::string low = p.substr(0, pos) + "Assets/SoundsLow/" + p.substr(pos + 14);
            MC_TraceStr("sfx: trying low", low.c_str());
            pgeWav* w = pgeWavLoad(low.c_str());
            if(w != NULL)
            {
                MC_TraceStr("sfx: loaded low", low.c_str());
                return w;
            }
            MC_TraceStr("sfx: low missing, trying full", low.c_str());
        }
    }

    // Names the clip before reading it, so the line that follows proves this one finished -- the same
    // breadcrumb-before-the-risky-work rule as BootTrace::Stage, at the granularity of one file. There
    // are about a hundred of these, so this is one line each rather than two: a successful load is
    // evidenced by the next entry, and only a failure needs to say so in its own words.
    MC_TraceStr("sfx: loading full", path);

    pgeWav* result = pgeWavLoad(path);

    if(result == NULL)
        MC_TraceStr("sfx: NULL result", path);
    else
        MC_TraceStr("sfx: loaded full", path);

    return result;
}

SoundManager* SoundManager::Instance()
{
	return &m_SoundManager;
}

SoundManager::SoundManager()
{
	// See the same line in TextureHelper's constructor: global constructors run before main() and before
	// Log.txt, so the trace file is the only record of how far the constructor phase got.
	MC_Trace("ctor: SoundManager entering");
    pistonPitchFlip = false;
	MC_Trace("ctor: SoundManager done");
}

SoundManager::~SoundManager()
{
    pgeWavStopAll();

    pgeWavUnload(rainSound);
    pgeWavUnload(buttonSound);
	pgeWavUnload(buttonSound2);
	pgeWavUnload(buttonSound3);
	pgeWavUnload(buttonStone);
	pgeWavUnload(buttonWood);
    pgeWavUnload(tnt);
    pgeWavUnload(doorOpen);
    pgeWavUnload(doorClose);
    pgeWavUnload(pistonOut);
    pgeWavUnload(pistonIn);
    pgeWavUnload(breakSound);
    pgeWavUnload(bow);
    for(int i = 0; i < 4; i++)
    {
        pgeWavUnload(bowHitSounds[i]);
    }
    pgeWavUnload(bowHitFleshSound);
    pgeWavUnload(plopSound);
    pgeWavUnload(chickenPlopSound);
    pgeWavUnload(spiderDeathSound);
    pgeWavUnload(zombieDieSound);
    pgeWavUnload(cowSaySound);
    pgeWavUnload(pigSaySound);
    pgeWavUnload(fuse);
    pgeWavUnload(fiss);
    pgeWavUnload(splash);
    pgeWavUnload(glass);
    pgeWavUnload(zombieSaySound);
    pgeWavUnload(shear);
	pgeWavUnload(flint);

    for(int i = 0; i <= 3; i++)
    {
        pgeWavUnload(grassSounds[i]);
        pgeWavUnload(grawelSounds[i]);
        pgeWavUnload(stoneSounds[i]);
        pgeWavUnload(woodSounds[i]);
        pgeWavUnload(sandSounds[i]);
        pgeWavUnload(snowSounds[i]);
    }
    for(int i = 0; i <= 2; i++)
    {
        pgeWavUnload(eatSounds[i]);
        pgeWavUnload(hitSounds[i]);
        pgeWavUnload(digSounds[i]);
    }
    for(int i = 0; i <= 1; i++)
    {
        pgeWavUnload(fallSounds[i]);
        pgeWavUnload(cowHurtSounds[i]);
        pgeWavUnload(pigHurtSounds[i]);
        pgeWavUnload(creeperHurtSounds[i]);
        pgeWavUnload(zombieHurtSounds[i]);
        pgeWavUnload(sheepHurtSounds[i]);
        pgeWavUnload(chickenHurtSounds[i]);
        pgeWavUnload(chickenStepSounds[i]);
        pgeWavUnload(noteSounds[i]);
    }
    for(int i = 0; i < 3; i++)
    {
        pgeWavUnload(chickenSaySounds[i]);
    }
    for(int i = 0; i < 4; i++)
    {
        pgeWavUnload(spiderSaySounds[i]);
        pgeWavUnload(spiderStepSounds[i]);
    }
    pgeWavUnload(skeletonDeathSound);
    for(int i = 0; i < 3; i++)
    {
        pgeWavUnload(skeletonSaySounds[i]);
    }
    for(int i = 0; i < 4; i++)
    {
        pgeWavUnload(skeletonHurtSounds[i]);
        pgeWavUnload(skeletonStepSounds[i]);
    }
}

void SoundManager::Init()
{
    //first of all init audio engine
    MC_Trace("snd: VirtualFileInit enter");
    VirtualFileInit();
    MC_Trace("snd: VirtualFileInit done");
   // oslInitAudioME(OSL_FMT_MP3);
    // The audio engine is the one subsystem in startup that owns real hardware (the audio block) and
    // starts its own threads, so it is traced on both sides of every call.
	MC_Trace("snd: oslInitAudio enter");
	oslInitAudio();
	MC_Trace("snd: oslInitAudio done");
    MC_Trace("snd: pgeWavInit enter");
	pgeWavInit();
	MC_Trace("snd: pgeWavInit done");

    //set default values
	currentWalkSound = 0;
	lastWalkSound = 0;
	playerSounds = true;
	lastAmbientSound = 0;
	currentAmbientSound = 0;
	ambientSoundsEnabled = true;
	srand(time(NULL));
	MC_Trace("snd: defaults set");

	// textures are already loaded at this point; a 64 MB PSP still has ~40 MB of heap
	// free here, a 32 MB PSP-1000 about 12 MB
	MC_Trace("snd: HeapFreeKB query");
	int heapFreeKB = CraftWorld::HeapFreeKB();
	MC_TraceInt("snd: heapFreeKB", heapFreeKB);
#if MC_FORCE_COMPACT_SOUNDS
	// Local reproduction switch. The hardware picks the low-memory clip set from the heap it has, and a
	// desktop emulator has so much more of it that it always picks the full-quality one -- so the whole
	// SoundsLow path is never executed there. This forces the same choice the console makes, so a fault
	// in that path can be found and fixed without a round trip to the device.
	gCompactSounds = true;
	MC_Trace("snd: compactSounds FORCED ON (MC_FORCE_COMPACT_SOUNDS)");
#else
	gCompactSounds = heapFreeKB < 24 * 1024;
#endif
	compactSounds = gCompactSounds;
	MC_TraceInt("snd: compactSounds", gCompactSounds ? 1 : 0);
	Aurora::Utils::Logger::Instance()->LogMessage("sound init heapFreeKB=%d compactSounds=%d\n", heapFreeKB, gCompactSounds ? 1 : 0);
	MC_Trace("snd: starting sfx loads");

	//button sound
	buttonSound = LoadSfx("Assets/Sounds/other/button1.wav");
	buttonSound2 = LoadSfx("Assets/Sounds/other/button2.wav");
    buttonSound3 = LoadSfx("Assets/Sounds/other/button3.wav");

	// Redstone buttons. vanilla 1.7.3 only ever plays "random.click" for a button (wooden
	// buttons differ by pitch alone), and random/click is the same sample the GUI uses.
	// button1.wav in this project is not that sample -- it is a softer, duller menu click,
	// which is why the redstone buttons sounded like GUI buttons. These are the real
	// 1.7.3 samples converted to WAV: random/click.ogg and random/wood_click.ogg.
	buttonStone = LoadSfx("Assets/Sounds/other/button_stone.wav");
	buttonWood = LoadSfx("Assets/Sounds/other/button_wood.wav");

	//TNT
	tnt = LoadSfx("Assets/Sounds/other/tnt.wav");
	fuse = LoadSfx("Assets/Sounds/other/fuse.wav");
    fiss = LoadSfx("Assets/Sounds/other/fizz.wav");
	splash = LoadSfx("Assets/Sounds/other/splash.wav");
	flint = LoadSfx("Assets/Sounds/other/flint.wav");

	doorOpen = LoadSfx("Assets/Sounds/other/door_open.wav");
	doorClose = LoadSfx("Assets/Sounds/other/door_close.wav");

	// Piston extend/retract, imported from the 1.7.3 assets by tools/make-vanilla-piston-sounds.py:
	// sounds/tile/piston/out.ogg and in.ogg, the two samples behind Java's tile.piston.out/in.
	pistonOut = LoadSfx("Assets/Sounds/other/piston_out.wav");
	pistonIn = LoadSfx("Assets/Sounds/other/piston_in.wav");

	breakSound = LoadSfx("Assets/Sounds/other/break.wav");

	rainSound = LoadSfx("Assets/Sounds/other/rain.wav");

	bow = LoadSfx("Assets/Sounds/other/bow.wav");

	// Arrow impact, imported from the 1.7.3 assets by tools/make-vanilla-bowhit-sounds.py. The four
	// bowhit clips are the block impact and the flesh one is a hit on a body; see the header.
	bowHitSounds[0] = LoadSfx("Assets/Sounds/other/bowhit1.wav");
	bowHitSounds[1] = LoadSfx("Assets/Sounds/other/bowhit2.wav");
	bowHitSounds[2] = LoadSfx("Assets/Sounds/other/bowhit3.wav");
	bowHitSounds[3] = LoadSfx("Assets/Sounds/other/bowhit4.wav");
	bowHitFleshSound = LoadSfx("Assets/Sounds/other/bowhit_flesh.wav");

	shear = LoadSfx("Assets/Sounds/mobs/sheep/shear.wav");

	//plop sound
	plopSound = LoadSfx("Assets/Sounds/other/plop.wav");
	pgeWavVolume(plopSound,50,50);

    //fall sounds
    fallSounds[0] = LoadSfx("Assets/Sounds/Damage/fallbig.wav");
    fallSounds[1] = LoadSfx("Assets/Sounds/Damage/fallsmall.wav");

    //note sounds
    noteSounds[0] = LoadSfx("Assets/Sounds/notes/bass.wav");
    noteSounds[1] = LoadSfx("Assets/Sounds/notes/pling.wav");
    pgeWavVolume(noteSounds[0],85,85);
    pgeWavVolume(noteSounds[1],85,85);

    //zombie sounds
    zombieHurtSounds[0] = LoadSfx("Assets/Sounds/mobs/zombie/hurt1.wav");
    zombieHurtSounds[1] = LoadSfx("Assets/Sounds/mobs/zombie/hurt2.wav");
    zombieSaySound = LoadSfx("Assets/Sounds/mobs/zombie/say1.wav");
    zombieDieSound = LoadSfx("Assets/Sounds/mobs/zombie/death.wav");

    //cow sounds
    cowHurtSounds[0] = LoadSfx("Assets/Sounds/mobs/cow/hurt1.wav");
    cowHurtSounds[1] = LoadSfx("Assets/Sounds/mobs/cow/hurt2.wav");
    cowSaySound = LoadSfx("Assets/Sounds/mobs/cow/say1.wav");

    //pig sounds (from Java 1.7.3 mob/pig)
    pigHurtSounds[0] = LoadSfx("Assets/Sounds/mobs/pig/hurt1.wav");
    pigHurtSounds[1] = LoadSfx("Assets/Sounds/mobs/pig/hurt2.wav");
    pigSaySound = LoadSfx("Assets/Sounds/mobs/pig/say1.wav");

    //creeper sounds
    creeperHurtSounds[0] = LoadSfx("Assets/Sounds/mobs/creeper/hurt1.wav");
    creeperHurtSounds[1] = LoadSfx("Assets/Sounds/mobs/creeper/hurt2.wav");

    //sheep sounds
    sheepHurtSounds[0] = LoadSfx("Assets/Sounds/mobs/sheep/say1.wav");
    sheepHurtSounds[1] = LoadSfx("Assets/Sounds/mobs/sheep/say2.wav");

    //chicken sounds (from Java 1.7.3 mob/chicken)
    chickenHurtSounds[0] = LoadSfx("Assets/Sounds/mobs/chicken/hurt1.wav");
    chickenHurtSounds[1] = LoadSfx("Assets/Sounds/mobs/chicken/hurt2.wav");
    chickenSaySounds[0] = LoadSfx("Assets/Sounds/mobs/chicken/say1.wav");
    chickenSaySounds[1] = LoadSfx("Assets/Sounds/mobs/chicken/say2.wav");
    chickenSaySounds[2] = LoadSfx("Assets/Sounds/mobs/chicken/say3.wav");
    chickenStepSounds[0] = LoadSfx("Assets/Sounds/mobs/chicken/step1.wav");
    chickenStepSounds[1] = LoadSfx("Assets/Sounds/mobs/chicken/step2.wav");
    // Java plays "mob.chicken.plop" when a chicken lays an egg. It is not the generic
    // Assets/Sounds/other/plop.wav that PlayPlopSound() uses for item pickups.
    chickenPlopSound = LoadSfx("Assets/Sounds/mobs/chicken/plop.wav");

    //spider sounds (from Java 1.7.3 mob/spider). The tree has no legacy spider clips to keep in
    // step with, so all four says and all four steps are the vanilla assets.
    spiderSaySounds[0] = LoadSfx("Assets/Sounds/mobs/spider/say1.wav");
    spiderSaySounds[1] = LoadSfx("Assets/Sounds/mobs/spider/say2.wav");
    spiderSaySounds[2] = LoadSfx("Assets/Sounds/mobs/spider/say3.wav");
    spiderSaySounds[3] = LoadSfx("Assets/Sounds/mobs/spider/say4.wav");
    spiderStepSounds[0] = LoadSfx("Assets/Sounds/mobs/spider/step1.wav");
    spiderStepSounds[1] = LoadSfx("Assets/Sounds/mobs/spider/step2.wav");
    spiderStepSounds[2] = LoadSfx("Assets/Sounds/mobs/spider/step3.wav");
    spiderStepSounds[3] = LoadSfx("Assets/Sounds/mobs/spider/step4.wav");
    spiderDeathSound = LoadSfx("Assets/Sounds/mobs/spider/death.wav");

    //skeleton sounds (from Java 1.7.3 mob/skeleton): three idles, four hurts, four footfalls, death.
    skeletonSaySounds[0] = LoadSfx("Assets/Sounds/mobs/skeleton/say1.wav");
    skeletonSaySounds[1] = LoadSfx("Assets/Sounds/mobs/skeleton/say2.wav");
    skeletonSaySounds[2] = LoadSfx("Assets/Sounds/mobs/skeleton/say3.wav");
    skeletonHurtSounds[0] = LoadSfx("Assets/Sounds/mobs/skeleton/hurt1.wav");
    skeletonHurtSounds[1] = LoadSfx("Assets/Sounds/mobs/skeleton/hurt2.wav");
    skeletonHurtSounds[2] = LoadSfx("Assets/Sounds/mobs/skeleton/hurt3.wav");
    skeletonHurtSounds[3] = LoadSfx("Assets/Sounds/mobs/skeleton/hurt4.wav");
    skeletonStepSounds[0] = LoadSfx("Assets/Sounds/mobs/skeleton/step1.wav");
    skeletonStepSounds[1] = LoadSfx("Assets/Sounds/mobs/skeleton/step2.wav");
    skeletonStepSounds[2] = LoadSfx("Assets/Sounds/mobs/skeleton/step3.wav");
    skeletonStepSounds[3] = LoadSfx("Assets/Sounds/mobs/skeleton/step4.wav");
    skeletonDeathSound = LoadSfx("Assets/Sounds/mobs/skeleton/death.wav");


	//grass sounds
	grassSounds[0] = LoadSfx("Assets/Sounds/walking/grass/grass1.wav");
	grassSounds[1] = LoadSfx("Assets/Sounds/walking/grass/grass2.wav");
	grassSounds[2] = LoadSfx("Assets/Sounds/walking/grass/grass3.wav");
	grassSounds[3] = LoadSfx("Assets/Sounds/walking/grass/grass4.wav");

	//gravel
	grawelSounds[0] = LoadSfx("Assets/Sounds/walking/gravel/gravel1.wav");
	grawelSounds[1] = LoadSfx("Assets/Sounds/walking/gravel/gravel2.wav");
	grawelSounds[2] = LoadSfx("Assets/Sounds/walking/gravel/gravel3.wav");
	grawelSounds[3] = LoadSfx("Assets/Sounds/walking/gravel/gravel4.wav");

	//stone
	stoneSounds[0] = LoadSfx("Assets/Sounds/walking/stone/stone1.wav");
	stoneSounds[1] = LoadSfx("Assets/Sounds/walking/stone/stone2.wav");
	stoneSounds[2] = LoadSfx("Assets/Sounds/walking/stone/stone3.wav");
	stoneSounds[3] = LoadSfx("Assets/Sounds/walking/stone/stone4.wav");

	//wood
	woodSounds[0] = LoadSfx("Assets/Sounds/walking/wood/wood1.wav");
	woodSounds[1] = LoadSfx("Assets/Sounds/walking/wood/wood2.wav");
	woodSounds[2] = LoadSfx("Assets/Sounds/walking/wood/wood3.wav");
	woodSounds[3] = LoadSfx("Assets/Sounds/walking/wood/wood4.wav");

    //sand
    sandSounds[0] = LoadSfx("Assets/Sounds/walking/sand/sand1.wav");
	sandSounds[1] = LoadSfx("Assets/Sounds/walking/sand/sand2.wav");
	sandSounds[2] = LoadSfx("Assets/Sounds/walking/sand/sand3.wav");
	sandSounds[3] = LoadSfx("Assets/Sounds/walking/sand/sand4.wav");

    //snow
    snowSounds[0] = LoadSfx("Assets/Sounds/walking/snow/snow1.wav");
	snowSounds[1] = LoadSfx("Assets/Sounds/walking/snow/snow2.wav");
	snowSounds[2] = LoadSfx("Assets/Sounds/walking/snow/snow3.wav");
	snowSounds[3] = LoadSfx("Assets/Sounds/walking/snow/snow4.wav");

	//glass
	glass = LoadSfx("Assets/Sounds/dig/glass1.wav");

	digSounds[0] = LoadSfx("Assets/Sounds/dig/grass1.wav");
	digSounds[1] = LoadSfx("Assets/Sounds/dig/gravel1.wav");
	digSounds[2] = LoadSfx("Assets/Sounds/dig/stone1.wav");

    // Eat sounds
	eatSounds[0] = LoadSfx("Assets/Sounds/eat/eat1.wav");
	eatSounds[1] = LoadSfx("Assets/Sounds/eat/eat2.wav");
	eatSounds[2] = LoadSfx("Assets/Sounds/eat/eat3.wav");

    // Hit sounds
    hitSounds[0] = LoadSfx("Assets/Sounds/Damage/hit1.wav");
    hitSounds[1] = LoadSfx("Assets/Sounds/Damage/hit2.wav");
    hitSounds[2] = LoadSfx("Assets/Sounds/Damage/hit3.wav");

    ambientTrack = NULL;
    ambientTrackName = " ";

    diskTrack = NULL;
    diskTrackName = " ";

    delayAmbient = false;

    MC_Trace("snd: SoundManager::Init complete");
}

void SoundManager::AudioSync()
{

    oslAudioVSync();
}

void SoundManager::PlayMenuSound()
{
	pgeWavPlay(buttonSound);
}

void SoundManager::PlayMenuSound2()
{
	pgeWavPlay(buttonSound2);
}

void SoundManager::PlayMenuSound3()
{
	pgeWavPlay(buttonSound3);
}

void SoundManager::PlayPlopSound()
{
	if(playerSounds)
		pgeWavPlay(plopSound);
}

// Redstone button press and spring-back.
//
// Java 1.7.3 BlockButton.onBlockActivated plays "random.click" at volume 0.3 and pitch 0.6,
// and the wooden button differs only by pitch (0.5) -- 1.7.3's own random/wood_click asset is
// never referenced by the client, so in vanilla the two really do share one sample (which is
// also the GUI click, hence the "sounds like a GUI button" report).
//
// The old code played Assets/Sounds/other/button1.wav here -- the project's *menu* click, which
// is a soft, dull 0.29 s swell, nothing like vanilla's crisp click. Both redstone buttons use
// real 1.7.3 samples now: random/click.ogg for stone and random/wood_click.ogg for wood, so the
// two are audibly different as well as correct at the source.
void SoundManager::PlayButtonSound(bool wood)
{
	if(!playerSounds)
		return;

	pgeWav* snd = wood ? buttonWood : buttonStone;
	if(snd == NULL)
		snd = buttonSound; // asset missing: still click rather than go silent

	if(snd == NULL)
		return;

	pgeWavVolume(snd, kButtonClickVolume, kButtonClickVolume);
	pgeWavPitch(snd, wood ? 1.0f : 0.6f); // 0.6 is Java's stone-button pitch; the wooden sample is already wood-toned
	pgeWavPlay(snd);
	pgeWavPitch(snd, 1.0f);
}

// Java plays one random.explode per exploding entity, and a cannon primes every block in the same frame,
// so N blocks start N copies of the sample at once. Identical copies sum coherently and drove the mixer to
// the ceiling. The soft limiter in pgeWav.c now makes that impossible to clip, but stacking ten copies of
// one sample is still just a wall of the same sound, so cap how many can overlap; the pitch jitter below
// keeps the ones that do play from being phase-identical.
static const int kMaxConcurrentExplosions = 4;

void SoundManager::TNTSound()
{
	if(playerSounds)
	{
		if(pgeWavCountPlaying(tnt) >= kMaxConcurrentExplosions)
			return;

		// Java 1.7.3 Explosion plays "random.explode" at volume 4.0F, which the sound
		// manager clamps back to 1.0F (so the volume was already right) and at pitch
		// (1.0F + (rand - rand) * 0.2F) * 0.7F. Matching that pitch is not just
		// flavour: it gives the deeper, larger sound Java has, and it detunes blasts
		// that go off together so they are not identical copies of one waveform.
		float r1 = (rand() % 1000) / 1000.0f;
		float r2 = (rand() % 1000) / 1000.0f;
		float pitch = (1.0f + (r1 - r2) * 0.2f) * 0.7f;

		pgeWavPitch(tnt, pitch);
		pgeWavPlay(tnt);
		pgeWavPitch(tnt, 1.0f);
	}
}

// Same problem as the explosion, and worse because the fuse is a long sound: a cannon primes every block
// in the cannon in a single frame, so all of them started an identical copy of the fuse sample together
// and it summed into a solid wall of one waveform for its whole length. Two voices is enough for a cluster
// to read as "several fuses lit" without turning into noise.
static const int kMaxConcurrentFuses = 2;

void SoundManager::PlayFuseSound()
{
	if(playerSounds && pgeWavCountPlaying(fuse) < kMaxConcurrentFuses)
		pgeWavPlay(fuse);
}

void SoundManager::PlayFissSound()
{
	if(playerSounds)
		pgeWavPlay(fiss);
}

void SoundManager::PlayShearSound()
{
	if(playerSounds)
		pgeWavPlay(shear);
}

void SoundManager::PlayFlintSound()
{
	if (playerSounds)
		pgeWavPlay(flint);
}

void SoundManager::PlaySplashSound()
{
	if(playerSounds)
    {
        int volume = 88+rand()%10;
        pgeWavVolume(splash,volume,volume);
		pgeWavPlay(splash);
    }
}

void SoundManager::PlayNoteSound(unsigned int noteType, float pitch)
{
	if(playerSounds)
    {
        pgeWavPitch(noteSounds[noteType],pitch);
        pgeWavPlay(noteSounds[noteType]);
        pgeWavPitch(noteSounds[noteType],1.0f);
    }
}

void SoundManager::PlayDoorSound(bool open)
{
	if(playerSounds)
    {
        open == true ? pgeWavPlay(doorOpen) : pgeWavPlay(doorClose);
    }
}

// Piston extend and retract, Java BlockPistonBase.onBlockEvent:
//
//     "tile.piston.out", 0.5, rand * 0.25 + 0.6   -- extending
//     "tile.piston.in",  0.5, rand * 0.15 + 0.6   -- retracting
//
// Volume 0.5 on Java's 0-1 scale is 50 here, the same ratio kButtonClickVolume keeps
// for the button's 0.3. The pitch keeps vanilla's spread as a two-value jitter around
// 0.6 rather than a fresh random: a row of pistons firing together then sounds like a
// row rather than one voice, without the sound code needing the game's RNG (which is
// deterministic on purpose -- see the world seed).
void SoundManager::PlayPistonSound(bool extend)
{
	if(!playerSounds)
		return;

	pgeWav* snd = extend ? pistonOut : pistonIn;
	if(snd == NULL)
		return;

	// Vanilla's random is per call and small; alternating two values is enough to stop
	// simultaneous pistons phase-locking, and leaves the game's seeded RNG alone.
	pistonPitchFlip = !pistonPitchFlip;

	pgeWavVolume(snd, kPistonSoundVolume, kPistonSoundVolume);
	pgeWavPitch(snd, pistonPitchFlip ? 0.60f : (extend ? 0.66f : 0.63f));
	pgeWavPlay(snd);
	pgeWavPitch(snd, 1.0f);
}

void SoundManager::PlayGlassSound()
{
	if(playerSounds)
		pgeWavPlay(glass);
}

// Longer than the 0.55 s the play states wait between calls, with room for a slow frame.
static const float kRainLeaseSeconds = 1.5f;

void SoundManager::PlayRainSound(int volume)
{
	if(!playerSounds || rainSound == NULL)
	{
		StopRainSound();
		return;
	}

	if(volume < 12)
		volume = 12;
	if(volume > 28)
		volume = 28;

	// Two independent grain chains over the 2 s clip: random stretches, directions and pitches,
	// crossfaded in the audio thread, so there is no seam and no repeating phrase. The lease is
	// renewed by every call; if the game stops calling (pause, leaving the world) the rain fades out.
	pgeWavLoop(rainSound, 0);
	pgeWavVolume(rainSound, volume, volume);

	if(pgeWavCountPlaying(rainSound) == 0)
	{
		pgeWavPlayGrains(rainSound, 1.0f, 1.5f, 0.35f, 0.06f, kRainLeaseSeconds);
		pgeWavPlayGrains(rainSound, 1.0f, 1.5f, 0.35f, 0.06f, kRainLeaseSeconds);
	}
	else
	{
		pgeWavKeepGrains(rainSound, kRainLeaseSeconds);
	}
}

bool SoundManager::RainSoundPlaying()
{
	return rainSound != NULL && pgeWavIsPlaying(rainSound);
}

void SoundManager::StopRainSound()
{
	if(rainSound == NULL)
		return;

	pgeWavFadeOut(rainSound, 0.6f);
}

void SoundManager::PlayEndDigSound(int type)
{
	if(playerSounds)
	{
		switch(type)
		{
			case 0://grass
			{
				pgeWavPlay(digSounds[0]);
			}
			break;
			case 1://gravel
			{
				pgeWavPlay(digSounds[1]);
			}
			break;
			case 2://stone
			{
				pgeWavPlay(digSounds[2]);
			}
			break;
			case 3://wood
			{
				pgeWavPlay(woodSounds[rand() % 4]);
			}
			break;
            case 4://cloth
			{
			    pgeWavPlay(snowSounds[rand() % 4]);
			}
			break;
            case 5://sand
			{
				pgeWavPlay(sandSounds[rand() % 4]);
			}
			break;
            case 6://snow
			{
				pgeWavPlay(snowSounds[rand() % 4]);
			}
			break;
		}
	}
}

void SoundManager::PlayBreakSound()
{
	if(playerSounds)
		pgeWavPlay(breakSound);
}

void SoundManager::PlayBowSound()
{
	if(playerSounds)
		pgeWavPlay(bow);
}

// A mob loosing an arrow: the same bow clip, quieter with distance on the 15-block ramp the impacts
// use, at vanilla's skeleton pitch of 1 / (0.8..1.2). Two voices at most, for a volley.
static const int kMaxConcurrentBowShots = 2;

void SoundManager::PlayBowSoundAt(float distanceToPlayer)
{
	if(!playerSounds || bow == NULL)
		return;
	if(distanceToPlayer >= 15.0f)
		return;
	if(pgeWavCountPlaying(bow) >= kMaxConcurrentBowShots)
		return;

	distanceToPlayer -= 2.0f;
	if(distanceToPlayer < 0.0f)
		distanceToPlayer = 0.0f;

	const float volume = (15.0f - distanceToPlayer) / 15.0f;
	const float pitch = 1.0f / (0.8f + (float)(rand() % 401) / 1000.0f);
	pgeWavPlayWith(bow, volume, pitch);
}

// An arrow landing. Vanilla plays these at the arrow's own position, so the volume is a falloff
// from the player rather than a fixed level -- the same 15-block ramp every mob sound uses, which
// is what makes a shot that lands across the valley quiet instead of at the player's ear.
//
// Two voices per sample is the same reasoning as the fuse cap: a full draw followed by a second
// arrow a moment later, or a skeleton volley, would otherwise start several copies of the same
// clip in one frame. Unlike the fuse these are short, so the cap only ever trims a genuine pile-up.
static const int kMaxConcurrentArrowHits = 2;

// The four block clips are picked at random so a wall of arrows does not tick like a metronome.
// Everything else about the two paths below is the same, so the shared tail is factored out.
static void PlayArrowImpact(pgeWav* clip, float distanceToPlayer)
{
	if(clip == NULL)
		return;
	if(distanceToPlayer >= 15.0f)
		return;

	// The 2-block subtraction is the same one the mob sounds make: right next to the listener the
	// sample is already at full scale, so the ramp starts a little out rather than at the feet.
	distanceToPlayer -= 2.0f;
	if(distanceToPlayer < 0.0f)
		distanceToPlayer = 0.0f;

	const float volume = (15.0f - distanceToPlayer) / 15.0f * 100.0f;
	pgeWavVolume(clip, volume, volume);
	pgeWavPlay(clip);
}

void SoundManager::PlayArrowHitSound(float distanceToPlayer)
{
	if(!playerSounds)
		return;

	// Which of the four is in use is decided here rather than by the caller, who only knows that
	// something solid stopped the arrow.
	pgeWav* clip = bowHitSounds[rand() % 4];
	if(pgeWavCountPlaying(clip) >= kMaxConcurrentArrowHits)
		return;

	PlayArrowImpact(clip, distanceToPlayer);
}

void SoundManager::PlayArrowFleshSound(float distanceToPlayer)
{
	if(!playerSounds)
		return;

	if(pgeWavCountPlaying(bowHitFleshSound) >= kMaxConcurrentArrowHits)
		return;

	PlayArrowImpact(bowHitFleshSound, distanceToPlayer);
}

void SoundManager::PlayZombieHurtSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand() % 2;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(zombieHurtSounds[i],volume,volume);
            pgeWavPlay(zombieHurtSounds[i]);
        }
    }
}

void SoundManager::PlayZombieDieSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(zombieDieSound,volume,volume);
            pgeWavPlay(zombieDieSound);
        }
    }
}

void SoundManager::PlayCowHurtSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%2;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(cowHurtSounds[i],volume,volume);
            pgeWavPlay(cowHurtSounds[i]);
        }
    }
}

void SoundManager::PlayCreeperHurtSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%2;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(creeperHurtSounds[i],volume,volume);
            pgeWavPlay(creeperHurtSounds[i]);
        }
    }
}

void SoundManager::PlaySheepHurtSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%2;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(sheepHurtSounds[i],volume,volume);
            pgeWavPlay(sheepHurtSounds[i]);
        }
    }
}

void SoundManager::PlayChickenHurtSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%2;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(chickenHurtSounds[i],volume,volume);
            pgeWavPlay(chickenHurtSounds[i]);
        }
    }
}

void SoundManager::PlayChickenSaySound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%3;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(chickenSaySounds[i],volume,volume);
            pgeWavPlay(chickenSaySounds[i]);
        }
    }
}

void SoundManager::PlayChickenStepSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%2;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(chickenStepSounds[i],volume,volume);
            pgeWavPlay(chickenStepSounds[i]);
        }
    }
}

void SoundManager::PlayChickenPlopSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(chickenPlopSound,volume,volume);
            pgeWavPlay(chickenPlopSound);
        }
    }
}

void SoundManager::PlaySpiderSaySound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%4;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(spiderSaySounds[i],volume,volume);
            pgeWavPlay(spiderSaySounds[i]);
        }
    }
}

// Java: getHurtSound() is mob.spider.say, so a hit reuses the idle clips.
void SoundManager::PlaySpiderHurtSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%4;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(spiderSaySounds[i],volume,volume);
            pgeWavPlay(spiderSaySounds[i]);
        }
    }
}

void SoundManager::PlaySpiderStepSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%4;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(spiderStepSounds[i],volume,volume);
            pgeWavPlay(spiderStepSounds[i]);
        }
    }
}

void SoundManager::PlaySpiderDeathSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(spiderDeathSound,volume,volume);
            pgeWavPlay(spiderDeathSound);
        }
    }
}

void SoundManager::PlaySkeletonSaySound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%3;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(skeletonSaySounds[i],volume,volume);
            pgeWavPlay(skeletonSaySounds[i]);
        }
    }
}

void SoundManager::PlaySkeletonHurtSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%4;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(skeletonHurtSounds[i],volume,volume);
            pgeWavPlay(skeletonHurtSounds[i]);
        }
    }
}

void SoundManager::PlaySkeletonStepSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%4;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(skeletonStepSounds[i],volume,volume);
            pgeWavPlay(skeletonStepSounds[i]);
        }
    }
}

void SoundManager::PlaySkeletonDeathSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(skeletonDeathSound,volume,volume);
            pgeWavPlay(skeletonDeathSound);
        }
    }
}

void SoundManager::PlayZombieSaySound(float distanceToPlayer)
{
	if(playerSounds)
    {
        if(distanceToPlayer < 22.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (22.0f-distanceToPlayer)/22.0f*100.0f;
            pgeWavVolume(zombieSaySound,volume,volume);
            pgeWavPlay(zombieSaySound);
        }
    }
}

void SoundManager::PlayCowSaySound(float distanceToPlayer)
{
	if(playerSounds)
    {
        if(distanceToPlayer < 20.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (20.0f-distanceToPlayer)/20.0f*100.0f;
            pgeWavVolume(cowSaySound,volume,volume);
            pgeWavPlay(cowSaySound);
        }
    }
}

void SoundManager::PlayPigHurtSound(float distanceToPlayer)
{
	if(playerSounds)
    {
        int i = rand()%2;
        if(distanceToPlayer < 15.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (15.0f-distanceToPlayer)/15.0f*100.0f;
            pgeWavVolume(pigHurtSounds[i],volume,volume);
            pgeWavPlay(pigHurtSounds[i]);
        }
    }
}

void SoundManager::PlayPigSaySound(float distanceToPlayer)
{
	if(playerSounds)
    {
        if(distanceToPlayer < 20.0f)
        {
            distanceToPlayer -= 2.0f;
            if(distanceToPlayer < 0)
            {
                distanceToPlayer = 0.0f;
            }
            float volume = (20.0f-distanceToPlayer)/20.0f*100.0f;
            pgeWavVolume(pigSaySound,volume,volume);
            pgeWavPlay(pigSaySound);
        }
    }
}

void SoundManager::PlayHitSound()
{
	if(playerSounds)
    {
		pgeWavPlay(hitSounds[rand() % 3]);
    }
}

void SoundManager::PlayEatSound()
{
	if(playerSounds)
    {
		pgeWavPlay(eatSounds[rand() % 3]);
    }
}


void SoundManager::PlayFallSound(float i)
{
	if(playerSounds)
    {
        if (i < -9 && i > -21)
        {
            pgeWavPlay(fallSounds[1]);
        }
        if (i < -21)
        {
            pgeWavPlay(fallSounds[0]);
        }
    }
}

void SoundManager::PlayWalkSound(int type)
{
	if(playerSounds)
	{
		switch(type)
		{
			case 0://grass
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(grassSounds[lastWalkSound]);

				//play new sound
				pgeWavPlay(grassSounds[currentWalkSound]);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
			case 1://gravel
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(grawelSounds[lastWalkSound]);

				//play new sound
				pgeWavPlay(grawelSounds[currentWalkSound]);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
			case 2://stone
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(stoneSounds[lastWalkSound]);

				//play new sound
				pgeWavPlay(stoneSounds[currentWalkSound]);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
			case 3://wood
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(woodSounds[lastWalkSound]);

				//play new sound
				pgeWavPlay(woodSounds[currentWalkSound]);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
            case 4://cloth
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(snowSounds[lastWalkSound]);

				//play new sound
				pgeWavPlay(snowSounds[currentWalkSound]);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
            case 5://sand
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(sandSounds[lastWalkSound]);

				//play new sound
				pgeWavPlay(sandSounds[currentWalkSound]);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
            case 6://snow
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(snowSounds[lastWalkSound]);

				//play new sound
				pgeWavPlay(snowSounds[currentWalkSound]);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
		}
	}
}

void SoundManager::PlayDigSound(int type)
{
	if(playerSounds)
	{
		switch(type)
		{
			case 0://grass
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(grassSounds[lastWalkSound]);
				pgeWavVolume(grassSounds[lastWalkSound],100,100);

				//play new sound
				pgeWavVolume(grassSounds[currentWalkSound],25,25);
				pgeWavPlay(grassSounds[currentWalkSound]);
				pgeWavVolume(grassSounds[currentWalkSound],100,100);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
			case 1://gravel
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(grawelSounds[lastWalkSound]);
				pgeWavVolume(grawelSounds[lastWalkSound],100,100);

				//play new sound
				pgeWavVolume(grawelSounds[currentWalkSound],25,25);
				pgeWavPlay(grawelSounds[currentWalkSound]);
				pgeWavVolume(grawelSounds[currentWalkSound],100,100);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
			case 2://stone
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(stoneSounds[lastWalkSound]);
				pgeWavVolume(stoneSounds[lastWalkSound],100,100);

				//play new sound
				pgeWavVolume(stoneSounds[currentWalkSound],25,25);
				pgeWavPlay(stoneSounds[currentWalkSound]);
				pgeWavVolume(stoneSounds[currentWalkSound],100,100);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
			case 3://wood
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(woodSounds[lastWalkSound]);
				pgeWavVolume(woodSounds[lastWalkSound],100,100);

				//play new sound
				pgeWavVolume(woodSounds[currentWalkSound],25,25);
				pgeWavPlay(woodSounds[currentWalkSound]);
				pgeWavVolume(woodSounds[currentWalkSound],100,100);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
            case 4://cloth
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(snowSounds[lastWalkSound]);
				pgeWavVolume(snowSounds[lastWalkSound],100,100);

				//play new sound
				pgeWavVolume(snowSounds[currentWalkSound],25,25);
				pgeWavPlay(snowSounds[currentWalkSound]);
				pgeWavVolume(snowSounds[currentWalkSound],100,100);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
            case 5://sand
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(sandSounds[lastWalkSound]);
				pgeWavVolume(sandSounds[lastWalkSound],100,100);

				//play new sound
				pgeWavVolume(sandSounds[currentWalkSound],25,25);
				pgeWavPlay(sandSounds[currentWalkSound]);
				pgeWavVolume(sandSounds[currentWalkSound],100,100);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
            case 6://snow
			{
				currentWalkSound = rand() % 4;

				//stop last sound
				pgeWavStop(snowSounds[lastWalkSound]);
				pgeWavVolume(snowSounds[lastWalkSound],100,100);

				//play new sound
				pgeWavVolume(snowSounds[currentWalkSound],25,25);
				pgeWavPlay(snowSounds[currentWalkSound]);
				pgeWavVolume(snowSounds[currentWalkSound],100,100);

				//set last sound as current
				lastWalkSound = currentWalkSound;
			}
			break;
		}
	}
}

void SoundManager::StopAmbient()
{
    if(ambientTrack != NULL)
    {
        oslStopSound(ambientTrack);
        oslDeleteSound(ambientTrack);
        ambientTrack = NULL;

        ambientTrackName = " ";
    }
}

int SoundManager::PlayRandomAmbient()
{
 	switch(rand() % 12)
 	{
 	    case 0: PlayAmbient("calm1"); break;
 	    case 1: PlayAmbient("calm2"); break;
 	    case 2: PlayAmbient("calm3"); break;
 	    case 3: PlayAmbient("piano1"); break;
 	    case 4: PlayAmbient("piano2"); break;
 	    case 5: PlayAmbient("piano3"); break;
 	    case 6: PlayAmbient("nuance1"); break;
 	    case 7: PlayAmbient("nuance2"); break;
 	    case 8: PlayAmbient("hal1"); break;
 	    case 9: PlayAmbient("hal2"); break;
 	    case 10: PlayAmbient("hal3"); break;
 	    case 11: PlayAmbient("hal4"); break;
 	}
    int delay = 540+rand()%121;
	return delay;
}

void SoundManager::PlayAmbient(std::string name)
{
    StopAmbient();

    ambientTrackName = name;
    std::string trackPath = "Assets/Sounds/ambient/"+name+".bgm";
    ambientTrack = oslLoadSoundFile(trackPath.c_str(),OSL_FMT_STREAM);
    if(ambientTrack != NULL)
    {
        oslPlaySound(ambientTrack,4);
    }
}

void SoundManager::StopDiskTrack()
{
    if(diskTrack != NULL)
    {
        oslStopSound(diskTrack);
        oslDeleteSound(diskTrack);
        diskTrack = NULL;

        diskTrackName = " ";
    }
}

void SoundManager::PlayDiskTrack(int diskNumber)
{
	StopAmbient();
	StopDiskTrack();

	std::string name = " ";
	switch(diskNumber)
	{
	    case 0: name = "13"; break;
	    case 1: name = "cat"; break;
	    case 2: name = "blocks"; break;
	    case 3: name = "chirp"; break;
	    case 4: name = "far"; break;
	    case 5: name = "mall"; break;
	    case 6: name = "mellohi"; break;
	    case 7: name = "stal"; break;
	    case 8: name = "strad"; break;
	    case 9: name = "ward"; break;
	    case 10: name = "11"; break;
	    case 11: name = "wait"; break;
	}
    diskTrackName = name;
	std::string trackPath = "Assets/Sounds/disk/"+name+".bgm";
    diskTrack = oslLoadSoundFile(trackPath.c_str(),OSL_FMT_STREAM);
    if(diskTrack != NULL)
    {
        oslPlaySound(diskTrack,4);
    }
}