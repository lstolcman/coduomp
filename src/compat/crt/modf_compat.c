#include "modf_compat.h"
#include "math_dispatch.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#define MODF_SIGN UINT64_C(0x8000000000000000)
#define MODF_EXPONENT UINT64_C(0x7ff0000000000000)
#define MODF_FRACTION UINT64_C(0x000fffffffffffff)
#define MODF_QUIET UINT64_C(0x0008000000000000)

enum {
    MODF_FRACTION_BITS = 52,
    MODF_EXPONENT_BIAS = 1023,
    MODF_X87_CONTROL = 0x1f3f,
    MODF_MXCSR_DENORMALS_ZERO = 0x0040
};

/* NOT_FROM_ORIGINAL_SOURCE: alias-safe binary64 representation boundary. */
static uint64_t coduomp_modf_bits(double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* NOT_FROM_ORIGINAL_SOURCE: materialize the original signed zero/NaN payload. */
static double coduomp_modf_value(uint64_t bits)
{
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: shared default nonfinite result. The original
 * cores store the whole part before reporting a NaN domain error. Full OS
 * exception dispatch and custom matherr callbacks are not implemented. */
static double coduomp_modf_nonfinite(uint64_t bits, double *whole)
{
    if ((bits & MODF_FRACTION) != 0) {
        const double quieted = coduomp_modf_value(bits | MODF_QUIET);
        *whole = quieted;
        errno = EDOM;
        return quieted;
    }
    *whole = coduomp_modf_value(bits);
    return coduomp_modf_value(bits & MODF_SIGN);
}

/* NOT_FROM_ORIGINAL_SOURCE: binary64 significand split from the SSE2 core.
 * Its subtraction uses MXCSR on Intel, including downward rounding's negative
 * zero for a positive integral input below 2^52. Smaller inputs are returned
 * directly, so subnormals are not flushed through an arithmetic operation. */
static CODUOMP_CRT_SSE2 double coduomp_modf_binary64(double value, double *whole)
{
    const uint64_t bits = coduomp_modf_bits(value);
    const unsigned exponent = (unsigned)((bits & MODF_EXPONENT) >> MODF_FRACTION_BITS);
    if (exponent < MODF_EXPONENT_BIAS) {
        *whole = coduomp_modf_value(bits & MODF_SIGN);
        return value;
    }
    if (exponent < MODF_EXPONENT_BIAS + MODF_FRACTION_BITS) {
        const unsigned shift = MODF_EXPONENT_BIAS + MODF_FRACTION_BITS - exponent;
        const uint64_t discarded = (UINT64_C(1) << shift) - 1;
        const double integral = coduomp_modf_value(bits & ~discarded);
        *whole = integral;
#if CODUOMP_CRT_INTEL
        const double fraction = _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(value), _mm_set_sd(integral)));
#else
        const volatile double fraction = value - integral;
#endif
        return coduomp_modf_value(coduomp_modf_bits(fraction) | (bits & MODF_SIGN));
    }
    if ((bits & MODF_EXPONENT) == MODF_EXPONENT) {
#if CODUOMP_CRT_INTEL
        /* The original compares the masked NaN significand as a binary64
         * subnormal. DAZ makes that comparison equal to zero, selecting the
         * infinity-style fraction without reporting a domain error. */
        if ((bits & MODF_FRACTION) != 0 && (_mm_getcsr() & MODF_MXCSR_DENORMALS_ZERO) != 0) {
            *whole = coduomp_modf_value(bits | MODF_QUIET);
            return coduomp_modf_value(bits & MODF_SIGN);
        }
#endif
        return coduomp_modf_nonfinite(bits, whole);
    }
    *whole = value;
    return coduomp_modf_value(bits & MODF_SIGN);
}

#if CODUO_X87_MATH_AVAILABLE
/* NOT_FROM_ORIGINAL_SOURCE: the fallback selects masked PC64/chop, stores
 * FRNDINT to binary64, subtracts and stores the fraction, repairs its zero
 * sign, and restores the caller's control word. Native x87 supplies FRNDINT;
 * neither its arithmetic nor its environment is emulated in software. */
static double coduomp_modf_x87(double value, double *whole)
{
    const unsigned short savedControl = coduomp_x87_control();
    const uint64_t bits = coduomp_modf_bits(value);
    double result;
    coduomp_x87_set_control(MODF_X87_CONTROL);
    if ((bits & MODF_EXPONENT) == MODF_EXPONENT) {
        result = coduomp_modf_nonfinite(bits, whole);
    } else {
        const volatile double integral = (double)coduomp_x87_round(value);
        *whole = integral;
        const volatile double fraction = (long double)value - (long double)integral;
        result = coduomp_modf_value(coduomp_modf_bits(fraction) | (bits & MODF_SIGN));
    }
    coduomp_x87_set_control(savedControl);
    return result;
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: shared CPU/exception-mask dispatch with power.
 * Non-x87 hosts use the portable binary64 core. Result bytes, errno and the
 * caller's control word are preserved; complete sticky-status parity is not
 * provided, including the nonfinite paths' original instruction side effects. */
double coduomp_crt_modf(double value, double *whole)
{
#if CODUO_X87_MATH_AVAILABLE
    if (!coduomp_crt_use_sse2_math())
        return coduomp_modf_x87(value, whole);
#endif
    return coduomp_modf_binary64(value, whole);
}
