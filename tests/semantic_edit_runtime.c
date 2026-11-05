#include "zir_edit.h"
#include "zir_document.h"
#include "zir_overlay.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
static int reject_rename;
static size_t grow_request;
static int request_stats;
int __real_fstat(int,struct stat *);
int __wrap_fstat(int fd,struct stat *info) {
    int result=__real_fstat(fd,info);
    if(!result && grow_request) {
        /* Grow after the caller's size snapshot and before read_fd's stat. */
        assert(++request_stats==1);
        int output=open("request.txt",O_WRONLY|O_APPEND); assert(output>=0);
        for(size_t i=0;i<grow_request;i++) assert(write(output,"x",1)==1);
        assert(!close(output)); grow_request=0;
    } else if(request_stats) request_stats++;
    return result;
}
int __real_renameat(int,const char *,int,const char *);
int __wrap_renameat(int a,const char *b,int c,const char *d) {
    if(reject_rename) return -1;
    return __real_renameat(a,b,c,d);
}
#define S(x) StringView((x),strlen(x))
static const char before[]="Answer :: () -> s32 { return 41 }\n";
static const char after[]="Answer :: () -> s32 { return 42 }\n";
static void put(const char *text) { FILE *file=fopen("main.zi","wb"); assert(file); fputs(text,file); assert(!fclose(file)); }
static void unchanged(void) { FILE *file=fopen("main.zi","rb"); assert(file); char data[128]; size_t n=fread(data,1,sizeof(data),file); fclose(file); assert(n==sizeof(before)-1 && !memcmp(data,before,n)); }
static void read_boundary(size_t initial,size_t growth,int64_t limit,int accepted) {
    FILE *file=fopen("request.txt","wb"); assert(file);
    for(size_t i=0;i<initial;i++) assert(fputc('x',file)!=EOF);
    assert(!fclose(file));
    request_stats=0; grow_request=growth;
    unsigned char *text=ZiranToolReadText(S("request.txt"),limit);
    assert(!!text==accepted);
    if(text) { assert(strlen((char *)text)==initial+growth); free(text); }
    if(growth) assert(!grow_request && request_stats==2);
    request_stats=0; unchanged();
    file=fopen("map.json","rb"); assert(file);
    char map[16]={0}; assert(fread(map,1,sizeof(map),file)==8);
    assert(!fclose(file)); assert(!strcmp(map,"sentinel"));
}
static void *prepare(const char *root) {
    put(before); char hash[65]; DocumentDigest(before,sizeof(before)-1,hash);
    void *edit=ZiranEditOpen(S(root),S("main.zi"),S(hash)); assert(edit);
    const char *at=strstr(before,"41"); assert(at);
    assert(ZiranEditAdd(edit,at-before,at-before+2,S("41"),S("42"),S("")));
    assert(ZiranEditBuild(edit)); assert(ZiranEditValidate(edit)); return edit;
}
int main(void) {
    char *root=getcwd(NULL,0); assert(root);
    void *edit=prepare(root);
    put("Answer :: () -> s32 { return 99 }\n");
    assert(!ZiranEditFinish(edit,S(after),1)); ZiranEditClose(edit);
    FILE *file=fopen("main.zi","rb"); assert(file); char data[128]={0}; fread(data,1,sizeof(data)-1,file); fclose(file); assert(strstr(data,"99"));
    edit=prepare(root); reject_rename=1;
    assert(!ZiranEditFinish(edit,S(after),1)); unchanged(); ZiranEditClose(edit);
    FILE *map=fopen("map.json","wb"); assert(map); assert(fputs("sentinel",map)>=0); assert(!fclose(map));
    read_boundary(0,0,0,1);
    read_boundary(7,0,8,1);
    read_boundary(8,0,8,1);
    read_boundary(9,0,8,0);
    read_boundary(7,1,8,1);
    read_boundary(8,1,8,0);
    read_boundary(1048575,1,1048576,1);
    read_boundary(1048575,2,1048576,0);
    /* The overlay lifecycle is bounded and never left active after rejection. */
    assert(!SourceOverlayBegin("one.zi","ab",1));
    assert(SourceOverlayBegin("one.zi","",0)); assert(!SourceOverlayBegin("two.zi","",0));
    size_t count=1; assert(SourceOverlayRead("one.zi",&count) && count==0); assert(!SourceOverlayRead("two.zi",&count));
    SourceOverlayEnd(); assert(SourceOverlayBegin("two.zi","",0)); SourceOverlayEnd(); free(root); return 0;
}
