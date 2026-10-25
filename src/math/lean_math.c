#include "q_math.h"

#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stddef.h>

/*
 * Complete shared lean-curve helper cluster.  The three canonical names are
 * retained by the supporting Mac engine, cgame, and game images.  The Windows
 * cgame and UI bodies are instruction-identical, and the Windows engine bodies
 * use the same operation graph:
 *
 *   CoDUOMP.exe             0x00450530, 0x00450560, 0x00450580
 *   uo_cgame_mp_x86.dll    0x3004f320, 0x3004f350, 0x3004f370
 *   uo_ui_mp_x86.dll       0x40007350, 0x40007380, 0x400073a0
 *   game.mp.uo.i386.so     0x00094623, 0x00094651, 0x00094688
 *
 * Linux game retains calls to GetLeanFraction and AngleVectors where the
 * optimized Windows bodies inline them. The forward curve and position
 * multiply-adds keep their intermediates wide. Linux's inverse curve alone
 * rounds the square root to binary32 before subtracting it from one.
 * GetLeanFraction and UnGetLeanFraction have binary32 source interfaces;
 * native i386 may nevertheless retain the result in ST0 until its caller
 * spills it.
 */


#if defined(__i386__) && defined(__GNUC__) && !defined(__clang__)
/* Keep the original live ST0 return when the caller has not requested a
 * binary32 store. The other ABIs retain their ordinary float interface. */
__attribute__((optimize("-fexcess-precision=fast")))
#endif
float GetLeanFraction(float fraction)
{
#if   defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
    return (2.0f - fabsf(fraction)) * fraction;
#else
    return (float)((2.0L - (long double)fabsf(fraction)) * (long double)fraction);
#endif
}

float UnGetLeanFraction(float fraction)
{
    return (float)(1.0L - coduo_x87_sqrtl(1.0L - (long double)fraction));
}

void AddLeanToPosition(vec3_t position, float yaw, float leanFraction,
                       float maxLean, float side)
{
    float lean;
    vec3_t angles;
    vec3_t right;

    if (leanFraction == 0.0f) {
        return;
    }

    lean = GetLeanFraction(leanFraction);
    angles[2] = lean * maxLean;
    angles[0] = 0.0f;
    angles[1] = yaw;
    AngleVectors(angles, NULL, right, NULL);

    lean *= side;
    position[0] = (float)((long double)right[0] * (long double)lean + (long double)position[0]);
    position[1] = (float)((long double)right[1] * (long double)lean + (long double)position[1]);
    position[2] = (float)((long double)right[2] * (long double)lean + (long double)position[2]);
}
