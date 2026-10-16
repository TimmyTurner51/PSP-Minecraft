#ifndef AURORA_LOGGER_H
#define AURORA_LOGGER_H

namespace Aurora
{
	namespace Utils
	{

		class Logger
		{
		private:

			static Logger m_Logger;

		public:

			static Logger* Instance()
			{
				return &m_Logger;
			}

			void LogMessage(const char *message, ...);

			// Empties Log.txt (development build, called once at boot).
			void ResetLog();

			// Pushes everything the card driver is still holding in RAM out to the memory stick.
			//
			// Needed because this console does NOT write a file's directory and FAT entries when the
			// file is closed -- it does that lazily, and an unhandled exception powers the console off
			// before "lazily" arrives. LogMessage calls this after every line for exactly that reason
			// (see Logger.cpp), and it is public because the same hazard applies to any file a caller
			// intends something else to open immediately afterwards. The installer is the caller that
			// matters: it writes a new EBOOT.PBP and hands it straight to loadexec, and loadexec reads
			// the file's size and cluster chain from the directory rather than from the cache.
			static void SyncCard();

		};
	}
}

#endif
