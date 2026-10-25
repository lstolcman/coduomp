// Sources: uo_cgame_mp_x86.dll 0x30009d30..0x3000a133,
//          uo_game_mp_x86.dll  0x20009ae0..0x20009ee9,
//          game.mp.uo.i386.so  0x00025cb4..0x000262b6
//
// PM_CrashLand -- process a pmove landing, including fall damage, landing stun,
// animation, velocity damping, and predictable landing events. The assigned
// G_SpawnTurret name is a size-only collision with a server function and is
// rejected: this body operates exclusively on player-state/pmove landing state.
//
// ABI: cdecl, no arguments; all state comes from pm and pml. Windows retains
// the initial fall-height solve as one live x87 chain under PC=53. Linux stores
// the named intermediates to binary32 and calls its binary64 libm sqrt under
// PC=64; complete platform bodies preserve that genuine source realization.

#include "bg_pmove.h"

#include "bg_player_state.h"
#include "bg_pmove_services.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_native_x87.h"


#include <math.h>
#include <stdint.h>

void Com_Printf(const char *format, ...);


#define PM_CRASH_DAMAGE_SCALE                100.0f
#define PM_CRASH_WATERLEVEL_TWO_SCALE        0.5f
#define PM_CRASH_EVENT_HEIGHT_BASE           12.0f
#define PM_CRASH_EVENT_HEIGHT_SCALE          0.03846154f
#define PM_CRASH_EVENT_PARM_BIAS             1.0f
#define PM_CRASH_EVENT_PARM_SCALE            4.0f
#define PM_CRASH_EVENT_PARM_MAX              24
#define PM_CRASH_MINOR_HEIGHT                4.0f
#define PM_CRASH_MEDIUM_HEIGHT               8.0f
#define PM_CRASH_VELOCITY_DAMP               0.67000002f
#define PM_CRASH_STUN_DAMAGE_MSEC            35
#define PM_CRASH_STUN_BASE_MSEC              500
#define PM_CRASH_STUN_RAMP_END_MSEC          1500
#define PM_CRASH_STUN_MAX_MSEC               2000
#define PM_CRASH_STUN_SPEED_HIGH             0.5f
#define PM_CRASH_STUN_SPEED_LOW              0.2f
#define PM_CRASH_STUN_MSEC_TO_SEC            0.001f
#define PM_CRASH_STUN_SPEED_SPAN             0.30000001f

enum {
    PM_CRASH_WATERLEVEL_TWO = 2,
    PM_CRASH_WATERLEVEL_FULL = 3
};

