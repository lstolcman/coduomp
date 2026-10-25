// Sources: uo_cgame_mp_x86.dll 0x30009700..0x300098b0,
//          uo_game_mp_x86.dll  0x200094b0..0x20009660,
//          game.mp.uo.i386.so  0x0002538b..0x000256ec
//
// PM_NoclipMove — the pmove no-clip movement step: a free-fly move that ignores
// world collision entirely (the origin is integrated directly, with no trace or
// step-slide). It forces the standing view height, applies inline friction, builds
// a full-3D wish-velocity from the command forward/right/up movement bytes rotated
// through the pmove basis vectors, accelerates the player-state velocity toward it,
// and advances the origin by velocity*frametime.
//
// Name adjudication: the .mcode header's "PM_VerifyPronePosition" is a SIZE-ONLY
// guess (win 0x1b1 == some game_mp function) and is REJECTED per the naming rules
// (pmove-prone verification is a trace-heavy server routine; this body never calls
// a trace and never clips). The identity is proven from behaviour and the call
// graph, all from the machine code:
//   * Dispatched by PmoveSingle (0x3000e050) at 0x3000e464 as one pmType-specific
//     movement handler, the sibling of the ground mover at 0x300098c0.
//   * It is the near-twin of the already-reconstructed PM_FlyMove
//     (0x30008f20, functions/FUN_30008f20_30009051.c): the same friction ->
//     PM_CmdScale -> wishvel(pml.forward/pml.right + upmove) -> VectorNormalize ->
//     PM_Accelerate acceleration path. It differs from the spectator move in exactly
//     the ways that distinguish Quake3/CoD PM_NoclipMove from PM_FlyMove:
//       - it forces ps->viewheight to the standing height (bg_viewheight_standing.integer)
//         at entry (no crouch/prone stance in noclip);
//       - the command scale is applied to wishspeed AFTER VectorNormalize
//         (wishspeed *= scale), not to the wishvel before it;
//       - the acceleration constant is 9.0f (0x41100000), not the spectator 8.0f;
//       - there are no vertical free-fly button impulses;
//       - the move ENDS by integrating the origin directly, origin += velocity *
//         pml.frametime (VectorMA), with NO PM_StepSlideMove / trace / collision.
//     That "friction, wishvel with a direct vertical, accelerate, then advance the
//     origin with no collision" shape is the canonical PM_NoclipMove.
//
// Globals / constants (all proven from the bytes):
//   * pm (0x30539850): the pmove context; ps = pm->ps.
//   * bg_viewheight_standing.integer (cgame 0x30539aac; game 0x203581cc):
//     standing view-height int, written to
//     ps->viewHeightTarget (+0xf4) and ps->standViewHeight
//     (+0x57c) at entry.
//   * pml.forward (0x30539580), pml.right (0x3053958c): pmove-locals basis vectors.
//   * pml.frametime (0x305395a4): pmove substep time (seconds).
//   * 0x300715ec = 100.0f  (friction stop-speed floor for the control speed).
//   * 0x3007bf7c = 8.25f   (friction coefficient).
//   * 0x3007bce0 = 1.0f    (min-speed cutoff: below it the velocity is zeroed).
//   * 0x3007bcec = 0.0f    (friction newspeed clamp; the 9.0f accel arg = 0x41100000).
//
// ABI: void(void). The lone PUSH ESI/PUSH EDI are register preservation; EDI caches
// pm, ESI the wishdir stack vec3. Callee (PM_Accelerate) stack args are
// cleaned by this frame (ADD ESP,8); expressed here as plain C.

#include "bg_pmove.h"

#include "compat/coduo_native_x87.h"
#include "math/q_math.h"


#include <stdint.h>



