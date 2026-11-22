#ifndef ZIR_SCALAR_H
#define ZIR_SCALAR_H

#include "compiler_scalar.h"

/* C callers keep read-only spellings while the implementation lives in Ziran. */
static inline unsigned ScalarWidth(const char *type)
{
    return ScalarWidthZiran((void *)type);
}

static inline int64_t ScalarStored(uint64_t bits, const char *type)
{
    return ScalarStoredZiran(bits, (void *)type);
}

#endif
