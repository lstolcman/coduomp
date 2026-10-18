#include "vector_math.h"

#include "compat/crt/random_compat.h"

#include <stdint.h>
#include <stdlib.h>

enum {
    COM_NOISE_TABLE_SIZE = 256,
    COM_NOISE_TABLE_MASK = COM_NOISE_TABLE_SIZE - 1,
    COM_NOISE_RANDOM_SEED = 1001,
    COM_NOISE_CRT_RANDOM_MAX = 32767
};

/* Original Windows storage: 0x0388c038 and 0x0388c438. Only Com_NoiseInit
 * writes these tables and only GetNoiseValue reads them. Their relative order
 * differs in the same-module Mac executable, so source ownership is more
 * accurate than preserving either linker's address order. */
static int32_t noisePermutation[COM_NOISE_TABLE_SIZE];
static float noiseValues[COM_NOISE_TABLE_SIZE];

/* Source: CoDUOMP.exe 0x0050e780..0x0050e7c3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050e780_0050e7c4.mcode.
 * Name: exact same-module Mac symbol GetNoiseValue. Windows retains this
 * standalone body and also inlines all eight calls into Com_NoiseGet4f. Both
 * forms prove the t/z/y/x permutation nesting and a 255 mask at every access. */
static float GetNoiseValue(int32_t x, int32_t y, int32_t z, int32_t t)
{
    const int32_t tHash =
        noisePermutation[t & COM_NOISE_TABLE_MASK];
    const int32_t zHash =
        noisePermutation[(z + tHash) & COM_NOISE_TABLE_MASK];
    const int32_t yHash =
        noisePermutation[(y + zHash) & COM_NOISE_TABLE_MASK];
    const int32_t valueIndex =
        noisePermutation[(x + yHash) & COM_NOISE_TABLE_MASK];

    return noiseValues[valueIndex];
}

/* Source: CoDUOMP.exe 0x0050e7d0..0x0050e82c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050e7d0_0050e82d.mcode.
 * Name: exact same-module Mac symbol Com_NoiseInit. The routine seeds the
 * Windows-domain provider with 1001 and consumes exactly two values per table
 * entry. The explicit random scale is the exact Windows float at
 * 0x005b9b90; mathematically it is 1.0f / 32767.0f. */
void Com_NoiseInit(void)
{
    const float randomScale = 3.0518509447574615e-05f;

    coduo_crt_srand(COM_NOISE_RANDOM_SEED);
    for (int32_t index = 0; index < COM_NOISE_TABLE_SIZE; ++index) {
        noiseValues[index] = (float)(
            (long double)coduo_crt_rand() * randomScale * 2.0L - 1.0L);

        /* This is rand * 256 / (RAND_MAX + 1), followed by the table mask.
         * The Windows and Mac compilers both reduce it to the proven
         * shift-by-7 result for the 15-bit MSVC random value. */
        noisePermutation[index] =
            (coduo_crt_rand() * COM_NOISE_TABLE_SIZE /
             (COM_NOISE_CRT_RANDOM_MAX + 1)) &
            COM_NOISE_TABLE_MASK;
    }
}

/* Source: CoDUOMP.exe 0x0050e830..0x0050ead4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050e830_0050ead5.mcode.
 * Name and source-level GetNoiseValue calls: exact same-module Mac symbols.
 * Windows proves the same eight lattice samples per t slice and the x, then y,
 * then z, then t linear-interpolation order. FastFloor reproduces the exact
 * 0x3fdfffffff000000 x87 floor bias used for all four coordinates. The spatial
 * interpolation stays live until each time slice is stored at 0x0050eaa7;
 * the final time interpolation returns without a binary32 store, and both
 * renderer consumers continue arithmetic on that retained result. */
long double Com_NoiseGet4f(float x, float y, float z, float t)
{
    const int32_t xBase = FastFloor(x);
    const int32_t yBase = FastFloor(y);
    const int32_t zBase = FastFloor(z);
    const int32_t tBase = FastFloor(t);
    const float xFraction = x - (float)xBase;
    const float yFraction = y - (float)yBase;
    const float zFraction = z - (float)zBase;
    const float tFraction = t - (float)tBase;
    const float inverseX = 1.0f - xFraction;
    const float inverseY = 1.0f - yFraction;
    const float inverseZ = 1.0f - zFraction;
    float timeValues[2];

    for (int32_t tCorner = 0; tCorner < 2; ++tCorner) {
        const int32_t latticeT = tBase + tCorner;
        const float noise000 =
            GetNoiseValue(xBase, yBase, zBase, latticeT);
        const float noise100 =
            GetNoiseValue(xBase + 1, yBase, zBase, latticeT);
        const float noise010 =
            GetNoiseValue(xBase, yBase + 1, zBase, latticeT);
        const float noise110 =
            GetNoiseValue(xBase + 1, yBase + 1, zBase, latticeT);
        const float noise001 =
            GetNoiseValue(xBase, yBase, zBase + 1, latticeT);
        const float noise101 =
            GetNoiseValue(xBase + 1, yBase, zBase + 1, latticeT);
        const float noise011 =
            GetNoiseValue(xBase, yBase + 1, zBase + 1, latticeT);
        const float noise111 =
            GetNoiseValue(xBase + 1, yBase + 1, zBase + 1, latticeT);

        const long double zLowerY0 =
            (long double)noise000 * inverseX + (long double)noise100 * xFraction;
        const long double zLowerY1 =
            (long double)noise010 * inverseX + (long double)noise110 * xFraction;
        const long double zUpperY0 =
            (long double)noise001 * inverseX + (long double)noise101 * xFraction;
        const long double zUpperY1 =
            (long double)noise011 * inverseX + (long double)noise111 * xFraction;
        const long double zLower =
            zLowerY0 * inverseY + zLowerY1 * yFraction;
        const long double zUpper =
            zUpperY0 * inverseY + zUpperY1 * yFraction;

        timeValues[tCorner] = (float)(
            zLower * inverseZ + zUpper * zFraction);
    }

    return (long double)timeValues[0] * (1.0L - tFraction) +
           (long double)timeValues[1] * tFraction;
}
