#ifndef AUDIO_STUB_SAMPLEBANK_TABLE_H
#define AUDIO_STUB_SAMPLEBANK_TABLE_H
#include "ultra64.h"
typedef struct { u32 addr; u32 size; u32 unused; } SampleBankEntry;
typedef struct { SampleBankEntry* entries; u32 numEntries; } SampleBank;
extern SampleBank gSampleBankTable[1];
#define NUM_SAMPLE_BANKS 0
#endif
