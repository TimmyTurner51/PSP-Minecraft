# MineCraft Launcher -- the EBOOT the XMB runs.
#
# Output: LAUNCHER.PBP, installed as PSP/GAME/Minecraft/EBOOT.PBP. It owns the folder; the game lives
# under Installs/<version>/EBOOT.PBP, one self-contained install per version. See Launcher.h for the
# layout and why it is shaped that way.
#
# Standalone build, but NOT an independent implementation: it compiles Scripts/Updater.cpp, the same
# file the game compiles. That is the whole point -- the two programs have to agree on every byte of the
# protocol, and the only way to guarantee that is one implementation in both binaries. So a change to
# Updater.cpp changes both, and neither can drift.
#
# The shared sources live outside this folder, so they are compiled by explicit rules into LOCAL object
# names (UpdaterShared.o, LoggerShared.o, pgeZipShared.o). The obvious alternative -- listing
# ../Scripts/Updater.o and letting the built-in rule write it next to its source -- would put this
# build's objects into the game's tree under the same names the game's own build uses, with different
# flags. Make would then consider them current for whichever build ran second and link a mixture. Local
# names keep the two builds' objects entirely separate.

TARGET = LAUNCHER

# FpuInit.o is LAST on purpose. The runtime walks .ctors BACKWARDS, so the object listed last is the one
# whose constructor runs first -- which is what the FPU clear has to be. MakefileGame places its own
# marker objects at the ends of its list for exactly this reason and documents the measurement that
# established it. See UpdaterApp/FpuInit.c.
OBJS = main.o Screen.o Launcher.o LauncherFiles.o LauncherUpdate.o \
       UpdaterShared.o LoggerShared.o pgeZipShared.o \
       TextureManagerShared.o SpriteShared.o intraFontShared.o libcccShared.o vramShared.o \
       LaunchFileShared.o InitTraceShared.o HttpsClient.o FpuInit.o


# ../Scripts so "Updater.h", "Version.h" and "FpuControl.h" resolve the same way they do from the game,
# and ../source ../include for the Aurora headers the shared files include by qualified path. Prefixed
# with ../ because this makefile lives one directory below those, unlike the game's.
INCDIR = ../include ../source ../3libs ../Scripts

# The game's warning set, minus what only applies to its own sources.
#
# -DPGE_ZIP_LOG=UpdateZipLog routes pgeZip's own error messages into this program's log instead of to a
# stdout that does not exist here. pgeZip.c names the failing file and the failing step on every one of
# its error paths, and with nowhere to go those messages were simply lost -- which is what makes an
# extraction failure indistinguishable from a crash. UpdateZipLog is defined in Updater.cpp.
CFLAGS = -O2 -G0 -Wall -std=gnu99 -mpreferred-stack-boundary=4 -MMD -MP -DPGE_ZIP_LOG=UpdateZipLog
CXXFLAGS = -O2 -G0 -Wall -std=gnu++11 -mpreferred-stack-boundary=4 -fno-exceptions -fno-rtti -fpermissive -MMD -MP -DMC_UPDATER_TRACE
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1

# 661 rather than the game's 371. Above 390 the SDK stops asking for the expanded memory partition by
# default, which build.mak calls the modern and safest choice -- and a repair tool should not inherit a
# setting that has already been the subject of a memory investigation in this project.
PSP_FW_VERSION = 661

PSP_EBOOT_TITLE = MineCraft Launcher

# The output is LAUNCHER.PBP. build.mak derives its PBP rule from $(PSP_EBOOT) and defaults it to
# EBOOT.PBP, so overriding it before the include produces this name directly, with no rename step to
# forget. EXTRA_TARGETS is what makes plain `make` build it, since `all` is defined as
# "$(EXTRA_TARGETS) $(FINAL_TARGET)" and FINAL_TARGET here is just LAUNCHER.prx.
#
# The deploy step installs this as PSP/GAME/Minecraft/EBOOT.PBP -- the name the XMB looks for. It is
# built under a different name so that it does not collide with the game's own EBOOT.PBP in the source
# tree, where both this build and the game's build write their output.
PSP_EBOOT = LAUNCHER.PBP
EXTRA_TARGETS = LAUNCHER.PBP
PSP_EBOOT_ICON = ../ICON0.png
PSP_EBOOT_PIC1 = ../PIC1.png

PSPSDK=$(shell psp-config --pspsdk-path)

