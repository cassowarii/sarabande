#include "cstrutil.h"

usize sbstrncpy(char *dst, const char *src, usize limit) {
    usize count = 0;
    char ch = 0;

    do {
        ch = dst[count] = src[count];
        count ++;
    } while (ch && count < limit);

    count --;
    if (dst[count] != 0) dst[count] = 0;

    return count;
}

int sbstrncmp(const char *a, const char *b, usize limit) {
    usize count = 0;

    while (a[count] && b[count] && a[count] == b[count] && count < limit) {
        count ++;
    }

    if (a[count] == 0 && b[count] == 0) {
        return 0;
    } else if (a[count] == 0) {
        return 1;
    } else if (b[count] == 0) {
        return -1;
    } else if (a[count] < b[count]) {
        return 1;
    } else if (b[count] < a[count]) {
        return -1;
    } else {
        /* we must have reached the limit */
        return 0;
    }
}
