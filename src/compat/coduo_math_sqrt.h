#ifndef CODUO_MATH_SQRT_H
#define CODUO_MATH_SQRT_H

#include "compat/coduo_x87emu.h"

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
#if defined(LINUX_BEHAVIOR) && !EMULATE_X87 && \
    (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    uint16_t savedControlWord;
    uint16_t sqrtControlWord;
    double result;

    __asm__ __volatile__("fnstcw %0" : "=m"(savedControlWord));
    /* Both supported precision policies become PC=53 by clearing this bit. */
    sqrtControlWord = savedControlWord & (uint16_t)~CODUOMP_SQRT_EXTENDED_PRECISION_BIT;
    __asm__ __volatile__("fldcw %[control]\n\t"
                         "fldl %[value]\n\t"
                         "fsqrt\n\t"
                         "fstpl %[result]\n\t"
                         "fldcw %[saved]"
                         : [result] "=m"(result)
                         : [control] "m"(sqrtControlWord), [value] "m"(value),
                           [saved] "m"(savedControlWord)
                         : "st", "memory");
    return result;
#else
    return sqrt(value);
#endif
}

#endif
