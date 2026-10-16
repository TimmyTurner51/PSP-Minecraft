#!/usr/bin/env python3
"""
Audit the section-border test: a cell on the far edge of a section must be seen as being on that edge.

Why this exists
---------------
When a block changes, the section it is in is remeshed, and if the block touches a section boundary the
*neighbouring* section has to be remeshed too -- the face the changed block was hiding is in the
neighbour's mesh, and it is only there because a cull said so at the time. Both remesh helpers asked
which neighbours that was by reading the section's own box:

    Vector3 test = mChunks[id]->bBox.onBorder(pos);

and that call cannot answer the question it is being asked. A section's box is built as
`BoundingBox(Vector3(StartX, StartY, StartZ), Vector3(StartX + CHUNK_SIZE, ...))`, so its *maximum* is the
first cell of the *next* section. `BoundingBox::onBorder` fires its `max` test on `max - pt == 0`, and no
cell of this section is ever at that point, so:

  * the `min` test works -- the first cell of a section really is at the section's minimum. Breaking a
    block at cell 0, 16, 32 ... queues the neighbour on the -X/-Y/-Z side, exactly as intended;
  * the `max` test can never fire for a cell the caller owns. Cell 15 (of a 16-cell section) is at
    `max - 1`, so `test` comes back 0 and the +X/+Y/+Z neighbour is never queued. It is dead code that
    reads as a border test, which is why it survived: half of it worked.

What that costs is a hole in the world. Dig a 1x1 shaft that straddles a 16 line -- invisible to the
player, so a shaft put down "on a corner" crosses one about half the time -- and every block removed
takes the face it was hiding off a block whose own section is never remeshed. That neighbour's mesh
still has the face culled from when the block was buried, so the face is not drawn and the block reads
as a window: one cell wide, straight down the seam, for as deep as the shaft goes. Breaking the block on
the *other* side of the same seam does not reproduce it, because that half of the test was never broken.

Checked:
  1. the positive branch of the rule compares the cell's local index against `CHUNK_SIZE - 1` and not
     against `CHUNK_SIZE` or the box's maximum -- the off-by-one in one line, read out of the source;
  2. the negative branch compares against 0, and the two branches assign -1 and +1 to the matching axis,
     so the offsets the callers add to the position are directions and not magnitudes;
  3. the local index is the cell minus the *section start*, which is the quantity the test needs. Reading
     it from the box's min/max was the bug, so a return to that shape has to fail here;
  4. the rule is *executed*: the parsed branches are run over all CHUNK_SIZE^3 cells of a section and each
     of the six faces must come back flagged exactly CHUNK_SIZE^2 times. Side by side, the old
     box-based test is run over the same cells and printed, so the numbers that prove the fix are visible
     rather than asserted -- on the old test the three positive faces come back flagged zero times;
  5. no live `onBorder(` call is left in Scripts/ (the declaration in the Aurora math library stays; it is
     a generic "is this point on this box's border" test, and it is correct for that);
  6. `AddChunkToUpdate` drops its *oldest* entry when its queue is full instead of clearing the whole
     queue. This is the same failure's second route: a full queue used to throw away every pending
     rebuild at the moment the world was changing fastest, which is a stale neighbour mesh again.
  7. the rule is *run as the caller runs it*: for all 4,096 cells, and for a 1x1 downdig down a corner
     column (the "every block along that line down" case), every section that owns a face the break
     uncovers is in the set `rebuildNearestChunks` queues -- every non-empty combination of the rule's
     per-axis offsets. The old box test leaves the positive-side faces out on every cell that has one,
     and the check names a cell and the sections it drops so the failure reads as the window it is.

Run from anywhere:  python tools/check-section-border.py
"""
import os
import re
import sys

GAME_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SCRIPTS = os.path.join(GAME_ROOT, "Scripts")
WORLD_CPP = os.path.join(SCRIPTS, "CraftWorld2.cpp")
WORLD_H = os.path.join(SCRIPTS, "CraftWorld2.h")

failures = []


