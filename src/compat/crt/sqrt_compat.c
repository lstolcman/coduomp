/* Shared reconstruction of the Windows CRT square-root boundary.
 *
 * The register-argument entries are game RVA 0x6a1b0, cgame RVA 0x6bee0,
 * UI RVA 0x2d300 and engine RVA 0x1820b0. Their binary64 argument store is
 * a non-popping classification probe: it does not replace the live operand.
 * Finite inputs select (savedControl & 0x0300) | 0x007f, retaining precision
 * while selecting nearest rounding and masking exceptions, then execute FSQRT.
 *
 * The negative branch distinguishes a stored -0 from a nonzero negative.
 * Negative zero bypasses FSQRT; a negative magnitude returns the indefinite
 * QNaN. Positive infinity also bypasses FSQRT. NaNs retain their payload
 * through the binary64 error return. Negative magnitudes and NaNs report EDOM.
 *
 * Normal returns remain live at control word 0x027f. Other control words use
 * a binary64 return store when precision is masked and its sticky status bit
 * is set, including a precision flag inherited from the caller. The saved
 * control word is restored after that store. No software x87 is used.
 */
#include "compat/crt/sqrt_compat.h"
#include "compat/coduo_x87_math_ops.h"

#include <errno.h>
#include <fenv.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define SQRT_SIGN UINT64_C(0x8000000000000000)
#define SQRT_EXPONENT UINT64_C(0x7ff0000000000000)
#define SQRT_FRACTION UINT64_C(0x000fffffffffffff)
#define SQRT_QUIET UINT64_C(0x0008000000000000)
#define SQRT_INDEFINITE UINT64_C(0xfff8000000000000)

enum {
    SQRT_DEFAULT_CONTROL = 0x027f,
    SQRT_PRECISION_CONTROL = 0x0300,
    SQRT_NEAREST_MASKED = 0x007f,
    SQRT_PRECISION_STATUS = 0x0020
};

/* NOT_FROM_ORIGINAL_SOURCE: alias-safe materialization of the CRT's binary64
 * error return, including the negative indefinite loaded on a domain error. */
static double coduomp_sqrt_value(uint64_t bits)
{
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

#if CODUO_X87_MATH_AVAILABLE
/* NOT_FROM_ORIGINAL_SOURCE: read the sticky precision bit without altering it. */
static unsigned short coduomp_sqrt_status(void)
{
    unsigned short status;
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("fnstsw %0" : "=a"(status) : : "memory");
#else
    __asm fnstsw status
#endif
    return status;
}

/* NOT_FROM_ORIGINAL_SOURCE: the masked domain-error dispatch raises invalid.
 * Native arithmetic supplies the status effect without installing OS handlers. */
static void coduomp_sqrt_invalid(void)
{
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("fldz; fldz; fdivp %%st, %%st(1); fstp %%st(0)" : : : "st", "st(1)");
#else
    __asm {
        fldz
        fldz
        fdivp st(1), st(0)
        fstp st(0)
    }
#endif
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: common default CRT behavior for recovered callers.
 * The original images install no custom matherr handler. OS exception callbacks
 * for unmasked exceptions are outside this masked game contract. Native hosts
 * without an extended carrier cannot retain x87's extra range or precision;
 * their finite operation uses native sqrtl at nearest rounding. */
WIDE_FLOAT coduomp_crt_sqrtl(WIDE_FLOAT value)
{
    uint64_t bits;
    /* Classify the stored bytes without reloading a binary64 subnormal into
     * x87: that extra FLD would raise a denormal-operand flag the CRT avoids.
     * A C narrowing conversion can emit that reload even at -O0. */
#if CODUO_X87_MATH_AVAILABLE && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("fstl %0" : "=m"(bits) : "t"(value) : "memory");
#else
    const volatile double argumentProbe = value;
    memcpy(&bits, (const void *)&argumentProbe, sizeof(bits));
#endif
    const int nonfinite = (bits & SQRT_EXPONENT) == SQRT_EXPONENT;
    const int nan = nonfinite && (bits & SQRT_FRACTION) != 0;
    const int negative = (bits & SQRT_SIGN) != 0 && (bits & ~SQRT_SIGN) != 0;
    WIDE_FLOAT result = value;

#if CODUO_X87_MATH_AVAILABLE
    const unsigned short savedControl = coduomp_x87_control();
    if (!nonfinite && savedControl != SQRT_DEFAULT_CONTROL)
        coduomp_x87_set_control((savedControl & SQRT_PRECISION_CONTROL) | SQRT_NEAREST_MASKED);

    if (nan) {
        result = coduomp_sqrt_value(bits | SQRT_QUIET);
        errno = EDOM;
    } else if (negative) {
        result = coduomp_sqrt_value(SQRT_INDEFINITE);
        coduomp_sqrt_invalid();
        errno = EDOM;
    } else if (!nonfinite && (bits & SQRT_SIGN) == 0) {
        result = coduomp_x87_sqrt(value);
    }

    /* The precision path stores under the temporary mode before restoring the
     * caller's rounding mode; the ordinary 0x027f return has no double store. */
    if (savedControl != SQRT_DEFAULT_CONTROL &&
        (savedControl & SQRT_PRECISION_STATUS) != 0 &&
        (coduomp_sqrt_status() & SQRT_PRECISION_STATUS) != 0) {
        const volatile double roundedResult = result;
        result = roundedResult;
    }
    coduomp_x87_set_control(savedControl);
#else
    if (nan) {
        if ((bits & SQRT_QUIET) == 0)
            feraiseexcept(FE_INVALID);
        result = coduomp_sqrt_value(bits | SQRT_QUIET);
        errno = EDOM;
    } else if (negative) {
        feraiseexcept(FE_INVALID);
        result = coduomp_sqrt_value(SQRT_INDEFINITE);
        errno = EDOM;
    } else if (!nonfinite && (bits & SQRT_SIGN) == 0) {
        fenv_t savedEnvironment;
        feholdexcept(&savedEnvironment);
        fesetround(FE_TONEAREST);
        /* An indirect call keeps the operation inside the temporary mode even
         * when the compiler otherwise treats sqrt as a pure builtin. */
        WIDE_FLOAT (*volatile nativeSqrt)(WIDE_FLOAT) = sqrtl;
        result = nativeSqrt(value);
        feupdateenv(&savedEnvironment);
    }
#endif
    return result;
}
