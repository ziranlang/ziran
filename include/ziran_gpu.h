#ifndef ZIRAN_GPU_H
#define ZIRAN_GPU_H

/* Device execution for `#parallel_gpu for` regions on the native C target, as
 * OpenGL ES 3.1 compute shaders. EGL and GLES load at run time, so programs
 * need no graphics libraries to build or start. ziran_gpu_run returns 0 when
 * it did not complete a region, with the region's storage untouched: no
 * usable device, a shader the driver refuses, a failed bounds check or a used
 * up loop budget. The caller then runs the region's CPU worker, which also
 * reports any failure as usual.
 *
 * The fastest hardware device is measured once per process. The display's GPU
 * may be among them, so work runs in short chunks, and shaders stop their
 * loops on a budget. A caller's current GL context is restored afterward.
 *
 * ZIRAN_GPU=off stops offload and ZIRAN_GPU=N chooses EGL device N;
 * ZIRAN_GPU_MIN sets the fewest iterations worth offloading (4096);
 * ZIRAN_GPU_LOG=1 reports the device and each offloaded region on stderr. */

#include <stdint.h>

enum {
    ZIRAN_GPU_S32,
    ZIRAN_GPU_U32,
    ZIRAN_GPU_F32,
    ZIRAN_GPU_U8,
    ZIRAN_GPU_BOOL
};

typedef struct ziran_gpu_binding {
    int kind;
    int is_buffer; /* otherwise one scalar, passed as a uniform */
    int writes;
    void *data;
    int64_t bytes;
} ziran_gpu_binding;

typedef struct ziran_gpu_region {
    const char *const *lines;
    int line_count;
    unsigned program; /* 0 before compiling */
    int refused;
} ziran_gpu_region;

#if defined(__linux__) && !defined(__EMSCRIPTEN__)

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct ziran_gpu_api {
    void *(*GetDisplay)(void *);
    void *(*GetPlatformDisplay)(unsigned, void *, const intptr_t *);
    unsigned (*Initialize)(void *, int *, int *);
    unsigned (*BindAPI)(unsigned);
    void *(*CreateContext)(void *, void *, void *, const int *);
    unsigned (*DestroyContext)(void *, void *);
    unsigned (*MakeCurrent)(void *, void *, void *, void *);
    void *(*GetCurrentDisplay)(void);
    void *(*GetCurrentSurface)(int);
    void *(*GetCurrentContext)(void);
    void *(*GetProcAddress)(const char *);
    unsigned (*Terminate)(void *);
    unsigned (*QueryDevices)(int, void **, int *);
    unsigned (*CreateShader)(unsigned);
    void (*ShaderSource)(unsigned, int, const char *const *, const int *);
    void (*CompileShader)(unsigned);
    void (*GetShaderiv)(unsigned, unsigned, int *);
    void (*DeleteShader)(unsigned);
    unsigned (*CreateProgram)(void);
    void (*AttachShader)(unsigned, unsigned);
    void (*LinkProgram)(unsigned);
    void (*GetProgramiv)(unsigned, unsigned, int *);
    void (*DeleteProgram)(unsigned);
    void (*UseProgram)(unsigned);
    void (*GenBuffers)(int, unsigned *);
    void (*DeleteBuffers)(int, const unsigned *);
    void (*BindBuffer)(unsigned, unsigned);
    void (*BufferData)(unsigned, intptr_t, const void *, unsigned);
    void (*BindBufferBase)(unsigned, unsigned, unsigned);
    void (*Uniform1i)(int, int);
    void (*Uniform1ui)(int, unsigned);
    void (*Uniform1f)(int, float);
    void (*DispatchCompute)(unsigned, unsigned, unsigned);
    void (*MemoryBarrier)(unsigned);
    void (*Finish)(void);
    void *(*MapBufferRange)(unsigned, intptr_t, intptr_t, unsigned);
    unsigned char (*UnmapBuffer)(unsigned);
    const unsigned char *(*GetString)(unsigned);
    unsigned (*GetError)(void);
} ziran_gpu_api;

typedef struct ziran_gpu_state {
    pthread_mutex_t lock;
    int configured;
    int opened;
    int usable;
    int log;
    int64_t minimum;
    ziran_gpu_api api;
    void *display;
    void *context;
} ziran_gpu_state;

/* One state for the process, however many generated files include this. */
__attribute__((weak)) ziran_gpu_state ziran_gpu_shared = {PTHREAD_MUTEX_INITIALIZER, 0, 0, 0, 0, 0, {0}, 0, 0};

