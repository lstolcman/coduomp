// Sources: uo_cgame_mp_x86.dll 0x300098c0..0x30009bb8,
//          uo_game_mp_x86.dll  0x20009670..0x20009968,
//          game.mp.uo.i386.so  0x000256ed..0x00025b9a

#include "bg_pmove.h"

#include "compat/coduo_native_x87.h"
#include "math/q_math.h"


#include <stdint.h>



/*
 * PM_UFOMove — pmType UO_PM_TYPE_UFO's collision-free flight step. The exact
 * spelling comes from the same-module PPC name bank; PmoveSingle dispatches this
 * address for pmType 3, matching the isolated server pmType_t bank.
 *
 * The Windows cgame and game bodies have the same instruction stream apart
 * from relocated globals and callees. Linux retains the same decisions but
 * stores the speed and every friction/wish/integration stage as binary32.
 */
void PM_UFOMove(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    usercmd_t *cmd = &move->command;
    float speed;

    ps->viewHeightTarget = bg_viewheight_standing.integer;
    move->ps->standViewHeight = bg_viewheight_standing.integer;

    /*
     * The Windows compiler combines both button contributions into one byte
     * expression. The uint8_t operations preserve the original modulo-256
     * IMUL/ADD behavior without signed-overflow assumptions.
     */
    {
        const uint8_t wbuttons = cmd->wbuttons;
        const uint8_t buttonDelta =
            (uint8_t)((wbuttons & PM_WBUTTON_LEAN_LEFT) -
                      (wbuttons & PM_WBUTTON_LEAN_RIGHT));
        const uint8_t buttonProduct = (uint8_t)(buttonDelta * 127u);

        cmd->upmove =
            (int8_t)((uint8_t)cmd->upmove + buttonProduct);
    }

    if (cmd->forwardmove == 0 && cmd->rightmove == 0) {
        if (cmd->upmove == 0) {
            speed = 0.0f;
        } else {
            move->ps->velocity[1] = 0.0f;
            move->ps->velocity[0] = 0.0f;
            move->ps->velocity[2] = (float)cmd->upmove;
            speed = 127.0f;
        }
    } else {
        /*
         * _CIsqrt consumes the live X/Y/Z x87 sum and the caller then stores
         * its result as binary32. There is no binary64 argument boundary.
         */
        long double speedSquared =
            (long double)move->ps->velocity[0] * move->ps->velocity[0];
        speedSquared +=
            (long double)move->ps->velocity[1] * move->ps->velocity[1];
        speedSquared +=
            (long double)move->ps->velocity[2] * move->ps->velocity[2];
        speed = (float)coduo_x87_sqrtl(speedSquared);
    }

    if (speed < 1.0f) {
        move->ps->velocity[0] = 0.0f;
        move->ps->velocity[1] = 0.0f;
        move->ps->velocity[2] = 0.0f;
    } else {
        const float control = speed < 100.0f ? 100.0f : speed;
        float drop;
        float newSpeed;
        float frictionScale;

        /*
         * Drop is stored as binary32 before the subtraction; newSpeed and its
         * quotient are also stored before the next stage consumes them.
         */
        drop = (float)(
            (long double)pml.frametime * (long double)control * 8.25L +
            0.0L);
        newSpeed =
            (float)((long double)speed - (long double)drop);
        if (newSpeed < 0.0f) {
            newSpeed = 0.0f;
        }
        frictionScale = (float)((long double)newSpeed /
                                (long double)speed);
        for (int32_t lane = 0; lane < 3; ++lane) {
            move->ps->velocity[lane] = (float)(
                (long double)frictionScale *
                (long double)move->ps->velocity[lane]);
        }
    }

    {
        const float commandScale = PM_CmdScale(cmd);
        const float forwardMove = (float)cmd->forwardmove;
        const float rightMove = (float)cmd->rightmove;
        const float upMove = (float)cmd->upmove;
        vec3_t forwardBasis;
        vec3_t wishdir;
        float wishSpeed;

        /*
         * The optimized Windows bodies inline CrossProduct({0,0,1},
         * pml.right): each multiply/subtract result is stored as binary32.
         * Retain those operations because signed zero and NaN behavior are
         * observable even though the ordinary finite result is {-right.y,
         * right.x, 0}.
         */
        const float zeroFromRightZ =
            (float)((long double)pml.right[2] * 0.0L);
        forwardBasis[0] = (float)(
            (long double)zeroFromRightZ -
            (long double)pml.right[1]);
        forwardBasis[1] = (float)(
            (long double)pml.right[0] -
            (long double)zeroFromRightZ);
        forwardBasis[2] = (float)(
            (long double)pml.right[1] * 0.0L -
            (long double)pml.right[0] * 0.0L);

        for (int32_t lane = 0; lane < 3; ++lane) {
            wishdir[lane] = (float)(
                (long double)pml.right[lane] * (long double)rightMove +
                (long double)forwardBasis[lane] *
                    (long double)forwardMove);
        }

        /*
         * The right/forward Z partial is already binary32 before the doubled
         * up-move is added.
         */
        wishdir[2] = (float)(
            ((long double)upMove + (long double)upMove) +
            (long double)wishdir[2]);

        wishSpeed = VectorNormalize(wishdir);
        wishSpeed = (float)((long double)wishSpeed *
                            (long double)commandScale);
        PM_Accelerate(wishdir, wishSpeed, 9.0f);
    }

    for (int32_t lane = 0; lane < 3; ++lane) {
        move->ps->psOrigin[lane] = (float)(
            (long double)pml.frametime *
                (long double)move->ps->velocity[lane] +
            (long double)move->ps->psOrigin[lane]);
    }
}
