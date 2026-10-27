#ifndef QCOMMON_INFO_PRIVATE_H
#define QCOMMON_INFO_PRIVATE_H

#include <stdint.h>

#include "com_sprintf.h"
#include "q_string.h"

/* Original qcommon dependencies of the shared info-string subsystem. */
void Com_Error(int32_t level, const char *format, ...);
void Com_Printf(const char *format, ...);

/* Shared error routing through the original Com_Error interface. */
#include "info_error_binding.h"

#endif
