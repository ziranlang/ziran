#include "zir_files.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

int main(void)
{
    assert(!PathIsAbsolute(NULL));
    assert(!PathIsAbsolute(""));
    assert(!PathIsAbsolute("relative/file"));
    assert(PathIsAbsolute("/absolute/file"));
#ifdef _WIN32
    assert(PathIsAbsolute("C:/absolute/file"));
    assert(PathIsAbsolute("C:\\absolute\\file"));
    assert(PathIsAbsolute("\\\\server\\share\\file"));
    assert(!PathIsAbsolute("C:relative"));
#else
    assert(!PathIsAbsolute("C:/absolute/file"));
#endif
    char executable[8192];
    assert(ExecutablePath(executable, sizeof(executable)) > 0);
    assert(PathIsAbsolute(executable));
    char tiny[2];
    assert(ExecutablePath(tiny, sizeof(tiny)) == 0 && tiny[0] == '\0');
    char *canonical = CanonicalPath(executable);
#ifdef _WIN32
    if(canonical == NULL)
        fprintf(stderr, "CanonicalPath executable failed: %lu, %s\n",
            (unsigned long)GetLastError(), executable);
#endif
    assert(canonical != NULL && PathIsAbsolute(canonical));
    free(canonical);
    canonical = CanonicalPath(".");
    assert(canonical != NULL && PathIsAbsolute(canonical));
    free(canonical);
    assert(CanonicalPath("missing-input.txt") == NULL);
    char *prefix = DuplicatePrefix("hello", 3);
    assert(prefix != NULL && strcmp(prefix, "hel") == 0);
    free(prefix);
    prefix = DuplicatePrefix("hello", 50);
    assert(prefix != NULL && strcmp(prefix, "hello") == 0);
    free(prefix);
    FILE *output = fopen("input.txt", "wb");
    assert(output != NULL);
    for(int i = 0; i < 4096; i++) assert(fputc('x', output) == 'x');
    assert(fputs("\nlast", output) >= 0);
    assert(fclose(output) == 0);
    FILE *input = fopen("input.txt", "rb");
    assert(input != NULL);
    char *line = NULL;
    size_t capacity = 0;
    assert(ReadFileLine(input, &line, &capacity) == 1);
    assert(strlen(line) == 4097 && line[4096] == '\n');
    assert(ReadFileLine(input, &line, &capacity) == 1);
    assert(strcmp(line, "last") == 0);
    assert(ReadFileLine(input, &line, &capacity) == 0);
    free(line);
    assert(fclose(input) == 0);
    struct stat info;
    assert(FileStatusNoLinks("input.txt", &info) == 0);
    assert(S_ISREG(info.st_mode) && info.st_size == 4101);
    assert(FileStatusNoLinks("missing-input.txt", &info) != 0);
#ifdef _WIN32
    int link_created = CreateSymbolicLinkA("link.txt", "input.txt", 0);
    if(link_created && GetFileAttributesA("link.txt") == INVALID_FILE_ATTRIBUTES) {
        /* Older Wine reports success from its unimplemented symlink API. */
        assert(GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL);
        fputs("Wine symlink creation unavailable; native symlink checks retained\n", stderr);
        link_created = 0;
    }
#else
    int link_created = symlink("input.txt", "link.txt") == 0;
    assert(link_created);
#endif
    if(link_created) {
        assert(FileStatusNoLinks("link.txt", &info) != 0);
        char *target = CanonicalPath("input.txt");
        char *link = CanonicalPath("link.txt");
        if(target == NULL || link == NULL || strcmp(target, link) != 0)
            fprintf(stderr, "canonical link mismatch: %s / %s\n",
                target ? target : "missing", link ? link : "missing");
        assert(target != NULL && link != NULL && strcmp(target, link) == 0);
        free(target);
        free(link);
        assert(remove("link.txt") == 0);
    }
    assert(remove("input.txt") == 0);
    return 0;
}
