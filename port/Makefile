PS2DEV ?= /usr/local/ps2dev
PS2SDK ?= $(PS2DEV)/ps2sdk
GSKIT  ?= $(PS2SDK)/ports

EE_BIN   = mm-ps2.elf
EE_OBJS  = main.o gfx.o object_ah.o
vpath %.c src src/gfx assets/object_ah
EE_INCS += -I$(GSKIT)/include -Isrc -Iassets/object_ah
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS  = -lgskit -lgskit_toolkit -ldmakit -lpad -lmath3d -lgraph -ldraw -lpacket -ldma -ldebug -lm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
