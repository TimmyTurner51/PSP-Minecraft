#include <pspaudio.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <pspiofilemgr.h>
#include <pspthreadman.h>
#include "pgeWav.h"

// This thread runs on its own COP1 context; see Scripts/FpuControl.h.
#include "Scripts/FpuControl.h"

#define PGE_WAV_MAX_SLOTS 16

static pgeWav pgeWavInfo[PGE_WAV_MAX_SLOTS];
// 0 free, 1 mixing, 2 claimed and still being filled in (not mixed yet). Both the game thread and the
// audio thread start voices, so a slot is claimed before its fields are written.
static volatile int pgeWavPlaying[PGE_WAV_MAX_SLOTS];
static int pgeWavId[PGE_WAV_MAX_SLOTS];

// Envelopes and grain chains, per slot, in output frames (44100 per second).
//   length 0 plays to the end of the clip; otherwise the voice ends after that many frames.
//   A grain voice starts its successor the frame its own fade-out begins, with a fade-in of the same
//   length, so the pair crossfade at constant power while the lease lasts.
static unsigned long pgeWavPlayed[PGE_WAV_MAX_SLOTS];
static unsigned long pgeWavLength[PGE_WAV_MAX_SLOTS];
static unsigned long pgeWavFadeIn[PGE_WAV_MAX_SLOTS];
static volatile unsigned long pgeWavFadeOutLen[PGE_WAV_MAX_SLOTS];
static volatile int pgeWavGrain[PGE_WAV_MAX_SLOTS];
static unsigned long pgeWavGrainMin[PGE_WAV_MAX_SLOTS];
static unsigned long pgeWavGrainMax[PGE_WAV_MAX_SLOTS];
static float pgeWavGrainPitch[PGE_WAV_MAX_SLOTS];
static volatile unsigned long pgeWavLease[PGE_WAV_MAX_SLOTS];

static volatile unsigned long pgeWavClock = 0;
static unsigned int pgeWavRandState = 0x2545F491;

#define PGE_WAV_OUTPUT_RATE 44100
#define PGE_WAV_CURVE_STEPS 64
static float pgeWavEqualPower[PGE_WAV_CURVE_STEPS + 1];

static short *pgeWavSamples;
static unsigned long pgeWavReq;
static int pgeWavIdFlag = 0;

static int pgeWavInitFlag = 0;

#define PGE_NUM_AUDIO_CHANNELS	1
#define PGE_NUM_AUDIO_SAMPLES	1024
#define PGE_VOLUME_MAX			0x8000

// Every voice is summed into a single integer and that sum used to be clamped
// straight at +-32767. A single sound never reached it, but anything that stacked
// up did. A TNT cluster is the worst case: TNTSound runs once per entity in the
// same frame, so several identical copies of the sample start together and sum
// coherently, and the hard clamp turned that into the crackle heard on a cannon.
// Two changes keep the mix clean:
//   - a master gain, so the sum starts with headroom instead of at the ceiling;
//   - a soft knee above PGE_WAV_KNEE whose curve cannot reach PGE_WAV_CEILING, so
//     no amount of stacking can clip, however many voices pile up.
// Raise or lower PGE_WAV_MASTER_GAIN_Q10 to taste; 1024 is 1.0 (no reduction).
#define PGE_WAV_MASTER_GAIN_Q10	768		// Q10 fixed point, so 768/1024 = 0.75
#define PGE_WAV_KNEE			20000
#define PGE_WAV_CEILING			32767

typedef void (* pgeAudioLibCallback)(void *buf, unsigned int reqn, void *pdata);

typedef struct
{
	int threadHandle;
	int handle;
	int volumeLeft;
	int volumeRight;
	pgeAudioLibCallback callback;
	void *data;

} pgeAudioLibChannelInfo;

static int pgeAudioLibReady = 0;
static short pgeAudioLibSoundBuffer[PGE_NUM_AUDIO_CHANNELS][2][PGE_NUM_AUDIO_SAMPLES][2];

static pgeAudioLibChannelInfo pgeAudioLibStatus[PGE_NUM_AUDIO_CHANNELS];

static volatile int pgeAudioLibTerminate = 0;

