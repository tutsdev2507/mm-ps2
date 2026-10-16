#include "ultra64.h"
#include "audio_stubs/samplebank_table.h"
#include "audio_stubs/soundfont_table.h"

SampleBank gSampleBankTable[1] = { { NULL, 0 } };
SoundFont gSoundFontTable[1] = { { NULL, 0 } };

/*
 * Placeholder banks referenced by the stub audio table headers in
 * build/gen/assets/audio/.  The generated table bodies take each
 * symbol's *address* as the size field (that is how the N64 build
 * encodes sizes via linker symbols), so the values are arbitrary here;
 * audio is milestone M5.
 */
u8 gMmps2StubBank_Start[16] __attribute__((used));
u8 gMmps2StubBank_Size[16] __attribute__((used));
u8 gMmps2StubFont_Start[16] __attribute__((used));
u8 gMmps2StubFont_Size[16] __attribute__((used));