def fail(msg):
    failures.append(msg)
    print("  FAIL: %s" % msg)


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def body_of(src, signature):
    """The { ... } body of a function, by brace balance from its opening brace."""
    at = src.find(signature)
    if at < 0:
        return None
    open_at = src.find("{", at)
    if open_at < 0:
        return None
    depth = 0
    for i in range(open_at, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[open_at:i + 1]
    return None


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def section_size(src):
    """CHUNK_SIZE as the code actually has it, so nothing here restates 16."""
    m = re.search(r"#define\s+CHUNK_SIZE\s+(\d+)", read(os.path.join(SCRIPTS, "StatePlay.cpp")))
    if not m:
        return None
    return int(m.group(1))


def main():
    src = read(WORLD_CPP)
    body = body_of(src, "Vector3 CraftWorld::SectionBorderOffset")
    print("SectionBorderOffset")
    if body is None:
        fail("CraftWorld::SectionBorderOffset is gone from CraftWorld2.cpp")
        return report()

    live = strip_comments(body)

    # 1/2. The six branches, read out of the source rather than restated.
    axes = {}
    for axis, var in (("x", "lx"), ("y", "ly"), ("z", "lz")):
        low = re.search(r"if\(\s*%s\s*==\s*([A-Za-z_0-9 ]+?)\s*\)\s*result\.%s\s*=\s*(-?[\d.]+f)"
                        % (var, axis), live)
        high = re.search(r"else\s+if\(\s*%s\s*==\s*([A-Za-z_0-9 \-]+?)\s*\)\s*result\.%s\s*=\s*(-?[\d.]+f)"
                         % (var, axis), live)
        if not low or not high:
            fail("axis %s: the two branches of the rule are not both present (low=%s high=%s)"
                 % (axis, bool(low), bool(high)))
            continue
        axes[axis] = (low.group(1).strip(), low.group(2), high.group(1).strip(), high.group(2))
        print("  %s: cell at %-16s -> %-5s   cell at %-16s -> %s"
              % (axis, low.group(1).strip(), low.group(2), high.group(1).strip(), high.group(2)))

        if low.group(1).strip() != "0":
            fail("axis %s: the negative branch compares against %r, not 0 -- the first cell of a section "
                 "is at local 0" % (axis, low.group(1).strip()))
        if low.group(2) != "-1.0f":
            fail("axis %s: the local-0 branch assigns %s, not -1.0f" % (axis, low.group(2)))
        if high.group(2) != "1.0f":
            fail("axis %s: the far-edge branch assigns %s, not 1.0f" % (axis, high.group(2)))

        # The off-by-one, stated as the thing that is allowed to appear here.
        if high.group(1).strip() != "CHUNK_SIZE - 1":
            fail("axis %s: the far-edge branch compares against %r. It has to be `CHUNK_SIZE - 1`: the "
                 "last cell of a section is at local CHUNK_SIZE-1, and `CHUNK_SIZE` is the first cell of "
                 "the *next* section -- which is the off-by-one that let the +X/+Y/+Z neighbour go "
                 "unremeshed and puts a one-cell x-ray down every 16 line"
                 % (axis, high.group(1).strip()))

    # 3. The local index has to come off the section start.
    for var, axis in (("lx", "X"), ("ly", "Y"), ("lz", "Z")):
        m = re.search(r"const int %s\s*=\s*([^;]+);" % var, live)
        if not m:
            fail("the local index %s is not computed in the rule" % var)
            continue
        expr = m.group(1).strip()
        print("  %s = %s" % (var, expr))
        if "chunkStart" not in expr or "-" not in expr:
            fail("%s is `%s`; it has to be the cell minus the section's chunkStart%s, because reading the "
                 "edge off the box's min/max is the bug this replaced" % (var, expr, axis))
        elif "bBox" in expr or "onBorder" in expr:
            fail("%s is derived from the section box again (`%s`)" % (var, expr))

    size = section_size(src)
    if size is None:
        fail("CHUNK_SIZE could not be read out of StatePlay.cpp")
        return report()
    print("  CHUNK_SIZE = %d (read from StatePlay.cpp)" % size)

    # 4. Execute the parsed rule over a whole section, and the old rule beside it.
    #
    # The constants are the ones read out of the source above, resolved to numbers -- not restated here.
    # That is the difference between a check that reports and a check that runs: on a tree whose
    # far-edge branch says `CHUNK_SIZE`, the counts below move and say so, instead of the script printing
    # its own idea of the rule next to a FAIL about someone else's.
    def resolve(expr):
        expr = expr.strip()
        if expr == "0":
            return 0
        if expr.replace(" ", "") == "CHUNK_SIZE":
            return size
        if expr.replace(" ", "") == "CHUNK_SIZE-1":
            return size - 1
        return None

    spec = {}
    for axis, (low_expr, low_assign, high_expr, high_assign) in axes.items():
        lo, hi = resolve(low_expr), resolve(high_expr)
        if lo is None or hi is None:
            fail("axis %s: the rule's comparison %r / %r is not a form this script can evaluate, so the "
                 "counts below would be this script's arithmetic and not the code's"
                 % (axis, low_expr, high_expr))
            lo, hi = 0, size - 1
        spec[axis] = (lo, hi, float(low_assign.rstrip("f")), float(high_assign.rstrip("f")))

    def new_rule(local):
        out = [0.0, 0.0, 0.0]
        for i, a in enumerate("xyz"):
            lo, hi, lo_v, hi_v = spec[a]
            if local[i] == lo:
                out[i] = lo_v
            elif local[i] == hi:
                out[i] = hi_v
        return tuple(out)

    def old_rule(local):
        # `mChunks[id]->bBox.onBorder(pos)` with the box [start, start + CHUNK_SIZE] and pos the cell's
        # own coordinates: fires on an exact match, in int arithmetic.
        lx, ly, lz = local
        out = [0, 0, 0]
        for i, l in enumerate((lx, ly, lz)):
            if l - 0 == 0:
                out[i] = -1
            elif size - l == 0:
                out[i] = 1
        return tuple(out)

    def face_counts(rule):
        counts = {(a, s): 0 for a in "xyz" for s in (-1, 1)}
        for lx in range(size):
            for ly in range(size):
                for lz in range(size):
                    r = rule((lx, ly, lz))
                    for i, a in enumerate("xyz"):
                        # Sign, not value: a branch that assigned something other than ±1 has already
                        # been failed above, and this loop is about *which* faces are seen.
                        if r[i] > 0:
                            counts[(a, 1)] += 1
                        elif r[i] < 0:
                            counts[(a, -1)] += 1
        return counts

    want = size * size
    new_counts = face_counts(new_rule)
    old_counts = face_counts(old_rule)
    print("  cells flagged, per face, over all %d cells of one section:" % (size ** 3))
    print("    %-6s %-10s %-10s" % ("face", "fixed", "old"))
    for a in "xyz":
        for s in (-1, 1):
            print("    %-6s %-10d %-10d" % ("%s%+d" % (a, s), new_counts[(a, s)], old_counts[(a, s)]))
    for a in "xyz":
        for s in (-1, 1):
            if new_counts[(a, s)] != want:
                fail("the rule flags the %s%+d face %d times over a section; it must be %d -- one per cell "
                     "on that face" % (a, s, new_counts[(a, s)], want))

    # And the regression this whole script is about, stated as the old test's own numbers.
    for a in "xyz":
        if old_counts[(a, 1)] != 0:
            print("  NOTE: the old box test now flags %s+1 %d times -- the box convention moved; re-read "
                  "the docstring" % (a, old_counts[(a, 1)]))

    # 5. No live onBorder call left in Scripts/.
    #
    # `onBorder(` and not `onBorder`: `SectionBorderOffset(` contains it, which is exactly the kind of
    # substring a source scan gets wrong. The negative lookbehind is the whole of why this reads a call
    # and not a name.
    call = re.compile(r"(?<!Section)\bonBorder\s*\(")
    live_onborder = []
    for name in sorted(os.listdir(SCRIPTS)):
        if not name.endswith((".cpp", ".h")):
            continue
        text = strip_comments(read(os.path.join(SCRIPTS, name)))
        if call.search(text):
            live_onborder.append(name)
    if live_onborder:
        fail("a live `onBorder(` call is back in %s. It is a point-on-a-box test, not a "
             "which-neighbour-is-in-another-section test; see the docstring for what it returns for the "
             "cells a caller owns" % ", ".join(live_onborder))
    else:
        print("  no live onBorder( call in Scripts/*.{cpp,h}")

    # The declaration must stay on the class, since two functions and any future caller use it.
    if "SectionBorderOffset" not in read(WORLD_H):
        fail("CraftWorld2.h no longer declares SectionBorderOffset")

    # 7. The rule as the caller runs it: what a break *exposes* against what it *queues*. -------------
    #
    # Everything above is about the rule *seeing* an edge. This is about the caller acting on it, which
    # is the half the player sees. Breaking a block makes up to six neighbouring faces visible, and a
    # neighbour that lives in another section needs that section remeshed, or its face stays culled from
    # when it was buried -- the window down the seam, one cell wide. rebuildNearestChunks turns the
    # rule's three per-axis offsets into every non-empty combination of them (the x block queues x, x+y,
    # x+z and x+y+z; the y block y and y+z; the z block z and z+x -- read off the function, not guessed),
    # so the question is whether that queued set contains every section that owns a newly exposed face.
    def queued_by_caller(local, rule):
        off = rule(local)
        live = [i for i in range(3) if off[i] != 0]
        out = set()
        for bits in range(1, 1 << len(live)):
            combo = [0, 0, 0]
            for b, i in enumerate(live):
                if bits & (1 << b):
                    combo[i] = int(off[i])
            out.add(tuple(combo))
        return out

    def exposed_neighbours(local):
        """The section offsets owning a face a break at this cell uncovers, outside this section."""
        out = set()
        for i in range(3):
            for s in (-1, 1):
                if 0 <= local[i] + s < size:
                    continue     # in this section, which the caller remeshes directly
                combo = [0, 0, 0]
                combo[i] = s
                out.add(tuple(combo))
        return out

    misses = {"fixed": 0, "old": 0}
    examples = {"fixed": [], "old": []}
    for lx in range(size):
        for ly in range(size):
            for lz in range(size):
                local = (lx, ly, lz)
                need = exposed_neighbours(local)
                for label, rule in (("fixed", new_rule), ("old", old_rule)):
                    if not need <= queued_by_caller(local, rule):
                        misses[label] += 1
                        if len(examples[label]) < 3:
                            examples[label].append(
                                (local, sorted(need - queued_by_caller(local, rule))))
    print("  breaks whose exposed face is never queued, over all %d cells of one section:"
          % (size ** 3))
    print("    fixed: %-6d old: %d" % (misses["fixed"], misses["old"]))
    if misses["fixed"]:
        fail("the rule leaves %d cell(s) where breaking a block uncovers a face in another section and "
             "that section is never queued, so the face keeps its old cull -- the see-through cell. First "
             "at cell %s, section(s) %s" % (misses["fixed"], examples["fixed"][0][0],
                                            examples["fixed"][0][1]))
    if misses["old"] == 0:
        fail("the box test queues every exposed face on every cell, so this check no longer models the "
             "bug it exists for")
    for local, missing in examples["old"]:
        print("    old, cell %-12s section(s) %s never queued -- the block past that face stays a window"
              % (str(local), missing))

    # The same question asked the way the player asks it: dig a 1x1 shaft *down* a corner column of a
    # section -- the "and every block along that line down" case -- and check every step of the dig.
    shaft_fixed = shaft_old = 0
    for ly in range(size):
        local = (size - 1, ly, size - 1)
        need = exposed_neighbours(local)
        if not need <= queued_by_caller(local, new_rule):
            shaft_fixed += 1
        if not need <= queued_by_caller(local, old_rule):
            shaft_old += 1
    print("  a 1x1 downdig down the corner column (%d steps):" % size)
    print("    fixed: %d step(s) leaving an exposed face unqueued    old: %d"
          % (shaft_fixed, shaft_old))
    if shaft_fixed:
        fail("a downdig down a section's corner column leaves an exposed face unqueued on %d of its %d "
             "steps -- this is the shaft the check is named after" % (shaft_fixed, size))
    if shaft_old == 0:
        fail("the box test queues the corner column's exposed faces on every step, so the shaft half of "
             "this check no longer models the bug")

    # 6. The caller side: both helpers use the rule, and the queue drops its oldest entry when full.
    for name, sig in (("rebuildNearestChunks", "void CraftWorld::rebuildNearestChunks("),
                      ("rebuildNearestTransparentChunks",
                       "void CraftWorld::rebuildNearestTransparentChunks(")):
        b = body_of(src, sig)
        if b is None:
            fail("%s is gone from CraftWorld2.cpp" % name)
            continue
        if "SectionBorderOffset(" not in strip_comments(b):
            fail("%s still decides its neighbours with something other than SectionBorderOffset" % name)
    print("  both remesh helpers call SectionBorderOffset")

    add = body_of(src, "void CraftWorld::AddChunkToUpdate(int id)")
    if add is None:
        fail("AddChunkToUpdate(int) is gone from CraftWorld2.cpp")
    else:
        add_live = strip_comments(add)
        if "toUpdate.clear()" in add_live:
            fail("AddChunkToUpdate clears its whole queue when full. A full queue is the moment the "
                 "world is changing fastest, so that throws away every pending rebuild -- a stale "
                 "neighbour mesh by the same route as the border bug")
        elif "erase(toUpdate.begin())" not in add_live:
            fail("AddChunkToUpdate no longer bounds its queue at all, so a fast dig can grow it without "
                 "limit")
        else:
            print("  AddChunkToUpdate drops its oldest entry when full")

    return report()


def report():
    print()
    if failures:
        print("%d problem(s)." % len(failures))
        return 1
    print("PASS: the far edge of a section is tested as an edge, and nothing calls the box for it.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
