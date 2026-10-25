#include "q_math.h"



/*
 * The four Windows copies of each function are instruction-identical apart
 * from relocated constants:
 *
 *                         CoDUOMP       cgame       UI          game
 *   PitchToQuaternion     0x00433970    0x3004bad0  0x40003aa0  0x20018b20
 *   YawToQuaternion       0x004339c0    0x3004bb20  0x40003af0  0x20018b70
 *   RollToQuaternion      0x00433a10    0x3004bb70  0x40003b40  0x20018bc0
 *
 * Windows multiplies by a binary32 pi/360 constant. The Linux engine bodies
 * at 0x08069b14, 0x08069b5e, and 0x08069ba8 and game bodies at RVAs
 * 0x0003d7d7, 0x0003d831, and 0x0003d88b use binary64 pi/360. Linux symbols
 * and the supporting Mac symbols establish Pitch/Yaw/RollToQuaternion as the
 * canonical names; the engine reconstruction's QuatFrom*AxisDegrees names
 * described the operation but were not original identifiers.
 */
void PitchToQuaternion(float degrees, vec4_t quaternion)
{
    float halfRadians;

    halfRadians = (float)((long double)degrees * 0.008726646f);
    quaternion[0] = 0.0f;
    quaternion[2] = 0.0f;
    BG_SinCos(halfRadians, &quaternion[1], &quaternion[3]);
}

void YawToQuaternion(float degrees, vec4_t quaternion)
{
    float halfRadians;

    halfRadians = (float)((long double)degrees * 0.008726646f);
    quaternion[0] = 0.0f;
    quaternion[1] = 0.0f;
    BG_SinCos(halfRadians, &quaternion[2], &quaternion[3]);
}

void RollToQuaternion(float degrees, vec4_t quaternion)
{
    float halfRadians;

    halfRadians = (float)((long double)degrees * 0.008726646f);
    quaternion[1] = 0.0f;
    quaternion[2] = 0.0f;
    BG_SinCos(halfRadians, &quaternion[0], &quaternion[3]);
}