#define ZIRAN_GPU_SSBO 0x90D2u
#define ZIRAN_GPU_CHUNK_GROUP 64
#define ZIRAN_GPU_MAX_GROUPS 65535

static double ziran_gpu_seconds(void)
{
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__ANDROID__)
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
#elif defined(CLOCK_MONOTONIC)
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
#else
    /* Strict C99 hides the wall clocks; this only sizes chunks. */
    return (double)clock() / CLOCKS_PER_SEC;
#endif
}

/* Function pointers from dlsym, in the one form strict C99 accepts. */
static void ziran_gpu_symbol(void *library, const char *name, void *slot)
{
    void *found = dlsym(library, name);
    memcpy(slot, &found, sizeof(found));
}

static void *ziran_gpu_open_any(const char *const *names)
{
    for(; *names != NULL; names++) {
        void *library = dlopen(*names, RTLD_NOW | RTLD_LOCAL);
        if(library != NULL)
            return library;
    }
    return NULL;
}

static int ziran_gpu_load(ziran_gpu_api *api)
{
    static const char *const egl_names[] = {"libEGL.so.1", "libEGL.so", NULL};
    static const char *const gles_names[] = {"libGLESv2.so.2", "libGLESv3.so", "libGLESv2.so", NULL};
    void *egl = ziran_gpu_open_any(egl_names);
    void *gles = egl != NULL ? ziran_gpu_open_any(gles_names) : NULL;
    if(gles == NULL)
        return 0;
    ziran_gpu_symbol(egl, "eglGetDisplay", &api->GetDisplay);
    ziran_gpu_symbol(egl, "eglGetPlatformDisplay", &api->GetPlatformDisplay);
    ziran_gpu_symbol(egl, "eglInitialize", &api->Initialize);
    ziran_gpu_symbol(egl, "eglBindAPI", &api->BindAPI);
    ziran_gpu_symbol(egl, "eglCreateContext", &api->CreateContext);
    ziran_gpu_symbol(egl, "eglDestroyContext", &api->DestroyContext);
    ziran_gpu_symbol(egl, "eglMakeCurrent", &api->MakeCurrent);
    ziran_gpu_symbol(egl, "eglGetCurrentDisplay", &api->GetCurrentDisplay);
    ziran_gpu_symbol(egl, "eglGetCurrentSurface", &api->GetCurrentSurface);
    ziran_gpu_symbol(egl, "eglGetCurrentContext", &api->GetCurrentContext);
    ziran_gpu_symbol(egl, "eglGetProcAddress", &api->GetProcAddress);
    ziran_gpu_symbol(egl, "eglTerminate", &api->Terminate);
    if(api->GetProcAddress != NULL) {
        void *query = api->GetProcAddress("eglQueryDevicesEXT");
        memcpy(&api->QueryDevices, &query, sizeof(query));
    }
    ziran_gpu_symbol(gles, "glCreateShader", &api->CreateShader);
    ziran_gpu_symbol(gles, "glShaderSource", &api->ShaderSource);
    ziran_gpu_symbol(gles, "glCompileShader", &api->CompileShader);
    ziran_gpu_symbol(gles, "glGetShaderiv", &api->GetShaderiv);
    ziran_gpu_symbol(gles, "glDeleteShader", &api->DeleteShader);
    ziran_gpu_symbol(gles, "glCreateProgram", &api->CreateProgram);
    ziran_gpu_symbol(gles, "glAttachShader", &api->AttachShader);
    ziran_gpu_symbol(gles, "glLinkProgram", &api->LinkProgram);
    ziran_gpu_symbol(gles, "glGetProgramiv", &api->GetProgramiv);
    ziran_gpu_symbol(gles, "glDeleteProgram", &api->DeleteProgram);
    ziran_gpu_symbol(gles, "glUseProgram", &api->UseProgram);
    ziran_gpu_symbol(gles, "glGenBuffers", &api->GenBuffers);
    ziran_gpu_symbol(gles, "glDeleteBuffers", &api->DeleteBuffers);
    ziran_gpu_symbol(gles, "glBindBuffer", &api->BindBuffer);
    ziran_gpu_symbol(gles, "glBufferData", &api->BufferData);
    ziran_gpu_symbol(gles, "glBindBufferBase", &api->BindBufferBase);
    ziran_gpu_symbol(gles, "glUniform1i", &api->Uniform1i);
    ziran_gpu_symbol(gles, "glUniform1ui", &api->Uniform1ui);
    ziran_gpu_symbol(gles, "glUniform1f", &api->Uniform1f);
    ziran_gpu_symbol(gles, "glDispatchCompute", &api->DispatchCompute);
    ziran_gpu_symbol(gles, "glMemoryBarrier", &api->MemoryBarrier);
    ziran_gpu_symbol(gles, "glFinish", &api->Finish);
    ziran_gpu_symbol(gles, "glMapBufferRange", &api->MapBufferRange);
    ziran_gpu_symbol(gles, "glUnmapBuffer", &api->UnmapBuffer);
    ziran_gpu_symbol(gles, "glGetString", &api->GetString);
    ziran_gpu_symbol(gles, "glGetError", &api->GetError);
    /* Every loaded entry point is required except the device extras. */
    return api->GetDisplay && api->Initialize && api->BindAPI && api->CreateContext &&
           api->DestroyContext && api->MakeCurrent && api->GetCurrentDisplay &&
           api->GetCurrentSurface && api->GetCurrentContext && api->Terminate &&
           api->CreateShader && api->ShaderSource && api->CompileShader && api->GetShaderiv &&
           api->DeleteShader && api->CreateProgram && api->AttachShader && api->LinkProgram &&
           api->GetProgramiv && api->DeleteProgram && api->UseProgram && api->GenBuffers &&
           api->DeleteBuffers && api->BindBuffer && api->BufferData && api->BindBufferBase &&
           api->Uniform1i && api->Uniform1ui && api->Uniform1f && api->DispatchCompute &&
           api->MemoryBarrier && api->Finish && api->MapBufferRange && api->UnmapBuffer &&
           api->GetString && api->GetError;
}

