/* Bundles compiled by the native ZX engine see only these C headers. */
#ifndef ZX_STDDEF_H
#define ZX_STDDEF_H
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#define NULL ((void *)0)
#define offsetof(type, field) ((size_t)&((type *)0)->field)
#endif
