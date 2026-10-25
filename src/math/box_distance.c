#include "q_math.h"


#include <stdint.h>

/*
 * The four Windows bodies have the same instruction stream apart from the
 * relocated zero constant:
 *
 *   CoDUOMP.exe                 0x00434930
 *   uo_cgame_mp_x86.dll        0x3004ca90
 *   uo_ui_mp_x86.dll           0x40004aa0
 *   uo_game_mp_x86.dll         0x20019ae0
 *
 * Linux coduo_lnxded 0x0806ab7e and game.mp.uo.i386.so RVA 0x0003ea0e
 * retain the same binary32 delta, square, selected-sum, and result stores.
 * Compiler scheduling and register allocation differ, but there is no
 * platform-specific computation.  In particular, each same-side product is
 * compared before a binary32 spill, while every square and accumulation is
 * explicitly narrowed to binary32.
 */
qboolean BoxDistSqrdExceeds(const vec3_t mins, const vec3_t maxs,
                            const vec3_t point, float distanceSquared)
{
    vec3_t minDelta;
    vec3_t maxDelta;
    float accumulated = 0.0f;

    for (int32_t component = 0; component < 3; ++component) {
        minDelta[component] = mins[component] - point[component];
        maxDelta[component] = maxs[component] - point[component];
    }

    for (int32_t component = 0; component < 3; ++component) {
        const long double sideProduct =
            (long double)minDelta[component] *
            (long double)maxDelta[component];
        const qboolean sameSide = !(sideProduct <= 0.0L);

        if (sameSide != qfalse) {
            const float minDeltaSquared = (float)(
                (long double)minDelta[component] *
                (long double)minDelta[component]);
            const float maxDeltaSquared = (float)(
                (long double)maxDelta[component] *
                (long double)maxDelta[component]);
            const float selected =
                maxDeltaSquared < minDeltaSquared
                    ? maxDeltaSquared
                    : minDeltaSquared;
            accumulated = (float)((long double)accumulated +
                                  (long double)selected);
        }
    }

    return accumulated > distanceSquared ? qtrue : qfalse;
}