static void pgeAudioLibSetChannelCallback(int channel, pgeAudioLibCallback callback, void *data)
{
	volatile pgeAudioLibChannelInfo *pci = &pgeAudioLibStatus[channel];

	if (callback == 0)
		pci->callback = 0;
	else
	{
		pci->callback = callback;
		sceKernelWakeupThread(pci->threadHandle);
	}
}

static int pgeAudioLibOutBlocking(unsigned int channel, unsigned int left, unsigned int right, void *data)
{
	if (!pgeAudioLibReady)
		return(-1);

	if (channel >= PGE_NUM_AUDIO_CHANNELS)
		return(-1);

	if (left > PGE_VOLUME_MAX)
		left = PGE_VOLUME_MAX;

	if (right > PGE_VOLUME_MAX)
		right = PGE_VOLUME_MAX;

	return sceAudioOutputPannedBlocking(pgeAudioLibStatus[channel].handle, left, right, data);
}

static int pgeAudioLibChannelThread(int args, void *argp)
{
	MC_DisableFpuExceptions();

	volatile int bufidx = 0;

	// Read the argument byte-wise: sceKernelStartThread hands the block over on this thread's stack and
	// it is word-aligned in practice, but a plain *(int *) here is the same device-only fault class as
	// the WAV header fields above, so it is not worth depending on.
	const unsigned char *argBytes = (const unsigned char *) argp;
	int channel = argBytes[0] | (argBytes[1] << 8) | (argBytes[2] << 16) | (argBytes[3] << 24);

	while (pgeAudioLibTerminate == 0)
	{
		void *bufptr = &pgeAudioLibSoundBuffer[channel][bufidx];
		pgeAudioLibCallback callback;
		callback = pgeAudioLibStatus[channel].callback;

		if (callback)
		{
			callback(bufptr, PGE_NUM_AUDIO_SAMPLES, pgeAudioLibStatus[channel].data);
			pgeAudioLibOutBlocking(channel, pgeAudioLibStatus[channel].volumeLeft, pgeAudioLibStatus[channel].volumeRight, bufptr);
		}
		else
		{
			sceKernelSleepThread();
		}

	bufidx = (bufidx ? 0:1);
	}

	sceKernelExitThread(0);

	return(0);
}

static int pgeAudioLibInit(int priority)
{
	int i, ret;
	int failed = 0;
	char str[32];

	pgeAudioLibTerminate = 0;
	pgeAudioLibReady = 0;

	for (i = 0; i < PGE_NUM_AUDIO_CHANNELS; i++)
	{
		pgeAudioLibStatus[i].handle = -1;
		pgeAudioLibStatus[i].threadHandle = -1;
		pgeAudioLibStatus[i].volumeRight = PGE_VOLUME_MAX;
		pgeAudioLibStatus[i].volumeLeft  = PGE_VOLUME_MAX;
		pgeAudioLibStatus[i].callback = 0;
		pgeAudioLibStatus[i].data = 0;
	}

	for (i = 0; i < PGE_NUM_AUDIO_CHANNELS; i++)
	{
		if ((pgeAudioLibStatus[i].handle = sceAudioChReserve(-1, PGE_NUM_AUDIO_SAMPLES, 0)) < 0)
			failed = 1;
	}

	if (failed)
	{
		for (i = 0; i < PGE_NUM_AUDIO_CHANNELS; i++)
		{
			if (pgeAudioLibStatus[i].handle != -1)
				sceAudioChRelease(pgeAudioLibStatus[i].handle);

			pgeAudioLibStatus[i].handle = -1;
		}

		return 0;
	}

	pgeAudioLibReady = 1;

	strcpy(str, "PgeAudioThread0");

	for (i = 0; i < PGE_NUM_AUDIO_CHANNELS; i++)
	{
		str[14] = '0' + i;
		pgeAudioLibStatus[i].threadHandle = sceKernelCreateThread(str, (void*)&pgeAudioLibChannelThread, priority, 0x10000, PSP_THREAD_ATTR_USER, NULL);

		if (pgeAudioLibStatus[i].threadHandle < 0)
		{
			pgeAudioLibStatus[i].threadHandle = -1;
			failed = 1;
			break;
		}

		ret = sceKernelStartThread(pgeAudioLibStatus[i].threadHandle, sizeof(i), &i);

		if (ret != 0)
		{
			failed = 1;
			break;
		}
	}

	if (failed)
	{
		pgeAudioLibTerminate = 1;

		for (i = 0; i < PGE_NUM_AUDIO_CHANNELS; i++)
		{
			if (pgeAudioLibStatus[i].threadHandle != -1)
			{
				sceKernelDeleteThread(pgeAudioLibStatus[i].threadHandle);
			}

			pgeAudioLibStatus[i].threadHandle = -1;
		}

		pgeAudioLibReady = 0;

		return 0;
	}

	return 1;
}

