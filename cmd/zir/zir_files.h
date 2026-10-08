#ifndef ZIR_FILES_H
#define ZIR_FILES_H

#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>

int PathIsAbsolute(const char *path);
char *CanonicalPath(const char *path);
int ExecutablePath(char *output, size_t capacity);
char *DuplicatePrefix(const char *text, size_t limit);
/* One line, end of file, or failure: 1, 0, or -1. */
int ReadFileLine(FILE *file, char **line, size_t *capacity);
int FileStatusNoLinks(const char *path, struct stat *info);

#endif
