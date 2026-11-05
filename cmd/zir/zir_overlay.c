#include "zir_overlay.h"
#include <string.h>
#include <stdlib.h>
static _Thread_local const char *overlay_path, *overlay_source;
static _Thread_local size_t overlay_count;
static _Thread_local const char *overlay_root, *overlay_standard, *overlay_roots[32];
static _Thread_local int overlay_roots_count;
int SourceOverlayBegin(const char *path, const char *source, size_t count) {
    if(overlay_path || !path || !source || memchr(source, 0, count) || source[count]) return 0;
    overlay_path=path; overlay_source=source; overlay_count=count; return 1;
}
const char *SourceOverlayRead(const char *path, size_t *count) {
    if(!overlay_path || strcmp(path,overlay_path)) return NULL;
    *count=overlay_count; return overlay_source;
}
void SourceOverlayRestrict(const char *root,const char *standard) { overlay_root=root; overlay_standard=standard; }
static int within(const char *path,const char *root) {
    if(!root) return 0; size_t n=strlen(root);
    return n && !strncmp(path,root,n) && (n==1 || !path[n] || path[n]=='/');
}
int SourceOverlayPermitRoot(const char *root) {
    if(!overlay_path || !root || overlay_roots_count==32) return 0;
    overlay_roots[overlay_roots_count++]=root; return 1;
}
int SourceOverlayAllowed(const char *path) {
    if(!overlay_path || !overlay_root) return 1;
    char *canonical=realpath(path,NULL);
    int allowed=canonical && (within(canonical,overlay_root) || within(canonical,overlay_standard));
    for(int i=0; canonical && !allowed && i<overlay_roots_count; i++) allowed=within(canonical,overlay_roots[i]);
    free(canonical); return allowed;
}
void SourceOverlayEnd(void) { overlay_root=NULL; overlay_standard=NULL; overlay_roots_count=0; overlay_path=NULL; overlay_source=NULL; overlay_count=0; }