static void pgeAudioLibShutdown(void)
{
	int i;
	pgeAudioLibReady = 0;
	pgeAudioLibTerminate = 1;

	for (i = 0; i < PGE_NUM_AUDIO_CHANNELS; i++)
	{
		if (pgeAudioLibStatus[i].threadHandle != -1)
		{
			sceKernelDeleteThread(pgeAudioLibStatus[i].threadHandle);
		}

		pgeAudioLibStatus[i].threadHandle = -1;
	}

	for (i = 0; i < PGE_NUM_AUDIO_CHANNELS; i++)
	{
		if (pgeAudioLibStatus[i].handle != -1)
		{
			sceAudioChRelease(pgeAudioLibStatus[i].handle);
			pgeAudioLibStatus[i].handle = -1;
		}
	}
}

// Cheap soft limiter for one output sample. Below the knee the sample passes through untouched, so an
// ordinary "one or two sounds at once" mix (the vast majority of the time) is bit-identical to before.
// Above it the excess is compressed instead of truncated, which is what removed the static.
//
// The compression is `range - range*range/(excess+range)`, a curve that rises towards PGE_WAV_CEILING
// but never reaches it: whatever is fed in, the output cannot clip. That guarantee is the point. The
// previous version divided the excess by 4, which merely *raised* the headroom, so a big enough stack
// clipped again -- a 3x3 powder cannon (9 TNT, all primed in one frame, each starting an identical copy
// of the sample that sums coherently) still tore. `range*range` is ~1.6e8 and fits in 32 bits, so this
// needs no 64-bit multiply; the division only happens on samples that actually overload.
static int pgeWavLimitSample(int sample)
{
	int sign = 1;
	int mag = sample;

	if(mag < 0)
	{
		sign = -1;
		mag = -mag;
	}

	if(mag > PGE_WAV_KNEE)
	{
		const int range = PGE_WAV_CEILING - PGE_WAV_KNEE;
		int excess = mag - PGE_WAV_KNEE;

		mag = PGE_WAV_KNEE + range - (range * range) / (excess + range);
	}

	return sign * mag;
}

// The mixer's own random numbers, so the audio thread never touches the game's seeded RNG.
static unsigned int pgeWavRand(void)
{
	pgeWavRandState = pgeWavRandState * 1664525u + 1013904223u;
	return pgeWavRandState >> 8;
}

// sin(t * pi/2) for t in 0..1: a fade-in and the matching fade-out sum to constant power.
static float pgeWavCurve(unsigned long pos, unsigned long span)
{
	if(span == 0 || pos >= span)
		return 1.0f;
	float t = (float)pos * PGE_WAV_CURVE_STEPS / (float)span;
	int k = (int)t;
	float f = t - (float)k;
	return pgeWavEqualPower[k] + (pgeWavEqualPower[k + 1] - pgeWavEqualPower[k]) * f;
}

static int pgeWavClaimSlot(void)
{
	int i;
	for(i = 0; i < PGE_WAV_MAX_SLOTS; i++)
	{
		if(pgeWavPlaying[i] == 0)
		{
			pgeWavPlaying[i] = 2;
			return i;
		}
	}
	return -1;
}

static void pgeWavClearEnvelope(int slot)
{
	pgeWavPlayed[slot] = 0;
	pgeWavLength[slot] = 0;
	pgeWavFadeIn[slot] = 0;
	pgeWavFadeOutLen[slot] = 0;
	pgeWavGrain[slot] = 0;
}

