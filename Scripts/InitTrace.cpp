// ---------------------------------------------------------------------------
// Implementation of the boot tracer. See Scripts/InitTrace.h for why it exists and how to read its
// output. The constraint that shapes this file is that it must run before anything else is working:
// no newlib, no C++ runtime, no Logger, no allocation. It is a static buffer, a file handle opened on
// first use, and small formatters.
// ---------------------------------------------------------------------------

#include "InitTrace.h"

#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stddef.h>

namespace
{
	// Its own file, so this firehose never has to be read past the last line or two and Log.txt stays
	// the readable narrative. Truncated once per boot, on the first call, which is from the constructor
	// in Scripts/BootTrace.cpp -- so the file always describes exactly the run that produced it.
	const char* const kTraceFile = "trace.txt";

	SceUID gFd = -1;

	// Distinguishes "not opened yet" from "opened and failed". Without this, a console where the open
	// fails would retry the open on every single trace call, turning a diagnosable failure into a slow
	// one.
	bool gOpenAttempted = false;

	unsigned int gSequence = 0;

	// Long enough for the longest tag below plus a path; anything beyond it is dropped rather than
	// overrunning, because a truncated line is still a breadcrumb and a smashed stack is not.
	const int kLineSize = 320;

	// Bounded append: copies at most "room" bytes and always terminates. Returns the new write cursor.
	char* AppendStr(char* out, const char* text, int room)
	{
		if(text == 0)
			return out;

		while(*text != '\0' && room > 1)
		{
			*out++ = *text++;
			room--;
		}
		*out = '\0';
		return out;
	}

	char* AppendUInt(char* out, unsigned int value)
	{
		char digits[10];
		int count = 0;

		if(value == 0)
		{
			*out++ = '0';
			return out;
		}

		while(value > 0)
		{
			digits[count++] = (char)('0' + (value % 10));
			value /= 10;
		}

		while(count > 0)
			*out++ = digits[--count];

		return out;
	}

	char* AppendInt(char* out, int value)
	{
		if(value < 0)
		{
			*out++ = '-';
			// Negated in unsigned arithmetic: -INT_MIN is undefined, and this register is exactly
			// where a crash report is likely to carry a nonsense value.
			return AppendUInt(out, 0u - (unsigned int)value);
		}
		return AppendUInt(out, (unsigned int)value);
	}

	char* AppendHex(char* out, unsigned int value)
	{
		static const char kDigits[] = "0123456789abcdef";

		*out++ = '0';
		*out++ = 'x';

		int shift = 28;
		while(shift >= 0)
		{
			*out++ = kDigits[(value >> shift) & 0xF];
			shift -= 4;
		}
		return out;
	}

	void OpenIfNeeded()
	{
		if(gOpenAttempted)
			return;

		gOpenAttempted = true;
		gFd = sceIoOpen(kTraceFile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	}

	// One line: "[ms] seq tag extra\n", then straight to the card.
	void Emit(const char* tag, const char* extra)
	{
		OpenIfNeeded();
		if(gFd < 0)
			return;

		char line[kLineSize];
		char* cursor = line;

		*cursor++ = '[';
		// Milliseconds since boot. sceKernelGetSystemTimeLow wraps after about 71 minutes, which is
		// far beyond any value a startup trace can reach.
		cursor = AppendUInt(cursor, (unsigned int)(sceKernelGetSystemTimeLow() / 1000u));
		*cursor++ = ']';
		*cursor++ = ' ';
		cursor = AppendUInt(cursor, gSequence++);
		*cursor++ = ' ';

		// The remaining room is recomputed at each step rather than tracked, so a change to one of the
		// prefixes above cannot silently stop bounding the copy below.
		cursor = AppendStr(cursor, tag, kLineSize - (int)(cursor - line));

		if(extra != 0)
			cursor = AppendStr(cursor, extra, kLineSize - (int)(cursor - line));

		// AppendStr left the terminator in place; overwrite it with the newline and re-terminate, so
		// the write length is the visible length.
		*cursor++ = '\n';

		sceIoWrite(gFd, line, (unsigned int)(cursor - line));

		// The point of the file. See the header: closing is not flushing on this filesystem, and the
		// crash this is here to catch is exactly the kind that never unwinds far enough to flush.
		sceIoSync("ms0:", 0);
		sceIoSync("msfat0:", 0);
	}
}

extern "C" void MC_Trace(const char* tag)
{
	Emit(tag, 0);
}

extern "C" void MC_TraceInt(const char* tag, int value)
{
	char buffer[16];
	char* cursor = buffer;
	*cursor++ = '=';
	cursor = AppendInt(cursor, value);
	*cursor = '\0';
	Emit(tag, buffer);
}

extern "C" void MC_TraceUInt(const char* tag, unsigned int value)
{
	char buffer[16];
	char* cursor = buffer;
	*cursor++ = '=';
	cursor = AppendUInt(cursor, value);
	*cursor = '\0';
	Emit(tag, buffer);
}

extern "C" void MC_TracePtr(const char* tag, const void* pointer)
{
	char buffer[16];
	char* cursor = buffer;
	*cursor++ = '=';
	cursor = AppendHex(cursor, (unsigned int)(size_t)pointer);
	*cursor = '\0';
	Emit(tag, buffer);
}

extern "C" void MC_TraceStr(const char* tag, const char* text)
{
	char buffer[200];
	char* cursor = buffer;
	*cursor++ = '=';
	cursor = AppendStr(cursor, text, sizeof(buffer) - 1);
	Emit(tag, buffer);
}
