#include "zir_c_build.h"
#include "zir_c_cache.h"
#include "zir.h"
#include "zir_diagnostic.h"
#include "zir_load.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

enum { MAX_ARGS = 1024, MAX_JOBS = 4 };
typedef struct Job { pid_t pid; int fd; } Job;
typedef struct ToolLog { char text[ZIR_TEXT_MAX]; size_t used; int truncated; } ToolLog;
typedef struct Files { char **items; int count; } Files;

static void append_log(ToolLog *log, const char *bytes, size_t length)
{
    size_t copied = length;
    if(copied > sizeof(log->text) - 1 - log->used) {
        copied = sizeof(log->text) - 1 - log->used;
        log->truncated = 1;
    }
    for(size_t i = 0; i < copied; i++) log->text[log->used++] = bytes[i] ? bytes[i] : '?';
}

static int directory(const char *path)
{
    char copy[ZIR_PATH_MAX];
    if(strlen(path) >= sizeof(copy)) return 0;
    strcpy(copy, path);
    for(char *p = copy + 1; *p; p++) {
        if(*p != '/') continue;
        *p = 0;
        if(mkdir(copy, 0755) != 0 && errno != EEXIST) return 0;
        *p = '/';
    }
    return mkdir(copy, 0755) == 0 || errno == EEXIST;
}

int c_build_lock(const char *out)
{
    char path[ZIR_PATH_MAX];
    if(!directory(out) || snprintf(path, sizeof(path), "%s/.build.lock", out) >= (int)sizeof(path))
        return -1;
    int flags = O_CREAT | O_RDWR;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    int fd = open(path, flags, 0600);
    if(fd < 0) return -1;
    struct stat info;
    if(fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) { close(fd); return -1; }
    int status;
    do { status = flock(fd, LOCK_EX); } while(status < 0 && errno == EINTR);
    if(status < 0) { close(fd); return -1; }
    /* Compiler children inherit this lock so a killed parent cannot expose
     * still-running object writers to another build of the same directory. */
    return fd;
}

static int words(const char *text, char **args, int count)
{
    if(!text) return count;
    char *copy = strdup(text);
    if(!copy) return -1;
    for(char *word = strtok(copy, " \t"); word; word = strtok(NULL, " \t")) {
        if(count >= MAX_ARGS - 1 || !(args[count] = strdup(word))) { free(copy); return -1; }
        count++;
    }
    free(copy);
    return count;
}

static int add_file(Files *files, const char *path)
{
    size_t length = strlen(path);
    if(length < 2 || strcmp(path + length - 2, ".c")) return 1;
    for(int i = 0; i < files->count; i++) if(!strcmp(files->items[i], path)) return 1;
    char **grown = realloc(files->items, (size_t)(files->count + 1) * sizeof(*grown));
    if(!grown) return 0;
    files->items = grown;
    files->items[files->count] = strdup(path);
    if(!files->items[files->count]) return 0;
    files->count++;
    return 1;
}