void PM_CrashLand(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    float gravity;
    float previousVerticalVelocity;
    /* The whole fall-height solve (0x30009d47..0x30009db7) is ONE x87 chain: the
     * DLL never stores displacement, discriminant, impactTime or landingVelocity to
     * a float slot -- discriminant is compared in-register (FCOM 0.0 at 0x30009d77),
     * and landingVelocity is kept on the stack for the debug print. Only gravity
     * (FST-kept at [ESP+8], and an exact integer) and fallHeight (FSTP [ESP+4] at
     * 0x30009db7) round. Carry the intermediates through native long double
     * on every host so `float`
     * locals do not introduce stores absent from the DLL. */
    long double landingVelocity;
    float fallHeight;
    int32_t fallDamage = 0;
    int32_t landingEventParm = 0;

    /* 0x30009d3a: fully submerged players take no landing action. */
    if (move->waterlevel == PM_CRASH_WATERLEVEL_FULL) {
        return;
    }

    /*
     * 0x30009d47..0x30009db7: solve z(t)=currentZ for constant acceleration
     * -gravity, retain the downward landing-speed magnitude, then convert its
     * kinetic term to an equivalent fall height. The discriminant comparison is
     * strictly `< 0`; unordered values continue, matching the x87 branch.
     */
    previousVerticalVelocity = pml.previousVelocity[2];
    const long double gravityWide = (long double)ps->gravity;
    const long double negativeGravity = -gravityWide;
    const long double halfNegativeGravity =
        0.5L * negativeGravity;
    const long double previousVelocityRegister =
        (long double)previousVerticalVelocity;
    const long double fallDistance =
        (long double)pml.previousOrigin[2] -
        (long double)ps->psOrigin[2];
    const long double discriminant =
        previousVelocityRegister * previousVelocityRegister -
        (fallDistance * halfNegativeGravity) * 4.0L;

    gravity = (float)gravityWide;
    if (discriminant < 0.0L) {
        return;
    }

    const long double impactTime =
        (-previousVelocityRegister - coduo_x87_sqrtl(discriminant)) /
        (halfNegativeGravity + halfNegativeGravity);
    landingVelocity =
        -(previousVelocityRegister + impactTime * negativeGravity);
    /* 0x30009daf reloads the rounded FST copy of gravity for this denominator. */
    fallHeight =
        (float)((landingVelocity * landingVelocity) /
                (2.0L * (long double)gravity));

    if (move->debugMove != 0) {
        Com_Printf("landing vel: %.1f fall height: %.1f\n",
                   (double)landingVelocity, (double)fallHeight);
        move = pm;
    }

    /*
     * 0x30009de2..0x30009e87: map the configured [min,max] fall-height envelope
     * to integer damage [0,100]. The game module also warns for invalid thresholds;
     * excluded pmTypes and the no-fall-damage state suppress the result.
     */
    float maxHeight = bg_fallDamageMaxHeight.value;
    float minHeightForRange = bg_fallDamageMinHeight.value;
    if (maxHeight <= minHeightForRange || bg_fallDamageMinHeight.value < 0.0f) {
        BG_PM_FALL_DAMAGE_WARNING("bg_fallDamageMaxHeight and bg_fallDamageMinHeight "
                                  "have bad values\n");
        move = pm;
    } else {
        float minHeightForFall = bg_fallDamageMinHeight.value;
        if (!(fallHeight <= minHeightForFall) &&
            ((uint8_t)pml.groundTrace.surfaceFlags &
             SURF_NODAMAGE) == 0) {
            playerState_t *damagePs = move->ps;
            if (damagePs->pmType < PM_TYPE_DEAD) {
                float maxHeightForCap = bg_fallDamageMaxHeight.value;
                if (fallHeight >= maxHeightForCap) {
                    fallDamage = 100;
                } else {
                    float minHeightForNumerator = bg_fallDamageMinHeight.value;
                    long double numerator =
                        (long double)fallHeight - minHeightForNumerator;
                    float maxHeightForDenominator =
                        bg_fallDamageMaxHeight.value;
                    float minHeightForDenominator =
                        bg_fallDamageMinHeight.value;
                    long double denominator =
                        (long double)maxHeightForDenominator
                        - minHeightForDenominator;
                    fallDamage = coduo_fp_to_i32_extended(
                        numerator / denominator * PM_CRASH_DAMAGE_SCALE);
                    if (fallDamage < 0) {
                        fallDamage = 0;
                    } else if (fallDamage > 100) {
                        fallDamage = 100;
                    }
                }
            }
        }
    }

    /* 0x30009e8b..0x30009ea3: waist-deep water halves and re-rounds damage. */
    if (move->waterlevel == PM_CRASH_WATERLEVEL_TWO) {
        /* fallDamage enters via bare FILD (0x30009e94) -- no (float) cast. */
        fallDamage = coduo_fp_to_i32_extended(
            (long double)fallDamage *
            (long double)PM_CRASH_WATERLEVEL_TWO_SCALE);
    }

    /* 0x30009ea5..0x30009ee7: derive the capped landing-event parameter. */
    if (!(fallHeight <= PM_CRASH_EVENT_HEIGHT_BASE)) {
        landingEventParm = coduo_fp_to_i32_extended(
            (((long double)fallHeight -
              (long double)PM_CRASH_EVENT_HEIGHT_BASE) *
                 (long double)PM_CRASH_EVENT_HEIGHT_SCALE +
             (long double)PM_CRASH_EVENT_PARM_BIAS) *
            (long double)PM_CRASH_EVENT_PARM_SCALE);
        if (landingEventParm > PM_CRASH_EVENT_PARM_MAX) {
            landingEventParm = PM_CRASH_EVENT_PARM_MAX;
        }
    }

    if (fallDamage != 0) {
        /*
         * 0x30009eef..0x30009f55: when no legs animation is active, choose and
         * run one matching command from landing event list 5.
         */
        playerState_t *landingAnimPs = move->ps; /* 0x30009ef0 reload. */
        if (landingAnimPs->legsTimer == 0) {
            /* Linux retains this source-level call. Both Windows optimizers
             * inline BG_AnimScriptEvent's event-list selection here. */
            BG_AnimScriptEvent(landingAnimPs, ANIM_EVENT_LAND,
                               qfalse, qtrue);
            move = pm;
        }

        if (move->debugMove != 0) {
            Com_Printf("falling damage: %i\n", fallDamage);
            move = pm;
        }

        if (fallDamage < 100 &&
            ((uint8_t)pml.groundTrace.surfaceFlags & SURF_SLICK) == 0) {
            int32_t stunTime = fallDamage * PM_CRASH_STUN_DAMAGE_MSEC +
                               PM_CRASH_STUN_BASE_MSEC;
            float speedScale;

            if (stunTime > PM_CRASH_STUN_MAX_MSEC) {
                stunTime = PM_CRASH_STUN_MAX_MSEC;
                speedScale = PM_CRASH_STUN_SPEED_LOW;
            } else if (stunTime <= PM_CRASH_STUN_BASE_MSEC) {
                speedScale = PM_CRASH_STUN_SPEED_HIGH;
            } else if (stunTime >= PM_CRASH_STUN_RAMP_END_MSEC) {
                speedScale = PM_CRASH_STUN_SPEED_LOW;
            } else {
                /* stunTime enters via bare FILD (0x30009fd4) -- no (float) cast;
                 * 500 stays the float constant the FSUB 500.0f loads. */
                speedScale = (float)(
                    (long double)PM_CRASH_STUN_SPEED_HIGH -
                    ((long double)stunTime -
                     (long double)PM_CRASH_STUN_BASE_MSEC) *
                        (long double)PM_CRASH_STUN_MSEC_TO_SEC *
                        (long double)PM_CRASH_STUN_SPEED_SPAN);
            }

            if (move->debugMove > 1) {
                Com_Printf("landing stun time: %i speed mult: %.2f\n",
                           stunTime, (double)speedScale);
                move = pm;
            }

            playerState_t *timerPs = move->ps;
            long double liveSpeedScale = speedScale;
            timerPs->pmTime = stunTime;
            playerState_t *flagPs = move->ps;
            flagPs->playerStateFlags |= PMF_LAND_STUN;
            playerState_t *velocityXPs = move->ps;
            velocityXPs->velocity[0] =
                (float)(liveSpeedScale * velocityXPs->velocity[0]);
            playerState_t *velocityYPs = move->ps;
            velocityYPs->velocity[1] =
                (float)((long double)speedScale * velocityYPs->velocity[1]);
            playerState_t *velocityZPs = move->ps;
            velocityZPs->velocity[2] =
                (float)((long double)speedScale * velocityZPs->velocity[2]);
        } else {
            playerState_t *velocityXPs = move->ps;
            velocityXPs->velocity[0] =
                (float)((long double)velocityXPs->velocity[0] * PM_CRASH_VELOCITY_DAMP);
            playerState_t *velocityYPs = move->ps;
            velocityYPs->velocity[1] =
                (float)((long double)velocityYPs->velocity[1] * PM_CRASH_VELOCITY_DAMP);
            playerState_t *velocityZPs = move->ps;
            velocityZPs->velocity[2] =
                (float)((long double)velocityZPs->velocity[2] * PM_CRASH_VELOCITY_DAMP);
        }

        {
            int32_t event = PM_DamageLandingForSurface();
            ps = move->ps;
            BG_AddPredictableEventToPlayerstate(event, fallDamage, ps);
        }
        return;
    }

    /*
     * 0x3000a08c..0x3000a11c: non-damaging landings select three height bands.
     * The smallest two use PM_AddEvent (implicit parm 0); the >=12-unit family
     * damps velocity and appends the explicit landingEventParm.
     */
    if (!(fallHeight > PM_CRASH_MINOR_HEIGHT)) {
        return;
    }
    if (fallHeight < PM_CRASH_MEDIUM_HEIGHT) {
        PM_AddEvent(PM_LightLandingForSurface());
        return;
    }
    if (fallHeight < PM_CRASH_EVENT_HEIGHT_BASE) {
        PM_AddEvent(PM_MediumLandingForSurface());
        return;
    }

    playerState_t *velocityXPs = move->ps;
    velocityXPs->velocity[0] =
        (float)((long double)velocityXPs->velocity[0] * PM_CRASH_VELOCITY_DAMP);
    playerState_t *velocityYPs = move->ps;
    velocityYPs->velocity[1] =
        (float)((long double)velocityYPs->velocity[1] * PM_CRASH_VELOCITY_DAMP);
    playerState_t *velocityZPs = move->ps;
    velocityZPs->velocity[2] =
        (float)((long double)velocityZPs->velocity[2] * PM_CRASH_VELOCITY_DAMP);
    {
        int32_t event = PM_HardLandingForSurface();
        ps = move->ps;
        BG_AddPredictableEventToPlayerstate(event, landingEventParm, ps);
    }
}
