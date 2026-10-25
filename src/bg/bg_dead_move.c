#include "bg_pmove.h"
#include "math/q_math.h"

#include <math.h>


#define PM_DEAD_MOVE_SPEED_DROP 20.0f

/*
 * Canonical Quake3/CoD grounded dead-player velocity decay. The Windows
 * modules are instruction-identical apart from relocations:
 *
 *   uo_cgame_mp_x86.dll  0x30009660..0x300096f3
 *   uo_game_mp_x86.dll   0x20009410..0x200094a3
 *
 * Linux game has the same decisions at 0x00025246..0x0002538a. Windows keeps
 * its square-root/subtraction chain live; Linux passes a binary64 squared sum
 * to glibc sqrt and stores both the root and reduced speed as binary32.
 */
void PM_DeadMove(void)
{
    if (pml.walking == 0) {
        return;
    }

    playerState_t *ps = pm->ps;
    const long double reducedSpeed = sqrtl(
        ((long double)ps->velocity[0] * (long double)ps->velocity[0] +
         (long double)ps->velocity[1] * (long double)ps->velocity[1]) +
        (long double)ps->velocity[2] * (long double)ps->velocity[2]) -
        (long double)PM_DEAD_MOVE_SPEED_DROP;
    const float storedSpeed = (float)reducedSpeed;
    if (reducedSpeed <= 0.0L) {
        ps->velocity[2] = 0.0f;
        pm->ps->velocity[1] = 0.0f;
        pm->ps->velocity[0] = 0.0f;
        return;
    }

    (void)VectorNormalize(ps->velocity);
    pm->ps->velocity[0] = (float)(
        (long double)storedSpeed * (long double)pm->ps->velocity[0]);
    pm->ps->velocity[1] = (float)(
        (long double)storedSpeed * (long double)pm->ps->velocity[1]);
    pm->ps->velocity[2] = (float)(
        (long double)storedSpeed * (long double)pm->ps->velocity[2]);
}
