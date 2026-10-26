#include "q_math.h"

#include "compat/coduo_native_x87.h"
#include "compat/crt/atan2_compat.h"

#include <math.h>


/*
 * The complete quaternion trace/yaw cluster is shared by the Windows engine,
 * cgame, UI, and game module.  The four Windows copies are instruction-
 * identical apart from relocated constants and calls:
 *
 *                         engine      cgame       UI          game
 * QuatEigenTrace          0x4337f0    0x3004b950  0x40003920  0x200189a0
 * AngleEigenTrace         0x4338a0    0x3004ba00  0x400039d0  0x20018a50
 * QuatRatioEigenTrace     0x4338d0    0x3004ba30  0x40003a00  0x20018a80
 * RotationToYaw           0x433910    0x3004ba70  0x40003a40  0x20018ac0
 *
 * Linux retains the same public cluster in the dedicated engine at
 * 0x0806998f..0x08069b14 and in game.mp.uo.i386.so at RVAs
 * 0x0003d602..0x0003d7d6.  The Linux game symbols establish the canonical
 * names, including AngleEigenTrace; SinSquaredDegrees was only a descriptive
 * reconstruction name.
 */

/*
 * Windows folds the denominator w,z,y,x and returns the z,y,x numerator sum
 * live under PC=53.  Linux folds x,y,z,w, then explicitly stores the final
 * x,y,z sum to binary32 before reloading it.  long double is the project-wide
 * source carrier for the Windows ST0 contract on every host. Native x87
 * applies PC=53; other hosts retain their available native wide precision.
 */
long double QuatEigenTrace(const vec4_t quaternion)
{
    float xSquared;
    float ySquared;
    float zSquared;
    float lengthSquared;

    xSquared = (float)((long double)quaternion[0] * quaternion[0]);
    ySquared = (float)((long double)quaternion[1] * quaternion[1]);
    zSquared = (float)((long double)quaternion[2] * quaternion[2]);
    lengthSquared = (float)(
        (((long double)quaternion[3] * quaternion[3] + zSquared) +
         ySquared) + xSquared);

    if (lengthSquared != 0.0f) {
        const float reciprocal =
            (float)((long double)1.0f / lengthSquared);
        const float scaledX =
            (float)((long double)reciprocal * xSquared);
        const float scaledY =
            (float)((long double)reciprocal * ySquared);
        const float scaledZ =
            (float)((long double)reciprocal * zSquared);

        return ((long double)scaledZ + scaledY) + scaledX;
    }

    return 0.0f;
}

/*
 * Windows supplies a binary32 pi/180 operand directly to the x87 sine
 * intrinsic.  Linux multiplies by the exact binary64 pi/180 constant and
 * stores the product to binary64 for its system sin call.  Both square a
 * binary64 sine result and narrow the public result to binary32.
 */
float AngleEigenTrace(float degrees)
{
    const double sine = (double)coduo_x87_sinl(
        (long double)degrees * 0.01745329238474369f);

    return (float)(sine * sine);
}

/*
 * All six authoritative bodies conjugate the right operand, then evaluate
 * inverse(right) * left through the original second-times-first QuatMultiply
 * convention.  The Windows compiler inlines QuatInverse; Linux calls it.
 */
long double QuatRatioEigenTrace(const vec4_t left, const vec4_t right)
{
    vec4_t inverse;
    vec4_t product;

    QuatInverse(right, inverse);
    QuatMultiply(left, inverse, product);
    return QuatEigenTrace(product);
}

/*
 * Both platforms explicitly store x*x, x*x+y*y, and 2/length to binary32.
 * Windows then passes the two live x87 expressions to its CRT atan2 entry;
 * Linux materializes binary64 arguments. Windows uses a float-derived
 * 180/pi value (57.2957763671875); Linux uses the exact binary64 value
 * 57.29577951308232.  The final result is binary32 on both platforms.
 */
float RotationToYaw(const vec2_t rotation)
{
    float xSquared;
    float lengthSquared;
    float scale;
    WIDE_FLOAT numerator;
    WIDE_FLOAT denominator;

    xSquared = (float)((long double)rotation[0] * rotation[0]);
    lengthSquared = (float)(
        (long double)rotation[1] * rotation[1] + xSquared);
    scale = (float)((long double)2.0f / lengthSquared);
    numerator = (WIDE_FLOAT)rotation[1] * rotation[0] * scale;
    denominator = (WIDE_FLOAT)1.0f - (WIDE_FLOAT)scale * xSquared;
    return (float)(coduomp_crt_atan2l(numerator, denominator) *
                   (long double)57.2957763671875);
}
