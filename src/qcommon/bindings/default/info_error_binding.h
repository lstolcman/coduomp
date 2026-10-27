#ifndef QCOMMON_INFO_ERROR_BINDING_H
#define QCOMMON_INFO_ERROR_BINDING_H

/* Shared qcommon dependency binding. The Windows game Info_* bodies also
 * call Com_Error, whose unused error-code argument is removed by the compiler. */
#define INFO_ERROR(level, ...) Com_Error((level), __VA_ARGS__)

#endif
