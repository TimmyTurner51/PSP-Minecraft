# sceKernelLoadExec handoff probe: a launcher and its harmless payload.
#
# Standalone on purpose, like tools/minimal-boot-test -- this is not part of the game's build and must
# not inherit the game's link flags, or it would stop being an instrument. The only things it copies
# from the game are the module settings that are known to work, and it takes those from the *baseline*
# variant rather than from the game (see probe.c).
#
# Usage:
#   make both            build EBOOT-probe.PBP and EBOOT-payload.PBP
#   make probe           build just the launcher
#   make payload         build just the payload
#   make distclean       remove everything this produces

TARGET = MCPSPLoadExecProbe
OBJS = probe.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =

# 661 rather than the game's 3.71. Above 390 the SDK stops asking for the expanded memory partition by
# default, which is the modern and safest choice, and it keeps the instrument from sharing a suspect
# setting with the build it is judging. tools/minimal-boot-test does the same.
PSP_FW_VERSION = 661

# The shape the game actually ships in. minimal-boot-test's variant D exists because a static EXEC and a
# relocatable PRX are different things to the loader, and the game is a PRX -- so an instrument that
# answered the question as a static executable would have answered a different question.
BUILD_PRX = 1

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = MCPSP LoadExec Probe

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# sctrlKernelLoadExecVSHWithApitype lives in libpspsystemctrl_user.a, stub module "SystemCtrlForUser".
# Unlike pspuser, this one is NOT in the psp-gcc driver's spec, so it has to be named. Added after the
# include so it lands in front of the spec's trailing libraries, which is where build.mak puts LIBS.
LIBS += -lpspsystemctrl_user

# No link-flag additions are needed for sceKernelLoadExec, and this is worth recording because the
# obvious fix is wrong.
#
# sceKernelLoadExec is exported by libpspuser.a (loadable module "LoadExecForUser", as an import stub),
# and build.mak's default LIBS for a user-mode build does *not* list pspuser -- it lists pspdebug,
# pspdisplay, pspge, pspctrl, pspnet and friends. That looks like it should leave the symbol undefined,
# but it does not: the psp-gcc driver's own spec already ends with
#
#     ... -lpspsdk -lpspmodinfo -lpspuser
#
# so pspuser is linked in last, after everything build.mak supplies. Adding an explicit -lpspuser
# double-links the archive, and psp-fixup-imports then reports "stubs out of order ... your binary may
# or not work" -- verified both ways with psp-gcc --trace-symbol, which shows the single definition
# coming from libpspuser.a(LoadExecForUser_0001.o) either way. tools/minimal-boot-test links with zero
# warnings for the same reason. So: nothing to add here, deliberately.


# Each build is cleaned first because the module settings are baked into the object file: a stale .o
# would silently ship the previous program's settings, which is exactly the mistake minimal-boot-test's
# Makefile calls out.
probe:
	$(MAKE) clean
	rm -f probe.o payload.o
	$(MAKE) OBJS=probe.o TARGET=MCPSPLoadExecProbe PSP_EBOOT_TITLE="MCPSP LoadExec Probe"
	cp EBOOT.PBP EBOOT-probe.PBP

payload:
	$(MAKE) clean
	rm -f probe.o payload.o
	$(MAKE) OBJS=payload.o TARGET=MCPSPLoadExecTarget PSP_EBOOT_TITLE="MCPSP LoadExec Target"
	cp EBOOT.PBP EBOOT-payload.PBP

both: probe payload

distclean: clean
	rm -f EBOOT-probe.PBP EBOOT-payload.PBP probe.o payload.o

.PHONY: probe payload both distclean
