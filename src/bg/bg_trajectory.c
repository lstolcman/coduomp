#include "bg_player_state.h"

#include <math.h>
#include <stdint.h>

#include "bg_trajectory_binding.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87_eval.h"
#include "math/q_math.h"


/*
 * The authoritative Windows cgame and game-module bodies have the same
 * trajectory switch, calculations, and stores:
 *
 *   uo_cgame_mp_x86.dll        0x30005f30, 0x30006250
 *   uo_game_mp_x86.dll         0x20005ca0, 0x20005fc0
 *
 * The Linux game-module bodies at RVA 0x0001fe63 and 0x00020494 implement the
 * same trajectory modes and output contract under the server's PC=64 x87
 * policy, but differ in their intermediate binary32 spills.  Keep complete
 * behavior bodies.  The local binding header preserves each module's original
 * fatal-error boundary without adding a role-selection build flag.
 */


#define TRAJECTORY_MSEC_TO_SEC       0.001f
#define TRAJECTORY_FULL_CIRCLE       6.2831855f
#define TRAJECTORY_HALF_GRAVITY      400.0f
#define TRAJECTORY_HALF_LOW_GRAVITY  120.00001f
#define TRAJECTORY_FLOAT_GRAVITY     80.0f
#define TRAJECTORY_DELTA_MSEC_TO_SEC      0.001f
#define TRAJECTORY_DELTA_FULL_CIRCLE      6.2831855f
#define TRAJECTORY_DELTA_SINE_SCALE       0.5f
#define TRAJECTORY_DELTA_GRAVITY          800.0f
#define TRAJECTORY_DELTA_LOW_GRAVITY      240.00002f
#define TRAJECTORY_DELTA_FLOAT_GRAVITY    160.0f

