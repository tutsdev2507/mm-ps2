#ifndef STUB_DMAKIT_H
#define STUB_DMAKIT_H

#include <tamtypes.h>

#define D_CTRL_RELE_OFF 0
#define D_CTRL_MFD_OFF 0
#define D_CTRL_STS_UNSPEC 0
#define D_CTRL_STD_OFF 0
#define D_CTRL_RCYC_8 8

#define DMA_CHANNEL_GIF 1

void dmaKit_init(int a, int b, int c, int d, int e, int f);
void dmaKit_chan_init(int chan);

#endif // STUB_DMAKIT_H
