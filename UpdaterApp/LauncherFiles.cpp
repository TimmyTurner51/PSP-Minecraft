#include "LauncherFiles.h"

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspthreadman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <Aurora/Utils/Logger.h>

using namespace Aurora::Utils;

// One directory entry's name. The FAT driver's own limit is 255, so this is a ceiling rather than a
// guess, and no path built from it can overflow the buffers below.
#define NAME_MAX_LOCAL 256
#define PATH_MAX_LOCAL 512

// The crash breadcrumb, beside the launcher. See LauncherPhase.
#define PHASE_FILE "phase.txt"

std::string Join(const std::string& a, const std::string& b)
{
	if (a.empty())
	{
		return b;
	}
	if (b.empty())
	{
		return a;
	}

	std::string out = a;
	if (out[out.size() - 1] == '/')
	{
		out.erase(out.size() - 1);
	}
	out += "/";
	out += b;
	return out;
}

std::string BaseName(const std::string& path)
{
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
	{
		return path;
	}
	return path.substr(slash + 1);
}

std::string LauncherRoot()
{
	char buffer[PATH_MAX_LOCAL];
	if (getcwd(buffer, sizeof(buffer)) == 0)
	{
		Logger::Instance()->LogMessage("launcher: getcwd FAILED, there is no root to work from\n");
		return "";
	}

	std::string root = buffer;

	// getcwd returns a trailing slash on this platform's device root ("ms0:/"), so the strip is
	// load-bearing rather than tidy: Join() would otherwise produce "ms0://PSP/..." and every path
	// built from it would be wrong in the same way at once.
	while (root.size() > 1 && root[root.size() - 1] == '/')
	{
		root.erase(root.size() - 1);
	}

	return root;
}

bool FileExists(const std::string& path)
{
	SceUID fd = sceIoOpen(path.c_str(), PSP_O_RDONLY, 0777);
	if (fd < 0)
	{
		return false;
	}
	sceIoClose(fd);
	return true;
}

bool ReadTextFile(const std::string& path, std::string* out)
{
	out->clear();

	SceUID fd = sceIoOpen(path.c_str(), PSP_O_RDONLY, 0777);
	if (fd < 0)
	{
		return false;
	}

	// 4 KB is generous for everything this reads -- one URL line, or an install descriptor of four
	// lines -- and the bound is what stops a corrupt or enormous file being read into the launcher's
	// 8 MB heap.
	char buffer[4096];
	int got = sceIoRead(fd, buffer, sizeof(buffer) - 1);
	sceIoClose(fd);

	if (got <= 0)
	{
		return false;
	}

	buffer[got] = '\0';
	out->assign(buffer);
	return true;
}

bool WriteTextFile(const std::string& path, const std::string& text)
{
	// Written beside the target and renamed over it. The rename is atomic on FAT for our purposes, so
	// a console that loses power mid-write leaves the OLD file rather than a truncated one -- which for
	// install.txt is the difference between "the previous metadata" and "an install that cannot be
	// described and therefore cannot be selected".
	std::string temp = path + ".tmp";

	SceUID fd = sceIoOpen(temp.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0)
	{
		return false;
	}

	int written = sceIoWrite(fd, text.data(), (unsigned int)text.size());
	sceIoClose(fd);

	if (written != (int)text.size())
	{
		sceIoRemove(temp.c_str());
		return false;
	}

	// Remove first: sceIoRename fails when the destination exists on some CFW drivers, and the window
	// where neither file exists is microseconds on a card that is about to be written anyway.
	sceIoRemove(path.c_str());

	if (sceIoRename(temp.c_str(), path.c_str()) < 0)
	{
		sceIoRemove(temp.c_str());
		return false;
	}

	return true;
}

bool MakeDir(const std::string& path)
{
	return sceIoMkdir(path.c_str(), 0777) >= 0;
}

