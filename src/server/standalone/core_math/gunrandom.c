#include <stdlib.h>

#include "compat/crt/random_compat.h"

#include "core_math_private.h"

void gunrandom(float *x, float *y)
{
    float angleDegrees;
    float radius;
    float sinValue;
    float cosValue;

    /* Angle and radius consume normalized samples in either server mode. */
    angleDegrees = (float)(coduo_server_rand_unit() * 360.0);
    radius = (float)coduo_server_rand_unit();
    BG_SinCos((float)(((double)angleDegrees * 3.141592653589793) / 180.0),
                 &sinValue, &cosValue);
    *x = radius * cosValue;
    *y = radius * sinValue;
}
