#ifndef GAME_BG_TRAJECTORY_BINDING_H
#define GAME_BG_TRAJECTORY_BINDING_H

#include "server/game/game_functions.h"

/* The Windows game module uses its game-error boundary, while the Linux game
 * module calls the imported Com_Error slot with ERR_DROP. */
#define BG_TRAJECTORY_ERROR(format, value) G_Error((format), (value))

#endif
