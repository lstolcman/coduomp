/* CoduoLibm_Asin / CoduoLibm_Acos — reconstruction of the MSVC CRT routines.
 *
 * Stock: uo_gamex86.dll 0x2008a700 -> 0x2008a71d (asin)
 *                       0x2008a0e0 -> 0x2008a0fd (acos)
 *
 * The two bodies are identical apart from one instruction and their error
 * codes. Shared shape:
 *
 *   push edx / fstcw [esp]            ; save control word
 *   je   <inf/NaN>                    ; ZF from the classify helper 0x2008aa88
 *   cmp  [esp],0x27f / call 0x2008aa15; CW := (saved & 0x300) | 0x7f
 *   cmp  eax,0x3ff00000 / jae <|x|>=1>
 *   fld1 / fadd st,st(1)              ; 1+x
 *   fld1 / fsub st,st(2)              ; 1-x
 *   fmulp st(1),st                    ; (1+x)*(1-x)   <-- not 1-x*x
 *   fsqrt                             ; sqrt(1-x^2)
 *   [acos only] fxch st(1)
 *   fpatan                            ; atan(st(1)/st(0))
 *
 * acos leaves x in st(0) via the FXCH, giving atan(sqrt(1-x^2)/x); asin omits
 * it, giving atan(x/sqrt(1-x^2)). The 1-x^2 factoring as (1+x)(1-x) is stock
 * and is preserved deliberately — it is not algebraically interchangeable with
 * 1-x*x in floating point.
 *
 * The control-word helper preserves the precision-control field. The Windows
 * engine initializes it to 53-bit precision; the live result still occupies
 * an x87 register until a caller stores it. Compilers with binary64 long
 * double and non-x87 targets retain their native precision limitations.
 *
 * |x| is classified from the exponent field alone, exactly as the stock code
 * does: the classify helper returns high & 0x7ff00000 for finite inputs, which
 * is compared against 0x3ff00000. Equality plus a zero mantissa means |x| == 1;
 * anything above means |x| > 1 and is a domain error.
 *
 *   |x| < 1   -> computed as above
 *   x == +1   -> asin: +pi/2 (TBYTE 0x200b018a)   acos: +0.0 (FLDZ)
 *   x == -1   -> asin: -pi/2 (FCHS of the same)   acos: pi   (FLDPI)
 *   |x| > 1   -> indefinite QNaN (TBYTE 0x200b0180), domain error
 *   NaN       -> quieted
 *
 * Native x87 builds execute FPATAN directly. Other supported
 * targets use the shared native wide-math adapters. Wide entry points retain
 * the result; the double-return APIs perform the explicit binary64 narrowing.
 */
#include "compat/libm/coduo_libm.h"
#include "compat/crt/asin_compat.h"

#include <errno.h>
#include <math.h>
#include <string.h>

#include "compat/libm/coduo_wide_math.h"

/* NOT_FROM_ORIGINAL_SOURCE: the domain return materializes binary64, keeps
 * a quieted NaN payload or uses negative indefinite, and reports EDOM. */
static WIDE_FLOAT coduomp_crt_inverse_trig_domain(double value)
{
    const uint64_t exponentMask = UINT64_C(0x7ff0000000000000);
    const uint64_t fractionMask = UINT64_C(0x000fffffffffffff);
    const uint64_t indefiniteBits = UINT64_C(0xfff8000000000000);
    uint64_t bits;
    WIDE_FLOAT result;

    memcpy(&bits, &value, sizeof(bits));
    if ((bits & exponentMask) == exponentMask && (bits & fractionMask) != 0) {
        result = (WIDE_FLOAT)value + 1.0L;
    } else {
        memcpy(&value, &indefiniteBits, sizeof(value));
        result = (WIDE_FLOAT)value;
    }
    errno = EDOM;
    return result;
}

#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__)) && LDBL_MANT_DIG == 64

#define ASIN_HIGH_SIGN_MASK     0x80000000u
#define ASIN_HIGH_EXPONENT_MASK 0x7ff00000u
#define ASIN_HIGH_MANTISSA_MASK 0x000fffffu
#define ASIN_EXPONENT_OF_ONE    0x3ff00000u

static uint32_t AsinHighDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)(bits >> 32);
}

static uint32_t AsinLowDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)bits;
}

/* TBYTE 0x200b018a: pi/2 as an 80-bit constant, significand
 * 0xC90FDAA22168C235 with exponent 0x3FFF. Built from those bytes rather than
 * from a decimal literal so the value is exact. */
static WIDE_FLOAT AsinHalfPi(void)
{
    const unsigned char raw[10] = {
        0x35, 0xc2, 0x68, 0x21, 0xa2, 0xda, 0x0f, 0xc9, 0xff, 0x3f
    };
    WIDE_FLOAT value = 0.0L;

    memcpy(&value, raw, sizeof(raw));
    return value;
}

/* Helper 0x2008aa15: preserve the precision-control field, force
 * round-to-nearest with every exception masked. */
static unsigned short AsinEnterControlWord(void)
{
    unsigned short saved;
    unsigned short entry;

    __asm__ __volatile__("fstcw %0" : "=m"(saved));
    entry = (unsigned short)((saved & 0x0300u) | 0x007fu);
    __asm__ __volatile__("fldcw %0" : : "m"(entry));
    return saved;
}

