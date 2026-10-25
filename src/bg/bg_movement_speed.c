#include "bg_movement.h"

#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"


#include <stdint.h>


enum {
    BG_LADDER_JUMP_SPEED_DELAY = 500
};

/*
 * The Windows cgame/game bodies are instruction-identical apart from the
 * relocated zero constant: uo_cgame_mp_x86.dll 0x3000e7d0 and
 * uo_game_mp_x86.dll 0x2000e580.  Both receive playerState_t directly and
 * return the live PC=53 FSQRT result in ST0.
 *
 * Linux game RVA 0x0002e2c7 has the same branches and player-state offsets,
 * but passes a binary64 squared-speed sum through glibc sqrt and stores the
 * public result as binary32.  The complete bodies retain that boundary.
 */
long double BG_GetSpeed(const playerState_t *ps, int32_t time)
{
    if ((ps->playerStateFlags & PMF_LADDER) != 0) {
        const int32_t jumpAge = coduo_int32_from_bits(
            (uint32_t)time - (uint32_t)ps->lastJumpCommandTime);
        if (jumpAge < BG_LADDER_JUMP_SPEED_DELAY) {
            return 0.0L;
        }
        return (long double)ps->velocity[2];
    }

    return coduo_x87_sqrtl(
        (long double)ps->velocity[0] * (long double)ps->velocity[0] +
        (long double)ps->velocity[1] * (long double)ps->velocity[1]);
}
