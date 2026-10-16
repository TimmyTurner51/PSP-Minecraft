# Minimal boot test and its isolating variants (see main.c for what they mean and how to read them).
#
# Standalone on purpose: this is not part of the game's build and must not inherit the game's link
# flags, or it would stop being a baseline. The only things it copies from the game are the module
# settings that variant B is specifically trying to test.

TARGET = MCBootTest
OBJS = main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

# -DVARIANT_GAME_SETTINGS / -DVARIANT_VFPU select the variants; nothing set means the baseline.
CFLAGS += $(EXTRA_DEFS)
CXXFLAGS += $(EXTRA_DEFS)

LIBDIR =
LDFLAGS =

# 6.61 rather than the game's 3.71. With PSP_FW_VERSION above 390 the SDK leaves the memory size at
# its default instead of requesting the expanded partition, which is the modern and safest choice -- and
# it keeps the baseline from sharing a suspect setting with the build it is meant to judge. Variant B
# deliberately goes back to 371 (plus PSP_LARGE_MEMORY=1) to reproduce the game's MEMSIZE=1 exactly.
PSP_FW_VERSION = 661

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = MCPSP Boot Test

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# ---------------------------------------------------------------------------
# Variants. `make` alone builds the baseline (variant A).
#
# Each builds in a clean tree because the module settings are baked into the object file, so a stale
# main.o would silently ship the previous variant's settings.
# ---------------------------------------------------------------------------

# B: the game's own module settings, and its MEMSIZE=1.
variant-b:
	$(MAKE) clean
	$(MAKE) PSP_FW_VERSION=371 PSP_LARGE_MEMORY=1 EXTRA_DEFS=-DVARIANT_GAME_SETTINGS \
		PSP_EBOOT_TITLE="MCPSP Test B"
	cp EBOOT.PBP EBOOT-variant-B.PBP
	@echo "built EBOOT-variant-B.PBP (check MEMSIZE is 1: python3 ../sfo-dump.py PARAM.SFO)"

# C: the baseline settings with only the main thread's VFPU attribute changed.
variant-c:
	$(MAKE) clean
	$(MAKE) EXTRA_DEFS=-DVARIANT_VFPU PSP_EBOOT_TITLE="MCPSP Test C"
	cp EBOOT.PBP EBOOT-variant-C.PBP
	@echo "built EBOOT-variant-C.PBP"

# A, freshly, under its own name so all three can be compared side by side.
variant-a:
	$(MAKE) clean
	$(MAKE)
	cp EBOOT.PBP EBOOT-variant-A.PBP
	@echo "built EBOOT-variant-A.PBP"

# D: the baseline source, but built as a real PRX (BUILD_PRX=1) -- the shape the game actually ships.
#
# This exists because A/B/C turned out to be weaker evidence than they looked. Without BUILD_PRX the
# SDK links a static executable (ELF type EXEC, absolute entry point, no relocations), and that is a
# different thing from the relocatable module (ELF type ffa0, base 0, kernel-relocated) that the game
# and every other EBOOT in this project produce. So A/B/C showed the console loads a *static* module
# from this toolchain; whether it loads a *PRX* from this toolchain was never tested, and that is the
# one variable left when the game fails and a build of the same vintage succeeds.
variant-d:
	$(MAKE) clean
	$(MAKE) BUILD_PRX=1 PSP_EBOOT_TITLE="MCPSP Test D"
	cp EBOOT.PBP EBOOT-variant-D.PBP
	@echo "built EBOOT-variant-D.PBP (check ELF type is ffa0: psp-readelf -h)"

variants: variant-a variant-b variant-c variant-d

.PHONY: variant-a variant-b variant-c variant-d variants
