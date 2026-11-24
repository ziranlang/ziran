/* POSIX open flags and struct stat fields for compiler_edit. The editor's
 * policy and path walking live in cmd/compiler_edit.zi; this file only names
 * the host's flags and stat layout so one generated seed serves every host.
 * A Windows host has no O_NOFOLLOW/O_DIRECTORY/openat walk: the editor reports
 * the path as unavailable there, while the compiler itself stays portable. */
#include "ziran_bootstrap_runtime.h"
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <io.h>
#include <stdlib.h>

/* The editor's POSIX atomic write is unavailable on Windows, but the portable
 * compiler archive still names these calls; keep failing definitions so the
 * runtime boundary links and the editor reports the path as unavailable. */
int fchmod(int descriptor, int mode)
{
    (void)descriptor; (void)mode;
    return -1;
}

int fsync(int descriptor)
{
    return _commit(descriptor);
}

long long pread(int descriptor, void *output, size_t count, long long offset)
{
    (void)descriptor; (void)output; (void)count; (void)offset;
    return -1;
}

char *realpath(const char *path, char *resolved)
{
    return _fullpath(resolved, path, 4096);
}

int renameat(int from_directory, const char *from, int to_directory, const char *to)
{
    (void)from_directory; (void)from; (void)to_directory; (void)to;
    return -1;
}

int unlinkat(int directory, const char *name, int flags)
{
    (void)directory; (void)name; (void)flags;
    return -1;
}

int ZiranEditHostOpenSource(const char *path)
{
    (void)path;
    return -1;
}

int ZiranEditHostOpenDirectory(const char *path)
{
    (void)path;
    return -1;
}

int ZiranEditHostOpenSubdirectory(int parent, const char *name)
{
    (void)parent; (void)name;
    return -1;
}

int ZiranEditHostOpenLeaf(int parent, const char *name)
{
    (void)parent; (void)name;
    return -1;
}

int ZiranEditHostCreatePrivate(int parent, const char *name)
{
    (void)parent; (void)name;
    return -1;
}

int ZiranEditHostFdInfo(int descriptor, int64_t *size, uint32_t *mode,
                        int *regular, uint64_t *device, uint64_t *inode)
{
    (void)descriptor; (void)size; (void)mode; (void)regular; (void)device; (void)inode;
    return -1;
}

int ZiranEditHostAtInfo(int directory, const char *name, uint64_t *device,
                        uint64_t *inode, uint32_t *mode, int *regular,
                        uint64_t *links, int64_t *size)
{
    (void)directory; (void)name; (void)device; (void)inode; (void)mode;
    (void)regular; (void)links; (void)size;
    return -1;
}

int ZiranEditHostPathIsDirectory(const char *path)
{
    struct stat info;
    return path != 0 && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}
#else
int ZiranEditHostOpenSource(const char *path)
{
    return open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
}

int ZiranEditHostOpenDirectory(const char *path)
{
    return open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
}

int ZiranEditHostOpenSubdirectory(int parent, const char *name)
{
    return openat(parent, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
}

int ZiranEditHostOpenLeaf(int parent, const char *name)
{
    return openat(parent, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
}

int ZiranEditHostCreatePrivate(int parent, const char *name)
{
    return openat(parent, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
}

/* 0 when the descriptor is a regular file, else -1. */
int ZiranEditHostFdInfo(int descriptor, int64_t *size, uint32_t *mode,
                        int *regular, uint64_t *device, uint64_t *inode)
{
    struct stat info;
    if(fstat(descriptor, &info) != 0) return -1;
    *size = (int64_t)info.st_size;
    *mode = (uint32_t)info.st_mode;
    *regular = S_ISREG(info.st_mode) != 0;
    *device = (uint64_t)info.st_dev;
    *inode = (uint64_t)info.st_ino;
    return 0;
}

/* 0 when NAME is there, else -1; the fields are only set on success. */
int ZiranEditHostAtInfo(int directory, const char *name, uint64_t *device,
                        uint64_t *inode, uint32_t *mode, int *regular,
                        uint64_t *links, int64_t *size)
{
    struct stat info;
    if(fstatat(directory, name, &info, AT_SYMLINK_NOFOLLOW) != 0) return -1;
    *device = (uint64_t)info.st_dev;
    *inode = (uint64_t)info.st_ino;
    *mode = (uint32_t)info.st_mode;
    *regular = S_ISREG(info.st_mode) != 0;
    *links = (uint64_t)info.st_nlink;
    *size = (int64_t)info.st_size;
    return 0;
}

int ZiranEditHostPathIsDirectory(const char *path)
{
    struct stat info;
    return path != 0 && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}
#endif