// Fills a claimed slot with one grain of `wav`: a random stretch, a random direction and a pitch within
// +-spread, sized so it ends inside the clip.
static void pgeWavFillGrain(int slot, const pgeWav *wav, unsigned long minLen, unsigned long maxLen,
	unsigned long fade, float spread, unsigned long fadeIn, unsigned long lease, float volL, float volR)
{
	pgeWav *wid = &pgeWavInfo[slot];
	unsigned long len = minLen;
	if(maxLen > minLen)
		len += pgeWavRand() % (maxLen - minLen + 1);

	float pitch = 1.0f;
	if(spread > 0.0f)
		pitch = 1.0f - spread + 2.0f * spread * (float)(pgeWavRand() % 1001) / 1000.0f;
	unsigned long rate = (unsigned long)((float)((wav->sampleRate * 0x4000) / 11025) * pitch);

	// Source frames this grain consumes, with a little margin so it cannot run off the clip.
	unsigned long needed = (unsigned long)(((unsigned long long)len * rate) >> 16) + 64;
	if(needed >= wav->sampleCount)
	{
		needed = wav->sampleCount - 1;
		len = (unsigned long)(((unsigned long long)(needed - 64) << 16) / rate);
	}
	if(fade * 2 > len)
		fade = len / 2;

	unsigned int reverse = pgeWavRand() & 1;
	unsigned long room = wav->sampleCount - needed;
	unsigned long start = (room > 0) ? (pgeWavRand() % room) : 0;
	if(reverse)
		start += needed;

	wid->channels = wav->channels;
	wid->sampleRate = wav->sampleRate;
	wid->sampleCount = wav->sampleCount;
	wid->dataLength = wav->dataLength;
	wid->data = wav->data;
	wid->rateRatio = rate;
	wid->playPtr = start;
	wid->playPtr_frac = 0;
	wid->loop = 0;
	wid->id = wav->id;
	wid->bitPerSample = wav->bitPerSample;
	wid->leftvolume = volL;
	wid->rightvolume = volR;
	wid->reverse = reverse;

	pgeWavPlayed[slot] = 0;
	pgeWavLength[slot] = len;
	pgeWavFadeIn[slot] = (fadeIn < len) ? fadeIn : len / 2;
	pgeWavFadeOutLen[slot] = fade;
	pgeWavGrainMin[slot] = minLen;
	pgeWavGrainMax[slot] = maxLen;
	pgeWavGrainPitch[slot] = spread;
	pgeWavLease[slot] = lease;
	pgeWavGrain[slot] = 1;
	pgeWavId[slot] = wav->id;
	pgeWavPlaying[slot] = 1;
}

