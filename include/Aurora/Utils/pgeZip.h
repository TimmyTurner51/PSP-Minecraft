/*
 * pgeZip.h: Header for Zip file access
 *
 * This file is part of "Phoenix Game Engine".
 *
 * Copyright (C) 2008 Phoenix Game Engine
 * Copyright (C) 2008 InsertWittyName <tias_dp@hotmail.com>
 * Copyright (C) 2008 MK2k <pge@mk2k.net>
 *
 * Phoenix Game Engine is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 
 * Phoenix Game Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with Phoenix Game Engine.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __PGEZIP_H__
#define __PGEZIP_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup pgeZip Zip Library
 *  @{
 */

#include <zlib.h>

/**
 * A zip
 */
typedef void pgeZip;

/**
 * A file within a zip
 */
typedef struct
{
	unsigned char *data;	/**<  The file data */
	int size;				/**<  Size of the data */
	
} pgeZipFile;

/**
 * Open a Zip file
 *
 * @param filename - Path of the zip to load.
 *
 * @returns A pointer to a ::pgeZip struct or NULL on error.
 */
pgeZip* pgeZipOpen(const char *filename);

/**
 * Close a Zip file
 *
 * @param zip - A valid (previously opened) ::pgeZip
 *
 * @returns 1 on success, 0 on error
 */
int pgeZipClose(pgeZip *zip);

/**
 * Read a file from a zip
 *
 * @param zip - A valid (previously opened) ::pgeZip
 *
 * @param filename - The file to read within the zip
 *
 * @param password - The password of the file (pass NULL if no password)
 *
 * @returns A ::pgeZipFile struct containing the file
 */
pgeZipFile* pgeZipFileRead(pgeZip *zip, const char *filename, const char *password);

/**
 * Begin reading one entry as a stream, holding only the decoder's own window in memory.
 *
 * WHY THIS EXISTS ALONGSIDE pgeZipFileRead
 * ----------------------------------------
 * pgeZipFileRead allocates the entry's ENTIRE uncompressed size in one malloc, so a caller's memory
 * requirement is set by the largest single file in the archive. On a PSP that is a real ceiling: the
 * launchable heap is around 15 MB, and a 12.7 MB asset cannot be extracted through that API at all,
 * however much room the rest of the archive would leave. It fails as an allocation failure, which
 * surfaces later as "the file is missing" rather than as anything mentioning memory.
 *
 * These three calls are the same decoder, driven a chunk at a time, so extracting a 12.7 MB file costs
 * a 32 KB buffer instead of 12.7 MB. The entry is located by name exactly as pgeZipFileRead does.
 *
 * Usage: open, then read until it returns 0, then close. An entry may be reopened, and opening another
 * implicitly abandons the previous one.
 *
 * @param zip       - A valid (previously opened) ::pgeZip
 * @param filename  - The entry to read, by that exact name
 * @param password  - The file's password, or NULL
 * @param outSize   - Receives the entry's uncompressed size, if not NULL
 *
 * @returns PGE_ZIP_OK (0) on success, negative on error
 */
int pgeZipStreamOpen(pgeZip *zip, const char *filename, const char *password, unsigned int *outSize);

/**
 * Read the next chunk of the entry opened by pgeZipStreamOpen.
 *
 * @param buf - Where to put the bytes
 * @param len - How many bytes to ask for; fewer may be returned
 *
 * @returns the number of bytes read, 0 at end of entry, negative on error
 */
int pgeZipStreamRead(pgeZip *zip, void *buf, unsigned int len);

/**
 * Finish the entry opened by pgeZipStreamOpen, releasing the decoder state for it.
 */
int pgeZipStreamClose(pgeZip *zip);

/**
 * Extract all files from a zip
 *
 * @param zip - A valid (previously opened) ::pgeZip file
 *
 * @param password - The password of the file (pass NULL if no password)
 *
 * @returns 1 on success, 0 on error.
 */
int pgeZipExtract(pgeZip *zip, const char *password);

/**
 * Extract all files from a zip, and report how many were written.
 *
 * Unlike pgeZipExtract, which returns a constant 1 and therefore cannot fail, this returns 1 only if
 * every entry was written, and fills in the counts. Prefer it anywhere the result matters: an install
 * that wrote 112 of 115 files must not be reported to the player as a success.
 *
 * @param zip - A valid (previously opened) ::pgeZip file
 *
 * @param password - The password of the file (pass NULL if no password)
 *
 * @param okCount - Optional; receives the number of entries written
 *
 * @param failCount - Optional; receives the number of entries that failed
 *
 * @returns 1 if every entry was written, 0 otherwise.
 */
int pgeZipExtractCounted(pgeZip *zip, const char *password, int *okCount, int *failCount);

/**
 * Free the file data previously loaded from a zip
 *
 * @param file - A valid (previously read) ::pgeZipFile
 */
void pgeZipFileFree(pgeZipFile *file);

/** @} */

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __PGEZIP_H__
