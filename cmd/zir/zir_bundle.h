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
int ZxAssetsWrite(FILE *out, const ZxAssets *assets);
int ZxAssetsRead(FILE *in, ZxAssets *assets);
/* Serialize an immutable checked program's portable view directly to OUT.
 * Native templates stay in the source graph; no seeking or temporary payload
 * stream is needed. The program must remain unchanged during measurement/write. */
int BundleWrite(FILE *out, const ZirProgram *program,
                const char *entry_module, const char *entry_function,
                const ZxAssets *assets);
/* Libraries are ordinary checked bundles. Thin programs retain their own
 * modules and are checked against the exact libraries supplied by the host. */
int BundleWriteInContext(FILE *out, const ZirProgram *program,
                const ZirProgram *context, const char *entry_module,
                const char *entry_function, const ZxAssets *assets);
typedef struct ZxLawRecord {
    char module[ZIR_NAME_MAX];
    char name[ZIR_NAME_MAX];
    char kind[16];
    char status[16];
    char detail[ZIR_TEXT_MAX];
    ZirLawEvidence evidence;
} ZxLawRecord;

typedef struct ZxLawWaiverRecord {
    char module[ZIR_NAME_MAX];
    char name[ZIR_NAME_MAX];
    char reason[ZIR_TEXT_MAX];
} ZxLawWaiverRecord;

typedef struct ZxLawTable {
    ZxLawRecord *laws;
    int law_count;
    ZxLawWaiverRecord *waivers;
    int waiver_count;
} ZxLawTable;

void ZxLawTableFree(ZxLawTable *table);
ZirProgram *BundleRead(FILE *in, const char *path,
                          char *entry_module, size_t module_size,
                          char *entry_function, size_t function_size,
                          ZxLawTable *laws, ZxAssets *assets);
/* Optional counts describe the validated payload before libraries are appended. */
ZirProgram *BundleReadInContext(FILE *in, const char *path,
                          char *entry_module, size_t module_size,
                          char *entry_function, size_t function_size,
                          ZxLawTable *laws, ZxAssets *assets,
                          const ZirProgram *const *libraries, size_t count,
                          int *own_module_count, size_t *own_capability_count);
typedef struct Bundle Bundle;
const ZirProgram *BundleProgram(const Bundle *bundle);
const ZirFunction *BundleEntry(const Bundle *bundle);

#endif
