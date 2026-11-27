/* Native streams and paths, and the existing C types at the call boundary. */
#include "zir_memory.h"
#include "ziran_bootstrap_runtime.h"
#include "zir_stream.h"
#include "compiler_load.h"
#include "compiler_zx_assets.h"

#ifndef ZIRAN_STD_DIR
#define ZIRAN_STD_DIR ""
#endif
#ifndef ZIRAN_INCLUDE_DIR
#define ZIRAN_INCLUDE_DIR ""
#endif

const char *CompilerBuiltinDirectory(int kind)
{
#ifdef __ANDROID__
    /* Build-host paths cannot locate modules on an Android device and make
     * independently built APKs differ. The loader still accepts explicit
     * ZIRAN_STD/ZIRAN_INCLUDE and directories beside the installed runner. */
    (void)kind;
    return "";
#else
    return kind == 0 ? ZIRAN_STD_DIR : ZIRAN_INCLUDE_DIR;
#endif
}

int CompilerMemoryStreams(void)
{
#ifdef ZIR_MEMORY_STREAMS
    return 1;
#else
    return 0;
#endif
}

FILE *CompilerOpenMemoryOutput(unsigned char **data, size_t *size)
{
#ifdef ZIR_MEMORY_STREAMS
    return ZirWriteMemory(data, size);
#else
    (void)data;
    (void)size;
    return NULL;
#endif
}

int ProgramWrite(const ZirProgram *program, FILE *out)
{
    return compiler_zir_serial_ProgramWrite((ZirProgram *)program, out);
}

ZirProgram *ProgramRead(FILE *in, const char *path)
{
    return compiler_zir_serial_ProgramRead(in, (uint8_t *)path);
}

int PathIsIR(const char *path)
{
    return compiler_zir_serial_PathIsIR((uint8_t *)path);
}

ZirProgram *ProgramLoad(const char *path, const char *root)
{
    return compiler_zir_serial_ProgramLoad((uint8_t *)path, (uint8_t *)root);
}

int CheckCanonicalPrograms(ZirProgram **programs, int count,
                           const char *const *input_paths)
{
    return compiler_zir_serial_CheckCanonicalPrograms(programs, count, (uint8_t **)input_paths);
}

const char *ToolchainStandardDirectory(void)
{
    return (const char *)compiler_load_ToolchainStandardDirectory();
}

const char *ToolchainIncludeDirectory(void)
{
    return (const char *)compiler_load_ToolchainIncludeDirectory();
}

int ProgramsLoad(ProgramSet *set, const char *root, const char *const *module_paths,
                 int module_path_count, const char *const *inputs, int input_count)
{
    return compiler_load_ProgramsLoad(set, (uint8_t *)root, (uint8_t **)module_paths,
                                      module_path_count, (uint8_t **)inputs, input_count);
}

int ProgramsLoadWithDefines(ProgramSet *set, const char *root,
                            const char *const *module_paths, int module_path_count,
                            const char *const *defines, int define_count,
                            const char *const *inputs, int input_count)
{
    return compiler_load_ProgramsLoadWithDefines(set, (uint8_t *)root, (uint8_t **)module_paths,
        module_path_count, (uint8_t **)defines, define_count, (uint8_t **)inputs, input_count);
}

ZirPackageMap *PackageMapLoad(const char *path)
{
    return compiler_package_resolve_PackageMapLoad((uint8_t *)path);
}

const char *PackageOwner(const ZirPackageMap *map, const char *path)
{
    return (const char *)compiler_package_resolve_PackageOwner((ZirPackageMap *)map, (uint8_t *)path);
}

const char *PackageModuleName(const ZirPackageMap *map, const char *path)
{
    return (const char *)compiler_package_resolve_PackageModuleName((ZirPackageMap *)map, (uint8_t *)path);
}

const char *PackageAmbiguity(const ZirPackageMap *map, const char *owner, const char *visible)
{
    return (const char *)compiler_package_resolve_PackageAmbiguity(
        (ZirPackageMap *)map, (uint8_t *)owner, (uint8_t *)visible);
}

int PackageDependencyModule(const ZirPackageMap *map, const char *owner,
                            const char *qualified, char *identity, size_t identity_size)
{
    return compiler_package_resolve_PackageDependencyModule((ZirPackageMap *)map,
        (uint8_t *)owner, (uint8_t *)qualified, (uint8_t *)identity, identity_size);
}

int PackageResolve(const ZirPackageMap *map, const char *owner, const char *visible,
                   char *path, size_t path_size, char *identity, size_t identity_size)
{
    return compiler_package_resolve_PackageResolve((ZirPackageMap *)map, (uint8_t *)owner,
        (uint8_t *)visible, (uint8_t *)path, path_size, (uint8_t *)identity, identity_size);
}

int PackageResolveSaved(const ZirPackageMap *map, const char *owner, const char *identity,
                        char *path, size_t path_size)
{
    return compiler_package_resolve_PackageResolveSaved((ZirPackageMap *)map, (uint8_t *)owner,
        (uint8_t *)identity, (uint8_t *)path, path_size);
}

int ZxAssetsAdd(ZxAssets *assets, const char *name, const void *data, size_t size)
{
    return compiler_zx_assets_ZxAssetsAdd(assets, (uint8_t *)name, (void *)data, size);
}

int ZxAssetsCollect(ZxAssets *assets, const char *spec)
{
    return compiler_zx_assets_ZxAssetsCollect(assets, (uint8_t *)spec);
}