static void AsinRestoreControlWord(unsigned short saved)
{
    __asm__ __volatile__("fldcw %0" : : "m"(saved));
}

/* (1+x)*(1-x) then FSQRT, matching the stock instruction order. */
static WIDE_FLOAT AsinRoot(WIDE_FLOAT x)
{
    WIDE_FLOAT t = (1.0L + x) * (1.0L - x);

    __asm__ __volatile__("fsqrt" : "=t"(t) : "0"(t));
    return t;
}

/* NOT_FROM_ORIGINAL_SOURCE: wide carrier for the original live CRT result. */
WIDE_FLOAT coduomp_crt_asinl(double value)
{
    const uint32_t high = AsinHighDword(value);
    const uint32_t low = AsinLowDword(value);
    const uint32_t exponent = high & ASIN_HIGH_EXPONENT_MASK;
    const uint32_t mantissa = high & ASIN_HIGH_MANTISSA_MASK;
    unsigned short saved;
    WIDE_FLOAT result;

    if (exponent == ASIN_HIGH_EXPONENT_MASK) {
        if (mantissa != 0u || low != 0u) {
            return coduomp_crt_inverse_trig_domain(value); /* quiet the NaN */
        }
        return coduomp_crt_inverse_trig_domain(value); /* +-inf is a domain error */
    }

    if (exponent >= ASIN_EXPONENT_OF_ONE) {
        if (exponent != ASIN_EXPONENT_OF_ONE || mantissa != 0u || low != 0u) {
            return coduomp_crt_inverse_trig_domain(value); /* |x| > 1 */
        }
        /* |x| == 1: +-pi/2, the sign taken from x (FCHS at 0x2008a786). */
        saved = AsinEnterControlWord();
        result = AsinHalfPi();
        if ((high & ASIN_HIGH_SIGN_MASK) != 0u) {
            result = -result;
        }
        AsinRestoreControlWord(saved);
        return result;
    }

    saved = AsinEnterControlWord();
    {
        const WIDE_FLOAT x = (WIDE_FLOAT)value;
        const WIDE_FLOAT root = AsinRoot(x);

        /* No FXCH: st(0)=root, st(1)=x -> atan(x/root). */
        __asm__ __volatile__("fpatan" : "=t"(result) : "0"(root), "u"(x)
                             : "st(1)");
    }
    AsinRestoreControlWord(saved);
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: wide carrier for the original live CRT result. */
WIDE_FLOAT coduomp_crt_acosl(double value)
{
    const uint32_t high = AsinHighDword(value);
    const uint32_t low = AsinLowDword(value);
    const uint32_t exponent = high & ASIN_HIGH_EXPONENT_MASK;
    const uint32_t mantissa = high & ASIN_HIGH_MANTISSA_MASK;
    unsigned short saved;
    WIDE_FLOAT result;

    if (exponent == ASIN_HIGH_EXPONENT_MASK) {
        if (mantissa != 0u || low != 0u) {
            return coduomp_crt_inverse_trig_domain(value); /* quiet the NaN */
        }
        return coduomp_crt_inverse_trig_domain(value); /* +-inf is a domain error */
    }

    if (exponent >= ASIN_EXPONENT_OF_ONE) {
        if (exponent != ASIN_EXPONENT_OF_ONE || mantissa != 0u || low != 0u) {
            return coduomp_crt_inverse_trig_domain(value); /* |x| > 1 */
        }
        /* |x| == 1: FLDZ for +1 (0x2008a166), FLDPI for -1 (0x2008a162). */
        saved = AsinEnterControlWord();
        if ((high & ASIN_HIGH_SIGN_MASK) != 0u) {
            __asm__ __volatile__("fldpi" : "=t"(result));
        } else {
            result = 0.0L;
        }
        AsinRestoreControlWord(saved);
        return result;
    }

    saved = AsinEnterControlWord();
    {
        const WIDE_FLOAT x = (WIDE_FLOAT)value;
        const WIDE_FLOAT root = AsinRoot(x);

        /* FXCH first: st(0)=x, st(1)=root -> atan(root/x). */
        __asm__ __volatile__("fpatan" : "=t"(result) : "0"(x), "u"(root)
                             : "st(1)");
    }
    AsinRestoreControlWord(saved);
    return result;
}

#else

/* NOT_FROM_ORIGINAL_SOURCE: native arithmetic adapter for this CRT boundary. */
WIDE_FLOAT coduomp_crt_asinl(double value)
{
    if (!(value >= -1.0 && value <= 1.0)) {
        return coduomp_crt_inverse_trig_domain(value);
    }
    return coduomp_math_asinl((WIDE_FLOAT)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: native arithmetic adapter for this CRT boundary. */
WIDE_FLOAT coduomp_crt_acosl(double value)
{
    if (!(value >= -1.0 && value <= 1.0)) {
        return coduomp_crt_inverse_trig_domain(value);
    }
    return coduomp_math_acosl((WIDE_FLOAT)value);
}

#endif

/* NOT_FROM_ORIGINAL_SOURCE: explicit binary64 return boundary. */
double CoduoLibm_Asin(double value)
{
    return (double)coduomp_crt_asinl(value);
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit binary64 return boundary. */
double CoduoLibm_Acos(double value)
{
    return (double)coduomp_crt_acosl(value);
}
