#!/usr/bin/env python3
"""
Audit the piston placement rule: a placed piston must point back at whoever placed it.

Why this exists
---------------
Java BlockPistonBase.determineOrientation does two things that pull in opposite directions, and the
first is the one that is easy to get backwards:

  * the four *horizontal* cases are decided from the placer's yaw, and the direction stored is the one
    the piston pushes -- which is the direction the player is looking *from*, not the one they are
    looking along. The look vector points away from the player, so a mapping that reads it directly
    gets every wall-mounted piston exactly 180 degrees out;
  * the two *vertical* cases are decided from the placer's height and point the head straight back at
    them -- up at a placer standing over the cell, down at one standing under it. Those were always
    right, which is what makes the bug so quiet: half the placement cases agree with the other half,
    and the half that is wrong has its arm inside the wall with its plate facing away.

The failure does not stay in placement either. A piston that points away from the player points *into*
the wall it was placed on, and the cell it then treats as its head is a cell the player cannot see
while wiring it -- which is how the same wrong nibble drags the power rule into it (see
GetPistonPowerTo in CraftWorld2.cpp) and turns a redstone block in front of a piston into a loop.

Both play states carry a copy of this code, so both are read here and both have to agree.

Checked:
  1. the direction tables in CraftWorld2.cpp are the six unit directions in the order every nibble
     reader assumes -- 0 down, 1 up, then -X, +X, -Z, +Z. The horizontal cases below are checked
     *against these tables* rather than against restated numbers, so the audit cannot pass a mapping
     that is self-consistent and wrong;
  2. each horizontal face of the mapping puts the piston's push direction opposite the look axis it
     was chosen from -- look +X stores the facing whose direction is -X, and so on for all four;
  3. the two vertical branches put the head back at the placer: placer above the cell stores 1 (+Y),
     cell above the placer stores 0 (-Y);
  4. the state nibble is actually written after the block id in both states, since a piston that is
     placed without one reads as facing 0 -- down -- whatever the mapping decided;
  5. the power rule that goes with the facing is still in place: `EvaluatePiston` asks
     `GetPistonPowerTo` (never `GetRedstonePowerTo`, which counts the head cell) and
     `GetPistonPowerTo` blanks the head cell, reads, and puts both the id and the nibble back.

Run from anywhere:  python tools/check-piston-facing.py
"""
import os
import re
import sys

GAME_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
WORLD_CPP = os.path.join(GAME_ROOT, "Scripts", "CraftWorld2.cpp")
PLAY_STATES = [
    os.path.join(GAME_ROOT, "Scripts", "StatePlay.cpp"),
    os.path.join(GAME_ROOT, "Scripts", "StatePlayCreative.cpp"),
]

failures = []


def read(path):
    if not os.path.exists(path):
        raise SystemExit("missing source file: %s" % path)
    return open(path, encoding="utf-8", errors="replace").read()


def check(ok, message):
    if not ok:
        failures.append(message)
    return ok


def dir_table(src, axis):
    """kPistonDir<axis>[6] as a list of ints, or None if it is not where it is expected."""
    m = re.search(r"kPistonDir%s\[6\]\s*=\s*\{([^}]*)\}" % axis, src)
    if not m:
        return None
    return [int(v) for v in re.findall(r"-?\d+", m.group(1))]


def fmt_dir(d):
    return "(%d,%d,%d)" % d


def function_body(src, signature):
    """The text of a function, from its signature line to the closing brace in column 0."""
    start = src.find(signature)
    if start == -1:
        return None
    end = src.find("\n}", start)
    if end == -1:
        return None
    return src[start:end]


def live_lines(body):
    """The body with full-line comments dropped, so a commented-out call cannot stand in for a live
    one. Every string check below runs against this rather than the raw text."""
    return "\n".join(l for l in body.splitlines() if not l.lstrip().startswith("//"))


