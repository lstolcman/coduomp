#ifndef BG_BOB_H
#define BG_BOB_H

#include "qcommon/player_state_types.h"
#include "compat/coduo_x87_eval.h"

/* Windows leaves each result live in ST0. Linux narrows the cycle and vertical
 * factor to binary32; its horizontal factor returns the final product live. */
long double BG_GetBobCycle(const playerState_t *ps);
long double BG_GetVerticalBobFactor(const playerState_t *ps, float phase,
                                    float amplitude, float maxAmplitude);
long double BG_GetHorizontalBobFactor(const playerState_t *ps, float phase,
                                      float amplitude, float maxAmplitude);

#endif
