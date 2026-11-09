#define _POSIX_C_SOURCE 200809L
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include "visual.h"
int zx_compare_run(const char*,FILE*,int64_t,int,int,int);
typedef struct {char directory[2048];int64_t seed;} Run;
void zx_progress(int trial,int kind,int variant) {__android_log_print(ANDROID_LOG_INFO,"ZxCompare","progress trial=%d kind=%d variant=%d",trial,kind,variant);}
static pthread_mutex_t status_lock=PTHREAD_MUTEX_INITIALIZER;
static ZxVisual *current_visual;
static int process_setup,process_status;
// Status and its mailbox survive Activity recreation. Setup and benchmark start
// happen once per process; the UI never waits for the detached compute worker.
void zx_activity_detach_visual(ZxVisual *visual) {
    pthread_mutex_lock(&status_lock);
    if(current_visual==visual) {current_visual=NULL;zx_visual_release(visual);}
    pthread_mutex_unlock(&status_lock);
}
static void publish(int status) {
    pthread_mutex_lock(&status_lock);process_status=status;
    if(current_visual)zx_visual_done(current_visual,status);
    pthread_mutex_unlock(&status_lock);
}
static void *run(void *argument) {
    Run *r=argument;
    // The worker owns a reference to a status mailbox, never the Activity/window.

    char path[2200];snprintf(path,sizeof(path),"%s/results.jsonl",r->directory);
    FILE *out=fopen(path,"w");
    int result=out?zx_compare_run(r->directory,out,r->seed,4096,16,9):99;
    if(out)fclose(out);fflush(stderr);
    __android_log_print(ANDROID_LOG_INFO,"ZxCompare","finished seed=%lld status=%d",(long long)r->seed,result);
    publish(result);free(r);return NULL;
}
static int benchmark_requested(ANativeActivity *a) {
    JNIEnv *e=a->env;jclass cls=(*e)->GetObjectClass(e,a->clazz);
    jmethodID get=(*e)->GetMethodID(e,cls,"getIntent","()Landroid/content/Intent;");
    jobject intent=(*e)->CallObjectMethod(e,a->clazz,get);
    jclass icls=(*e)->GetObjectClass(e,intent);
    jmethodID extra=(*e)->GetMethodID(e,icls,"getBooleanExtra","(Ljava/lang/String;Z)Z");
    jstring key=(*e)->NewStringUTF(e,"benchmark");
    int enabled=(*e)->CallBooleanMethod(e,intent,extra,key,JNI_FALSE);
    (*e)->DeleteLocalRef(e,key);(*e)->DeleteLocalRef(e,icls);(*e)->DeleteLocalRef(e,intent);(*e)->DeleteLocalRef(e,cls);
    if((*e)->ExceptionCheck(e))abort();return enabled;
}
void ANativeActivity_onCreate(ANativeActivity *a,void *state,size_t size) {
    (void)state;(void)size;Run *r=calloc(1,sizeof(*r));if(!r)abort();
    snprintf(r->directory,sizeof(r->directory),"%s",a->internalDataPath);r->seed=(int64_t)time(NULL)%100000000;
    int first=!process_setup;
    if(first) {
    process_setup=1;process_status=benchmark_requested(a)?2:0;
    char log[2200];snprintf(log,sizeof(log),"%s/native.log",r->directory);
    if(!freopen(log,"w",stderr))abort();setvbuf(stderr,NULL,_IONBF,0);
    char cache[2200];snprintf(cache,sizeof(cache),"%s/native-cache",r->directory);mkdir(cache,0700);
    setenv("TMPDIR",r->directory,1);setenv("ZIRAN_NATIVE_DIR",r->directory,1);
    setenv("ZIRAN_NATIVE_CACHE",cache,1);setenv("ZIRAN_NATIVE_TIMING","1",1);
    const char *files[]={"fixture.zx","fixture.aot","component-old.zx","component-new.zx"};
    for(int i=0;i<4;i++) {
        AAsset *asset=AAssetManager_open(a->assetManager,files[i],AASSET_MODE_STREAMING);if(!asset)abort();
        char path[2200];snprintf(path,sizeof(path),"%s/%s",r->directory,files[i]);FILE *f=fopen(path,"wb");if(!f)abort();
        char bytes[4096];int n;while((n=AAsset_read(asset,bytes,sizeof(bytes)))>0)if(fwrite(bytes,1,n,f)!=(size_t)n)abort();
        if(n<0||fclose(f))abort();AAsset_close(asset);
    }
    }
    ZxVisual *visual=zx_visual_create(a,r->directory);
    pthread_mutex_lock(&status_lock);
    current_visual=visual;zx_visual_retain(visual);
    if(process_status!=2)zx_visual_done(visual,process_status);
    pthread_mutex_unlock(&status_lock);
    if(!first||process_status!=2) {free(r);return;}
    pthread_t worker;
    if(pthread_create(&worker,NULL,run,r))abort();pthread_detach(worker);
}