/* The caller's current GL binding, restored after device work. */
typedef struct ziran_gpu_saved {
    void *display, *draw, *read, *context;
} ziran_gpu_saved;

static void ziran_gpu_save(const ziran_gpu_api *api, ziran_gpu_saved *saved)
{
    saved->display = api->GetCurrentDisplay();
    saved->draw = api->GetCurrentSurface(0x3059);
    saved->read = api->GetCurrentSurface(0x305A);
    saved->context = api->GetCurrentContext();
}

static void ziran_gpu_restore(const ziran_gpu_api *api, const ziran_gpu_saved *saved, void *ours)
{
    if(saved->context != NULL)
        api->MakeCurrent(saved->display, saved->draw, saved->read, saved->context);
    else
        api->MakeCurrent(ours, NULL, NULL, NULL);
}

static void *ziran_gpu_context(const ziran_gpu_api *api, void *display)
{
    static const int attributes[] = {0x3098, 3, 0x30FB, 1, 0x3038};
    int major, minor;
    if(display == NULL || !api->Initialize(display, &major, &minor) || !api->BindAPI(0x30A0))
        return NULL;
    return api->CreateContext(display, NULL, NULL, attributes);
}

static unsigned ziran_gpu_compile(const ziran_gpu_api *api, const char *const *lines, int count)
{
    int status = 0;
    unsigned shader = api->CreateShader(0x91B9u);
    if(shader == 0)
        return 0;
    api->ShaderSource(shader, count, lines, NULL);
    api->CompileShader(shader);
    api->GetShaderiv(shader, 0x8B81u, &status);
    if(!status) {
        api->DeleteShader(shader);
        return 0;
    }
    unsigned program = api->CreateProgram();
    api->AttachShader(program, shader);
    api->LinkProgram(program);
    api->DeleteShader(shader);
    api->GetProgramiv(program, 0x8B82u, &status);
    if(!status) {
        api->DeleteProgram(program);
        return 0;
    }
    return program;
}

/* A short arithmetic workload's seconds on the current context, or a negative
 * number when the device cannot run it. The first run also pays for start-up,
 * so the best of the rest counts. */
