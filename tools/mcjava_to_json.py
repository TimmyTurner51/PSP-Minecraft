"""
Minecraft Java mob model -> JSON. The geometry half of the model pipeline.

Vanilla 1.7.3 keeps mob models in Java classes, and the boxes are declared with a rigid pattern:

    this.head = new ModelRenderer(this, 0, 0);          // texture offset (u, v)
    this.head.addBox(-2.0F, -6.0F, -2.0F, 4, 6, 3, 0.0F);   // corner then size
    this.head.setRotationPoint(0.0F, 15.0F, -4.0F);         // bone pivot

so the boxes can be pulled out mechanically instead of transcribed by hand. This script does that
and emits JSON, plus the derived mesh geometry, in the engine's axis convention:

    mesh X = -vanilla z  (fore-aft, + forward)
    mesh Y = -vanilla y  (up; vanilla y is down)
    mesh Z =  vanilla x  (lateral)
    mesh origin = the body's box centre

with vanilla's render-time rotation taken into account -- a bone that vanilla rotates 90 degrees
about X has its height running fore-aft and its depth running vertically, so its mesh extents are
(H, D, W) rather than (D, H, W). ModelChicken's body and ModelQuadruped's body both do this, which
is why the pig's fore-aft length is its vanilla *depth* of 8, not its height of 16.

The JSON is the deterministic contract: the mesh C++ and the draw offsets are both generated from
it, so neither is hand-typed. tools/check-chicken-vanilla.py asserts the checked-in chicken mesh
still matches what this derives.

Usage:
    python tools/mcjava_to_json.py tools/vanilla-models/ModelChicken.java
    python tools/mcjava_to_json.py tools/vanilla-models/ModelPig.java --json
"""
import argparse
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    return re.sub(r"//[^\n]*", " ", src)


def eval_operand(expr, env):
    """Evaluate a vanilla float/int expression with a tiny restricted namespace."""
    e = expr.strip()
    e = re.sub(r"(?<=[0-9])F\b", "", e)         # 0.0F -> 0.0 (JVM float suffix)
    e = e.replace("(float)", "").replace("(int)", "")
    e = e.replace("Math.PI", "PI")
    e = re.sub(r"\bPI\b", str(math.pi), e)
    try:
        return float(eval(e, {"__builtins__": {}}, dict(env)))
    except Exception as exc:
        raise ValueError("cannot evaluate %r (%s)" % (expr, exc))


def parse_constructor(src):
    """Pull the model's bones out of its constructor."""
    src = strip_comments(src)

    ctor = re.search(r"public\s+(\w+)\s*\(([^)]*)\)\s*\{", src)
    if not ctor:
        raise ValueError("no constructor found")
    name = ctor.group(1)

    # take the constructor body by brace matching
    i = src.index("{", ctor.end() - 1)
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                break
    body = src[i + 1:j]

    env = {}
    bones = {}
    order = []
    current = None
    tex = (0, 0)

    for stmt in body.split(";"):
        s = " ".join(stmt.split())
        if not s:
            continue

        # locals: `byte b0 = 16`, `int par1 = 6`, `float par2 = 0.0F`
        m = re.match(r"^(?:byte|short|int|long|float|double)\s+(\w+)\s*=\s*(.+)$", s)
        if m:
            env[m.group(1)] = eval_operand(m.group(2), env)
            continue

        # `this.name = new ModelRenderer(this, u, v)`
        m = re.match(r"^this\.(\w+)\s*=\s*new\s+ModelRenderer\s*\(\s*this\s*,\s*(.+?),\s*(.+?)\)$", s)
        if m:
            current = m.group(1)
            tex = (int(eval_operand(m.group(2), env)), int(eval_operand(m.group(3), env)))
            if current not in bones:
                bones[current] = {"name": current, "pivot": None, "cubes": []}
                order.append(current)
            continue

        # `this.name.setTextureOffset(u, v).addBox(...)` -- a cube on its own texture offset
        m = re.match(r"^this\.(\w+)\.setTextureOffset\s*\((.+?),\s*(.+?)\)\.addBox\s*\((.+)\)$", s)
        if m:
            bone = m.group(1)
            own = (int(eval_operand(m.group(2), env)), int(eval_operand(m.group(3), env)))
            add_cube(bones, order, bone, own, m.group(4), env)
            continue

        # `this.name.addBox(x, y, z, W, H, D[, expand])`
        m = re.match(r"^this\.(\w+)\.addBox\s*\((.+)\)$", s)
        if m:
            bone = m.group(1)
            if bone not in bones:
                bones[bone] = {"name": bone, "pivot": None, "cubes": []}
                order.append(bone)
            add_cube(bones, order, bone, tex if bone == current else (0, 0), m.group(2), env)
            continue

        # `this.name.setRotationPoint(x, y, z)`
        m = re.match(r"^this\.(\w+)\.setRotationPoint\s*\((.+)\)$", s)
        if m:
            bone = m.group(1)
            if bone not in bones:
                bones[bone] = {"name": bone, "pivot": None, "cubes": []}
                order.append(bone)
            parts = split_args(m.group(2))
            bones[bone]["pivot"] = tuple(eval_operand(p, env) for p in parts[:3])
            continue

    # render-time rotations live in setRotationAngles, not the constructor
    rotated = set()
    for m in re.finditer(r"this\.(\w+)\.rotateAngleX\s*=\s*\(?\s*\(float\)?\s*Math\.PI\s*/\s*2", src):
        rotated.add(m.group(1))
    for m in re.finditer(r"this\.(\w+)\.rotateAngleX\s*=\s*1\.5707964", src):
        rotated.add(m.group(1))

    for b in bones.values():
        b["rotatedX"] = b["name"] in rotated

    return name, [bones[n] for n in order]


