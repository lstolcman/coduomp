#include "bg_weapon.h"

#include "bg_pmove.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87_eval.h"

#include <stdint.h>


enum {
    BG_VIEWHEIGHT_LERP_LONG_MSEC = 400,
    BG_VIEWHEIGHT_LERP_SHORT_MSEC = 200
};

/*
 * The Windows cgame and game bodies are instruction-identical apart from
 * relocated globals and constants:
 *
 *   uo_cgame_mp_x86.dll 0x30011950
 *   uo_game_mp_x86.dll  0x20011890
 *
 * Both retain the active transition fraction and final interpolation in x87
 * ST0 under the process PC=53 policy.  The caller may therefore consume more
 * than binary32 precision even though each settled-stance return is a loaded
 * float.  The long-double carrier preserves that original live-ST0 contract.
 *
 * Linux game RVA 0x00032902 makes the same stance decisions but stores the
 * fraction and every interpolated result as binary32 before returning.  Its
 * public body consequently retains the original float return boundary.
 */

long double BG_GetMinSpreadForWeapon(const playerState_t *ps, int32_t weapon,
                                     int32_t time, int32_t isAds)
{
    const weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
    const float standing = isAds != 0
        ? weaponInfo->adsSpread : weaponInfo->hipSpreadStandMin;
    const float crouched = isAds != 0
        ? weaponInfo->adsSpreadDucked : weaponInfo->hipSpreadDucked;
    const float prone = isAds != 0
        ? weaponInfo->adsSpreadProne : weaponInfo->hipSpreadProne;
    int32_t lerpStartTime = ps->viewHeightLerpTime;
    int32_t fromViewHeight;
    int32_t duration;
    int32_t elapsed;

    /* Compare the integer target without first rounding it to binary32. */
    if ((WIDE_FLOAT)ps->viewHeightLerpTarget == (WIDE_FLOAT)ps->viewHeightCurrent ||
        lerpStartTime == 0) {
        if ((ps->playerStateFlags & PMF_PRONE) != 0) {
            return (long double)prone;
        }
        if ((ps->playerStateFlags & PMF_DUCKED) != 0) {
            return (long double)crouched;
        }
        return (long double)standing;
    }

    fromViewHeight = ps->viewHeightLerpTarget;
    if (fromViewHeight == ps->proneViewHeight) {
        duration = BG_VIEWHEIGHT_LERP_LONG_MSEC;
    } else if (fromViewHeight == ps->crouchViewHeight) {
        duration = ps->viewHeightLerpDown != 0
            ? BG_VIEWHEIGHT_LERP_SHORT_MSEC
            : BG_VIEWHEIGHT_LERP_LONG_MSEC;
    } else {
        duration = BG_VIEWHEIGHT_LERP_SHORT_MSEC;
    }

    elapsed = coduo_int32_from_bits(
        (uint32_t)time - (uint32_t)lerpStartTime);

    long double fraction =
        (long double)elapsed / (long double)duration;

    if (fraction < 0.0L) {
        fraction = 0.0L;
    } else if (fraction > 1.0L) {
        fraction = 1.0L;
    }

#define BG_WINDOWS_SPREAD_LERP(from, to)                                  \
    ((long double)(from) + fraction *                                    \
        ((long double)(to) - (long double)(from)))

    if (fromViewHeight == ps->proneViewHeight) {
        return BG_WINDOWS_SPREAD_LERP(crouched, prone);
    }
    if (fromViewHeight == ps->standViewHeight) {
        return BG_WINDOWS_SPREAD_LERP(crouched, standing);
    }
    if (ps->viewHeightLerpDown != 0) {
        return BG_WINDOWS_SPREAD_LERP(standing, crouched);
    }
    return BG_WINDOWS_SPREAD_LERP(prone, crouched);

#undef BG_WINDOWS_SPREAD_LERP
}
