// Source: uo_cgame_mp_x86.dll 0x30015ca0..0x30016112
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30015ca0_30016112.mcode
//
// BG_CalculateWeaponPosition_Sway -- blend the active weapon's hip/ADS sway
// envelope, derive view-angle deltas from the saved previous view angles, and
// smoothly update the two sway-output carriers. The G_SetUICvars header name is
// rejected: this body reads player/weaponInfo_t data and performs angle math only.

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

#define BG_SWAY_MILLISECONDS_TO_SECONDS 0.001f
/* The snap epsilon compared at 0x30015ee7 etc. is the QWORD at 0x3007be28 =
 * 0x3f50624de0000000, which is the FLOAT 0.001f widened to double storage --
 * NOT the double 0.001 (0x3f50624dd2f1a9fc). The f-suffixed literal promotes
 * to exactly that value in the double compares below. */
#define BG_SWAY_SNAP_EPSILON 0.001f
#define BG_SWAY_HALF_TURN 180.0f
#define BG_SWAY_FULL_TURN 360.0f

/*
 * The separately emitted smoothing body is present in both authoritative
 * targets (Windows cgame 0x30015c50; Linux game RVA 0x00039236).  Its source
 * argument order is target, current, rate, msec.  Windows returns the live
 * PC=53 x87 expression, whereas Linux explicitly reloads a binary32 result
 * before returning.  The Windows sway body below inlines the same source
 * operation instead of calling this function; Linux calls it four times.
 */
long double BG_SmoothWeaponSwayValue(float target, float current, float rate,
                                     int32_t msec)
{
    const long double delta =
        (long double)target - (long double)current;
    const float step = (float)(
        (long double)msec * (long double)BG_SWAY_MILLISECONDS_TO_SECONDS *
        delta * (long double)rate);
    const long double absoluteDelta = fabsl(delta);
    const long double absoluteStep =
        (long double)fabsf(step);

    if (absoluteDelta > (long double)(double)BG_SWAY_SNAP_EPSILON &&
        !(absoluteDelta < absoluteStep)) {
        return (long double)current + (long double)step;
    }
    return (long double)target;
}

