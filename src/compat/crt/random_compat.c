#include "compat/crt/random_compat.h"

#include <stdlib.h>

enum {
    CODUO_CRT_RAND_MASK = 32767,
    CODUO_CRT_RAND_CARDINALITY = 32768,
    CODUO_CRT_RAND_MULTIPLIER = 214013,
    CODUO_CRT_RAND_INCREMENT = 2531011,
    CODUO_CRT_RAND_SHIFT = 16
};

/* NOT_FROM_ORIGINAL_SOURCE: compiler TLS carries the random word owned by
 * each image's statically linked Windows CRT. It must not share the host
 * libc generator with another image or another calling thread. */
static __thread uint32_t coduo_crt_random_state = CODUO_CRT_RAND_INITIAL_SEED;

#if RAND_MAX % 32768 != 32767
#error "Host rand domain must contain a whole number of 15-bit domains"
#endif

#define CODUO_GLIBC_RAND_MASK UINT32_C(2147483647)
#define CODUO_GLIBC_RAND_CARDINALITY 2147483648.0

/* NOT_FROM_ORIGINAL_SOURCE: map a non-negative compatibility-domain sample
 * without overflowing when the requested signed interval spans INT32_MIN to
 * INT32_MAX. The public range functions require maximum > minimum. */
static int32_t coduo_random_map_range(int32_t sample, int32_t minimum,
                                      int32_t maximum)
{
    const uint32_t span =
        (uint32_t)((int64_t)maximum - (int64_t)minimum);
    const int64_t result =
        (int64_t)minimum + (int64_t)((uint32_t)sample % span);

    return (int32_t)result;
}

/* NOT_FROM_ORIGINAL_SOURCE: seed the same image-local thread state consumed
 * by the Windows-domain provider. */
void coduo_crt_srand(uint32_t seed)
{
    coduo_crt_random_state = seed;
}

/* NOT_FROM_ORIGINAL_SOURCE: the five Windows-CRT-domain provider functions
 * below preserve the original modulo-2^32 recurrence and 15-bit results. */
int32_t coduo_crt_rand(void)
{
    coduo_crt_random_state = coduo_crt_random_state * (uint32_t)CODUO_CRT_RAND_MULTIPLIER + CODUO_CRT_RAND_INCREMENT;
    return (int32_t)((coduo_crt_random_state >> CODUO_CRT_RAND_SHIFT) & CODUO_CRT_RAND_MASK);
}

float coduo_crt_randf(void)
{
    return (float)coduo_crt_rand() / (float)CODUO_CRT_RAND_CARDINALITY;
}

double coduo_crt_rand_unit(void)
{
    return (double)coduo_crt_rand() / (double)CODUO_CRT_RAND_CARDINALITY;
}

double coduo_crt_rand_signed_unit(void)
{
    return coduo_crt_rand_unit() * 2.0 - 1.0;
}

int32_t coduo_crt_randrange(int32_t minimum, int32_t maximum)
{
    return coduo_random_map_range(coduo_crt_rand(), minimum, maximum);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain host libc seed ownership for Linux behavior. */
void coduo_glibc_srand(uint32_t seed)
{
    srand(seed);
}

/* NOT_FROM_ORIGINAL_SOURCE: the four glibc-domain provider functions below
 * supply 31 bits, combining native samples when the host rand is narrower. */
int32_t coduo_glibc_rand(void)
{
#if RAND_MAX >= 2147483647
    return (int32_t)((uint32_t)rand() & CODUO_GLIBC_RAND_MASK);
#else
    const uint32_t high = (uint32_t)rand() & CODUO_CRT_RAND_MASK;
    const uint32_t middle = (uint32_t)rand() & CODUO_CRT_RAND_MASK;
    const uint32_t low = (uint32_t)rand() & 1u;

    return (int32_t)((high << 16) | (middle << 1) | low);
#endif
}

double coduo_glibc_rand_unit(void)
{
    return (double)coduo_glibc_rand() / CODUO_GLIBC_RAND_CARDINALITY;
}

double coduo_glibc_rand_signed_unit(void)
{
    return coduo_glibc_rand_unit() * 2.0 - 1.0;
}

int32_t coduo_glibc_randrange(int32_t minimum, int32_t maximum)
{
    return coduo_random_map_range(coduo_glibc_rand(), minimum, maximum);
}
