#include "q_math.h"

#include "compat/coduo_int32_bits.h"

#include <math.h>
#include <stdint.h>


enum {
    SHARED_RANDOM_MULTIPLIER = 214013,
    SHARED_RANDOM_INCREMENT = 2531011,
    SHARED_RANDOM_RESULT_SHIFT = 17,
    SHARED_RANDOM_INTEGER_SCALE_SHIFT = 15
};

#define SHARED_RANDOM_INITIAL_SEED UINT32_C(0x89abcdef)

/*
 * Complete stateful q_math random bank.  The four Windows implementations are
 * instruction-identical apart from image-local addresses:
 *
 *                              Rand_Init    flrand       irand        Q_SwayRand
 *   CoDUOMP.exe                0x00435010   0x00435020   0x00435070   0x004350a0
 *   uo_cgame_mp_x86.dll       0x3004d170   0x3004d180   0x3004d1d0   0x3004d200
 *   uo_ui_mp_x86.dll          0x40005180   0x40005190   0x400051e0   0x40005210
 *   uo_game_mp_x86.dll        0x2001a1c0   0x2001a1d0   0x2001a220   0x2001a250
 *
 * The Linux engine and game module retain the same LCG and range-selection
 * bodies at 0x0806b3dd/RVA 0x0003f31f, 0x0806b3ea/RVA 0x0003f33a, and
 * 0x0806b440/RVA 0x0003f3a3.  The seed is one target-local global, initially
 * 0x89abcdef; it is not the CRT rand state.
 */
uint32_t sharedRandSeed = SHARED_RANDOM_INITIAL_SEED;

void Rand_Init(uint32_t seed)
{
    sharedRandSeed = seed;
}

float flrand(float minimum, float maximum)
{
    float randomValue;

    sharedRandSeed =
        sharedRandSeed * SHARED_RANDOM_MULTIPLIER +
        SHARED_RANDOM_INCREMENT;
    randomValue = (float)(sharedRandSeed >> SHARED_RANDOM_RESULT_SHIFT);

    return (float)(
        (((long double)maximum - (long double)minimum) *
         (long double)randomValue / 32768.0L) +
        (long double)minimum);
}

int32_t irand(int32_t minimum, int32_t maximum)
{
    uint32_t range;
    uint32_t product;
    uint32_t result;

    sharedRandSeed =
        sharedRandSeed * SHARED_RANDOM_MULTIPLIER +
        SHARED_RANDOM_INCREMENT;
    range = (uint32_t)maximum - (uint32_t)minimum;
    product = range *
              (sharedRandSeed >> SHARED_RANDOM_RESULT_SHIFT);
    result = coduo_int32_sar_bits(
                 product, SHARED_RANDOM_INTEGER_SCALE_SHIFT) +
             (uint32_t)minimum;
    return coduo_int32_from_bits(result);
}

/*
 * Q_SwayRand has one genuine platform result difference.  The Windows images
 * store milliseconds/1000 as binary64 and multiply by the binary64 widening
 * of the float constant 6.2831855f.  The Linux images retain the division in
 * x87, multiply by binary64 pi, double the angle in x87, and only then store
 * the libm argument as binary64 (coduo_lnxded 0x0806b480 and game RVA
 * 0x0003f3f6). The maintained body follows the Windows graph using native
 * arithmetic and explicit stores.
 */
float Q_SwayRand(float sineRate, float cosineRate, float milliseconds)
{
    const double seconds =
        (double)((long double)milliseconds / 1000.0L);
    const long double sineArgument =
        (long double)sineRate * (long double)seconds *
        6.283185482025146484375L;
    const long double cosineArgument =
        (long double)cosineRate * (long double)seconds *
        6.283185482025146484375L;

    return (float)((long double)sin((double)sineArgument) *
                   (long double)cos((double)cosineArgument));
}
