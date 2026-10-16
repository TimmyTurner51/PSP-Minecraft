"""
Extract vanilla mob model geometry straight out of the obfuscated 1.7.3 client jar.

Why this exists: 1.7.3 ships its models obfuscated -- there is no `ModelChicken`, only classes named
like `bic` -- so tools/mcjava_to_json.py has nothing to read and the models kept in
tools/vanilla-models/ have to come from a third-party mirror (the chicken's is a 1.7.10 mirror, and
its leg texture offset turned out not to be 1.7.3's, which silently put the legs on transparent
texels). This reads the bytecode instead, so the geometry is the real 1.7.3 numbers.

Two facts make it mechanical:

  * every model class's constant pool contains the descriptor of ModelRenderer.addBox, `(FFFIII)`,
    so the model package can be located without any mappings -- 37 classes in the 1.7.3 client.
  * the arguments are read by walking javap's disassembly backwards from each call and tracking the
    operand-stack depth of every argument still outstanding, so a local holding a constant
    (`fconst_0; fstore_1` ... `fload_1`) and a computed argument (`fload_2; ldc 0.5f; fsub`) both
    resolve to a single argument. Slicing the last N pushed constants instead is wrong twice over:
    it drops every `ldc` float operand, and a class that hoists one shared `0.0f` expand argument
    into a local -- ModelSpider does -- shifts the window and reports `4x8x8` for an `8x8x8` box.

No decompiler is needed, only a JDK's `javap` (already present: Microsoft OpenJDK 21).

Bone names are NOT recoverable from the bytecode (fields are obfuscated), so bones are reported
positionally, in declaration order, as `bone0`, `bone1`, ... -- matching the order vanilla's
constructor declares them. The box sizes and texture offsets are exact, which is what the mesher
needs. `tools/mcjava_to_json.py` still owns the named, hand-checked sources; this is the fallback
that works for every mob.

Usage:
    python tools/jar_model_extract.py --list
    python tools/jar_model_extract.py bic
    python tools/jar_model_extract.py --all --out tools/jar-models
    python tools/jar_model_extract.py --find 6,8,6 4,6,3 3,5,3     # identify a mob by its boxes
"""
import argparse
import json
import os
import re
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# ModelRenderer.addBox's descriptor. 1.7.3 uses both the 6-arg form (which returns the renderer) and
# the 7-arg form carrying the box expansion, so both are matched. Longest first, because "(FFFIII)"
# is a prefix of "(FFFIIIF)" and would otherwise swallow it.
ADD_BOX_DESCS = ("(FFFIIIF)", "(FFFIII)", "(FFFIIIZ)")
MODEL_RENDERER_CTOR = re.compile(r"\(L\w+;II\)V")
LINE_OFFSET = re.compile(r"^\s*\d+:\s*")
JAR_IN_JAR = "assets/minecraft/textures/"

DEFAULT_JAR = os.path.join(
    os.environ.get("APPDATA", os.path.expanduser("~")),
    ".minecraft", "versions", "1.7.3", "1.7.3.jar")

# Mojang's obfuscated class names are stable for a given version, so the model package is recorded
# once it has been identified by shape rather than re-discovered every run.
MODEL_PACKAGE = ("bhx", "bhy", "bhz", "bia", "bib", "bic", "bid", "bie", "bif", "big", "bih",
                 "bii", "bij", "bik", "bil", "bin", "bio", "bip", "bir", "bis", "bit", "biu",
                 "biv", "biw", "bix", "biz", "bja", "bjb", "bjc", "bje", "bjf", "bjg", "bjh",
                 "bji", "bjj", "bjm", "bjs")


def find_javap(explicit=None):
    cand = [explicit]
    jh = os.environ.get("JAVA_HOME")
    if jh:
        cand.append(os.path.join(jh, "bin", "javap.exe" if os.name == "nt" else "javap"))
        cand.append(os.path.join(jh, "bin", "javap"))
    for path in cand:
        if path and os.path.isfile(path):
            return path
    raise SystemExit("javap not found. Pass --javap, or set JAVA_HOME to a JDK.")


def const_on_line(line):
    """The numeric constant pushed by one javap line, or None."""
    s = LINE_OFFSET.sub("", line.strip())
    m = re.match(r"^iconst_m1\b", s)
    if m:
        return -1
    m = re.match(r"^iconst_([0-5])\b", s)
    if m:
        return int(m.group(1))
    m = re.match(r"^bipush\s+(-?\d+)", s)
    if m:
        return int(m.group(1))
    m = re.match(r"^sipush\s+(-?\d+)", s)
    if m:
        return int(m.group(1))
    m = re.match(r"^fconst_([0-2])\b", s)
    if m:
        return float(m.group(1))
    m = re.match(r"^ldc2?(_w)?\s+#\d+\s*//\s*(?:float|int|long|double)\s+(-?[\d.eE+-]+)", s)
    if m:
        v = float(m.group(2))
        return int(v) if v.is_integer() and "float" not in s else v
    return None