static double ziran_gpu_measure(const ziran_gpu_api *api)
{
    static const char *const lines[] = {
        "#version 310 es\n",
        "precision highp float;\nprecision highp int;\n",
        "layout(local_size_x=64) in;\n",
        "layout(std430,binding=0) buffer B{float v[];};\n",
        "void main(){float a=float(gl_GlobalInvocationID.x)*0.001;",
        "for(int i=0;i<2000;i++)a=a*a*0.5+0.25;v[gl_GlobalInvocationID.x&255u]=a;}\n",
    };
    unsigned program = ziran_gpu_compile(api, lines, 6), buffer = 0;
    double best = -1.0;
    if(program == 0)
        return -1.0;
    api->UseProgram(program);
    api->GenBuffers(1, &buffer);
    api->BindBuffer(ZIRAN_GPU_SSBO, buffer);
    api->BufferData(ZIRAN_GPU_SSBO, 1024, NULL, 0x88E9u);
    api->BindBufferBase(ZIRAN_GPU_SSBO, 0, buffer);
    for(int round = 0; round < 4; round++) {
        double started = ziran_gpu_seconds();
        api->DispatchCompute(256, 1, 1);
        api->Finish();
        double elapsed = ziran_gpu_seconds() - started;
        if(round > 0 && (best < 0 || elapsed < best))
            best = elapsed;
    }
    if(api->GetError() != 0)
        best = -1.0;
    api->DeleteBuffers(1, &buffer);
    api->DeleteProgram(program);
    return best;
}

static int ziran_gpu_software(const char *renderer)
{
    return strstr(renderer, "llvmpipe") != NULL || strstr(renderer, "softpipe") != NULL ||
           strstr(renderer, "SwiftShader") != NULL || strstr(renderer, "Software") != NULL;
}

/* Try one display; keep it when it is the fastest so far. */
static void ziran_gpu_consider(ziran_gpu_state *state, void *display, int device, int chosen,
                               double *best)
{
    ziran_gpu_api *api = &state->api;
    void *context = ziran_gpu_context(api, display);
    if(context == NULL)
        return;
    int keep = 0;
    if(api->MakeCurrent(display, NULL, NULL, context)) {
        const unsigned char *renderer = api->GetString(0x1F01u);
        const char *name = renderer != NULL ? (const char *)renderer : "";
        double seconds = -1.0;
        /* A chosen device runs whatever it is; only measuring skips software. */
        if(chosen >= 0 || !ziran_gpu_software(name))
            seconds = ziran_gpu_measure(api);
        keep = seconds >= 0 && (*best < 0 || seconds < *best);
        if(state->log && seconds >= 0)
            fprintf(stderr, "ziran: gpu: device %d %s: %.1f us\n", device, name, seconds * 1e6);
        if(keep) {
            if(state->context != NULL)
                api->DestroyContext(state->display, state->context);
            state->display = display;
            state->context = context;
            *best = seconds;
        }
    }
    api->MakeCurrent(display, NULL, NULL, NULL);
    if(!keep)
        api->DestroyContext(display, context);
}

static void ziran_gpu_open(ziran_gpu_state *state)
{
    ziran_gpu_api *api = &state->api;
    const char *setting = getenv("ZIRAN_GPU");
    int chosen = -1;
    double best = -1.0;
    state->opened = 1;
    if(setting != NULL && !strcmp(setting, "off"))
        return;
    if(setting != NULL && setting[0] != '\0')
        chosen = atoi(setting);
    if(!ziran_gpu_load(api))
        return;
    ziran_gpu_saved saved;
    ziran_gpu_save(api, &saved);
    void *devices[16];
    int count = 0;
    if(api->QueryDevices != NULL && api->GetPlatformDisplay != NULL &&
       !api->QueryDevices(16, devices, &count))
        count = 0;
    for(int i = 0; i < count; i++)
        if(chosen < 0 || chosen == i)
            ziran_gpu_consider(state, api->GetPlatformDisplay(0x313Fu, devices[i], NULL), i, chosen, &best);
    /* Android and other hosts without the device extension have one GPU. */
    if(count == 0 && chosen <= 0)
        ziran_gpu_consider(state, api->GetDisplay(NULL), 0, chosen, &best);
    state->usable = state->context != NULL;
    if(state->usable) {
        api->MakeCurrent(state->display, NULL, NULL, state->context);
        if(state->log) {
            const unsigned char *renderer = api->GetString(0x1F01u);
            fprintf(stderr, "ziran: gpu: using %s\n", renderer != NULL ? (const char *)renderer : "a device");
        }
    }
    /* Measuring left no context current; put back the caller's. */
    if(saved.context != NULL)
        ziran_gpu_restore(api, &saved, state->display);
    else if(state->usable)
        api->MakeCurrent(state->display, NULL, NULL, NULL);
}

static int64_t ziran_gpu_element_size(int kind)
{
    return kind == ZIRAN_GPU_U8 || kind == ZIRAN_GPU_BOOL ? 1 : 4;
}

