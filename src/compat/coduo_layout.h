/* NOT_FROM_ORIGINAL_SOURCE: constant offsets for array members, including
 * compilers whose offsetof cannot evaluate an indexed member designator. */
#ifndef CODUO_LAYOUT_H
#define CODUO_LAYOUT_H

#include <stddef.h>

#define CODUO_ARRAY_OFFSET(type, member, index) \
    (offsetof(type, member) + sizeof(((type *)0)->member[0]) * (index))

#endif
