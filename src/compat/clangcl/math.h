/* NOT_FROM_ORIGINAL_SOURCE: SDK ABI bridge for clang-cl's extended carrier.
 * Product targets include this before the SDK. Algorithms live in the shared
 * math layer; SDK declarations and third-party libraries keep their normal
 * Microsoft ABI. */
#ifndef CODUO_CLANGCL_MATH_H
#define CODUO_CLANGCL_MATH_H

#include <corecrt_math.h>
#include "compat/libm/coduo_wide_math.h"

#define acosl coduomp_math_acosl
#define asinl coduomp_math_asinl
#define cosl coduomp_math_cosl
#define sinl coduomp_math_sinl
#define expl coduomp_math_expl
#define logl coduomp_math_logl
#define powl coduomp_math_powl
#define fabsl __builtin_fabsl
#define sqrtl coduomp_math_sqrtl

#ifndef __cplusplus
#undef isnan
#undef isinf
#undef isfinite
#undef signbit
#define isnan(value) __builtin_isnan(value)
#define isinf(value) __builtin_isinf(value)
#define isfinite(value) __builtin_isfinite(value)
#define signbit(value) __builtin_signbit(value)
#endif

#endif
