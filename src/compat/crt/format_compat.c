#include "compat/crt/format_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * NOT_FROM_ORIGINAL_SOURCE
 *
 * Preserve Visual C++ 7.1 _vsnprintf: an exact fit returns count without a
 * terminator; longer output writes count bytes and returns -1. Shorter output
 * is terminated and returns its length. The attempted final terminator does
 * not change the formatter's saved return value, including empty output with
 * count == 0. The one-byte-wider temporary preserves the final output byte
 * that host vsnprintf would otherwise replace with its terminator.
 */
int coduo_crt_vsnprintf(char *destination, size_t count,
                        const char *format, va_list arguments)
{
    va_list lengthArguments;
    va_list formatArguments;
    int required;

    va_copy(lengthArguments, arguments);
    required = vsnprintf(NULL, 0, format, lengthArguments);
    va_end(lengthArguments);
    if (required < 0) {
        return -1;
    }

    if ((size_t)required < count) {
        va_copy(formatArguments, arguments);
        required = vsnprintf(destination, count, format, formatArguments);
        va_end(formatArguments);
        return required;
    }

    if (count != 0 && destination != NULL) {
        char *const outputPrefix = malloc(count + 1);

        if (outputPrefix == NULL) {
            return -1;
        }
        va_copy(formatArguments, arguments);
        required = vsnprintf(outputPrefix, count + 1, format, formatArguments);
        if (required >= 0) {
            memcpy(destination, outputPrefix, count);
        }
        va_end(formatArguments);
        free(outputPrefix);
    }
    return required >= 0 && (size_t)required == count ? required : -1;
}

/* NOT_FROM_ORIGINAL_SOURCE: variadic legacy-MSVC formatting entry. */
int coduo_crt_snprintf(char *destination, size_t count,
                       const char *format, ...)
{
    va_list arguments;
    int result;

    va_start(arguments, format);
    result = coduo_crt_vsnprintf(destination, count, format, arguments);
    va_end(arguments);
    return result;
}
