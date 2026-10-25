/* NOT_FROM_ORIGINAL_SOURCE: extended-precision math ABI for clang-cl x64.
 * Microsoft CRT long-double entries use binary64. These private entry points
 * keep the project's 80-bit carriers inside code built with the same ABI. */
#ifndef CODUO_EXTENDED_MATH_H
#define CODUO_EXTENDED_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

long double coduomp_math_acosl(long double value);
long double coduomp_math_asinl(long double value);
long double coduomp_math_cosl(long double value);
long double coduomp_math_sinl(long double value);
long double coduomp_math_expl(long double value);
long double coduomp_math_logl(long double value);
long double coduomp_math_powl(long double base, long double exponent);
long double coduomp_math_fabsl(long double value);
long double coduomp_math_sqrtl(long double value);

#ifdef __cplusplus
}
#endif

#endif
