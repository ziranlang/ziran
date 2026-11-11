#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(MATRIX_WEB)
#include <unistd.h>
#include <fcntl.h>
#endif

#if defined(MATRIX_NATIVE)
#include MATRIX_HEADER
static int64_t invoke(void) {
#ifdef MATRIX_VOID
    MATRIX_ENTRY(); return 0;
#else
    return (int64_t)MATRIX_ENTRY();
#endif
}
#elif defined(MATRIX_ZX)
#include "ziran_host.h"
static Bundle *bundle;
static BundleInstance *instance;
static int open_program(const char *path) {
    bundle = BundleOpen(path);
    if(!bundle) return 0;
    instance = BundleInstantiatePortable(bundle, NULL, 0);
    return instance != NULL;
}
static int64_t invoke(void) {
    long long result = 0;
    int present = 0;
    if(!BundleInstanceRun(instance, &result, &present)) abort();
    return result;
}
#elif defined(MATRIX_WAMR)
#include "wasm_c_api.h"
#include "zir_text.h"
#include "zir_zx_wasm.h"
static wasm_engine_t *engine;
static wasm_store_t *store;
static wasm_module_t *module;
static wasm_instance_t *instance;
static wasm_extern_vec_t imports, exports;
static wasm_byte_vec_t image;
static wasm_global_t *budget;
static wasm_func_t *entry;
static wasm_memory_t *memory;
static int named(const wasm_name_t *name, const char *text) {
    size_t size = strlen(text);
    return (name->num_elems == size || (name->num_elems == size + 1 && name->data[size] == 0)) &&
           !memcmp(name->data, text, size);
}
static wasm_trap_t *overrun(const wasm_val_vec_t *arguments, wasm_val_vec_t *results) {
    (void)arguments; (void)results;
    fputs("WebAssembly budget exceeded\n", stderr);
    abort();
}
static wasm_trap_t *host_output(const wasm_val_vec_t *arguments, wasm_val_vec_t *results) {
    uint32_t kind = (uint32_t)arguments->data[0].of.i32;
    int64_t value = arguments->data[1].of.i64;
    char text[64];
    switch(kind) {
    case ZX_WASM_PRINT_STRING: {
        size_t address = (uint32_t)value, size = (uint64_t)value >> 32;
        if(!memory || address + size > wasm_memory_data_size(memory)) abort();
        if(fwrite(wasm_memory_data(memory) + address, 1, size, stdout) != size) abort();
        results->data[0] = (wasm_val_t)WASM_I64_VAL(0);
        return NULL;
    }
    case ZX_WASM_PRINT_SIGNED: snprintf(text, sizeof(text), "%lld", (long long)value); break;
    case ZX_WASM_PRINT_UNSIGNED: snprintf(text, sizeof(text), "%llu", (unsigned long long)value); break;
    case ZX_WASM_PRINT_BOOL: snprintf(text, sizeof(text), "%s", value ? "true" : "false"); break;
    case ZX_WASM_PRINT_FLOAT32: { uint32_t bits = (uint32_t)value; float number; memcpy(&number, &bits, 4); FormatPrintFloat(number, 1, text, sizeof(text)); break; }
    case ZX_WASM_PRINT_FLOAT64: { double number; memcpy(&number, &value, 8); FormatPrintFloat(number, 0, text, sizeof(text)); break; }
    default: abort();
    }
    fputs(text, stdout);
    results->data[0] = (wasm_val_t)WASM_I64_VAL(0);
    return NULL;
}
static int open_program(const char *path) {
    engine = wasm_engine_new();
    if(!engine || !(store = wasm_store_new(engine))) return 0;
    FILE *file = fopen(path, "rb");
    if(!file || fseek(file, 0, SEEK_END)) return 0;
    long size = ftell(file);
    if(size <= 0 || size > 10000000) return 0;
    rewind(file);
    wasm_byte_vec_new_uninitialized(&image, (size_t)size);
    if(!image.data || fread(image.data, 1, (size_t)size, file) != (size_t)size) return 0;
    fclose(file);
    module = wasm_module_new(store, &image);
    if(!module) return 0;
    wasm_importtype_vec_t types = {0};
    wasm_module_imports(module, &types);
    if(types.num_elems < 2 || types.num_elems > 3) return 0;
    wasm_extern_t *bindings[3] = {0};
    for(size_t i = 0; i < types.num_elems; i++) {
        const wasm_importtype_t *type = types.data[i];
        if(!named(wasm_importtype_module(type), "zx")) return 0;
        const wasm_externtype_t *external = wasm_importtype_type(type);
        if(named(wasm_importtype_name(type), "budget")) {
            wasm_val_t initial = WASM_I64_VAL(10000000);
            budget = wasm_global_new(store, wasm_externtype_as_globaltype_const(external), &initial);
            if(!budget) return 0;
            bindings[i] = wasm_global_as_extern(budget);
        } else if(named(wasm_importtype_name(type), "overrun")) {
            wasm_func_t *function = wasm_func_new(store, wasm_externtype_as_functype_const(external), overrun);
            if(!function) return 0;
            bindings[i] = wasm_func_as_extern(function);
        } else if(named(wasm_importtype_name(type), "host_i64")) {
            wasm_func_t *function = wasm_func_new(store, wasm_externtype_as_functype_const(external), host_output);
            if(!function) return 0;
            bindings[i] = wasm_func_as_extern(function);
        } else return 0;
    }
    wasm_extern_vec_new(&imports, types.num_elems, bindings);
    wasm_importtype_vec_delete(&types);
    instance = wasm_instance_new(store, module, &imports, NULL);
    if(!instance) return 0;
    wasm_instance_exports(instance, &exports);
    wasm_exporttype_vec_t names = {0};
    wasm_module_exports(module, &names);
    for(size_t i = 0; i < names.num_elems; i++)
        if(named(wasm_exporttype_name(names.data[i]), "zx_entry"))
            entry = wasm_extern_as_func(exports.data[i]);
        else if(named(wasm_exporttype_name(names.data[i]), "memory"))
            memory = wasm_extern_as_memory(exports.data[i]);
    wasm_exporttype_vec_delete(&names);
    return entry != NULL && budget != NULL;
}
static int64_t invoke(void) {
    wasm_val_t count = WASM_I64_VAL(10000000);
    wasm_global_set(budget, &count);
    wasm_val_t value[] = {WASM_I64_VAL(0)};
    wasm_val_vec_t arguments = WASM_EMPTY_VEC, result = WASM_ARRAY_VEC(value);
    wasm_trap_t *trap = wasm_func_call(entry, &arguments, &result);
    if(trap || value[0].kind != WASM_I64) abort();
    return value[0].of.i64;
}
#endif

