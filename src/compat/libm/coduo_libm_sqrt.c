/* CoduoLibm_Sqrt — reconstruction of the MSVC CRT sqrt.
 *
 * Stock: uo_gamex86.dll 0x2008a640 (public entry) -> 0x2008a65d (body).
 *
 *   2008a643  fst   QWORD PTR [esp]        ; argument kept as a double
 *   2008a646  call  0x2008aa88             ; classify: ZF set iff exponent is all ones
 *   2008a666  je    0x2008a6b9             ; -> inf/NaN path (flags survive the call)
 *   2008a668  cmp   WORD PTR [esp],0x27f
 *   2008a670  call  0x2008aa15             ; control word := (saved & 0x300) | 0x7f
 *   2008a675  test  eax,0x80000000
 *   2008a67a  jne   0x2008a69b             ; -> negative path
 *   2008a67c  fsqrt
 *
 * The control-word helper preserves the precision-control field and only forces
 * round-to-nearest with all exceptions masked. With the ambient word 0x137F it
 * yields 0x37F, so the root is taken at 64-bit extended precision and narrowed
 * to double on return. That double rounding is stock behaviour.
 *
 * Negative path (0x2008a69b) distinguishes -0.0 from a genuine negative: only
 * when exponent, mantissa and low dword are all zero does it rejoin the normal
 * exit, returning -0.0 without executing FSQRT. Anything else is a domain error
 * and loads the indefinite QNaN from 0x200b0180.
 *
 * Inf/NaN path (0x2008a6b9): +inf returns +inf with FSQRT skipped; -inf falls
 * through to the domain error; a NaN is quieted (0x2008aa2c adds 1.0, which
 * turns a signalling NaN into a quiet one).
 */
#include "coduo_libm.h"

#include <string.h>
#include "coduo_wide_math.h"

#define SQRT_HIGH_SIGN_MASK     0x80000000u
#define SQRT_HIGH_EXPONENT_MASK 0x7ff00000u
#define SQRT_HIGH_MANTISSA_MASK 0x000fffffu

static uint32_t CoduoLibm_DoubleHighDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)(bits >> 32);
}

static uint32_t CoduoLibm_DoubleLowDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)bits;
}


/* The x87 indefinite QNaN the CRT loads from 0x200b0180 on a domain error. */
static double CoduoLibm_IndefiniteNaN(void)
{
    const uint64_t bits = UINT64_C(0xfff8000000000000);
    double result;

    memcpy(&result, &bits, sizeof(result));
    return result;
}

double CoduoLibm_Sqrt(double value)
{
    const uint32_t high = CoduoLibm_DoubleHighDword(value);
    const uint32_t low = CoduoLibm_DoubleLowDword(value);
    const uint32_t exponent = high & SQRT_HIGH_EXPONENT_MASK;
    const uint32_t mantissa = high & SQRT_HIGH_MANTISSA_MASK;

    if (exponent == SQRT_HIGH_EXPONENT_MASK) {
        /* 0x2008a6b9: infinity or NaN. */
        if (mantissa != 0u || low != 0u) {
            /* NaN: quieted by the add at 0x2008aa39, value otherwise kept. */
            return (double)((long double)value + 1.0L);
        }
        if ((high & SQRT_HIGH_SIGN_MASK) == 0u) {
            return value; /* +inf: FSQRT skipped, argument returned as is. */
        }
        return CoduoLibm_IndefiniteNaN(); /* -inf: domain error. */
    }

    if ((high & SQRT_HIGH_SIGN_MASK) != 0u) {
        /* 0x2008a69b: negative, unless every magnitude bit is clear (-0.0). */
        if (exponent != 0u || mantissa != 0u || low != 0u) {
            return CoduoLibm_IndefiniteNaN();
        }
        return value; /* -0.0 rejoins the normal exit without FSQRT. */
    }

    /* 0x2008a67c: FSQRT at the ambient 64-bit precision, narrowed on return. */
    {
        /* The shared adapter uses native FSQRT where supported and the
         * widest native C square root elsewhere. */
        return (double)coduomp_math_sqrtl((long double)value);
    }
}
