#ifndef ZIRAN_ZIR_BUNDLE_H
#define ZIRAN_ZIR_BUNDLE_H

#include "zir.h"
#include "zir_string_types.h"

/* Records of the .zx format, defined in the generated compiler_bundle.h. */
struct Capability;
struct Law;
struct Waiver;

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

/* Assets read from a bundle, names and data, share one STORAGE block;
 * collected ones own theirs. */
typedef struct ZxAssets {
    ZxAsset *items;
    size_t count;
    unsigned char *storage;
} ZxAssets;

void ZxAssetsFree(ZxAssets *assets);
int ZxAssetsCollect(ZxAssets *assets, const char *spec);
/* Adds a copy of an asset in name order, as a bundle stores them; 0 when
 * its name is taken or it is too large. */
int ZxAssetsAdd(ZxAssets *assets, const char *name, const void *data, size_t size);
/* Serialize an immutable checked program's portable view directly to OUT.
 * Native templates stay in the source graph; no seeking or temporary payload
 * stream is needed. The program must remain unchanged during measurement/write. */
int BundleWrite(FILE *out, const ZirProgram *program,
                const char *entry_module, const char *entry_function,
                const ZxAssets *assets);
/* Subtract matching declarations, preserving new declarations in shared
 * modules. Conflicting declarations or duplicate library providers reject. */
ZirProgram *BundleLibraryDelta(const ZirProgram *program,
                const ZirProgram *const *libraries, size_t count);
/* Compose disjoint declarations into their original module identities. The
 * result owns private graphs until the host shares validated library bodies. */
ZirProgram *BundleComposeLibraries(const ZirProgram *program,
                const ZirProgram *const *libraries, size_t count);
/* Libraries are ordinary checked bundles. Thin programs retain their own
 * declarations and are checked against the exact libraries supplied by the host. */
int BundleWriteInContext(FILE *out, const ZirProgram *program,
                const ZirProgram *context, const char *entry_module,
                const char *entry_function, const ZxAssets *assets);
/* A bundle's own law and waiver tables. TEXT holds their strings, each
 * followed by a NUL, so hosts read them as C strings. */
typedef struct ZxLawTable {
    struct Law *laws;
    int law_count;
    struct Waiver *waivers;
    int waiver_count;
    char *text;
} ZxLawTable;

void ZxLawTableFree(ZxLawTable *table);
/* Reads the rest of IN, a seekable stream, as one bundle. */
ZirProgram *BundleRead(FILE *in, const char *path,
                          char *entry_module, size_t module_size,
                          char *entry_function, size_t function_size,
                          ZxLawTable *laws, ZxAssets *assets);
/* Optional counts describe the original payload module prefix. Library
 * declarations compose after its per-module function/law prefixes.
 * With *trusted set the caller vouches that this compiler checked these exact
 * bytes before: semantic checking and law evaluation are skipped, and on any
 * failure *trusted is cleared without a diagnostic, so the caller can read
 * the bytes again with checking. */
ZirProgram *BundleReadInContext(const unsigned char *bytes, size_t size, const char *path,
                          char *entry_module, size_t module_size,
                          char *entry_function, size_t function_size,
                          ZxLawTable *laws, ZxAssets *assets,
                          const ZirProgram *const *libraries, size_t count,
                          int *own_module_count, int *trusted);
/* The offset just past a bundle's program: the code it compiles to depends on
 * those bytes alone, not on the assets after them. 0 for a malformed or
 * truncated header. */
size_t BundleProgramEnd(const unsigned char *bytes, size_t size);
/* The rest of IN, a seekable stream, in one block; NULL when it cannot be
 * read. */
unsigned char *BundleReadStream(FILE *in, size_t *size);
/* The host capabilities the first MODULE_COUNT modules of PROGRAM call, in
 * bundle order: called externs that no Ziran provider implements. OWN looks
 * only at a composed module's own declarations, not its library's. The
 * names borrow the program and end in a NUL. */
struct Capability *BundleLinkedCapabilities(const ZirProgram *program, int module_count,
                                            int own, size_t *count);
/* Copies TEXT to *NEXT with a NUL after it and moves *NEXT past both. */
String BundleKeepText(char **next, String text);
/* Runs the checks a trusted read skipped, on a program without libraries. */
int BundleCheckTrusted(ZirProgram **program, int own_module_count, const ZxLawTable *laws);
typedef struct Bundle Bundle;
const ZirProgram *BundleProgram(const Bundle *bundle);
const ZirFunction *BundleEntry(const Bundle *bundle);
/* Instances opened after this run on the portable VM when ENABLED, even
 * where compiled code is available (`zi2zx run --vm`). */
void BundleUsePortableVm(int enabled);
/* Lets the bundle's procedure #asm variants run as native code. Call it
 * only for a bundle built from local source in this process, as
 * `zi2zx run FILE.zi` does; it does nothing for a bundle that links
 * libraries. Every other bundle runs each procedure's portable body. */
void BundleTrustNativeAssembly(Bundle *bundle);
/* Native engine builds: the SHA-256 naming the bundle's compiled code, and
 * its program written as C that a host links ahead of time
 * (ZxNativeWriteLinked), which returns that identity too. */
void BundleIdentity(const Bundle *bundle, unsigned char identity[32]);
int BundleWriteLinked(const Bundle *bundle, const char *directory, unsigned char identity[32]);

#endif