// One level of a recursive walk. Returns false only on an I/O error worth reporting; a directory that
// is already absent counts as success, so callers can delete optimistically.
static bool RemoveTreeInner(const std::string& path, unsigned int* removed, bool* wasDirectory)
{
	*wasDirectory = false;

	SceUID dir = sceIoDopen(path.c_str());
	if (dir >= 0)
	{
		*wasDirectory = true;

		SceIoDirent entry;
		while (true)
		{
			memset(&entry, 0, sizeof(entry));
			if (sceIoDread(dir, &entry) <= 0)
			{
				break;
			}

			if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
			{
				continue;
			}

			std::string child = Join(path, entry.d_name);

			if (FIO_S_ISDIR(entry.d_stat.st_mode))
			{
				bool childWasDir = false;
				RemoveTreeInner(child, removed, &childWasDir);
				continue;
			}

			if (sceIoRemove(child.c_str()) >= 0 && removed != 0)
			{
				(*removed)++;
			}
		}

		sceIoDclose(dir);

		// The children are gone by now, so this removes an empty directory rather than failing on a
		// non-empty one. That distinction is the whole bug surface here: the files come off the card one
		// by one and the directory is the part that refuses to go, so a re-listing cannot tell "the card
		// said no" apart from "the walk never got here". Report the code either way.
		//
		// Retried because the card is write-behind and the parent's removal can still be in flight; the
		// delay is only paid on the attempt that failed, so the common case costs nothing.
		bool ok = false;
		for (int attempt = 0; attempt < 3 && !ok; ++attempt)
		{
			int result = sceIoRmdir(path.c_str());
			ok = (result >= 0);

			if (!ok)
			{
				Logger::Instance()->LogMessage("launcher: rmdir '%s' attempt %d -> 0x%08X\n",
					path.c_str(), attempt + 1, (unsigned int)result);
				SyncCard();
				sceKernelDelayThread(30000);
			}
		}

		return ok;
	}

	// Not a directory: a plain file, or nothing at all.
	if (sceIoRemove(path.c_str()) >= 0 && removed != 0)
	{
		(*removed)++;
	}
	return true;
}

bool RemoveTree(const std::string& path, unsigned int* removed)
{
	bool wasDirectory = false;
	RemoveTreeInner(path, removed, &wasDirectory);

	/*
	 * The card is write-behind, and a directory cannot be removed while the driver still believes it
	 * has children. On a memory stick those two can reach the hardware in the wrong order, and the
	 * result is debris that is worse than a failed delete: a folder skeleton with every directory still
	 * in it and no files, which the menu then lists as an install -- with a version in its install.txt
	 * if that survived -- and which the player can select and cannot start. That is exactly the shape
	 * the install failure path deletes a half-written install to avoid.
	 *
	 * One SyncCard and one retry closes it. The sync is not free, so it only happens on the path that
	 * would otherwise leave something behind, which is the rare one.
	 */
	if (wasDirectory)
	{
		SceUID still = sceIoDopen(path.c_str());
		if (still >= 0)
		{
			sceIoDclose(still);
			SyncCard();
			RemoveTreeInner(path, removed, &wasDirectory);
		}
	}

	// What actually happened, rather than what was attempted. The caller uses this to decide whether it
	// can tell the player the install is gone, and a delete that leaves a folder must not report success
	// -- the menu would then show an install the player thinks they removed.
	SceUID leftover = sceIoDopen(path.c_str());
	if (leftover >= 0)
	{
		sceIoDclose(leftover);
		return false;
	}

	return true;
}

// The size and count walk, done twice by the callers (once to size the bar, once to copy) rather than
// cached, because a cached total that disagrees with the tree is a bar that stops at 97%.
static void TreeWalk(const std::string& path, FileSize* bytes, unsigned int* files)
{
	SceUID dir = sceIoDopen(path.c_str());
	if (dir < 0)
	{
		// A file, not a directory: count it.
		SceIoStat stat;
		memset(&stat, 0, sizeof(stat));
		if (sceIoGetstat(path.c_str(), &stat) >= 0)
		{
			if (bytes != 0) { *bytes += (FileSize)stat.st_size; }
			if (files != 0) { (*files)++; }
		}
		return;
	}

	SceIoDirent entry;
	while (true)
	{
		memset(&entry, 0, sizeof(entry));
		if (sceIoDread(dir, &entry) <= 0)
		{
			break;
		}

		if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
		{
			continue;
		}

		TreeWalk(Join(path, entry.d_name), bytes, files);
	}

	sceIoDclose(dir);
}

FileSize TreeSizeBytes(const std::string& path)
{
	FileSize bytes = 0;
	TreeWalk(path, &bytes, 0);
	return bytes;
}

unsigned int TreeFileCount(const std::string& path)
{
	unsigned int files = 0;
	TreeWalk(path, 0, &files);
	return files;
}

