#ifndef CODUO_CRT_SCAN_COMPAT_H
#define CODUO_CRT_SCAN_COMPAT_H

#include <stdio.h>
#include "compat/crt/msvc_compat.h"

/* NOT_FROM_ORIGINAL_SOURCE: select the original numeric scanner for the
 * audited scalar and vector formats, independently of the build host. */
#define coduo_compat_scan_float coduo_crt_scan_float
#define coduo_compat_scan_vec3 coduo_crt_scan_vec3
#define coduo_compat_scan_int coduo_crt_scan_int

#ifdef __cplusplus
extern "C" {
#endif

/* NOT_FROM_ORIGINAL_SOURCE: scan a fixed sequence of unqualified %f values
 * separated by format whitespace. Successful earlier values survive failure. */
int coduo_crt_scan_floats(const char *text, float *output, int valueCount);

#ifdef __cplusplus
}
#endif

#endif
