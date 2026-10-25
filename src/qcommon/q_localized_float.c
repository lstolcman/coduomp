#include "q_string.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>


/*
 * Complete localized-float helper pair:
 *
 *                                  decimal delimiter     formatter
 * CoDUOMP.exe                      0x00450830            0x00450860
 * uo_cgame_mp_x86.dll             0x3004f620            0x3004f650
 * uo_ui_mp_x86.dll                0x40007650            0x40007680
 * uo_game_mp_x86.dll              0x20058e40            0x20058e70
 * coduo_lnxded                     0x08087d42            0x08087d4c
 * game.mp.uo.i386.so              RVA 0x000949d7        RVA 0x000949e1
 *
 * The four Windows bodies are instruction-identical apart from relocations:
 * French, German, Italian, Spanish, Russian, and Polish use a comma. Both
 * Linux bodies ignore the language argument and always use a period. This is
 * therefore a genuine build-mode distinction, not compiler shaping.
 *
 * Legacy MSVC _snprintf receives bufferSize - 1 and can fill that complete
 * count without terminating; the following explicit store terminates at
 * bufferSize - 1. C99 snprintf reserves one character of its count for NUL.
 * Passing bufferSize to C99 preserves that output-capacity convention when
 * both formatters produce the same character sequence. Conversion, locale,
 * and formatting-error behavior remain provider-specific. The Windows path
 * limits positive precision to 512 before conversion. Linux keeps its
 * literal size-minus-one call.
 * The Windows implementation below supplies the legacy conversion and counted
 * output itself, including its two rounding stages and one-byte locale radix.
 */

enum {
    Q_LEGACY_LIMB_BITS = 16,
    Q_LEGACY_MANTISSA_LIMBS = 5,
    Q_LEGACY_INTEGER_LIMBS = 6,
    Q_LEGACY_SIGNIFICANT_DIGITS = 17,
    Q_LEGACY_DECIMAL_CAPACITY = 22,
    Q_LEGACY_DIGIT_SCRATCH = 24,
    Q_LEGACY_DEFAULT_PRECISION = 6,
    Q_LEGACY_MAX_PRECISION = 512,
    Q_LEGACY_INLINE_PRECISION = 163,
    Q_LEGACY_INLINE_CAPACITY = 512,
    Q_LEGACY_ALLOCATION_EXTRA = 349,
    Q_LEGACY_BINARY64_SPECIAL_EXPONENT = 2047,
    Q_LEGACY_EXTENDED_SPECIAL_EXPONENT = 32767,
    Q_LEGACY_BINARY64_EXPONENT_ADJUSTMENT = 15360,
    Q_LEGACY_PRODUCT_EXPONENT_ADJUSTMENT = 16382,
    Q_LEGACY_UNIT_EXPONENT = 16383,
    Q_LEGACY_ESTIMATE_EXPONENT_FACTOR = 19728,
    Q_LEGACY_ESTIMATE_BYTE_FACTOR = 77,
    Q_LEGACY_ESTIMATE_OFFSET = 323162868,
    Q_LEGACY_ESTIMATE_SCALE = 65536,
    Q_LEGACY_TABLE_RADIX = 8,
    Q_LEGACY_TABLE_ROW_LENGTH = 7,
    Q_LEGACY_FINITE = 1,
    Q_LEGACY_SPECIAL = 0
};

#define Q_LEGACY_LIMB_HIGH_BIT UINT16_C(0x8000)
#define Q_LEGACY_BINARY64_FRACTION_MASK UINT64_C(0x000fffffffffffff)
#define Q_LEGACY_INTEGER_BIT UINT64_C(0x8000000000000000)
#define Q_LEGACY_QUIET_BIT UINT64_C(0x4000000000000000)
#define Q_LEGACY_COUNT_SIGN_BIT UINT32_C(0x80000000)

typedef struct {
    uint16_t mantissa[Q_LEGACY_MANTISSA_LIMBS];
    uint16_t exponent;
} coduomp_legacy_scaled_t;

