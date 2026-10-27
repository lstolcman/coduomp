#include "bg_animation.h"
#include "bg_animation_services.h"
#include "bg_vehicle.h"

#include "compat/coduo_fp_conversion.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>


enum {
    BG_SWING_ANGLE_MASK = 65535
};

/*
 * Windows cgame/game are instruction-identical after relocated addresses are
 * normalized:
 *
 *   BG_SwingAngles  0x30004330 / 0x20004330
 *   BG_PlayerAngles 0x30004550 / 0x20004550
 *
 * Linux game retains the canonical symbols at RVA 0x0001d9b4 and 0x0001dbac.
 * Supporting Mac cgame/game retain both names, equal per-pair sizes, and the
 * same call graph. Linux spills each updated angle to binary32 before calling
 * AngleMod; Windows carries the add/subtract directly into its inline BAMS
 * conversion. The shared body retains those live operations and the original
 * float stores before comparisons.
 */

/* NOT_FROM_ORIGINAL_SOURCE: source-level expression of the repeated inline
 * Windows ANGLE2SHORT/SHORT2ANGLE instruction sequence. */
static float bg_compat_quantize_angle_add(float left, float right)
{
    const float angleToShort = 182.04444885253906f;
    const float shortToAngle = 0.0054931640625f;
    int32_t packed;

    packed = coduo_fp_to_i32_extended(
        ((long double)left + (long double)right) * angleToShort);
    return (float)((double)(packed & BG_SWING_ANGLE_MASK) * shortToAngle);
}

/* NOT_FROM_ORIGINAL_SOURCE: subtraction companion for the two clamp sites. */
static float bg_compat_quantize_angle_subtract(float left, float right)
{
    const float angleToShort = 182.04444885253906f;
    const float shortToAngle = 0.0054931640625f;
    int32_t packed;

    packed = coduo_fp_to_i32_extended(
        ((long double)left - (long double)right) * angleToShort);
    return (float)((double)(packed & BG_SWING_ANGLE_MASK) * shortToAngle);
}

