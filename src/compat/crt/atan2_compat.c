#include "atan2_compat.h"

#include "compat/coduo_x87_math_ops.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#if CODUO_X87_MATH_AVAILABLE

enum {
    CODUOMP_ATAN2_CONTROL_WORD = 0x133f,
    CODUOMP_X87_CLASS_MASK = 0x4500,
    CODUOMP_X87_SIGN = 0x0200,
    CODUOMP_X87_NAN = 0x0100,
    CODUOMP_X87_ZERO = 0x4000,
    CODUOMP_X87_INFINITY = 0x0500
};

enum coduomp_atan2_axis {
    CODUOMP_ATAN2_HORIZONTAL,
    CODUOMP_ATAN2_VERTICAL
};

/* NOT_FROM_ORIGINAL_SOURCE: inspect the x87 operand without narrowing it or
 * replacing sign-bit classification with a comparison against zero. */
static unsigned short coduomp_crt_atan2_classify(WIDE_FLOAT value)
{
    unsigned short status;
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("fxam\n\tfnstsw %%ax" : "=a"(status) : "t"(value));
#else
    __asm {
        fld value
        fxam
        fnstsw ax
        mov status, ax
        fstp st(0)
    }
#endif
    return status;
}

/* NOT_FROM_ORIGINAL_SOURCE: the CRT computes the positive-quadrant FPATAN,
 * then subtracts from the hardware pi constant for negative X, and finally
 * applies Y's sign. Keep the subtraction inside the temporary PC=64 mode.
 * MSVC's binary64 result carrier remains its native precision limitation. */
static WIDE_FLOAT coduomp_crt_atan2_quadrant(WIDE_FLOAT y, WIDE_FLOAT x, unsigned int negativeX, unsigned int negativeY)
{
    WIDE_FLOAT result;
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__(
        "fabs\n\tfxch %%st(1)\n\tfabs\n\tfxch %%st(1)\n\tfpatan\n\t"
        "test %[negativeX], %[negativeX]\n\tjz 1f\n\t"
        "fldpi\n\tfsubp %%st, %%st(1)\n\t"
        "1: test %[negativeY], %[negativeY]\n\tjz 2f\n\tfchs\n\t2:"
        : "=t"(result)
        : "0"(x), "u"(y), [negativeX] "r"(negativeX), [negativeY] "r"(negativeY)
        : "st(1)", "cc");
#else
    __asm {
        fld y
        fabs
        fld x
        fabs
        fpatan
        cmp negativeX, 0
        je positive_x
        fldpi
        fsubrp st(1), st(0)
    positive_x:
        cmp negativeY, 0
        je positive_y
        fchs
    positive_y:
        fstp result
    }
#endif
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: signed axis result, including the sign of zero.
 * The exact extended pi/2 constant is the CRT's stored axis constant. */
static WIDE_FLOAT coduomp_crt_atan2_axis(unsigned int negativeX, unsigned int negativeY, enum coduomp_atan2_axis axis)
{
    WIDE_FLOAT result;
    if (axis == CODUOMP_ATAN2_VERTICAL) {
        result = 0xc.90fdaa22168c235p-3L;
    } else if (negativeX) {
#if defined(__GNUC__) || defined(__clang__)
        __asm__ __volatile__("fldpi" : "=t"(result));
#else
        __asm {
            fldpi
            fstp result
        }
#endif
    } else {
        result = 0.0L;
    }
    return negativeY ? -result : result;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the CRT's native atan2 value graph for
 * recovered callers. It selects PC=64/nearest with exceptions masked, handles
 * signed axes separately, and returns the live result after restoring the
 * caller's control word. The error path materializes a binary64 result and
 * reports EDOM for NaNs or two infinite operands. Recovered callers form the
 * operands from binary32 values; the CRT's binary64-underflow dispatch is
 * outside that finite input range. There is no game-owned matherr callback;
 * operating-system exception dispatch is not reproduced. */
WIDE_FLOAT coduomp_crt_atan2l(WIDE_FLOAT y, WIDE_FLOAT x)
{
    const unsigned short saved = coduomp_x87_control();
    WIDE_FLOAT result;
    coduomp_x87_set_control(CODUOMP_ATAN2_CONTROL_WORD);

    const unsigned short xStatus = coduomp_crt_atan2_classify(x);
    const unsigned short yStatus = coduomp_crt_atan2_classify(y);
    const unsigned int xClass = xStatus & CODUOMP_X87_CLASS_MASK;
    const unsigned int yClass = yStatus & CODUOMP_X87_CLASS_MASK;
    const unsigned int negativeX = xStatus & CODUOMP_X87_SIGN;
    const unsigned int negativeY = yStatus & CODUOMP_X87_SIGN;

    if (xClass == CODUOMP_X87_NAN || yClass == CODUOMP_X87_NAN) {
        const volatile double rounded = (double)(y + x);
        result = rounded;
        errno = EDOM;
    } else if (xClass == CODUOMP_X87_INFINITY && yClass == CODUOMP_X87_INFINITY) {
        const uint64_t indefiniteBits = UINT64_C(0xfff8000000000000);
        double indefinite;
        memcpy(&indefinite, &indefiniteBits, sizeof(indefinite));
        result = indefinite;
        errno = EDOM;
    } else if (yClass == CODUOMP_X87_ZERO || xClass == CODUOMP_X87_INFINITY) {
        result = coduomp_crt_atan2_axis(negativeX, negativeY, CODUOMP_ATAN2_HORIZONTAL);
    } else if (xClass == CODUOMP_X87_ZERO || yClass == CODUOMP_X87_INFINITY) {
        result = coduomp_crt_atan2_axis(negativeX, negativeY, CODUOMP_ATAN2_VERTICAL);
    } else {
        result = coduomp_crt_atan2_quadrant(y, x, negativeX, negativeY);
    }
    /* The CRT probes a binary64 store for exceptions without replacing ST0.
     * Keep this separate from the live result and inside its temporary mode. */
    const volatile double exceptionProbe = (double)result;
    (void)exceptionProbe;
    coduomp_x87_set_control(saved);
    return result;
}

#else

/* NOT_FROM_ORIGINAL_SOURCE: non-x87 builds keep the native atan2 boundary. */
WIDE_FLOAT coduomp_crt_atan2l(WIDE_FLOAT y, WIDE_FLOAT x)
{
    return atan2l(y, x);
}

#endif
