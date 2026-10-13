#ifndef CODUOMP_PLATFORM_DISPLAY_H
#define CODUOMP_PLATFORM_DISPLAY_H

#include "../q_shared.h"

#if defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: release and reacquire the exclusive display mode
 * when the native game window loses or gains focus. */
void coduomp_glw_exclusive_focus_changed_compat(qboolean active);
#endif

#endif