static void wavout_snd_callback(void *_buf, unsigned int _reqn, void *pdata)
{
	int i,slot;
	pgeWav *wi;
	unsigned long ptr, frac;
	short *buf = _buf;

	pgeWavSamples = _buf;
	pgeWavReq = _reqn;

	for(i = 0; i < _reqn; i++)
	{
		int outr = 0, outl = 0;
		pgeWavClock++;

		for(slot = 0; slot < PGE_WAV_MAX_SLOTS; slot++)
		{
			if(pgeWavPlaying[slot] != 1) continue;

			wi = &pgeWavInfo[slot];

			// The envelope, and the hand-off to the next grain.
			float env = 1.0f;
			unsigned long played = pgeWavPlayed[slot]++;
			if(pgeWavFadeIn[slot] > 0 && played < pgeWavFadeIn[slot])
				env = pgeWavCurve(played, pgeWavFadeIn[slot]);
			unsigned long length = pgeWavLength[slot];
			if(length > 0)
			{
				if(played >= length)
				{
					pgeWavPlaying[slot] = 0;
					continue;
				}
				unsigned long fadeOut = pgeWavFadeOutLen[slot];
				unsigned long remaining = length - played;
				if(fadeOut > 0 && remaining <= fadeOut)
				{
					float out = pgeWavCurve(remaining, fadeOut);
					if(out < env)
						env = out;
					if(remaining == fadeOut && pgeWavGrain[slot]
						&& (long)(pgeWavLease[slot] - pgeWavClock) > 0)
					{
						int next = pgeWavClaimSlot();
						if(next >= 0)
						{
							pgeWav source = *wi;
							pgeWavFillGrain(next, &source, pgeWavGrainMin[slot], pgeWavGrainMax[slot],
								fadeOut, pgeWavGrainPitch[slot], fadeOut, pgeWavLease[slot],
								wi->leftvolume, wi->rightvolume);
						}
					}
				}
			}
			frac = wi->playPtr_frac + wi->rateRatio;
			wi->playPtr_frac = (frac & 0xffff);

			if(wi->reverse)
			{
				unsigned long step = frac >> 16;
				if(wi->playPtr <= step)
				{
					if(wi->loop && wi->sampleCount > 0)
					{
						wi->playPtr = wi->sampleCount - 1;
						wi->playPtr_frac = 0;
						ptr = wi->playPtr;
					}
					else
					{
						pgeWavPlaying[slot] = 0;
						continue;
					}
				}
				else
				{
					wi->playPtr -= step;
					ptr = wi->playPtr;
				}
			}
			else
			{
				wi->playPtr = ptr = wi->playPtr + (frac>>16);

				if(ptr >= wi->sampleCount)
				{
					if(wi->loop)
					{
						wi->playPtr = 0;
						wi->playPtr_frac = 0;
						ptr = 0;
					}
					else
					{
						pgeWavPlaying[slot] = 0;
						continue;
					}
				}
			}

			if(ptr >= wi->sampleCount)
				continue;

			short *src16 = (short *)wi->data;
			unsigned char *src8 = (unsigned char *)wi->data;
			const float volL = wi->leftvolume * env;
			const float volR = wi->rightvolume * env;

			if(wi->channels == 1)
			{
				if(wi->bitPerSample == 8)
				{
					outl += (src8[ptr] * 256) - 32768 * volL;
					outr += (src8[ptr] * 256) - 32768 * volR;
				}
				else
				{
					outl += src16[ptr] * volL;
					outr += src16[ptr] * volR;
				}
			}
			else
			{
				if(wi->bitPerSample == 8)
				{
					outl += (src8[ptr*2] * 256) - 32768 * volL;
					outr += (src8[ptr*2+1] * 256) - 32768 * volR;
				}
				else
				{
					outl += src16[ptr*2] * volL;
					outr += src16[ptr*2+1] * volR;
				}
			}
		}

		// Headroom first, then the soft knee. The multiply cannot overflow: the
		// loudest possible sum is PGE_WAV_MAX_SLOTS * 32767 (~524k), and
		// 524k * 768 is ~4.0e8, well inside a 32-bit int.
		outl = pgeWavLimitSample((outl * PGE_WAV_MASTER_GAIN_Q10) >> 10);
		outr = pgeWavLimitSample((outr * PGE_WAV_MASTER_GAIN_Q10) >> 10);

		*(buf++) = outl;
		*(buf++) = outr;
	}
}

int pgeWavInit(void)
{
	int i;

	for(i = 0; i <= PGE_WAV_CURVE_STEPS; i++)
		pgeWavEqualPower[i] = sinf((float)i / PGE_WAV_CURVE_STEPS * 1.5707963f);

	pgeAudioLibInit(0x12);

	pgeAudioLibSetChannelCallback(0, wavout_snd_callback, 0);

	for(i = 0; i < PGE_WAV_MAX_SLOTS; i++)
		pgeWavPlaying[i] = 0;

	pgeWavInitFlag = 1;

	return(1);
}

void pgeWavShutdown(void)
{
	if(pgeWavInitFlag)
		pgeAudioLibShutdown();
}

void pgeWavStop(pgeWav *wav)
{
	int i;

	if(wav == NULL)
		return;

	for(i = 0; i < PGE_WAV_MAX_SLOTS; i++)
	{
		if(wav->id == pgeWavId[i])
			pgeWavPlaying[i] = 0;
	}
}

void pgeWavStopAll(void)
{
	int i;

	for(i = 0; i < PGE_WAV_MAX_SLOTS; i++)
		pgeWavPlaying[i] = 0;
}

void pgeWavLoop(pgeWav *wav, unsigned int loop)
{
	if(wav == NULL)
		return;

	wav->loop = loop;
}

