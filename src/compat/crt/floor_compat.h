#ifndef CODUO_CRT_FLOOR_COMPAT_H
#define CODUO_CRT_FLOOR_COMPAT_H

#include <errno.h>
#include <stdint.h>
#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: preserve binary64 floor results and the Windows
 * CRT's default NaN-domain result. Both original arithmetic paths preserve
 * signed zero and infinity; NaNs retain their sign/payload, are quieted, and
 * report EDOM. Integer significand rounding avoids host-libm differences.
 * The recovered callers use masked exceptions and do not inspect FP status.
 * This adapter does not reproduce alternate exception handlers, denormal
 * flushing modes, or the x87 fallback's inexact-status side effect. */
static inline double coduomp_crt_floor(double value)
{
    enum { FRACTION_BITS = 52, EXPONENT_BIAS = 1023 };
    const uint64_t signMask = UINT64_C(0x8000000000000000);
    const uint64_t exponentMask = UINT64_C(0x7ff0000000000000);
    const uint64_t fractionMask = UINT64_C(0x000fffffffffffff);
    const uint64_t quietMask = UINT64_C(0x0008000000000000);
    uint64_t bits;
    unsigned int exponent;

    memcpy(&bits, &value, sizeof(bits));
    exponent = (unsigned int)((bits & exponentMask) >> FRACTION_BITS);
    if ((bits & exponentMask) == exponentMask) {
        if ((bits & fractionMask) != 0) {
            if ((bits & quietMask) == 0) {
                volatile double quieted = value + value;
                (void)quieted;
            }
            bits |= quietMask;
            errno = EDOM;
        }
    } else if (exponent < EXPONENT_BIAS) {
        if ((bits & ~signMask) != 0) {
            return (bits & signMask) != 0 ? -1.0 : 0.0;
        }
    } else if (exponent < EXPONENT_BIAS + FRACTION_BITS) {
        const unsigned int shift = EXPONENT_BIAS + FRACTION_BITS - exponent;
        const uint64_t unit = UINT64_C(1) << shift;
        const uint64_t discarded = unit - UINT64_C(1);

        if ((bits & discarded) != 0) {
            if ((bits & signMask) != 0) {
                bits += unit;
            }
            bits &= ~discarded;
        }
    }
    memcpy(&value, &bits, sizeof(value));
    return value;
}

#endif
