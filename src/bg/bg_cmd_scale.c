#include "bg_pmove.h"

#include "compat/coduo_native_x87.h"


#include <math.h>
#include <stdint.h>

/*
 * The Windows cgame/game PM_CmdScale bodies are instruction-identical apart
 * from relocations (0x30008690 and 0x20008440).  They keep the complete scale
 * in x87 through the stance and free-move multipliers.  All five callers in
 * each DLL immediately store ST0 as binary32, so float is the common observable
 * interface despite the Windows callee's excess precision.
 *
 * Linux game RVA 0x00023905 has the same integer inputs and branch decisions,
 * but calls glibc sqrt through a binary64 argument and stores the length, base
 * scale, and each subsequent multiplier result as binary32.  The whole bodies
 * below preserve those proven spill graphs; see the discrepancy record.
 */

float PM_CmdScale(const usercmd_t *command)
{
    int32_t forward = command->forwardmove;
    int32_t right = command->rightmove;
    int32_t up = command->upmove;
    int32_t maximum;
    int32_t lengthSquared;
    const playerState_t *ps;

    if (forward < 0) {
        forward = -forward;
    }
    if (right < 0) {
        right = -right;
    }
    if (up < 0) {
        up = -up;
    }
    maximum = forward;
    if (maximum < right) {
        maximum = right;
    }
    if (maximum < up) {
        maximum = up;
    }

    if (maximum == 0) {
        return 0.0f;
    }

    forward = command->forwardmove;
    right = command->rightmove;
    up = command->upmove;
    lengthSquared = forward * forward + right * right + up * up;
    ps = pm->ps;

    {
        long double scale =
            ((long double)ps->speed * (long double)maximum) /
            (coduo_x87_sqrtl((long double)lengthSquared) * 127.0L);

        if ((ps->playerStateFlags & PMF_WALKING) != 0 ||
            ps->leanFraction != 0.0f) {
            scale *= (long double)ps->walkSpeedScale;
        } else if ((ps->playerStateFlags & PMF_SPRINTING) != 0) {
            scale *= (long double)ps->sprintSpeedScale;
        } else {
            scale *= (long double)ps->runSpeedScale;
        }

        if (ps->pmType == PM_TYPE_NOCLIP) {
            scale *= 3.0L;
        }
        if (ps->pmType == PM_TYPE_UFO) {
            scale *= 6.0L;
        }
        return (float)scale;
    }
}
