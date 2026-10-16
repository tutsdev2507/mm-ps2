#ifndef AUDIO_STUB_SOUNDFONT_TABLE_H
#define AUDIO_STUB_SOUNDFONT_TABLE_H
#include "ultra64.h"
typedef struct { u8* ptr; u32 size; u32 unused; } SoundFontEntry;
typedef struct { SoundFontEntry* entries; u32 numEntries; } SoundFont;
extern SoundFont gSoundFontTable[1];
#define NUM_SOUNDFONTS 0
#endif