void BG_CalculateWeaponPosition_Sway(const playerState_t *ps,
                                     vec3_t previousViewAngles,
                                     vec3_t outPosition, vec2_t outAngles,
                                     float scale, int frameTime)
{
    weaponInfo_t **weaponTable = bg_weaponInfos;
    float adsFraction = ps->adsFraction;
    int32_t currentWeapon = ps->currentWeapon;
    const weaponInfo_t *weapon = weaponTable[currentWeapon];
    float maxAngle;
    float lerpSpeed;
    float pitchScale;
    float yawScale;
    float horizontalScale;
    float verticalScale;
    float pitchDelta;
    float yawDelta;
    float target;
    float error;
    float step;
    float dt;

    if (weapon->adsEnabled != 0) {
        /* 0x30015cde TEST AH,0x41 / JNZ skips the gate check when
         * adsFraction < 0 OR == 0 (or unordered): the early return needs
         * adsFraction STRICTLY greater than zero. */
        if (adsFraction > 0.0f && weapon->adsOverlayReticle != 0) {
            return;
        }
        maxAngle = (float)(
            ((long double)weapon->adsSwayMaxAngle -
             (long double)weapon->swayMaxAngle) *
                (long double)adsFraction +
            (long double)weapon->swayMaxAngle);
        lerpSpeed = (float)(
            ((long double)weapon->adsSwayLerpSpeed -
             (long double)weapon->swayLerpSpeed) *
                (long double)adsFraction +
            (long double)weapon->swayLerpSpeed);
        pitchScale = (float)(
            ((long double)weapon->adsSwayPitchScale -
             (long double)weapon->swayPitchScale) *
                (long double)adsFraction +
            (long double)weapon->swayPitchScale);
        yawScale = (float)(
            ((long double)weapon->adsSwayYawScale -
             (long double)weapon->swayYawScale) *
                (long double)adsFraction +
            (long double)weapon->swayYawScale);
        horizontalScale = (float)(
            ((long double)weapon->adsSwayHorizScale -
             (long double)weapon->swayHorizScale) *
                (long double)adsFraction +
            (long double)weapon->swayHorizScale);
        verticalScale = (float)(
            ((long double)weapon->adsSwayVertScale -
             (long double)weapon->swayVertScale) *
                (long double)adsFraction +
            (long double)weapon->swayVertScale);
    } else {
        maxAngle = weapon->swayMaxAngle;
        lerpSpeed = weapon->swayLerpSpeed;
        pitchScale = weapon->swayPitchScale;
        yawScale = weapon->swayYawScale;
        horizontalScale = weapon->swayHorizScale;
        verticalScale = weapon->swayVertScale;
    }

    pitchScale *= scale;
    yawScale *= scale;
    horizontalScale *= scale;
    verticalScale *= scale;

    pitchDelta = AngleSubtract(ps->viewAngles[0], previousViewAngles[0]);
    yawDelta = AngleSubtract(ps->viewAngles[1], previousViewAngles[1]);

    /* Both deltas are clamped to +/-maxAngle. The negated bound is built by
     * the FLD at 0x30015e27, which executes while the four AngleSubtract args
     * are still pushed: [ESP+0x1c] there is frame slot +0xc, the maxAngle
     * blend (NOT the yawScale slot the raw displacement suggests). FCHS then
     * stores -maxAngle for both lower-bound FCOMs (0x30015e3c/0x30015e64). */
    if (pitchDelta < -maxAngle) {
        pitchDelta = -maxAngle;
    } else if (pitchDelta > maxAngle) {
        pitchDelta = maxAngle;
    }
    if (yawDelta < -maxAngle) {
        yawDelta = -maxAngle;
    } else if (yawDelta > maxAngle) {
        yawDelta = maxAngle;
    }

    /* Round the millisecond count to binary32 before scaling to seconds. */
    dt = (float)frameTime;
    dt *= BG_SWAY_MILLISECONDS_TO_SECONDS;

    /* Snap-condition polarity at every carrier (0x30015eef/0x30015f00 etc.):
     * the step-add path requires |error| STRICTLY greater than the epsilon
     * (TEST AH,0x41/JNZ snaps on less-or-equal), and is taken when
     * |step| <= |error| OR the compare is unordered (TEST AH,0x41/JZ snaps
     * only on strictly-greater) -- hence `>` and `!(>)` below. */
    target = yawDelta * horizontalScale;
    error = target - outPosition[1];
    /* Both products stay wide until the completed smoothing step is stored. */
    step = (float)((long double)dt * error * lerpSpeed);
    if (fabs((double)error) > BG_SWAY_SNAP_EPSILON &&
        !(fabs((double)step) > fabs((double)error))) {
        outPosition[1] += step;
    } else {
        outPosition[1] = target;
    }

    target = pitchDelta * verticalScale;
    error = target - outPosition[2];
    step = (float)((long double)dt * error * lerpSpeed);
    if (fabs((double)error) > BG_SWAY_SNAP_EPSILON &&
        !(fabs((double)step) > fabs((double)error))) {
        outPosition[2] += step;
    } else {
        outPosition[2] = target;
    }

    target = pitchDelta * pitchScale;
    while ((long double)target - outAngles[0] > BG_SWAY_HALF_TURN) {
        target -= BG_SWAY_FULL_TURN;
    }
    error = target - outAngles[0];
    step = (float)((long double)dt * error * lerpSpeed);
    if (fabs((double)error) > BG_SWAY_SNAP_EPSILON &&
        !(fabs((double)step) > fabs((double)error))) {
        outAngles[0] += step;
    } else {
        outAngles[0] = target;
    }

    target = yawDelta * yawScale;
    while ((long double)target - outAngles[1] > BG_SWAY_HALF_TURN) {
        target -= BG_SWAY_FULL_TURN;
    }
    error = target - outAngles[1];
    step = (float)((long double)dt * error * lerpSpeed);
    if (fabs((double)error) > BG_SWAY_SNAP_EPSILON &&
        !(fabs((double)step) > fabs((double)error))) {
        outAngles[1] += step;
    } else {
        outAngles[1] = target;
    }

    outAngles[0] = AngleNormalize180(outAngles[0]);
    outAngles[1] = AngleNormalize180(outAngles[1]);
    previousViewAngles[0] = ps->viewAngles[0];
    previousViewAngles[1] = ps->viewAngles[1];
    previousViewAngles[2] = ps->viewAngles[2];
}