void BG_EvaluateTrajectory(const trajectory_t *tr, int32_t atTime,
                           vec3_t result)
{
    int32_t elapsedMs;
    WIDE_FLOAT deltaTime;

    switch (tr->trType) {
    case TR_STATIONARY:
    case TR_INTERPOLATE:
    case TR_LINKED:
        /* 0x30005f49..0x30005f58: three exact dword copies of trBase. */
        result[0] = tr->trBase[0];
        result[1] = tr->trBase[1];
        result[2] = tr->trBase[2];
        return;

    case TR_LINEAR:
        /* 0x30005f60..0x30005fda: signed FILD of the wrapping i386 SUB. */
        elapsedMs = (int32_t)((uint32_t)atTime - (uint32_t)tr->trTime);
        deltaTime =
            (WIDE_FLOAT)elapsedMs * (WIDE_FLOAT)TRAJECTORY_MSEC_TO_SEC;
        result[0] = (float)((WIDE_FLOAT)tr->trBase[0] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[0]);
        result[1] = (float)((WIDE_FLOAT)tr->trBase[1] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[1]);
        result[2] = (float)((WIDE_FLOAT)tr->trBase[2] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[2]);
        return;

    case TR_LINEAR_STOP: {
        int32_t startTime = tr->trTime;       /* 0x30005f8b */
        int32_t duration = tr->trDuration;    /* 0x30005f8e */
        int32_t endTime =
            coduo_int32_from_bits((uint32_t)startTime + (uint32_t)duration);
        int32_t clampedTime = atTime;

        /* 0x30005f8b..0x30005f99: signed JLE; cap only above the end. */
        if (clampedTime > endTime) {
            clampedTime = endTime;
        }
        elapsedMs =
            coduo_int32_from_bits((uint32_t)clampedTime - (uint32_t)startTime);
        deltaTime =
            (WIDE_FLOAT)elapsedMs * (WIDE_FLOAT)TRAJECTORY_MSEC_TO_SEC;
        /* 0x30005fa9..0x30005fb8: negative elapsed time is clamped to zero. */
        if (deltaTime < 0.0f) {
            deltaTime = 0.0f;
        }
        result[0] = (float)((WIDE_FLOAT)tr->trBase[0] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[0]);
        result[1] = (float)((WIDE_FLOAT)tr->trBase[1] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[1]);
        result[2] = (float)((WIDE_FLOAT)tr->trBase[2] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[2]);
        return;
    }

    case TR_SINE:
        elapsedMs = (int32_t)((uint32_t)atTime - (uint32_t)tr->trTime);
        /* 0x30005f7a..0x30005f89: FILD, integer-memory FDIV, 2*pi, FSIN. */
        deltaTime = coduo_x87_sinl(
            ((WIDE_FLOAT)elapsedMs / (WIDE_FLOAT)tr->trDuration) *
            (WIDE_FLOAT)TRAJECTORY_FULL_CIRCLE);
        result[0] = (float)((WIDE_FLOAT)tr->trBase[0] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[0]);
        result[1] = (float)((WIDE_FLOAT)tr->trBase[1] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[1]);
        result[2] = (float)((WIDE_FLOAT)tr->trBase[2] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[2]);
        return;

    case TR_GRAVITY:
        elapsedMs = (int32_t)((uint32_t)atTime - (uint32_t)tr->trTime);
        deltaTime =
            (WIDE_FLOAT)elapsedMs * (WIDE_FLOAT)TRAJECTORY_MSEC_TO_SEC;
        result[0] = (float)((WIDE_FLOAT)tr->trBase[0] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[0]);
        result[1] = (float)((WIDE_FLOAT)tr->trBase[1] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[1]);
        result[2] = (float)(
            (WIDE_FLOAT)tr->trBase[2] +
            deltaTime * (WIDE_FLOAT)tr->trDelta[2] -
            deltaTime * deltaTime *
                (WIDE_FLOAT)TRAJECTORY_HALF_GRAVITY);
        return;

    case TR_GRAVITY_LOW:
        elapsedMs = (int32_t)((uint32_t)atTime - (uint32_t)tr->trTime);
        deltaTime =
            (WIDE_FLOAT)elapsedMs * (WIDE_FLOAT)TRAJECTORY_MSEC_TO_SEC;
        result[0] = (float)((WIDE_FLOAT)tr->trBase[0] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[0]);
        result[1] = (float)((WIDE_FLOAT)tr->trBase[1] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[1]);
        result[2] = (float)(
            (WIDE_FLOAT)tr->trBase[2] +
            deltaTime * (WIDE_FLOAT)tr->trDelta[2] -
            deltaTime * deltaTime *
                (WIDE_FLOAT)TRAJECTORY_HALF_LOW_GRAVITY);
        return;

    case TR_GRAVITY_FLOAT:
        elapsedMs = (int32_t)((uint32_t)atTime - (uint32_t)tr->trTime);
        deltaTime =
            (WIDE_FLOAT)elapsedMs * (WIDE_FLOAT)TRAJECTORY_MSEC_TO_SEC;
        result[0] = (float)((WIDE_FLOAT)tr->trBase[0] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[0]);
        result[1] = (float)((WIDE_FLOAT)tr->trBase[1] +
                            deltaTime * (WIDE_FLOAT)tr->trDelta[1]);
        /* The 80.0 term is linear in time (0x3000609b..0x300060a3). */
        result[2] = (float)(
            (WIDE_FLOAT)tr->trBase[2] +
            deltaTime * (WIDE_FLOAT)tr->trDelta[2] -
            deltaTime * (WIDE_FLOAT)TRAJECTORY_FLOAT_GRAVITY);
        return;

    case TR_ACCELERATE: {
        int32_t duration = tr->trDuration;    /* 0x300060ad: retained */
        int32_t startTime = tr->trTime;       /* 0x300060b0 */
        int32_t endTime =
            coduo_int32_from_bits((uint32_t)duration + (uint32_t)startTime);
        int32_t clampedTime = atTime;
        WIDE_FLOAT speed;
        float acceleration;
        WIDE_FLOAT distance;

        if (clampedTime > endTime) {
            clampedTime = endTime;
        }
        elapsedMs =
            coduo_int32_from_bits((uint32_t)clampedTime - (uint32_t)startTime);
        const float storedDeltaTime = (float)(
            (WIDE_FLOAT)elapsedMs * (WIDE_FLOAT)TRAJECTORY_MSEC_TO_SEC);
        deltaTime = (WIDE_FLOAT)storedDeltaTime;

        /* 0x300060d6..0x30006102: inline VectorLength and acceleration. */
        speed = sqrtl(
            ((WIDE_FLOAT)tr->trDelta[0] *
                 (WIDE_FLOAT)tr->trDelta[0] +
             (WIDE_FLOAT)tr->trDelta[1] *
                 (WIDE_FLOAT)tr->trDelta[1]) +
            (WIDE_FLOAT)tr->trDelta[2] *
                (WIDE_FLOAT)tr->trDelta[2]);
        acceleration = (float)(
            speed /
            ((WIDE_FLOAT)duration *
             (WIDE_FLOAT)TRAJECTORY_MSEC_TO_SEC));

        /* 0x30006106: normalize trDelta into the result buffer. */
        (void)VectorNormalize2(tr->trDelta, result);
        distance = (WIDE_FLOAT)acceleration * deltaTime * deltaTime *
                   (WIDE_FLOAT)0.5f;
        result[0] = (float)((WIDE_FLOAT)tr->trBase[0] +
                            distance * (WIDE_FLOAT)result[0]);
        result[1] = (float)((WIDE_FLOAT)tr->trBase[1] +
                            distance * (WIDE_FLOAT)result[1]);
        result[2] = (float)((WIDE_FLOAT)tr->trBase[2] +
                            distance * (WIDE_FLOAT)result[2]);
        return;
    }

    case TR_DECCELERATE: {
        int32_t duration = tr->trDuration;    /* 0x30006142: retained */
        int32_t startTime = tr->trTime;       /* 0x30006145 */
        int32_t endTime =
            coduo_int32_from_bits((uint32_t)duration + (uint32_t)startTime);
        int32_t clampedTime = atTime;
        WIDE_FLOAT speed;
        float deceleration;
        WIDE_FLOAT decelDistance;

        if (clampedTime > endTime) {
            clampedTime = endTime;
        }
        elapsedMs =
            coduo_int32_from_bits((uint32_t)clampedTime - (uint32_t)startTime);
        const float storedDeltaTime = (float)(
            (WIDE_FLOAT)elapsedMs * (WIDE_FLOAT)TRAJECTORY_MSEC_TO_SEC);
        deltaTime = (WIDE_FLOAT)storedDeltaTime;

        /* 0x3000616b..0x30006197: same speed/duration magnitude as accelerate. */
        speed = sqrtl(
            ((WIDE_FLOAT)tr->trDelta[0] *
                 (WIDE_FLOAT)tr->trDelta[0] +
             (WIDE_FLOAT)tr->trDelta[1] *
                 (WIDE_FLOAT)tr->trDelta[1]) +
            (WIDE_FLOAT)tr->trDelta[2] *
                (WIDE_FLOAT)tr->trDelta[2]);
        deceleration = (float)(
            speed /
            ((WIDE_FLOAT)duration *
             (WIDE_FLOAT)TRAJECTORY_MSEC_TO_SEC));
        (void)VectorNormalize2(tr->trDelta, result);

        /* 0x300061a2..0x300061fb: v*t plus -0.5*a*t^2 along unit trDelta.
         * 0x300061c0: the [2] linear term is rounded to a float slot before
         * decelDistance is computed; the [0]/[1] terms stay in registers. */
        const WIDE_FLOAT linearX =
            (WIDE_FLOAT)tr->trBase[0] +
            deltaTime * (WIDE_FLOAT)tr->trDelta[0];
        const WIDE_FLOAT linearY =
            (WIDE_FLOAT)tr->trBase[1] +
            deltaTime * (WIDE_FLOAT)tr->trDelta[1];
        const float linearZ = (float)(
            (WIDE_FLOAT)tr->trBase[2] +
            deltaTime * (WIDE_FLOAT)tr->trDelta[2]);
        decelDistance =
            (WIDE_FLOAT)deceleration * deltaTime * deltaTime *
            (WIDE_FLOAT)-0.5f;
        const float storedDecelDistance = (float)decelDistance;
        result[0] = (float)(linearX +
                            decelDistance * (WIDE_FLOAT)result[0]);
        result[1] = (float)(linearY +
                            (WIDE_FLOAT)storedDecelDistance *
                                (WIDE_FLOAT)result[1]);
        result[2] = (float)((WIDE_FLOAT)linearZ +
                            (WIDE_FLOAT)storedDecelDistance *
                                (WIDE_FLOAT)result[2]);
        return;
    }

    default:
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        BG_TRAJECTORY_ERROR(
            "\x15" "BG_EvaluateTrajectory: unknown trType: %i",
            tr->trTime);
        return;
    }
}

