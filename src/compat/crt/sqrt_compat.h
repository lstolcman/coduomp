#ifndef CODUO_SQRT_COMPAT_H
#define CODUO_SQRT_COMPAT_H

#include "compat/coduo_x87_eval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NOT_FROM_ORIGINAL_SOURCE: original CRT square-root boundary, distinct from
 * an inline FSQRT. Preserve live operands/results and caller-owned stores. */
WIDE_FLOAT coduomp_crt_sqrtl(WIDE_FLOAT value);

#ifdef __cplusplus
}
#endif

#endif
