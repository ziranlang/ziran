#define _POSIX_C_SOURCE 200809L
#include "ziran_host.h"
#include <stdio.h>
#include <time.h>
static double ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000.0+t.tv_nsec/1e6; }
static int migrate(void *ctx, const BundleState *old, BundleState *next) {
    (void)ctx; size_t i; BundleStateValue v;
    return BundleStateFind(old, "component", "count", &i) && BundleStateValueAt(old,i,&v) &&
        BundleStateFind(next, "component", "count", &i) && BundleStateSetInteger(next,i,v.value.integer);
}
static int reject(void *ctx, BundleInstance *instance) { (void)ctx; (void)instance; return 0; }
static int run(BundleComponentHandle *h,long long expected) { long long n; int has; return BundleComponentRun(h,&n,&has)&&has&&n==expected; }
int main(int argc,char **argv) {
    if(argc!=3) return 2;
    Bundle *old=BundleOpen(argv[1]),*next=BundleOpen(argv[2]);
    if(!old||!next) return 3;
    for(int trial=0;trial<9;trial++) {
        BundleComponent *c=BundleComponentOpen(old,NULL,0,100000);
        if(!c) return 4;
        BundleComponentHandle *h=BundleComponentAcquire(c),*fresh=NULL;
        if(!run(h,8)) return 5;
        double begin=ms();
        if(BundleComponentReplace(c,next,migrate,reject,NULL,NULL)) return 6;
        double rollback=ms()-begin;
        if(!BundleComponentHandleValid(h)||!run(h,9)) return 7;
        begin=ms();
        if(!BundleComponentReplace(c,next,migrate,NULL,NULL,&fresh)) return 8;
        double replace=ms()-begin;
        if(BundleComponentHandleValid(h)||run(h,10)||!run(fresh,11)) return 9;
        printf("{\"trial\":%d,\"rollback_ms\":%.9f,\"replace_ms\":%.9f,\"state_rollback_stale_verified\":true}\n",trial,rollback,replace);
        BundleComponentHandleClose(h); BundleComponentHandleClose(fresh); BundleComponentClose(c);
    }
    BundleClose(old); BundleClose(next); return 0;
}
