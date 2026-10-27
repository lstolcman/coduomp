#ifndef CODUO_SIN_COS_COMPAT_H
#define CODUO_SIN_COS_COMPAT_H

#include "compat/coduo_x87_eval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NOT_FROM_ORIGINAL_SOURCE: CRT trigonometric entries with live operands and
 * results. Unlike bare FSIN/FCOS, these include range and domain handling. */
WIDE_FLOAT coduomp_crt_sinl(WIDE_FLOAT angle);
WIDE_FLOAT coduomp_crt_cosl(WIDE_FLOAT angle);

#ifdef __cplusplus
}
#endif

#endif
