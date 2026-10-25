// Source: uo_cgame_mp_x86.dll 0x300154d0..0x30015601
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300154d0_30015601.mcode
//
// BG_CalculateWeaponPosition_DamageKick — the view-kick angle-component contributor
// accumulated by BG_CalculateWeaponAngles (0x30015920), which calls it
// fourth in the chain (idle, ads-spread, sway, VIEWKICK, weapon-idle) with the running
// angle vector `angles` in ECX and the pm_weapon_angle_state_t block in EDX (see the declaration
// in client_recovered.h and the call site 0x300159d7: MOV ECX,EDI / MOV EDX,ESI).
//
// The .mcode header's size-guess name `CalculateRanks` (win size 0x131 == matched size
// 0x131) is REJECTED: this function does no client-list iteration, no score sort, and
// no rank assignment. It reads state->ps and the current weapon's weaponInfo_t, computes
// an eased view-kick envelope from the elapsed kick age, and folds a pitch/yaw/roll
// contribution into `angles`. Name is adopted from the already-reconstructed caller and
// the same-module BG_CalculateWeaponPosition_* family; the exact source suffix is
// provisional but the role (view-kick angle contributor) is proven by the fields it
// reads (state->viewKickStartTime/viewKickPitch/viewKickYaw at +0x20/+0x24/+0x28, which
// match the server pm_weapon_angle_state_s).
//
// Register ABI (fastcall-shaped, proven at the caller and by RET with no imm): angles in
// ECX (vec3, accumulated into — not overwritten), state in EDX; no stack arguments. The
// SUB ESP,8 scratch region and the EDI/ESI push/pop are i386 frame detail, expressed
// here as two local float temporaries and plain locals.
//
// x87 self-check notes (top-of-stack model verified instruction-by-instruction):
//   A       = (adsFraction + 1.0f) * 0.5f                 (0x300154e2..0x300154f2)
//   B       = 100.0f * A                                  (kept live on the x87 stack;
//                                                          FLD 100.0f @0x300715ec,
//                                                          FMUL ST(1))
//   attackW = 400.0f * A  (== 4*B), stored to a scratch slot for the decay branch
//   Small float constants dumped from .rdata by exact address:
//     0x3007bce0 = 1.0f, 0x3007bce4 = 2.0f, 0x3007bce8 = 0.5f, 0x3007bcec = 0.0f,
//     0x300715ec = 100.0f, 0x3007bf58 = 400.0f, 0x3007be38 = 0.75f.
//   The two |x| steps are FST-to-memory then AND 0x7fffffff on the stored bits
//     (clear sign bit) — modeled as fabsf on the just-stored float value.
//   Two 4-byte stack scratch slots are reused across branches; because the ESI POP
//   between them shifts ESP by 4, the [esp+8] read in the decay branch (0x30015586)
//   aliases the SAME slot that held attackW (400.0f*A), NOT the integer kick age.

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "math/q_math.h"

#include <math.h>


