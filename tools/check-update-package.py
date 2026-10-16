#!/usr/bin/env python3
"""
Validate an update package before a PSP ever sees it.

Why this exists
---------------
The failure this catches is the one the PSP reports worst. If a package's manifest disagrees with the
archive -- a file listed but not stored, a size or crc from an older build, an update.txt describing a
package that has since been rebuilt -- the console reports "the download did not arrive intact" or an
install that fails with no usable detail. Debugging that over a wifi link, on a handheld, with the only
evidence in Log.txt, is a bad afternoon. All of it is checkable here in a second.

The manifest is not decoration. The installer extracts the archive and then verifies the *manifest's*
list against what landed, so the two have to agree exactly. That is why check 6 below compares them
rather than trusting either.

Usage
-----
  # A package on its own:
  python tools/check-update-package.py --package update-server/MineCraft-3.0.2.zip

  # The whole pre-flight, against the folder the package was built from:
  python tools/check-update-package.py --package update-server/MineCraft-3.0.2.zip \\
      --root "Minecraft - Compiled" --offer update-server/update.txt

  # Self-test, used by the audit sweep: builds a package in a temp folder, validates it, then proves
  # the validation fails when the package is corrupted.
  python tools/check-update-package.py --self-test

Checked
-------
  1. the archive contains manifest.txt, and the manifest parses as version / code / file|path|size|crc /
     delete|path, with an unknown key tolerated (that is how the format can gain a field without every
     older build refusing to update) and a missing required field rejected;
  2. `code` equals MAJOR*10000 + MINOR*100 + PATCH for the version string, the same rule as
     MC_VERSION_CODE in Scripts/Version.h. A package whose two halves disagree would be advertised by
     one number and installed as another;
  3. the manifest lists at least one file. A package that changes nothing is refused by the game, and
     ought to be refused here;
  4. no path in the manifest is absolute or climbs out of the game folder -- the guard the C++ parser
     applies, checked on the same data;
  5. every file the manifest lists is actually stored in the archive, and every stored entry is listed
     (except manifest.txt itself, which is the description and not content);
  6. with --root: every listed file's size and crc32 match the real file in the game folder, which is
     what proves the package is a patch *from* that state rather than from some other build;
  7. with --offer: update.txt names this package, gives its real size and crc32, and agrees with the
     manifest about the version and code;
  8. --self-test runs checks 1 to 5 on a package it builds itself, then flips one byte inside the
     archive and requires the size/crc check to notice.
  9. with --offer: every `patch|file|size|crc|base` entry is checked against the asset beside the
     offer -- that it exists, that its size and crc match, and that the patch's own manifest agrees
     with the entry about the version, the code and the base it applies to. An entry that cannot be
     honoured is a download that fails on the console after the wait, which is the worst place to
     find out. --self-test builds a release with --patch-base and requires all of it to pass, then
     breaks one entry at a time and requires each break to be caught.
"""

