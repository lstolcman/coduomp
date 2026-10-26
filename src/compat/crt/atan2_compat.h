#ifndef CODUO_ATAN2_COMPAT_H
#define CODUO_ATAN2_COMPAT_H

#include "compat/coduo_x87_eval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NOT_FROM_ORIGINAL_SOURCE: native replacement for the CRT atan2 entry whose
 * operands and result remain live in x87. This is distinct from an inline
 * FPATAN instruction. Non-x87 targets retain their native math implementation. */
WIDE_FLOAT coduomp_crt_atan2l(WIDE_FLOAT y, WIDE_FLOAT x);

#ifdef __cplusplus
}
#endif

#endif