#if defined(MATRIX_WEB)
#include <emscripten/emscripten.h>
#if defined(MATRIX_ZX)
EMSCRIPTEN_KEEPALIVE int MatrixOpen(const char *path) { return open_program(path); }
#endif
EMSCRIPTEN_KEEPALIVE double MatrixRun(void) { double result = (double)invoke(); fflush(stdout); return result; }
#else
static double milliseconds(void) {
    struct timespec value;
    if(clock_gettime(CLOCK_MONOTONIC, &value)) abort();
    return value.tv_sec * 1000.0 + value.tv_nsec / 1000000.0;
}
static double batch(int count, int64_t expected) {
    int64_t checksum = 0;
    double start = milliseconds();
    for(int i = 0; i < count; i++) checksum += invoke();
    fflush(stdout);
    double elapsed = milliseconds() - start;
    if(checksum != expected * count) { fputs("Benchmark oracle mismatch\n", stderr); exit(3); }
    return elapsed;
}
int main(int argc, char **argv) {
    if(argc != 4) return 2;
    int64_t expected = strtoll(argv[2], NULL, 10);
    int samples = atoi(argv[3]);
    if(samples < 3 || samples > 31) return 2;
    double loaded = milliseconds();
#if !defined(MATRIX_NATIVE)
    if(!open_program(argv[1])) { fputs("Cannot load benchmark program\n", stderr); return 3; }
#endif
    loaded = milliseconds() - loaded;
    if(invoke() != expected) return 4;
    fflush(stdout);
    int output = dup(STDOUT_FILENO), sink = open("/dev/null", O_WRONLY);
    if(output < 0 || sink < 0 || dup2(sink, STDOUT_FILENO) < 0) return 5;
    close(sink);
    double start = milliseconds();
    int warmup = 0;
    do { (void)batch(1, expected); warmup++; } while(milliseconds() - start < 150);
    double warmup_ms = milliseconds() - start;
    int batches = 1;
    while(batches < 65536 && batch(batches, expected) < 10) batches *= 2;
    double times[31];
    for(int i = 0; i < samples; i++) times[i] = batch(batches, expected);
    fflush(stdout);
    if(dup2(output, STDOUT_FILENO) < 0) return 5;
    close(output);
    printf("{\"result\":\"%lld\",\"batches\":%d,\"load_ms\":%.9f,\"warmup_ms\":%.9f,\"warmup_count\":%d,\"batch_samples_ms\":[", (long long)expected, batches, loaded, warmup_ms, warmup);
    for(int i = 0; i < samples; i++) printf("%s%.9f", i ? "," : "", times[i]);
    puts("]}");
#if defined(MATRIX_ZX)
    BundleInstanceClose(instance);
    BundleClose(bundle);
#elif defined(MATRIX_WAMR)
    wasm_extern_vec_delete(&exports);
    wasm_instance_delete(instance);
    wasm_extern_vec_delete(&imports);
    wasm_module_delete(module);
    wasm_byte_vec_delete(&image);
    wasm_store_delete(store);
    wasm_engine_delete(engine);
#endif
    return 0;
}
#endif
