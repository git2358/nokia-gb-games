/* The build is freestanding; common/platform/gba/libc.c holds these. */
#ifndef GBA_STRING_H
#define GBA_STRING_H

#include <stddef.h>

void *memset(void *dst, int value, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

#endif