void BG_CalculateWeaponPosition_DamageKick(pm_weapon_angle_state_t *state,
                                           vec3_t angles)
{
    /* 0x300154d4..0x300154d9: skip the whole contribution when no kick is active
     * (viewKickStartTime == 0). `angles` is left untouched. */
    int32_t viewKickStartTime = state->viewKickStartTime;
    if (viewKickStartTime == 0) {
        return;
    }

    playerState_t *ps = state->ps;               /* 0x300154e0: ESI = [EDX+0x00] */

    /* 0x300154e2..0x300154f2: A = (adsFraction + 1.0f) * 0.5f — the ADS-widened kick
     * scale (1x at hip, up to 1.5x while fully aimed down sight since adsFraction<=1). */
    float ads = ps->adsFraction;                /* [ESI+0xe0] */
    long double scale =
        ((long double)ads + (long double)1.0f) * (long double)0.5f;

    /* 0x300154f4..0x300154fa: attackDuration = 100.0f * A. Kept live on the x87 stack
     * as the ramp-up window that the elapsed kick age is compared against. */
    long double attackDuration = (long double)100.0f * scale;

    /* 0x300154fc..0x30015504: attackWindow = 400.0f * A, stashed for the decay branch. */
    float attackWindow = (float)((long double)400.0f * scale);

    /* 0x30015508..0x3001554a: when the player is aiming down sight (adsFraction != 0.0f)
     * and the current weapon has its overlay/reticle ADS-reduce gate set, shrink the
     * kick scale by the ADS fraction: scale *= (1.0f - 0.75f * adsFraction). The
     * FUCOMPP against 0.0f takes the skip (JNP) only on exact equality, so any nonzero
     * adsFraction (including negative, though it is a 0..1 fraction) enters the block. */
    if (ps->adsFraction != 0.0f) {
        int32_t currentWeapon = ps->currentWeapon;
        weaponInfo_t **weaponTable = bg_weaponInfos;
        weaponInfo_t *weaponInfo = weaponTable[currentWeapon]; /* [0x30134cd8 + [ps+0xd8]*4] */
        if (weaponInfo->adsOverlayReticle != 0) {                 /* [wi+0x284] */
            scale *= (long double)1.0f -
                     (long double)0.75f *
                         (long double)ps->adsFraction;
        }
    }

    /* 0x3001554c..0x30015551: kickAge = time - viewKickStartTime (signed ms), formed in
     * integer registers then FILD'd to float. */
    int32_t kickAgeMs = coduo_int32_from_bits(
        (uint32_t)state->time - (uint32_t)viewKickStartTime);
    long double kickAge = (long double)kickAgeMs;

    /* 0x3001555a..0x30015561: FCOM kickAge vs attackDuration. The JP branch (to the
     * decay path) is taken unless kickAge < attackDuration; i.e. the ramp-up (attack)
     * path runs while the kick is younger than its attack window. */
    long double envelope;
    if (kickAge < attackDuration) {
        /* 0x30015563..0x30015582: attack ramp.
         *   r = kickAge / attackDuration        (FDIVRP: kickAge/attackDuration)
         *   envelope = (2.0f - |r|) * r          (eases 0 -> 1 as r goes 0 -> 1) */
        const long double r = kickAge / attackDuration;
        const float storedR = (float)r;
        envelope = ((long double)2.0f -
                    (long double)fabsf(storedR)) * r;
    } else {
        /* 0x30015584..0x300155c0: decay ramp.
         *   q = (kickAge - attackDuration) / attackWindow
         * The kick is fully over once q >= 1.0f (1.0f - q <= 0.0f): abort with `angles`
         * untouched (0x30015590 FCOM 0.0f / TEST AH,0x41 / JNE 0x300155f8).
         * The same branch also rejects an unordered (NaN) result. */
        const long double remaining =
            (long double)1.0f -
            (kickAge - attackDuration) / (long double)attackWindow;
        if (!(remaining > (long double)0.0f)) {
            return;
        }
        /* 0x3001559d..0x300155c0:
         *   envelope = 1.0f - (2.0f - |q|) * q   (eases 1 -> 0 as q goes 0 -> 1) */
        const long double q = (long double)1.0f - remaining;
        const float storedQ = (float)q;
        envelope = (long double)1.0f -
            ((long double)2.0f - (long double)fabsf(storedQ)) * q;
    }

    /* 0x300155c6: fold the eased envelope into the kick scale. */
    const long double kick = scale * envelope;

    /* 0x300155cb..0x300155f1: distribute the kick into the running angle vector.
     *   angles[0] += 0.5f * kick * viewKickPitch    (pitch, halved)
     *   angles[1] -= kick * viewKickYaw             (yaw, full, subtracted)
     *   angles[2] += 0.5f * kick * viewKickYaw      (roll from yaw, halved)
     * viewKickPitch = state->+0x24, viewKickYaw = state->+0x28. */
    angles[0] = (float)((long double)angles[0] +
                     (long double)0.5f * kick *
                         (long double)state->viewKickPitch);
    angles[1] = (float)((long double)angles[1] -
                     kick * (long double)state->viewKickYaw);
    angles[2] = (float)((long double)angles[2] +
                     (long double)0.5f * kick *
                         (long double)state->viewKickYaw);
}
