// Source: uo_cgame_mp_x86.dll 0x3000aa70..0x3000b003
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000aa70_3000b003.mcode
//
// PM_ViewHeightAdjust - update the player-state floating viewheight toward its
// integer stance target. Handles the four machine-code-resident easing tables,
// mid-transition reversals, and the optional table-driven horizontal velocity
// adjustment. No arguments; operates on pm/pml globals. Name is from
// the same-module PPC bank. The CanDamage size guess is rejected.
// Windows game 0x2000a830 is instruction-identical to Windows cgame
// 0x3000aa70 apart from relocations; both Mac modules retain the same name and
// 0x74c-byte body size.

#include "bg_pmove.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "math/q_math.h"

#include <math.h>


enum {
    VIEWHEIGHT_PERCENT_MAX = 100
};

void PM_ViewHeightAdjust(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    int32_t target = ps->viewHeightTarget;
    int32_t percent = 0;

    if (target == 0 || ps->viewHeightCurrent == 0.0f) {
        ps->viewHeightCurrent =
            (ps->pmType == PM_TYPE_SPECTATOR) ? 0.0f : (float)target;
        return;
    }

    if ((long double)ps->viewHeightCurrent == (long double)target &&
        ps->viewHeightLerpTime == 0) {
        return;
    }

    if (target != ps->proneViewHeight &&
        target != ps->crouchViewHeight &&
        target != ps->standViewHeight) {
        ps->viewHeightLerpTime = 0;
        ps = move->ps; /* 0x3000aaf8 reload after clearing the lerp timer. */
        qboolean moveUp =
            ((long double)target > (long double)ps->viewHeightCurrent);
        /* The target/current comparison is issued before this load. step stays
         * live in st0 through the branch-specific FADD/FSUBR. */
        long double step = (long double)pml.frametime * 180.0f;
        if (moveUp) {
            long double adjusted = step + ps->viewHeightCurrent;
            ps->viewHeightCurrent = (float)adjusted;
            ps = move->ps;
            if ((long double)target <= (long double)ps->viewHeightCurrent) {
                ps->viewHeightCurrent = (float)target;
            }
        } else {
            long double adjusted = (long double)ps->viewHeightCurrent - step;
            ps->viewHeightCurrent = (float)adjusted;
            ps = move->ps;
            if ((long double)target >= (long double)ps->viewHeightCurrent) {
                ps->viewHeightCurrent = (float)target;
            }
        }
        return;
    }

    if (ps->viewHeightLerpTime != 0) {
        const pmLerpEntry_t *table;
        float originAdjust;
        int32_t lerpStartTime = ps->viewHeightLerpTime;
        int32_t lerpTarget = ps->viewHeightLerpTarget;
        int32_t lerpDown = ps->viewHeightLerpDown;
        int32_t duration = PM_GetViewHeightLerpTime(
            ps, lerpTarget, lerpDown);

        int32_t elapsed = coduo_int32_from_bits(
            (uint32_t)move->command.commandTime - (uint32_t)lerpStartTime);
        int32_t scaledElapsed = coduo_int32_from_bits(
            (uint32_t)elapsed * (uint32_t)VIEWHEIGHT_PERCENT_MAX);
        percent = scaledElapsed / duration;
        if (percent < 0) {
            percent = 0;
        } else if (percent > VIEWHEIGHT_PERCENT_MAX) {
            percent = VIEWHEIGHT_PERCENT_MAX;
        }

        if (percent == VIEWHEIGHT_PERCENT_MAX) {
            /* 0x3000abe6 fild [esp+0x18]; fstp [ecx+0xf8]: [esp+0x18] aliases the
             * spill of edi=[ecx+0x100] (viewHeightLerpTarget) at 0x3000ab9d, so the
             * binary sets viewHeightCurrent from viewHeightLerpTarget (+0x100), matching the
             * second lerp block's 100% case (fild [eax+0x100]). A prior pass used
             * `target` (viewHeightTarget, +0xf4) here. */
            ps->viewHeightCurrent = (float)lerpTarget;
            playerState_t *timePs = move->ps;
            timePs->viewHeightLerpTime = 0;
            playerState_t *adjustPs = move->ps;
            adjustPs->viewHeightLerpPosAdj = 0.0f;
        } else {
            if (lerpTarget == ps->proneViewHeight) {
                table = pmViewHeightLerpProne;
            } else if (lerpTarget == ps->crouchViewHeight) {
                table = lerpDown
                      ? pmViewHeightLerpCrouchedRising
                      : pmViewHeightLerpCrouchedFalling;
            } else {
                table = pmViewHeightLerpStanding;
            }

            long double height =
                PM_ViewHeightTableLerp(percent, table, &originAdjust);
            playerState_t *heightPs = move->ps;
            heightPs->viewHeightCurrent = (float)height;
            ps = move->ps;
            /* FABS is on the 80-bit diff and FCOMP is against (double)0.05f
             * (0x3007c240 == 0.05f widened, so the value is 0.05f); fabsl keeps the
             * diff unrounded to match the inline FABS at 0x3000ac65. */
            long double adjustmentDelta =
                (long double)ps->viewHeightLerpPosAdj - originAdjust;
            if (fabsl(adjustmentDelta) > 0.05f) {
                vec3_t savedVelocity;
                vec3_t direction;
                /* speed is one 80-bit chain (FSUB; optional FMUL 0.5f; FDIV
                 * pml.frametime) kept in st0 and rounded ONCE at FSTP [ESP+0x18]
                 * (0x3000accb) before the velocity rescale reloads it. */
                float speedRounded;

                savedVelocity[0] = ps->velocity[0];
                long double speed =
                    (long double)originAdjust - ps->viewHeightLerpPosAdj;
                savedVelocity[1] = ps->velocity[1];
                savedVelocity[2] = ps->velocity[2];
                if (ps->groundEntityNum == ENTITYNUM_NONE) {
                    speed *= 0.5f;
                }
                speed /= pml.frametime;
                direction[0] = pml.forward[0];
                direction[1] = pml.forward[1];
                direction[2] = 0.0f;
                speedRounded = (float)speed;   /* FSTP float [ESP+0x18] */
                VectorNormalize(direction);
                ps->velocity[0] =
                    (float)((long double)direction[0] * speedRounded);
                playerState_t *velocityYPs = move->ps;
                velocityYPs->velocity[1] =
                    (float)((long double)direction[1] * speedRounded);
                playerState_t *velocityZPs = move->ps;
                velocityZPs->velocity[2] =
                    (float)((long double)direction[2] * speedRounded);
                PM_StepSlideMove(1);
                move = pm; /* 0x3000ad04 callback-boundary reload. */
                playerState_t *restoreXPs = move->ps;
                restoreXPs->velocity[0] = savedVelocity[0];
                playerState_t *restoreYPs = move->ps;
                restoreYPs->velocity[1] = savedVelocity[1];
                playerState_t *restoreZPs = move->ps;
                restoreZPs->velocity[2] = savedVelocity[2];
                playerState_t *publishPs = move->ps;
                publishPs->viewHeightLerpPosAdj = originAdjust;
            }
        }
    }

    ps = move->ps; /* 0x3000ad3a reload before reversal/new-transition handling. */
    if (ps->viewHeightLerpTime != 0) {
        int32_t liveTarget = ps->viewHeightTarget;
        int32_t start = ps->viewHeightLerpTarget;
        qboolean reverse;

        if (liveTarget == start) {
            return;
        }
        if (liveTarget < start) {
            reverse = (ps->viewHeightLerpDown == 0);
        } else {
            reverse = (ps->viewHeightLerpDown != 0);
        }
        if (!reverse) {
            return;
        }

        ps->viewHeightLerpDown ^= 1;
        percent = coduo_int32_from_bits((uint32_t)VIEWHEIGHT_PERCENT_MAX -
                                   (uint32_t)percent);
        ps = move->ps;
        if (ps->viewHeightLerpDown != 0) {
            int32_t currentLerpTarget = ps->viewHeightLerpTarget;
            if (currentLerpTarget == ps->standViewHeight) {
                int32_t crouchHeight = ps->crouchViewHeight;
                ps->viewHeightLerpTarget = crouchHeight;
            } else if (currentLerpTarget == ps->crouchViewHeight) {
                int32_t proneHeight = ps->proneViewHeight;
                ps->viewHeightLerpTarget = proneHeight;
            }
        } else {
            int32_t currentLerpTarget = ps->viewHeightLerpTarget;
            if (currentLerpTarget == ps->proneViewHeight) {
                int32_t crouchHeight = ps->crouchViewHeight;
                ps->viewHeightLerpTarget = crouchHeight;
            } else if (currentLerpTarget == ps->crouchViewHeight) {
                int32_t standHeight = ps->standViewHeight;
                ps->viewHeightLerpTarget = standHeight;
            }
        }

        if (percent == VIEWHEIGHT_PERCENT_MAX) {
            playerState_t *heightPs = move->ps;
            heightPs->viewHeightCurrent = (float)heightPs->viewHeightLerpTarget;
            playerState_t *timePs = move->ps;
            timePs->viewHeightLerpTime = 0;
            playerState_t *adjustPs = move->ps;
            adjustPs->viewHeightLerpPosAdj = 0.0f;
            return;
        }

        {
            const pmLerpEntry_t *table;
            float originAdjust;
            ps = move->ps; /* 0x3000ae2c reload before duration selection. */
            int32_t reversedLerpDown = ps->viewHeightLerpDown;
            int32_t reversedLerpTarget = ps->viewHeightLerpTarget;
            int32_t duration = PM_GetViewHeightLerpTime(
                ps, reversedLerpTarget, reversedLerpDown);
            long double reversedElapsed = (long double)duration * percent;
            reversedElapsed *= 0.01f;
            int32_t elapsed = coduo_fp_to_i32_extended(reversedElapsed);

            ps->viewHeightLerpTime = coduo_int32_from_bits(
                (uint32_t)move->command.commandTime - (uint32_t)elapsed);
            ps = move->ps;
            if (ps->viewHeightLerpTarget == ps->proneViewHeight) {
                table = pmViewHeightLerpProne;
            } else if (ps->viewHeightLerpTarget == ps->crouchViewHeight) {
                table = ps->viewHeightLerpDown
                      ? pmViewHeightLerpCrouchedRising
                      : pmViewHeightLerpCrouchedFalling;
            } else {
                table = pmViewHeightLerpStanding;
            }
            /* 0x3000ad97..0x3000ad9c already replaced percent with
             * 100-oldPercent; ESI passes that complemented value directly. */
            (void)PM_ViewHeightTableLerp(percent, table, &originAdjust);
            ps = move->ps;
            ps->viewHeightLerpPosAdj = originAdjust;
        }
        return;
    }

    target = ps->viewHeightTarget; /* 0x3000aecb uses the live target. */
    if ((long double)ps->viewHeightCurrent == (long double)target) {
        return;
    }

    ps->viewHeightLerpTime = move->command.commandTime;
    ps = move->ps;
    target = ps->viewHeightTarget;
    if (target == ps->proneViewHeight) {
        ps->viewHeightLerpDown = 1;
        ps = move->ps;
        int32_t crouchHeight = ps->crouchViewHeight;
        if ((long double)crouchHeight < (long double)ps->viewHeightCurrent) {
            ps->viewHeightLerpTarget = crouchHeight;
        } else {
            ps->viewHeightLerpTarget = ps->proneViewHeight;
        }
    } else if (target == ps->crouchViewHeight) {
        ps->viewHeightLerpDown =
            ((long double)target < (long double)ps->viewHeightCurrent) ? 1 : 0;
        ps = move->ps;
        ps->viewHeightLerpTarget = ps->crouchViewHeight;
    } else if (target == ps->standViewHeight) {
        ps->viewHeightLerpDown = 0;
        ps = move->ps;
        int32_t crouchHeight = ps->crouchViewHeight;
        if ((long double)crouchHeight > (long double)ps->viewHeightCurrent) {
            ps->viewHeightLerpTarget = crouchHeight;
        } else {
            ps->viewHeightLerpTarget = ps->standViewHeight;
        }
    }
}