/* Upload BINDING to the current buffer. Bytes widen to the words shaders read. */
static int ziran_gpu_upload(const ziran_gpu_api *api, const ziran_gpu_binding *binding)
{
    /* Read back or only read: tells the driver where the memory is best. */
    unsigned usage = binding->writes ? 0x88E9u : 0x88E4u;
    if(binding->kind != ZIRAN_GPU_U8 && binding->kind != ZIRAN_GPU_BOOL) {
        api->BufferData(ZIRAN_GPU_SSBO, (intptr_t)binding->bytes, binding->data, usage);
        return 1;
    }
    uint32_t *words = (uint32_t *)malloc((size_t)binding->bytes * sizeof(*words));
    if(words == NULL)
        return 0;
    const unsigned char *bytes = (const unsigned char *)binding->data;
    for(int64_t i = 0; i < binding->bytes; i++)
        words[i] = bytes[i];
    api->BufferData(ZIRAN_GPU_SSBO, (intptr_t)(binding->bytes * 4), words, usage);
    free(words);
    return 1;
}

/* Copy a written buffer back to BINDING's storage. */
static int ziran_gpu_download(const ziran_gpu_api *api, const ziran_gpu_binding *binding)
{
    int widened = binding->kind == ZIRAN_GPU_U8 || binding->kind == ZIRAN_GPU_BOOL;
    int64_t size = widened ? binding->bytes * 4 : binding->bytes;
    void *mapped = api->MapBufferRange(ZIRAN_GPU_SSBO, 0, (intptr_t)size, 1u);
    if(mapped == NULL)
        return 0;
    if(widened) {
        const uint32_t *words = (const uint32_t *)mapped;
        unsigned char *bytes = (unsigned char *)binding->data;
        for(int64_t i = 0; i < binding->bytes; i++)
            bytes[i] = (unsigned char)words[i];
    } else {
        memcpy(binding->data, mapped, (size_t)size);
    }
    return api->UnmapBuffer(ZIRAN_GPU_SSBO) != 0;
}

static void ziran_gpu_scalar(const ziran_gpu_api *api, int location, const ziran_gpu_binding *binding)
{
    switch(binding->kind) {
    case ZIRAN_GPU_S32: api->Uniform1i(location, *(const int32_t *)binding->data); break;
    case ZIRAN_GPU_U32: api->Uniform1ui(location, *(const uint32_t *)binding->data); break;
    case ZIRAN_GPU_F32: api->Uniform1f(location, *(const float *)binding->data); break;
    case ZIRAN_GPU_U8: api->Uniform1ui(location, *(const unsigned char *)binding->data); break;
    default: api->Uniform1ui(location, *(const unsigned char *)binding->data != 0); break;
    }
}

/* Run iterations in chunks near a few milliseconds, so a display on this GPU
 * keeps refreshing. Returns 0 if a shader raised the fault word. */
static int ziran_gpu_iterate(const ziran_gpu_api *api, unsigned fault, int64_t first, int64_t last)
{
    int64_t at = first, groups = 1;
    while(at <= last) {
        int64_t remaining = (last - at) / ZIRAN_GPU_CHUNK_GROUP + 1;
        int64_t now = groups < remaining ? groups : remaining;
        if(now > ZIRAN_GPU_MAX_GROUPS)
            now = ZIRAN_GPU_MAX_GROUPS;
        double started = ziran_gpu_seconds();
        api->Uniform1i(0, (int)at);
        api->DispatchCompute((unsigned)now, 1, 1);
        api->MemoryBarrier(0x2000u);
        api->Finish();
        double elapsed = ziran_gpu_seconds() - started;
        api->BindBuffer(ZIRAN_GPU_SSBO, fault);
        const uint32_t *word = (const uint32_t *)api->MapBufferRange(ZIRAN_GPU_SSBO, 0, 4, 1u);
        int faulted = word == NULL || *word != 0;
        api->UnmapBuffer(ZIRAN_GPU_SSBO);
        if(faulted)
            return 0;
        at += now * ZIRAN_GPU_CHUNK_GROUP;
        if(elapsed < 0.002 && groups < ZIRAN_GPU_MAX_GROUPS)
            groups *= 2;
        else if(elapsed > 0.008 && groups > 1)
            groups /= 2;
    }
    return 1;
}

