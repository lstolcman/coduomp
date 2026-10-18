#ifndef CODUO_CRT_RANDOM_COMPAT_H
#define CODUO_CRT_RANDOM_COMPAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NOT_FROM_ORIGINAL_SOURCE
 *
 * These boundaries describe the numeric domain expected by recovered code.
 * Client-only code uses coduo_crt_* directly. Shared server code uses the
 * coduo_server_* aliases selected here by WINDOWS_BEHAVIOR or LINUX_BEHAVIOR.
 */
#define CODUO_CRT_RAND_INITIAL_SEED UINT32_C(1)

/* Keep the Windows provider bound to its owning image on ELF hosts too. */
#if defined(__GNUC__) && !defined(_WIN32)
#define CODUO_CRT_RANDOM_LOCAL __attribute__((visibility("hidden")))
#else
#define CODUO_CRT_RANDOM_LOCAL
#endif

/* Replace this image's calling-thread Windows random state. */
CODUO_CRT_RANDOM_LOCAL void coduo_crt_srand(uint32_t seed);
/* Raw Windows-CRT-domain sample in [0, 32767]. */
CODUO_CRT_RANDOM_LOCAL int32_t coduo_crt_rand(void);
/* Binary32 normalized sample in [0, 1); all 15 source bits remain exact. */
CODUO_CRT_RANDOM_LOCAL float coduo_crt_randf(void);
/* Exact binary64 normalized sample in [0, 1). */
CODUO_CRT_RANDOM_LOCAL double coduo_crt_rand_unit(void);
/* Exact binary64 normalized sample in [-1, 1). */
CODUO_CRT_RANDOM_LOCAL double coduo_crt_rand_signed_unit(void);
/* Integer sample in [minimum, maximum); maximum must exceed minimum. */
CODUO_CRT_RANDOM_LOCAL int32_t coduo_crt_randrange(int32_t minimum, int32_t maximum);

#undef CODUO_CRT_RANDOM_LOCAL

/* Seed the native provider used by the Linux behavior domain. */
void coduo_glibc_srand(uint32_t seed);
/* Raw glibc-domain sample in [0, 2147483647]. */
int32_t coduo_glibc_rand(void);
/* Exact binary64 normalized sample in [0, 1). */
double coduo_glibc_rand_unit(void);
/* Exact binary64 normalized sample in [-1, 1). */
double coduo_glibc_rand_signed_unit(void);
/* Integer sample in [minimum, maximum); maximum must exceed minimum. */
int32_t coduo_glibc_randrange(int32_t minimum, int32_t maximum);

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Shared server random compatibility requires a behavior selection"
#endif

/* NOT_FROM_ORIGINAL_SOURCE: sole build-behavior selection point for server
 * random-number services. These aliases add no forwarding function layer. */
#if defined(WINDOWS_BEHAVIOR)
#define coduo_server_srand coduo_crt_srand
#define coduo_server_rand coduo_crt_rand
#define coduo_server_rand_unit coduo_crt_rand_unit
#define coduo_server_rand_signed_unit coduo_crt_rand_signed_unit
#define coduo_server_randrange coduo_crt_randrange
#else
#define coduo_server_srand coduo_glibc_srand
#define coduo_server_rand coduo_glibc_rand
#define coduo_server_rand_unit coduo_glibc_rand_unit
#define coduo_server_rand_signed_unit coduo_glibc_rand_signed_unit
#define coduo_server_randrange coduo_glibc_randrange
#endif

#ifdef __cplusplus
}
#endif

#endif /* CODUO_CRT_RANDOM_COMPAT_H */
