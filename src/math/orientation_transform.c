#include "q_math.h"


#include <stdint.h>


/*
 * Complete orientation transform bank.  The 0x138-byte Windows cluster is
 * byte-identical in all four authoritative images:
 *
 *   CoDUOMP.exe                 0x004506f0..0x00450828
 *   uo_cgame_mp_x86.dll        0x3004f4e0..0x3004f618
 *   uo_ui_mp_x86.dll           0x40007510..0x40007648
 *   uo_game_mp_x86.dll         0x20058d00..0x20058e38
 *
 * The Linux engine functions at 0x08087ab5, 0x08087b63, 0x08087bfe, and
 * 0x08087ca7 have the same instruction-level arithmetic as the game-module
 * functions at RVAs 0x0009474a, 0x000947f8, 0x00094893, and 0x0009493c.
 * Linux deliberately stores the origin-relative position deltas as binary32;
 * Windows keeps those three values live in x87 registers.  The platforms also
 * retain different addition orders, so each platform body remains whole.
 */


void OrientationPosToWorldPos(const orientation_t *orientation,
                              const vec3_t position, vec3_t worldPosition)
{
    for (int32_t component = 0; component < 3; ++component) {
        worldPosition[component] = (float)(
            (((long double)orientation->axis[2][component] * position[2] +
              (long double)orientation->axis[0][component] * position[0]) +
             (long double)orientation->axis[1][component] * position[1]) +
            (long double)orientation->origin[component]);
    }
}

void OrientationDirToWorldDir(const orientation_t *orientation,
                              const vec3_t direction, vec3_t worldDirection)
{
    for (int32_t component = 0; component < 3; ++component) {
        worldDirection[component] = (float)(
            ((long double)orientation->axis[2][component] * direction[2] +
             (long double)orientation->axis[0][component] * direction[0]) +
            (long double)orientation->axis[1][component] * direction[1]);
    }
}

void OrientationPosFromWorldPos(const orientation_t *orientation,
                                const vec3_t worldPosition, vec3_t position)
{
    const long double translated0 =
        (long double)worldPosition[0] - orientation->origin[0];
    const long double translated1 =
        (long double)worldPosition[1] - orientation->origin[1];
    const long double translated2 =
        (long double)worldPosition[2] - orientation->origin[2];

    for (int32_t component = 0; component < 3; ++component) {
        position[component] = (float)(
            ((long double)orientation->axis[component][2] * translated2 +
             (long double)orientation->axis[component][1] * translated1) +
            (long double)orientation->axis[component][0] * translated0);
    }
}

void OrientationDirFromWorldDir(const orientation_t *orientation,
                                const vec3_t worldDirection, vec3_t direction)
{
    for (int32_t component = 0; component < 3; ++component) {
        direction[component] = (float)(
            ((long double)orientation->axis[component][2] * worldDirection[2] +
             (long double)orientation->axis[component][0] * worldDirection[0]) +
            (long double)orientation->axis[component][1] * worldDirection[1]);
    }
}
