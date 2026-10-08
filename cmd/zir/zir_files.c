#include "zir_files.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

int PathIsAbsolute(const char *path)
{
    if(path == NULL || path[0] == '\0') return 0;
    if(path[0] == '/') return 1;
#ifdef _WIN32
    if(path[0] == '\\' && path[1] == '\\') return 1;
    return isalpha((unsigned char)path[0]) && path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\');
#else
    return 0;
#endif
}

char *CanonicalPath(const char *path)
{
#ifdef _WIN32
    HANDLE file = CreateFileA(path, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if(file == INVALID_HANDLE_VALUE) return NULL;
    DWORD capacity = MAX_PATH + 1;
    char *result = malloc((size_t)capacity);
    DWORD length = result ? GetFinalPathNameByHandleA(file, result, capacity,
        FILE_NAME_NORMALIZED) : 0;
    if(length >= capacity && length < UINT32_MAX) {
        capacity = length + 1;
        char *grown = realloc(result, (size_t)capacity);
        if(grown != NULL) {
            result = grown;
            length = GetFinalPathNameByHandleA(file, result, capacity,
                FILE_NAME_NORMALIZED);
        } else {
            length = 0;
        }
    }
    CloseHandle(file);
    if(length == 0 || length >= capacity) {
        free(result);
        return NULL;
    }
    if(strncmp(result, "\\\\?\\UNC\\", 8) == 0) {
        memmove(result + 2, result + 8, (size_t)length - 7);
        result[0] = '/';
        result[1] = '/';
    } else if(strncmp(result, "\\\\?\\", 4) == 0) {
        memmove(result, result + 4, (size_t)length - 3);
    }
    for(char *p = result; *p; p++)
        if(*p == '\\') *p = '/';
    return result;
#else
    return realpath(path, NULL);
#endif
}

int ExecutablePath(char *output, size_t capacity)
{
    if(output == NULL || capacity < 2 || capacity > INT_MAX) return 0;
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, output, (DWORD)capacity);
    if(length == 0 || length >= capacity) {
        output[0] = '\0';
        return 0;
    }
    for(DWORD i = 0; i < length; i++)
        if(output[i] == '\\') output[i] = '/';
#else
    ssize_t length = readlink("/proc/self/exe", output, capacity - 1);
    if(length <= 0 || (size_t)length >= capacity - 1) {
        output[0] = '\0';
        return 0;
    }
#endif
    output[length] = '\0';
    return (int)length;
}

char *DuplicatePrefix(const char *text, size_t limit)
{
    size_t length = 0;
    while(length < limit && text[length] != '\0') length++;
    if(length == SIZE_MAX) return NULL;
    char *result = malloc(length + 1);
    if(result != NULL) {
        memcpy(result, text, length);
        result[length] = '\0';
    }
    return result;
}

int ReadFileLine(FILE *file, char **line, size_t *capacity)
{
    size_t length = 0;
    int value;
    while((value = fgetc(file)) != EOF) {
        if(length + 1 >= *capacity) {
            size_t next = *capacity < 128 ? 128 : *capacity * 2;
            if(next <= *capacity) return -1;
            char *grown = realloc(*line, next);
            if(grown == NULL) return -1;
            *line = grown;
            *capacity = next;
        }
        (*line)[length++] = (char)value;
        if(value == '\n') break;
    }
    if(ferror(file)) return -1;
    if(length == 0) return 0;
    (*line)[length] = '\0';
    return 1;
}

int FileStatusNoLinks(const char *path, struct stat *info)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    if(attributes == INVALID_FILE_ATTRIBUTES ||
       (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) return -1;
    return stat(path, info);
#else
    if(lstat(path, info) != 0 || S_ISLNK(info->st_mode)) return -1;
    return 0;
#endif
}
