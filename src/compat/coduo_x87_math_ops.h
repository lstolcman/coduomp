/* NOT_FROM_ORIGINAL_SOURCE: native x87 instruction boundary for shared math.
 * C has no operation with the processor-specific approximation of FYL2X,
 * F2XM1, FSIN, FCOS or FPATAN. Keep those instructions here; compose their
 * arithmetic and domain handling in libm/coduo_wide_math.h.
 * MSVC x86 has only a binary64 C carrier; it still executes native x87 here,
 * but its stores cannot preserve extended range or transcendental tail bits. */
#ifndef CODUO_X87_MATH_OPS_H
#define CODUO_X87_MATH_OPS_H

#include <float.h>

#if defined(_MSC_VER)
#define CODUO_MATH_INLINE __forceinline
#else
#define CODUO_MATH_INLINE inline __attribute__((always_inline))
#endif

#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__)) && __LDBL_MANT_DIG__ == 64
#define CODUO_X87_MATH_AVAILABLE 1
#define CODUO_MATH_LDBL_MAX __LDBL_MAX__

/* NOT_FROM_ORIGINAL_SOURCE: FSIN instruction adapter. */
static CODUO_MATH_INLINE long double coduomp_x87_sin(long double value)
{
    __asm__ __volatile__("fsin" : "+t"(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: FCOS instruction adapter. */
static CODUO_MATH_INLINE long double coduomp_x87_cos(long double value)
{
    __asm__ __volatile__("fcos" : "+t"(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: FSQRT instruction adapter. */
static CODUO_MATH_INLINE long double coduomp_x87_sqrt(long double value)
{
    __asm__ __volatile__("fsqrt" : "+t"(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: FRNDINT instruction adapter. */
static CODUO_MATH_INLINE long double coduomp_x87_round(long double value)
{
    __asm__ __volatile__("frndint" : "+t"(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: F2XM1 instruction adapter. */
static CODUO_MATH_INLINE long double coduomp_x87_exp2m1(long double value)
{
    __asm__ __volatile__("f2xm1" : "+t"(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: load the hardware constant in the active rounding mode. */
static CODUO_MATH_INLINE long double coduomp_x87_ln2(void)
{
    long double result;
    __asm__ __volatile__("fldln2" : "=t"(result));
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: load the hardware constant in the active rounding mode. */
static CODUO_MATH_INLINE long double coduomp_x87_log2e(void)
{
    long double result;
    __asm__ __volatile__("fldl2e" : "=t"(result));
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: FPATAN consumes x in ST0 and y in ST1. */
static CODUO_MATH_INLINE long double coduomp_x87_atan2(long double y, long double x)
{
    long double result;
    __asm__ __volatile__("fpatan" : "=t"(result) : "0"(x), "u"(y) : "st(1)");
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: FYL2X consumes x in ST0 and y in ST1. */
static CODUO_MATH_INLINE long double coduomp_x87_log2_product(long double y, long double x)
{
    long double result;
    __asm__ __volatile__("fyl2x" : "=t"(result) : "0"(x), "u"(y) : "st(1)");
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: FSCALE accepts a floating-point integer exponent. */
static CODUO_MATH_INLINE long double coduomp_x87_scale(long double value, long double exponent)
{
    __asm__ __volatile__("fscale; fstp %%st(1)" : "+t"(value) : "u"(exponent) : "st(1)");
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: CRT control-word save and restore boundary. */
static CODUO_MATH_INLINE unsigned short coduomp_x87_control(void)
{
    unsigned short control;
    __asm__ __volatile__("fnstcw %0" : "=m"(control) : : "memory");
    return control;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the caller's floating-point environment. */
static CODUO_MATH_INLINE void coduomp_x87_set_control(unsigned short control)
{
    __asm__ __volatile__("fldcw %0" : : "m"(control) : "memory");
}

#elif defined(_MSC_VER) && defined(_M_IX86)
#define CODUO_X87_MATH_AVAILABLE 1
#define CODUO_MATH_LDBL_MAX LDBL_MAX

/* NOT_FROM_ORIGINAL_SOURCE: FSIN instruction adapter for the MSVC x86 ABI. */
static CODUO_MATH_INLINE long double coduomp_x87_sin(long double value)
{
    __asm {
        fld value
        fsin
        fstp value
    }
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: FCOS instruction adapter for the MSVC x86 ABI. */
static CODUO_MATH_INLINE long double coduomp_x87_cos(long double value)
{
    __asm {
        fld value
        fcos
        fstp value
    }
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: FSQRT instruction adapter for the MSVC x86 ABI. */
static CODUO_MATH_INLINE long double coduomp_x87_sqrt(long double value)
{
    __asm {
        fld value
        fsqrt
        fstp value
    }
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: FRNDINT instruction adapter for the MSVC x86 ABI. */
static CODUO_MATH_INLINE long double coduomp_x87_round(long double value)
{
    __asm {
        fld value
        frndint
        fstp value
    }
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: F2XM1 instruction adapter for the MSVC x86 ABI. */
static CODUO_MATH_INLINE long double coduomp_x87_exp2m1(long double value)
{
    __asm {
        fld value
        f2xm1
        fstp value
    }
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: hardware constant through the MSVC x86 C carrier. */
static CODUO_MATH_INLINE long double coduomp_x87_ln2(void)
{
    long double result;
    __asm {
        fldln2
        fstp result
    }
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: hardware constant through the MSVC x86 C carrier. */
static CODUO_MATH_INLINE long double coduomp_x87_log2e(void)
{
    long double result;
    __asm {
        fldl2e
        fstp result
    }
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: FPATAN consumes x in ST0 and y in ST1. */
static CODUO_MATH_INLINE long double coduomp_x87_atan2(long double y, long double x)
{
    long double result;
    __asm {
        fld y
        fld x
        fpatan
        fstp result
    }
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: FYL2X consumes x in ST0 and y in ST1. */
static CODUO_MATH_INLINE long double coduomp_x87_log2_product(long double y, long double x)
{
    long double result;
    __asm {
        fld y
        fld x
        fyl2x
        fstp result
    }
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: FSCALE without an integer conversion of the exponent. */
static CODUO_MATH_INLINE long double coduomp_x87_scale(long double value, long double exponent)
{
    __asm {
        fld exponent
        fld value
        fscale
        fstp st(1)
        fstp value
    }
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: CRT control-word save and restore boundary. */
static CODUO_MATH_INLINE unsigned short coduomp_x87_control(void)
{
    unsigned short control;
    __asm fnstcw control
    return control;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the caller's floating-point environment. */
static CODUO_MATH_INLINE void coduomp_x87_set_control(unsigned short control)
{
    __asm fldcw control
}

#else
#define CODUO_X87_MATH_AVAILABLE 0
#endif

#endif