void BG_SwingAngles(float target, float deadband, float maxDeviation,
                    float stepScale, float *angle, qboolean *active)
{
    float delta;
    float step;

    if (*active == qfalse) {
        delta = AngleSubtract(*angle, target);
        if (!(delta > deadband) && !(-deadband > delta)) {
            return;
        }
        *active = qtrue;
    }

    delta = AngleSubtract(target, *angle);
    step = fabsf(delta) * 0.05f;
    if (step < 0.5f) {
        step = 0.5f;
    }

    /* Frame time rounds to float before the two multiplies; their product
     * rounds only after both operations, before the delta comparison. */
    if (delta >= 0.0f) {
        const WIDE_FLOAT frameTime = coduo_round_f32(bg_compat_animation_frame_time());
        float increment = coduo_round_f32(frameTime * step * stepScale);
        if (increment >= delta) {
            increment = delta;
            *active = qfalse;
        } else {
            *active = qtrue;
        }
        *angle = bg_compat_quantize_angle_add(*angle, increment);
    } else if (delta < 0.0f) {
        const WIDE_FLOAT frameTime = coduo_round_f32(bg_compat_animation_frame_time());
        float increment = coduo_round_f32(-(frameTime * step * stepScale));
        if (increment <= delta) {
            increment = delta;
            *active = qfalse;
        } else {
            *active = qtrue;
        }
        *angle = bg_compat_quantize_angle_add(*angle, increment);
    }

    delta = AngleSubtract(target, *angle);
    if (delta > maxDeviation) {
        *angle = bg_compat_quantize_angle_subtract(target, maxDeviation);
    } else if (delta < -maxDeviation) {
        *angle = bg_compat_quantize_angle_add(target, maxDeviation);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve an unspilled x87 multiply/add chain in
 * the shared source on both original precision-control modes. */
static float bg_compat_player_angle_multiply_add(float left, float scale,
                                                  float addend)
{
    return (float)((double)left * scale + addend);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the corresponding subtract/multiply
 * chain used for the wrapped pitch target. */
static float bg_compat_player_angle_subtract_multiply(float left,
                                                       float subtract,
                                                       float scale)
{
    return (float)(((double)left - subtract) * scale);
}

void BG_PlayerAngles(const entityState_t *entity, clientInfo_t *clientInfo)
{
    const uint32_t entityFlags = entity->eFlags;
    const uint32_t vehicleState = (uint32_t)entity->vehicleAnimState;
    const vehicle_type_t vehicleType = (vehicle_type_t)(
        (vehicleState & VEHICLE_ANIM_STATE_TYPE_MASK) >>
        VEHICLE_ANIM_STATE_TYPE_SHIFT);
    const int32_t vehiclePosition = (int32_t)(
        (vehicleState & VEHICLE_ANIM_STATE_POS_MASK) >>
        VEHICLE_ANIM_STATE_POS_SHIFT);
    const uint32_t animationIndex =
        (uint32_t)entity->legsAnim & ~ANIM_TOGGLEBIT;
    float leanAmount;
    float viewPitch;
    float viewYaw;
    const qboolean restricted = (entityFlags & EF_RESTRICTED_MASK) != 0;
    float torsoTarget;
    float legsTarget;
    float torsoMaximum;
    uint32_t moveType;

    leanAmount = clientInfo->leanAmount;
    viewPitch = clientInfo->viewPitch;
    viewYaw = AngleMod(clientInfo->viewYaw);

    if (!restricted ||
        BG_AllowPlayerWeaponAtVehiclePos(vehicleType, vehiclePosition)) {
        moveType = (uint32_t)BG_GetConditionValue(
            clientInfo, ANIM_COND_MOVETYPE, qfalse);
        if ((moveType & BG_ANIM_CLIMB_MOVE_TYPE_MASK) != 0 ||
            (moveType & BG_ANIM_IDLE_MOVE_TYPE_MASK) == 0) {
            clientInfo->torsoYawActive = qtrue;
            clientInfo->leanActive = qtrue;
            clientInfo->legsYawActive = qtrue;
        } else if (BG_GetConditionValue(clientInfo, ANIM_COND_FIRING,
                                        qtrue) != 0) {
            clientInfo->torsoYawActive = qtrue;
            clientInfo->leanActive = qtrue;
        }
    } else {
        clientInfo->torsoYawActive = qtrue;
        clientInfo->leanActive = qtrue;
        clientInfo->legsYawActive = qtrue;
    }

    legsTarget = viewYaw + leanAmount;
    torsoTarget = viewYaw;
    if ((entityFlags & EF_ANGLE_FIXED_YAW) == 0) {
        moveType = (uint32_t)BG_GetConditionValue(
            clientInfo, ANIM_COND_MOVETYPE, qfalse);
        if ((moveType & BG_ANIM_CLIMB_MOVE_TYPE_MASK) != 0) {
            torsoMaximum = 0.0f;
            torsoTarget = legsTarget;
        } else if ((entityFlags & EF_ANGLE_TORSO_ONLY_YAW) != 0) {
            torsoMaximum = 90.0f;
        } else if ((entityFlags & EF_ANGLE_TURRET_TIGHT) != 0) {
            torsoMaximum = 45.0f;
        } else {
            if ((entityFlags & EF_ANGLE_TURRET_YAW) == 0) {
                torsoTarget = bg_compat_player_angle_multiply_add(
                    leanAmount, 0.3f, viewYaw);
            }
            torsoMaximum = 90.0f;
        }
    } else {
        torsoMaximum = 90.0f;
        legsTarget = viewYaw;
    }

    if (restricted &&
        BG_AllowPlayerWeaponAtVehiclePos(vehicleType, vehiclePosition) &&
        entity->weapon != 0) {
        legsTarget = clientInfo->turretOverrideAngles[1];
    }

    BG_SwingAngles(torsoTarget, 0.0f, torsoMaximum,
                   bg_compat_animation_swing_speed(),
                   &clientInfo->torsoYawAngle,
                   &clientInfo->torsoYawActive);

    if ((entityFlags & EF_ANGLE_FIXED_YAW) != 0) {
        BG_SwingAngles(legsTarget, 0.0f, 150.0f,
                       bg_compat_animation_swing_speed(),
                       &clientInfo->legsYawAngle,
                       &clientInfo->legsYawActive);
    } else if ((entityFlags & EF_ANGLE_TORSO_ONLY_YAW) != 0) {
        clientInfo->legsYawActive = qfalse;
        clientInfo->legsYawAngle = viewYaw + leanAmount;
    } else {
        /* The original bodies clear ANIM_TOGGLEBIT and index the 512-entry
         * animation table directly.  This is implicitly bounded: legsAnim is
         * a 10-bit entity netfield, and clearing its 0x200 toggle bit leaves
         * exactly the table's 0..511 index domain. */
        if ((bgs.animationTable.entries[animationIndex].flagsLowByte &
             BG_ANIM_ENTRY_STRAFE_MASK) != 0) {
            clientInfo->legsYawActive = qfalse;
            BG_SwingAngles(viewYaw, 0.0f, 150.0f,
                           bg_compat_animation_swing_speed(),
                           &clientInfo->legsYawAngle,
                           &clientInfo->legsYawActive);
        } else {
            BG_SwingAngles(legsTarget,
                           clientInfo->legsYawActive != qfalse ? 0.0f : 40.0f,
                           150.0f, bg_compat_animation_swing_speed(),
                           &clientInfo->legsYawAngle,
                           &clientInfo->legsYawActive);
        }
    }

    if (restricted) {
        clientInfo->torsoYawAngle = viewYaw;
        clientInfo->legsYawAngle = viewYaw;
    } else {
        moveType = (uint32_t)BG_GetConditionValue(
            clientInfo, ANIM_COND_MOVETYPE, qfalse);
        if ((moveType & BG_ANIM_CLIMB_MOVE_TYPE_MASK) != 0) {
            clientInfo->torsoYawAngle = viewYaw + leanAmount;
            clientInfo->legsYawAngle = viewYaw + leanAmount;
        }
    }

    if ((entityFlags & EF_ANGLE_FIXED_YAW) != 0 || restricted) {
        viewPitch = 0.0f;
    } else {
        moveType = (uint32_t)BG_GetConditionValue(
            clientInfo, ANIM_COND_MOVETYPE, qfalse);
        if ((moveType & BG_ANIM_CLIMB_MOVE_TYPE_MASK) != 0) {
            viewPitch = 0.0f;
        } else if (viewPitch > 180.0f) {
            viewPitch = bg_compat_player_angle_subtract_multiply(
                viewPitch, 360.0f, 0.6f);
        } else {
            viewPitch = bg_compat_player_angle_multiply_add(
                viewPitch, 0.6f, 0.0f);
        }
    }

    BG_SwingAngles(viewPitch, 0.0f, 45.0f, 0.15f,
                   &clientInfo->leanAngle, &clientInfo->leanActive);
}
