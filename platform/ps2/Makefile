PS2DEV ?= /usr/local/ps2dev
PS2SDK ?= $(PS2DEV)/ps2sdk
GSKIT  ?= $(PS2SDK)/ports

EE_BIN  = mmps2-boot.elf
EE_OBJS = boot.o mmps2_os.o gfx.o

vpath %.c ../../port/src/gfx
EE_INCS += -I$(GSKIT)/include -I../../port/src
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS = -lgskit -ldmakit -lpad -ldebug -lm

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
