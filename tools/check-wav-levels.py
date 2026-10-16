"""Report peak / RMS level of the game's WAV assets.

Useful because the mixer in Scripts/sound_utils/pgeWav.c sums every playing voice
into one integer: a sample already near full scale leaves no headroom, so it clips
as soon as anything else plays with it. Peak here says how much room a file leaves.

    python tools/check-wav-levels.py
    python tools/check-wav-levels.py "Assets/SoundsLow"

Output is sorted loudest-first, with a summary of how many files sit at or above
full scale (>= 32700, i.e. no headroom at all).
"""
import glob
import os
import struct
import sys
import wave

ROOT = sys.argv[1] if len(sys.argv) > 1 else "Assets/Sounds"

rows = []
for path in sorted(glob.glob(os.path.join(ROOT, "**", "*.wav"), recursive=True)):
    try:
        w = wave.open(path)
        n, sw = w.getnframes(), w.getsampwidth()
        data = w.readframes(n)
        ch, rate = w.getnchannels(), w.getframerate()
        w.close()
    except Exception as e:
        rows.append((path, None, None, None, None, str(e)))
        continue
    if sw != 2:
        rows.append((path, None, None, None, None, f"{sw * 8}-bit not analysed"))
        continue
    vals = struct.unpack("<%dh" % (len(data) // 2), data)
    peak = max(abs(v) for v in vals) if vals else 0
    rms = (sum(v * v for v in vals) / len(vals)) ** 0.5 if vals else 0.0
    rows.append((path, peak, peak / 327.68, rms, rate, f"{ch}ch"))

rows.sort(key=lambda r: (r[1] is None, -(r[1] or 0)))
print(f"{'peak':>6s} {'peak%':>6s} {'rms%':>6s}  {'format':12s} file")
print("-" * 78)
for path, peak, pct, rms, rate, note in rows:
    rel = os.path.relpath(path, ROOT).replace("\\", "/")
    if peak is None:
        print(f"{'--':>6s} {'--':>6s} {'--':>6s}  {note:12s} {rel}")
    else:
        print(f"{peak:6d} {pct:6.1f} {rms / 327.68:6.1f}  "
              f"{rate / 1000:.1f}k{note:4s} {rel}")

clipped = [r for r in rows if r[1] is not None and r[1] >= 32700]
print()
print(f"{len(rows)} files, {len(clipped)} at or above full scale "
      f"(>=32700, i.e. no headroom to mix with)")
