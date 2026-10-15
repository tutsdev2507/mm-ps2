PS2DEV ?= /usr/local/ps2dev
PS2SDK ?= $(PS2DEV)/ps2sdk
GSKIT  ?= $(PS2SDK)/ports

EE_BIN  = mmps2-boot.elf
EE_OBJS = boot.o mmps2_os.o gfx.o

vpath %.c ../../port/src/gfx
EE_INCS += -I$(GSKIT)/include -I../../port/src
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS = -lgskit -ldmakit -lpad -ldebug -lm

# Optional: build the bring-up runtime against the decomp's own libultra gu math
# (M3). Enable with MM_DIR=<decomp tree> (scripts/setup.sh fetches it). EE_OBJS
# must be extended before including Makefile.eeglobal, which expands $(EE_OBJS)
# into the link rule at parse time.
ifneq ($(MM_DIR),)
EE_OBJS += gu_adapter.o gu_compat.o perspective.o lookat.o
endif

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

ifneq ($(MM_DIR),)
EE_CFLAGS += -DMMPS2_USE_DECOMP_GU
vpath %.c ../../port/src/ultra $(MM_DIR)/src/libultra/gu

# The decomp gu sources need their own include tree and -D_LANGUAGE_C; give them
# a static pattern rule so they override Makefile.eeglobal's generic %.o: %.c.
GUDEC_OBJS := gu_compat.o perspective.o lookat.o
$(GUDEC_OBJS): %.o: %.c
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) -w -D_LANGUAGE_C -DNON_MATCHING \
	    -I$(MM_DIR)/include -I$(MM_DIR)/include/libc -I$(MM_DIR) \
	    -c $< -o $@
endif
