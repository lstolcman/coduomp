#include "q_math.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_native_x87.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>



enum {
    COLOR_BYTE_SCALE = 255,
    COLOR_GREEN_SHIFT = 8,
    COLOR_BLUE_SHIFT = 16,
    COLOR_ALPHA_SHIFT = 24,
    COLOR_FISTP_I16_MINIMUM_EXCLUSIVE = -32769,
    COLOR_FISTP_I16_MAXIMUM_EXCLUSIVE = 32768
};



/*
 * The four authoritative Windows bodies are instruction-identical apart from
 * constant and `_ftol2` addresses:
 *
 *   CoDUOMP.exe                 0x00433a50, 0x00433aa0
 *   uo_cgame_mp_x86.dll        0x3004bbb0, 0x3004bc00
 *   uo_ui_mp_x86.dll           0x40003b80, 0x40003bd0
 *   uo_game_mp_x86.dll         0x20018c00, 0x20018c50
 *
 * Each channel is multiplied by binary32 255 under Windows PC=53, converted
 * through `_ftol2`, and packed from the returned low byte.
 *
 * The two Linux bodies use the same channel and packing order, but truncate
 * each live PC=64 product with FISTP m16: coduo_lnxded 0x08069bf2 and
 * 0x08069c61; game.mp.uo.i386.so RVAs 0x0003d8e5 and 0x0003d964.  The m16
 * destination makes overflow/NaN produce integer-indefinite 0x8000, whose low
 * byte is zero, rather than the Windows `_ftol2` low-dword result.
 */
uint32_t ColorBytes3(float red, float green, float blue)
{
    const double scale = (double)(float)COLOR_BYTE_SCALE;
    const uint32_t redByte =
        (uint8_t)coduo_fp_to_i32_extended((long double)((double)red * scale));
    const uint32_t greenByte =
        (uint8_t)coduo_fp_to_i32_extended((long double)((double)green * scale));
    const uint32_t blueByte =
        (uint8_t)coduo_fp_to_i32_extended((long double)((double)blue * scale));

    return redByte | (greenByte << COLOR_GREEN_SHIFT) |
           (blueByte << COLOR_BLUE_SHIFT) |
           (UINT32_C(255) << COLOR_ALPHA_SHIFT);
}

uint32_t ColorBytes4(float red, float green, float blue, float alpha)
{
    const double scale = (double)(float)COLOR_BYTE_SCALE;
    const uint32_t redByte =
        (uint8_t)coduo_fp_to_i32_extended((long double)((double)red * scale));
    const uint32_t greenByte =
        (uint8_t)coduo_fp_to_i32_extended((long double)((double)green * scale));
    const uint32_t blueByte =
        (uint8_t)coduo_fp_to_i32_extended((long double)((double)blue * scale));
    const uint32_t alphaByte =
        (uint8_t)coduo_fp_to_i32_extended((long double)((double)alpha * scale));

    return redByte | (greenByte << COLOR_GREEN_SHIFT) |
           (blueByte << COLOR_BLUE_SHIFT) |
           (alphaByte << COLOR_ALPHA_SHIFT);
}

/*
 * NormalizeColor is instruction-identical within the four Windows images at
 * 0x00433b00, 0x3004bc60, 0x40003c30, and 0x20018cb0.  The Linux bodies at
 * coduo_lnxded 0x08069ce7 and game RVA 0x0003d9fa have the same max scan,
 * exact-zero branch, black fallback, lane order, and binary32 return.  Windows
 * performs each divide under PC=53; Linux performs it under PC=64.  The x87
 * backend selects that precision without duplicating comparison source.
 */
float NormalizeColor(const vec3_t input, vec3_t output)
{
    float maximum;
    memcpy(&maximum, &input[0], sizeof(maximum));

    if (maximum < input[1]) {
        memcpy(&maximum, &input[1], sizeof(maximum));
    }
    if (maximum < input[2]) {
        memcpy(&maximum, &input[2], sizeof(maximum));
    }

    if (maximum == 0.0f) {
        output[2] = 0.0f;
        output[1] = 0.0f;
        output[0] = 0.0f;
    } else {
        output[0] = (float)((long double)input[0] / (long double)maximum);
        output[1] = (float)((long double)input[1] / (long double)maximum);
        output[2] = (float)((long double)input[2] / (long double)maximum);
    }
    return maximum;
}

/*
 * ColorNormalize is likewise instruction-identical within the four Windows
 * images at 0x00434ba0, 0x3004cd00, 0x40004d10, and 0x20019d50.  The Linux
 * bodies at coduo_lnxded 0x0806ae56 and game RVA 0x0003ed26 agree on the same
 * behavior: zero produces white and returns +0.0f; otherwise a 1/max
 * reciprocal is narrowed once to binary32 and reused for three products.
 * Windows evaluates the reciprocal/products under PC=53 and loads scale first
 * for each multiply; Linux uses PC=64 and loads the input lane first.  The
 * load-order commutation of each single product is not a distinct computation.
 */
float ColorNormalize(const vec3_t input, vec3_t output)
{
    float maximum;
    memcpy(&maximum, &input[0], sizeof(maximum));

    if (maximum < input[1]) {
        memcpy(&maximum, &input[1], sizeof(maximum));
    }
    if (maximum < input[2]) {
        memcpy(&maximum, &input[2], sizeof(maximum));
    }

    if (maximum == 0.0f) {
        output[2] = 1.0f;
        output[1] = 1.0f;
        output[0] = 1.0f;
        return 0.0f;
    }

    {
        const float scale = (float)(1.0L / (long double)maximum);
        output[0] = (float)((long double)input[0] * (long double)scale);
        output[1] = (float)((long double)input[1] * (long double)scale);
        output[2] = (float)((long double)input[2] * (long double)scale);
    }
    return maximum;
}
