#ifndef MC_VERSION_H_
#define MC_VERSION_H_

// What build is this. Kept in one place because two consumers now need the same answer:
//
//   - the title screen, which used to hardcode "Minecraft v3.0.0" in two separate DrawText calls
//     and had already drifted from the changelog (this file was the one that was right), and
//   - the online updater, which compares this against the version the update server offers.
//
// MC_VERSION_CODE is the comparison key, never the display string. "3.0.10" is newer than "3.0.9"
// but sorts *before* it as text, so anything that orders builds by the string is wrong; the integer
// exists so that mistake cannot be made by accident. 100 per minor and 10000 per major leaves room
// for 99 patches and 99 minors, which is more than this project will ever use.
//
// THIS NUMBER IS ALSO THE ONLY THING THAT MAKES A NEW BUILD *VISIBLE* TO THE LAUNCHER. It decides
// whether an install is current by comparing versions, and its integrity check only asks whether an
// install matches its OWN record -- so a build deployed by copying files, with this left alone, is
// self-consistent and reads as up to date forever. Bumping this is one way to make a rebuild show up;
// serving the same version with `force` is the other, and the one to prefer while that version is
// still being worked on. See the note on SelectedIntegrity in UpdaterApp/Launcher.cpp.
#define MC_VERSION_MAJOR 4
#define MC_VERSION_MINOR 0
#define MC_VERSION_PATCH 1

#define MC_VERSION_STRING "4.0.1"

// The string the title screen draws. Literal concatenation, so it stays a compile-time constant and
// costs nothing to build at runtime.
#define MC_VERSION_TITLE "Minecraft v" MC_VERSION_STRING

#define MC_VERSION_CODE (MC_VERSION_MAJOR * 10000 + MC_VERSION_MINOR * 100 + MC_VERSION_PATCH)

#endif
