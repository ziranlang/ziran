/* Native ABI adapter; workload implementations live in kernels.zi/reference.c. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifdef STANDARD_REFERENCE
int64_t Run(int64_t,int64_t);
#else
#include "ziran_host.h"
static int64_t inputs[2];
static BundleInstance *instance;
static int input(void *ctx,const char *mod,const char *fn,const HostValue *args,int n,HostValue *out) {
    (void)ctx;
    (void)mod;
    (void)fn;
    if(n!=1||args[0].kind!=HOST_INTEGER||args[0].integer<0||args[0].integer>1)return 0;
    out->kind=HOST_INTEGER;
    out->type="s64";
    out->integer=inputs[args[0].integer];
    return 1;
}
#endif
static int64_t invoke(int64_t kind,int64_t size) {
    #ifdef STANDARD_REFERENCE
    return Run(kind,size);
    #else
    inputs[0]=kind;
    inputs[1]=size;
    long long value;
    int present;
    if(!BundleInstanceRun(instance,&value,&present)||!present)abort();
    return value;
    #endif
}
static double now(void) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t))abort();
    return t.tv_sec*1000.0+t.tv_nsec/1e6;
}
int main(int argc,char **argv) {
    if(argc!=6)return 2;
    int64_t kind=atoll(argv[2]),size=atoll(argv[3]);
    int samples=atoi(argv[4]);
    if(kind<0||kind>28||size<1||(samples!=0&&samples!=11))return 2;
    #ifndef STANDARD_REFERENCE
    Bundle *bundle=BundleOpen(argv[1]);
    if(!bundle)return 3;
    HostBinding binding= {
        "entry","Input",input,NULL
    }
    ;
    BundleInstanceOptions options= {
        0
    }
    ;
    options.backend=BUNDLE_BACKEND_NATIVE;
    instance=BundleInstantiateWithOptions(bundle,&binding,1,&options);
    if(!instance)return 4;
    if(BundleInstanceBackend(instance)!=BUNDLE_EXECUTION_NATIVE_X86_64 && BundleInstanceBackend(instance)!=BUNDLE_EXECUTION_NATIVE_AARCH64)return 5;
    #endif
    int64_t first=invoke(kind,size),expected=first;
    if(strcmp(argv[5],"oracle")) {
        expected=atoll(argv[5]);
        if(first!=expected) {
            fprintf(stderr,"%lld != %lld\n",(long long)first,(long long)expected);
            return 6;
        }
    }
    if(!samples) {
        printf("{\"result\":\"%lld\"}\n",(long long)first);
        #ifndef STANDARD_REFERENCE
        BundleInstanceClose(instance);
        BundleClose(bundle);
        #endif
        return 0;
    }
    double times[11],start=now();
    do {
        if(invoke(kind,size)!=expected)return 6;
    }
    while(now()-start<150);
    for(int i=0;i<samples;i++) {
        start=now();
        int64_t value=invoke(kind,size);
        times[i]=now()-start;
        if(value!=expected)return 6;
    }
    printf("{\"result\":\"%lld\",\"batches\":1,\"batch_samples_ms\":[",(long long)first);
    for(int i=0;i<samples;i++)printf("%s%.9f",i?",":"",times[i]);
    puts("]}");
    #ifndef STANDARD_REFERENCE
    BundleInstanceClose(instance);
    BundleClose(bundle);
    #endif
    return 0;
}
