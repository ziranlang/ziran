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
int BundleWrite(FILE *out, const ZirProgram *program,
                   const char *entry_module, const char *entry_function);
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
                          ZibLawTable *laws);

#endif
