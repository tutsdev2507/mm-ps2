PS2DEV ?= /usr/local/ps2dev
PS2SDK ?= $(PS2DEV)/ps2sdk
GSKIT  ?= $(PS2DEV)/gsKit

MM_DIR ?= ../../third_party/mm

EE_BIN  = MMPS2.ELF
EE_OBJS = rom_io.o boot.o mmps2_os.o hw_shim.o cache_tlb.o vi_gs.o pi_dma.o rsp_shim.o segloader.o gfx.o
# audio stubs
EE_OBJS += audio_stubs.o

vpath %.c ../../port/src/gfx:../../port/src/ps2
EE_INCS += -I$(GSKIT)/include -I../../port/src -I/usr/local/ps2dev/ps2sdk/ee/include -I/usr/local/ps2dev/ps2sdk/common/include -I. -I../../include -I../../build/generated/audio -I../../include/audio_stubs -I$(MM_DIR)/include -I$(MM_DIR)/include/libc -I$(MM_DIR)
EE_CFLAGS += -DMMPS2_NATIVE_TYPES -D_EE -G0 -DFRAMEBUFFERS_START_ADDR=0x81F80000
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS  = -lgskit -ldmakit -lpad -ldebug -lm -latomic -lfileXio

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

# Compile gfx.o with port includes first (avoid decomp std headers shadowing)
gfx.o: ../../port/src/gfx/gfx.c
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) -I$(GSKIT)/include -I../../port/src -I/usr/local/ps2dev/ps2sdk/ee/include -I/usr/local/ps2dev/ps2sdk/common/include -I. -I../../include -I../../build/generated/audio -I../../include/audio_stubs -c $< -o $@
