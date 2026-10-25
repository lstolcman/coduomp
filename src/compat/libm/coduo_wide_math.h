/* NOT_FROM_ORIGINAL_SOURCE: shared native arithmetic cores for extended
 * source expressions. These adapters preserve the documented operation and
 * rounding boundaries, not the CRT's dispatch, errno or exception machinery.
 * Unsupported domains use platform math; there is no software x87 backend. */
#ifndef CODUO_WIDE_MATH_H
#define CODUO_WIDE_MATH_H

#include <math.h>
#include "compat/coduo_x87_math_ops.h"

#if CODUO_X87_MATH_AVAILABLE

#define CODUOMP_X87_TRIG_LIMIT 0x1p63L
#define CODUOMP_X87_PRECISION_MASK 0x0300u
#define CODUOMP_X87_NEAREST_MASKED 0x007fu

/* NOT_FROM_ORIGINAL_SOURCE: classification without an SDK binary64 narrowing. */
static CODUO_MATH_INLINE int coduomp_math_finitel(long double value)
{
    return value <= CODUO_MATH_LDBL_MAX && value >= -CODUO_MATH_LDBL_MAX;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve an extended sine operand and result.
 * FSIN leaves |x| >= 2^63 unchanged; use platform range reduction there. */
static CODUO_MATH_INLINE long double coduomp_math_sinl(long double value)
{
    if (value > -CODUOMP_X87_TRIG_LIMIT && value < CODUOMP_X87_TRIG_LIMIT)
        return coduomp_x87_sin(value);
    return (long double)sin((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: extended cosine with the same hardware range. */
static CODUO_MATH_INLINE long double coduomp_math_cosl(long double value)
{
    if (value > -CODUOMP_X87_TRIG_LIMIT && value < CODUOMP_X87_TRIG_LIMIT)
        return coduomp_x87_cos(value);
    return (long double)cos((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: the CRT core factors (1+x)*(1-x), not 1-x*x.
 * Keep arithmetic in C and the raw result without a binary64 return store. */
static CODUO_MATH_INLINE long double coduomp_math_asinl(long double value)
{
    if (value >= -1.0L && value <= 1.0L) {
        const long double root = coduomp_x87_sqrt((1.0L + value) * (1.0L - value));
        return coduomp_x87_atan2(value, root);
    }
    return (long double)asin((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: the same CRT core with reversed FPATAN operands. */
static CODUO_MATH_INLINE long double coduomp_math_acosl(long double value)
{
    if (value >= -1.0L && value <= 1.0L) {
        const long double root = coduomp_x87_sqrt((1.0L + value) * (1.0L - value));
        return coduomp_x87_atan2(root, value);
    }
    return (long double)acos((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the hardware square-root result. */
static CODUO_MATH_INLINE long double coduomp_math_sqrtl(long double value)
{
    return coduomp_x87_sqrt(value);
}

/* NOT_FROM_ORIGINAL_SOURCE: the inlined water logarithm loads LN2 then FYL2X.
 * No double call boundary or CRT control-word change occurs at this site. */
static CODUO_MATH_INLINE long double coduomp_math_logl(long double value)
{
    if (value > 0.0L && coduomp_math_finitel(value))
        return coduomp_x87_log2_product(coduomp_x87_ln2(), value);
    return (long double)log((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: shared finite exponent core. FRNDINT follows the
 * active rounding mode; the remainder is in [-1,1], F2XM1's input interval.
 * FSCALE avoids narrowing the integer part to a C integer or multiplying by
 * an intermediate power of two that could overflow independently. */
static CODUO_MATH_INLINE long double coduomp_math_exp2l_finite(long double value)
{
    const long double integer = coduomp_x87_round(value);
    const long double fraction = value - integer;
    const long double significand = coduomp_x87_exp2m1(fraction) + 1.0L;
    return coduomp_x87_scale(significand, integer);
}

/* NOT_FROM_ORIGINAL_SOURCE: the inlined water exponential multiplies by the
 * hardware LOG2E constant before the shared exponent core. */
static CODUO_MATH_INLINE long double coduomp_math_expl(long double value)
{
    if (coduomp_math_finitel(value)) {
        const long double exponent = value * coduomp_x87_log2e();
        if (coduomp_math_finitel(exponent))
            return coduomp_math_exp2l_finite(exponent);
    }
    return (long double)exp((double)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: the CRT's native power path stores/reloads the
 * base as binary64, retains the exponent in x87, then uses FYL2X and the
 * exponent core. Its control helper preserves PC, selects round-to-nearest
 * and masks exceptions. Negative integer powers use the same core and an
 * odd/even test on exponent/2. Other domains use the platform CRT. The
 * optional original SSE2 dispatch and its exception dispatcher are omitted. */
static CODUO_MATH_INLINE long double coduomp_math_powl(long double base, long double exponent)
{
    volatile double storedBase = (double)base;
    base = storedBase;
    if (base != 0.0L && coduomp_math_finitel(base) && coduomp_math_finitel(exponent)) {
        const unsigned short savedControl = coduomp_x87_control();
        int negativeResult = 0;
        coduomp_x87_set_control((unsigned short)((savedControl & CODUOMP_X87_PRECISION_MASK) | CODUOMP_X87_NEAREST_MASKED));
        if (base < 0.0L) {
            if (coduomp_x87_round(exponent) != exponent) {
                coduomp_x87_set_control(savedControl);
                return (long double)pow((double)base, (double)exponent);
            }
            const long double half = exponent * 0.5L;
            negativeResult = coduomp_x87_round(half) != half;
            base = -base;
        }
        const long double logarithm = coduomp_x87_log2_product(exponent, base);
        if (coduomp_math_finitel(logarithm)) {
            long double result = coduomp_math_exp2l_finite(logarithm);
            if (negativeResult)
                result = -result;
            coduomp_x87_set_control(savedControl);
            return result;
        }
        coduomp_x87_set_control(savedControl);
        if (negativeResult)
            base = -base;
    }
    return (long double)pow((double)base, (double)exponent);
}

#else

/* NOT_FROM_ORIGINAL_SOURCE: platform fallbacks when native x87 is unavailable.
 * Microsoft has no extended long-double CRT ABI; binary64 is intentional.
 * These rendering fallbacks prioritize native platform speed over emulation. */
#define CODUOMP_PLATFORM_UNARY(name) \
    static CODUO_MATH_INLINE long double coduomp_math_##name##l(long double value) \
    { return (long double)name((double)value); }

CODUOMP_PLATFORM_UNARY(sin)
CODUOMP_PLATFORM_UNARY(cos)
CODUOMP_PLATFORM_UNARY(asin)
CODUOMP_PLATFORM_UNARY(acos)
CODUOMP_PLATFORM_UNARY(sqrt)
CODUOMP_PLATFORM_UNARY(log)
CODUOMP_PLATFORM_UNARY(exp)
#undef CODUOMP_PLATFORM_UNARY

/* NOT_FROM_ORIGINAL_SOURCE: native platform power without x87 emulation. */
static CODUO_MATH_INLINE long double coduomp_math_powl(long double base, long double exponent)
{
    return (long double)pow((double)base, (double)exponent);
}

#endif
#endif