def main():
    world = read(WORLD_CPP)

    xs = dir_table(world, "X")
    ys = dir_table(world, "Y")
    zs = dir_table(world, "Z")
    if xs is None or ys is None or zs is None:
        raise SystemExit("could not read kPistonDirX/Y/Z from CraftWorld2.cpp")
    if not all(len(t) == 6 for t in (xs, ys, zs)):
        raise SystemExit("kPistonDirX/Y/Z must each have six entries")

    dirs = [(xs[i], ys[i], zs[i]) for i in range(6)]

    # 1. The tables themselves. Order is load-bearing: the piston nibble shares its order with the
    #    button mounting order, so a reorder here silently re-points every piston in every save.
    expected = [(0, -1, 0), (0, 1, 0), (-1, 0, 0), (1, 0, 0), (0, 0, -1), (0, 0, 1)]
    check(dirs == expected,
          "kPistonDir tables are not the documented order (0 down, 1 up, -X, +X, -Z, +Z): got %s"
          % ", ".join(fmt_dir(d) for d in dirs))

    # The mapping is read as "the look axis and sign it was chosen from" -> "the facing stored".
    horizontal = re.compile(r"pistonFacing = \(look\.([xz]) > 0\.0f\) \? (\d) : (\d);")
    lookup = {"x": (1, 0, 0), "z": (0, 0, 1)}
    report = []

    for path in PLAY_STATES:
        src = read(path)
        name = os.path.basename(path)

        # 2. Horizontal: the stored facing must push back along the look axis.
        found = horizontal.findall(src)
        line = []
        for axis, positive_facing, negative_facing in found:
            ax = lookup[axis]
            for sign, facing in ((1, int(positive_facing)), (-1, int(negative_facing))):
                want = tuple(-sign * c for c in ax)
                line.append("look %s%s -> %d %s"
                            % ("+" if sign > 0 else "-", axis.upper(), facing, fmt_dir(dirs[facing])))
                check(dirs[facing] == want,
                      "%s: looking %s%s stores facing %d, whose push direction is %s, but it has to "
                      "be %s -- the head comes back towards the placer"
                      % (name, "+" if sign > 0 else "-", axis.upper(), facing,
                         fmt_dir(dirs[facing]), fmt_dir(want)))
        if len(found) != 2:
            failures.append("%s: expected exactly two horizontal placement ternaries, found %d"
                            % (name, len(found)))

        # 3. Vertical: over the cell stores up, under it stores down.
        above = re.search(r"playerPosition\.y - testPos2\.y > 2\.0f\)\s*\n\s*pistonFacing = (\d);", src)
        below = re.search(r"testPos2\.y - playerPosition\.y > 0\.0f\)\s*\n\s*pistonFacing = (\d);", src)
        if above is None:
            failures.append("%s: no placer-above branch found" % name)
        else:
            line.append("placer over the cell -> %d %s"
                        % (int(above.group(1)), fmt_dir(dirs[int(above.group(1))])))
            check(dirs[int(above.group(1))] == (0, 1, 0),
                  "%s: a placer standing over the cell stores facing %s, but the head has to come up "
                  "at them (%s)" % (name, fmt_dir(dirs[int(above.group(1))]), fmt_dir((0, 1, 0))))
        if below is None:
            failures.append("%s: no cell-above-placer branch found" % name)
        else:
            line.append("cell over the placer -> %d %s"
                        % (int(below.group(1)), fmt_dir(dirs[int(below.group(1))])))
            check(dirs[int(below.group(1))] == (0, -1, 0),
                  "%s: a cell above the placer stores facing %s, but the head has to come down at "
                  "them (%s)" % (name, fmt_dir(dirs[int(below.group(1))]), fmt_dir((0, -1, 0))))

        # 4. The nibble is written by hand on this path (the cell was air, so nothing else sets it).
        #    Anchored to the start of a line so a commented-out call cannot stand in for a live one.
        writes = re.findall(r"^[ \t]*mWorld->SetPistonState\(\(int\)testPos2\.x", src, re.M)
        check(len(writes) == 1,
              "%s: the placed piston is not given its facing -- SetPistonState(testPos2...) is not "
              "a live call exactly once (found %d)" % (name, len(writes)))

        report.append((name, line))

    # 5. The power rule that goes with the facing: a piston must not hear the cell it pushes into.
    #    Checked here rather than left to a comment because the two are the same bug seen twice -- the
    #    wrong nibble picks the wrong head cell, and a head cell left in the sum is the loop in
    #    GetPistonPowerTo's comment.
    piston_power = function_body(world, "int CraftWorld::GetPistonPowerTo")
    evaluate = function_body(world, "void CraftWorld::EvaluatePiston")
    if piston_power is None:
        failures.append("CraftWorld2.cpp: GetPistonPowerTo is gone; a piston that listens to the "
                        "cell it pushes into fires itself")
    else:
        live = live_lines(piston_power)
        blanked = live.find("GetBlock(hx,hy,hz) = 0;")
        restored = live.find("GetBlock(hx,hy,hz) = savedBlock;")
        check(blanked != -1,
              "GetPistonPowerTo does not blank the head cell -- the source in front of the piston "
              "powers it again")
        check(restored != -1,
              "GetPistonPowerTo does not put the head cell back")
        if blanked != -1 and restored != -1:
            check(blanked < restored,
                  "GetPistonPowerTo restores the head cell before reading the power")
        check("GetBlockSettingsNoCheck(hx,hy,hz) = savedState;" in live,
              "GetPistonPowerTo leaves the head cell's state nibble behind, so a light read sees "
              "state where the cell is now air")
    if evaluate is None:
        failures.append("CraftWorld2.cpp: EvaluatePiston is gone")
    else:
        live = live_lines(evaluate)
        check("GetPistonPowerTo(x,y,z)" in live,
              "EvaluatePiston no longer asks GetPistonPowerTo")
        check("GetRedstonePowerTo(" not in live,
              "EvaluatePiston asks GetRedstonePowerTo directly, which counts the head cell")

    print("piston placement facing")
    print("-" * 78)
    print("direction tables (CraftWorld2.cpp):")
    for i, d in enumerate(dirs):
        print("  %d = %s" % (i, fmt_dir(d)))
    print()
    for name, line in report:
        print(name)
        for entry in line:
            print("  %s" % entry)
    print()

    if failures:
        print("%d problem(s):" % len(failures))
        for f in failures:
            print("  - %s" % f)
        return 1

    print("a placed piston points back at the placer in both play states, agrees with the direction")
    print("tables, and only hears the five cells that are not its head cell")
    return 0


if __name__ == "__main__":
    sys.exit(main())
