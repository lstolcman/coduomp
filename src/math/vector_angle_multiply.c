#include "q_math.h"


#include <string.h>


/*
 * The four Windows bodies are instruction-identical apart from the relocated
 * binary32 pi/180 constant:
 *
 *   CoDUOMP.exe                 0x00433550
 *   uo_cgame_mp_x86.dll        0x3004b6b0
 *   uo_ui_mp_x86.dll           0x40003680
 *   uo_game_mp_x86.dll         0x20018700
 *
 * The Linux bodies at coduo_lnxded 0x080695a6 and game.mp.uo.i386.so RVA
 * 0x0003d1e9 instead multiply by a binary64 pi/180 constant. Both platforms
 * spill the radians and each rotated component to binary32, and both retain
 * the original X lane until after calculating the new Y lane. The complete
 * Windows body preserves its constant widths on every supported host.
 */
void VectorAngleMultiply(vec2_t vector, float angleDegrees)
{
    float sine;
    float cosine;
    float radians;
    float rotatedX;

    radians = (float)((long double)angleDegrees * 0.017453292f);
    BG_SinCos(radians, &sine, &cosine);
    rotatedX = (float)((long double)vector[0] * cosine -
                       (long double)vector[1] * sine);
    vector[1] = (float)((long double)vector[0] * sine +
                        (long double)vector[1] * cosine);
    memcpy(&vector[0], &rotatedX, sizeof(vector[0]));
}
