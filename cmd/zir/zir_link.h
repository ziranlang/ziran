#ifndef ZIRAN_ZIR_LINK_H
#define ZIRAN_ZIR_LINK_H

#include "zir.h"
#include "zir_string_types.h"

/* Native entry pruning also retains exported implementations of host effects. */
ZirProgram *NativeLink(const ZirProgram *program, const char *entry_module,
                       const char *entry_function);
/* Go entry pruning also retains methods of reachable receiver records. */
ZirProgram *NativeGoLink(const ZirProgram *program, const char *entry_module,
                         const char *entry_function);
/* Bind one declared host capability to an exported Ziran implementation. */
int BindHostProvider(ZirProgram *program, const char *spec);
int BindHostModule(ZirProgram *program, const char *spec);

/* Files `zi2zx bundle --asset-dir` stores in a bundle. */
typedef struct ZxAsset {
    char *name;
    unsigned char *data;
    uint32_t size;
} ZxAsset;

typedef struct ZxAssets {
    ZxAsset *items;
    size_t count;
} ZxAssets;

void ZxAssetsFree(ZxAssets *assets);
int ZxAssetsCollect(ZxAssets *assets, const char *spec);
/* Adds a copy of an asset in name order, as a bundle stores them; 0 when
 * its name is taken or it is too large. */
int ZxAssetsAdd(ZxAssets *assets, const char *name, const void *data, size_t size);

#endif
