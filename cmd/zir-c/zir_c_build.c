#include "zir_c_build.h"
#include "zir_c_cache.h"
#include "zir_c_process.h"
#include "zir.h"
#include "zir_diagnostic.h"
#include "zir_load.h"
#include "zir_profile.h"

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
#ifdef __linux__
#include <sched.h>
#include <sys/syscall.h>
#endif

enum { MAX_ARGS = 1024, MAX_JOBS = 16, WORKER_MEMORY_MIB = 512 };
typedef struct Job { pid_t pid; int fd, completion, reaped, ok; } Job;
typedef struct ToolLog { char text[ZIR_TEXT_MAX]; size_t used; int truncated; } ToolLog;
typedef struct Files { char **items; int count; } Files;

/* MemAvailable in MiB, or -1 where the kernel does not report it. */
static long available_memory_mib(void)
{
#ifdef __linux__
    char text[4096];
    int fd = open("/proc/meminfo", O_RDONLY | O_CLOEXEC);
    if(fd < 0) return -1;
    ssize_t length;
    do { length = read(fd, text, sizeof(text) - 1); } while(length < 0 && errno == EINTR);
    close(fd);
    if(length <= 0) return -1;
    text[length] = 0;
    const char *line = strstr(text, "MemAvailable:");
    if(!line) return -1;
    long kib = strtol(line + 13, NULL, 10);
    return kib > 0 ? kib / 1024 : -1;
#else
    return -1;
#endif
}

/* Compiler workers never take the whole machine: at most half of the CPUs
 * this process may run on, and only as many as available memory holds at
 * WORKER_MEMORY_MIB each. ZIRAN_JOBS can lower the count further. */
int c_worker_limit(void)
{
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
#ifdef __linux__
    cpu_set_t affinity;
    CPU_ZERO(&affinity);
    if(sched_getaffinity(0, sizeof(affinity), &affinity) == 0) {
        int available = CPU_COUNT(&affinity);
        if(available > 0 && (cpus <= 0 || available < cpus)) cpus = available;
    }
#endif
    long workers = cpus > 1 ? cpus / 2 : 1;
    const char *requested = getenv("ZIRAN_JOBS");
    if(requested && *requested) {
        char *end = NULL;
        long value = strtol(requested, &end, 10);
        if(*end == 0 && value > 0 && value < workers) workers = value;
    }
    long memory = available_memory_mib();
    if(memory >= 0 && memory / WORKER_MEMORY_MIB < workers)
        workers = memory / WORKER_MEMORY_MIB > 0 ? memory / WORKER_MEMORY_MIB : 1;
    return workers < MAX_JOBS ? (int)workers : MAX_JOBS;
}

static void append_log(ToolLog *log, const char *bytes, size_t length)
{
    size_t copied = length;
    if(copied > sizeof(log->text) - 1 - log->used) {
        copied = sizeof(log->text) - 1 - log->used;
        log->truncated = 1;
    }
    for(size_t i = 0; i < copied; i++) log->text[log->used++] = bytes[i] ? bytes[i] : '?';
    log->text[log->used] = 0;
}