void BG_EvaluateTrajectoryDelta(const trajectory_t *trajectory, int32_t atTime,
                                vec3_t result)
{
    WIDE_FLOAT scale;
    trType_t trType = trajectory->trType; /* 0x30006252: retained in ESI */

    switch (trType) {
    case TR_STATIONARY:
    case TR_INTERPOLATE:
        break;
    case TR_LINEAR:
        result[0] = trajectory->trDelta[0];
        result[1] = trajectory->trDelta[1];
        result[2] = trajectory->trDelta[2];
        return;
    case TR_LINEAR_STOP: {
        int32_t duration = trajectory->trDuration; /* 0x30006294 */
        int32_t endTime = coduo_int32_from_bits(
            (uint32_t)duration + (uint32_t)trajectory->trTime);
        if (atTime <= endTime) {
            result[0] = trajectory->trDelta[0];
            result[1] = trajectory->trDelta[1];
            result[2] = trajectory->trDelta[2];
            return;
        }
        break;
    }
    case TR_SINE: {
        int32_t elapsedMs = coduo_int32_from_bits(
            (uint32_t)atTime - (uint32_t)trajectory->trTime);
        /* 0x3000627a..0x3000628f: FILD/FIDIV/FMUL/FCOS/FMUL remains
         * unstored and feeds all three component multiplies through ST0. */
        scale = coduo_x87_cosl(
            ((WIDE_FLOAT)elapsedMs /
             (WIDE_FLOAT)trajectory->trDuration) *
            (WIDE_FLOAT)TRAJECTORY_DELTA_FULL_CIRCLE) *
            (WIDE_FLOAT)TRAJECTORY_DELTA_SINE_SCALE;
        result[0] = (float)(scale * (WIDE_FLOAT)trajectory->trDelta[0]);
        result[1] = (float)(scale * (WIDE_FLOAT)trajectory->trDelta[1]);
        result[2] = (float)(scale * (WIDE_FLOAT)trajectory->trDelta[2]);
        return;
    }
    case TR_GRAVITY: {
        int32_t elapsedMs = coduo_int32_from_bits(
            (uint32_t)atTime - (uint32_t)trajectory->trTime);
        WIDE_FLOAT elapsed =
            (WIDE_FLOAT)elapsedMs *
            (WIDE_FLOAT)TRAJECTORY_DELTA_MSEC_TO_SEC;
        result[0] = trajectory->trDelta[0];
        result[1] = trajectory->trDelta[1];
        result[2] = (float)((WIDE_FLOAT)trajectory->trDelta[2] -
                            elapsed *
                                (WIDE_FLOAT)TRAJECTORY_DELTA_GRAVITY);
        return;
    }
    case TR_GRAVITY_LOW: {
        int32_t elapsedMs = coduo_int32_from_bits(
            (uint32_t)atTime - (uint32_t)trajectory->trTime);
        WIDE_FLOAT elapsed =
            (WIDE_FLOAT)elapsedMs *
            (WIDE_FLOAT)TRAJECTORY_DELTA_MSEC_TO_SEC;
        result[0] = trajectory->trDelta[0];
        result[1] = trajectory->trDelta[1];
        result[2] = (float)((WIDE_FLOAT)trajectory->trDelta[2] -
                            elapsed *
                                (WIDE_FLOAT)TRAJECTORY_DELTA_LOW_GRAVITY);
        return;
    }
    case TR_GRAVITY_FLOAT: {
        int32_t elapsedMs = coduo_int32_from_bits(
            (uint32_t)atTime - (uint32_t)trajectory->trTime);
        WIDE_FLOAT elapsed =
            (WIDE_FLOAT)elapsedMs *
            (WIDE_FLOAT)TRAJECTORY_DELTA_MSEC_TO_SEC;
        result[0] = trajectory->trDelta[0];
        result[1] = trajectory->trDelta[1];
        result[2] = (float)((WIDE_FLOAT)trajectory->trDelta[2] -
                            elapsed *
                                (WIDE_FLOAT)TRAJECTORY_DELTA_FLOAT_GRAVITY);
        return;
    }
    case TR_ACCELERATE: {
        int32_t startTime = trajectory->trTime;       /* 0x30006337 */
        int32_t duration = trajectory->trDuration;    /* 0x3000633a */
        int32_t endTime = coduo_int32_from_bits(
            (uint32_t)startTime + (uint32_t)duration);
        if (atTime <= endTime) {
            int32_t elapsedMs = coduo_int32_from_bits(
                (uint32_t)atTime - (uint32_t)startTime);
            scale = (WIDE_FLOAT)elapsedMs *
                    (WIDE_FLOAT)TRAJECTORY_DELTA_MSEC_TO_SEC;
            scale *= scale;
            result[0] = (float)(scale * (WIDE_FLOAT)trajectory->trDelta[0]);
            result[1] = (float)(scale * (WIDE_FLOAT)trajectory->trDelta[1]);
            result[2] = (float)(scale * (WIDE_FLOAT)trajectory->trDelta[2]);
            return;
        }
        break;
    }
    case TR_DECCELERATE: {
        int32_t startTime = trajectory->trTime;       /* 0x3000635d */
        int32_t duration = trajectory->trDuration;    /* 0x30006360 */
        int32_t endTime = coduo_int32_from_bits(
            (uint32_t)startTime + (uint32_t)duration);
        if (atTime <= endTime) {
            int32_t elapsedMs = coduo_int32_from_bits(
                (uint32_t)atTime - (uint32_t)startTime);
            scale = (WIDE_FLOAT)elapsedMs *
                    (WIDE_FLOAT)TRAJECTORY_DELTA_MSEC_TO_SEC;
            result[0] = (float)(scale * (WIDE_FLOAT)trajectory->trDelta[0]);
            result[1] = (float)(scale * (WIDE_FLOAT)trajectory->trDelta[1]);
            result[2] = (float)(scale * (WIDE_FLOAT)trajectory->trDelta[2]);
            return;
        }
        break;
    }
    default:
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        BG_TRAJECTORY_ERROR(
            "\x15" "BG_EvaluateTrajectoryDelta: unknown trType: %i",
            trajectory->trTime);
        return;
    }
    result[0] = result[1] = result[2] = 0.0f;
}
