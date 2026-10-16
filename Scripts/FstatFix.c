// ---------------------------------------------------------------------------
// fstat, replaced.
//
// WHY THIS EXISTS. The game is normally started by the XMB. The launcher starts
// it with LoadExec instead, and in that case the game wedges on the very first
// fseek -- intraFontLoad's, sizing a font file. It wedges inside fstat, which is
// the call newlib makes to pick a stream's buffer size. Reproduced in PPSSPP and
// seen on hardware; the two traces match line for line.
//
// It is not the card, the kernel, or the descriptor. From one process, at the
// same moment, on the same stream:
//
//     sceIoOpen / sceIoLseek / sceIoRead   fine, exact file size
//     fopen                                fine, valid FILE*
//     lseek(fileno(fp), 0, SEEK_END)       fine, exact file size
//     fstat(fileno(fp))                    never returns        <-- here
//
// fseek only reaches fstat because the stream has no buffer yet. Give the
// stream one with setvbuf and the seek, the reads and everything after them
// work. That is the whole bug, and it is why every stream in the program
// hangs, not only the font's -- pgeWav, the save loader and the input config
// all seek, and would all have stopped at the same call.
//
// So the blocking call is removed in ONE place rather than avoided at each of
// the game's 26 fopen call sites. libcglue's own _fstat is a table lookup and a
// memset and cannot block, so what is replaced below is newlib's wrapper around
// it.
//
// WHAT THIS REPORTS. What libcglue itself reports for a file: a regular file
// with an 8 KB block size. st_blksize is the only field stdio reads from it --
// that is the buffer size -- and the game never calls fstat on its own: its
// stat() calls are the path-based kind, which this does not touch. isatty()
// will now answer no for every descriptor, which is correct for this program.
//
// REMOVING IT. If the launch path is ever changed so that fstat is no longer
// blocked, this file can go. See MakefileGame, which carries the memory
// partition note that was written while this hang was still unexplained and was
// very likely misreading it -- that note is worth re-reading before it is
// trusted again.
// ---------------------------------------------------------------------------

#include <string.h>
#include <sys/stat.h>
#include <reent.h>

int _fstat_r(struct _reent *ptr, int fd, struct stat *pstat)
{
	(void)ptr;
	(void)fd;

	if (pstat != 0)
	{
		memset(pstat, 0, sizeof(*pstat));
		pstat->st_mode = S_IFREG | 0777;
		pstat->st_blksize = 8192;
	}

	return 0;
}
