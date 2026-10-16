#include <Aurora/Utils/Logger.h>

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

namespace Aurora
{
	namespace Utils
	{
		Logger Logger::m_Logger;

	// Closing a file on the memory stick does not write the cached directory and FAT entries to the
	// card -- the console does that lazily. An unhandled hardware exception powers the console off
	// before that happens, so without this the log of a crash on real hardware is exactly the log
	// that gets lost, which is how this game spent a long time looking like it was never executed
	// at all. Syncing here makes every line on the card current as of when it was written.
	//
	// This is the single implementation of that flush; Logger.h explains why it is public and which
	// other caller depends on it. The log writes below call it after every line, which is a device
	// flush per line. The game only logs at milestones (see Scripts/DebugConfig.h), so that is
	// affordable during the investigation and this call can be dropped once a console build is being
	// shipped for play rather than for debugging.
	void Logger::SyncCard()
	{
		sceIoSync("ms0:", 0);
		sceIoSync("msfat0:", 0);
	}

	static void FlushCard()
	{
		Logger::SyncCard();
	}

		// Two names on purpose. Log.txt is the one everything already greps. UPDLOG.TXT is 8.3 with
		// no long-name, so Windows can still open it when a yanked card leaves Log.txt as a
		// directory entry the PC cannot read (the `]OG~1.TXT` leftover). Same bytes in both.
		static const char* const kLogNames[] = { "Log.txt", "UPDLOG.TXT" };
		static const int kLogNameCount = 2;

		static void WriteLogs(int flags, const char* bytes, int length)
		{
			for (int i = 0; i < kLogNameCount; ++i)
			{
				SceUID file = sceIoOpen(kLogNames[i], flags, 0777);
				if (file >= 0)
				{
					if (bytes != 0 && length > 0)
					{
						sceIoWrite(file, bytes, length);
					}
					sceIoClose(file);
				}
			}
			FlushCard();
		}

		// Truncates Log.txt so the file only ever describes the current run. Used by the
		// development build at boot (see Scripts/DebugConfig.h); the game otherwise appends
		// forever, which makes a crash log a needle in tens of thousands of lines.
		void Logger::ResetLog()
		{
			WriteLogs(PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0, 0);
		}

		void Logger::LogMessage(const char *message, ...)
		{
			va_list argList;
			char cbuffer[1024];
			va_start(argList, message);
			vsnprintf(cbuffer, 1024, message, argList);
			va_end(argList);

			WriteLogs(PSP_O_APPEND | PSP_O_CREAT | PSP_O_WRONLY, cbuffer, (int)strlen(cbuffer));
		}
	}
}
