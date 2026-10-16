// The game's half of the launch handoff. See Scripts/LaunchDir.h for what the handoff is for, and for
// why the two halves are separate objects compiled into separate programs.
//
// WHY THIS IS A CONSTRUCTOR AND NOT A LINE IN main()
//
// The obvious place for a chdir is the top of main(). It is too late. Scripts/BootTrace.cpp opens
// trace.txt from ITS constructor, which runs before main() is entered -- so a chdir in main() would
// leave trace.txt (and therefore the only record of the run) outside the game folder, in whatever
// directory loadexec happened to leave behind, or nowhere at all. Doing it here puts the chdir before
// the first file the program opens.
//
// WHICH CONSTRUCTOR RUNS FIRST is not the order they appear in a file; the runtime walks .ctors
// BACKWARDS, so the object listed LAST in OBJS has its constructor run first. MakefileGame lists this
// object last for exactly that reason, the same arrangement (and the same measurement) that places
// Scripts/BootTrace.o last and the launcher's FpuInit.o last.
//
// This file is C, not C++, so that its constructor cannot depend on the C++ runtime having
// initialised. It uses sceIo directly and never Logger, which needs the working directory this file is
// still in the middle of establishing.

#include "LaunchDir.h"

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <string.h>

#include "InitTrace.h"

// Long enough for a full ms0: path plus a deep install folder. A longer one is truncated rather than
// overrunning, and a truncated path simply fails the chdir below.
#define LAUNCH_DIR_SIZE 192

static void StripTrailingSpace(char* text)
{
	int length = (int)strlen(text);

	while (length > 0)
	{
		char last = text[length - 1];
		if (last != '\n' && last != '\r' && last != ' ' && last != '\t')
			break;
		text[length - 1] = '\0';
		length--;
	}
}

void MC_ApplyLaunchDir(void)
{
	int i;
	for (i = 0; i < MC_LAUNCH_FILE_COUNT; ++i)
	{
		char dir[LAUNCH_DIR_SIZE];
		int got;
		int result;
		SceUID fd = sceIoOpen(MC_LaunchFilePaths[i], PSP_O_RDONLY, 0777);

		if (fd < 0)
		{
			continue;
		}

		got = sceIoRead(fd, dir, LAUNCH_DIR_SIZE - 1);
		sceIoClose(fd);

		if (got <= 0)
		{
			// An empty file cannot be acted on and must not be left behind to be re-read next boot.
			sceIoRemove(MC_LaunchFilePaths[i]);
			continue;
		}

		dir[got] = '\0';
		StripTrailingSpace(dir);

		if (dir[0] == '\0')
		{
			sceIoRemove(MC_LaunchFilePaths[i]);
			continue;
		}

		result = sceIoChdir(dir);

		// Consumed on use, whatever the outcome. A launch file that survives a failed chdir would
		// silently redirect the NEXT run -- including one started from the XMB, which already has the
		// right directory and must not be moved out of it.
		sceIoRemove(MC_LaunchFilePaths[i]);

		// Traced AFTER the chdir, so these lines land in the game folder's trace.txt rather than beside
		// wherever loadexec left the process. This is the first thing written in a normal run.
		MC_TraceStr("launchdir: chdir to", dir);
		MC_TraceInt("launchdir: chdir result", result);
		return;
	}

	// No handoff: started from the XMB, where the directory is already right. Worth a line, because a
	// run that says "none" and then cannot find Assets is a different bug from one that chdir'd wrong.
	MC_Trace("launchdir: none, keeping the inherited directory");
}

// See the note at the top: this object is listed LAST in MakefileGame so this runs FIRST.
__attribute__((constructor)) static void LaunchDirConstructor(void)
{
	MC_ApplyLaunchDir();
}
