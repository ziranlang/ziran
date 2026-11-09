#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "ziran_host.h"
#include "wasm_c_api.h"
#define REQUIRE(c) do { if(!(c)) { fprintf(out,"{\"error\":\"%s:%d %s\"}\n",__FILE__,__LINE__,#c); fflush(out); return 1; } } while(0)
static int64_t inputs[3];
static double ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000.0+t.tv_nsec/1e6; }
int64_t Bench(int64_t,int64_t,int64_t);
int zx_components(FILE*,const char*,const char*);
void zx_progress(int,int,int);
static int input(void *ctx,const char *mod,const char *fn,const VmHostValue *args,int n,VmHostValue *value) {
    (void)ctx;(void)mod;(void)fn;
    if(n!=1||args[0].kind!=VM_HOST_INTEGER||args[0].integer<0||args[0].integer>2) return 0;
    value->kind=VM_HOST_INTEGER;value->type="s64";value->integer=inputs[args[0].integer];return 1;
}
static wasm_trap_t *host(const wasm_val_vec_t *a,wasm_val_vec_t *r) {
    if(a->num_elems!=2||a->data[0].of.i32!=0||a->data[1].of.i64<0||a->data[1].of.i64>2) abort();
    r->data[0]=(wasm_val_t)WASM_I64_VAL(inputs[a->data[1].of.i64]); return NULL;
}
static wasm_trap_t *overrun(const wasm_val_vec_t *a,wasm_val_vec_t *r) { (void)a;(void)r;abort(); }
static int named(const wasm_name_t *n,const char *s) { size_t len=strlen(s); return (n->num_elems==len||(n->num_elems==len+1&&n->data[len]==0))&&!memcmp(n->data,s,len); }
typedef struct {
    wasm_store_t *store; wasm_module_t *module; wasm_instance_t *instance;
    wasm_extern_vec_t imports,exports; wasm_byte_vec_t bytes;
    wasm_global_t *budget; wasm_func_t *entry;
} Wasm;
static int wasm_open(FILE *out,Wasm *w,wasm_engine_t *engine,const char *path,double *load,double *instantiate) {
    double begin=ms();
    w->store=wasm_store_new(engine); REQUIRE(w->store);
    FILE *f=fopen(path,"rb"); REQUIRE(f);
    REQUIRE(!fseek(f,0,SEEK_END)); long len=ftell(f); REQUIRE(len>0&&len<10000000); rewind(f);
    wasm_byte_vec_new_uninitialized(&w->bytes,len); REQUIRE(w->bytes.data);
    REQUIRE(fread(w->bytes.data,1,len,f)==(size_t)len); fclose(f);
    w->module=wasm_module_new(w->store,&w->bytes); REQUIRE(w->module);
    *load=ms()-begin; begin=ms();
    wasm_importtype_vec_t types={0}; wasm_module_imports(w->module,&types);
    REQUIRE(types.num_elems==3);
    wasm_extern_t *imports[3]={0};
    for(size_t i=0;i<types.num_elems;i++) {
        const wasm_importtype_t *t=types.data[i];
        REQUIRE(named(wasm_importtype_module(t),"zx"));
        const wasm_externtype_t *type=wasm_importtype_type(t);
        if(named(wasm_importtype_name(t),"budget")) {
            wasm_val_t v=WASM_I64_VAL(1000000000);
            w->budget=wasm_global_new(w->store,wasm_externtype_as_globaltype_const(type),&v);
            REQUIRE(w->budget); imports[i]=wasm_global_as_extern(w->budget);
        } else {
            wasm_func_callback_t callback=named(wasm_importtype_name(t),"host_i64")?host:overrun;
            REQUIRE(named(wasm_importtype_name(t),"host_i64")||named(wasm_importtype_name(t),"overrun"));
            wasm_func_t *fn=wasm_func_new(w->store,wasm_externtype_as_functype_const(type),callback);
            REQUIRE(fn); imports[i]=wasm_func_as_extern(fn);
        }
    }
    wasm_extern_vec_new(&w->imports,3,imports); wasm_importtype_vec_delete(&types);
    w->instance=wasm_instance_new(w->store,w->module,&w->imports,NULL); REQUIRE(w->instance);
    wasm_instance_exports(w->instance,&w->exports);
    wasm_exporttype_vec_t exports={0}; wasm_module_exports(w->module,&exports);
    for(size_t i=0;i<exports.num_elems;i++)
        if(named(wasm_exporttype_name(exports.data[i]),"zx_entry")) w->entry=wasm_extern_as_func(w->exports.data[i]);
    wasm_exporttype_vec_delete(&exports); REQUIRE(w->entry&&w->budget);
    *instantiate=ms()-begin; return 0;
}
static void wasm_close(Wasm *w) {
    wasm_extern_vec_delete(&w->exports); wasm_instance_delete(w->instance);
    wasm_extern_vec_delete(&w->imports); wasm_module_delete(w->module);
    wasm_byte_vec_delete(&w->bytes); wasm_store_delete(w->store);
}
static int wasm_run(Wasm *w,int64_t *value) {
    wasm_val_t v=WASM_I64_VAL(1000000000); wasm_global_set(w->budget,&v);
    wasm_val_t r[]={WASM_I64_VAL(0)};
    wasm_val_vec_t args=WASM_EMPTY_VEC,results=WASM_ARRAY_VEC(r);
    wasm_trap_t *trap=wasm_func_call(w->entry,&args,&results);
    if(trap) { wasm_trap_delete(trap); return 0; }
    *value=r[0].of.i64; return r[0].kind==WASM_I64;
}
static int call(FILE *out,int variant,BundleInstance *instance,Wasm *w,int64_t seed,int64_t *value) {
    inputs[0]=seed;
    if(variant==2) { *value=Bench(seed,inputs[1],inputs[2]); return 0; }
    if(variant==3) { REQUIRE(wasm_run(w,value)); return 0; }
    long long n=0; int has=0; REQUIRE(BundleInstanceRun(instance,&n,&has)&&has); *value=n;return 0;
}
static int sample(FILE *out,const char *dir,wasm_engine_t *engine,int trial,int kind,int variant,int64_t seed,int rounds,int batches) {
    static const char *names[]={"vm","native","c","wamr-aot"};
    char path[2048]; Bundle *bundle=NULL; BundleInstance *instance=NULL; Wasm w={0};
    inputs[0]=seed;inputs[1]=rounds;inputs[2]=kind;
    double load=0,instantiate=0,begin=ms();
    if(variant<2) {
        snprintf(path,sizeof(path),"%s/fixture.zx",dir); bundle=BundleOpen(path); REQUIRE(bundle);
        load=ms()-begin;begin=ms();
        HostBinding binding={"zx_compare","Input",input,NULL};
        instance=variant==0?BundleInstantiatePortable(bundle,&binding,1):BundleInstantiate(bundle,&binding,1);
        REQUIRE(instance); instantiate=ms()-begin;
    } else if(variant==3) {
        snprintf(path,sizeof(path),"%s/fixture.aot",dir);
        REQUIRE(!wasm_open(out,&w,engine,path,&load,&instantiate));
    }
    int64_t value=0,checksum=0;begin=ms();
    REQUIRE(!call(out,variant,instance,&w,seed,&value)); double first=ms()-begin;
    begin=ms();int warmup=0;
    do { REQUIRE(!call(out,variant,instance,&w,seed+(warmup++&1023),&value)); } while(ms()-begin<500);
    double warmed=ms()-begin;begin=ms();
    for(int j=0;j<batches;j++) { REQUIRE(!call(out,variant,instance,&w,seed+j,&value));checksum+=value; }
    double elapsed=ms()-begin;
    int64_t expected=0; for(int j=0;j<batches;j++) expected+=Bench(seed+j,rounds,kind);
    REQUIRE(checksum==expected);
    fprintf(out,"{\"trial\":%d,\"kind\":%d,\"variant\":\"%s\",\"seed\":%lld,\"rounds\":%d,\"batches\":%d,\"checksum\":%lld,\"load_ms\":%.9f,\"instantiate_ms\":%.9f,\"first_ms\":%.9f,\"warm_ms\":%.9f,\"warmup_count\":%d,\"warmup_ms\":%.6f}\n",trial,kind,names[variant],(long long)seed,rounds,batches,(long long)checksum,load,instantiate,first,elapsed,warmup,warmed);fflush(out);
    zx_progress(trial,kind,variant);
    if(variant==3) wasm_close(&w);
    if(instance) BundleInstanceClose(instance);
    if(bundle) BundleClose(bundle);
    return 0;
}
int zx_compare_run(const char *dir,FILE *out,int64_t seed,int rounds,int batches,int repetitions) {
    double begin=ms();wasm_engine_t *engine=wasm_engine_new(); REQUIRE(engine);
    fprintf(out,"{\"engine\":\"WAMR 2.4.5 AOT-only\",\"pid\":%d,\"engine_init_ms\":%.9f}\n",getpid(),ms()-begin);fflush(out);
    for(int trial=0;trial<repetitions;trial++) {
        // Deterministic changing order: each variant occupies every slot.
        for(int j=0;j<8;j++) { int slot=(j*3+trial)%8;
            REQUIRE(!sample(out,dir,engine,trial,slot/4,slot%4,seed+100*trial,rounds,batches));
        }
    }
    char old[2048],next[2048];snprintf(old,sizeof(old),"%s/component-old.zx",dir);snprintf(next,sizeof(next),"%s/component-new.zx",dir);
    REQUIRE(!zx_components(out,old,next));
    wasm_engine_delete(engine);fprintf(out,"{\"complete\":true}\n");fflush(out);return 0;
}

