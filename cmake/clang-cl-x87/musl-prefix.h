/* NOT_FROM_ORIGINAL_SOURCE: namespace the pinned math implementation after
 * loading the unmodified SDK declarations. No extended value crosses its CRT. */
#ifndef CODUOMP_MUSL_PREFIX_H
#define CODUOMP_MUSL_PREFIX_H

#include <math.h>
#include "compat/coduo_extended_math.h"

#define hidden
#define acosl coduomp_musl_acosl
#define asinl coduomp_musl_asinl
#define cosl coduomp_musl_cosl
#define sinl coduomp_musl_sinl
#define expl coduomp_musl_expl
#define logl coduomp_musl_logl
#define powl coduomp_musl_powl
#define fabsl coduomp_musl_fabsl
#define floorl coduomp_musl_floorl
#define frexpl coduomp_musl_frexpl
#define scalbnl coduomp_musl_scalbnl
#define sqrtl coduomp_math_sqrtl
#define __polevll coduomp_musl_polevll
#define __p1evll coduomp_musl_p1evll
#define __invtrigl_R coduomp_musl_invtrigl_R
#define __sinl coduomp_musl_sin_kernel
#define __cosl coduomp_musl_cos_kernel
#define __rem_pio2l coduomp_musl_rem_pio2l
#define __rem_pio2_large coduomp_musl_rem_pio2_large
#define __pio2_hi coduomp_musl_pio2_hi
#define __pio2_lo coduomp_musl_pio2_lo

#undef isnan
#undef isinf
#undef isfinite
#undef signbit
#define isnan(value) __builtin_isnan(value)
#define isinf(value) __builtin_isinf(value)
#define isfinite(value) __builtin_isfinite(value)
#define signbit(value) __builtin_signbit(value)

long double acosl(long double);
long double asinl(long double);
long double cosl(long double);
long double sinl(long double);
long double expl(long double);
long double logl(long double);
long double powl(long double, long double);
long double fabsl(long double);
long double floorl(long double);
long double frexpl(long double, int *);
long double scalbnl(long double, int);

#endif