typedef struct {
    uint16_t limb[Q_LEGACY_INTEGER_LIMBS];
} coduomp_legacy_integer_t;

typedef struct {
    int16_t position;
    char sign;
    unsigned char length;
    char digits[Q_LEGACY_DECIMAL_CAPACITY];
} coduomp_legacy_decimal_t;

typedef struct {
    int32_t sign;
    int32_t position;
    int32_t status;
    char *digits;
} coduomp_legacy_coefficient_t;

/* cgame RVAs 0x81d90 and 0x81ef0: the two base-eight rows reachable from
 * binary32 input. Each entry is five little-endian mantissa limbs and an
 * unsigned exponent; these are exact encoded constants. */
static const coduomp_legacy_scaled_t coduomp_legacy_positive_powers[] = {
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xa000}, 16386},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xc800}, 16389},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xfa00}, 16392},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0x9c40}, 16396},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xc350}, 16399},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xf424}, 16402},
    {{0x0000, 0x0000, 0x0000, 0x8000, 0x9896}, 16406},
    {{0x0000, 0x0000, 0x0000, 0x2000, 0xbebc}, 16409},
    {{0x0000, 0x0000, 0x0400, 0xc9bf, 0x8e1b}, 16436},
    {{0x0000, 0xa100, 0xcced, 0x1bce, 0xd3c2}, 16462},
    {{0xf020, 0xb59e, 0x2b70, 0xada8, 0x9dc5}, 16489},
    {{0x5dd0, 0x25fd, 0x1ae5, 0x4f8e, 0xeb19}, 16515},
};

static const coduomp_legacy_scaled_t coduomp_legacy_negative_powers[] = {
    {{0xcccd, 0xcccd, 0xcccc, 0xcccc, 0xcccc}, 16379},
    {{0x3d71, 0xd70a, 0x70a3, 0x0a3d, 0xa3d7}, 16376},
    {{0x645a, 0xdf3b, 0x8d4f, 0x6e97, 0x8312}, 16373},
    {{0xd3c3, 0x652c, 0xe219, 0x1758, 0xd1b7}, 16369},
    {{0x0fd0, 0x8423, 0x1b47, 0xac47, 0xa7c5}, 16366},
    {{0xa640, 0x69b6, 0xaf6c, 0xbd05, 0x8637}, 16363},
    {{0x3d33, 0x42bc, 0xe57a, 0x94d5, 0xd6bf}, 16359},
    {{0xfdc2, 0xcefd, 0x8461, 0x7711, 0xabcc}, 16356},
    {{0x4c2f, 0xe15b, 0xc44d, 0x94be, 0xe695}, 16329},
    {{0xc492, 0x3b53, 0x4475, 0x14cd, 0x9abe}, 16303},
    {{0x67de, 0x94ba, 0x4539, 0x1ead, 0xcfb1}, 16276},
};

/* NOT_FROM_ORIGINAL_SOURCE: portable limb representation of the raw 96-bit
 * shift at cgame RVA 0x6953e. These are integer limbs, not long double. */
