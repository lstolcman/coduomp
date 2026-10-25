#include "bg_weapon_position.h"

#include "bg_weapon.h"


/*
 * The Windows cgame and game bodies are instruction-identical apart from the
 * weapon-table relocation (0x30015190 and 0x200150d0).  The Linux game body at
 * RVA 0x381e9 retains the same field accesses, arithmetic, and helper call.
 * The Mac cgame/game symbols independently preserve the canonical name.
 */
void BG_CalculateWeaponPosition_BaseAngles(pm_weapon_angle_state_t *state,
                                           vec3_t angles)
{
    playerState_t *ps = state->ps;
    const weaponInfo_t *weapon = BG_GetInfoForWeapon(ps->currentWeapon);

    if (weapon->adsEnabled != 0) {
        angles[0] = (float)(
            (long double)weapon->adsPitchOffset *
                (long double)ps->adsFraction +
            (long double)angles[0]);
    }
    BG_CalculateWeaponPosition_BasePosition_angles(state, angles);
}
