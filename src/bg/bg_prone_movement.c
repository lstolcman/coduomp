#include "bg_pmove.h"

#include "bg_player_state.h"
#include "math/q_math.h"

#include <stddef.h>

/*
 * Shared prone movement cluster.  The Windows cgame/game bodies are
 * instruction-identical apart from relocations and dependency addresses:
 *
 *   uo_cgame_mp_x86.dll  BG_CheckProne       0x30008130
 *   uo_game_mp_x86.dll   BG_CheckProne       0x20007ee0
 *   uo_cgame_mp_x86.dll  BG_CheckProneTurned 0x3000c800
 *   uo_game_mp_x86.dll   BG_CheckProneTurned 0x2000c5c0
 *   uo_cgame_mp_x86.dll  PM_UpdatePronePitch 0x3000d470
 *   uo_game_mp_x86.dll   PM_UpdatePronePitch 0x2000d220
 *
 * Linux game retains the same functions at RVAs 0x00022ff3, 0x0002b031,
 * and 0x0002c158.  The state gates, trace arguments, event writes, and angle
 * updates agree.  The two proven floating-point spill differences are kept at
 * their exact expression boundaries below; see the platform discrepancy note.
 */

int32_t BG_CheckProne(int32_t clientNum, const vec3_t origin,
                      float radius, float height, float yaw,
                      float *groundOffset, float *pitchDown,
                      float *pitchUp, qboolean skipInitialTrace,
                      qboolean allowFallback, const vec3_t groundNormal,
                      pm_trace_fn_t traceFunc,
                      pm_trace_fn_t traceDownFunc,
                      qboolean useAltContentMask, float proneLength,
                      pm_entity_type_fn_t entityTypeFunc)
{
    return BG_CheckProneValid(
        clientNum, origin, radius, height, yaw,
        groundOffset, pitchDown, pitchUp,
        skipInitialTrace, allowFallback, groundNormal,
        traceFunc, traceDownFunc, useAltContentMask, proneLength,
        qfalse, entityTypeFunc);
}

int32_t BG_CheckProneTurned(playerState_t *ps, float yaw,
                            pm_trace_fn_t traceFunc)
{
    const volatile float delta = AngleDelta(yaw, ps->viewAngles[1]);
    volatile float fraction;
    float checkYaw;
    float proneLength;

    fraction = (float)((long double)PM_FloatAbs(delta) / 240.0L);
    {
        const volatile float complement =
            (float)(1.0L - (long double)fraction);
        checkYaw = AngleNormalize360Accurate((float)(
            (long double)yaw -
            (long double)complement * (long double)delta));
        proneLength = (float)(
            (long double)complement * 60.0L +
            (long double)fraction * 45.0L);
    }

    return BG_CheckProne(
        ps->psClientNum, ps->psOrigin, ps->playerMaxs[0], 30.0f,
        checkYaw, &ps->torsoHeight, &ps->torsoPitch, &ps->waistPitch,
        qtrue, ps->groundEntityNum != ENTITYNUM_NONE, NULL, traceFunc,
        NULL, qfalse, proneLength, NULL);
}

void PM_UpdatePronePitch(void)
{
    const float *groundNormal;
    float targetPitch;

    if ((pm->ps->playerStateFlags & PMF_PRONE) == 0) {
        return;
    }

    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        groundNormal = pml.groundPlane == 0
                           ? NULL
                           : pml.groundTrace.normal;
        if (BG_CheckProne(
                pm->ps->psClientNum, pm->ps->psOrigin,
                pm->ps->playerMaxs[0], 30.0f, pm->ps->proneDirection,
                &pm->ps->torsoHeight, &pm->ps->torsoPitch,
                &pm->ps->waistPitch, qtrue,
                pm->ps->groundEntityNum != ENTITYNUM_NONE,
                groundNormal, pm->trace3, pm->trace2, qfalse, 60.0f,
                pm->entityType) == 0 ||
            pm->waterlevel != 0) {
            BG_AddPredictableEventToPlayerstate(
                EV_STANCE_FORCE_CROUCH, 0, pm->ps);
            pm->ps->playerStateFlags |= PMF_PRONE_BLOCKED;
        }
    } else if (pml.groundPlane != 0 &&
               pml.groundTrace.normal[2] < 0.7f) {
        BG_AddPredictableEventToPlayerstate(
            EV_STANCE_FORCE_CROUCH, 0, pm->ps);
    }

    targetPitch = pml.groundPlane == 0
                      ? 0.0f
                      : PitchForYawOnNormal(
                            pm->ps->proneDirection,
                            pml.groundTrace.normal);
    {
        const volatile float delta =
            AngleDelta(targetPitch, pm->ps->proneDirectionPitch);

        if (delta != 0.0f) {
            const volatile float maxStep =
                (float)((long double)pml.frametime * 70.0L);
            if ((long double)PM_FloatAbs(delta) >
                (long double)maxStep) {
                const long double updateStep = (long double)maxStep;
                pm->ps->proneDirectionPitch = (float)(
                    (long double)pm->ps->proneDirectionPitch +
                    (long double)PM_FloatSign(delta) *
                        updateStep);
            } else {
                pm->ps->proneDirectionPitch = (float)(
                    (long double)pm->ps->proneDirectionPitch +
                    (long double)delta);
            }
            pm->ps->proneDirectionPitch = AngleNormalize180Accurate(
                pm->ps->proneDirectionPitch);
        }
    }

    targetPitch = pml.groundPlane == 0
                      ? 0.0f
                      : PitchForYawOnNormal(
                            pm->ps->viewAngles[1],
                            pml.groundTrace.normal);
    {
        const volatile float delta =
            AngleDelta(targetPitch, pm->ps->proneTorsoPitch);

        if (delta != 0.0f) {
            const volatile float maxStep =
                (float)((long double)pml.frametime * 70.0L);
            if ((long double)PM_FloatAbs(delta) >
                (long double)maxStep) {
                const long double updateStep = (long double)maxStep;
                pm->ps->proneTorsoPitch = (float)(
                    (long double)pm->ps->proneTorsoPitch +
                    (long double)PM_FloatSign(delta) *
                        updateStep);
            } else {
                pm->ps->proneTorsoPitch = (float)(
                    (long double)pm->ps->proneTorsoPitch +
                    (long double)delta);
            }
            pm->ps->proneTorsoPitch = AngleNormalize180Accurate(
                pm->ps->proneTorsoPitch);
        }
    }
}
