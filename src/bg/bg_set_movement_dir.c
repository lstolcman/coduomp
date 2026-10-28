#include "compat/crt/sqrt_compat.h"
#include "bg_pmove.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "math/q_math.h"


#include <math.h>
#include <stdint.h>

enum {
    PM_MOVEMENT_DIR_LIMIT = 90
};

#define PM_MOVEMENT_DIR_DISTANCE_FACTOR 5.0f
#define PM_MOVEMENT_DIR_YAW_FLIP 180.0f

/*
 * Canonical Quake3/CoD post-move direction helper.  The two authoritative
 * Windows module bodies are instruction-identical apart from relocated globals
 * and callees:
 *
 *   uo_cgame_mp_x86.dll  0x300089f0..0x30008bfe
 *   uo_game_mp_x86.dll   0x200087a0..0x200089ae
 *
 * Linux game retains the same prone, ladder, displacement, backpedal, clamp,
 * signed-byte, and zero-result decisions at 0x00023f62..0x000242b1.  Its only
 * material arithmetic difference here is the displacement length: Windows
 * passes the live z+y+x x87 sum to the CRT square root, whereas Linux stores its
 * x+y+z sum as binary64 for glibc sqrt and then stores the result as binary32.
 */
void PM_SetMovementDir(void)
{
    playerState_t *const ps = pm->ps;
    const uint32_t playerStateFlags = ps->playerStateFlags;

    if ((playerStateFlags & PMF_PRONE) != 0 &&
        (ps->entityStateFlags & EF_RESTRICTED_MASK) == 0) {
        int32_t direction = coduo_fp_to_i32_extended(
            AngleDelta(ps->proneDirection, ps->viewAngles[1]));
        const uint32_t sign = (uint32_t)-(direction < 0);
        const int32_t magnitude = coduo_int32_from_bits(
            ((uint32_t)direction ^ sign) - sign);

        if (magnitude > PM_MOVEMENT_DIR_LIMIT) {
            direction = direction > 0 ? PM_MOVEMENT_DIR_LIMIT
                                      : -PM_MOVEMENT_DIR_LIMIT;
        }
        ps->movementDir = (int32_t)(int8_t)direction;
        return;
    }

    if ((playerStateFlags & PMF_LADDER) != 0) {
        const float ladderYaw = (float)(
            (long double)vectoyaw(ps->ladderNormal) +
            (long double)PM_MOVEMENT_DIR_YAW_FLIP);
        int32_t direction = coduo_fp_to_i32_extended(
            AngleDelta(ladderYaw, ps->viewAngles[1]));
        const uint32_t sign = (uint32_t)-(direction < 0);
        const int32_t magnitude = coduo_int32_from_bits(
            ((uint32_t)direction ^ sign) - sign);

        if (magnitude > PM_MOVEMENT_DIR_LIMIT) {
            direction = direction > 0 ? PM_MOVEMENT_DIR_LIMIT
                                      : -PM_MOVEMENT_DIR_LIMIT;
        }
        ps->movementDir = (int32_t)(int8_t)direction;
        return;
    }

    {
        vec3_t displacement;
        const int8_t forwardMove = pm->command.forwardmove;

        displacement[0] = (float)((long double)ps->psOrigin[0] -
                                  (long double)pml.previousOrigin[0]);
        displacement[1] = (float)((long double)ps->psOrigin[1] -
                                  (long double)pml.previousOrigin[1]);
        displacement[2] = (float)((long double)ps->psOrigin[2] -
                                  (long double)pml.previousOrigin[2]);

        if ((forwardMove == 0 && pm->command.rightmove == 0) ||
            ps->groundEntityNum == ENTITYNUM_NONE) {
            ps->movementDir = 0;
            return;
        }

        float distance;
        distance = (float)coduomp_crt_sqrtl(
            ((long double)displacement[2] *
                 (long double)displacement[2] +
             (long double)displacement[1] *
                 (long double)displacement[1]) +
            (long double)displacement[0] *
                (long double)displacement[0]);

        if (distance == 0.0f ||
            !((long double)pml.frametime *
                  (long double)PM_MOVEMENT_DIR_DISTANCE_FACTOR <
              (long double)distance)) {
            ps->movementDir = 0;
            return;
        }

        {
            vec3_t directionVector;
            vec3_t directionAngles;
            int32_t direction;

            (void)VectorNormalize2(displacement, directionVector);
            vectoangles(directionVector, directionAngles);
            direction = coduo_fp_to_i32_extended(
                AngleDelta(directionAngles[1], ps->viewAngles[1]));

            if (forwardMove < 0) {
                direction = coduo_fp_to_i32_extended(
                    AngleNormalize180(
                        (float)((long double)direction +
                                (long double)PM_MOVEMENT_DIR_YAW_FLIP)));
            }

            const uint32_t sign = (uint32_t)-(direction < 0);
            const int32_t magnitude = coduo_int32_from_bits(
                ((uint32_t)direction ^ sign) - sign);
            if (magnitude > PM_MOVEMENT_DIR_LIMIT) {
                direction = direction > 0 ? PM_MOVEMENT_DIR_LIMIT
                                          : -PM_MOVEMENT_DIR_LIMIT;
            }
            ps->movementDir = (int32_t)(int8_t)direction;
        }
    }
}