static int ziran_gpu_dispatch(ziran_gpu_state *state, ziran_gpu_region *region, int64_t first, int64_t last,
                              ziran_gpu_binding *bindings, int count)
{
    const ziran_gpu_api *api = &state->api;
    unsigned buffers[17] = {0};
    int ok = 0, buffer_count = 1, location = 2;
    if(region->program == 0) {
        region->program = ziran_gpu_compile(api, region->lines, region->line_count);
        if(region->program == 0) {
            region->refused = 1;
            if(state->log)
                fprintf(stderr, "ziran: gpu: the driver refused a shader\n");
            return 0;
        }
    }
    api->UseProgram(region->program);
    api->Uniform1i(1, (int)last);
    for(int i = 0; i < count; i++)
        buffer_count += bindings[i].is_buffer != 0;
    api->GenBuffers(buffer_count, buffers);
    /* Binding 0 is the fault word, starting clear. */
    {
        uint32_t clear = 0;
        api->BindBuffer(ZIRAN_GPU_SSBO, buffers[0]);
        api->BufferData(ZIRAN_GPU_SSBO, 4, &clear, 0x88E9u);
        api->BindBufferBase(ZIRAN_GPU_SSBO, 0, buffers[0]);
    }
    for(int i = 0, next = 1; i < count; i++) {
        if(!bindings[i].is_buffer) {
            ziran_gpu_scalar(api, location++, &bindings[i]);
            continue;
        }
        api->BindBuffer(ZIRAN_GPU_SSBO, buffers[next]);
        if(!ziran_gpu_upload(api, &bindings[i]))
            goto done;
        api->BindBufferBase(ZIRAN_GPU_SSBO, (unsigned)next, buffers[next]);
        next++;
    }
    if(api->GetError() != 0 || !ziran_gpu_iterate(api, buffers[0], first, last))
        goto done;
    for(int i = 0, next = 1; i < count; i++) {
        if(!bindings[i].is_buffer)
            continue;
        if(bindings[i].writes) {
            api->BindBuffer(ZIRAN_GPU_SSBO, buffers[next]);
            if(!ziran_gpu_download(api, &bindings[i]))
                goto done;
        }
        next++;
    }
    ok = api->GetError() == 0;
done:
    api->DeleteBuffers(buffer_count, buffers);
    return ok;
}

__attribute__((unused))
static int ziran_gpu_run(ziran_gpu_region *region, int64_t first, int64_t last,
                         ziran_gpu_binding *bindings, int count)
{
    ziran_gpu_state *state = &ziran_gpu_shared;
    int ok = 0;
    /* Iterations address the shader's 32-bit `it`. */
    if(first > last || first < 0 || last > INT32_MAX - ZIRAN_GPU_CHUNK_GROUP)
        return 0;
    for(int i = 0; i < count; i++)
        if(bindings[i].is_buffer &&
           (bindings[i].bytes <= 0 || bindings[i].bytes % ziran_gpu_element_size(bindings[i].kind) != 0))
            return 0;
    pthread_mutex_lock(&state->lock);
    if(!state->configured) {
        const char *minimum = getenv("ZIRAN_GPU_MIN");
        state->configured = 1;
        state->log = getenv("ZIRAN_GPU_LOG") != NULL;
        state->minimum = minimum != NULL && minimum[0] != '\0' ? strtoll(minimum, NULL, 10) : 4096;
    }
    /* Small regions never pay for starting a driver. */
    if(last - first + 1 < state->minimum) {
        pthread_mutex_unlock(&state->lock);
        return 0;
    }
    if(!state->opened)
        ziran_gpu_open(state);
    if(state->usable && !region->refused) {
        ziran_gpu_saved saved;
        ziran_gpu_save(&state->api, &saved);
        if(state->api.MakeCurrent(state->display, NULL, NULL, state->context)) {
            ok = ziran_gpu_dispatch(state, region, first, last, bindings, count);
            ziran_gpu_restore(&state->api, &saved, state->display);
        }
        if(state->log)
            fprintf(stderr, "ziran: gpu: region of %lld iterations %s\n", (long long)(last - first + 1),
                    ok ? "ran on the device" : "returned to the CPU");
    }
    pthread_mutex_unlock(&state->lock);
    return ok;
}

#else

__attribute__((unused))
static int ziran_gpu_run(ziran_gpu_region *region, int64_t first, int64_t last,
                         ziran_gpu_binding *bindings, int count)
{
    (void)region; (void)first; (void)last; (void)bindings; (void)count;
    return 0;
}

#endif

#endif
