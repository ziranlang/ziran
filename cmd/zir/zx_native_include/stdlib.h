#ifndef ZX_STDLIB_H
#define ZX_STDLIB_H
#include <stddef.h>
void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *pointer, size_t size);
void free(void *pointer);
void abort(void);
void exit(int status);
int atoi(const char *text);
long strtol(const char *text, char **end, int base);
char *getenv(const char *name);
double strtod(const char *text, char **end);
float strtof(const char *text, char **end);
#endif