# -lz          crc32 and zlib, for the package checks and for pgeZip
# -lpsphttp    the HTTP client for LAN http://
# -lpspssl     still linked so the unused Sony HTTPS init path stays resolvable
# -lmbedtls -lmbedx509 -lmbedcrypto
#              TLS 1.2 for GitHub Releases. sceSsl cannot handshake those hosts
#              (SCE_HTTP_ERROR_SSL); mbedtls is already in pspdev and ships inside
#              the launcher, so the player does not install anything extra.
# -lpsputility the net module loading
# -lstdc++ -lm C++ runtime and libm, which PRX builds do not get automatically
# -lpng        the PNG decoder, for the dirt and bar art this program now draws
# -lpspgu -lpspgum  GU itself and its matrix helpers, which Sprite::Draw uses
# -lpspvfpu -lpspmath  what intraFont's glyph rasteriser needs
#
# The graphics libraries were added with Screen.cpp, which draws the game's own look instead of the SDK
# text console. They are all libraries the game already links for the same purposes.
#
# Defined BEFORE the include, because build.mak works by APPENDING its own libraries to whatever LIBS
# already holds (its USE_KERNEL_LIBS block adds -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspnet
# -lpspnet_apctl). Assigning after the include instead -- with `=` rather than `+=` -- silently
# discards all of them, and the failure is a wall of undefined references to pspDebugScreenPrintf and
# sceCtrlReadBufferPositive rather than anything that mentions make.
LIBS = -lz -lmbedtls -lmbedx509 -lmbedcrypto -lpsphttp -lpspssl -lpsputility -lstdc++ -lm -lpng -lpspgu -lpspgum -lpspvfpu -lpspmath -lpsppower

include $(PSPSDK)/lib/build.mak

# -lpspsystemctrl_user is the important one: sctrlKernelLoadExecVSHWithApitype, the CFW's own launcher,
# which is how this program starts the game again afterwards. Stub module SystemCtrlForUser, a
# user-mode stub. Appended AFTER the include so it links last, alongside the driver spec's trailing
# entries. A lib named above the include would come out ahead of build.mak's, which makes
# psp-fixup-imports warn "stubs out of order ... your binary may or not work".
LIBS += -lpspsystemctrl_user

# The shared sources, compiled locally (see the note at the top).
UpdaterShared.o: ../Scripts/Updater.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

HttpsClient.o: ../Scripts/HttpsClient.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

LoggerShared.o: ../source/Aurora/Utils/Logger.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

pgeZipShared.o: ../source/Aurora/Utils/pgeZip.c
	$(CC) $(CFLAGS) -c $< -o $@

# The four files Screen.cpp draws with, compiled locally for the same reason as the three above: the
# game's build uses different flags, and a shared object file under two sets of flags is how a mixture
# gets linked. These are the game's own sources, not copies -- TextureManager/Sprite/intraFont/vram are
# the exact files the game renders with, which is what makes the look identical rather than similar.
#
# vram.c is C, not C++, and is what TextureManager uses to place a texture. It is compiled with $(CC)
# for that reason, as pgeZip.c is -- and intraFont.c is too, which is why there is no intraFont.cpp to
# find: this project's intraFont is the original C library.
TextureManagerShared.o: ../source/Aurora/Graphics/TextureManager.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

SpriteShared.o: ../source/Aurora/Graphics/Sprite.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

intraFontShared.o: ../source/Aurora/Graphics/intraFont/intraFont.c
	$(CC) $(CFLAGS) -c $< -o $@

libcccShared.o: ../source/Aurora/Graphics/intraFont/libccc.c
	$(CC) $(CFLAGS) -c $< -o $@

vramShared.o: ../source/Aurora/Graphics/vram.c
	$(CC) $(CFLAGS) -c $< -o $@

# The init tracer (Scripts/InitTrace.cpp), compiled into this program for one reason: it writes
# trace.txt through sceIoOpen/sceIoWrite and never through Logger, so it survives a fault that takes
# the logger with it, and it is the instrument the PNG loader is traced with (see the block at the top
# of ../source/Aurora/Graphics/TextureManager.cpp). Its first call truncates trace.txt, so the file
# describes exactly this program's run. Temporary, like the trace itself.
InitTraceShared.o: ../Scripts/InitTrace.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# The launcher's half of the launch handoff (Scripts/LaunchFile.c), compiled locally for the same reason
# as the others. Note which half: this build does NOT link Scripts/LaunchDir.c, the reader, because the
# launcher is not started by loadexec and already has the right working directory -- and a reader here
# would move the launcher into an install folder at boot if a handoff had been left behind. See the
# long note in ../Scripts/LaunchDir.h.
LaunchFileShared.o: ../Scripts/LaunchFile.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

# Thin wrapper so the build can be driven from this folder without knowing the game's makefile.
game:
	$(MAKE) -C .. -f MakefileGame

.PHONY: game
