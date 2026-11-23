/* Host calls used by the compiler's path and generated-output code. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "zir_files.h"

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

int CompilerDirectoryCloseStatus(void *directory) { return closedir(directory); }

void CompilerDirectoryClose(void *directory) { (void)CompilerDirectoryCloseStatus(directory); }

static int entry_kind(const struct stat *info)
{
    if(S_ISREG(info->st_mode)) return 1;
    return S_ISDIR(info->st_mode) ? 2 : 0;
}

int CompilerPathEntryKind(const char *path)
{
    struct stat info;
#ifdef _WIN32
    int listed = stat(path, &info) == 0;
#else
    int listed = lstat(path, &info) == 0;
#endif
    return listed ? entry_kind(&info) : 0;
}

int CompilerPathEntryKindNoLinks(const char *path)
{
    struct stat info;
    return FileStatusNoLinks(path, &info) == 0 ? entry_kind(&info) : 0;
}

int CompilerPathIsRegular(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

/* The ziran api example sidecar read: 0 when an optional file is missing, 1
 * when BYTES were read from a regular file (a link, directory or pipe fails),
 * -1 for any other failure. The caller owns all length and content checks. */
int ZiranApiReadExampleFile(const char *path, int optional, unsigned char *text,
                            size_t capacity, size_t *bytes)
{
    int flags = O_RDONLY;
#ifdef O_NONBLOCK
    flags |= O_NONBLOCK;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
#ifdef O_BINARY
    flags |= O_BINARY;
#endif
    int descriptor = open(path, flags);
    if(descriptor < 0 && optional && errno == ENOENT) return 0;
    struct stat info;
    FILE *file = descriptor >= 0 && FileStatusNoLinks(path, &info) == 0 &&
                 fstat(descriptor, &info) == 0 && S_ISREG(info.st_mode) ?
                 fdopen(descriptor, "rb") : NULL;
    if(file == NULL) {
        if(descriptor >= 0) close(descriptor);
        return -1;
    }
    *bytes = fread(text, 1, capacity, file);
    int failed = ferror(file);
    if(fclose(file) != 0 || failed) return -1;
    return 1;
}

int64_t CompilerFileTell(FILE *file) { return ftell(file); }

int CompilerFileSeek(FILE *file, int64_t offset, int origin)
{
    if(offset < LONG_MIN || offset > LONG_MAX) return -1;
    return fseek(file, (long)offset, origin);
}

/* The device and inode of PATH and whether it is a regular file: 0, 1 when
 * nothing is there, or -1 for another failure. */
int S15FileIdentity(const char *path, uint64_t *device, uint64_t *inode, int *regular)
{
    struct stat info;
    if(stat(path, &info) != 0) return errno == ENOENT || errno == ENOTDIR ? 1 : -1;
    *device = (uint64_t)info.st_dev;
    *inode = (uint64_t)info.st_ino;
    *regular = S_ISREG(info.st_mode) != 0;
    return 0;
}
