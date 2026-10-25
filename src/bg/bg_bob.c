#include "bg_bob.h"

#include "bg_bob_binding.h"
#include "compat/coduo_native_x87.h"


#include <stdint.h>


/*
 * Complete BG bob-wave subsystem.  The Windows cgame/game bodies are
 * instruction-identical after relocating constants and cvar globals:
 *
 *   uo_cgame_mp_x86.dll  0x30014db0, 0x30014dd0, 0x30014e50
 *   uo_game_mp_x86.dll   0x20014cf0, 0x20014d10, 0x20014d90
 *
 * Linux game retains the same state tests and waveforms at RVAs 0x00037b2d,
 * 0x00037b7c, and 0x00037c5f.  Its original source/compiler boundary stores
 * the cycle/vertical results and intermediate sine terms as binary32. Its
 * horizontal product remains live in x87; Windows returns live PC=53 values.
 * The maintained body follows Windows and retains native wide intermediates.
 */


long double BG_GetBobCycle(const playerState_t *ps)
{
    return (long double)(uint8_t)ps->bobCycle * 0.0246399436f +
           6.2831854820f;
}

long double BG_GetVerticalBobFactor(const playerState_t *ps, float phase,
                                    float amplitude, float maxAmplitude)
{
    float stanceScale;
    float clampedAmplitude;

    if (ps->viewHeightTarget == ps->proneViewHeight) {
        stanceScale = BG_BOB_AMPLITUDE_PRONE;
    } else if (ps->viewHeightTarget == ps->crouchViewHeight) {
        stanceScale = BG_BOB_AMPLITUDE_DUCKED;
    } else {
        stanceScale = BG_BOB_AMPLITUDE_STANDING;
    }

    const long double amplitudeWide =
        (long double)stanceScale * (long double)amplitude;
    clampedAmplitude = (float)amplitudeWide;
    if (amplitudeWide > (long double)maxAmplitude) {
        clampedAmplitude = maxAmplitude;
    }

    const long double harmonic =
        coduo_x87_sinl((long double)phase * 4.0f + 1.5707964f) * 0.2f;
    const long double fundamental =
        coduo_x87_sinl((long double)phase + (long double)phase);
    return (harmonic + fundamental) * clampedAmplitude * 0.75f;
}

long double BG_GetHorizontalBobFactor(const playerState_t *ps, float phase,
                                      float amplitude, float maxAmplitude)
{
    float stanceScale;

    if (ps->viewHeightTarget == ps->proneViewHeight) {
        stanceScale = BG_BOB_AMPLITUDE_PRONE;
    } else if (ps->viewHeightTarget == ps->crouchViewHeight) {
        stanceScale = BG_BOB_AMPLITUDE_DUCKED;
    } else {
        stanceScale = BG_BOB_AMPLITUDE_STANDING;
    }

    long double clampedAmplitude =
        (long double)stanceScale * (long double)amplitude;
    if (clampedAmplitude > (long double)maxAmplitude) {
        clampedAmplitude = maxAmplitude;
    }
    return coduo_x87_sinl((long double)phase) * clampedAmplitude;
}


#undef BG_BOB_AMPLITUDE_STANDING
#undef BG_BOB_AMPLITUDE_DUCKED
#undef BG_BOB_AMPLITUDE_PRONE
