#include "bg_bullet.h"

#include "compat/coduo_native_x87.h"
#include "compat/crt/random_compat.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>


enum {
    BG_BULLET_VECTOR_LANES = 3
};

#define BG_BULLET_TRACE_DISTANCE 10000.0f


/*
 * Complete random bullet-spread subsystem.  The Windows cgame/game bodies are
 * instruction-identical after relocating constants, calls, and data:
 *
 *   uo_cgame_mp_x86.dll  gunrandom 0x30049270, BG_Bullet_Endpos 0x3000fa20
 *   uo_game_mp_x86.dll   gunrandom 0x200162c0, BG_Bullet_Endpos 0x2000f7d0
 *
 * Linux game retains the same random-disk and endpoint geometry at RVAs
 * 0x000399e0 and 0x0003011c.  Whole platform bodies preserve the genuine CRT,
 * constant-width, tangent boundary, and lateral-product spill differences.
 */

void gunrandom(float *x, float *y)
{
    const float angleSample = (float)coduo_server_rand();
    float angleDegrees;
    float radiusSample;
    float radius;
    float radians;
    float sine;
    float cosine;

    angleDegrees = (float)(((long double)angleSample / 32768.0f) *
                           360.0f);

    radiusSample = (float)coduo_server_rand();
    radius = (float)((long double)radiusSample / 32768.0f);
    radians = (float)(((long double)angleDegrees * 3.1415927f) /
                      180.0f);

    BG_SinCos(radians, &sine, &cosine);
    *x = (float)((long double)cosine * radius);
    *y = (float)((long double)sine * radius);
}

void BG_Bullet_Endpos(float spread, vec3_t end,
                      const float *muzzlePoints)
{
    const float *const forward = muzzlePoints;
    const float *const right = muzzlePoints + 3;
    const float *const up = muzzlePoints + 6;
    const float *const origin = muzzlePoints + 9;
    float randomRight;
    float randomUp;
    float radius;

    long double radians = (long double)spread * 3.1415927f;

    radians *= 0.0055555557f;
    radius = (float)(coduo_x87_tanl(radians) *
                     (long double)BG_BULLET_TRACE_DISTANCE);

    gunrandom(&randomRight, &randomUp);

    const long double lateralRight =
        (long double)randomRight * radius;
    const long double lateralUp =
        (long double)randomUp * radius;

    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = (float)((long double)forward[lane] *
                                BG_BULLET_TRACE_DISTANCE +
                            origin[lane]);
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = (float)(lateralRight * right[lane] + end[lane]);
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = (float)(lateralUp * up[lane] + end[lane]);
    }
}

#undef BG_BULLET_TRACE_DISTANCE