void pgeWavVolume(pgeWav *wav, unsigned char leftvolume, unsigned char rightvolume)
{
	if(wav == NULL)
		return;

	if(leftvolume > 100)
		leftvolume = 100;

	if(rightvolume > 100)
		rightvolume = 100;

	wav->leftvolume = leftvolume/100.0f;
	wav->rightvolume = rightvolume/100.0f;

	for(int i = 0; i < PGE_WAV_MAX_SLOTS; i++)
	{
		if(pgeWavPlaying[i] && pgeWavId[i] == wav->id)
		{
			pgeWavInfo[i].leftvolume = wav->leftvolume;
			pgeWavInfo[i].rightvolume = wav->rightvolume;
		}
	}
}

int pgeWavIsPlaying(pgeWav *wav)
{
	int i;

	if(wav == NULL)
		return 0;

	for(i = 0; i < PGE_WAV_MAX_SLOTS; i++)
	{
		if(pgeWavPlaying[i] && pgeWavId[i] == wav->id)
			return 1;
	}

	return 0;
}

int pgeWavCountPlaying(pgeWav *wav)
{
	int i, n;

	if(wav == NULL)
		return 0;

	n = 0;
	for(i = 0; i < PGE_WAV_MAX_SLOTS; i++)
	{
		if(pgeWavPlaying[i] && pgeWavId[i] == wav->id)
			n++;
	}

	return n;
}

void pgeWavPitch(pgeWav *wav, float pitch)
{
	if(wav == NULL)
		return;

	wav->rateRatio = ((wav->sampleRate*0x4000)/11025) * pitch;

	if(wav->rateRatio < (2000*0x4000)/11025)
		wav->rateRatio = (2000*0x4000)/11025;
	else if(wav->rateRatio > (100000*0x4000)/11025)
		wav->rateRatio = (100000*0x4000)/11025;
}

int pgeWavPlay(pgeWav *wav)
{
	return pgeWavPlayAt(wav, 0, 0);
}

int pgeWavPlayAt(pgeWav *wav, unsigned long startSample, unsigned int reverse)
{
	if(wav == NULL || !pgeWavInitFlag)
		return(0);

	pgeWav *wid;

	if(wav->sampleCount == 0)
		return(0);

	int i = pgeWavClaimSlot();
	if(i < 0)
		return(0);

	if(startSample >= wav->sampleCount)
		startSample = wav->sampleCount - 1;

	pgeWavClearEnvelope(i);
	wid = &pgeWavInfo[i];
	wid->channels = wav->channels;
	wid->sampleRate = wav->sampleRate;
	wid->sampleCount = wav->sampleCount;
	wid->dataLength = wav->dataLength;
	wid->data = wav->data;
	wid->rateRatio = wav->rateRatio;
	wid->playPtr = startSample;
	wid->playPtr_frac = 0;
	wid->loop = wav->loop;
	wid->id = wav->id;
	wid->bitPerSample = wav->bitPerSample;
	wid->leftvolume = wav->leftvolume;
	wid->rightvolume = wav->rightvolume;
	wid->reverse = reverse ? 1 : 0;
	pgeWavId[i] = wav->id;
	pgeWavPlaying[i] = 1;

	return(1);
}

int pgeWavPlayWith(pgeWav *wav, float volume, float pitch)
{
	if(wav == NULL || !pgeWavInitFlag || wav->sampleCount == 0)
		return(0);

	int i = pgeWavClaimSlot();
	if(i < 0)
		return(0);

	if(volume < 0.0f)
		volume = 0.0f;
	if(volume > 1.0f)
		volume = 1.0f;

	pgeWavClearEnvelope(i);
	pgeWav *wid = &pgeWavInfo[i];
	wid->channels = wav->channels;
	wid->sampleRate = wav->sampleRate;
	wid->sampleCount = wav->sampleCount;
	wid->dataLength = wav->dataLength;
	wid->data = wav->data;
	wid->rateRatio = (unsigned long)((float)((wav->sampleRate * 0x4000) / 11025) * pitch);
	wid->playPtr = 0;
	wid->playPtr_frac = 0;
	wid->loop = 0;
	wid->id = wav->id;
	wid->bitPerSample = wav->bitPerSample;
	wid->leftvolume = volume;
	wid->rightvolume = volume;
	wid->reverse = 0;
	pgeWavId[i] = wav->id;
	pgeWavPlaying[i] = 1;

	return(1);
}

