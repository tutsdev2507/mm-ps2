#include "ultra64.h"

u8 D_01000000[16];
u8 D_08000000[16];
u8 D_0E000000[16];
u8 D_0F000000[16];
u8 __osExceptionPreamble[16];
u8 __osHwIntTable[16];

u8 aspMainDataEnd[16];
u8 aspMainDataStart[16];
u8 aspMainStack[16];
u8 aspMainTextStart[16];

u8 gAudioHeapInitSizes[16];
u8 gAudioTatumInit[16];
DmaEntry gDmaDataTable[2000]; /* large enough placeholder replaced by real? */
u8 gSampleBankTable_dummy[16];
u8 gSequenceFontTable[16];
u8 gSoundFontTable_dummy[16];

u8 gspF3DZEX2_NoN_PosLight_fifoDataStart[16];
u8 gspF3DZEX2_NoN_PosLight_fifoTextStart[16];
u8 gspS2DEX2_fifoDataStart[16];
u8 gspS2DEX2_fifoTextStart[16];

u8 njpgdspMainDataStart[16];
u8 njpgdspMainTextStart[16];

u8 osAppNMIBuffer[0x40];
u32 osMemSize;
u32 osResetType;
uintptr_t osRomBase;
u32 osTvType;
u32 osSetIntMask(u32 a) { (void)a; return 0; }

u8 qNaN0x10000[16];
u8 rspbootTextEnd[16];
u8 rspbootTextStart[16];
