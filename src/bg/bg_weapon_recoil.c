#include "bg_weapon.h"

#include "compat/crt/random_compat.h"

#include <stdint.h>


/*
 * The Windows cgame and game bodies are instruction-identical apart from
 * relocations to their weapon table, rand routine, and constants:
 *
 *   uo_cgame_mp_x86.dll  0x30016120..0x300162e5
 *   uo_game_mp_x86.dll   0x20016060..0x20016225
 *
 * Both receive playerState_t, recoil velocity, and view kick in EAX, EBX, and
 * EDI respectively.  The Linux game body at RVA 0x000396bc uses the same
 * source-level argument order through its ordinary stack ABI.  Its first
 * pointer is named gclient_t in game code, but playerState_t is its zero-offset
 * prefix and is the only portion this function reads.
 *
 * The behavioral split is genuine.  Windows rand has a 15-bit result and its
 * optimized body retains selected max-min/sample expressions in x87 before
 * their final stores.  Linux rand has a 31-bit result and the unoptimized body
 * spills each range delta and completed random result to binary32. The random
 * quotient remains live through multiplication and addition before that store.
 */


/* NOT_FROM_ORIGINAL_SOURCE: native-x87 carrier for the repeated original
 * Windows random-range expression. */
static long double bg_compat_weapon_recoil_range(float minimum,
                                                  float maximum)
{
    return (long double)minimum +
           (long double)coduo_server_rand() *
               (long double)(1.0f / 32768.0f) *
               ((long double)maximum - (long double)minimum);
}

void BG_WeaponFireRecoil(playerState_t *ps, vec2_t recoilVelocity,
                         vec3_t viewKick)
{
    const weaponInfo_t *weaponInfo = bg_weaponInfos[ps->currentWeapon];
    const float adsFraction = ps->adsFraction;

    long double viewKickYaw;
    float viewKickPitch;

    if (adsFraction == 1.0f) {
        viewKickPitch = (float)bg_compat_weapon_recoil_range(
            weaponInfo->fireViewkickAdsPitchMin,
            weaponInfo->fireViewkickAdsPitchMax);
        viewKickYaw = bg_compat_weapon_recoil_range(
            weaponInfo->fireViewkickAdsYawMin,
            weaponInfo->fireViewkickAdsYawMax);
    } else {
        viewKickPitch = (float)bg_compat_weapon_recoil_range(
            weaponInfo->fireViewkickHipPitchMin,
            weaponInfo->fireViewkickHipPitchMax);
        viewKickYaw = bg_compat_weapon_recoil_range(
            weaponInfo->fireViewkickHipYawMin,
            weaponInfo->fireViewkickHipYawMax);
    }

    viewKick[0] = -viewKickPitch;
    viewKick[1] = (float)viewKickYaw;
    viewKick[2] = (float)(viewKickYaw * (long double)-0.5f);

    long double recoilYaw;
    float recoilPitch;
    if (adsFraction > 0.0f) {
        recoilPitch = (float)bg_compat_weapon_recoil_range(
            weaponInfo->fireRecoilAdsPitchMin,
            weaponInfo->fireRecoilAdsPitchMax);
        recoilYaw = bg_compat_weapon_recoil_range(
            weaponInfo->fireRecoilAdsYawMin,
            weaponInfo->fireRecoilAdsYawMax);
    } else {
        recoilPitch = (float)bg_compat_weapon_recoil_range(
            weaponInfo->fireRecoilHipPitchMin,
            weaponInfo->fireRecoilHipPitchMax);
        recoilYaw = bg_compat_weapon_recoil_range(
            weaponInfo->fireRecoilHipYawMin,
            weaponInfo->fireRecoilHipYawMax);
    }

    recoilVelocity[0] = (float)((long double)recoilPitch +
                                (long double)recoilVelocity[0]);
    recoilVelocity[1] = (float)(recoilYaw +
                                (long double)recoilVelocity[1]);
}
