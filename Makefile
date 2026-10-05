NAME := lynxibear
GAME_TITLE := Lynxibear
GAME_SUBTITLE := Your cozyness loop
GAME_AUTHOR := lairLynx Studios
GAME_ICON := assets/lynxibear_icon.gif
SOURCEDIRS := source

# Streamed music lives in NitroFS; tools/convert_music.py fills nitrofs/music.
NITROFSDIR := nitrofs
$(shell mkdir -p $(NITROFSDIR)/music)
LIBS := -lmm9 -lnds9
LIBDIRS := $(BLOCKSDS)/libs/maxmod

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile

# Appended after the include because it resets NDSTOOL_ARGS when NitroFS is used.
NDSTOOL_ARGS += -g LXYV 00 "Lynxibear" 0