/* NOT_FROM_ORIGINAL_SOURCE: clang-cl's native-x87 math boundary. The build
 * places this directory before the SDK only for native-x87 product targets;
 * third-party libraries retain their normal Microsoft CRT ABI. */
#ifndef CODUO_CLANGCL_MATH_H
#define CODUO_CLANGCL_MATH_H

#include <corecrt_math.h>
#include "compat/coduo_extended_math.h"

#define acosl coduomp_math_acosl
#define asinl coduomp_math_asinl
#define cosl coduomp_math_cosl
#define sinl coduomp_math_sinl
#define expl coduomp_math_expl
#define logl coduomp_math_logl
#define powl coduomp_math_powl
#define fabsl coduomp_math_fabsl
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
