/* POSIX open flags and struct stat fields for compiler_edit. The editor's
 * policy and path walking live in cmd/compiler_edit.zi; this file only names
 * the host's flags and stat layout so one generated seed serves every host. */
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

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
