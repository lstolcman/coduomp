#include "bg_pmove.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

#define PM_SHORT_TO_ANGLE 0.0054931640625f

/* Float objects preserve the original binary32 operands even when the native
 * x87 compiler carries promoted decimal literals at excess precision. */
static const float PM_SPREAD_TURN_SCALE = 0.01f;
static const float PM_SPREAD_AIR_GROWTH = 1.28f;

/*
 * The authoritative Windows cgame/game bodies are instruction-identical at
 * 0x30013a90 and 0x200139d0.  The Linux game body at RVA 0x00035dcb retains
 * the same decisions, but the supported results depend on different rounding
 * boundaries. Windows rounds a frame-time reciprocal and retains the final
 * turn contribution through movement growth and the spread update. Linux
 * divides each contribution by frame time and spills each accumulator update.
 * The arithmetic precision is selected independently by the build.
 */
void PM_AdjustAimSpreadScale(void)
{
    const weaponInfo_t *const weaponInfo = pml.weaponInfo;
    playerState_t *const ps = pm->ps;
    float decay = 1.0f;
    long double growth = 0.0L;
    long double decayRate = weaponInfo->aimSpreadDecayRate;

    if (weaponInfo->aimSpreadDecayRate != 0.0f) {
        float stanceScale = 1.0f;
        if (ps->groundEntityNum == ENTITYNUM_NONE && ps->pmType != PM_TYPE_LINKED) {
            stanceScale = 0.5f;
        } else if ((ps->entityStateFlags & EF_PRONE) != 0) {
            stanceScale = weaponInfo->aimSpreadProneScale;
        } else if ((ps->entityStateFlags & EF_CROUCHING) != 0) {
            stanceScale = weaponInfo->aimSpreadCrouchScale;
        }

        /* The stance product remains wide until the frame-time product is stored. */
        decayRate *= stanceScale;
        decay = (float)(decayRate * pml.frametime);

        if (ps->adsFraction != 1.0f) {
            if (weaponInfo->aimSpreadTurnRate != 0.0f) {
                const float inverseFrameTime = (float)(1.0L / pml.frametime);
                float previousGrowth = 0.0f;
                for (int32_t axis = 0; axis < 2; ++axis) {
                    const float currentAngle = (float)((long double)pm->command.angles[axis] * PM_SHORT_TO_ANGLE);
                    const float previousAngle = (float)((long double)pm->oldCommand.angles[axis] * PM_SHORT_TO_ANGLE);
                    const float delta = fabsf(AngleSubtract(currentAngle, previousAngle));
                    growth = (long double)delta * weaponInfo->aimSpreadTurnRate *
                             PM_SPREAD_TURN_SCALE * inverseFrameTime + previousGrowth;
                    previousGrowth = (float)growth;
                }
                /* Only the next axis reloads the rounded accumulator. The last
                 * axis remains wide through all following additions and products. */
            }

            if (weaponInfo->aimSpreadMoveAdd != 0.0f &&
                (pm->command.forwardmove != 0 || pm->command.rightmove != 0)) {
                growth += weaponInfo->aimSpreadMoveAdd;
            }
            if (ps->groundEntityNum == ENTITYNUM_NONE && ps->pmType != PM_TYPE_LINKED) {
                for (int32_t axis = 0; axis < 2; ++axis) {
                    growth += PM_SPREAD_AIR_GROWTH;
                }
            }
            growth *= pml.frametime;
        }
    }

    ps->aimSpreadScale = (float)((growth - decay) * PM_AIM_SPREAD_SCALE_MAX + ps->aimSpreadScale);

    if (ps->aimSpreadScale < 0.0f) {
        ps->aimSpreadScale = 0.0f;
    } else if (ps->aimSpreadScale > (float)PM_AIM_SPREAD_SCALE_MAX) {
        ps->aimSpreadScale = (float)PM_AIM_SPREAD_SCALE_MAX;
    }
}
