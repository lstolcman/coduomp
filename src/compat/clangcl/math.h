/* NOT_FROM_ORIGINAL_SOURCE: clang-cl's native-x87 math boundary. The build
 * places this directory before the SDK only for native-x87 product targets;
 * third-party libraries retain their normal Microsoft CRT ABI. */
#ifndef CODUO_CLANGCL_MATH_H
#define CODUO_CLANGCL_MATH_H

#include <corecrt_math.h>
#include "compat/coduo_native_x87.h"

/* NOT_FROM_ORIGINAL_SOURCE: native instruction adapters for the project's
 * extended carriers. Keep the caller's x87 control word. These are arithmetic
 * kernels, not replacements for the CRT's dispatch and exception machinery.
 * Unsupported domains use the platform's binary64 math; no software x87 is
 * used. The SDK declarations above retain their normal Microsoft ABI. */

/* NOT_FROM_ORIGINAL_SOURCE: FSIN/FCOS leave an out-of-range operand unchanged
 * at |x| >= 2^63. Delegate that range, infinities and NaNs to the platform. */
#define CODUOMP_X87_TRIG_LIMIT 0x1p63L

/* NOT_FROM_ORIGINAL_SOURCE: preserve an extended sine operand and result. */
static __inline__ __attribute__((always_inline)) long double coduomp_math_sinl(long double value)
{
    if (__builtin_fabsl(value) < CODUOMP_X87_TRIG_LIMIT) {
        __asm__ __volatile__("fsin" : "+t"(value));
        return value;
    }
    return (long double)sin((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve an extended cosine operand and result. */
static __inline__ __attribute__((always_inline)) long double coduomp_math_cosl(long double value)
{
    if (__builtin_fabsl(value) < CODUOMP_X87_TRIG_LIMIT) {
        __asm__ __volatile__("fcos" : "+t"(value));
        return value;
    }
    return (long double)cos((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: CRT arithmetic core (1+x)*(1-x), FSQRT, FPATAN.
 * Keep the raw result rather than adding a binary64 return boundary. */
static __inline__ __attribute__((always_inline)) long double coduomp_math_asinl(long double value)
{
    if (__builtin_fabsl(value) <= 1.0L) {
        long double result;
        __asm__ __volatile__("fld1\n\t"
                             "fadd %%st(1), %%st\n\t"
                             "fld1\n\t"
                             "fsub %%st(2), %%st\n\t"
                             "fmulp\n\t"
                             "fsqrt\n\t"
                             "fpatan"
                             : "=t"(result) : "0"(value) : "st(1)", "st(2)");
        return result;
    }
    return (long double)asin((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: the same root with reversed FPATAN operands. */
static __inline__ __attribute__((always_inline)) long double coduomp_math_acosl(long double value)
{
    if (__builtin_fabsl(value) <= 1.0L) {
        long double result;
        __asm__ __volatile__("fld1\n\t"
                             "fadd %%st(1), %%st\n\t"
                             "fld1\n\t"
                             "fsub %%st(2), %%st\n\t"
                             "fmulp\n\t"
                             "fsqrt\n\t"
                             "fxch %%st(1)\n\t"
                             "fpatan"
                             : "=t"(result) : "0"(value) : "st(1)", "st(2)");
        return result;
    }
    return (long double)acos((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: FLDLN2/FYL2X as used by the native water path. */
static __inline__ __attribute__((always_inline)) long double coduomp_math_logl(long double value)
{
    if (value > 0.0L && __builtin_isfinite(value)) {
        __asm__ __volatile__("fldln2\n\tfxch %%st(1)\n\tfyl2x" : "+t"(value) : : "st(1)");
        return value;
    }
    return (long double)log((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: split a finite base-two exponent at FRNDINT,
 * evaluate its fractional part with F2XM1, then scale by its integer part.
 * The water exponential and CRT power cores use this operation graph. */
static __inline__ __attribute__((always_inline)) long double coduomp_math_exp2l_finite(long double value)
{
    __asm__ __volatile__("fld %%st(0)\n\t"
                         "frndint\n\t"
                         "fxch %%st(1)\n\t"
                         "fsub %%st(1), %%st\n\t"
                         "f2xm1\n\t"
                         "fld1\n\t"
                         "faddp\n\t"
                         "fscale\n\t"
                         "fstp %%st(1)"
                         : "+t"(value) : : "st(1)", "st(2)");
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the native FLDL2E/FMUL exponential core. */
static __inline__ __attribute__((always_inline)) long double coduomp_math_expl(long double value)
{
    long double exponent = value;
    if (__builtin_isfinite(value)) {
        __asm__ __volatile__("fldl2e\n\tfmulp" : "+t"(exponent) : : "st(1)");
        if (__builtin_isfinite(exponent))
            return coduomp_math_exp2l_finite(exponent);
    }
    return (long double)exp((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: native positive-base power core. Nonpositive
 * bases and nonfinite inputs/results use the platform's domain handling.
 * This does not reproduce the original CRT's optional SSE2 dispatch. */
static __inline__ __attribute__((always_inline)) long double coduomp_math_powl(long double base, long double exponent)
{
    if (base > 0.0L && __builtin_isfinite(base) && __builtin_isfinite(exponent)) {
        long double logarithm;
        __asm__ __volatile__("fyl2x" : "=t"(logarithm) : "0"(base), "u"(exponent) : "st(1)");
        if (__builtin_isfinite(logarithm))
            return coduomp_math_exp2l_finite(logarithm);
    }
    return (long double)pow((double)base, (double)exponent);
}

#define acosl coduomp_math_acosl
#define asinl coduomp_math_asinl
#define cosl coduomp_math_cosl
#define sinl coduomp_math_sinl
#define expl coduomp_math_expl
#define logl coduomp_math_logl
#define powl coduomp_math_powl
#define fabsl __builtin_fabsl
#define sqrtl coduo_x87_sqrtl

#ifndef __cplusplus
#undef isnan
#undef isinf
#undef isfinite
#undef signbit
#define isnan(value) __builtin_isnan(value)
#define isinf(value) __builtin_isinf(value)
#define isfinite(value) __builtin_isfinite(value)
#define signbit(value) __builtin_signbit(value)
#endif

#endif