static unsigned long pgeWavFrames(float seconds)
{
	if(seconds <= 0.0f)
		return 0;
	return (unsigned long)(seconds * PGE_WAV_OUTPUT_RATE);
}

int pgeWavPlayGrains(pgeWav *wav, float minSeconds, float maxSeconds, float fadeSeconds, float pitchSpread, float leaseSeconds)
{
	if(wav == NULL || !pgeWavInitFlag || wav->sampleCount < 256)
		return(0);

	int slot = pgeWavClaimSlot();
	if(slot < 0)
		return(0);

	unsigned long fade = pgeWavFrames(fadeSeconds);
	pgeWavFillGrain(slot, wav, pgeWavFrames(minSeconds), pgeWavFrames(maxSeconds), fade, pitchSpread,
		fade, pgeWavClock + pgeWavFrames(leaseSeconds), wav->leftvolume, wav->rightvolume);
	return(1);
}

void pgeWavKeepGrains(pgeWav *wav, float leaseSeconds)
{
	int i;

	if(wav == NULL)
		return;

	unsigned long until = pgeWavClock + pgeWavFrames(leaseSeconds);
	for(i = 0; i < PGE_WAV_MAX_SLOTS; i++)
	{
		if(pgeWavPlaying[i] == 1 && pgeWavId[i] == wav->id)
			pgeWavLease[i] = until;
	}
}

void pgeWavFadeOut(pgeWav *wav, float seconds)
{
	int i;

	if(wav == NULL)
		return;

	unsigned long fade = pgeWavFrames(seconds);
	if(fade == 0)
	{
		pgeWavStop(wav);
		return;
	}

	for(i = 0; i < PGE_WAV_MAX_SLOTS; i++)
	{
		if(pgeWavPlaying[i] != 1 || pgeWavId[i] != wav->id)
			continue;

		pgeWavGrain[i] = 0;

		// A voice already ending sooner keeps its own fade, so calling this every frame finishes it.
		unsigned long played = pgeWavPlayed[i];
		unsigned long length = pgeWavLength[i];
		if(length > 0 && length - played <= fade)
			continue;

		pgeWavFadeOutLen[i] = fade;
		pgeWavLength[i] = played + fade;
	}
}

// Read a 32-bit little-endian value out of the WAV header, one byte at a time.
//
// The header is parsed straight out of the file buffer, and the data-length field does not sit at a
// word-aligned offset in every clip. A file with a LIST/INFO chunk between "fmt " and "data" -- which
// most of the shipped sounds have -- carries the fourcc at 0x46, so the length that follows it lands at
// 0x4A. A plain *(unsigned long *) there is an unaligned word load: the Allegrex raises it as an Address
// Error and takes the process down with no chance to report anything, while PPSSPP performs it silently
// and carries on. That asymmetry is why this only ever failed on the console.
//
// PSP is little-endian, so the bytes are assembled least-significant first and the result is identical
// to the plain load on the aligned files.
static unsigned long pgeWavReadU32(const unsigned char *p)
{
	return (unsigned long)p[0]
	     | ((unsigned long)p[1] << 8)
	     | ((unsigned long)p[2] << 16)
	     | ((unsigned long)p[3] << 24);
}

