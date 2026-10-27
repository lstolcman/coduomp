#ifndef QCOMMON_COM_PARSE_ERROR_BINDING_H
#define QCOMMON_COM_PARSE_ERROR_BINDING_H

/* Shared qcommon dependency binding. The Windows game parser also calls
 * Com_Error/Com_Printf; its compiler removes the unused error-code argument. */
#define COM_PARSE_ERROR(level, ...) Com_Error((level), __VA_ARGS__)
#define COM_PARSE_PRINT(...) Com_Printf(__VA_ARGS__)

#endif
