#ifndef CODUO_CRT_ATOF_COMPAT_H
#define CODUO_CRT_ATOF_COMPAT_H

#include <stdlib.h>
#include "compat/crt/msvc_compat.h"

/* NOT_FROM_ORIGINAL_SOURCE: use the Windows decimal parser independently of
 * the host platform used to build or run the recovered code. */
#define coduo_compat_atof coduo_crt_atof

#endif