static pgeWav *pgeWavLoadInternal(pgeWav *wav, unsigned char *wavfile, int size)
{
	unsigned long channels;
	unsigned long samplerate;
	unsigned long bitpersample;
	unsigned long datalength;
	unsigned long samplecount;
	unsigned long dataAt;

	// Everything below reads fixed header fields out of the buffer, so the file has to be at least as
	// long as the header before any of it is safe to touch.
	if(size < 0x2c)
	{
		free(wav);
		return NULL;
	}

	if(memcmp(wavfile, "RIFF", 4) != 0)
	{
		free(wav);
		return NULL;
	}

	channels = *(short *)(wavfile+0x16);
	samplerate = pgeWavReadU32(wavfile+0x18);
	bitpersample = *(short *)(wavfile+0x22);

	int i;

	// Walk the chunk list for the "data" fourcc. One byte at a time, because chunk sizes are not
	// required to be multiples of four and this deliberately visits unaligned offsets; the fourcc is
	// compared byte by byte so that nothing here depends on the address being word-aligned. The bound
	// also keeps the search inside the buffer, so a truncated file fails cleanly instead of reading
	// past the end of the allocation.
	dataAt = 0;
	for(i = 0; i < 0xFF; i++)
	{
		if((unsigned long)(0x24 + i + 8) > (unsigned long)size)
			break;

		if(wavfile[0x24 + i]     == 'd' && wavfile[0x24 + i + 1] == 'a'
		&& wavfile[0x24 + i + 2] == 't' && wavfile[0x24 + i + 3] == 'a')
		{
			dataAt = 0x24 + i;
			break;
		}
	}

	if(dataAt == 0)
	{
		free(wav);
		return NULL;
	}

	datalength = pgeWavReadU32(wavfile + dataAt + 4);

	if(datalength + 0x2c > size)
	{
		free(wav);
		return NULL;
	}

	if(channels != 2 && channels != 1)
	{
		free(wav);
		return NULL;
	}

	if(samplerate > 100000 || samplerate < 2000)
	{
		free(wav);
		return NULL;
	}

	if(channels == 2)
	{
		samplecount = datalength/(bitpersample>>2);
	}
	else
	{
		samplecount = datalength/((bitpersample>>2)>>1);
	}

	if(samplecount <= 0)
	{
		free(wav);
		return NULL;
	}

	// The mixer reads samples through a short*, so a data chunk starting on an odd address could not be
	// played by it at all -- it would be an unaligned halfword load on hardware and silently fine in the
	// emulator. None of the shipped clips do this, but refusing the file turns a would-be device-only
	// crash into a clip that simply does not play, which is the failure worth having if a bad WAV ever
	// ships.
	if(((unsigned long)(wavfile + dataAt + 8) & 1) != 0)
	{
		free(wav);
		return NULL;
	}

	wav->channels = channels;
	wav->sampleRate = samplerate;
	wav->sampleCount = samplecount;
	wav->dataLength = datalength;
	wav->data = wavfile + dataAt + 8;
	wav->rateRatio = (samplerate*0x4000)/11025;
	wav->playPtr = 0;
	wav->playPtr_frac= 0;
	wav->loop = 0;
	wav->reverse = 0;
	pgeWavIdFlag++;
	wav->id = pgeWavIdFlag;
	wav->bitPerSample = bitpersample;
    wav->leftvolume = 1.0f;
	wav->rightvolume = 1.0f;

	return wav;
}

pgeWav *pgeWavLoad(const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if(fp == NULL)
		return NULL;

	if(fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);
		return NULL;
	}

	long lSize = ftell(fp);
	if(lSize <= 0)
	{
		fclose(fp);
		return NULL;
	}

	if(fseek(fp, 0, SEEK_SET) != 0)
	{
		fclose(fp);
		return NULL;
	}

	pgeWav *wav = (pgeWav*)malloc(sizeof(pgeWav) + (size_t)lSize);
	if(wav == NULL)
	{
		fclose(fp);
		return NULL;
	}

	unsigned char *wavfile = (unsigned char*)(wav + 1);
	size_t filelen = fread(wavfile, 1, (size_t)lSize, fp);
	fclose(fp);

	return pgeWavLoadInternal(wav, wavfile, (int)filelen);
}

pgeWav *pgeWavLoadMemory(const unsigned char *buffer, int size)
{
	if(buffer == NULL || size <= 0)
		return NULL;

	pgeWav *wav = (pgeWav*)malloc(sizeof(pgeWav) + (size_t)size);
	if(wav == NULL)
		return NULL;

	unsigned char *wavfile = (unsigned char*)(wav + 1);
	memcpy(wavfile, buffer, (size_t)size);

	return pgeWavLoadInternal(wav, wavfile, size);
}

void pgeWavUnload(pgeWav *wav)
{
	if(wav != NULL)
		free(wav);
}
