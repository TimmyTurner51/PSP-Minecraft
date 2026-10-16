"""
Audit every crafting recipe that produces a tool: the result's amount must be that tool's
durability, and the result must not be stackable.

Why this exists
---------------
An inventory slot's amount field means two different things. For an ordinary item it is a stack
size. For a tool it is the *remaining durability* (see CraftWorld::DurabilityPointsItem, and note
that both meanings are plain small integers, so a wrong one does not look wrong).

A recipe hands its result to the crafting table's output slot through craftItemAm/craftItemAm3, so
"how many" and "how much durability" are the same field there. A tool recipe that sets it to 1 --
the obvious thing to write for "one bow" -- produces a tool with a single use and a durability bar
that is already empty. Worse, 1 is also a perfectly legal reading of "a nearly-spent tool", so the
bug is invisible in the code and only shows up as a pickaxe that breaks on its first block.

The bow's recipe read (stackable 1, amount 1) until the bow learned to wear down on firing, at which
point the mistake turned into a bow that destroyed itself on the first shot. This checks all of them
at once so the next one is caught at build time rather than on the panel.

Three things are checked:
  1. every item constructor that gives an item durability also marks it non-stackable;
  2. every `X::getID()` agrees with the item's position in InitItemVector (id == 250 + index);
  3. every recipe producing a tool sets amount == that tool's durability and stackable == 0.

Run from anywhere:  python tools/check-tool-durability.py
"""
import os
import re
import sys

GAME_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))


def read(rel):
    return open(os.path.join(GAME_ROOT, rel), encoding="utf-8", errors="replace").read()


# ---------------------------------------------------------------- items

# InitItemVector() decides the ids: the Nth push_back is item id 250 + N. That ordering is the only
# place the numbering is defined, so it is what the getID() values are checked against.
init = read("Scripts/CraftWorld_InitTypes.cpp")
init_body = init[init.index("void CraftWorld::InitItemVector()"):]
init_body = init_body[:init_body.index("\n}")]
order = re.findall(r"itemTypes\.push_back\(\s*(\w+)\s*\(\s*\)\s*\)", init_body)

items = read("Scripts/Items.cpp")

# Constructor bodies: ClassName::ClassName():BaseItem() { ... }. The defaults come from BaseItem
# (durabilityPoints -1, stackable true), so an item only counts as a tool if it assigns durability.
ctor_durability = {}
ctor_stackable = {}
for m in re.finditer(r"^(\w+)::\1\(\)\s*:\s*BaseItem\(\)\s*\{(.*?)\n\}", items, re.S | re.M):
    cls, body = m.group(1), m.group(2)
    d = re.search(r"durabilityPoints\s*=\s*(-?\d+)", body)
    s = re.search(r"stackable\s*=\s*(true|false)", body)
    ctor_durability[cls] = int(d.group(1)) if d else -1
    ctor_stackable[cls] = (s.group(1) == "true") if s else True

getid = {c: int(v) for c, v in re.findall(r"item_t\s+(\w+)::getID\(\)\s*\{\s*return\s+(\d+)\s*;\s*\}", items)}

by_name = {}
by_id = {}
for index, cls in enumerate(order):
    iid = 250 + index
    by_name[cls] = iid
    by_id[iid] = cls

problems = []

# (1) and (2): definition sanity.
for cls, iid in by_name.items():
    dura = ctor_durability.get(cls, -1)
    if cls in getid and getid[cls] != iid:
        problems.append(
            "%s: getID() says %d but InitItemVector puts it at %d (id 250 + index %d)"
            % (cls, getid[cls], iid, iid - 250))
    if dura > 0 and ctor_stackable.get(cls, True):
        problems.append(
            "%s (id %d): has durabilityPoints %d but is stackable -- a tool must not stack, or its "
            "durability is read as a stack size" % (cls, iid, dura))


# ---------------------------------------------------------------- recipes

def resolve(expr):
    """Turn a recipe's item expression into an id: '250', 'Bow::getID()', 'mWorld->...'."""
    expr = expr.strip()
    m = re.fullmatch(r"(\w+)::getID\(\)", expr)
    if m:
        return by_name.get(m.group(1), -1), m.group(1)
    m = re.fullmatch(r"-?\d+", expr)
    if m:
        iid = int(expr)
        return iid, by_id.get(iid, "id %d" % iid)
    return None, expr


def scan(path, label):
    src = read(path)
    for kind, idvar, stvar, amvar in (("3x3", "craftItemId3", "craftItemSt3", "craftItemAm3"),
                                      ("2x2", "craftItemId", "craftItemSt", "craftItemAm")):
        for m in re.finditer(r"\b%s\s*=\s*([^;]+);" % idvar, src):
            id_expr = m.group(1)
            # look inside the assignment block that follows for the other two
            window = src[m.end():m.end() + 600]
            st = re.search(r"\b%s\s*=\s*([^;]+);" % stvar, window)
            am = re.search(r"\b%s\s*=\s*([^;]+);" % amvar, window)
            iid, name = resolve(id_expr)
            if iid is None:
                continue
            if iid < 250 or iid not in by_id:
                continue
            cls = by_id[iid]
            dura = ctor_durability.get(cls, -1)
            if dura <= 0:
                continue  # not a tool -- amount really is a count
            line = src[:m.start()].count("\n") + 1
            where = "%s %s:%d (%s, id %d, durability %d)" % (label, path.split("/")[-1], line, cls, iid, dura)

            # An amount written as FreshItemAmount(...) is the durability by construction.
            am_expr = am.group(1).strip() if am else None
            if am_expr is None:
                problems.append("%s: tool recipe does not set an amount at all" % where)
            elif "FreshItemAmount" in am_expr:
                pass
            elif re.fullmatch(r"-?\d+", am_expr):
                if int(am_expr) != dura:
                    problems.append(
                        "%s: amount is %s, but a fresh tool must start at its durability -- this "
                        "makes %s that breaks after %s use(s)"
                        % (where, am_expr, cls, am_expr))
            else:
                problems.append("%s: amount is an expression the audit cannot read (%s)" % (where, am_expr))

            st_expr = st.group(1).strip() if st else None
            if st_expr is None:
                problems.append("%s: tool recipe does not set stackable at all" % where)
            elif st_expr not in ("0", "false"):
                problems.append(
                    "%s: stackable is %s, but a tool must not stack -- a stackable tool has its "
                    "durability read as a stack size" % (where, st_expr))


print("item table: %d entries from InitItemVector(), %d with durability (tools)"
      % (len(order), sum(1 for c in order if ctor_durability.get(c, -1) > 0)))
print()
print("%-28s %-6s %-11s %s" % ("tool", "id", "durability", "stackable"))
print("-" * 62)
for cls in order:
    dura = ctor_durability.get(cls, -1)
    if dura > 0:
        print("%-28s %-6d %-11d %s"
              % (cls, by_name[cls], dura, "yes (BAD)" if ctor_stackable.get(cls, True) else "no"))
print()

for state in ("StatePlay.cpp", "StatePlayCreative.cpp"):
    scan("Scripts/%s" % state, "recipe")

if problems:
    print("PROBLEMS:")
    for p in problems:
        print("  %s" % p)
    print()
    print("%d problem(s) found" % len(problems))
    sys.exit(1)

print("every tool recipe starts its result at full durability and non-stackable")
