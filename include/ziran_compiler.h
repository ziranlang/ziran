#ifndef ZIRAN_COMPILER_H
#define ZIRAN_COMPILER_H

/* Compiler annotations shared by generated translation units and headers. */
#if defined(__GNUC__) || defined(__clang__)
#define ZIRAN_ALWAYS_INLINE __attribute__((always_inline))
#define ZIRAN_COLD __attribute__((noinline, cold))
#define ZIRAN_MAYBE_UNUSED __attribute__((unused))
#else
#define ZIRAN_ALWAYS_INLINE
#define ZIRAN_COLD
#define ZIRAN_MAYBE_UNUSED
#endif

#endif
