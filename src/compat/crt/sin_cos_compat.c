#include "sin_cos_compat.h"

#include "compat/coduo_x87_math_ops.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

enum coduomp_crt_trig_operation {
    CODUOMP_CRT_SINE,
    CODUOMP_CRT_COSINE
};

#define CODUOMP_BINARY64_EXPONENT_MASK UINT64_C(0x7ff0000000000000)
#define CODUOMP_BINARY64_FRACTION_MASK UINT64_C(0x000fffffffffffff)
#define CODUOMP_BINARY64_QUIET_NAN_BIT UINT64_C(0x0008000000000000)
#define CODUOMP_BINARY64_NEGATIVE_INDEFINITE UINT64_C(0xfff8000000000000)

#if CODUO_X87_MATH_AVAILABLE && (defined(__GNUC__) || defined(__clang__))

enum {
    CODUOMP_TRIG_CONTROL_WORD = 0x027f,
    CODUOMP_X87_PRECISION_BIT = 0x0020,
    CODUOMP_X87_RANGE_BIT = 0x0400
};

/* NOT_FROM_ORIGINAL_SOURCE: native CRT finite core. The extended reduction
 * divisor is pi * 2^62, not a rounded two-pi constant. FPREM1 brings the input
 * inside the hardware trigonometric instruction's accepted range. */
static WIDE_FLOAT coduomp_crt_trig_finite(WIDE_FLOAT angle, enum coduomp_crt_trig_operation operation, unsigned short *status)
{
    static const long double reductionDivisor = 0xc.90fdaa22168c235p+60L;
    unsigned short resultStatus;

    __asm__ __volatile__(
        "test %[operation], %[operation]\n\t"
        "jnz 1f\n\tfsin\n\tjmp 2f\n\t"
        "1: fcos\n\t"
        "2: fnstsw %%ax\n\ttest $%c[rangeBit], %%ax\n\tjz 5f\n\t"
        "fldt %[divisor]\n\tfxch %%st(1)\n\t"
        "3: fprem1\n\tfnstsw %%ax\n\ttest $%c[rangeBit], %%ax\n\tjnz 3b\n\t"
        "fstp %%st(1)\n\ttest %[operation], %[operation]\n\t"
        "jnz 4f\n\tfsin\n\tjmp 5f\n\t"
        "4: fcos\n\t"
        "5: fnstsw %%ax"
        : "+t"(angle), "=&a"(resultStatus)
        : [operation] "r"(operation), [rangeBit] "i"(CODUOMP_X87_RANGE_BIT), [divisor] "m"(reductionDivisor)
        : "st(1)", "cc");
    *status = resultStatus;
    return angle;
}

#endif

/* NOT_FROM_ORIGINAL_SOURCE: shared CRT boundary for float-derived operands.
 * The binary64 argument probe classifies the input without replacing its live
 * value. NaNs retain their payload through the binary64 error return; either
 * infinity returns negative indefinite. Both report EDOM. The original game
 * installs no matherr handler; OS exception callbacks are not reproduced.
 * Arbitrary extended inputs outside binary64 range are outside this interface's
 * recovered caller domain. Non-x87 and MSVC builds retain native libm results;
 * MSVC has no extended C carrier for the hardware path. */
static WIDE_FLOAT coduomp_crt_trig(WIDE_FLOAT angle, enum coduomp_crt_trig_operation operation)
{
    const volatile double argumentProbe = (double)angle;
    const double storedArgument = argumentProbe;
    uint64_t argumentBits;
    memcpy(&argumentBits, &storedArgument, sizeof(argumentBits));

    if ((argumentBits & CODUOMP_BINARY64_EXPONENT_MASK) == CODUOMP_BINARY64_EXPONENT_MASK) {
        const uint64_t fraction = argumentBits & CODUOMP_BINARY64_FRACTION_MASK;
        const uint64_t resultBits = fraction != 0
            ? argumentBits | CODUOMP_BINARY64_QUIET_NAN_BIT
            : CODUOMP_BINARY64_NEGATIVE_INDEFINITE;
        double result;
        memcpy(&result, &resultBits, sizeof(result));
        errno = EDOM;
        return result;
    }

#if CODUO_X87_MATH_AVAILABLE && (defined(__GNUC__) || defined(__clang__))
    const unsigned short savedControl = coduomp_x87_control();
    unsigned short status;
    if (savedControl != CODUOMP_TRIG_CONTROL_WORD) {
        coduomp_x87_set_control(CODUOMP_TRIG_CONTROL_WORD);
    }
    WIDE_FLOAT result = coduomp_crt_trig_finite(angle, operation, &status);

    /* The precision-status dispatch materializes a binary64 result only when
     * the caller used a different control word with precision masked. */
    if (savedControl != CODUOMP_TRIG_CONTROL_WORD &&
        (savedControl & CODUOMP_X87_PRECISION_BIT) != 0 &&
        (status & CODUOMP_X87_PRECISION_BIT) != 0) {
        const volatile double roundedResult = (double)result;
        result = roundedResult;
    }
    coduomp_x87_set_control(savedControl);
    return result;
#else
    return operation == CODUOMP_CRT_COSINE ? cosl(angle) : sinl(angle);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: CRT sine entry. */
WIDE_FLOAT coduomp_crt_sinl(WIDE_FLOAT angle)
{
    return coduomp_crt_trig(angle, CODUOMP_CRT_SINE);
}

/* NOT_FROM_ORIGINAL_SOURCE: CRT cosine entry. */
WIDE_FLOAT coduomp_crt_cosl(WIDE_FLOAT angle)
{
    return coduomp_crt_trig(angle, CODUOMP_CRT_COSINE);
}
