/* Test-only native allocator fault injection; no raster implementation. */
#include "image_resize_fallible.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void *,size_t);
void __real_free(void *);
static struct { void *pointer; size_t size; } slots[1024];
static size_t live, bytes, peak, calls, fail_at;
static int tracking, rejected;
static int deny(void) {
    if (!tracking) return 0;
    calls++;
    if (fail_at && calls==fail_at) { rejected=1; return 1; }
    return 0;
}
static void remove_pointer(void *p) {
    if (!p) return;
    for (size_t i=0;i<1024;i++) if (slots[i].pointer==p) {
        live--; bytes-=slots[i].size; slots[i].pointer=NULL; return;
    }
}
static void add_pointer(void *p,size_t n) {
    if (!tracking || !p) return;
    for (size_t i=0;i<1024;i++) if (!slots[i].pointer) {
        slots[i].pointer=p; slots[i].size=n; live++; bytes+=n;
        if(bytes>peak)peak=bytes;
        if(bytes>16777216)abort();
        return;
    }
    abort();
}
void *__wrap_malloc(size_t n) { if(deny())return NULL;void*p=__real_malloc(n);add_pointer(p,n);return p; }
void *__wrap_calloc(size_t n,size_t s) { if(deny())return NULL;void*p=__real_calloc(n,s);add_pointer(p,n*s);return p; }
void *__wrap_realloc(void *p,size_t n) {
    if(deny())return NULL;
    void *r=__real_realloc(p,n);
    if(r||!n){remove_pointer(p);add_pointer(r,n);}
    return r;
}
void __wrap_free(void *p) {remove_pointer(p);__real_free(p);}
static int balanced(int mode,size_t at) {
    if(!live&&!bytes)return 1;
    fprintf(stderr,"mode %d fault %zu retained %zu allocations / %zu bytes\n",mode,at,live,bytes);return 0;
}
int main(void) {
    if(Prepare()!=42)return 1;
    tracking=1;
    size_t faults=0;
    for(int mode=0;mode<5;mode++) {
        for(int repeat=0;repeat<200;repeat++)if(Exercise(mode)!=42||!balanced(mode,0))return 2;
        calls=0;
        if(Exercise(mode)!=42||!balanced(mode,0))return 3;
        size_t total=calls;
        for(size_t at=1;at<=total;at++) {
            calls=0;fail_at=at;rejected=0;
            int result=Exercise(mode);
            if(!rejected||result!=0||!balanced(mode,at)) {
                fprintf(stderr,"mode %d fault %zu result %d rejected %d\n",mode,at,result,rejected);return 4;
            }
            faults++;
            fail_at=0;
            if(Exercise(mode)!=42||!balanced(mode,at))return 5;
        }
    }
    tracking=0;Finish();
    fprintf(stderr,"bicubic native: 1000 repeated resizes, %zu allocation faults + immediate recovery; live=%zu bytes=%zu peak=%zu\n",faults,live,bytes,peak);
    return 0;
}
