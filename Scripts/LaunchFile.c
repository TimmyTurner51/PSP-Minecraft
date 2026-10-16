// The launcher's half of the launch handoff. See Scripts/LaunchDir.h for what the handoff is for, and
// for why the two halves are separate objects compiled into separate programs.
//
// This file is C, not C++, so that it has no dependency on the C++ runtime -- one of the two programs
// that links it is entered by loadexec and should not have to bring anything up before it can write a
// file. It uses sceIo directly and never Logger.

#include "LaunchDir.h"

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <string.h>

// See the note in the header: one list, shared with the reader, so the two cannot disagree about where
// the handoff lives.
const char* const MC_LaunchFilePaths[MC_LAUNCH_FILE_COUNT] =
{
	"ms0:/" MC_LAUNCH_FILE_NAME,
	"ef0:/" MC_LAUNCH_FILE_NAME
};

// Long enough for a full ms0: path. A longer one is truncated rather than overrunning, and a truncated
// path simply fails the open below.
#define LAUNCH_PATH_SIZE 192

int MC_WriteLaunchDir(const char* absoluteDir)
{
	const char* colon;
	const char* name;
	char path[LAUNCH_PATH_SIZE];
	int deviceLength;
	int at;
	int length;
	int written;
	SceUID fd;

	if (absoluteDir == 0 || absoluteDir[0] == '\0')
	{
		return -1;
	}

	// Written on the same device as the directory it names, so the pair can never disagree about which
	// storage the install is on.
	colon = strchr(absoluteDir, ':');
	if (colon == 0)
	{
		return -2;
	}

	deviceLength = (int)(colon - absoluteDir) + 1;	// includes the colon
	if (deviceLength + 2 + (int)strlen(MC_LAUNCH_FILE_NAME) >= LAUNCH_PATH_SIZE)
	{
		return -3;
	}

	memcpy(path, absoluteDir, (size_t)deviceLength);
	path[deviceLength] = '/';

	at = deviceLength + 1;
	name = MC_LAUNCH_FILE_NAME;
	while (*name != '\0')
	{
		path[at++] = *name++;
	}
	path[at] = '\0';

	fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0)
	{
		return (int)fd;
	}

	length = (int)strlen(absoluteDir);
	written = sceIoWrite(fd, absoluteDir, length);

	// Terminated so the reader can treat the file as a line without checking the read count. The reader
	// strips this anyway; it is here so a hand-edited file behaves the same as a written one.
	sceIoWrite(fd, "\n", 1);
	sceIoClose(fd);

	return (written == length) ? 0 : -4;
}

void MC_ClearLaunchDir(void)
{
	int i;
	for (i = 0; i < MC_LAUNCH_FILE_COUNT; ++i)
	{
		sceIoRemove(MC_LaunchFilePaths[i]);
	}
}