static void coduomp_legacy_shift_left(coduomp_legacy_integer_t *value)
{
    uint32_t carry = 0;
    for (int i = 0; i < Q_LEGACY_INTEGER_LIMBS; ++i) {
        uint32_t word = ((uint32_t)value->limb[i] << 1) | carry;
        value->limb[i] = (uint16_t)word;
        carry = word >> Q_LEGACY_LIMB_BITS;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: portable limb representation of cgame RVA 0x6956c. */
static void coduomp_legacy_shift_right(coduomp_legacy_integer_t *value)
{
    uint16_t carry = 0;
    for (int i = Q_LEGACY_INTEGER_LIMBS - 1; i >= 0; --i) {
        uint16_t word = value->limb[i];
        value->limb[i] = (uint16_t)((word >> 1) | carry);
        carry = (word & 1u) ? Q_LEGACY_LIMB_HIGH_BIT : 0;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: portable limb representation of cgame RVA 0x694e0. */
static void coduomp_legacy_add(coduomp_legacy_integer_t *value, const coduomp_legacy_integer_t *addend)
{
    uint32_t carry = 0;
    for (int i = 0; i < Q_LEGACY_INTEGER_LIMBS; ++i) {
        uint32_t word = (uint32_t)value->limb[i] + addend->limb[i] + carry;
        value->limb[i] = (uint16_t)word;
        carry = word >> Q_LEGACY_LIMB_BITS;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: bounded positive-normal path of cgame RVA 0x69677.
 * Float inputs and the selected power tables cannot reach the original special
 * or underflow paths. Only the 15 products with i+j >= 4 participate: replacing
 * this with a full 160-bit product would introduce discarded low-diagonal carries.
 * Both operands have bit 79 set, so normalization needs at most one left shift. */
static void coduomp_legacy_multiply(coduomp_legacy_scaled_t *value, const coduomp_legacy_scaled_t *factor)
{
    coduomp_legacy_integer_t product = {{0}};
    uint64_t carry = 0;
    int exponent = value->exponent + factor->exponent - Q_LEGACY_PRODUCT_EXPONENT_ADJUSTMENT;

    for (int diagonal = 0; diagonal < Q_LEGACY_MANTISSA_LIMBS; ++diagonal) {
        uint64_t sum = carry;
        for (int j = 0; j < Q_LEGACY_MANTISSA_LIMBS - diagonal; ++j) {
            sum += (uint64_t)value->mantissa[diagonal + j] * factor->mantissa[Q_LEGACY_MANTISSA_LIMBS - 1 - j];
        }
        product.limb[diagonal] = (uint16_t)sum;
        carry = sum >> Q_LEGACY_LIMB_BITS;
    }
    product.limb[Q_LEGACY_INTEGER_LIMBS - 1] = (uint16_t)carry;

    if (!(product.limb[Q_LEGACY_INTEGER_LIMBS - 1] & Q_LEGACY_LIMB_HIGH_BIT)) {
        coduomp_legacy_shift_left(&product);
        --exponent;
    }

    if (product.limb[0] > Q_LEGACY_LIMB_HIGH_BIT ||
        (product.limb[0] == Q_LEGACY_LIMB_HIGH_BIT && (product.limb[1] & 1u))) {
        uint32_t increment = 1;
        for (int i = 1; i < Q_LEGACY_INTEGER_LIMBS; ++i) {
            uint32_t word = (uint32_t)product.limb[i] + increment;
            product.limb[i] = (uint16_t)word;
            increment = word >> Q_LEGACY_LIMB_BITS;
        }
        if (increment) {
            product.limb[Q_LEGACY_INTEGER_LIMBS - 1] = Q_LEGACY_LIMB_HIGH_BIT;
            ++exponent;
        }
    }
    for (int i = 0; i < Q_LEGACY_MANTISSA_LIMBS; ++i) {
        value->mantissa[i] = product.limb[i + 1];
    }
    value->exponent = (uint16_t)exponent;
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame RVA 0x698a9 restricted to the float-domain
 * powers [-39,44] and the caller's preserve-low-word mode. */
static void coduomp_legacy_scale(coduomp_legacy_scaled_t *value, int power)
{
    const coduomp_legacy_scaled_t *table = coduomp_legacy_positive_powers;
    if (power < 0) {
        table = coduomp_legacy_negative_powers;
        power = -power;
    }
    int row = 0;
    while (power) {
        int digit = power % Q_LEGACY_TABLE_RADIX;
        power /= Q_LEGACY_TABLE_RADIX;
        if (digit) {
            coduomp_legacy_scaled_t factor = table[row * Q_LEGACY_TABLE_ROW_LENGTH + digit - 1];
            if (factor.mantissa[0] >= Q_LEGACY_LIMB_HIGH_BIT) {
                /* The original decrements only the DWORD at byte offset 2;
                 * a borrow must not propagate into the following limbs. */
                uint32_t low = factor.mantissa[1] | ((uint32_t)factor.mantissa[2] << Q_LEGACY_LIMB_BITS);
                --low;
                factor.mantissa[1] = (uint16_t)low;
                factor.mantissa[2] = (uint16_t)(low >> Q_LEGACY_LIMB_BITS);
            }
            coduomp_legacy_multiply(value, &factor);
        }
        ++row;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: selected 17-digit, mode-zero path of cgame RVA
 * 0x6a841. The input is the explicit 64-bit significand and unsigned exponent
 * produced by RVA 0x699a6; it is never a host long-double object. */
static int coduomp_legacy_decimal(uint64_t significand, int exponent, char sign, coduomp_legacy_decimal_t *decimal)
{
    decimal->sign = sign;
    if (exponent == 0 && significand == 0) {
        goto zero;
    }
    if (exponent == Q_LEGACY_EXTENDED_SPECIAL_EXPONENT) {
        const char *text;
        decimal->position = 1;
        if (significand == Q_LEGACY_INTEGER_BIT) {
            text = "1#INF";
        } else if (!(significand & Q_LEGACY_QUIET_BIT)) {
            text = "1#SNAN";
        } else if (sign == '-' && significand == (Q_LEGACY_INTEGER_BIT | Q_LEGACY_QUIET_BIT)) {
            text = "1#IND";
        } else {
            text = "1#QNAN";
        }
        strcpy(decimal->digits, text);
        decimal->length = (unsigned char)strlen(text);
        return Q_LEGACY_SPECIAL;
    }

    coduomp_legacy_scaled_t scaled = {{0}, (uint16_t)exponent};
    for (int i = 1; i < Q_LEGACY_MANTISSA_LIMBS; ++i) {
        scaled.mantissa[i] = (uint16_t)(significand >> ((i - 1) * Q_LEGACY_LIMB_BITS));
    }
    int estimate = exponent * Q_LEGACY_ESTIMATE_EXPONENT_FACTOR +
                   ((exponent >> 8) + 2 * (int)(significand >> 56)) * Q_LEGACY_ESTIMATE_BYTE_FACTOR -
                   Q_LEGACY_ESTIMATE_OFFSET;
    int position = estimate / Q_LEGACY_ESTIMATE_SCALE;
    if (estimate < 0 && estimate % Q_LEGACY_ESTIMATE_SCALE) {
        --position;
    }
    coduomp_legacy_scale(&scaled, -position);
    if (scaled.exponent >= Q_LEGACY_UNIT_EXPONENT) {
        const coduomp_legacy_scaled_t tenth = {{0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc}, 16379};
        ++position;
        coduomp_legacy_multiply(&scaled, &tenth);
    }
    decimal->position = (int16_t)position;

    coduomp_legacy_integer_t fraction = {{0}};
    for (int i = 0; i < Q_LEGACY_MANTISSA_LIMBS; ++i) {
        fraction.limb[i] = scaled.mantissa[i];
    }
    for (int i = 0; i < 8; ++i) {
        coduomp_legacy_shift_left(&fraction);
    }
    int shift = Q_LEGACY_PRODUCT_EXPONENT_ADJUSTMENT - scaled.exponent;
    if (shift > 0) {
        shift &= UINT8_MAX;
        while (shift--) {
            coduomp_legacy_shift_right(&fraction);
        }
    }
    for (int i = 0; i <= Q_LEGACY_SIGNIFICANT_DIGITS; ++i) {
        coduomp_legacy_integer_t saved = fraction;
        coduomp_legacy_shift_left(&fraction);
        coduomp_legacy_shift_left(&fraction);
        coduomp_legacy_add(&fraction, &saved);
        coduomp_legacy_shift_left(&fraction);
        decimal->digits[i] = (char)('0' + (fraction.limb[Q_LEGACY_INTEGER_LIMBS - 1] >> 8));
        fraction.limb[Q_LEGACY_INTEGER_LIMBS - 1] &= UINT8_MAX;
    }

    int last = Q_LEGACY_SIGNIFICANT_DIGITS - 1;
    if (decimal->digits[Q_LEGACY_SIGNIFICANT_DIGITS] >= '5') {
        while (last >= 0 && decimal->digits[last] == '9') {
            decimal->digits[last--] = '0';
        }
        if (last < 0) {
            last = 0;
            ++decimal->position;
        }
        ++decimal->digits[last];
    } else {
        while (last >= 0 && decimal->digits[last] == '0') {
            --last;
        }
        if (last < 0) {
            goto zero;
        }
    }
    decimal->length = (unsigned char)(last + 1);
    decimal->digits[last + 1] = '\0';
    return Q_LEGACY_FINITE;

zero:
    decimal->position = 0;
    decimal->sign = ' ';
    decimal->length = 1;
    decimal->digits[0] = '0';
    decimal->digits[1] = '\0';
    return Q_LEGACY_FINITE;
}

/* NOT_FROM_ORIGINAL_SOURCE: float-domain realization of cgame RVAs 0x699a6 and
 * 0x69a60. A nonzero finite promoted float is always a normal binary64 value;
 * both zero signs are cleared by the original integer conversion. */
static void coduomp_legacy_coefficient(double value, coduomp_legacy_coefficient_t *coefficient, char *digits)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    int exponent = (int)((bits >> 52) & Q_LEGACY_BINARY64_SPECIAL_EXPONENT);
    uint64_t significand = 0;
    char sign = (bits & Q_LEGACY_INTEGER_BIT) ? '-' : ' ';
    if (exponent) {
        significand = ((bits & Q_LEGACY_BINARY64_FRACTION_MASK) << 11) | Q_LEGACY_INTEGER_BIT;
        exponent = exponent == Q_LEGACY_BINARY64_SPECIAL_EXPONENT ? Q_LEGACY_EXTENDED_SPECIAL_EXPONENT :
                   exponent + Q_LEGACY_BINARY64_EXPONENT_ADJUSTMENT;
    } else {
        sign = ' ';
    }
    coduomp_legacy_decimal_t decimal;
    int status = coduomp_legacy_decimal(significand, exponent, sign, &decimal);
    coefficient->status = status;
    coefficient->sign = decimal.sign;
    coefficient->position = decimal.position;
    strcpy(digits, decimal.digits);
    coefficient->digits = digits;
}

/* cgame RVA 0x6992f: the extra leading zero bounds carry propagation. The
 * coefficient and output are disjoint, including for the nonfinite tokens. */
static void coduomp_legacy_round_copy(char *output, int count, coduomp_legacy_coefficient_t *coefficient)
{
    const char *source = coefficient->digits;
    char *cursor = output;
    *cursor++ = '0';
    for (int i = 0; i < count; ++i) {
        *cursor++ = *source ? *source++ : '0';
    }
    *cursor = '\0';
    if (count >= 0 && (signed char)*source >= '5') {
        --cursor;
        while (*cursor == '9') {
            *cursor-- = '0';
        }
        ++*cursor;
    }
    if (*output == '1') {
        ++coefficient->position;
    } else {
        memmove(output, output + 1, strlen(output + 1) + 1);
    }
}

/* cgame RVA 0x65667, with byte extents expressed by the host's size_t. */
static void coduomp_legacy_insert(char *cursor, size_t count)
{
    if (count) {
        memmove(cursor + count, cursor, strlen(cursor) + 1);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: mode-zero path of cgame RVA 0x6579e. The radix
 * publisher at RVA 0x62c52 takes the first byte of the locale's decimal_point;
 * localeconv supplies that record on the host. */
static void coduomp_legacy_emit(char *output, int precision, const coduomp_legacy_coefficient_t *coefficient)
{
    char *cursor = output;
    if (coefficient->sign == '-') {
        *cursor++ = '-';
    }
    if (coefficient->position <= 0) {
        coduomp_legacy_insert(cursor, 1);
        *cursor++ = '0';
    } else {
        cursor += coefficient->position;
    }
    if (precision > 0) {
        coduomp_legacy_insert(cursor, 1);
        *cursor++ = localeconv()->decimal_point[0];
        if (coefficient->position < 0) {
            int zeros = -coefficient->position;
            if (precision < zeros) {
                zeros = precision;
            }
            coduomp_legacy_insert(cursor, (size_t)zeros);
            memset(cursor, '0', (size_t)zeros);
        }
    }
}

/* cgame RVA 0x6583a: conversion always generates 17 significant digits before
 * the separate requested-precision rounding pass. */
static void coduomp_legacy_fixed(double value, char *output, int precision)
{
    coduomp_legacy_coefficient_t coefficient;
    char digits[Q_LEGACY_DIGIT_SCRATCH];
    coduomp_legacy_coefficient(value, &coefficient, digits);
    coduomp_legacy_round_copy(output + (coefficient.sign == '-'), coefficient.position + precision, &coefficient);
    coduomp_legacy_emit(output, precision, &coefficient);
}

/* NOT_FROM_ORIGINAL_SOURCE: selected string-output path of cgame RVAs 0x5cb85
 * and 0x5cb2e. The original tests SF after a wrapping DWORD decrement. */
static void coduomp_legacy_write(char **output, uint32_t *remaining, const char *text, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        --*remaining;
        if (*remaining & Q_LEGACY_COUNT_SIGN_BIT) {
            break;
        }
        *(*output)++ = text[i];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: fixed-format selection from cgame RVAs 0x5cbdb
 * and 0x5c17e. Allocation failure retains the inline buffer and precision 163.
 * Prefix and body remain separate writes, including after prefix exhaustion. */
static void coduomp_legacy_format(double value, char *buffer, uint32_t capacity, int precision)
{
    char inlineText[Q_LEGACY_INLINE_CAPACITY];
    char *allocated = NULL;
    char *text = inlineText;
    if (precision < 0) {
        precision = Q_LEGACY_DEFAULT_PRECISION;
    } else if (precision > Q_LEGACY_MAX_PRECISION) {
        precision = Q_LEGACY_MAX_PRECISION;
    }
    if (precision > Q_LEGACY_INLINE_PRECISION) {
        allocated = malloc((size_t)precision + Q_LEGACY_ALLOCATION_EXTRA);
        if (allocated) {
            text = allocated;
        } else {
            precision = Q_LEGACY_INLINE_PRECISION;
        }
    }
    coduomp_legacy_fixed(value, text, precision);
    char *output = buffer;
    uint32_t remaining = capacity - 1u;
    if (*text == '-') {
        coduomp_legacy_write(&output, &remaining, text++, 1);
    }
    coduomp_legacy_write(&output, &remaining, text, strlen(text));
    if (allocated) {
        free(allocated);
    }
    --remaining;
    if (!(remaining & Q_LEGACY_COUNT_SIGN_BIT)) {
        *output = '\0';
    }
}

int32_t Q_GetDecimalDelimiter(language_t language)
{
    switch (language) {
    case LANGUAGE_FRENCH:
    case LANGUAGE_GERMAN:
    case LANGUAGE_ITALIAN:
    case LANGUAGE_SPANISH:
    case LANGUAGE_RUSSIAN:
    case LANGUAGE_POLISH:
        return ',';
    default:
        return '.';
    }
}

void Q_LocalizedFloatToString(float value, char *buffer,
                              uint32_t bufferSize, int32_t precision,
                              language_t language)
{
    /* NOT_FROM_ORIGINAL_SOURCE: a zero-sized output has no writable
     * terminator slot. */
    if (bufferSize == 0) {
        return;
    }
    coduomp_legacy_format((double)value, buffer, bufferSize, precision);
    buffer[bufferSize - 1u] = '\0';

    const int32_t delimiter = Q_GetDecimalDelimiter(language);
    if (delimiter == '.') {
        return;
    }

    for (uint32_t index = 0; index < bufferSize; ++index) {
        if (buffer[index] == '.') {
            buffer[index] = (char)delimiter;
            return;
        }
    }
}