// One file, streamed. A whole file in RAM is what the Updater's install path does and it is the reason
// its heap has a 4.9 MB floor; a copy has no such need, so this moves a fixed buffer instead and the
// memory it uses does not depend on the largest file on the card.
static bool CopyFile(const std::string& src, const std::string& dst, FileSize* copied)
{
	SceUID in = sceIoOpen(src.c_str(), PSP_O_RDONLY, 0777);
	if (in < 0)
	{
		Logger::Instance()->LogMessage("launcher: copy cannot read '%s'\n", src.c_str());
		return false;
	}

	SceUID out = sceIoOpen(dst.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (out < 0)
	{
		sceIoClose(in);
		Logger::Instance()->LogMessage("launcher: copy cannot write '%s'\n", dst.c_str());
		return false;
	}

	static char buffer[32 * 1024];
	bool ok = true;

	while (true)
	{
		int got = sceIoRead(in, buffer, sizeof(buffer));
		if (got < 0)
		{
			ok = false;
			break;
		}
		if (got == 0)
		{
			break;
		}

		int put = sceIoWrite(out, buffer, (unsigned int)got);
		if (put != got)
		{
			ok = false;
			break;
		}

		if (copied != 0)
		{
			*copied += (FileSize)got;
		}
	}

	sceIoClose(out);
	sceIoClose(in);

	if (!ok)
	{
		// A truncated destination is worse than no destination: it looks like a real file to every
		// check downstream.
		sceIoRemove(dst.c_str());
	}
	return ok;
}

static bool CopyTreeInner(const std::string& src, const std::string& dst,
						  CopyProgressFn progress, void* ctx,
						  FileSize* done, FileSize total)
{
	SceUID dir = sceIoDopen(src.c_str());
	if (dir < 0)
	{
		return false;
	}

	SceIoDirent entry;
	while (true)
	{
		memset(&entry, 0, sizeof(entry));
		if (sceIoDread(dir, &entry) <= 0)
		{
			break;
		}

		if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
		{
			continue;
		}

		std::string from = Join(src, entry.d_name);
		std::string to = Join(dst, entry.d_name);

		if (FIO_S_ISDIR(entry.d_stat.st_mode))
		{
			sceIoMkdir(to.c_str(), 0777);
			if (!CopyTreeInner(from, to, progress, ctx, done, total))
			{
				sceIoDclose(dir);
				return false;
			}
			continue;
		}

		if (progress != 0 && !progress(ctx, *done, total, entry.d_name))
		{
			sceIoDclose(dir);
			return false;
		}

		if (!CopyFile(from, to, done))
		{
			sceIoDclose(dir);
			return false;
		}
	}

	sceIoDclose(dir);
	return true;
}

bool CopyTree(const std::string& src, const std::string& dst, CopyProgressFn progress, void* ctx)
{
	sceIoMkdir(dst.c_str(), 0777);

	FileSize done = 0;
	FileSize total = TreeSizeBytes(src);

	Logger::Instance()->LogMessage("launcher: copying '%s' to '%s' (%u bytes, %u files)\n",
		src.c_str(), dst.c_str(), (unsigned int)total, TreeFileCount(src));

	return CopyTreeInner(src, dst, progress, ctx, &done, total);
}

void SyncCard()
{
	// Delegated to Logger rather than issuing the device call here. The Updater already had to work
	// out what actually flushes this card -- the observation was a removed handoff file reaching the
	// card while the package following it did not -- and that answer lives in Logger::SyncCard, in the
	// module that opens files for a living. A second copy here would be a second answer.
	Logger::SyncCard();
}

void LauncherPhase(const char* what)
{
	/*
	 * The free heap goes in the file too, not just the step name.
	 *
	 * A crash this early in the boot cannot be reproduced in the emulator -- PPSSPP's memory and its
	 * networking are both unlike the console's -- so the breadcrumb has to carry as much as one small
	 * write can. The step says where it stopped; the two memory numbers say whether it stopped there
	 * because it could not allocate, which is the single most likely reason for a launcher that dies
	 * part way through a network operation on hardware.
	 *
	 * sceKernelTotalFreeMemSize (everything free) and sceKernelMaxFreeMemSize (the largest single
	 * block) together separate "not much left" from "fragmented", which are different failures with
	 * the same symptom.
	 */
	char line[192];
	snprintf(line, sizeof(line), "%s\nfree=%d largest=%d\n",
		what, sceKernelTotalFreeMemSize(), sceKernelMaxFreeMemSize());

	SceUID file = sceIoOpen(PHASE_FILE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (file >= 0)
	{
		sceIoWrite(file, line, strlen(line));
		sceIoClose(file);
	}

	// Flushed like a log line, because the point of this file is to survive the next instruction.
	// Logger writes "Log.txt" relative to the working directory and this is the same directory.
	SyncCard();
}
