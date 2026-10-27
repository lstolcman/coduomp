#include "pow_compat.h"
#include "pow_tables.h"
#include "compat/coduo_x87_math_ops.h"

#include <errno.h>
#include <fenv.h>
#include <stdint.h>
#include <string.h>

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#define CODUOMP_POW_INTEL 1
#include <emmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#if defined(__GNUC__) || defined(__clang__)
#define CODUOMP_POW_SSE2 __attribute__((target("sse2")))
#else
#define CODUOMP_POW_SSE2
#endif
#else
#define CODUOMP_POW_INTEL 0
#define CODUOMP_POW_SSE2
#endif

#define POW_SIGN UINT64_C(0x8000000000000000)
#define POW_MAGNITUDE UINT64_C(0x7fffffffffffffff)
#define POW_EXPONENT UINT64_C(0x7ff0000000000000)
#define POW_FRACTION UINT64_C(0x000fffffffffffff)
#define POW_QUIET UINT64_C(0x0008000000000000)
#define POW_INDEFINITE UINT64_C(0xfff8000000000000)
#define POW_ONE UINT64_C(0x3ff0000000000000)
#define POW_SPLIT UINT64_C(0xfffffffff8000000)
#define POW_SIGNALING_NAN UINT64_C(0x7ff0000000000001)
#define POW_AMD_VENDOR_B UINT32_C(0x68747541)
#define POW_AMD_VENDOR_C UINT32_C(0x444d4163)
#define POW_AMD_VENDOR_D UINT32_C(0x69746e65)

/* Binary64 coefficients retain their exact stored bit patterns. */
#define POW_SUBNORMAL_NORMALIZER 0x1p64
#define POW_LOG_RESIDUAL_BIAS -0x1.715474p0
#define POW_MIN_NORMAL 0x1p-1022
#define POW_INDEX_SCALE 128.0
#define POW_INDEX_ROUND_BIAS 0x1.8p52
#define POW_LOG_CUBIC_HI 0x1.47fd462b3b816p-3
#define POW_LOG_QUARTIC_HI -0x1.550472a8bb463p-4
#define POW_LOG_LINEAR_LO 0x1.9c3a6966457eep-24
#define POW_LOG_QUADRATIC_LO -0x1.62e4346694107p-2
#define POW_INDEX_RECIPROCAL 0x1p-7
#define POW_EXP_QUARTIC 0x1.3b2ab6fba4e77p-7
#define POW_EXP_QUINTIC 0x1.5d87fe78a6731p-10
#define POW_EXP_QUADRATIC 0x1.ebfbdff82c58ep-3
#define POW_EXP_CUBIC 0x1.c6b08d704a0bfp-5
#define POW_LOG_TWO 0x1.62e42fefa39efp-1
#define POW_MAX_NORMAL_SCALE_BITS UINT64_C(0x7fe0000000000000)
#define POW_MIN_NORMAL_BITS UINT64_C(0x0010000000000000)
#define POW_X87_RANGE_SCALE 1536.0L

enum {
    POW_EXPONENT_BIAS = 1023,
    POW_FRACTION_BITS = 52,
    POW_EXP_TABLE_COUNT = 128,
    POW_SCALE_BIAS = 1023 * POW_EXP_TABLE_COUNT,
    POW_NORMAL_LOW = -0x1e1ff,
    POW_NORMAL_HIGH = 0x1ff7f,
    POW_EXTREME_LOW = -0x3fe00,
    POW_EXTREME_HIGH = 0x40000,
    POW_PRODUCT_LOW = 0x3c70,
    POW_PRODUCT_HIGH = 0x40a0,
    POW_MXCSR_MASKS = 0x1f80,
    POW_X87_MASKS = 0x007f,
    POW_X87_PRECISION = 0x0300,
    POW_NONINTEGRAL = 0,
    POW_ODD = 1,
    POW_EVEN = 2,
    POW_CPU_FEATURES_LEAF = 1,
    POW_CPU_SSE2_BIT = 26,
    POW_CPU_EXTENDED_FAMILY = 15
};

