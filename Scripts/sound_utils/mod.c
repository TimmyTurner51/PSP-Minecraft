#include "oslib.h"

/*
 * Old OSLib MOD player targeted MikMod 3.1 (UNIMOD, MikMod_LoadSong, a
 * different MDRIVER layout). Current PSPDEV ships libmikmod 3.3, which
 * does not compile that driver. WAV / BGM / AT3 / MP3 still work.
 * Return NULL so callers treat MOD as "file not loaded".
 */

void oslSetModSampleRate(int freq, int stereo, int shift)
{
	(void)freq;
	(void)stereo;
	(void)shift;
}

OSL_SOUND *oslLoadSoundFileMOD(const char *filename, int stream)
{
	(void)filename;
	(void)stream;
	return NULL;
}
