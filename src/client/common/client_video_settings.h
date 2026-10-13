#ifndef CODUOMP_CLIENT_VIDEO_SETTINGS_H
#define CODUOMP_CLIENT_VIDEO_SETTINGS_H

/* NOT_FROM_ORIGINAL_SOURCE: archived r_fullscreen values shared by the
 * graphics menu and native window backends. Keep existing selections stable. */
typedef enum coduomp_window_mode_e {
    CODUOMP_WINDOW_MODE_WINDOWED = 0,
    CODUOMP_WINDOW_MODE_FULLSCREEN = 1,
    CODUOMP_WINDOW_MODE_BORDERLESS = 2,
    CODUOMP_WINDOW_MODE_EXCLUSIVE = 3
} coduomp_window_mode_t;

#endif
