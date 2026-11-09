#define _POSIX_C_SOURCE 200809L
#include "visual.h"
#include <android/bitmap.h>
#include <android/input.h>
#include <android/looper.h>
#include <android/log.h>
#include <android/native_window.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
void *zx_visual_guest_open(const char*,int,FILE*);
int zx_visual_guest_step(void*,int64_t,int64_t*);
void zx_visual_guest_close(void*);
void zx_activity_detach_visual(ZxVisual*);
struct ZxVisual {
    atomic_int refs,ready;pthread_mutex_t lock;int closed,pipefd[2];
    ALooper *looper;AInputQueue *input;ANativeWindow *window;
    JNIEnv *env;jobject bitmap,canvas,paint;jclass bitmap_cls,canvas_cls,paint_cls;
    jmethodID color,size,rect,text,background;int width,height;
    char directory[2048];FILE *trace;int engine,pressed,target,taps,state;int64_t result;
    void *guest[2];
};
static double ms(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000.0+t.tv_nsec/1e6;}
static void fill(ZxVisual *v,float x,float y,float w,float h,int color) {
    JNIEnv *e=v->env;(*e)->CallVoidMethod(e,v->paint,v->color,color);
    (*e)->CallVoidMethod(e,v->canvas,v->rect,x,y,x+w,y+h,v->paint);
}
static void text(ZxVisual *v,const char *s,float x,float y,float size,int color) {
    JNIEnv *e=v->env;(*e)->CallVoidMethod(e,v->paint,v->color,color);
    (*e)->CallVoidMethod(e,v->paint,v->size,size);jstring str=(*e)->NewStringUTF(e,s);
    (*e)->CallVoidMethod(e,v->canvas,v->text,str,x,y,v->paint);(*e)->DeleteLocalRef(e,str);
}
static void bitmap(ZxVisual *v,int w,int h) {
    JNIEnv *e=v->env;if(v->bitmap&&v->width==w&&v->height==h)return;
    if(v->canvas)(*e)->DeleteGlobalRef(e,v->canvas);if(v->bitmap)(*e)->DeleteGlobalRef(e,v->bitmap);
    jclass config=(*e)->FindClass(e,"android/graphics/Bitmap$Config");
    jfieldID argb=(*e)->GetStaticFieldID(e,config,"ARGB_8888","Landroid/graphics/Bitmap$Config;");
    jobject format=(*e)->GetStaticObjectField(e,config,argb);
    jmethodID create=(*e)->GetStaticMethodID(e,v->bitmap_cls,"createBitmap","(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject b=(*e)->CallStaticObjectMethod(e,v->bitmap_cls,create,w,h,format);v->bitmap=(*e)->NewGlobalRef(e,b);
    jmethodID ctor=(*e)->GetMethodID(e,v->canvas_cls,"<init>","(Landroid/graphics/Bitmap;)V");
    jobject c=(*e)->NewObject(e,v->canvas_cls,ctor,b);v->canvas=(*e)->NewGlobalRef(e,c);
    (*e)->DeleteLocalRef(e,c);(*e)->DeleteLocalRef(e,b);(*e)->DeleteLocalRef(e,format);(*e)->DeleteLocalRef(e,config);
    v->width=w;v->height=h;
}
static void draw(ZxVisual *v,const char *event,double event_ms,double compute_ms) {
    if(!v->window)return;double begin=ms();
    int w=ANativeWindow_getWidth(v->window),h=ANativeWindow_getHeight(v->window);
    if(w<=0||h<=0)return;bitmap(v,w,h);JNIEnv *e=v->env;float s=w/720.0f;
    (*e)->CallVoidMethod(e,v->canvas,v->background,(int)0xff101b2a);
    text(v,"ZIRAN / WASM",24*s,65*s,38*s,0xfff0f6ff);
    text(v,"NATIVE ANDROID TEST",24*s,105*s,22*s,0xff9fbbd7);
    fill(v,24*s,140*s,328*s,90*s,v->engine==0?0xff2468b4:0xff26364b);
    fill(v,368*s,140*s,328*s,90*s,v->engine==1?0xffac5b20:0xff26364b);
    text(v,"ZX NATIVE",50*s,197*s,30*s,0xffffffff);
    text(v,"WASM AOT",396*s,197*s,30*s,0xffffffff);
    int ready=atomic_load(&v->ready);
    text(v,ready==1?"READY - TAP THE BUTTON":ready<0?"BENCHMARK FAILED":"COMPUTE BENCHMARK RUNNING",24*s,280*s,26*s,0xffa7c9e9);
    fill(v,24*s,320*s,672*s,150*s,v->pressed?0xffe2ad48:0xff248f86);
    text(v,v->pressed?"PRESSED":"RUN GUEST EVENT",90*s,410*s,38*s,0xffffffff);
    char label[160];snprintf(label,sizeof(label),"TAPS  %d     STATE  %d",v->taps,v->state);
    text(v,label,24*s,550*s,30*s,0xfff0f6ff);
    snprintf(label,sizeof(label),"CHECKSUM  %lld",(long long)v->result);
    text(v,label,24*s,610*s,25*s,0xffa7c9e9);
    text(v,"Same guest work. Same native drawing.",24*s,690*s,24*s,0xff9fbbd7);
    text(v,"Switching keeps the host-owned state.",24*s,730*s,23*s,0xff9fbbd7);
    text(v,"Host fixture - not a Kryon renderer test.",24*s,770*s,23*s,0xff9fbbd7);
    ANativeWindow_Buffer buffer;AndroidBitmapInfo info;void *pixels=NULL;
    if(ANativeWindow_lock(v->window,&buffer,NULL))return;
    if(!AndroidBitmap_getInfo(e,v->bitmap,&info)&&!AndroidBitmap_lockPixels(e,v->bitmap,&pixels)) {
        int rows=h<buffer.height?h:buffer.height,cols=w<buffer.width?w:buffer.width;
        for(int y=0;y<rows;y++)memcpy((char*)buffer.bits+y*buffer.stride*4,(char*)pixels+y*info.stride,cols*4);
        AndroidBitmap_unlockPixels(e,v->bitmap);
    }
    ANativeWindow_unlockAndPost(v->window);double end=ms();
    fprintf(v->trace,"{\"event\":\"%s\",\"engine\":\"%s\",\"pressed\":%s,\"taps\":%d,\"state\":%d,\"checksum\":%lld,\"ready\":%d,\"input_to_buffer_post_ms\":%.6f,\"compute_ms\":%.6f,\"draw_post_ms\":%.6f,\"width\":%d,\"height\":%d}\n",event,v->engine?"wamr-aot":"native",v->pressed?"true":"false",v->taps,v->state,(long long)v->result,ready,event_ms?end-event_ms:0,compute_ms,end-begin,w,h);fflush(v->trace);
}
static int target_at(ZxVisual *v,float x,float y) {
    float s=v->width/720.0f;
    if(x>=24*s&&x<=696*s&&y>=320*s&&y<=470*s)return 1;
    if(y>=140*s&&y<=230*s) {
        if(x>=24*s&&x<=352*s)return 2;
        if(x>=368*s&&x<=696*s)return 3;
    }
    return 0;
}
static int input_event(int fd,int events,void *context) {
    (void)fd;(void)events;ZxVisual *v=context;AInputEvent *event;
    while(v->input&&AInputQueue_getEvent(v->input,&event)>=0) {
        if(AInputQueue_preDispatchEvent(v->input,event))continue;int handled=0;
        if(AInputEvent_getType(event)==AINPUT_EVENT_TYPE_MOTION) {
            float x=AMotionEvent_getX(event,0),y=AMotionEvent_getY(event,0);
            int action=AMotionEvent_getAction(event)&AMOTION_EVENT_ACTION_MASK;
            double event_ms=AMotionEvent_getEventTime(event)/1e6;handled=1;
            if(action==AMOTION_EVENT_ACTION_DOWN) {
                v->target=target_at(v,x,y);v->pressed=v->target==1;draw(v,"down",event_ms,0);
            } else if(action==AMOTION_EVENT_ACTION_POINTER_DOWN||action==AMOTION_EVENT_ACTION_POINTER_UP||action==AMOTION_EVENT_ACTION_CANCEL) {
                v->target=0;v->pressed=0;draw(v,"cancel",event_ms,0);
            } else if(action==AMOTION_EVENT_ACTION_MOVE&&v->target&&target_at(v,x,y)!=v->target) {
                v->target=0;v->pressed=0;draw(v,"cancel-drag",event_ms,0);
            } else if(action==AMOTION_EVENT_ACTION_UP) {
                int target=v->target;v->target=0;v->pressed=0;
                if(!target||target_at(v,x,y)!=target) {draw(v,"up-outside",event_ms,0);}
                else if(target>1) {v->engine=target-2;draw(v,"select",event_ms,0);}
                else {
                    double begin=ms();
                    if(atomic_load(&v->ready)==1) {
                        if(!v->guest[v->engine])v->guest[v->engine]=zx_visual_guest_open(v->directory,v->engine?3:1,v->trace);
                        if(v->guest[v->engine]&&zx_visual_guest_step(v->guest[v->engine],123457+v->taps,&v->result)) {
                            v->taps++;v->state=v->result%100000;
                        } else atomic_store(&v->ready,-1);
                    }
                    draw(v,"up",event_ms,ms()-begin);
                }
            }
        }
        AInputQueue_finishEvent(v->input,event,handled);
    }
    return 1;
}
static int wake(int fd,int events,void *context) {
    (void)events;char bytes[64];while(read(fd,bytes,sizeof(bytes))>0){}
    draw(context,"status",0,0);return 1;
}
static void created(ANativeActivity *a,ANativeWindow *window) {
    ZxVisual *v=a->instance;v->window=window;ANativeWindow_acquire(window);
    ANativeWindow_setBuffersGeometry(window,0,0,WINDOW_FORMAT_RGBA_8888);draw(v,"window",0,0);
}
static void resized(ANativeActivity *a,ANativeWindow *window) {(void)window;draw(a->instance,"resize",0,0);}
static void window_destroyed(ANativeActivity *a,ANativeWindow *window) {
    ZxVisual *v=a->instance;if(v->window==window) {ANativeWindow_release(window);v->window=NULL;}
}
static void input_created(ANativeActivity *a,AInputQueue *input) {
    ZxVisual *v=a->instance;v->input=input;AInputQueue_attachLooper(input,v->looper,1,input_event,v);
}
static void input_destroyed(ANativeActivity *a,AInputQueue *input) {
    ZxVisual *v=a->instance;AInputQueue_detachLooper(input);if(v->input==input)v->input=NULL;
}
void zx_visual_retain(ZxVisual *v) {atomic_fetch_add(&v->refs,1);}
void zx_visual_release(ZxVisual *v) {if(atomic_fetch_sub(&v->refs,1)==1) {pthread_mutex_destroy(&v->lock);free(v);}}
void zx_visual_done(ZxVisual *v,int status) {
    atomic_store(&v->ready,status? -1:1);pthread_mutex_lock(&v->lock);
    if(!v->closed) {char byte=1;(void)write(v->pipefd[1],&byte,1);}
    pthread_mutex_unlock(&v->lock);
}
static void destroy(ANativeActivity *a) {
    ZxVisual *v=a->instance;zx_activity_detach_visual(v);pthread_mutex_lock(&v->lock);v->closed=1;
    ALooper_removeFd(v->looper,v->pipefd[0]);close(v->pipefd[0]);close(v->pipefd[1]);
    pthread_mutex_unlock(&v->lock);
    if(v->input)AInputQueue_detachLooper(v->input);
    if(v->window)ANativeWindow_release(v->window);
    for(int i=0;i<2;i++)if(v->guest[i])zx_visual_guest_close(v->guest[i]);
    JNIEnv *e=v->env;jobject refs[]={v->bitmap,v->canvas,v->paint,v->bitmap_cls,v->canvas_cls,v->paint_cls};
    for(int i=0;i<6;i++)if(refs[i])(*e)->DeleteGlobalRef(e,refs[i]);
    fclose(v->trace);ALooper_release(v->looper);a->instance=NULL;zx_visual_release(v);
}
static jclass klass(JNIEnv *e,const char *name) {
    jclass local=(*e)->FindClass(e,name);jclass global=(*e)->NewGlobalRef(e,local);(*e)->DeleteLocalRef(e,local);return global;
}
ZxVisual *zx_visual_create(ANativeActivity *a,const char *directory) {
    ZxVisual *v=calloc(1,sizeof(*v));if(!v)abort();
    atomic_init(&v->refs,1);atomic_init(&v->ready,0);pthread_mutex_init(&v->lock,NULL);
    snprintf(v->directory,sizeof(v->directory),"%s",directory);char path[2200];snprintf(path,sizeof(path),"%s/visual.jsonl",directory);
    v->trace=fopen(path,"w");if(!v->trace)abort();v->env=a->env;JNIEnv *e=v->env;
    v->bitmap_cls=klass(e,"android/graphics/Bitmap");v->canvas_cls=klass(e,"android/graphics/Canvas");v->paint_cls=klass(e,"android/graphics/Paint");
    jmethodID ctor=(*e)->GetMethodID(e,v->paint_cls,"<init>","(I)V");
    jobject paint=(*e)->NewObject(e,v->paint_cls,ctor,1);v->paint=(*e)->NewGlobalRef(e,paint);(*e)->DeleteLocalRef(e,paint);
    v->color=(*e)->GetMethodID(e,v->paint_cls,"setColor","(I)V");
    v->size=(*e)->GetMethodID(e,v->paint_cls,"setTextSize","(F)V");
    v->rect=(*e)->GetMethodID(e,v->canvas_cls,"drawRect","(FFFFLandroid/graphics/Paint;)V");
    v->text=(*e)->GetMethodID(e,v->canvas_cls,"drawText","(Ljava/lang/String;FFLandroid/graphics/Paint;)V");
    v->background=(*e)->GetMethodID(e,v->canvas_cls,"drawColor","(I)V");
    if((*e)->ExceptionCheck(e))abort();
    if(pipe(v->pipefd))abort();
    fcntl(v->pipefd[0],F_SETFL,O_NONBLOCK);fcntl(v->pipefd[1],F_SETFL,O_NONBLOCK);
    v->looper=ALooper_forThread();if(!v->looper)v->looper=ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);ALooper_acquire(v->looper);
    ALooper_addFd(v->looper,v->pipefd[0],ALOOPER_POLL_CALLBACK,ALOOPER_EVENT_INPUT,wake,v);
    a->instance=v;a->callbacks->onDestroy=destroy;
    a->callbacks->onNativeWindowCreated=created;a->callbacks->onNativeWindowResized=resized;
    a->callbacks->onNativeWindowRedrawNeeded=resized;a->callbacks->onNativeWindowDestroyed=window_destroyed;
    a->callbacks->onInputQueueCreated=input_created;a->callbacks->onInputQueueDestroyed=input_destroyed;
    return v;
}
