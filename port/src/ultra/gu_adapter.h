// Adapter exposing the decomp's float libultra gu math to the port.
//
// The decomp's gu (src/libultra/gu) computes in float[4][4] and produces the
// same split intPart/fracPart Mtx the port uses. We forward-declare just the
// float entry points (no decomp header leaks into the port) and convert the
// result into the port's Mtx so the display-list interpreter can consume it.
#ifndef MMPS2_GU_ADAPTER_H
#define MMPS2_GU_ADAPTER_H

#include "gfx/gbi.h"

void mmps2_gu_perspective(Mtx *m, float fovy, float aspect, float near, float far);
void mmps2_gu_lookat(Mtx *m, float xEye, float yEye, float zEye,
                     float xAt, float yAt, float zAt,
                     float xUp, float yUp, float zUp);

#endif // MMPS2_GU_ADAPTER_H
