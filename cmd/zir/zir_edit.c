#include <limits.h>
#include "zir_edit.h"
#include "zir_document.h"
#include "zir_semantic.h"
#include "zir_overlay.h"
#include "zir_load.h"
#include "zir_check.h"
#include "zir_law.h"
#include "zir_serial.h"
#include "zir_diagnostic.h"
#include "zir_token.h"
#include "zir_text.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define EDIT_BYTES (16*1024*1024)
#define EDIT_COUNT 256
typedef struct EditPart { size_t start,end,count; char *replacement; } EditPart;
typedef struct Edit {
    char *root,*path,*source,*candidate,*leaf;
    int parent,fd,count;
    size_t size,candidate_size;
    struct stat info;
    char **module_paths, **module_roots;
    int module_count, define_count;
    char *defines[32];
    EditPart parts[EDIT_COUNT];
} Edit;
static int fail(const char *message) { Diagnostic((ZirSourceSpan){0},"command.arguments","source tool: %s",message); return 0; }
int ZiranEditJsonEnabled(void) { return DiagnosticJsonEnabled(); }
int ZiranEditDiagnosticFormat(String value) {
    if(value.length==4 && !memcmp(value.data,"json",4)) return SetDiagnosticFormat("json");
    if(value.length==4 && !memcmp(value.data,"text",4)) return SetDiagnosticFormat("text");
    return 0;
}
static char *copy(String value) {
    if(value.length>EDIT_BYTES || (value.length && (!value.data || memchr(value.data,0,value.length)))) return NULL;
    char *text=malloc(value.length+1); if(!text) return NULL;
    if(value.length) memcpy(text,value.data,value.length); text[value.length]=0; return text;
}
static int utf8(const char *text,size_t count) {
    for(size_t i=0;i<count;) {
        unsigned char c=(unsigned char)text[i];
        size_t n=c<0x80 ? 1 : c>=0xc2 && c<=0xdf ? 2 : c>=0xe0 && c<=0xef ? 3 : c>=0xf0 && c<=0xf4 ? 4 : 0;
        if(!c || !n || n>count-i) return 0;
        for(size_t j=1;j<n;j++) if((unsigned char)text[i+j]<0x80 || (unsigned char)text[i+j]>0xbf) return 0;
        if((c==0xe0 && (unsigned char)text[i+1]<0xa0) || (c==0xed && (unsigned char)text[i+1]>=0xa0) ||
           (c==0xf0 && (unsigned char)text[i+1]<0x90) || (c==0xf4 && (unsigned char)text[i+1]>=0x90)) return 0;
        i+=n;
    }
    return 1;
}
static char *read_fd(int fd,size_t *count) {
    struct stat info; if(fstat(fd,&info) || !S_ISREG(info.st_mode) || info.st_size<0 || info.st_size>33554464) return NULL;
    char *text=malloc((size_t)info.st_size+1); if(!text) return NULL;
    size_t at=0,size=(size_t)info.st_size;
    while(at<size) { ssize_t n=pread(fd,text+at,size-at,(off_t)at); if(n<=0) { free(text); return NULL; } at+=(size_t)n; }
    text[size]=0; *count=size; return text;
}
unsigned char *ZiranToolReadText(String path,int64_t limit) {
    if(limit<0 || limit>33554464) { fail("invalid text byte limit"); return NULL; }
    char *name=copy(path); if(!name) { fail("invalid request path"); return NULL; }
    int fd=open(name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC); free(name);
    struct stat info;
    if(fd<0) { fail("cannot open request"); return NULL; }
    if(fstat(fd,&info) || !S_ISREG(info.st_mode) || info.st_size<0 || info.st_size>limit) {
        close(fd); fail("input must be a regular file within the byte limit"); return NULL;
    }
    size_t count=0; char *text=read_fd(fd,&count); close(fd);
    if(count>(size_t)limit) { free(text); fail("input must be a regular file within the byte limit"); return NULL; }
    if(!text || memchr(text,0,count) || !utf8(text,count)) { free(text); fail("request must be UTF-8 without null bytes"); return NULL; }
    return (unsigned char *)text;
}
const unsigned char *ZiranToolDigest(String text) {
    static _Thread_local char digest[65];
    DocumentDigest(text.data,text.length,digest); return (const unsigned char *)digest;
}
int ZiranToolWriteMap(String path,String text) {
    char *name=copy(path); if(!name) return fail("invalid map path");
    char *slash=strrchr(name,'/'); const char *leaf=slash ? slash+1 : name;
    if(!*leaf || !strcmp(leaf,".") || !strcmp(leaf,"..")) { free(name); return fail("invalid map filename"); }
    char *directory=NULL;
    if(slash) { *slash=0; directory=realpath(*name ? name : "/",NULL); } else directory=realpath(".",NULL);
    int parent=directory ? open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC) : -1; free(directory);
    if(parent<0) { free(name); return fail("cannot open map directory"); }
    struct stat info; int exists=fstatat(parent,leaf,&info,AT_SYMLINK_NOFOLLOW)==0;
    if(exists && (!S_ISREG(info.st_mode) || info.st_nlink!=1)) { close(parent); free(name); return fail("map destination is linked or nonregular"); }
    char temporary[64]; int fd=-1;
    for(unsigned i=0;i<128 && fd<0;i++) { snprintf(temporary,sizeof(temporary),".zi-map-%ld-%u",(long)getpid(),i); fd=openat(parent,temporary,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600); }
    if(fd<0) { close(parent); free(name); return fail("cannot create map"); }
    size_t at=0;
    while(at<text.length) { ssize_t n=write(fd,text.data+at,text.length-at); if(n<=0) break; at+=(size_t)n; }
    int valid=at==text.length && !fchmod(fd,exists ? info.st_mode&0777 : 0644) && !fsync(fd);
    if(close(fd)) valid=0;
    if(valid) valid=renameat(parent,temporary,parent,leaf)==0;
    if(!valid) unlinkat(parent,temporary,0);
    close(parent); free(name); if(!valid) return fail("cannot finish map"); return 1;
}
void ZiranEditClose(void *handle) {
    Edit *e=handle; if(!e) return;
    for(int i=0;i<e->count;i++) free(e->parts[i].replacement);
    for(int i=0;i<e->module_count;i++) { free(e->module_paths[i]); free(e->module_roots[i]); }
    free(e->module_paths); free(e->module_roots);
    for(int i=0;i<e->define_count;i++) free(e->defines[i]);
    if(e->fd>=0) close(e->fd); if(e->parent>=0) close(e->parent);
    free(e->root); free(e->path); free(e->source); free(e->candidate); free(e->leaf); free(e);
}
void *ZiranEditOpen(String root,String path,String hash) {
    Edit *e=calloc(1,sizeof(*e)); if(!e) { fail("out of memory"); return NULL; } e->fd=e->parent=-1;
    char *raw=copy(root),*relative=copy(path),*expected=copy(hash);
    e->root=raw ? realpath(raw,NULL) : NULL; free(raw);
    int valid=e->root && relative && expected && strlen(expected)==64 && relative[0] && relative[0]!='/';
    for(size_t i=0;valid && i<64;i++) valid=(expected[i]>='0' && expected[i]<='9') || (expected[i]>='a' && expected[i]<='f');
    if(valid) {
        size_t n=strlen(relative); valid=n>3 && !strcmp(relative+n-3,".zi");
        e->path=malloc(strlen(e->root)+n+2); if(e->path) sprintf(e->path,"%s/%s",e->root,relative); else valid=0;
        e->parent=open(e->root,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        if(e->parent<0) valid=0;
        char *component=relative;
        while(valid) {
            char *slash=strchr(component,'/'); if(slash) *slash=0;
            if(!*component || !strcmp(component,".") || !strcmp(component,"..")) { valid=0; break; }
            if(!slash) { e->leaf=strdup(component); break; }
            int next=openat(e->parent,component,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
            if(next<0) { valid=0; break; }
            close(e->parent); e->parent=next; component=slash+1;
        }
    }
    if(valid && e->leaf) e->fd=openat(e->parent,e->leaf,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    if(e->fd<0 || fstat(e->fd,&e->info) || e->info.st_size>EDIT_BYTES) valid=0;
    if(valid) { e->source=read_fd(e->fd,&e->size); valid=e->source && utf8(e->source,e->size); }
    if(valid) { char actual[65]; DocumentDigest(e->source,e->size,actual); valid=!strcmp(actual,expected); }
    if(valid) {
        char *canonical=realpath(e->path,NULL);
        if(!canonical) valid=0;
        else { free(e->path); e->path=canonical; }
    }
    free(relative); free(expected);
    if(!valid) { ZiranEditClose(e); fail("invalid path, non-UTF-8 source, or stale document hash"); return NULL; }
    return e;
}
int ZiranEditModulePath(void *handle,String value) {
    Edit *e=handle; if(!e || e->candidate) return fail("invalid edit state");
    char *path=copy(value); if(!path) return fail("invalid module path");
    char *equals=strchr(path,'='); const char *directory=equals ? equals+1 : path;
    char *canonical=realpath(directory,NULL); struct stat info;
    if(!canonical || stat(canonical,&info) || !S_ISDIR(info.st_mode)) { free(path); free(canonical); return fail("module path is not a directory"); }
    size_t prefix=equals ? (size_t)(equals-path)+1 : 0;
    char *argument=malloc(prefix+strlen(canonical)+1);
    if(!argument) { free(path); free(canonical); return fail("out of memory"); }
    memcpy(argument,path,prefix); strcpy(argument+prefix,canonical); free(path);
    if(e->module_count == INT_MAX) { free(argument); free(canonical); return fail("module path storage overflow"); }
    char **paths=realloc(e->module_paths, ((size_t)e->module_count+1)*sizeof(*paths));
    if(!paths) { free(argument); free(canonical); return fail("out of memory"); }
    e->module_paths=paths;
    char **roots=realloc(e->module_roots, ((size_t)e->module_count+1)*sizeof(*roots));
    if(!roots) { free(argument); free(canonical); return fail("out of memory"); }
    e->module_roots=roots;
    e->module_paths[e->module_count]=argument; e->module_roots[e->module_count++]=canonical; return 1;
}
int ZiranEditDefine(void *handle,String name) {
    Edit *e=handle; if(!e || e->define_count==32 || !name.length || name.length>=ZIR_NAME_MAX) return fail("invalid define");
    for(size_t i=0;i<name.length;i++) {
        unsigned char c=(unsigned char)name.data[i];
        if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_' || (i && c>='0' && c<='9'))) return fail("invalid define");
    }
    char *text=copy(name); if(!text) return fail("out of memory"); e->defines[e->define_count++]=text; return 1;
}
static int check_candidate(Edit *e,const char *text,size_t count,ProgramSet *set) {
    if(!SourceOverlayBegin(e->path,text,count)) return fail("cannot establish source overlay");
    const char *standard=ToolchainStandardDirectory();
    char *canonical_standard=standard ? realpath(standard,NULL) : NULL;
    SourceOverlayRestrict(e->root,canonical_standard);
    for(int i=0;i<e->module_count;i++) if(!SourceOverlayPermitRoot(e->module_roots[i])) { SourceOverlayEnd(); free(canonical_standard); return fail("too many permitted module roots"); }
    const char *input=e->path;
    int result=ProgramsLoadWithDefines(set,e->root,(const char *const *)e->module_paths,e->module_count,
        (const char *const *)e->defines,e->define_count,&input,1);
    SourceOverlayEnd(); free(canonical_standard);
    if(result) {
        char expected[65]; DocumentDigest(text,count,expected);
        int captured=0;
        for(int p=0;p<set->count;p++) if(!strcmp(set->paths[p],e->path))
            for(int m=0;m<set->programs[p]->module_count;m++) {
                const ZirDocument *document=set->programs[p]->modules[m].document;
                if(document && document->bytes==count && !strcmp(document->sha256,expected)) captured=1;
            }
        if(!captured) return fail("candidate source overlay was not loaded");
    }
    if(result) result=CheckCanonicalPrograms(set->programs,set->count,(const char *const *)set->paths);
    return result;
}
/* A semantic replacement must cover one entire checked declaration. The lexer
 * determines its terminator, respecting string/comment tokens and nesting.
 * Omitted terminators and rewritten raw coordinates are rejected. */
static int declaration_range(Edit *e,ZirModule *module,ZirSourceSpan span,size_t *start,size_t *end) {
    const ZirDocument *doc=DocumentForSpan(module,span);
    if(!doc || doc->rewritten || strcmp(SpanPath(span),module->source_path) ||
       !DocumentOffset(doc,span.line,span.column,start)) return 0;
    ZirLexer lexer; LexerInit(&lexer,e->source,e->path);
    int active=0,braces=0,parens=0,brackets=0,saw_brace=0;
    for(;;) {
        ZirToken token=LexerNextCode(&lexer); if(token.kind==ZIR_TOKEN_EOF || token.truncated) return 0;
        size_t at;
        if(!DocumentOffset(doc,token.span.line,token.span.column,&at)) return 0;
        if(!active) { if(at<*start) continue; if(at!=*start) return 0; active=1; }
        if(token.kind!=ZIR_TOKEN_PUNCT && token.kind!=ZIR_TOKEN_OPERATOR) continue;
        if(!strcmp(token.text,"(")) parens++;
        else if(!strcmp(token.text,")")) parens--;
        else if(!strcmp(token.text,"[")) brackets++;
        else if(!strcmp(token.text,"]")) brackets--;
        else if(!strcmp(token.text,"{")) { braces++; saw_brace=1; }
        else if(!strcmp(token.text,"}")) {
            braces--;
            if(!braces && !parens && !brackets && saw_brace) { *end=lexer.pos; return 1; }
        } else if(!strcmp(token.text,";") && !braces && !parens && !brackets) { *end=lexer.pos; return 1; }
        if(braces<0 || parens<0 || brackets<0) return 0;
    }
}
static int semantic_range(Edit *e,String wanted,size_t *start,size_t *end) {
    if(wanted.length!=67) return 0;
    ProgramSet set={0}; int found=0;
    if(!check_candidate(e,e->source,e->size,&set)) { ProgramsFree(&set); return 0; }
    for(int p=0;p<set.count;p++) for(int m=0;m<set.programs[p]->module_count;m++) {
        ZirModule *module=&set.programs[p]->modules[m];
        if(strcmp(set.paths[p], e->path)) continue;
        if(!module->document || module->document->bytes!=e->size) continue;
        char id[68],hash[65]; DocumentDigest(e->source,e->size,hash);
        if(strcmp(hash,module->document->sha256)) continue;
#define MATCH(kind,name,sig,result,span) do { SemanticId(module,kind,name,sig,result,id); \
    if(!memcmp(wanted.data,id,67) && declaration_range(e,module,span,start,end)) found++; } while(0)
        for(int i=0;i<module->function_count;i++) {
            const ZirFunction *f=&module->functions[i];
            if(!f->is_public || f->is_file_private || f->is_specialization || f->is_global_initializer) continue;
            FunctionSemanticId(module,f,id);
            if(!memcmp(wanted.data,id,67) && declaration_range(e,module,f->span,start,end)) found++;
        }
        for(int i=0;i<module->type_count;i++) { const ZirType *t=&module->types[i];
            if(t->is_public && !t->is_file_private && !t->is_results && !t->is_synthetic_application && !t->is_type_instance)
                MATCH("type",t->name,t->template_params,"",t->span); }
        for(int i=0;i<module->define_count;i++) { const ZirDefine *d=&module->defines[i];
            if(d->is_public && !d->is_file_private) MATCH("constant",d->name,"","",d->span); }
        for(int i=0;i<module->global_count;i++) { const ZirGlobal *g=&module->globals[i];
            if(!g->is_static && !g->is_file_private) MATCH("global",g->name,g->type,"",g->span); }
#undef MATCH
    }
    ProgramsFree(&set); return found==1;
}
int ZiranEditAdd(void *handle,int64_t low,int64_t high,String original,String replacement,String semantic) {
    Edit *e=handle; if(!e || e->count==EDIT_COUNT || e->candidate) return fail("too many edits or invalid edit state");
    size_t start=0,end=0;
    if(semantic.length) {
        if(low!=-1 || high!=-1 || !semantic_range(e,semantic,&start,&end)) return fail("unresolved or ambiguous declaration identity");
    } else { if(low<0 || high<low || (uint64_t)high>e->size) return fail("invalid byte range"); start=(size_t)low; end=(size_t)high; }
    if(start>e->size || end>e->size || end<start || original.length!=end-start ||
       (original.length && (!original.data || memcmp(e->source+start,original.data,original.length))) ||
       (start<e->size && ((unsigned char)e->source[start]&0xc0)==0x80) ||
       (end<e->size && ((unsigned char)e->source[end]&0xc0)==0x80) ||
       !utf8(replacement.data,replacement.length) || replacement.length>EDIT_BYTES)
        return fail("preimage mismatch, invalid UTF-8 boundary, or oversized replacement");
    if(original.length==replacement.length && (!original.length || !memcmp(original.data,replacement.data,original.length))) return fail("no-op edit");
    for(int i=0;i<e->count;i++) if(start==e->parts[i].start ||
        (start<e->parts[i].end && end>e->parts[i].start)) return fail("overlapping or colliding edits");
    char *text=copy(replacement); if(!text) return fail("cannot retain replacement");
    e->parts[e->count++]=(EditPart){start,end,replacement.length,text}; return 1;
}
static int order(const void *a,const void *b) { const EditPart *x=a,*y=b; return x->start<y->start ? -1 : x->start>y->start; }
const unsigned char *ZiranEditBuild(void *handle) {
    Edit *e=handle; if(!e || !e->count || e->candidate) { fail("empty or repeated edit build"); return NULL; }
    qsort(e->parts,(size_t)e->count,sizeof(e->parts[0]),order);
    size_t size=e->size;
    for(int i=0;i<e->count;i++) { size-=e->parts[i].end-e->parts[i].start; if(e->parts[i].count>EDIT_BYTES-size) { fail("candidate exceeds byte budget"); return NULL; } size+=e->parts[i].count; }
    e->candidate=malloc(size+1); if(!e->candidate) { fail("out of memory"); return NULL; }
    size_t at=0,out=0;
    for(int i=0;i<e->count;i++) { EditPart *part=&e->parts[i]; size_t n=part->start-at;
        memcpy(e->candidate+out,e->source+at,n); out+=n;
        memcpy(e->candidate+out,part->replacement,part->count); out+=part->count; at=part->end; }
    memcpy(e->candidate+out,e->source+at,e->size-at); e->candidate[size]=0; e->candidate_size=size;
    return (const unsigned char *)e->candidate;
}
const unsigned char *ZiranEditSource(void *handle) { Edit *e=handle; return e ? (const unsigned char *)e->source : NULL; }
int64_t ZiranEditSize(void *handle) { Edit *e=handle; return e ? (int64_t)e->candidate_size : 0; }
int64_t ZiranEditSourceSize(void *handle) { Edit *e=handle; return e ? (int64_t)e->size : 0; }
/* Reject file/string imports that climb out of the document tree before any
 * checker reads them. Ordinary package imports still use the real resolver. */
static int safe_imports(const char *text) {
    ZirLexer lexer; LexerInit(&lexer,text,"<edit>"); int importing=0;
    for(;;) { ZirToken token=LexerNextCode(&lexer); if(token.kind==ZIR_TOKEN_EOF) return 1;
        if(token.kind==ZIR_TOKEN_DIRECTIVE && !strcmp(token.text,"#import")) importing=1;
        else if(importing && token.kind==ZIR_TOKEN_STRING) {
            if(token.truncated || token.text[0]=='/' || strstr(token.text,"..") || strchr(token.text,'\\')) return 0;
            importing=0;
        } else if(importing && (!strcmp(token.text,";") || !strcmp(token.text,"{"))) importing=0;
    }
}
static int still_current(Edit *e) {
    struct stat current;
    if(fstatat(e->parent,e->leaf,&current,AT_SYMLINK_NOFOLLOW) || !S_ISREG(current.st_mode) ||
       current.st_dev!=e->info.st_dev || current.st_ino!=e->info.st_ino || current.st_mode!=e->info.st_mode) return 0;
    size_t size=0; char *text=read_fd(e->fd,&size);
    int same=text && size==e->size && !memcmp(text,e->source,size); free(text); return same;
}
static int same_tokens(const char *a,const char *b) {
    ZirLexer left,right; LexerInit(&left,a,"<before>"); LexerInit(&right,b,"<after>");
    typedef struct TokenPair { ZirToken a,b; } TokenPair;
    TokenPair *pair=malloc(sizeof(*pair)); if(!pair) return 0;
    int same=0;
    for(;;) {
        pair->a=LexerNext(&left); pair->b=LexerNext(&right);
        if(pair->a.truncated || pair->b.truncated || !strcmp(pair->a.text,"#string") ||
           !strcmp(pair->b.text,"#string") || pair->a.kind!=pair->b.kind || strcmp(pair->a.text,pair->b.text)) break;
        if(pair->a.kind==ZIR_TOKEN_EOF) { same=1; break; }
    }
    free(pair); return same;
}
int ZiranEditValidate(void *handle) {
    Edit *e=handle;
    if(!e || !e->candidate || !safe_imports(e->candidate)) return fail("invalid candidate or import escape");
    if(same_tokens(e->source,e->candidate)) return fail("edit changes formatting only");
    ProgramSet set={0}; int valid=check_candidate(e,e->candidate,e->candidate_size,&set); ProgramsFree(&set); return valid;
}
int ZiranEditFinish(void *handle,String formatted,int apply) {
    Edit *e=handle; if(!e || !e->candidate || formatted.length>EDIT_BYTES || !utf8(formatted.data,formatted.length)) return fail("invalid formatted candidate");
    char *text=copy(formatted); if(!text) return fail("out of memory");
    if(!safe_imports(text)) { free(text); return fail("import escapes the permitted source tree"); }
    ProgramSet set={0}; int valid=check_candidate(e,text,formatted.length,&set); ProgramsFree(&set);
    if(!valid || !still_current(e)) { free(text); if(valid) fail("document changed during validation"); return 0; }
    if(!apply) { free(text); return 1; }
    char temporary[64]; int fd=-1;
    /* O_EXCL creates one private sibling; no predictable file is overwritten. */
    for(unsigned i=0;i<128 && fd<0;i++) {
        snprintf(temporary,sizeof(temporary),".zi-edit-%ld-%u",(long)getpid(),i);
        fd=openat(e->parent,temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    }
    if(fd<0) { free(text); return fail("cannot create replacement"); }
    size_t at=0;
    while(at<formatted.length) { ssize_t n=write(fd,text+at,formatted.length-at); if(n<=0) break; at+=(size_t)n; }
    valid=at==formatted.length && !fchmod(fd,e->info.st_mode&07777) && !fsync(fd);
    if(close(fd)) valid=0;
    if(valid) valid=still_current(e);
    if(valid) valid=renameat(e->parent,temporary,e->parent,e->leaf)==0;
    if(!valid) unlinkat(e->parent,temporary,0);
    free(text); if(!valid) return fail("cannot replace current document"); return 1;
}