typedef struct {
    int variant; FILE *out; Bundle *bundle; BundleInstance *instance;
    wasm_engine_t *engine; Wasm wasm;
} VisualGuest;
void zx_visual_guest_close(void *context) {
    VisualGuest *g=context;if(!g)return;
    if(g->variant==3&&g->wasm.instance)wasm_close(&g->wasm);
    if(g->engine)wasm_engine_delete(g->engine);
    if(g->instance)BundleInstanceClose(g->instance);
    if(g->bundle)BundleClose(g->bundle);
    free(g);
}
void *zx_visual_guest_open(const char *directory,int variant,FILE *out) {
    if(variant!=1&&variant!=3)return NULL;
    VisualGuest *g=calloc(1,sizeof(*g));if(!g)return NULL;
    g->variant=variant;g->out=out;char path[2200];
    if(variant==1) {
        snprintf(path,sizeof(path),"%s/fixture.zx",directory);
        g->bundle=BundleOpen(path);if(!g->bundle)goto failed;
        HostBinding binding={"zx_compare","Input",input,NULL};
        g->instance=BundleInstantiate(g->bundle,&binding,1);if(!g->instance)goto failed;
    } else {
        g->engine=wasm_engine_new();if(!g->engine)goto failed;
        snprintf(path,sizeof(path),"%s/fixture.aot",directory);double load,instantiate;
        if(wasm_open(out,&g->wasm,g->engine,path,&load,&instantiate))goto failed;
    }
    return g;
failed:
    zx_visual_guest_close(g);return NULL;
}
int zx_visual_guest_step(void *context,int64_t seed,int64_t *result) {
    VisualGuest *g=context;inputs[1]=128;inputs[2]=1;
    return !call(g->out,g->variant,g->instance,&g->wasm,seed,result)&&*result==Bench(seed,128,1);
}
