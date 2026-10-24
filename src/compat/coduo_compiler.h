/* NOT_FROM_ORIGINAL_SOURCE: compiler spelling and caller-stack adapters. */
#ifndef CODUO_COMPILER_H
#define CODUO_COMPILER_H

#include <stddef.h>

#ifdef __cplusplus
#define CODUO_ALIGNOF(type) alignof(type)
#define CODUO_FUNCTION_POINTER_TYPE(function) decltype(&(function))
#else
#define CODUO_FUNCTION_POINTER_TYPE(function) __typeof__(&(function))
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define CODUO_ALIGNOF(type) _Alignof(type)
#elif defined(_MSC_VER)
#define CODUO_ALIGNOF(type) __alignof(type)
#else
#define CODUO_ALIGNOF(type) __alignof__(type)
#endif
#endif

#if defined(_MSC_VER)
#include <malloc.h>
#define CODUO_ALLOCA(bytes) _alloca(bytes)
#define CODUO_UNUSED
#define CODUO_THREAD_LOCAL __declspec(thread)
/* MSVC's C headers omit max_align_t; its standard library uses double. */
#define CODUO_MAX_ALIGN_TYPE double
#else
#define CODUO_ALLOCA(bytes) __builtin_alloca(bytes)
#define CODUO_UNUSED __attribute__((unused))
#define CODUO_THREAD_LOCAL __thread
#define CODUO_MAX_ALIGN_TYPE max_align_t
#endif

/* Keep VLA block lifetime where supported. MSVC's allocation lives until the
 * caller returns: use only for bounded scratch storage outside repeated loops.
 * count is evaluated once; neither backend supports over-aligned element types.
 * Callers must retain their element count rather than use sizeof(name). */
#if defined(_MSC_VER) || defined(__cplusplus) || defined(__STDC_NO_VLA__)
#define CODUO_STACK_ARRAY(type, name, count) \
    type *name = (type *)CODUO_ALLOCA(sizeof(type) * (size_t)(count))
#else
#define CODUO_STACK_ARRAY(type, name, count) type name[count]
#endif

#endif
