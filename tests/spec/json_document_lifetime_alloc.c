#include "json_document_lifetime_test.h"
#include "json_document_secure_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
static struct { void *pointer; size_t bytes; } slots[4096];
static size_t live_bytes, peak_bytes;
static int tracking, live;
static size_t allocation, fail_at;
static int rejected;
static int deny(void) {
    if (!tracking) return 0;
    allocation++;
    if (fail_at && allocation == fail_at) { rejected = 1; return 1; }
    return 0;
}
static void assert_erased(void *pointer) {
    if (!tracking || !pointer) return;
    for (int i = 0; i < 4096; i++) if (slots[i].pointer == pointer) {
        const unsigned char *bytes = pointer;
        for (size_t j = 0; j + 6 <= slots[i].bytes; j++) {
            if (!memcmp(bytes + j, "secret", 6)) {
                fprintf(stderr,"private JSON bytes freed or relocated before erasure\n"); abort();
            }
        }
        return;
    }
}
static void remove_pointer(void *pointer) {
    if (!pointer) return;
    for (int i = 0; i < 4096; i++) if (slots[i].pointer == pointer) {
        live_bytes -= slots[i].bytes; live--; slots[i].pointer = NULL; return;
    }
}
static void add_pointer(void *pointer, size_t bytes) {
    if (!tracking || !pointer) return;
    for (int i = 0; i < 4096; i++) if (!slots[i].pointer) {
        slots[i].pointer = pointer; slots[i].bytes = bytes;
        live++; live_bytes += bytes;
        if (live_bytes > peak_bytes) peak_bytes = live_bytes;
        if (peak_bytes > 65536) { fprintf(stderr,"JSON allocations exceeded 64 KiB\n"); abort(); }
        return;
    }
    fprintf(stderr,"JSON allocation count grew without release\n"); abort();
}
void *__wrap_malloc(size_t size) {
    if (deny()) return NULL;
    void *pointer = __real_malloc(size); add_pointer(pointer,size); return pointer;
}
void *__wrap_calloc(size_t count, size_t size) {
    if (deny()) return NULL;
    void *pointer = __real_calloc(count,size); add_pointer(pointer,count*size); return pointer;
}
void *__wrap_realloc(void *pointer, size_t size) {
    if (deny()) return NULL;
    assert_erased(pointer);
    void *result = __real_realloc(pointer,size);
    if (result || !size) { remove_pointer(pointer); add_pointer(result,size); }
    return result;
}
void __wrap_free(void *pointer) { assert_erased(pointer); remove_pointer(pointer); __real_free(pointer); }
int main(void) {
#ifndef LIFETIME_MEMORY_ONLY
    if (Answer() != 42) return 1;
#endif
    tracking = 1;
    int result = Secure();
    if (result != 42 || live) { fprintf(stderr,"secure lifecycle result=%d live=%d\n",result,live); return 3; }
    allocation = 0;
    result = Faults();
    size_t attempts = allocation;
    if (result != 42 || live) return 4;
    for (size_t at = 1; at <= attempts; at++) {
        allocation = 0; fail_at = at; rejected = 0;
        result = Faults();
        if (!rejected || result != 0 || live || live_bytes) {
            fprintf(stderr,"JSON allocation fault=%zu rejected=%d result=%d live=%d bytes=%zu\n",at,rejected,result,live,live_bytes);
            return 5;
        }
    }
    fail_at = 0;
    result = Memory();
    tracking = 0;
    if (result != 42 || live || live_bytes) {
        fprintf(stderr,"JSON lifecycle result=%d live=%d bytes=%zu peak=%zu\n", result,live,live_bytes,peak_bytes);
        return 2;
    }
    return 0;
}
