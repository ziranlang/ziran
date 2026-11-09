#ifndef ZX_COMPARE_VISUAL_H
#define ZX_COMPARE_VISUAL_H
#include <android/native_activity.h>
typedef struct ZxVisual ZxVisual;
ZxVisual *zx_visual_create(ANativeActivity*,const char*);
void zx_visual_retain(ZxVisual*);
void zx_visual_release(ZxVisual*);
void zx_visual_done(ZxVisual*,int);
#endif
