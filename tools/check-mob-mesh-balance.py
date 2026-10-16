"""Sanity-check the mob mesh builders in CraftWorld_MobMeshes.cpp.

Each mesh section fills three scratch vectors and then frees them. Because the
cleanup loops used to be bounded on mPosition.size() while indexing mtextures
too, a section that pushed unequal numbers of positions and textures would read
past mtextures.size() and free a stale pointer that an earlier section had
already deleted -- a heap double free that surfaced as an intermittent crash
inside _malloc_r during world generation. (pigLegVertices was exactly that.)

Run this after adding or editing a mob mesh section:

    python tools/check-mob-mesh-balance.py
"""

import re
import sys
from pathlib import Path

DEFAULT = (
    Path(__file__).resolve().parent.parent / "Scripts" / "CraftWorld_MobMeshes.cpp"
)

SECTION_RE = re.compile(r"^\s*///\s*\*?\s*---\s*(.+?)\s*---")


def main() -> int:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT
    lines = path.read_text(errors="replace").split("\n")

    name = "(top)"
    counts = {"pos": 0, "tex": 0, "tri": 0}
    sections = []

    for line in lines:
        m = SECTION_RE.match(line)
        if m:
            sections.append((name, dict(counts)))
            name = m.group(1).strip()
            counts = {"pos": 0, "tex": 0, "tri": 0}
            continue
        counts["pos"] += line.count("mPosition.push_back")
        counts["tex"] += line.count("mtextures.push_back")
        counts["tri"] += line.count("mTriangle.push_back")
    sections.append((name, dict(counts)))

    print(f"{'section':38s} {'positions':>9s} {'textures':>9s} {'tris':>6s}")
    bad = 0
    for name, c in sections:
        if c["pos"] == 0 and c["tex"] == 0:
            continue
        flag = ""
        if c["pos"] != c["tex"]:
            flag += f"  <-- positions != textures ({c['pos'] - c['tex']:+d})"
        # every quad is 4 verts / 2 triangles, so positions and textures must be
        # multiples of 4 and the triangle count a multiple of 2
        if c["pos"] % 4 or c["tex"] % 4 or c["tri"] % 2:
            flag += "  [not quad-aligned]"
        if flag:
            bad += 1
        print(f"{name:38s} {c['pos']:9d} {c['tex']:9d} {c['tri']:6d}{flag}")

    print(f"\n{bad} section(s) with a problem")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
