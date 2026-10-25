#ifndef CODUO_MATH_SQRT_H
#define CODUO_MATH_SQRT_H


#include <math.h>
#include <stdint.h>

#define CODUOMP_SQRT_EXTENDED_PRECISION_BIT UINT16_C(0x0100)

/* NOT_FROM_ORIGINAL_SOURCE: preserve the Linux double-sqrt call boundary.
 * Its x87 library selects 53-bit precision for FSQRT and restores the caller's
 * control word. Some host libraries instead retain the ambient 64-bit result,
 * which can change the following float store. Keep the squared operand's
 * earlier binary64 store and the caller's later binary32 store separate. */
static inline double coduomp_math_sqrt(double value)
{
    return sqrt(value);
}

#endif