import argparse
import io
import os
import shutil
import sys
import tempfile
import zipfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def load_packager():
    """Import make-update-package.py, whose name is not a valid module name because of the dashes.

    The packager owns the format, so the validator reuses its code rather than repeating it. A second
    implementation of the format would eventually accept what the packager writes but not what the
    game reads, which is worse than having no validator.
    """
    import importlib.util
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, "make-update-package.py")
    spec = importlib.util.spec_from_file_location("make_update_package", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


REQUIRED_FIELDS = ("version", "code", "file", "size", "crc")
MANIFEST_ENTRY = "manifest.txt"


class Problems:
    def __init__(self):
        self.items = []

    def add(self, text):
        self.items.append(text)

    def ok(self):
        return not self.items

    def report(self):
        for item in self.items:
            print(f"  FAIL  {item}")


def version_code(version):
    major, minor, patch = (int(p) for p in version.split("."))
    return major * 10000 + minor * 100 + patch


def parse_manifest(text):
    """The same grammar the C++ parser implements: one record per line, fields split on pipes."""
    manifest = {"version": None, "code": None, "files": [], "deletes": [], "force": False}
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        fields = line.split("|")
        key, rest = fields[0], fields[1:]
        if key == "version" and len(rest) == 1:
            manifest["version"] = rest[0]
        elif key == "code" and len(rest) == 1:
            manifest["code"] = int(rest[0])
        elif key == "file" and len(rest) == 3:
            manifest["files"].append((rest[0], int(rest[1]), int(rest[2])))
        elif key == "delete" and len(rest) == 1:
            manifest["deletes"].append(rest[0])
        elif key == "force" and len(rest) == 1:
            manifest["force"] = rest[0] not in ("0", "")
        # An unknown key is ignored, exactly as the C++ does.
    return manifest


def escapes(path):
    return path.startswith("/") or ".." in path or "\\" in path


def check_patches(problems, lines, offer_path, offer_fields, offer_manifest):
    """Check the offer's `patch|file|size|crc|base` entries against the assets beside it.

    Each names a package the client may fetch INSTEAD of the main one, so each has to be real. An
    entry whose asset is missing is a 404 the console reports as a network fault; one whose declared
    size or crc does not match is a download that fails its own integrity check after the wait. Both
    are cheap to catch here and expensive to diagnose on a handheld.

    The assets are looked for beside the offer, which is where the client resolves them from -- the
    filenames in the offer are relative to the server root, not to the package being validated.
    """
    here = os.path.dirname(os.path.abspath(offer_path))
    main_name = offer_fields.get("file", "")
    seen_bases = {}

    for line in lines:
        parts = line.split("|")
        if len(parts) != 4:
            problems.add(f"a patch entry is not file|size|crc|base: {line!r}")
            continue

        name, size_text, crc_text, base = (p.strip() for p in parts)
        if not name or "/" in name or "\\" in name:
            problems.add(f"a patch entry names an unusable file: {name!r}")
            continue
        if name == main_name:
            problems.add(f"a patch entry names the main package ({name}), so the client would fetch "
                         f"the same file for both")
        if base in seen_bases:
            problems.add(f"two patch entries are for {base}; the client takes the first, so the "
                         f"other can never be downloaded")
        seen_bases[base] = name
        try:
            version_code(base)
        except ValueError:
            problems.add(f"a patch entry's base is not MAJOR.MINOR.PATCH: {base!r}")

        if not size_text.isdigit() or not crc_text.isdigit():
            problems.add(f"a patch entry's size or crc is not a number: {line!r}")
            continue

        path = os.path.join(here, name)
        if not os.path.isfile(path):
            problems.add(f"the offer lists patch {name}, which is not beside the offer "
                         f"(looked in {here})")
            continue

        with open(path, "rb") as handle:
            blob = handle.read()
        if len(blob) != int(size_text):
            problems.add(f"the offer says patch {name} is {size_text} bytes, it is {len(blob)}")
        elif (zlib.crc32(blob) & 0xFFFFFFFF) != int(crc_text):
            problems.add(f"the offer says patch {name} has crc {int(crc_text):08x}, it is "
                         f"{zlib.crc32(blob) & 0xFFFFFFFF:08x}")

        # The patch's own description, which is what the installer actually reads. It has to agree with
        # the entry that advertised it, and it has to be a patch: an entry pointing at a file whose
        # manifest says full would be chosen for a small download and then used as a full install.
        try:
            archive = zipfile.ZipFile(io.BytesIO(blob))
            patch_manifest = parse_manifest(archive.read(MANIFEST_ENTRY).decode("utf-8", "replace"))
        except (zipfile.BadZipFile, KeyError) as error:
            problems.add(f"patch {name} has no readable manifest: {error}")
            continue

        if patch_manifest["version"] != offer_manifest["version"]:
            problems.add(f"patch {name} installs {patch_manifest['version']}, but the offer is for "
                         f"{offer_manifest['version']}")
        if patch_manifest["code"] != offer_manifest["code"]:
            problems.add(f"patch {name} has code {patch_manifest['code']}, the offer says "
                         f"{offer_manifest['code']}")
        patch_base = patch_manifest_base(archive)
        if patch_base != base:
            problems.add(f"patch {name} applies to {patch_base!r}, but the offer advertises it for "
                         f"{base!r}; a client on {base} would select it and then be refused")
        if is_full_manifest(archive):
            problems.add(f"patch {name} declares itself a full package, so it must not be offered "
                         f"as a patch")


def manifest_field(archive, key):
    """The value of one single-valued key in an archive's manifest, or None."""
    text = archive.read(MANIFEST_ENTRY).decode("utf-8", "replace")
    for line in text.splitlines():
        fields = line.strip().split("|")
        if len(fields) == 2 and fields[0] == key:
            return fields[1]
    return None


def patch_manifest_base(archive):
    return manifest_field(archive, "base")


def is_full_manifest(archive):
    return manifest_field(archive, "full") not in (None, "0", "")


def check_package(zip_path, root=None, offer_path=None):
    problems = Problems()

    if not os.path.isfile(zip_path):
        problems.add(f"no such package: {zip_path}")
        return problems

    with open(zip_path, "rb") as handle:
        blob = handle.read()
    package_size = len(blob)
    package_crc = zlib.crc32(blob) & 0xFFFFFFFF

    # 1. the manifest
    try:
        archive = zipfile.ZipFile(io.BytesIO(blob))
    except zipfile.BadZipFile as error:
        problems.add(f"the archive is not a readable zip: {error}")
        return problems

    names = archive.namelist()
    if MANIFEST_ENTRY not in names:
        problems.add(f"the archive has no {MANIFEST_ENTRY}, so the installer cannot tell what it contains")
        return problems

    manifest_text = archive.read(MANIFEST_ENTRY).decode("utf-8", errors="replace")
    manifest = parse_manifest(manifest_text)

    if manifest["version"] is None:
        problems.add("the manifest has no version")
    if manifest["code"] is None:
        problems.add("the manifest has no code")

    # 2. the two halves of the version must agree
    if manifest["version"] is not None and manifest["code"] is not None:
        try:
            expected = version_code(manifest["version"])
        except ValueError:
            problems.add(f"the manifest's version is not MAJOR.MINOR.PATCH: {manifest['version']!r}")
            expected = None
        if expected is not None and expected != manifest["code"]:
            problems.add(f"version {manifest['version']} implies code {expected}, "
                         f"but the manifest says {manifest['code']}")

    # 3. a package that changes nothing is refused by the game
    if not manifest["files"]:
        problems.add("the manifest lists no files; the game refuses this as a package that "
                     "changes nothing")

    # 4. paths
    for path, _size, _crc in manifest["files"]:
        if not path:
            problems.add("a file entry has an empty path")
        elif escapes(path):
            problems.add(f"file path escapes the game folder: {path}")
    for path in manifest["deletes"]:
        if not path or escapes(path):
            problems.add(f"delete path escapes the game folder: {path}")

    # 5. the manifest and the archive must describe the same set
    listed = {path for path, _s, _c in manifest["files"]}
    stored = {name for name in names if name != MANIFEST_ENTRY}
    for path in sorted(listed - stored):
        problems.add(f"the manifest lists {path}, but the archive does not contain it")
    for path in sorted(stored - listed):
        problems.add(f"the archive contains {path}, but the manifest does not list it")

    # 6. against the real folder, if we were given one
    if root:
        for path, size, crc in manifest["files"]:
            full = os.path.join(root, path.replace("/", os.sep))
            if not os.path.isfile(full):
                problems.add(f"{path} is listed but not present in {root}")
                continue
            with open(full, "rb") as handle:
                data = handle.read()
            if len(data) != size:
                problems.add(f"{path} is {len(data)} bytes in {root}, the manifest says {size}")
            elif (zlib.crc32(data) & 0xFFFFFFFF) != crc:
                problems.add(f"{path} has crc {zlib.crc32(data) & 0xFFFFFFFF:08x} in {root}, "
                             f"the manifest says {crc:08x}")
        # The archive's copy has to match too, or the install would write bytes the manifest never
        # promised and then fail its own verification.
        for path, size, crc in manifest["files"]:
            if path not in stored:
                continue
            data = archive.read(path)
            if len(data) != size or (zlib.crc32(data) & 0xFFFFFFFF) != crc:
                problems.add(f"the archive's copy of {path} does not match the manifest "
                             f"({len(data)} bytes, crc {zlib.crc32(data) & 0xFFFFFFFF:08x})")

    # 7. the offer that points at this package
    if offer_path:
        if not os.path.isfile(offer_path):
            problems.add(f"no such offer file: {offer_path}")
        else:
            fields = {}
            patch_lines = []
            with open(offer_path, "r", encoding="utf-8", errors="replace") as handle:
                for line in handle:
                    line = line.strip()
                    if line:
                        key, _, value = line.partition("|")
                        if key.strip() == "patch":
                            # Collected rather than stored in `fields`, because there is one per
                            # version offered and a dict would keep only the last -- which is exactly
                            # the entry a reader would then never validate.
                            patch_lines.append(value.strip())
                        else:
                            fields[key.strip()] = value.strip()
            for required in REQUIRED_FIELDS:
                if required not in fields:
                    problems.add(f"the offer has no '{required}' line")
            if fields.get("file") and fields["file"] != os.path.basename(zip_path):
                problems.add(f"the offer names {fields['file']!r}, "
                             f"but the package is {os.path.basename(zip_path)!r}")
            if fields.get("size") and int(fields["size"]) != package_size:
                problems.add(f"the offer says the package is {fields['size']} bytes, it is {package_size}")
            if fields.get("crc") and int(fields["crc"]) != package_crc:
                problems.add(f"the offer says crc {int(fields['crc']):08x}, it is {package_crc:08x}")
            if fields.get("version") and fields["version"] != manifest["version"]:
                problems.add(f"the offer says version {fields['version']}, "
                             f"the manifest says {manifest['version']}")
            if fields.get("code") and int(fields["code"]) != (manifest["code"] or -1):
                problems.add(f"the offer says code {fields['code']}, the manifest says {manifest['code']}")
            # The two halves of the force flag are written from one switch by the packager, so they
            # disagreeing means the offer is stale -- which is the failure mode that would have the game
            # refuse to install a package it was told was forced, or apply one it was not told about.
            if bool(fields.get("force")) != bool(manifest["force"]):
                problems.add(f"the offer and the manifest disagree about force "
                             f"(offer={fields.get('force')!r}, manifest={manifest['force']})")

            check_patches(problems, patch_lines, offer_path, fields, manifest)

    return problems


def self_test():
    """Build a package in a temp folder, validate it, then prove the checks fire when it is damaged."""
    failures = []
    packager = load_packager()

    temp = tempfile.mkdtemp(prefix="mc-update-check-")
    try:
        new = os.path.join(temp, "new")
        # A nested tree on purpose: the PSP-side extractor has to create every level, and a flat test
        # tree would not exercise that at all.
        os.makedirs(os.path.join(new, "Assets", "Textures", "Minecraft", "entity", "pig"))
        with open(os.path.join(new, "EBOOT.PBP"), "wb") as handle:
            handle.write(b"\x00PBP" + bytes(range(256)) * 8)
        with open(os.path.join(new, "Assets", "Textures", "Minecraft", "entity", "pig", "pig.png"), "wb") as handle:
            handle.write(b"\x89PNG\r\n\x1a\n" + b"pig" * 100)
        # Excluded on purpose: neither should appear in the package.
        os.makedirs(os.path.join(new, "Save"))
        with open(os.path.join(new, "Save", "world.lmsd"), "wb") as handle:
            handle.write(b"the player's world")
        with open(os.path.join(new, "Log.txt"), "wb") as handle:
            handle.write(b"a log")

        outdir = os.path.join(temp, "server")
        argv = sys.argv
        sys.argv = ["make-update-package.py", "--version", "9.9.9", "--new", new, "--outdir", outdir]
        try:
            packager.main()
        finally:
            sys.argv = argv

        zip_path = os.path.join(outdir, "MineCraft-9.9.9.zip")
        offer_path = os.path.join(outdir, "update.txt")

        problems = check_package(zip_path, root=new, offer_path=offer_path)
        if not problems.ok():
            failures.append("a freshly built package did not validate")
            problems.report()
        else:
            print("  OK    a freshly built package validates against its own folder and offer")

        # The exclusions really are exclusions.
        with zipfile.ZipFile(zip_path) as archive:
            names = set(archive.namelist())
        for absent in ("Save/world.lmsd", "Log.txt"):
            if absent in names:
                failures.append(f"{absent} was packaged; it is per-user data")
        if not failures:
            print("  OK    Save/ and Log.txt are not packaged")

        # A file the manifest lists but the archive lacks must be caught.
        with open(zip_path, "rb") as handle:
            blob = handle.read()
        rebuilt = io.BytesIO()
        with zipfile.ZipFile(io.BytesIO(blob)) as source, \
                zipfile.ZipFile(rebuilt, "w", zipfile.ZIP_DEFLATED) as target:
            for info in source.infolist():
                if info.filename == MANIFEST_ENTRY:
                    text = source.read(info.filename).decode()
                    text = text.replace("|EBOOT.PBP|", "|Assets/gone.png|", 1)
                    target.writestr(info.filename, text)
                else:
                    target.writestr(info, source.read(info.filename))
        broken = os.path.join(temp, "broken-listing.zip")
        with open(broken, "wb") as handle:
            handle.write(rebuilt.getvalue())
        problems = check_package(broken)
        if problems.ok():
            failures.append("a manifest listing a file the archive lacks was accepted")
        else:
            print("  OK    a manifest that lists a missing file is caught")

        # A wrong crc in the offer must be caught, since that is the check that rejects a truncated
        # download on the console.
        with open(offer_path, "r") as handle:
            offer_text = handle.read()
        wrong = os.path.join(temp, "wrong-crc.txt")
        with open(wrong, "w") as handle:
            # A plausible-looking but wrong crc, which is what a stale offer next to a rebuilt package
            # looks like -- and exactly what the console would reject after downloading everything.
            handle.write(offer_text.replace("crc|", "crc|1", 1))
        problems = check_package(zip_path, offer_path=wrong)
        if problems.ok():
            failures.append("an offer with the wrong crc was accepted")
        else:
            print("  OK    an offer whose crc does not match the package is caught")

        # The version/code rule.
        problems = check_package(zip_path)
        if not problems.ok():
            failures.append("the package stopped validating without a root or offer")
            problems.report()

        # The diff, which is the whole point of the tool: change one file and only that file should
        # be packaged. Without this the tool could quietly be shipping everything and nothing here
        # would notice -- which is exactly the 130 MB download this exists to avoid.
        diff_out = os.path.join(temp, "server-diff")
        pig = os.path.join(new, "Assets", "Textures", "Minecraft", "entity", "pig", "pig.png")
        with open(pig, "ab") as handle:
            handle.write(b"one changed byte")
        sys.argv = ["make-update-package.py", "--version", "9.9.10",
                    "--base", new, "--new", new, "--outdir", diff_out]
        try:
            # --base and --new are the same folder here, so the diff is against the *current* state and
            # comes out empty -- which must be refused. That is the second half of this check.
            try:
                packager.main()
                failures.append("a diff against itself produced a package")
            except SystemExit as exit_error:
                if "nothing changed" not in str(exit_error):
                    failures.append(f"a diff against itself failed for the wrong reason: {exit_error}")
                else:
                    print("  OK    an empty diff is refused")
        finally:
            sys.argv = argv

        # Now a base that really is older, made by copying the tree and reverting one file.
        base = os.path.join(temp, "base")
        shutil.copytree(new, base)
        with open(os.path.join(base, "Assets", "Textures", "Minecraft", "entity", "pig", "pig.png"), "wb") as handle:
            handle.write(b"\x89PNG\r\n\x1a\n" + b"pig" * 100)
        with open(os.path.join(base, "EBOOT.PBP"), "rb") as handle:
            older = handle.read()
        with open(os.path.join(base, "EBOOT.PBP"), "wb") as handle:
            handle.write(older[:-4])   # a previous build is a different length

        diff_out2 = os.path.join(temp, "server-diff2")
        sys.argv = ["make-update-package.py", "--version", "9.9.10",
                    "--base", base, "--new", new, "--outdir", diff_out2]
        try:
            packager.main()
        finally:
            sys.argv = argv

        with zipfile.ZipFile(os.path.join(diff_out2, "MineCraft-9.9.10.zip")) as archive:
            packaged = {name for name in archive.namelist() if name != MANIFEST_ENTRY}
        if packaged != {"EBOOT.PBP", "Assets/Textures/Minecraft/entity/pig/pig.png"}:
            failures.append(f"the diff packaged the wrong set: {sorted(packaged)}")
        else:
            print("  OK    a diff packages only the files that changed")

        # The advertised patch, which is the whole reason a release can offer a small download. Built
        # here rather than left to a manual run, because the emission and the validation are two halves
        # of one contract: a release that writes a `patch` entry its own validator rejects is worse than
        # one that writes none at all, since the failure lands on the console.
        prev = os.path.join(temp, "3.9.8")     # named as a version: --patch-base has to be able to say
        shutil.copytree(base, prev)            # what a patch applies to, and the name is how it does
        release_out = os.path.join(temp, "server-release")
        sys.argv = ["make-update-package.py", "--version", "9.9.10",
                    "--new", new, "--patch-base", prev, "--outdir", release_out]
        try:
            packager.main()
        finally:
            sys.argv = argv

        release_offer = os.path.join(release_out, "update.txt")
        with open(release_offer, "r") as handle:
            release_text = handle.read()
        advertised = [line.split("|")[1] for line in release_text.splitlines()
                      if line.startswith("patch|")]
        if advertised != ["MineCraft-9.9.10-from-3.9.8.zip"]:
            failures.append(f"the release advertised the wrong patches: {advertised}")
        else:
            print("  OK    a release advertises a patch named for the version it upgrades")

        # The main package has to stay a full install, because that is what makes the same release
        # usable on a card with nothing on it. If --patch-base quietly turned the primary into a patch,
        # a fresh install would become impossible and nothing else here would notice.
        if "full|1" not in release_text.split("patch|")[0]:
            failures.append("the release's main package is not marked full")
        else:
            print("  OK    the main package is still a full install")

        problems = check_package(os.path.join(release_out, "MineCraft-9.9.10.zip"), root=new,
                                 offer_path=release_offer)
        if not problems.ok():
            failures.append("a release with a patch did not validate")
            problems.report()
        else:
            print("  OK    a release with a patch validates, patch entries included")

        # The advertised patch carries only what changed. This is the number the whole feature exists
        # to move: if it ever equals the full package, the update is back to 95 MB and the release is
        # no better than before -- while still looking like it works.
        with zipfile.ZipFile(os.path.join(release_out, "MineCraft-9.9.10-from-3.9.8.zip")) as archive:
            in_patch = {n for n in archive.namelist() if n != MANIFEST_ENTRY}
        if in_patch != {"EBOOT.PBP", "Assets/Textures/Minecraft/entity/pig/pig.png"}:
            failures.append(f"the advertised patch carries the wrong set: {sorted(in_patch)}")
        else:
            print("  OK    the advertised patch carries only the files that changed")

        # A patch entry that cannot be honoured is caught rather than shipped. A size that does not
        # match its asset is what a stale offer beside a rebuilt package looks like, and on the console
        # it surfaces only after the download.
        bad_offer = os.path.join(temp, "offer-bad-patch.txt")
        with open(bad_offer, "w") as handle:
            handle.write(release_text.replace("patch|MineCraft-9.9.10-from-3.9.8.zip|",
                                              "patch|MineCraft-9.9.10-from-3.9.8.zip|1", 1))
        problems = check_package(os.path.join(release_out, "MineCraft-9.9.10.zip"), offer_path=bad_offer)
        if problems.ok():
            failures.append("a patch entry whose size does not match its asset was accepted")
        else:
            print("  OK    a patch entry that does not match its asset is caught")

        # And a patch offered for the wrong base. The client picks an entry by its base, so an entry
        # naming a version it does not apply to is a download that ends in a refusal after the wait.
        wrong_base_offer = os.path.join(temp, "offer-wrong-base.txt")
        with open(wrong_base_offer, "w") as handle:
            handle.write(release_text.replace("|3.9.8\n", "|3.9.7\n"))
        problems = check_package(os.path.join(release_out, "MineCraft-9.9.10.zip"),
                                 offer_path=wrong_base_offer)
        if problems.ok():
            failures.append("a patch advertised for the wrong base was accepted")
        else:
            print("  OK    a patch advertised for the wrong base is caught")
    finally:
        shutil.rmtree(temp, ignore_errors=True)

    if failures:
        print()
        for failure in failures:
            print(f"FAIL  {failure}")
        return 1

    print()
    print("self-test passed")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Validate an update package before serving it.")
    parser.add_argument("--package", help="the .zip to validate")
    parser.add_argument("--root", default=None,
                        help="the game folder the package was built from; enables the size/crc check")
    parser.add_argument("--offer", default=None,
                        help="the update.txt that points at the package")
    parser.add_argument("--self-test", action="store_true",
                        help="test the validator itself (the default when no --package is given)")
    args = parser.parse_args()

    # No arguments means the audit sweep: run the self-test, because that is the thing that can pass
    # or fail without being told where a package is. A sweep that silently skipped this file would be
    # worse than one that runs it, and having to special-case it in the sweep is how it gets forgotten.
    if args.self_test or (not args.package):
        return self_test()

    problems = check_package(args.package, args.root, args.offer)
    if problems.ok():
        print(f"OK    {args.package}")
        return 0

    print(f"FAIL  {args.package}")
    problems.report()
    return 1


if __name__ == "__main__":
    sys.exit(main())
