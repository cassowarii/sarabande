#include "string.h"

usize mystrncpy(char *dst, char *src, usize limit) {
    usize count = 0;
    char ch = 0;

    do {
        ch = dst[count] = src[count];
        count ++;
    } while (ch && count < limit);
    dst[--count] = 0;

    return count;
}