void PM_NoclipMove(void)
{
    int32_t targetViewHeight = bg_viewheight_standing.integer;
    pmove_t *move = pm;
    playerState_t *ps = move->ps;

    /* 0x30009700..0x30009724: force the standing view height.
     * bg_viewheight_standing.integer is stored into ps->viewHeightTarget
     * (+0xf4) and ps->standViewHeight (+0x57c). Noclip has no crouch/prone
     * stance. */
    ps->viewHeightTarget = targetViewHeight;
    int32_t standingViewHeight = bg_viewheight_standing.integer;
    playerState_t *standingPs = move->ps;
    standingPs->standViewHeight = standingViewHeight;

    /* 0x30009726..0x300097bf: inline friction (classic Quake3 PM_Friction, folded
     * into this mover rather than the shared PM_Friction 0x30008470).
     *   speed = |velocity| (full 3-D). */
    /* speed is never stored to a float slot: the FSQRT result (0x30009741) is kept
     * in st0 through the FCOM 1.0f, the control-select, the FSUBR ST0,ST1 and the
     * FDIVRP (0x300097a3, speed is the divisor). The carrier therefore retains
     * the live Windows PC=53 x87 value through the friction calculation. */
    playerState_t *frictionPs = move->ps; /* 0x30009726 independent reload. */
    long double speedSquared =
        (long double)frictionPs->velocity[0] * frictionPs->velocity[0];
    speedSquared +=
        (long double)frictionPs->velocity[1] * frictionPs->velocity[1];
    speedSquared +=
        (long double)frictionPs->velocity[2] * frictionPs->velocity[2];
    long double speed = coduo_x87_sqrtl(speedSquared);

    if (speed < 1.0f) {
        /* 0x30009756..0x30009767: below the cutoff, kill the velocity entirely. */
        frictionPs->velocity[0] = 0.0f;
        playerState_t *velocityYPs = move->ps;
        velocityYPs->velocity[1] = 0.0f;
        playerState_t *velocityZPs = move->ps;
        velocityZPs->velocity[2] = 0.0f;
    } else {
        /* 0x30009769..0x300097bf: control = max(speed, 100.0f) (stop-speed floor);
         * drop = control * pml.frametime * 8.25f; newspeed = max(speed - drop, 0);
         * velocity *= newspeed / speed. The x87 selects `speed` when speed >= 100.0f,
         * else the 100.0f constant. */
        /* control/newspeed/frictionScale stay in the x87 stack (control via FLD ST0,
         * the friction product * frametime * 8.25f, the FSUBR, and the FDIVRP) with
         * no float store until each velocity component is written -- long double. */
        long double control = (speed < 100.0L) ? 100.0L : speed;
        long double newspeed =
            speed - control * (long double)pml.frametime * 8.25L;
        if (newspeed < 0.0f) {
            newspeed = 0.0f;
        }
        long double frictionScale = newspeed / speed;
        frictionPs->velocity[0] =
            (float)(frictionScale * frictionPs->velocity[0]);
        playerState_t *velocityYPs = move->ps;
        velocityYPs->velocity[1] =
            (float)(frictionScale * velocityYPs->velocity[1]);
        playerState_t *velocityZPs = move->ps;
        velocityZPs->velocity[2] =
            (float)(frictionScale * velocityZPs->velocity[2]);
    }

    /* 0x300097bf..0x300097c7: scale = PM_CmdScale(&move->command). The current usercmd
     * is embedded at move+0x04 (LEA ECX,[EDI+4]); reached through pmove_t's
     * flattened fields as &move->command, matching the sibling
     * movers. */
    usercmd_t *cmd = &move->command;
    float scale = PM_CmdScale(cmd);

    /* 0x300097cb..0x30009839: sign-extend the three command movement bytes and
     * convert to float (forwardmove = forwardmove @ +0x18, rightmove = s8[1] @
     * +0x19, upmove = s8[2] @ +0x1a) and build the wish-velocity in the pmove basis:
     *   wishvel[i] = pml.forward[i]*forwardmove + pml.right[i]*rightmove,
     *   wishvel[2] additionally += upmove (direct vertical, FILD'd from the int). */
    float fmove = (float)cmd->forwardmove; /* forwardmove */
    float smove = (float)cmd->rightmove; /* rightmove   */
    int32_t umove = cmd->upmove;      /* upmove (widened to int by FILD) */

    vec3_t wishdir;
    long double wishX = (long double)pml.right[0] * smove;
    wishX += (long double)pml.forward[0] * fmove;
    long double wishY = (long double)pml.right[1] * smove;
    wishY += (long double)pml.forward[1] * fmove;
    long double wishZBasis = (long double)pml.right[2] * smove;
    wishZBasis += (long double)pml.forward[2] * fmove;
    long double wishZ = (long double)umove + wishZBasis;
    wishdir[2] = (float)wishZ; /* 0x3000983f stores z first. */
    wishdir[0] = (float)wishX;
    wishdir[1] = (float)wishY;

    /* 0x30009857..0x3000985c: wishspeed = |wishdir| (VectorNormalize normalizes in
     * place and returns the pre-normalization length), then the command scale is
     * applied to the SPEED (wishspeed *= scale) -- the noclip ordering, distinct from
     * the spectator move which scales the wishvel before normalizing. */
    const float normalizedWishspeed = VectorNormalize(wishdir);
    float wishspeed =
        (float)((long double)normalizedWishspeed * (long double)scale);

    /* 0x30009860..0x30009870: accelerate the player velocity toward wishdir with the
     * noclip acceleration of 9.0f (the pushed 0x41100000). */
    PM_Accelerate(wishdir, wishspeed, 9.0f);

    /* 0x30009875..0x300098b0: advance the origin with NO collision:
     * origin[i] += velocity[i] * pml.frametime (VectorMA). This direct integration,
     * with no PM_StepSlideMove/trace, is what makes this the no-clip move. */
    long double frameTimeX = pml.frametime; /* 0x30009875 load precedes ps reload. */
    playerState_t *originXPs = move->ps;
    originXPs->psOrigin[0] = (float)(frameTimeX * originXPs->velocity[0]
                                     + originXPs->psOrigin[0]);
    playerState_t *originYPs = move->ps;
    long double frameTimeY = pml.frametime;
    originYPs->psOrigin[1] = (float)(frameTimeY * originYPs->velocity[1]
                                     + originYPs->psOrigin[1]);
    playerState_t *originZPs = move->ps;
    long double frameTimeZ = pml.frametime;
    originZPs->psOrigin[2] = (float)(frameTimeZ * originZPs->velocity[2]
                                     + originZPs->psOrigin[2]);
}
