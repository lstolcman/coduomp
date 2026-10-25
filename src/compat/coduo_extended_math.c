/* NOT_FROM_ORIGINAL_SOURCE: native-x87 adapters for the pinned extended math
 * library. Its polynomial kernels require a 64-bit significand; preserve the
 * caller's rounding mode and restore its precision after each library call.
 * Direct square roots retain the caller's precision, like the native helper. */
#include "compat/coduo_extended_math.h"
#include "compat/coduo_native_x87.h"

enum { CODUOMP_MATH_X87_EXTENDED_PRECISION = 0x0300 };

/* NOT_FROM_ORIGINAL_SOURCE: preserve the caller's complete x87 control word. */
static uint16_t coduomp_math_enter(void)
{
    uint16_t saved;
    uint16_t extended;
    __asm__ __volatile__("fnstcw %0" : "=m"(saved));
    extended = (uint16_t)(saved | CODUOMP_MATH_X87_EXTENDED_PRECISION);
    __asm__ __volatile__("fldcw %0" : : "m"(extended));
    return saved;
}

/* NOT_FROM_ORIGINAL_SOURCE: restore precision without clearing exceptions. */
static void coduomp_math_leave(uint16_t saved)
{
    __asm__ __volatile__("fldcw %0" : : "m"(saved));
}

/* NOT_FROM_ORIGINAL_SOURCE: uniform ABI boundary for unary math kernels. */
#define CODUOMP_MATH_UNARY(name) \
    long double coduomp_math_##name(long double value) \
    { \
        uint16_t saved = coduomp_math_enter(); \
        long double result = coduomp_musl_##name(value); \
        coduomp_math_leave(saved); \
        return result; \
    }

CODUOMP_MATH_UNARY(acosl)
CODUOMP_MATH_UNARY(asinl)
CODUOMP_MATH_UNARY(cosl)
CODUOMP_MATH_UNARY(sinl)
CODUOMP_MATH_UNARY(expl)
CODUOMP_MATH_UNARY(logl)
CODUOMP_MATH_UNARY(fabsl)

#undef CODUOMP_MATH_UNARY

/* NOT_FROM_ORIGINAL_SOURCE: binary companion for the power kernel. */
long double coduomp_math_powl(long double base, long double exponent)
{
    uint16_t saved = coduomp_math_enter();
    long double result = coduomp_musl_powl(base, exponent);
    coduomp_math_leave(saved);
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the existing direct-FSQRT semantics. */
long double coduomp_math_sqrtl(long double value)
{
    return coduo_x87_sqrtl(value);
}