static int compare_paths(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static Job start(char **args, int capture)
{
    int pipes[2] = {-1, -1};
    if(capture && pipe(pipes) != 0) return (Job){-1, -1};
    if(capture) {
        fcntl(pipes[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipes[1], F_SETFD, FD_CLOEXEC);
    }
    fflush(NULL);
    pid_t pid = fork();
    if(pid == 0) {
        if(capture) {
            close(pipes[0]);
            if(dup2(pipes[1], STDOUT_FILENO) < 0 || dup2(pipes[1], STDERR_FILENO) < 0) _exit(127);
            if(pipes[1] > STDERR_FILENO) close(pipes[1]);
        }
        execvp(args[0], args);
        dprintf(STDERR_FILENO, "cannot run %s: %s\n", args[0], strerror(errno));
        _exit(127);
    }
    if(capture) {
        close(pipes[1]);
        if(pid < 0) close(pipes[0]);
    }
    return (Job){pid, pid > 0 ? pipes[0] : -1};
}

static int finish(Job *jobs, int count, ToolLog *log)
{
    int ok = 1, pending;
    int capture = 0;
    for(int i = 0; i < count; i++) capture |= jobs[i].fd >= 0;
    ToolLog *outputs = capture ? calloc((size_t)count, sizeof(*outputs)) : NULL;
    if(capture && !outputs) ok = 0;
    /* Drain every pipe, even after truncating the displayed log. A compiler
     * must never block on stderr while another job is being waited for. */
    do {
        struct pollfd pollers[MAX_JOBS];
        pending = 0;
        for(int i = 0; i < count; i++) {
            pollers[i] = (struct pollfd){jobs[i].fd, POLLIN, 0};
            pending += jobs[i].fd >= 0;
        }
        if(!pending) break;
        int ready;
        do { ready = poll(pollers, (nfds_t)count, -1); } while(ready < 0 && errno == EINTR);
        if(ready < 0) { ok = 0; break; }
        for(int i = 0; i < count; i++) {
            if(jobs[i].fd < 0 || !pollers[i].revents) continue;
            char bytes[4096];
            ssize_t length;
            do { length = read(jobs[i].fd, bytes, sizeof(bytes)); } while(length < 0 && errno == EINTR);
            if(length <= 0) {
                if(length < 0) ok = 0;
                close(jobs[i].fd); jobs[i].fd = -1; continue;
            }
            if(outputs) append_log(&outputs[i], bytes, (size_t)length);
        }
    } while(pending);
    for(int i = 0; i < count; i++) {
        if(outputs && outputs[i].used) {
            if(log->used && log->text[log->used - 1] != '\n') append_log(log, "\n", 1);
            append_log(log, outputs[i].text, outputs[i].used);
            log->truncated |= outputs[i].truncated;
        }
        if(jobs[i].fd >= 0) close(jobs[i].fd);
        int status = 0;
        pid_t waited;
        do { waited = jobs[i].pid > 0 ? waitpid(jobs[i].pid, &status, 0) : -1; }
        while(waited < 0 && errno == EINTR);
        if(waited <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) ok = 0;
    }
    free(outputs);
    return ok;
}

int c_compile_executable(const char *out, const char *name, int threads)
{
    Files files = {0};
    CBuildCache *cache = NULL;
    ToolLog *log = calloc(1, sizeof(*log));
    char **base = calloc(MAX_ARGS, sizeof(*base)), **objects = NULL;
    char **tail = calloc(MAX_ARGS, sizeof(*tail));
    char object_dir[ZIR_PATH_MAX], output[ZIR_PATH_MAX], temporary[ZIR_PATH_MAX] = {0};
    int count = 0, ok = 0;
    if(!base || !log || !tail) goto done;
    count = words(getenv("CC") && *getenv("CC") ? getenv("CC") : "cc", base, count);
    if(count >= 0) count = words(getenv("CFLAGS") ? getenv("CFLAGS") : "-O2", base, count);
    if(count < 0 || count + 10 >= MAX_ARGS) goto done;
    int tail_count = words(getenv("LDFLAGS"), tail, 0);
    if(tail_count >= 0) tail_count = words(getenv("LDLIBS"), tail, tail_count);
    if(tail_count < 0) goto done;
    base[count++] = strdup("-I"); base[count++] = strdup(out);
    if(threads) base[count++] = strdup("-pthread");
    const char *include = ToolchainIncludeDirectory();
    if(include) { base[count++] = strdup("-I"); base[count++] = strdup(include); }
    for(int i = 0; i < count; i++) if(!base[i]) goto done;
    for(int i = 0; GeneratedOutputPath(i); i++) if(!add_file(&files, GeneratedOutputPath(i))) goto done;
    /* Keep manually supplied root-level C hosts, as the driver always did.
     * Nested generated sources come only from this invocation's manifest. */
    DIR *dir = opendir(out);
    if(!dir) goto done;
    struct dirent *entry;
    int listed = 1;
    while((entry = readdir(dir))) {
        char path[ZIR_PATH_MAX];
        if(snprintf(path, sizeof(path), "%s/%s", out, entry->d_name) >= (int)sizeof(path) || !add_file(&files, path)) {
            listed = 0; break;
        }
    }
    closedir(dir);
    if(!listed || !files.count || count + files.count + tail_count + 24 >= MAX_ARGS) goto done;
    qsort(files.items, (size_t)files.count, sizeof(*files.items), compare_paths);
    if(snprintf(object_dir, sizeof(object_dir), "%s/.objects", out) >= (int)sizeof(object_dir) || !directory(object_dir)) goto done;
    objects = calloc((size_t)files.count, sizeof(*objects));
    if(!objects) goto done;
    for(int i = 0; i < files.count; i++) {
        if(strncmp(files.items[i], out, strlen(out)) ||
           strlen(files.items[i]) <= strlen(out) || files.items[i][strlen(out)] != '/') goto done;
        const char *relative = files.items[i] + strlen(out);
        while(*relative == '/') relative++;
        char path[ZIR_PATH_MAX];
        if(snprintf(path, sizeof(path), "%s/%s.o", object_dir, relative) >= (int)sizeof(path)) goto done;
        char *slash = strrchr(path, '/'); *slash = 0;
        if(!directory(path)) goto done;
        *slash = '/'; objects[i] = strdup(path);
        if(!objects[i]) goto done;
    }
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int workers = cpus <= 0 ? 1 : cpus < MAX_JOBS ? (int)cpus : MAX_JOBS;
    cache = c_cache_open(base, count, out, name);
    for(int first = 0; first < files.count;) {
        Job jobs[MAX_JOBS];
        int indices[MAX_JOBS], tracked[MAX_JOBS], amount = 0;
        while(first < files.count && amount < workers) {
            int i = first++;
            if(c_cache_hit(cache, objects[i])) continue;
            indices[amount] = i;
            char *args[MAX_ARGS];
            char dependencies[ZIR_PATH_MAX], stamp[ZIR_PATH_MAX];
            memcpy(args, base, (size_t)count * sizeof(*args));
            int n = count;
            int collect = cache && snprintf(dependencies, sizeof(dependencies), "%s.d", objects[i]) < (int)sizeof(dependencies);
            if(collect && unlink(dependencies) != 0 && errno != ENOENT) collect = 0;
            if(collect) {
                args[n++] = "-MD"; args[n++] = "-MF"; args[n++] = dependencies;
                args[n++] = "-MT"; args[n++] = "ziran-cache";
            }
            if(snprintf(stamp, sizeof(stamp), "%s.inputs", objects[i]) < (int)sizeof(stamp)) (void)unlink(stamp);
            tracked[amount] = collect;
            args[n++] = "-c"; args[n++] = files.items[i]; args[n++] = "-o"; args[n++] = objects[i]; args[n] = NULL;
            if(unlink(objects[i]) != 0 && errno != ENOENT) jobs[amount] = (Job){-1, -1};
            else jobs[amount] = start(args, DiagnosticJsonEnabled());
            amount++;
        }
        if(!finish(jobs, amount, log)) goto done;
        for(int j = 0; j < amount; j++) {
            struct stat info;
            int i = indices[j];
            if(stat(objects[i], &info) != 0 || !S_ISREG(info.st_mode) || !info.st_size) goto done;
            if(tracked[j]) c_cache_record(cache, objects[i], files.items[i]);
        }
    }
    if(snprintf(output, sizeof(output), "%s/%s", out, name) >= (int)sizeof(output) ||
       snprintf(temporary, sizeof(temporary), "%s/.%s.XXXXXX", out, name) >= (int)sizeof(temporary)) goto done;
    int temp = mkstemp(temporary);
    if(temp < 0) goto done;
    close(temp);
    char *args[MAX_ARGS];
    memcpy(args, base, (size_t)count * sizeof(*args));
    int n = count;
    /* Reset a language forced through CC/CFLAGS before passing object files. */
    args[n++] = "-x"; args[n++] = "none";
    for(int i = 0; i < files.count; i++) args[n++] = objects[i];
    args[n++] = "-o"; args[n++] = temporary;
    for(int i = 0; i < tail_count; i++) args[n++] = tail[i];
    args[n++] = "-lm"; args[n] = NULL;
    Job link = start(args, DiagnosticJsonEnabled());
    int linked = finish(&link, 1, log);
    struct stat executable;
    if(linked && stat(temporary, &executable) == 0 && S_ISREG(executable.st_mode) &&
       executable.st_size && (executable.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) &&
       rename(temporary, output) == 0) { temporary[0] = 0; ok = 1; }
done:
    c_cache_close(cache);
    if(temporary[0]) unlink(temporary);
    if(ok && log && log->used) Warning((ZirSourceSpan){0}, "zir_c.toolchain", "%s%s", log->text,
                                     log->truncated ? " [output truncated]" : "");
    if(!ok) DiagnosticTarget((ZirSourceSpan){0}, "zir_c.toolchain", "c", "toolchain.native",
                            "compiling %s failed%s%s%s", out, log && log->used ? ": " : "",
                            log ? log->text : "", log && log->truncated ? " [output truncated]" : "");
    if(base) for(int i = 0; i < MAX_ARGS; i++) free(base[i]);
    if(tail) for(int i = 0; i < MAX_ARGS; i++) free(tail[i]);
    free(base); free(tail); free(log);
    for(int i = 0; i < files.count; i++) { free(files.items[i]); if(objects) free(objects[i]); }
    free(files.items); free(objects);
    return ok;
}