def disassemble(javap, jar, cls):
    r = subprocess.run([javap, "-c", "-p", "-classpath", jar, cls],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("javap failed on %s:\n%s" % (cls, r.stderr))
    return r.stdout.splitlines()


# A javap instruction line is "   34: bipush        8"; the comment after the operand is where a
# call site names its descriptor.
INSN = re.compile(r"^\s*(\d+):\s+([a-z_0-9]+)\s*(.*)$")
LOAD_STORE = re.compile(r"^([iflda])(load|store)(?:_(\d+))?$")

# (pushes, pops) for the instructions model constructors actually use. An opcode that is missing
# counts as having no stack effect, which is safe: the walk stops at the first line that is not an
# instruction, so an unknown opcode costs an argument's value, never the argument count.
STACK_EFFECT = {
    "i2f": (1, 1), "f2i": (1, 1), "i2d": (1, 1), "d2f": (1, 1), "f2d": (1, 1),
    "ineg": (1, 1), "fneg": (1, 1), "arraylength": (1, 1),
    "fadd": (1, 2), "fsub": (1, 2), "fmul": (1, 2), "fdiv": (1, 2), "frem": (1, 2),
    "iadd": (1, 2), "isub": (1, 2), "imul": (1, 2), "idiv": (1, 2), "irem": (1, 2),
    "dadd": (2, 4), "dsub": (2, 4), "dmul": (2, 4), "ddiv": (2, 4),
    "fcmpg": (1, 2), "fcmpl": (1, 2),
    "iaload": (1, 2), "faload": (1, 2), "aaload": (1, 2),
    "iastore": (0, 3), "fastore": (0, 3), "aastore": (0, 3),
    "dup": (2, 1), "dup_x1": (3, 2), "dup2": (4, 2),
    "pop": (0, 1), "pop2": (0, 2),
    "new": (1, 0), "newarray": (1, 1), "anewarray": (1, 1),
    "getfield": (1, 1), "putfield": (0, 2), "getstatic": (1, 0), "putstatic": (0, 1),
    "checkcast": (0, 0), "goto": (0, 0), "iinc": (0, 0),
    "return": (0, 0), "ireturn": (0, 1), "freturn": (0, 1),
    "ldc2_w": (2, 0),
}


def param_count(desc):
    """Operand-stack slots a JVM method descriptor's parameter list consumes."""
    n = i = 0
    while i < len(desc):
        c = desc[i]
        if c == "L":
            end = desc.find(";", i)
            i = len(desc) if end < 0 else end + 1
            n += 1
        elif c == "[":
            i += 1
        else:
            i += 1
            if c in "FIJDZBCS":
                n += 1
    return n


def stack_effect(op, line, rest):
    """(pushes, pops) for one instruction."""
    if op in STACK_EFFECT:
        return STACK_EFFECT[op]
    if const_on_line(line) is not None:
        return (1, 0)
    ls = LOAD_STORE.match(op)
    if ls:
        return (1, 0) if ls.group(2) == "load" else (0, 1)
    if op in ("ldc", "ldc_w"):
        return (1, 0)
    if op.startswith("if"):
        return (0, 2) if op.startswith("if_icmp") else (0, 1)
    if op.startswith("invoke"):
        d = re.search(r"\(([^)]*)\)(\S*)", rest)
        if d:
            return (0 if d.group(2) == "V" else 1,
                    param_count(d.group(1)) + (0 if op == "invokestatic" else 1))
    return (0, 0)


def call_args(lines, call_idx, nargs):
    """The nargs arguments of the call at lines[call_idx], read backwards.

    Reading forwards would mean writing a JVM interpreter -- these constructors contain loops,
    arrays and calls. Reading backwards does not, because an addBox's arguments are always
    evaluated immediately before the call. Each outstanding argument carries the operand-stack
    depth it still sits at, so a computed argument such as `fload_2; ldc 0.5f; fsub` resolves to
    one value-less argument rather than three arguments, and the walk stops as soon as it reaches
    a line that is not an instruction.
    """
    want = [(depth, nargs - depth) for depth in range(1, nargs + 1)]
    values = [None] * nargs
    i = call_idx - 1
    while want and i >= 0:
        m = INSN.match(lines[i])
        if not m:
            if lines[i].strip():
                break               # a declaration, not an instruction: this is the method start
            i -= 1
            continue
        pushes, pops = stack_effect(m.group(2), lines[i], m.group(3))
        remaining = []
        for depth, arg in want:
            if depth <= pushes:
                if pushes == 1:
                    values[arg] = const_on_line(lines[i])
            else:
                remaining.append((depth - pushes + pops, arg))
        want = remaining
        i -= 1
    return values


def _num(v):
    """A rounded float for the JSON, or None when the argument was not a literal."""
    return round(float(v), 4) if isinstance(v, (int, float)) else None


def _int(v):
    """The int value of a literal, or None when the argument was not one."""
    return int(v) if isinstance(v, (int, float)) else None


def extract_class(javap, jar, cls):
    """Every addBox in a class, as (texture offset, corner, size, expand).

    A size is not always a literal -- the base classes (ModelQuadruped and friends) take it from a
    constructor parameter -- so it comes back as None and is written `?` rather than guessed. The
    audit skips those boxes, so an unresolvable box can never be mistaken for a verified match.
    """
    lines = disassemble(javap, jar, cls)
    boxes = []
    current_uv = [0, 0]

    for i, line in enumerate(lines):
        m = INSN.match(line)
        if not m:
            continue
        op, rest = m.group(2), m.group(3)

        # a ModelRenderer construction: (L<class>;II)V, the last two args being the texture offset
        if op == "invokespecial" and MODEL_RENDERER_CTOR.search(rest):
            uv = call_args(lines, i, 2)
            if all(isinstance(u, (int, float)) for u in uv):
                current_uv = [int(u) for u in uv]

        if not op.startswith("invoke"):
            continue
        desc = next((d for d in ADD_BOX_DESCS if d in rest), None)
        if desc is None:
            continue
        nargs = 7 if desc == "(FFFIIIF)" else 6
        args = call_args(lines, i, nargs)
        if not any(a is not None for a in args):
            continue
        boxes.append({"uv": list(current_uv),
                      "origin": [_num(v) for v in args[0:3]],
                      "size": [_int(v) for v in args[3:6]],
                      "expand": _num(args[6]) if nargs == 7 else 0.0})
    return boxes


def modelled_classes(jar):
    """Classes whose constant pool carries an addBox descriptor."""
    z = zipfile.ZipFile(jar)
    out = []
    for n in z.namelist():
        if n.endswith(".class"):
            b = z.read(n)
            if any(d.encode() in b for d in ADD_BOX_DESCS):
                out.append(n[:-len(".class")])
    return sorted(out)


def describe(cls, boxes):
    shapes = " ".join("x".join("%s" % (s if isinstance(s, int) else "?") for s in b["size"])
                      for b in boxes)
    return "%-6s %2d box(es)  %s" % (cls, len(boxes), shapes)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("classes", nargs="*", help="obfuscated class names to extract")
    ap.add_argument("--jar", help="path to the 1.7.3 client jar")
    ap.add_argument("--javap", help="path to javap")
    ap.add_argument("--list", action="store_true", help="list every model class in the jar")
    ap.add_argument("--all", action="store_true", help="extract every model class")
    ap.add_argument("--out", help="write one <class>.json per model here")
    ap.add_argument("--find", nargs="+", metavar="WxHxD",
                    help="find the class whose box sizes contain all of these, e.g. 6x8x6 4x6x3")
    args = ap.parse_args()

    jar = args.jar or (os.environ.get("MC173_JAR") or DEFAULT_JAR)
    if not os.path.isfile(jar):
        raise SystemExit("no 1.7.3 client jar at %s (pass --jar)" % jar)
    javap = find_javap(args.javap)

    if args.list:
        z = zipfile.ZipFile(jar)
        classes = modelled_classes(jar)
        print("%d model classes in %s" % (len(classes), os.path.basename(jar)))
        for c in classes:
            print("  " + describe(c, extract_class(javap, jar, c)))
        return 0

    if args.find:
        want = [tuple(int(x) for x in s.lower().split("x")) for s in args.find]
        print("looking for a class containing all of: %s"
              % ", ".join("%dx%dx%d" % w for w in want))
        found = 0
        for c in modelled_classes(jar):
            sizes = [tuple(b["size"]) for b in extract_class(javap, jar, c)]
            if all(w in sizes for w in want):
                found += 1
                print("  MATCH  %s" % describe(c, extract_class(javap, jar, c)))
        if not found:
            print("  no class matches")
        return 0

    targets = modelled_classes(jar) if args.all else args.classes
    if not targets:
        print("nothing to do: name a class, or pass --all / --list / --find")
        return 1

    if args.out:
        os.makedirs(args.out, exist_ok=True)

    for c in targets:
        boxes = extract_class(javap, jar, c)
        uv = sorted({tuple(b["uv"]) for b in boxes})
        print("%-44s texture offset(s): %s"
              % (describe(c, boxes), ", ".join("%d,%d" % u for u in uv)))
        if args.out:
            doc = {"obfuscatedClass": c, "jar": jar,
                   "note": ("extracted from 1.7.3 bytecode; bone names are not recoverable, so "
                            "boxes are reported in declaration order"),
                   "boxes": boxes}
            with open(os.path.join(args.out, c + ".json"), "w", encoding="utf-8") as f:
                json.dump(doc, f, indent=2)
                f.write("\n")
    if args.out:
        print("\nwrote %d file(s) to %s" % (len(targets), args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
