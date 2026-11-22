/* Host calls used by the compiler's path and generated-output code. */
#include <dirent.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

int S15HostOS(void)
{
#if defined(_WIN32)
    return 1;
#elif defined(__APPLE__)
    return 2;
#else
    return 0;
#endif
}

size_t S15PathMax(void)
{
#ifdef _WIN32
    return MAX_PATH;
#else
    return PATH_MAX;
#endif
}

ptrdiff_t S15ReadLink(const char *path, char *output, size_t size)
{
#ifdef _WIN32
    (void)path; (void)output; (void)size;
    return -1;
#else
    return readlink(path, output, size);
#endif
}

char *S15RealPath(const char *path, char *output)
{
#ifdef _WIN32
    (void)path; (void)output;
    return NULL;
#else
    return realpath(path, output);
#endif
}

int S15FileStat(const char *path, void *info, int follow)
{
#ifdef _WIN32
    (void)follow;
    return stat(path, info);
#else
    return follow ? stat(path, info) : lstat(path, info);
#endif
}

int S15StatIsLink(const void *info)
{
#ifdef _WIN32
    (void)info;
    return 0;
#else
    return S_ISLNK(((const struct stat *)info)->st_mode);
#endif
}

int S15DarwinExecutablePath(char *output, uint32_t *size)
{
#ifdef __APPLE__
    return _NSGetExecutablePath(output, size);
#else
    (void)output; (void)size;
    return -1;
#endif
}

void *S15WindowsPathOpen(const char *path)
{
#ifdef _WIN32
    HANDLE file = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    return file == INVALID_HANDLE_VALUE ? NULL : file;
#else
    (void)path;
    return NULL;
#endif
}

uint32_t S15WindowsFinalPath(void *file, char *output, uint32_t capacity)
{
#ifdef _WIN32
    return GetFinalPathNameByHandleA(file, output, capacity, FILE_NAME_NORMALIZED);
#else
    (void)file; (void)output; (void)capacity;
    return 0;
#endif
}

void S15WindowsPathClose(void *file)
{
#ifdef _WIN32
    CloseHandle(file);
#else
    (void)file;
#endif
}

uint32_t S15WindowsExecutablePath(char *output, uint32_t capacity)
{
#ifdef _WIN32
    return GetModuleFileNameA(NULL, output, capacity);
#else
    (void)output; (void)capacity;
    return 0;
#endif
}

uint32_t S15WindowsAttributes(const char *path)
{
#ifdef _WIN32
    return GetFileAttributesA(path);
#else
    (void)path;
    return UINT32_MAX;
#endif
}

char *S15WindowsFullPath(const char *path)
{
#ifdef _WIN32
    return _fullpath(NULL, path, 0);
#else
    (void)path;
    return NULL;
#endif
}

void *CompilerDirectoryOpen(const char *path) { return opendir(path); }

const char *CompilerDirectoryNext(void *directory)
{
    struct dirent *entry = readdir(directory);
    return entry ? entry->d_name : NULL;
}

void CompilerDirectoryClose(void *directory) { closedir(directory); }

int CompilerPathEntryKind(const char *path)
{
    struct stat info;
#ifdef _WIN32
    int listed = stat(path, &info) == 0;
#else
    int listed = lstat(path, &info) == 0;
#endif
    if(!listed) return 0;
    if(S_ISREG(info.st_mode)) return 1;
    return S_ISDIR(info.st_mode) ? 2 : 0;
}