static void merge_log(ToolLog *target, const ToolLog *source)
{
    if(target->used && source->used && target->text[target->used - 1] != '\n')
        append_log(target, "\n", 1);
    append_log(target, source->text, source->used);
    target->truncated |= source->truncated;
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
    if(capture && pipe(pipes) != 0) return (Job){.pid = -1, .fd = -1, .completion = -1, .reaped = 1};
    if(capture) {
        fcntl(pipes[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipes[1], F_SETFD, FD_CLOEXEC);
    }
    pid_t pid = c_start_process(args, pipes[0], pipes[1], 1);
    if(capture) {
        close(pipes[1]);
        if(pid < 0) close(pipes[0]);
    }
    int completion = -1;
#if defined(__linux__) && defined(SYS_pidfd_open)
    /* A close-on-exec completion descriptor avoids periodic wakeups. Only
     * our own unreaped child is opened; unsupported kernels use waitpid. */
    if(pid > 0) completion = (int)syscall(SYS_pidfd_open, pid, 0);
#endif
    if(pid > 0) ProfileCount(completion >= 0 ? "native_process_event" : "native_process_poll");
    return (Job){.pid = pid, .fd = pid > 0 ? pipes[0] : -1,
                 .completion = completion, .reaped = pid < 0};
}

static int read_output(Job *job, ToolLog *log)
{
    char bytes[4096];
    ssize_t length;
    do { length = read(job->fd, bytes, sizeof(bytes)); } while(length < 0 && errno == EINTR);
    if(length <= 0) { close(job->fd); job->fd = -1; return length == 0; }
    append_log(log, bytes, (size_t)length);
    return 1;
}

static void reap(Job *job, int flags)
{
    if(job->reaped) return;
    int status = 0;
    pid_t waited;
    /* Reap only this build's children, including on failure. */
    do { waited = waitpid(job->pid, &status, flags); } while(waited < 0 && errno == EINTR);
    if(waited != 0) {
        job->reaped = 1;
        job->ok = waited == job->pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
        if(job->completion >= 0) { close(job->completion); job->completion = -1; }
    }
}

static int finish(Job *job, ToolLog *log)
{
    int ok = 1;
    while(job->fd >= 0) {
        struct pollfd poller = {job->fd, POLLIN, 0};
        int ready;
        do { ready = poll(&poller, 1, -1); } while(ready < 0 && errno == EINTR);
        if(ready < 0) { close(job->fd); job->fd = -1; ok = 0; break; }
        if(!read_output(job, log)) ok = 0;
    }
    reap(job, 0);
    return ok && job->ok;
}

typedef struct CompileSlot {
    Job job;
    int index, tracked;
} CompileSlot;

static int reuse_object(CBuildCache *cache, const char *object)
{
    if(!cache) return 0;
    uint64_t started = ProfileStart();
    int hit = c_cache_hit(cache, object);
    ProfileEnd("native.cache.lookup", started);
    return hit;
}

static int compile_objects(const Files *files, char **objects, char **base,
                            int count, CBuildCache *cache, int workers, ToolLog *log)
{
    CompileSlot *slots = calloc((size_t)workers, sizeof(*slots));
    int capture = DiagnosticJsonEnabled();
    /* Empty compiler output needs no per-file buffer. Nonempty logs retain
     * their input order even when workers complete in a different order. */
    ToolLog **outputs = capture ? calloc((size_t)files->count, sizeof(*outputs)) : NULL;
    ToolLog *scratch = capture ? calloc(1, sizeof(*scratch)) : NULL;
    if(!slots || (capture && (!outputs || !scratch))) { free(slots); free(outputs); free(scratch); return 0; }
    for(int s = 0; s < workers; s++) { slots[s].index = -1; slots[s].job.fd = -1; }
    int requested_pipe = c_cache_requested_pipe(base, count);
    int add_pipe = c_cache_piped(cache) && !requested_pipe;
    int next = 0, active = 0, ok = 1;
    while(active || (ok && next < files->count)) {
        for(int s = 0; ok && s < workers && next < files->count; s++) {
            CompileSlot *slot = &slots[s];
            if(slot->index >= 0) continue;
            int i = next++;
            while(reuse_object(cache, objects[i])) {
                ProfileCount("native_object_reuse");
                if(next == files->count) { i = -1; break; }
                i = next++;
            }
            if(i < 0) break;
            ProfileCount("native_object_compile");
            char *args[MAX_ARGS], dependencies[ZIR_PATH_MAX], stamp[ZIR_PATH_MAX];
            memcpy(args, base, (size_t)count * sizeof(*args));
            int n = count;
            if(add_pipe) args[n++] = "-pipe";
            if(add_pipe || requested_pipe) ProfileCount("native_object_pipe");
            int collect = cache && snprintf(dependencies, sizeof(dependencies), "%s.d", objects[i]) < (int)sizeof(dependencies);
            if(collect && unlink(dependencies) != 0 && errno != ENOENT) collect = 0;
            if(collect) {
                args[n++] = "-MD"; args[n++] = "-MF"; args[n++] = dependencies;
                args[n++] = "-MT"; args[n++] = "ziran-cache";
            }
            if(snprintf(stamp, sizeof(stamp), "%s.inputs", objects[i]) < (int)sizeof(stamp)) (void)unlink(stamp);
            args[n++] = "-c"; args[n++] = files->items[i]; args[n++] = "-o"; args[n++] = objects[i]; args[n] = NULL;
            slot->index = i; slot->tracked = collect;
            if(unlink(objects[i]) != 0 && errno != ENOENT)
                slot->job = (Job){.pid = -1, .fd = -1, .completion = -1, .reaped = 1};
            else slot->job = start(args, capture);
            active++;
            if(slot->job.pid < 0) ok = 0;
        }
        if(!active) break;
        int draining = !ok || next == files->count, output_pending = 0;
        for(int s = 0; s < workers; s++)
            output_pending |= slots[s].index >= 0 && slots[s].job.fd >= 0;
        /* With no more work to launch and no captured output to drain,
         * blocking on these owned children avoids a final polling delay. */
        if(draining && !output_pending)
            for(int s = 0; s < workers; s++) if(slots[s].index >= 0) reap(&slots[s].job, 0);
        struct pollfd pollers[MAX_JOBS * 2];
        int timeout = -1, all_reaped = 1;
        for(int s = 0; s < workers; s++) {
            Job *job = &slots[s].job;
            int active_slot = slots[s].index >= 0;
            pollers[s * 2] = (struct pollfd){active_slot ? job->fd : -1, POLLIN, 0};
            pollers[s * 2 + 1] = (struct pollfd){active_slot ? job->completion : -1, POLLIN, 0};
            if(active_slot && !job->reaped) {
                all_reaped = 0;
                if(job->completion < 0) timeout = 1;
            }
        }
        if(all_reaped && !output_pending) timeout = 0;
        /* Platforms without completion descriptors inspect only our own
         * PIDs after a short bounded wait, never waitpid(-1). */
        int ready;
        do { ready = poll(pollers, (nfds_t)(workers * 2), timeout); } while(ready < 0 && errno == EINTR);
        if(ready < 0) {
            ok = 0;
            for(int s = 0; s < workers; s++) if(slots[s].index >= 0) {
                if(slots[s].job.fd >= 0) { close(slots[s].job.fd); slots[s].job.fd = -1; }
                reap(&slots[s].job, 0);
            }
        }
        for(int s = 0; s < workers; s++) {
            CompileSlot *slot = &slots[s];
            if(slot->index < 0) continue;
            int i = slot->index;
            if(slot->job.fd >= 0 && pollers[s * 2].revents) {
                ToolLog *output = outputs[i];
                if(!output) { scratch->used = 0; scratch->truncated = 0; scratch->text[0] = 0; output = scratch; }
                if(!read_output(&slot->job, output)) ok = 0;
                if(!outputs[i] && (scratch->used || scratch->truncated)) {
                    outputs[i] = malloc(sizeof(*outputs[i]));
                    if(outputs[i]) *outputs[i] = *scratch;
                    else ok = 0;
                }
            }
            reap(&slot->job, WNOHANG);
            if(slot->job.reaped && !slot->job.ok) ok = 0;
            if(!slot->job.reaped || slot->job.fd >= 0) continue;
            struct stat info;
            if(slot->job.ok) {
                if(stat(objects[i], &info) != 0 || !S_ISREG(info.st_mode) || !info.st_size) ok = 0;
                else if(slot->tracked) {
                    uint64_t started = ProfileStart();
                    c_cache_record(cache, objects[i], files->items[i]);
                    ProfileEnd("native.cache.record", started);
                }
            }
            slot->index = -1; active--;
        }
    }
    if(outputs) for(int i = 0; i < files->count; i++) {
        if(outputs[i]) { merge_log(log, outputs[i]); free(outputs[i]); }
    }
    free(outputs); free(slots); free(scratch);
    return ok;
}

int c_compile_executable(const char *out, const char *name, int threads, int release)
{
    Files files = {0};
    CBuildCache *cache = NULL;
    ToolLog *log = calloc(1, sizeof(*log));
    char **base = calloc(MAX_ARGS, sizeof(*base)), **objects = NULL;
    char **tail = calloc(MAX_ARGS, sizeof(*tail));
    char object_dir[ZIR_PATH_MAX], output[ZIR_PATH_MAX], temporary[ZIR_PATH_MAX] = {0};
    uint64_t link_started = 0;
    int count = 0, ok = 0;
    if(!base || !log || !tail) goto done;
    count = words(getenv("CC") && *getenv("CC") ? getenv("CC") : "cc", base, count);
    /* Link-time optimization repeats code generation for every module at each
     * link, which makes one-file edits many times slower, so only a release
     * asks for it. CFLAGS replaces both defaults, as in make. */
    if(count >= 0) count = words(getenv("CFLAGS") ? getenv("CFLAGS") : release ? "-O3 -flto=auto" : "-O3", base, count);
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
    int workers = c_worker_limit();
    if(workers > files.count) workers = files.count;
    for(int i = 0; i < workers; i++) ProfileCount("native_worker_slots");
    uint64_t stage_started = ProfileStart();
    cache = c_cache_open(base, count, out, name);
    ProfileEnd("native.toolchain", stage_started);
    stage_started = ProfileStart();
    int compiled = compile_objects(&files, objects, base, count, cache, workers, log);
    ProfileEnd("native.objects", stage_started);
    if(!compiled) goto done;
    link_started = ProfileStart();
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
    int capture = DiagnosticJsonEnabled();
    Job link = start(args, capture);
    ToolLog *link_log = capture ? calloc(1, sizeof(*link_log)) : NULL;
    /* Even an allocation failure must drain and reap the child we started. */
    int linked = finish(&link, link_log ? link_log : log);
    if(link_log) { merge_log(log, link_log); free(link_log); }
    else if(capture) linked = 0;
    struct stat executable;
    if(linked && stat(temporary, &executable) == 0 && S_ISREG(executable.st_mode) &&
       executable.st_size && (executable.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) &&
       rename(temporary, output) == 0) { temporary[0] = 0; ok = 1; }
done:
    ProfileEnd("native.link", link_started);
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
