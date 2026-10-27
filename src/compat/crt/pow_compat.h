#ifndef CODUOMP_POW_COMPAT_H
#define CODUOMP_POW_COMPAT_H

#include "compat/coduo_x87_eval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NOT_FROM_ORIGINAL_SOURCE: live-operand CRT power boundary. Processor and
 * exception-mask dispatch select the binary64 table core or native x87 core.
 * Hosts without a native x87 core use the binary64 table core. */
WIDE_FLOAT coduomp_math_powl(WIDE_FLOAT base, WIDE_FLOAT exponent);

#ifdef __cplusplus
}
#endif

#endif