/* NOT_FROM_ORIGINAL_SOURCE: alias-safe binary64 representation boundaries. */
static uint64_t coduomp_pow_bits(double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* NOT_FROM_ORIGINAL_SOURCE: construct a binary64 value without host libm. */
static double coduomp_pow_value(uint64_t bits)
{
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: each original scalar/vector lane operation rounds
 * to binary64. Intel intrinsics keep this independent of x87 precision and
 * use MXCSR, including its rounding and denormal modes. Other hosts use native
 * double arithmetic with an explicit store; contraction is disabled by the
 * product build. No x87 arithmetic or instruction emulation is involved. */
#if CODUOMP_POW_INTEL
#define CODUOMP_POW_OPERATION(name, intrinsic, operation) \
    static CODUOMP_POW_SSE2 CODUO_MATH_INLINE double coduomp_pow_##name(double a, double b) \
    { return _mm_cvtsd_f64(intrinsic(_mm_set_sd(a), _mm_set_sd(b))); }
#else
#define CODUOMP_POW_OPERATION(name, intrinsic, operation) \
    static CODUO_MATH_INLINE double coduomp_pow_##name(double a, double b) \
    { volatile double result = a operation b; return result; }
#endif
CODUOMP_POW_OPERATION(add, _mm_add_sd, +)
CODUOMP_POW_OPERATION(sub, _mm_sub_sd, -)
CODUOMP_POW_OPERATION(mul, _mm_mul_sd, *)
CODUOMP_POW_OPERATION(div, _mm_div_sd, /)
#undef CODUOMP_POW_OPERATION

/* NOT_FROM_ORIGINAL_SOURCE: CVTSD2SI uses the active rounding mode. The kernel
 * only reaches this conversion with a finite value well inside int32 range. */
static CODUOMP_POW_SSE2 int coduomp_pow_integer(double value)
{
#if CODUOMP_POW_INTEL
    return _mm_cvtsd_si32(_mm_set_sd(value));
#else
    int integer = (int)value;
    const double fraction = value - (double)integer;
    const int mode = fegetround();
    if (mode == FE_UPWARD)
        return integer + (fraction > 0.0);
    if (mode == FE_DOWNWARD)
        return integer - (fraction < 0.0);
    if (mode == FE_TOWARDZERO)
        return integer;
    if (fraction > 0.5 || (fraction == 0.5 && integer % 2 != 0))
        ++integer;
    else if (fraction < -0.5 || (fraction == -0.5 && integer % 2 != 0))
        --integer;
    return integer;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: classify the finite binary64 exponent as
 * nonintegral, odd, or even without a narrowing integer conversion. */
static int coduomp_pow_parity(uint64_t magnitude)
{
    const int exponent = (int)(magnitude >> POW_FRACTION_BITS) - POW_EXPONENT_BIAS;
    if (magnitude == 0 || exponent > POW_FRACTION_BITS)
        return POW_EVEN;
    if (exponent < 0)
        return POW_NONINTEGRAL;
    const uint64_t unit = UINT64_C(1) << (POW_FRACTION_BITS - exponent);
    if ((magnitude & (unit - 1)) != 0)
        return POW_NONINTEGRAL;
    return ((magnitude | POW_ONE) & unit) != 0 ? POW_ODD : POW_EVEN;
}

/* NOT_FROM_ORIGINAL_SOURCE: default CRT domain return. The game installs no
 * matherr replacement. Full OS exception callbacks are outside this adapter. */
static double coduomp_pow_domain(uint64_t bits)
{
    errno = EDOM;
    return coduomp_pow_value(bits | POW_QUIET);
}

/* NOT_FROM_ORIGINAL_SOURCE: default CRT overflow return. Underflow deliberately
 * leaves errno alone; both original power routes have that behavior. */
static double coduomp_pow_overflow(uint64_t sign)
{
    errno = ERANGE;
    return coduomp_pow_value(POW_EXPONENT | sign);
}

/* NOT_FROM_ORIGINAL_SOURCE: the three reduction stages operate on the
 * normalized mantissa. Each table index rounds a selected group of fraction
 * bits upward at its midpoint; the final entry folds into the next exponent.
 * High/low logarithm lanes remain separate until exponent multiplication. */
static CODUOMP_POW_SSE2 double coduomp_pow_binary64(double base, double exponent)
{
    const uint64_t baseBits = coduomp_pow_bits(base);
    const uint64_t exponentBits = coduomp_pow_bits(exponent);
    const uint64_t baseMagnitude = baseBits & POW_MAGNITUDE;
    const uint64_t exponentMagnitude = exponentBits & POW_MAGNITUDE;
    const int parity = coduomp_pow_parity(exponentMagnitude);
    const uint64_t sign = (baseBits & POW_SIGN) && parity == POW_ODD ? POW_SIGN : 0;

    if (exponentMagnitude == 0) {
        if (baseMagnitude > POW_EXPONENT)
            return coduomp_pow_domain(baseBits);
        return 1.0;
    }
    if (baseMagnitude > POW_EXPONENT)
        return coduomp_pow_domain(baseBits);
    if (exponentMagnitude > POW_EXPONENT)
        return coduomp_pow_domain(exponentBits);
    if (exponentMagnitude == POW_EXPONENT) {
        if (baseMagnitude == POW_ONE)
            return coduomp_pow_domain(POW_INDEFINITE);
        return (baseMagnitude < POW_ONE) == ((exponentBits & POW_SIGN) != 0)
            ? coduomp_pow_value(POW_EXPONENT) : 0.0;
    }
    if (baseMagnitude == POW_EXPONENT)
        return coduomp_pow_value(((exponentBits & POW_SIGN) ? 0 : POW_EXPONENT) | sign);
    if (baseMagnitude == 0) {
        if (exponentBits & POW_SIGN) {
            /* The discarded division raises the native divide-by-zero flag. */
            volatile double divideByZero = coduomp_pow_div(1.0, base);
            (void)divideByZero;
            return coduomp_pow_overflow(sign);
        }
        return coduomp_pow_value(sign);
    }
    if ((baseBits & POW_SIGN) && parity == POW_NONINTEGRAL) {
        volatile double invalid = coduomp_pow_mul(coduomp_pow_value(POW_SIGNALING_NAN), coduomp_pow_value(POW_SIGNALING_NAN));
        (void)invalid;
        return coduomp_pow_domain(POW_INDEFINITE);
    }

    double magnitude = coduomp_pow_value(baseMagnitude);
    int exponentAdjustment = 0;
    if ((baseMagnitude & POW_EXPONENT) == 0) {
        magnitude = coduomp_pow_mul(magnitude, POW_SUBNORMAL_NORMALIZER);
        exponentAdjustment = -64;
    }
    const uint64_t normalizedBits = coduomp_pow_bits(magnitude);
    const unsigned fractionIndex = (unsigned)((normalizedBits >> 44) & 255);
    const unsigned first = (fractionIndex + 1) >> 1;
    const int binaryExponent = (int)(normalizedBits >> POW_FRACTION_BITS) - POW_EXPONENT_BIAS +
        (fractionIndex >= 127) + exponentAdjustment;
    const double mantissa = coduomp_pow_value((normalizedBits & POW_FRACTION) | POW_ONE);
    double reciprocal = coduomp_pow_reciprocal1[first];
    double reduced = coduomp_pow_mul(mantissa, reciprocal);
    double logHi = coduomp_pow_log1[first].hi;
    double logLo = coduomp_pow_log1[first].lo;

    const unsigned second = (unsigned)((((coduomp_pow_bits(reduced) >> 38) & 255) + 1) >> 1);
    reduced = coduomp_pow_mul(reduced, coduomp_pow_reciprocal2[second]);
    reciprocal = coduomp_pow_mul(reciprocal, coduomp_pow_reciprocal2[second]);
    logHi = coduomp_pow_add(logHi, coduomp_pow_log2[second].hi);
    logLo = coduomp_pow_add(logLo, coduomp_pow_log2[second].lo);
    logHi = coduomp_pow_add(logHi, (double)binaryExponent);
    const double mantissaHi = coduomp_pow_value(coduomp_pow_bits(mantissa) & POW_SPLIT);
    const double mantissaLo = coduomp_pow_sub(mantissa, mantissaHi);

    const unsigned third = (unsigned)((((coduomp_pow_bits(reduced) >> 31) & 511) + 1) >> 1);
    reciprocal = coduomp_pow_mul(reciprocal, coduomp_pow_reciprocal3[third]);
    reduced = coduomp_pow_mul(reduced, coduomp_pow_reciprocal3[third]);
    logHi = coduomp_pow_add(logHi, coduomp_pow_log3[third].hi);
    logLo = coduomp_pow_add(logLo, coduomp_pow_log3[third].lo);
    const double reciprocalHi = coduomp_pow_value(coduomp_pow_bits(reciprocal) & POW_SPLIT);
    const double reciprocalLo = coduomp_pow_sub(reciprocal, reciprocalHi);
    double residual = coduomp_pow_add(POW_LOG_RESIDUAL_BIAS, reduced);
    double error = coduomp_pow_sub(reduced, coduomp_pow_mul(reciprocalHi, mantissaHi));
    error = coduomp_pow_sub(error, coduomp_pow_mul(mantissaHi, reciprocalLo));
    error = coduomp_pow_sub(error, coduomp_pow_mul(reciprocalHi, mantissaLo));
    error = coduomp_pow_sub(error, coduomp_pow_mul(mantissaLo, reciprocalLo));
    const double sum = coduomp_pow_add(logHi, residual);
    double low = coduomp_pow_add(coduomp_pow_sub(logHi, sum), residual);
    residual = coduomp_pow_sub(residual, error);
    logHi = coduomp_pow_sub(sum, error);

    int productExponent = (int)((exponentMagnitude >> 48) & 0x7ff0) - 0x3ff0 +
        (int)((coduomp_pow_bits(sum) >> 48) & 0x7ff0);
    double correctionBase = sum;
    if (productExponent < POW_PRODUCT_LOW || productExponent > POW_PRODUCT_HIGH) {
        correctionBase = coduomp_pow_mul(exponent, logHi);
        productExponent = (int)((coduomp_pow_bits(correctionBase) >> 48) & 0x7ff0);
        if (productExponent < POW_PRODUCT_LOW)
            return coduomp_pow_value(POW_ONE | sign);
        if (productExponent > POW_PRODUCT_HIGH) {
            if ((baseMagnitude < POW_ONE) != ((exponentBits & POW_SIGN) != 0)) {
                const double tiny = coduomp_pow_mul(POW_MIN_NORMAL, POW_MIN_NORMAL);
                return coduomp_pow_value(coduomp_pow_bits(tiny) | sign);
            }
            return coduomp_pow_overflow(sign);
        }
    }

    /* The fallback product probe replaces the correction base when it returns
     * to this path. Preserve that dependency at the range boundary as well. */
    error = coduomp_pow_sub(error, coduomp_pow_sub(correctionBase, logHi));
    low = coduomp_pow_sub(low, error);
    const double exponentHi = coduomp_pow_value(exponentBits & POW_SPLIT);
    const double exponentLo = coduomp_pow_sub(exponent, exponentHi);
    const double logarithmHi = coduomp_pow_value(coduomp_pow_bits(logHi) & POW_SPLIT);
    const double logarithmLo = coduomp_pow_sub(logHi, logarithmHi);
    const double scaled = coduomp_pow_mul(coduomp_pow_mul(exponentHi, logarithmHi), POW_INDEX_SCALE);
    double cross = coduomp_pow_add(coduomp_pow_mul(exponentHi, logarithmLo), coduomp_pow_mul(logarithmHi, exponentLo));
    cross = coduomp_pow_add(cross, coduomp_pow_mul(logarithmLo, exponentLo));
    const int index = coduomp_pow_integer(scaled);
    const int normal = index >= POW_NORMAL_LOW && index <= POW_NORMAL_HIGH;
    if (index >= POW_EXTREME_HIGH)
        return coduomp_pow_overflow(sign);
    if (index <= POW_EXTREME_LOW) {
        const double tiny = coduomp_pow_mul(POW_MIN_NORMAL, POW_MIN_NORMAL);
        return coduomp_pow_value(coduomp_pow_bits(tiny) | sign);
    }

    const unsigned tableIndex = (unsigned)index & (POW_EXP_TABLE_COUNT - 1);
    const int integralIndex = index - (int)tableIndex;
    int firstScale;
    uint64_t secondScaleBits;
    if (normal) {
        firstScale = integralIndex + POW_SCALE_BIAS;
        secondScaleBits = 0;
    } else if (index > 0) {
        firstScale = integralIndex - POW_EXP_TABLE_COUNT;
        secondScaleBits = POW_MAX_NORMAL_SCALE_BITS | sign;
    } else {
        firstScale = integralIndex + 2045 * POW_EXP_TABLE_COUNT;
        secondScaleBits = POW_MIN_NORMAL_BITS | sign;
    }

    double polynomial = coduomp_pow_add(logLo, low);
    const double rounded = coduomp_pow_sub(coduomp_pow_add(scaled, POW_INDEX_ROUND_BIAS), POW_INDEX_ROUND_BIAS);
    const double residualSquared = coduomp_pow_mul(residual, residual);
    const double logPolynomialHi = coduomp_pow_add(coduomp_pow_mul(POW_LOG_CUBIC_HI, residual),
        coduomp_pow_mul(POW_LOG_QUARTIC_HI, residualSquared));
    const double logPolynomialLo = coduomp_pow_add(coduomp_pow_mul(POW_LOG_LINEAR_LO, residual),
        coduomp_pow_mul(POW_LOG_QUADRATIC_LO, residualSquared));
    polynomial = coduomp_pow_add(coduomp_pow_add(coduomp_pow_mul(logPolynomialHi, residualSquared), logPolynomialLo), polynomial);
    polynomial = coduomp_pow_mul(polynomial, exponent);
    const double remainder = coduomp_pow_add(coduomp_pow_mul(coduomp_pow_sub(scaled, rounded), POW_INDEX_RECIPROCAL), coduomp_pow_add(cross, polynomial));
    const double scale = coduomp_pow_value(((uint64_t)(unsigned)firstScale << 45) | (normal ? sign : 0));
    double tableHi = coduomp_pow_mul(coduomp_pow_exp2[tableIndex].hi, scale);
    const double tableLo = coduomp_pow_mul(coduomp_pow_exp2[tableIndex].lo, scale);
    const double remainderSquared = coduomp_pow_mul(remainder, remainder);
    const double expPolynomialHi = coduomp_pow_mul(coduomp_pow_add(POW_EXP_QUARTIC,
        coduomp_pow_mul(POW_EXP_QUINTIC, remainder)), remainderSquared);
    const double expPolynomialLo = coduomp_pow_mul(coduomp_pow_add(POW_EXP_QUADRATIC,
        coduomp_pow_mul(POW_EXP_CUBIC, remainder)), remainderSquared);
    double result = coduomp_pow_mul(coduomp_pow_mul(remainderSquared, tableHi), expPolynomialHi);
    result = coduomp_pow_add(result, tableLo);
    result = coduomp_pow_add(result, coduomp_pow_mul(expPolynomialLo, tableHi));
    result = coduomp_pow_add(result, coduomp_pow_mul(coduomp_pow_mul(POW_LOG_TWO, remainder), tableHi));
    if (normal)
        return coduomp_pow_add(result, tableHi);

    /* Near the representable limits, split before the second scaling to avoid
     * prematurely rounding away the part that becomes a subnormal result. */
    const int scaleExponent = (firstScale - POW_SCALE_BIAS) / POW_EXP_TABLE_COUNT;
    int shift = 2 - scaleExponent;
    shift += (int)((unsigned)shift & 32u);
    const uint64_t mask = (unsigned)shift < 64 ? UINT64_MAX << shift : 0;
    const double head = coduomp_pow_value(coduomp_pow_bits(tableHi) & mask);
    tableHi = coduomp_pow_sub(tableHi, head);
    const double roundedHead = coduomp_pow_value(coduomp_pow_bits(coduomp_pow_add(head, result)) & mask);
    result = coduomp_pow_add(result, coduomp_pow_sub(head, roundedHead));
    result = coduomp_pow_add(result, tableHi);
    const double secondScale = coduomp_pow_value(secondScaleBits);
    if (scaleExponent > 0)
        result = coduomp_pow_mul(coduomp_pow_add(result, roundedHead), secondScale);
    else
        result = coduomp_pow_add(coduomp_pow_mul(result, secondScale), coduomp_pow_mul(roundedHead, secondScale));
    result = coduomp_pow_add(result, coduomp_pow_mul(index > 0 ? 1.0 : 0.0, result));
    if ((coduomp_pow_bits(result) & POW_EXPONENT) == POW_EXPONENT)
        errno = ERANGE;
    return result;
}

#if CODUO_X87_MATH_AVAILABLE

/* NOT_FROM_ORIGINAL_SOURCE: preserve the original CPU exclusion. Supported
 * x86 operating systems provide SSE state management whenever SSE2 is present;
 * the per-call exception checks remain separate from CPU availability. */
static int coduomp_pow_cpu_sse2(void)
{
    unsigned eax, edx;
    unsigned vendorB, vendorC, vendorD;
#if defined(_MSC_VER)
    int registers[4];
    __cpuid(registers, 0);
    if (registers[0] < POW_CPU_FEATURES_LEAF)
        return 0;
    vendorB = (unsigned)registers[1];
    vendorC = (unsigned)registers[2];
    vendorD = (unsigned)registers[3];
    __cpuid(registers, POW_CPU_FEATURES_LEAF);
    eax = (unsigned)registers[0];
    edx = (unsigned)registers[3];
#else
    unsigned ebx, ecx;
    if (__get_cpuid_max(0, 0) < POW_CPU_FEATURES_LEAF)
        return 0;
    __cpuid(0, eax, vendorB, vendorC, vendorD);
    __cpuid(POW_CPU_FEATURES_LEAF, eax, ebx, ecx, edx);
#endif
    if ((edx & (1u << POW_CPU_SSE2_BIT)) == 0)
        return 0;
    unsigned family = (eax >> 8) & 15;
    if (family == POW_CPU_EXTENDED_FAMILY)
        family += (eax >> 20) & 255;
    return !(vendorB == POW_AMD_VENDOR_B && vendorD == POW_AMD_VENDOR_D && vendorC == POW_AMD_VENDOR_C && family == POW_CPU_EXTENDED_FAMILY);
}

/* NOT_FROM_ORIGINAL_SOURCE: query MXCSR only after CPU availability is known. */
static CODUOMP_POW_SSE2 int coduomp_pow_sse_masks(void)
{
    return (_mm_getcsr() & POW_MXCSR_MASKS) == POW_MXCSR_MASKS;
}

/* NOT_FROM_ORIGINAL_SOURCE: the CRT scales by 2^1536, stores binary64, then
 * shifts the significand into the subnormal range without rounding it up.
 * This integer result conversion is independent of the native x87 core. */
static double coduomp_pow_x87_underflow(WIDE_FLOAT result)
{
    const WIDE_FLOAT scaled = coduomp_x87_scale(result, POW_X87_RANGE_SCALE);
    const volatile double stored = (double)scaled;
    const uint64_t bits = coduomp_pow_bits(stored);
    const int exponent = (int)((bits & POW_EXPONENT) >> POW_FRACTION_BITS) - 1536;
    const unsigned shift = (unsigned)(1 - exponent);
    const uint64_t significand = (bits & POW_FRACTION) | (UINT64_C(1) << POW_FRACTION_BITS);
    return coduomp_pow_value((bits & POW_SIGN) | (shift <= POW_FRACTION_BITS ? significand >> shift : 0));
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the native FYL2X/F2XM1/FSCALE core and
 * its live result. The binary64 result probe detects range errors without
 * replacing an ordinary live x87 result. No software x87 backend is used. */
static WIDE_FLOAT coduomp_pow_x87(double base, WIDE_FLOAT exponent)
{
    const unsigned short savedControl = coduomp_x87_control();
    const volatile double storedExponent = (double)exponent;
    const uint64_t baseBits = coduomp_pow_bits(base);
    const uint64_t baseMagnitude = baseBits & POW_MAGNITUDE;
    const uint64_t exponentBits = coduomp_pow_bits(storedExponent);
    const uint64_t exponentMagnitude = exponentBits & POW_MAGNITUDE;
    WIDE_FLOAT result;
    int negative = 0;

    coduomp_x87_set_control((unsigned short)((savedControl & POW_X87_PRECISION) | POW_X87_MASKS));
    if (baseMagnitude > POW_EXPONENT || exponentMagnitude > POW_EXPONENT) {
        /* FADDP selects the native x87 NaN, which differs from SSE2 when both
         * operands are NaNs. Materialize the domain return as binary64. */
        const volatile double combined = exponent + (WIDE_FLOAT)base;
        errno = EDOM;
        result = combined;
    } else if (exponentMagnitude == POW_EXPONENT) {
        result = baseMagnitude == POW_ONE ? coduomp_pow_domain(POW_INDEFINITE) :
            ((baseMagnitude < POW_ONE) == ((exponentBits & POW_SIGN) != 0)
                ? coduomp_pow_value(POW_EXPONENT) : 0.0);
    } else if (baseMagnitude == POW_EXPONENT) {
        const uint64_t sign = (baseBits & POW_SIGN) && coduomp_pow_parity(exponentMagnitude) == POW_ODD ? POW_SIGN : 0;
        result = exponentMagnitude == 0 ? 1.0 :
            coduomp_pow_value(((exponentBits & POW_SIGN) ? 0 : POW_EXPONENT) | sign);
    } else if (baseMagnitude == 0) {
        const WIDE_FLOAT half = exponent * 0.5L;
        const int odd = coduomp_x87_round(exponent) == exponent && coduomp_x87_round(half) != half;
        const uint64_t sign = (baseBits & POW_SIGN) && odd ? POW_SIGN : 0;
        result = exponentMagnitude == 0 ? 1.0 :
            ((exponentBits & POW_SIGN) ? coduomp_pow_overflow(sign) : coduomp_pow_value(sign));
    } else {
        if (baseBits & POW_SIGN) {
            if (coduomp_x87_round(exponent) != exponent) {
                result = coduomp_pow_domain(POW_INDEFINITE);
                coduomp_x87_set_control(savedControl);
                return result;
            }
            const WIDE_FLOAT half = exponent * 0.5L;
            negative = coduomp_x87_round(half) != half;
            base = -base;
        }
        const WIDE_FLOAT logarithm = coduomp_x87_log2_product(exponent, base);
        const WIDE_FLOAT integer = coduomp_x87_round(logarithm);
        const WIDE_FLOAT fraction = logarithm - integer;
        result = coduomp_x87_scale(coduomp_x87_exp2m1(fraction) + 1.0L, integer);
        if (negative)
            result = -result;
        const volatile double probe = (double)result;
        const uint64_t resultMagnitude = coduomp_pow_bits(probe) & POW_MAGNITUDE;
        if (resultMagnitude >= POW_EXPONENT) {
            result = coduomp_pow_overflow(negative ? POW_SIGN : 0);
        } else if ((resultMagnitude & POW_EXPONENT) == 0) {
            result = coduomp_pow_x87_underflow(result);
        }
    }
    coduomp_x87_set_control(savedControl);
    return result;
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: the register-argument entry stores the base on
 * both routes. SSE2 also stores the exponent and returns binary64; x87 keeps
 * the exponent and ordinary return live. Non-x87 hosts deliberately select
 * the portable binary64 table algorithm. Their surrounding wide arithmetic
 * retains native precision/range limits, as does MSVC's binary64 C carrier.
 * Unmasked OS exception dispatch and custom matherr callbacks are not cloned. */
WIDE_FLOAT coduomp_math_powl(WIDE_FLOAT base, WIDE_FLOAT exponent)
{
    const volatile double storedBase = (double)base;
#if CODUO_X87_MATH_AVAILABLE
    if (!coduomp_pow_cpu_sse2() || !coduomp_pow_sse_masks() ||
        (coduomp_x87_control() & POW_X87_MASKS) != POW_X87_MASKS)
        return coduomp_pow_x87(storedBase, exponent);
#endif
    const volatile double storedExponent = (double)exponent;
    return coduomp_pow_binary64(storedBase, storedExponent);
}
