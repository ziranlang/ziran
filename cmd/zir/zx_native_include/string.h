#ifndef ZX_STRING_H
#define ZX_STRING_H
#include <stddef.h>
void *memcpy(void *destination, const void *source, size_t size);
void *memmove(void *destination, const void *source, size_t size);
void *memset(void *destination, int c, size_t size);
int memcmp(const void *a, const void *b, size_t size);
size_t strlen(const char *text);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t size);
#endif
