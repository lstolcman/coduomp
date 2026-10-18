#ifndef CODUOMP_RENDERER_GL_API_H
#define CODUOMP_RENDERER_GL_API_H

#include <stddef.h>
#include <stdint.h>

#include "../q_shared.h"
#include "gl_dispatch_types.h"

/* SDK declarations for the legacy OpenGL entry points used by recovered
 * renderer source. The platform loader owns the actual dispatch. */

#ifdef __cplusplus
extern "C" {
#endif

#define QGL_GL_ENTRY(type_, name_) extern type_ qgl##name_;
#include "qgl_gl_entries.h"
#undef QGL_GL_ENTRY

void QGL_Shutdown(void);
qboolean QGL_Init(const char *dllName);
void QGL_EnableErrorChecking(qboolean enable);
void QGL_EnableLogging(int32_t logLevel);
void QGL_EnableDrawProfiling(qboolean enable);

const char *GL_BooleanToString(uint8_t value);
const char *GL_DepthFuncToString(uint32_t depthFunc);
const char *GL_PrimitiveModeToString(uint32_t primitiveMode);
const char *GL_EnumToString(uint32_t value);
const char *GL_TypeToString(uint32_t type);
const char *GL_MemoryPointerToString(const void *pointerOrOffset);
void GL_BlendFactorToString(uint32_t factor, char *buffer);

#ifdef __cplusplus
}
#endif

#endif
