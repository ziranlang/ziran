/* Explicit execution policy for tier benchmarks sharing site_target_host.c.
 * Include before the host source so its historical Auto measurements remain
 * reproducible while new receipts select the tier they actually name. */
#ifndef SITE_TARGET_BACKEND_H
#define SITE_TARGET_BACKEND_H
#include "ziran_host.h"
#ifndef MATRIX_ZX_BACKEND
#error "select a benchmark execution backend"
#endif
static inline BundleInstance *MatrixInstantiate(const Bundle *bundle,
        const HostBinding *bindings, size_t count)
{
    const BundleInstanceOptions options = {MATRIX_ZX_BACKEND, 0};
    return BundleInstantiateWithOptions(bundle, bindings, count, &options);
}
#define BundleInstantiate MatrixInstantiate
#endif
