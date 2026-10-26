#include "q_math.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/crt/atan2_compat.h"

#include <math.h>
#include <stdint.h>


/*
 * NormalToLatLong is byte-identical within the four Windows targets at
 * CoDUOMP.exe 0x00434aa0, cgame 0x3004cc00, UI 0x40004c10, and game
 * 0x20019c50.  Linux coduo_lnxded 0x0806ac7e and game RVA 0x0003eb22 have the
 * same branches, operation graph, 16-bit truncating conversions, and byte
 * stores.  The sole source-level platform difference is the pi constant:
 * Windows widens binary32 pi to binary64, while Linux loads binary64 pi.
 */
void NormalToLatLong(const vec3_t normal, uint8_t encoded[2])
{
    const double degrees = 180.0;
    const double pi = 3.1415927410125732;
    const double byteScale = 0.7083333134651184;
    if (normal[0] == 0.0f && normal[1] == 0.0f) {
        encoded[1] = 0;
        encoded[0] = normal[2] > 0.0f ? 0 : 128;
        return;
    }

    const long double longitude =
        coduomp_crt_atan2l((WIDE_FLOAT)normal[1], (WIDE_FLOAT)normal[0]) *
        (long double)degrees / (long double)pi * (long double)byteScale;
    const long double latitude =
        (long double)acos((double)normal[2]) *
        (long double)degrees / (long double)pi * (long double)byteScale;
    encoded[1] = coduo_fp_to_u8_extended(longitude);
    encoded[0] = coduo_fp_to_u8_extended(latitude);
}
