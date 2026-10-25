#include "bg_pmove.h"

#include "bg_weapon.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

/*
 * The authoritative Windows cgame/game bodies are instruction-identical apart
 * from relocations and their dependency addresses:
 *
 *   uo_cgame_mp_x86.dll  PM_UpdateLean 0x3000c560
 *   uo_game_mp_x86.dll   PM_UpdateLean 0x2000c320
 *
 * Linux game RVA 0x0002ac45 retains the same button gates, stance selection,
 * easing state machine, trace arguments, inverse-lean clearance curve, and
 * final sign restoration.  Its unoptimized body retains calls to the shared
 * helpers and spills named binary32 locals that MSVC inlines or keeps live.
 * Windows uses binary32 reciprocal durations; Linux divides by the durations.
 * Keep that arithmetic distinction independently of the host architecture.
 */
#define PM_LEAN_RETURN_MSEC 280.0f
#define PM_LEAN_ENTER_MSEC 350.0f
/* Exact binary32 values also survive excess-precision literal evaluation. */
#define PM_LEAN_RETURN_RATE 0.00357142859138548374176025390625f
#define PM_LEAN_ENTER_RATE 0.00285714282654225826263427734375f

void PM_UpdateLean(playerState_t *ps, const usercmd_t *command,
                   pm_trace_fn_t traceFunc)
{
    int32_t leanInput = 0;
    float maxLean;
    float leanFraction;

    if ((command->wbuttons & (PM_WBUTTON_LEAN_LEFT |
                              PM_WBUTTON_LEAN_RIGHT)) != 0 &&
        (ps->playerStateFlags & PMF_FOLLOW) == 0 &&
        ps->pmType < PM_TYPE_DEAD &&
        (ps->groundEntityNum != ENTITYNUM_NONE ||
         ps->pmType == PM_TYPE_LINKED)) {
        if ((command->wbuttons & PM_WBUTTON_LEAN_LEFT) != 0) {
            leanInput = -1;
        }
        if ((command->wbuttons & PM_WBUTTON_LEAN_RIGHT) != 0) {
            leanInput++;
        }
    }

    if ((ps->entityStateFlags & EF_RESTRICTED_MASK) != 0 ||
        ((ps->playerStateFlags & PMF_ADS) != 0 &&
         BG_GetInfoForWeapon(ps->currentWeapon)->weaponClass ==
             WEAPCLASS_LMG)) {
        leanInput = 0;
    }

    maxLean = PM_GetEffectiveStance(ps) == EFFECTIVE_STANCE_PRONE
                  ? 0.25f
                  : 0.5f;
    leanFraction = ps->leanFraction;

    /* The movement frame supplies a bounded integer millisecond count.
     * Windows multiplies by a stored reciprocal; Linux retains the divide. */
    const long double leanStep = (long double)pml.msec *
        (long double)(leanInput == 0 ? PM_LEAN_RETURN_RATE : PM_LEAN_ENTER_RATE) * (long double)maxLean;

    if (leanInput == 0) {
        if (leanFraction > 0.0f) {
            leanFraction = (float)((long double)leanFraction - leanStep);
            if (leanFraction < 0.0f) {
                leanFraction = 0.0f;
            }
        } else if (leanFraction < 0.0f) {
            leanFraction = (float)((long double)leanFraction + leanStep);
            if (leanFraction > 0.0f) {
                leanFraction = 0.0f;
            }
        }
    } else if (leanInput < 0) {
        if (leanFraction > -maxLean) {
            leanFraction = (float)((long double)leanFraction - leanStep);
        }
        if (leanFraction < -maxLean) {
            leanFraction = -maxLean;
        }
    } else {
        if (leanFraction < maxLean) {
            leanFraction = (float)((long double)leanFraction + leanStep);
        }
        if (leanFraction > maxLean) {
            leanFraction = maxLean;
        }
    }

    ps->leanFraction = leanFraction;
    if (ps->leanFraction != 0.0f || isnan(ps->leanFraction)) {
        trace_t leanTrace;
        vec3_t start;
        vec3_t end;
        const vec3_t mins = {-8.0f, -8.0f, -8.0f};
        const vec3_t maxs = {8.0f, 8.0f, 8.0f};
        float blockedFraction;

        end[0] = ps->psOrigin[0];
        end[1] = ps->psOrigin[1];
        end[2] = ps->psOrigin[2] + ps->viewHeightCurrent;
        start[0] = end[0];
        start[1] = end[1];
        start[2] = end[2];
        AddLeanToPosition(end, ps->viewAngles[1],
                          (float)PM_FloatSign(ps->leanFraction),
                          16.0f, 20.0f);

        traceFunc(&leanTrace, start, mins, maxs, end,
                  ps->psClientNum, MASK_PLAYERSOLID);
        blockedFraction = UnGetLeanFraction(leanTrace.fraction);
        if (blockedFraction < PM_FloatAbs(ps->leanFraction)) {
            ps->leanFraction = (float)(
                (long double)PM_FloatSign(ps->leanFraction) *
                (long double)blockedFraction);
        }
    }
}