def split_args(s):
    """Split on commas that are not inside parentheses."""
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return [a.strip() for a in out]


def add_cube(bones, order, bone, tex, args, env):
    parts = split_args(args)
    if len(parts) < 6:
        raise ValueError("addBox needs at least 6 arguments, got %r" % args)
    corner = tuple(eval_operand(p, env) for p in parts[0:3])
    size = tuple(int(eval_operand(p, env)) for p in parts[3:6])
    expand = eval_operand(parts[6], env) if len(parts) > 6 else 0.0
    if bone not in bones:
        bones[bone] = {"name": bone, "pivot": None, "cubes": []}
        order.append(bone)
    bones[bone]["cubes"].append({
        "uv": [int(tex[0]), int(tex[1])],
        "origin": [round(v, 4) for v in corner],
        "size": list(size),
        "expand": round(expand, 4),
    })


def to_mesh(v):
    return (-v[2], -v[1], v[0])


def derive(bones):
    """Mesh box centre and extents for every cube, from the vanilla pivots."""
    by_name = {b["name"]: b for b in bones}
    body = by_name.get("body")
    if not body or not body["cubes"]:
        raise ValueError("no body bone to use as the mesh origin")

    c = [body["pivot"][i] + body["cubes"][0]["origin"][i] + body["cubes"][0]["size"][i] / 2.0
         for i in range(3)]
    if body["rotatedX"]:
        p = body["pivot"]
        d = [c[i] - p[i] for i in range(3)]
        c = [p[i] + v for i, v in enumerate([d[0], -d[2], d[1]])]
    origin = c

    out = []
    for b in bones:
        for cube in b["cubes"]:
            ctr = [b["pivot"][i] + cube["origin"][i] + cube["size"][i] / 2.0 for i in range(3)]
            W, H, D = cube["size"]
            if b["rotatedX"]:
                d = [ctr[i] - b["pivot"][i] for i in range(3)]
                ctr = [b["pivot"][i] + v for i, v in enumerate([d[0], -d[2], d[1]])]
                ext = (H, D, W)
            else:
                ext = (D, H, W)
            mc = to_mesh([ctr[i] - origin[i] for i in range(3)])
            lo = [round(mc[i] - ext[i] / 2.0, 4) for i in range(3)]
            hi = [round(mc[i] + ext[i] / 2.0, 4) for i in range(3)]
            out.append({
                "bone": b["name"],
                "uv": cube["uv"],
                "vanillaOrigin": cube["origin"],
                "vanillaSize": cube["size"],
                "meshExtents": [lo, hi],
            })
    return origin, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("java", help="a vanilla Model*.java file")
    ap.add_argument("--json", action="store_true", help="print the JSON instead of a table")
    ap.add_argument("--out", help="write the JSON here")
    args = ap.parse_args()

    src = open(args.java, encoding="utf-8", errors="replace").read()
    name, bones = parse_constructor(src)
    origin, cubes = derive(bones)

    doc = {
        "model": name,
        "source": os.path.basename(args.java),
        "axisConvention": {
            "meshX": "-vanilla z (fore-aft, + forward)",
            "meshY": "-vanilla y (up)",
            "meshZ": "vanilla x (lateral)",
            "origin": "the body box centre",
        },
        "meshOriginVanilla": [round(v, 4) for v in origin],
        "bones": [
            {
                "name": b["name"],
                "pivot": [round(v, 4) for v in (b["pivot"] or (0, 0, 0))],
                "rotatedX": b["rotatedX"],
                "cubes": b["cubes"],
            }
            for b in bones
        ],
        "cubes": cubes,
    }

    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(doc, f, indent=2)
            f.write("\n")
        print("wrote", args.out)

    if args.json:
        print(json.dumps(doc, indent=2))
        return 0

    print("%s  (%d bones, %d cubes)" % (name, len(bones), len(cubes)))
    print("mesh origin = the body box centre, vanilla (%.1f, %.1f, %.1f)" % tuple(origin))
    print()
    print("%-12s %-14s %-26s %s" % ("bone", "uv", "vanilla (corner + size)", "mesh extents (px)"))
    print("-" * 84)
    for cube in cubes:
        o = cube["vanillaOrigin"]
        s = cube["vanillaSize"]
        lo, hi = cube["meshExtents"]
        print("%-12s %-14s (%5.1f,%5.1f,%5.1f)+(%2d,%2d,%2d)  X%+6.1f..%+6.1f Y%+6.1f..%+6.1f Z%+6.1f..%+6.1f"
              % (cube["bone"], "%d,%d" % tuple(cube["uv"]),
                 o[0], o[1], o[2], s[0], s[1], s[2],
                 lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
