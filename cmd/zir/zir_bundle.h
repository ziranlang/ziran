#ifndef ZIRAN_ZIR_BUNDLE_H
#define ZIRAN_ZIR_BUNDLE_H

#include "zir.h"

/* Linked portable program and certificates. The reader owns the returned IR. */
ZirProgram *BundleLink(const ZirProgram *program, const char *entry_module,
                       const char *entry_function);
/* Native entry pruning also retains exported implementations of host effects. */
ZirProgram *NativeLink(const ZirProgram *program, const char *entry_module,
                       const char *entry_function);
/* Go entry pruning also retains methods of reachable receiver records. */
ZirProgram *NativeGoLink(const ZirProgram *program, const char *entry_module,
                         const char *entry_function);
/* Bind one declared host capability to an exported Ziran implementation. */
int BindHostProvider(ZirProgram *program, const char *spec);
int BindHostModule(ZirProgram *program, const char *spec);
typedef struct ZibAsset {
    char *name;
    unsigned char *data;
    uint32_t size;
} ZibAsset;

typedef struct ZibAssets {
    ZibAsset *items;
    size_t count;
} ZibAssets;

void ZibAssetsFree(ZibAssets *assets);
int ZibAssetsCollect(ZibAssets *assets, const char *spec);
int ZibAssetsWrite(FILE *out, const ZibAssets *assets);
int ZibAssetsRead(FILE *in, ZibAssets *assets);
int BundleWrite(FILE *out, const ZirProgram *program,
                const char *entry_module, const char *entry_function,
                const ZibAssets *assets);
/* Libraries are ordinary checked bundles. Thin programs retain their own
 * modules and are checked against the exact libraries supplied by the host. */
int BundleWriteInContext(FILE *out, const ZirProgram *program,
                const ZirProgram *context, const char *entry_module,
                const char *entry_function, const ZibAssets *assets);
typedef struct ZibLawRecord {
    char module[ZIR_NAME_MAX];
    char name[ZIR_NAME_MAX];
    char kind[16];
    char status[16];
    char detail[ZIR_TEXT_MAX];
    ZirLawEvidence evidence;
} ZibLawRecord;

typedef struct ZibLawWaiverRecord {
    char module[ZIR_NAME_MAX];
    char name[ZIR_NAME_MAX];
    char reason[ZIR_TEXT_MAX];
} ZibLawWaiverRecord;

typedef struct ZibLawTable {
    ZibLawRecord *laws;
    int law_count;
    ZibLawWaiverRecord *waivers;
    int waiver_count;
} ZibLawTable;

void ZibLawTableFree(ZibLawTable *table);
ZirProgram *BundleRead(FILE *in, const char *path,
                          char *entry_module, size_t module_size,
                          char *entry_function, size_t function_size,
                          ZibLawTable *laws, ZibAssets *assets);
ZirProgram *BundleReadInContext(FILE *in, const char *path,
                          char *entry_module, size_t module_size,
                          char *entry_function, size_t function_size,
                          ZibLawTable *laws, ZibAssets *assets,
                          const ZirProgram *const *libraries, size_t count,
                          int *own_module_count);
typedef struct Bundle Bundle;
const ZirProgram *BundleProgram(const Bundle *bundle);

#endif
