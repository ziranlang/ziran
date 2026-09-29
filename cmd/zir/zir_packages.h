#ifndef ZIRAN_PACKAGES_H
#define ZIRAN_PACKAGES_H

#include "zir.h"

typedef struct ZirPackageMap ZirPackageMap;

/* The launcher supplies a complete, locked module graph. No filesystem
 * search-order decisions are made when this map is active. */
ZirPackageMap *PackageMapLoad(const char *path);
void PackageMapFree(ZirPackageMap *map);
const char *PackageOwner(const ZirPackageMap *map, const char *path);
const char *PackageModuleName(const ZirPackageMap *map,
                              const char *path);
/* The package-qualified spellings of a short import that several direct
 * dependencies export, or NULL when the short name is not ambiguous. */
const char *PackageAmbiguity(const ZirPackageMap *map, const char *owner,
                             const char *visible);
/* Any module of a direct dependency by its import path, such as
 * kryon/raster_text, exported or not. Host bindings use this so a test host
 * can provide a capability that a dependency declares internally. */
int PackageDependencyModule(const ZirPackageMap *map, const char *owner,
                            const char *qualified, char *identity,
                            size_t identity_size);
int PackageResolve(const ZirPackageMap *map, const char *owner,
                   const char *visible, char *path, size_t path_size,
                   char *identity, size_t identity_size);

#endif
