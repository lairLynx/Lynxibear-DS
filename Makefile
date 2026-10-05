NAME := lynxibear
GAME_TITLE := Lynxibear
GAME_SUBTITLE := Your cozyness loop
GAME_AUTHOR := lairLynx Studios
GAME_ICON := assets/lynxibear_icon.gif
NDSTOOL_ARGS += -g LXYV 00 "Lynxibear" 0
SOURCEDIRS := source

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
