#include "bg_pmove.h"
#include "math/q_math.h"

#include <stdint.h>


/* The unoptimized Linux module loads this exported constant through the GOT.
 * Both optimized Windows modules propagate the same binary32 value directly
 * into the PM_Accelerate call. */
const float pm_airaccelerate = 1.0f;

/*
 * The authoritative Windows bodies are instruction-identical apart from
 * relocated globals and callees:
 *
 *   uo_cgame_mp_x86.dll  0x30009060..0x300091dd
 *   uo_game_mp_x86.dll   0x20008e10..0x20008f8d
 *
 * Linux game retains the same movement decisions at
 * 0x000249cc..0x00024b7b. Its unoptimized body stores the normalized wish
 * length before scaling and calls its independently shared PM_ClipVelocity;
 * Windows keeps those values live and inlines the target-specific clip graph.
 */
void PM_AirMove(void)
{
    PM_Friction();

    pmove_t *const move = pm;
    const float forwardMove = (float)move->command.forwardmove;
    const float rightMove = (float)move->command.rightmove;
    usercmd_t command = move->command;
    const float commandScale = PM_CmdScale(&command);

    pml.forward[2] = 0.0f;
    pml.right[2] = 0.0f;
    (void)VectorNormalize(pml.forward);
    (void)VectorNormalize(pml.right);

    vec3_t wishDirection;
    wishDirection[0] = (float)(
        (long double)pml.right[0] * (long double)rightMove +
        (long double)pml.forward[0] * (long double)forwardMove);
    wishDirection[1] = (float)(
        (long double)pml.right[1] * (long double)rightMove +
        (long double)pml.forward[1] * (long double)forwardMove);
    wishDirection[2] = 0.0f;

    const float normalizedLength = VectorNormalize(wishDirection);
    const float wishSpeed = (float)(
        (long double)normalizedLength * (long double)commandScale);
    PM_Accelerate(wishDirection, wishSpeed, pm_airaccelerate);

    if (pml.groundPlane != 0) {
        playerState_t *const ps = move->ps;

        long double backoff =
            ((long double)pml.groundTrace.normal[1] *
                 (long double)ps->velocity[1] +
             (long double)pml.groundTrace.normal[2] *
                 (long double)ps->velocity[2]) +
            (long double)pml.groundTrace.normal[0] *
                (long double)ps->velocity[0];
        const float overbounce = backoff < 0.0L ? 1.001f : 0.99900097f;
        backoff *= (long double)overbounce;

        ps->velocity[0] = (float)(
            (long double)ps->velocity[0] -
            (long double)pml.groundTrace.normal[0] * backoff);
        ps->velocity[1] = (float)(
            (long double)ps->velocity[1] -
            (long double)pml.groundTrace.normal[1] * backoff);
        ps->velocity[2] = (float)(
            (long double)ps->velocity[2] -
            (long double)pml.groundTrace.normal[2] * backoff);
    }

    PM_StepSlideMove(1);
    PM_SetMovementDir();
}
