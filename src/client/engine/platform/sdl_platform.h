#ifndef CODUOMP_SDL_PLATFORM_H
#define CODUOMP_SDL_PLATFORM_H

#include "../q_shared.h"

#include <stddef.h>

/* NOT_FROM_ORIGINAL_SOURCE: the native application shell is SDL on every
 * supported host. Window creation, display modes, the OpenGL context,
 * presentation, gamma, clipboard, dialogs, and translated input all cross
 * this boundary; no Win32 window or WGL context exists in the client. */
typedef struct coduomp_sdl_display_info_s {
    int32_t logicalWidth;
    int32_t logicalHeight;
    int32_t backingWidth;
    int32_t backingHeight;
    int32_t nativeWidth;
    int32_t nativeHeight;
    int32_t refreshRate;
    qboolean builtin;
} coduomp_sdl_display_info_t;

qboolean CoduoSDL_Init(void);
void CoduoSDL_Shutdown(void);
qboolean CoduoSDL_CreateOpenGLWindow(int32_t width, int32_t height,
                                     int32_t colorBits, int32_t depthBits,
                                     int32_t stencilBits,
                                     int32_t windowMode,
                                     int32_t displayIndex,
                                     int32_t refreshRate);
void CoduoSDL_DestroyOpenGLWindow(void);
int32_t coduomp_sdl_get_display_count_compat(void);
qboolean coduomp_sdl_get_display_info_compat(
    int32_t displayIndex, coduomp_sdl_display_info_t *displayInfo);
void CoduoSDL_GetDesktopMode(int32_t displayIndex,
                             int32_t *width, int32_t *height,
                             int32_t *refreshRate, int32_t *colorBits);
qboolean coduomp_sdl_get_native_display_mode_compat(int32_t displayIndex,
                                                     int32_t *width,
                                                     int32_t *height,
                                                     int32_t *refreshRate);
qboolean coduomp_sdl_display_mode_available_compat(int32_t displayIndex,
                                                    int32_t width,
                                                    int32_t height);
void coduomp_sdl_window_size_for_drawable_compat(int32_t displayIndex,
                                                  int32_t *width,
                                                  int32_t *height);
void CoduoSDL_GetFramebufferSize(int32_t *width, int32_t *height);
void coduomp_sdl_get_window_size_compat(int32_t *width, int32_t *height);
qboolean coduomp_sdl_get_window_position_compat(int32_t *x, int32_t *y);
qboolean coduomp_sdl_get_window_gamma_ramp(uint16_t red[256],
                                           uint16_t green[256],
                                           uint16_t blue[256]);
qboolean coduomp_sdl_set_window_gamma_ramp(const uint16_t red[256],
                                           const uint16_t green[256],
                                           const uint16_t blue[256]);
#if defined(__linux__)
qboolean coduomp_sdl_get_x11_window_compat(
    void **display, unsigned long *window,
    int32_t *x, int32_t *y, int32_t *width, int32_t *height);
#endif
const char *coduomp_sdl_error_compat(void);
void CoduoSDL_GetOpenGLFormat(int32_t *colorBits, int32_t *depthBits,
                              int32_t *stencilBits);
void CoduoSDL_GetOpenGLSymbol(const char *name, void *destination,
                              size_t destinationSize);
void CoduoSDL_SwapWindow(void);
void CoduoSDL_SetSwapInterval(int32_t interval);
void CoduoSDL_PumpEvents(void);
void CoduoSDL_SetRelativeMouse(qboolean active);
void CoduoSDL_SetWindowGrab(qboolean active);
void CoduoSDL_GetWindowCenter(int32_t *x, int32_t *y);
void CoduoSDL_WarpMouseToWindowCenter(void);
qboolean CoduoSDL_GetGlobalMousePosition(int32_t *x, int32_t *y);
qboolean CoduoSDL_HasOpenGLWindow(void);
void CoduoSDL_ShowErrorDialog(const char *message, const char *title);
typedef enum coduomp_sdl_dialog_style_e {
    CODUOMP_SDL_DIALOG_OK = 0,
    CODUOMP_SDL_DIALOG_OK_CANCEL = 1,
    CODUOMP_SDL_DIALOG_YES_NO = 2,
    CODUOMP_SDL_DIALOG_YES_NO_CANCEL = 3
} coduomp_sdl_dialog_style_t;
typedef enum coduomp_sdl_dialog_result_e {
    CODUOMP_SDL_DIALOG_FIRST = 0,
    CODUOMP_SDL_DIALOG_SECOND = 1,
    CODUOMP_SDL_DIALOG_THIRD = 2,
    CODUOMP_SDL_DIALOG_FAILED = -1
} coduomp_sdl_dialog_result_t;
int32_t coduomp_sdl_show_message_box_compat(
    const char *title, const char *body, coduomp_sdl_dialog_style_t style);
char *coduomp_sdl_get_clipboard_text_compat(void);
void *coduomp_sdl_native_window_handle_compat(void);
qboolean coduomp_sdl_probe_renderer_string_compat(char *buffer,
                                                  size_t bufferSize);

#endif
