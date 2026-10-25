#include "bg_animation.h"

#include "compat/coduo_int32_bits.h"
#include "math/q_math.h"

#include <stdint.h>
#include <string.h>


/*
 * The Windows cgame/game bodies are instruction-identical after relocated
 * constants are normalized:
 *
 *   BG_LerpAngles  0x300055b0 / 0x200053e0
 *   BG_LerpOffset  0x30005670 / 0x200054a0
 *
 * Supporting Mac cgame/game retain both canonical names with equal sizes and
 * call graphs. Linux game retains BG_LerpAngles at RVA 0x0001f107 and
 * BG_LerpOffset at RVA 0x0001f1e7. The latter contains a genuine malformed
 * Q_rsqrt call/return ABI, so BG_LerpOffset has whole-function platform bodies.
 */

void BG_LerpAngles(const vec3_t target, float maxStep, vec3_t current)
{
    int32_t lane;

    for (lane = 0; lane < 3; ++lane) {
        const double delta = (double)target[lane] - current[lane];

        if (delta > maxStep) {
            current[lane] = (float)((double)current[lane] + maxStep);
        } else if (delta < -(double)maxStep) {
            current[lane] = (float)((double)current[lane] - maxStep);
        } else {
            current[lane] = target[lane];
        }
    }
}

void BG_LerpOffset(const vec3_t target, float scale, vec3_t current)
{
    const float zero = 0.0f;
    const float half = 0.5f;
    const float one = 1.0f;
    const float threeHalves = 1.5f;
    float delta[3];
    uint32_t bits;
    float estimate;

    double delta2Wide;
    double lengthSquared;
    double factor;

    delta[0] = (float)((double)target[0] - current[0]);
    delta[1] = (float)((double)target[1] - current[1]);
    delta2Wide = (double)target[2] - current[2];
    delta[2] = (float)delta2Wide;
    lengthSquared =
        (delta2Wide * delta[2] + (double)delta[1] * delta[1]) +
        (double)delta[0] * delta[0];
    if (lengthSquared == zero) {
        return;
    }

    {
        const float storedLengthSquared = (float)lengthSquared;

        memcpy(&bits, &storedLengthSquared, sizeof(bits));
    }
    bits = UINT32_C(0x5f3759df) - coduo_int32_sar_bits(bits, 1U);
    memcpy(&estimate, &bits, sizeof(estimate));

    factor = ((double)threeHalves -
              ((lengthSquared * half * (double)estimate) * estimate)) *
             (double)estimate;
    factor *= scale;
    if (!(factor < one)) {
        current[0] = target[0];
        current[1] = target[1];
        current[2] = target[2];
        return;
    }

    current[0] =
        (float)((double)current[0] + (double)delta[0] * factor);
    current[1] =
        (float)((double)current[1] + (double)delta[1] * factor);
    current[2] =
        (float)((double)current[2] + (double)delta[2] * factor);
}
