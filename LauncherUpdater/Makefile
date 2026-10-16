# MineCraft Launcher Updater -- the second program that swaps the launcher's own EBOOT.PBP.
#
# Output: LAUNCHERUPDATER.PBP, installed as PSP/GAME/Minecraft/Updater/EBOOT.PBP. Two levels below
# PSP/GAME, which is deliberate: the XMB only scans PSP/GAME/<one folder>/, so this is invisible in the
# menu and can only be started by the launcher (or the player, from a file manager). See main.c.
#
# This makefile is a stripped-down UpdaterApp/Makefile on purpose. Where the launcher shares
# Scripts/Updater.cpp and the graphics sources so that it cannot disagree with the game about a package,
# this program shares NOTHING, because every shared thing is a way for it not to start. The build has
# to be able to say that plainly, so the file list here is one object long.

TARGET = LAUNCHERUPDATER

OBJS = main.o

# ONLY ../Scripts, for FpuControl.h. No ../source, no ../include, no ../3libs: nothing in this program
# includes an Aurora header, and leaving them off the search path is what makes that a fact rather than
# a claim.
INCDIR = ../Scripts

CFLAGS = -O2 -G0 -Wall -std=gnu99 -mpreferred-stack-boundary=4 -MMD -MP
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1

# The same firmware target as the launcher. A repair tool is the last thing that should inherit a
# memory-partition setting nobody chose; see UpdaterApp/Makefile.
PSP_FW_VERSION = 661

PSP_EBOOT_TITLE = MineCraft Launcher Updater

# Built under its own name so it cannot collide with the game's EBOOT.PBP or the launcher's
# LAUNCHER.PBP, which both build in their own folders but are both named for what they become on the
# card. See UpdaterApp/Makefile for the same reasoning.
PSP_EBOOT = LAUNCHERUPDATER.PBP
EXTRA_TARGETS = LAUNCHERUPDATER.PBP

PSPSDK=$(shell psp-config --pspsdk-path)

# -lm only. build.mak appends its own set (pspdebug, pspdisplay, pspge, pspctrl, pspnet), and the rest
# of what the launcher links -- zlib, the HTTP client, the net modules, PNG, GU -- is for things this
# program does not do. Kept minimal so a failure here cannot be a library's.
LIBS = -lm

include $(PSPSDK)/lib/build.mak

-include $(OBJS:.o=.d)

# Thin wrapper so the build can be driven from this folder without knowing the parent scripts.
launcher:
	$(MAKE) -C ../UpdaterApp

.PHONY: launcher
