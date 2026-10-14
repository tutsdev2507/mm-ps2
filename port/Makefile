PS2DEV ?= /usr/local/ps2dev
PS2SDK ?= $(PS2DEV)/ps2sdk
GSKIT  ?= $(PS2DEV)/gsKit

EE_BIN   = mm-ps2.elf
EE_OBJS  = main.o
vpath %.c src
EE_INCS += -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS  = -lgsKit -lpad -lmath3d -lgraph -ldraw -lpacket -ldma -ldebug

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
