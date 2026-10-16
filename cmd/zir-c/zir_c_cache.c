#include "zir_c_cache.h"
#include "zir.h"
#include "zir_profile.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* This is an optional, local object cache, not a portable artifact format.
 * Missing/unsupported inputs always fall back to the regular compiler. */
enum { MAX_ITEMS = 16384, MEMO_SLOTS = 32768, PROBE_LIMIT = 65536 };
typedef struct Hash { uint32_t h[8]; uint64_t size; unsigned char block[64]; size_t used; } Hash;
typedef struct Names { char **items; int count; } Names;
typedef struct Fingerprint {
    char *path;
    char *parent;
    unsigned char digest[32];
    struct stat info;
    int unsafe;
    int directory;
    uint64_t epoch;
    struct Fingerprint *next;
    struct Fingerprint *bucket_next;
} Fingerprint;
struct CBuildCache {
    unsigned char identity[32];
    Names roots;
    char *output, *executable;
    struct timespec started;
    Fingerprint *files, *directories, *lookups;
    Fingerprint **slots;
    uint64_t epoch;
    int fingerprints, piped;
};

static uint32_t rotate(uint32_t value, int amount) { return (value >> amount) | (value << (32 - amount)); }
static void hash_block(Hash *hash)
{
    static const uint32_t constants[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t w[64];
    for(int i = 0; i < 16; i++) w[i] = (uint32_t)hash->block[i*4] << 24 |
        (uint32_t)hash->block[i*4+1] << 16 | (uint32_t)hash->block[i*4+2] << 8 | hash->block[i*4+3];
    for(int i = 16; i < 64; i++) {
        uint32_t x = w[i-15], y = w[i-2];
        w[i] = w[i-16] + (rotate(x,7)^rotate(x,18)^(x>>3)) + w[i-7] + (rotate(y,17)^rotate(y,19)^(y>>10));
    }
    uint32_t a=hash->h[0],b=hash->h[1],c=hash->h[2],d=hash->h[3],e=hash->h[4],f=hash->h[5],g=hash->h[6],h=hash->h[7];
    for(int i = 0; i < 64; i++) {
        uint32_t t = h + (rotate(e,6)^rotate(e,11)^rotate(e,25)) + ((e&f)^(~e&g)) + constants[i] + w[i];
        uint32_t u = (rotate(a,2)^rotate(a,13)^rotate(a,22)) + ((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
    }
    hash->h[0]+=a;hash->h[1]+=b;hash->h[2]+=c;hash->h[3]+=d;
    hash->h[4]+=e;hash->h[5]+=f;hash->h[6]+=g;hash->h[7]+=h;
}
static Hash hash_new(void)
{
    return (Hash){.h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
}
static void hash_bytes(Hash *hash, const void *input, size_t size)
{
    const unsigned char *bytes = input;
    hash->size += size;
    while(size) {
        size_t amount = sizeof(hash->block) - hash->used;
        if(amount > size) amount = size;
        memcpy(hash->block + hash->used, bytes, amount);
        hash->used += amount; bytes += amount; size -= amount;
        if(hash->used == sizeof(hash->block)) { hash_block(hash); hash->used = 0; }
    }
}
static void hash_integer(Hash *hash, uint64_t value)
{
    unsigned char bytes[8];
    for(int i = 0; i < 8; i++) bytes[i] = (unsigned char)(value >> (8*i));
    hash_bytes(hash, bytes, sizeof(bytes));
}
static void hash_text(Hash *hash, const char *text) { hash_integer(hash, strlen(text)); hash_bytes(hash, text, strlen(text)); }
static void hash_finish(Hash *hash, unsigned char digest[32])
{
    uint64_t bits = hash->size * 8;
    unsigned char byte = 128, zero = 0, length[8];
    hash_bytes(hash, &byte, 1);
    while(hash->used != 56) hash_bytes(hash, &zero, 1);
    for(int i = 0; i < 8; i++) length[7-i] = (unsigned char)(bits >> (8*i));
    hash_bytes(hash, length, sizeof(length));
    for(int i = 0; i < 32; i++) digest[i] = (unsigned char)(hash->h[i/4] >> (24 - 8*(i%4)));
}

static struct timespec modified(const struct stat *info)
{
#ifdef __APPLE__
    return info->st_mtimespec;
#else
    return info->st_mtim;
#endif
}
static struct timespec changed(const struct stat *info)
{
#ifdef __APPLE__
    return info->st_ctimespec;
#else
    return info->st_ctim;
#endif
}
static int equal_time(struct timespec a, struct timespec b) { return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec; }
static int older(struct timespec a, struct timespec b) { return a.tv_sec < b.tv_sec || (a.tv_sec == b.tv_sec && a.tv_nsec < b.tv_nsec); }
static int same_stat(const struct stat *a, const struct stat *b)
{
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino && a->st_mode == b->st_mode && a->st_size == b->st_size &&
        equal_time(modified(a),modified(b)) && equal_time(changed(a),changed(b));
}
static void hash_stat(Hash *hash, const struct stat *info)
{
    hash_integer(hash,info->st_dev);hash_integer(hash,info->st_ino);hash_integer(hash,info->st_mode);hash_integer(hash,info->st_size);
    hash_integer(hash,modified(info).tv_sec);hash_integer(hash,modified(info).tv_nsec);
    hash_integer(hash,changed(info).tv_sec);hash_integer(hash,changed(info).tv_nsec);
}
static int compare_names(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static int add_name(Names *names, const char *name)
{
    for(int i = 0; i < names->count; i++) if(!strcmp(names->items[i], name)) return 1;
    if(names->count >= MAX_ITEMS) return 0;
    char **grown = realloc(names->items, (size_t)(names->count + 1) * sizeof(*grown));
    if(!grown) return 0;
    names->items = grown;
    if(!(names->items[names->count] = strdup(name))) return 0;
    names->count++;
    return 1;
}
static void free_names(Names *names) { for(int i=0;i<names->count;i++) free(names->items[i]); free(names->items); }
static int parent(const char *path, char directory[ZIR_PATH_MAX])
{
    if(strlen(path) >= ZIR_PATH_MAX) return 0;
    strcpy(directory,path);
    char *slash = strrchr(directory,'/');
    if(!slash) strcpy(directory,".");
    else if(slash == directory) slash[1]=0;
    else *slash=0;
    return 1;
}
static int suffix_path(const char *path, const char *suffix, char result[ZIR_PATH_MAX])
{
    int size = snprintf(result,ZIR_PATH_MAX,"%s%s",path,suffix);
    return size >= 0 && size < ZIR_PATH_MAX;
}

static size_t memo_slot(const char *path,int directory)
{
    uint64_t value=UINT64_C(14695981039346656037) ^ (unsigned)directory;
    for(const unsigned char *p=(const unsigned char *)path;*p;p++) { value^=*p;value*=UINT64_C(1099511628211); }
    return (size_t)value & (MEMO_SLOTS-1);
}
static Fingerprint *find_fingerprint(CBuildCache *cache,const char *path,int directory)
{
    for(Fingerprint *file=cache->slots[memo_slot(path,directory)];file;file=file->bucket_next)
        if(file->directory==directory && !strcmp(file->path,path)) return file;
    return NULL;
}
static Fingerprint *new_fingerprint(CBuildCache *cache, Fingerprint **list, const char *path)
{
    if(cache->fingerprints >= MAX_ITEMS) return NULL;
    Fingerprint *file = calloc(1,sizeof(*file));
    if(!file) return NULL;
    file->path = strdup(path);
    if(!file->path) { free(file); return NULL; }
    file->next = *list; *list=file; cache->fingerprints++;
    file->directory=list==&cache->directories ? 1 : list==&cache->lookups ? 2 : 0;
    size_t slot=memo_slot(path,file->directory);
    file->bucket_next=cache->slots[slot];cache->slots[slot]=file;
    return file;
}
static int unsafe_text(const char *text)
{
    const char *tokens[]={"__DATE__","__TIME__","__TIMESTAMP__",".incbin",".include","../"};
    for(size_t i=0;i<sizeof(tokens)/sizeof(*tokens);i++) if(strstr(text,tokens[i])) return 1;
    const char *probe=text;
    while((probe=strstr(probe,"__has_include"))) {
        probe+=13;
        if(!strncmp(probe,"_next",5)) probe+=5;
        while(isspace((unsigned char)*probe)) probe++;
        if(*probe++!='(') return 1;
        while(isspace((unsigned char)*probe)) probe++;
        int end=*probe=='"' ? '"' : *probe=='<' ? '>' : 0;
        if(!end) return 1;
        probe++;
        const char *begin=probe;
        while(*probe && *probe!=end) { if(*probe=='/' || *probe=='\n') return 1;probe++; }
        if(*probe!=end || probe==begin) return 1;
        probe++;
        while(isspace((unsigned char)*probe)) probe++;
        if(*probe++!=')') return 1;
        /* Flat literal probes are covered by the names in every include root
         * and dependency parent, even when that header is absent today. */
    }
    return 0;
}
static Fingerprint *file_digest(CBuildCache *cache, const char *path)
{
    struct stat before, after;
    if(stat(path,&before) != 0 || !S_ISREG(before.st_mode)) return NULL;
    Fingerprint *file=find_fingerprint(cache,path,0);
    if(file && same_stat(&file->info,&before)) return file;
    FILE *input = fopen(path,"rb");
    if(!input) return NULL;
    Hash hash = hash_new();
    char bytes[8192 + 33];
    size_t retained=0, length;
    int unsafe=0;
    while((length=fread(bytes+retained,1,8192,input))) {
        hash_bytes(&hash,bytes+retained,length);
        bytes[retained+length]=0;
        if(memchr(bytes,0,retained+length)) unsafe=1;
        if(unsafe_text(bytes)) unsafe=1;
        size_t total=retained+length;
        retained=total<32 ? total : 32;
        memmove(bytes,bytes+total-retained,retained);
    }
    int ok=!ferror(input) && fstat(fileno(input),&after)==0 && same_stat(&before,&after);
    fclose(input);
    if(!ok) return NULL;
    if(!file && !(file=new_fingerprint(cache,&cache->files,path))) return NULL;
    file->info=after;file->unsafe=unsafe;free(file->parent);file->parent=NULL;hash_finish(&hash,file->digest);
    return file;
}

/* Include lookup can change when a previously absent, higher-priority header
 * appears. Fingerprint the names in candidate parent directories, not their
 * mtimes or every file's bytes. Output/object writes cannot churn this key. */
static Fingerprint *directory_digest(CBuildCache *cache, const char *path)
{
    Fingerprint *saved=find_fingerprint(cache,path,1);
    if(saved) return saved->unsafe ? NULL : saved;
    Fingerprint *item=new_fingerprint(cache,&cache->directories,path);
    if(!item) return NULL;
    item->unsafe=1;
    Hash hash=hash_new();
    hash_text(&hash,path);
    if(stat(path,&item->info)!=0) {
        if(errno!=ENOENT && errno!=ENOTDIR) return NULL;
        hash_text(&hash,"absent"); hash_finish(&hash,item->digest); item->unsafe=0; return item;
    }
    if(!S_ISDIR(item->info.st_mode)) { hash_text(&hash,"not a directory"); hash_finish(&hash,item->digest); item->unsafe=0; return item; }
    hash_integer(&hash,item->info.st_dev);hash_integer(&hash,item->info.st_ino);
    struct dirent **entries=NULL;
    int count=scandir(path,&entries,NULL,alphasort),ok=count>=0;
    char *resolved=realpath(path,NULL);
    int output=resolved && !strcmp(resolved,cache->output);
    free(resolved);
    for(int i=0;i<count;i++) {
        struct dirent *entry=entries[i];
        const char *name=entry->d_name;
        if(!strcmp(name,".") || !strcmp(name,"..") || (output && (!strcmp(name,".objects") || !strcmp(name,cache->executable)))) { free(entry); continue; }
        hash_text(&hash,name);
        unsigned char type=0;
#ifdef DT_UNKNOWN
        type=entry->d_type;
#endif
        char full[ZIR_PATH_MAX];
        struct stat info;
        if(snprintf(full,sizeof(full),"%s/%s",path,name)>=(int)sizeof(full)) ok=0;
        else {
            if(!type) {
                if(lstat(full,&info)!=0) ok=0;
                else type=S_ISLNK(info.st_mode) ? 10 : S_ISDIR(info.st_mode) ? 4 : S_ISREG(info.st_mode) ? 8 : 1;
            }
            hash_integer(&hash,type);
            if(type==10) {
                char target[ZIR_PATH_MAX];
                ssize_t size=readlink(full,target,sizeof(target));
                if(size<0 || size>=(ssize_t)sizeof(target)) ok=0;
                else hash_bytes(&hash,target,(size_t)size);
            }
        }
        free(entry);
    }
    free(entries);
    if(!ok) return NULL;
    hash_finish(&hash,item->digest);
    item->unsafe=0;
    return item;
}

static char *executable_path(const char *name)
{
    if(strchr(name,'/')) return access(name,X_OK)==0 ? realpath(name,NULL) : NULL;
    char *paths=strdup(getenv("PATH") ? getenv("PATH") : "/bin:/usr/bin");
    if(!paths) return NULL;
    char *part=paths,*next,*found=NULL;
    do {
        next=strchr(part,':'); if(next) *next=0;
        char path[ZIR_PATH_MAX];
        if(snprintf(path,sizeof(path),"%s/%s",*part?part:".",name)<(int)sizeof(path) && access(path,X_OK)==0) { found=realpath(path,NULL); break; }
        part=next ? next+1 : NULL;
    } while(part);
    free(paths);return found;
}
static int native_executable(const char *path)
{
    unsigned char magic[4];
    FILE *file=fopen(path,"rb");
    if(!file) return 0;
    int ok=fread(magic,1,sizeof(magic),file)==sizeof(magic);
    fclose(file);
    return ok && (!memcmp(magic,"\177ELF",4) || !memcmp(magic,"\xcf\xfa\xed\xfe",4) ||
                  !memcmp(magic,"\xfe\xed\xfa\xcf",4) || !memcmp(magic,"\xca\xfe\xba\xbe",4));
}
static int argument_value(const char *arg)
{
    const char *paired[]={"-I","-isystem","-iquote","-idirafter","-include","-imacros","-isysroot","--sysroot","-target","--target","-x","-D","-U"};
    for(size_t i=0;i<sizeof(paired)/sizeof(*paired);i++)
        if(!strcmp(arg,paired[i])) return 1;
    return 0;
}

int c_cache_requested_pipe(char *const *args,int count)
{
    for(int i=1;i<count;i++) {
        if(argument_value(args[i])) { i++;continue; }
        if(!strcmp(args[i],"-pipe")) return 1;
    }
    return 0;
}

static int supported_arguments(char *const *args,int count)
{
    const char *effects[]={"DEPENDENCIES_OUTPUT","SUNPRO_DEPENDENCIES","GCC_DIAGNOSTICS_LOG","GCC_COMPARE_DEBUG","GCC_EXTRA_DIAGNOSTIC_OUTPUT","EXPERIMENTAL_SARIF_SOCKET"};
    for(size_t i=0;i<sizeof(effects)/sizeof(*effects);i++) if(getenv(effects[i])) return 0;
    for(int i=1;i<count;i++) if(strstr(args[i],"__DATE__") || strstr(args[i],"__TIME__") || strstr(args[i],"__TIMESTAMP__") ||
        strstr(args[i],"__has_include") || !strcmp(args[i],"-march=native") || !strcmp(args[i],"-mcpu=native") || !strcmp(args[i],"-mtune=native")) return 0;
    for(int i=1;i<count;i++) {
        const char *arg=args[i];
        if(argument_value(arg)) { if(++i==count || (!strcmp(arg,"-x") && strcmp(args[i],"c"))) return 0; continue; }
        if(!strncmp(arg,"-D",2) || !strncmp(arg,"-U",2) || !strncmp(arg,"-I",2) || !strncmp(arg,"-std=",5) ||
           !strncmp(arg,"--sysroot=",10) || !strncmp(arg,"--target=",9)) continue;
        if(!strncmp(arg,"-W",2) && strncmp(arg,"-Wa,",4) && strncmp(arg,"-Wp,",4) && strncmp(arg,"-Wl,",4)) continue;
        if(!strncmp(arg,"-g",2) && strncmp(arg,"-gsplit",7)) continue;
        if(!strcmp(arg,"-O0") || !strcmp(arg,"-O1") || !strcmp(arg,"-O2") || !strcmp(arg,"-O3") ||
           !strcmp(arg,"-Os") || !strcmp(arg,"-Oz") || !strcmp(arg,"-Og") || !strcmp(arg,"-Ofast")) continue;
        if(!strncmp(arg,"-m",2) && strcmp(arg,"-mllvm")) continue;
        const char *plain[]={"-pthread","-pipe","-pedantic","-pedantic-errors","-ansi","-nostdinc","-nostdlib","-static","-shared","-s",
            "-fPIC","-fpic","-fPIE","-fpie","-fno-plt","-fomit-frame-pointer","-fno-omit-frame-pointer","-ffast-math","-fno-fast-math",
            "-fstrict-aliasing","-fno-strict-aliasing","-fwrapv","-fno-wrapv","-fcommon","-fno-common","-fsigned-char","-funsigned-char",
            "-fexceptions","-fno-exceptions"};
        int known=0;
        for(size_t j=0;j<sizeof(plain)/sizeof(*plain);j++) known|=!strcmp(arg,plain[j]);
        if(known || !strncmp(arg,"-flto",5) || !strncmp(arg,"-fvisibility=",13)) continue;
        return 0;
    }
    return 1;
}

/* cc1 and as can report independent metadata in either order when piped.
 * Preserve each complete line, including the option order within a command;
 * include-search precedence is fingerprinted separately while parsing it. */
static int hash_probe_output(Hash *hash,const char *text)
{
    size_t count=1;
    for(const char *p=text;*p;p++) count+=*p=='\n';
    if(count>MAX_ITEMS) return 0;
    char *copy=strdup(text);
    char **lines=malloc(count*sizeof(*lines));
    if(!copy || !lines) { free(copy);free(lines);return 0; }
    size_t used=0;
    lines[used++]=copy;
    for(char *p=copy;*p;p++) if(*p=='\n') { *p=0;lines[used++]=p+1; }
    qsort(lines,used,sizeof(*lines),compare_names);
    hash_integer(hash,used);
    for(size_t i=0;i<used;i++) hash_text(hash,lines[i]);
    free(lines);free(copy);return 1;
}

static char *probe(char *const *base,int count,int piped)
{
    char **args=calloc((size_t)count+10,sizeof(*args));
    char *output=malloc(PROBE_LIMIT+1);
    int pipes[2];
    if(!args || !output || pipe(pipes)!=0) { free(args);free(output);return NULL; }
    memcpy(args,base,(size_t)count*sizeof(*args));
    /* Pipes remove the temporary assembly file and let cc1 and as overlap.
     * Keep the real compile probe's compiler/library/checksum diagnostics. */
    if(piped && !c_cache_requested_pipe(base,count)) args[count++]="-pipe";
    args[count++]="-v";args[count++]="-x";args[count++]="c";
    args[count++]="-c";args[count++]="/dev/null";
    args[count++]="-o";args[count]="/dev/null";
    fflush(NULL);
    pid_t pid=fork();
    if(pid==0) {
        close(pipes[0]);
        if(dup2(pipes[1],STDOUT_FILENO)<0 || dup2(pipes[1],STDERR_FILENO)<0) _exit(127);
        if(pipes[1]>STDERR_FILENO) close(pipes[1]);
        execvp(args[0],args);_exit(127);
    }
    close(pipes[1]);
    if(pid>0) ProfileCount(piped ? "native_probe_pipe" : "native_probe_compile");
    size_t used=0;
    int ok=pid>0;
    if(pid>0) {
        char bytes[4096];ssize_t length;
        while((length=read(pipes[0],bytes,sizeof(bytes)))!=0) {
            if(length<0) { if(errno==EINTR) continue;ok=0;break; }
            if((size_t)length>PROBE_LIMIT-used) { ok=0;continue; }
            memcpy(output+used,bytes,(size_t)length);used+=(size_t)length;
        }
    }
    close(pipes[0]);
    int status=0;pid_t waited;
    do { waited=pid>0 ? waitpid(pid,&status,0) : -1; } while(waited<0 && errno==EINTR);
    if(waited<=0 || !WIFEXITED(status) || WEXITSTATUS(status)!=0) ok=0;
    free(args);
    if(memchr(output,0,used)) ok=0;
    output[used]=0;
    if(!ok) { free(output);return NULL; }
    return output;
}

CBuildCache *c_cache_open(char *const *args,int count,const char *output,const char *executable)
{
    if(!count || !supported_arguments(args,count)) return NULL;
    char *compiler=executable_path(args[0]);
    if(!compiler) return NULL;
    const char *name=strrchr(compiler,'/');name=name?name+1:compiler;
    if((!strstr(name,"gcc") && !strstr(name,"clang") && strcmp(name,"cc")) || !native_executable(compiler)) { free(compiler);return NULL; }
    /* A native cc alias may be GCC even when its file is not named gcc. */
    int piped=strstr(name,"clang")==NULL;
    CBuildCache *cache=calloc(1,sizeof(*cache));
    if(!cache) { free(compiler);return NULL; }
    cache->piped=piped;
    cache->slots=calloc(MEMO_SLOTS,sizeof(*cache->slots));
    cache->output=realpath(output,NULL);cache->executable=strdup(executable);
    if(!cache->slots || !cache->output || !cache->executable || clock_gettime(CLOCK_REALTIME,&cache->started)!=0) goto fail;
    Hash hash=hash_new();
    hash_text(&hash,"Ziran native object cache 2");
    hash_integer(&hash,(uint64_t)piped);
    char *cwd=getcwd(NULL,0);
    if(!cwd) goto fail;
    hash_text(&hash,cwd);free(cwd);
    for(int i=0;i<count;i++) hash_text(&hash,args[i]);
    extern char **environ;
    int environments=0;
    while(environ[environments]) environments++;
    char **sorted=malloc((size_t)(environments+1)*sizeof(*sorted));
    if(!sorted) goto fail;
    memcpy(sorted,environ,(size_t)environments*sizeof(*sorted));
    qsort(sorted,(size_t)environments,sizeof(*sorted),compare_names);
    for(int i=0;i<environments;i++) hash_text(&hash,sorted[i]);
    free(sorted);
    struct stat info;
    if(stat(compiler,&info)!=0) goto fail;
    hash_text(&hash,compiler);hash_stat(&hash,&info);free(compiler);compiler=NULL;
    char *text=probe(args,count,piped);
    if(!text) goto fail;
    /* A fixed piped probe has no random assembly pathname. Include all its
     * derived options in the identity, including installed specs changes.
     * Interleaved diagnostics can cause a conservative miss, never a hit. */
    if(piped && !hash_probe_output(&hash,text)) { free(text);goto fail; }
    int required_tools=piped && !(strstr(text,"clang version") &&
        strstr(text,"-emit-obj")) ? 2 : 1;
    int searching=0,ended=0,tools=0,ok=1;
    char *line=text;
    while(line && ok) {
        char *next=strchr(line,'\n');if(next) *next++=0;
        if(!strncmp(line,"COLLECT_GCC=",12)) {
            char *driver=executable_path(line+12);
            if(!driver || stat(driver,&info)!=0) ok=0;
            else { hash_text(&hash,driver);hash_stat(&hash,&info); }
            free(driver);
        } else if(!strncmp(line,"#include ",9)) searching=1;
        else if(!strcmp(line,"End of search list.")) { searching=0;ended=1; }
        else if(searching && *line==' ') {
            char *path=line+1;
            char *framework=strstr(path," (framework directory)");if(framework) *framework=0;
            struct stat directory;
            if(piped && (stat(path,&directory)!=0 || !S_ISDIR(directory.st_mode))) ok=0;
            if(ok) { ok=add_name(&cache->roots,path);hash_text(&hash,path); }
        } else if(searching && piped) {
            /* Concurrent verbose output must not hide a search root. */
            ok=0;
        } else if(!strncmp(line,"ignoring nonexistent directory \"",32)) {
            char *path=strchr(line,'"')+1,*end=strrchr(path,'"');
            if(end) { *end=0;ok=add_name(&cache->roots,path);hash_text(&hash,path); }
        } else if(*line==' ' && line[1] && !isspace((unsigned char)line[1])) {
            char *token=line+1,*end;
            if(*token=='"') { token++;end=strchr(token,'"'); }
            else { end=token;while(*end && !isspace((unsigned char)*end)) end++; }
            if(end) {
                char saved=*end;*end=0;
                char *tool=executable_path(token);
                if(tool) {
                    if(!native_executable(tool) || stat(tool,&info)!=0) ok=0;
                    else { hash_text(&hash,tool);hash_stat(&hash,&info);tools++; }
                    free(tool);
                }
                *end=saved;
            }
        } else if(!piped && (!strncmp(line,"Compiler executable checksum:",29) || strstr(line," version "))) hash_text(&hash,line);
        line=next;
    }
    free(text);
    if(!ok || !ended || tools<required_tools || !add_name(&cache->roots,cache->output)) goto fail;
    for(int i=0;i<cache->roots.count;i++) {
        char *resolved=realpath(cache->roots.items[i],NULL);
        if(resolved) { free(cache->roots.items[i]);cache->roots.items[i]=resolved; }
    }
    hash_finish(&hash,cache->identity);
    return cache;
fail:
    free(compiler);c_cache_close(cache);return NULL;
}

int c_cache_piped(const CBuildCache *cache)
{
    return cache != NULL && cache->piped;
}

static int read_integer(FILE *file,uint32_t *value)
{
    unsigned char bytes[4];
    if(fread(bytes,1,4,file)!=4) return 0;
    *value=(uint32_t)bytes[0] | (uint32_t)bytes[1]<<8 | (uint32_t)bytes[2]<<16 | (uint32_t)bytes[3]<<24;
    return 1;
}
static int write_integer(FILE *file,uint32_t value)
{
    unsigned char bytes[4];
    for(int i=0;i<4;i++) bytes[i]=(unsigned char)(value>>(8*i));
    return fwrite(bytes,1,4,file)==4;
}
static int lookup_digest(CBuildCache *cache,Names *dependencies,unsigned char digest[32],int recording)
{
    Names roots={0},parents={0};Hash hash=hash_new();int ok=1;
    for(int i=0;i<cache->roots.count && ok;i++) ok=add_name(&roots,cache->roots.items[i]);
    for(int i=0;i<dependencies->count && ok;i++) {
        Fingerprint *input=find_fingerprint(cache,dependencies->items[i],0);
        if(input && !input->parent) {
            char *resolved=realpath(dependencies->items[i],NULL),directory[ZIR_PATH_MAX];
            if(resolved && !strstr(resolved,"/.objects/") && parent(resolved,directory)) input->parent=strdup(directory);
            free(resolved);
        }
        ok=input && input->parent && add_name(&roots,input->parent) && add_name(&parents,input->parent);
    }
    if(roots.count>1) qsort(roots.items,(size_t)roots.count,sizeof(*roots.items),compare_names);
    if(parents.count>1) qsort(parents.items,(size_t)parents.count,sizeof(*parents.items),compare_names);
    Hash key_hash=hash_new();unsigned char key_bytes[32];char key[65];
    for(int i=0;i<parents.count;i++) hash_text(&key_hash,parents.items[i]);
    hash_finish(&key_hash,key_bytes);
    for(int i=0;i<32;i++) snprintf(key+i*2,3,"%02x",key_bytes[i]);
    Fingerprint *saved=find_fingerprint(cache,key,2);
    if(ok && saved) {
        memcpy(digest,saved->digest,32);free_names(&roots);free_names(&parents);
        return !recording || !saved->unsafe;
    }
    int recordable=1;
    uint64_t epoch=++cache->epoch;
    for(int i=0;i<parents.count && ok;i++) {
        const char *absolute=parents.items[i];
        for(int r=0;r<roots.count && ok;r++) {
            /* Every suffix can be the spelling in an include directive. A
             * parent may be absent today and gain a shadowing header later. */
            const char *relative=absolute+1;
            while(relative && ok) {
                char candidate[ZIR_PATH_MAX];
                if(snprintf(candidate,sizeof(candidate),"%s/%s",roots.items[r],relative)>=(int)sizeof(candidate)) { ok=0;break; }
                Fingerprint *directory=directory_digest(cache,candidate);
                if(!directory) ok=0;
                else {
                    if(directory->info.st_ino && !older(changed(&directory->info),cache->started)) recordable=0;
                    if(directory->epoch!=epoch) { hash_bytes(&hash,directory->digest,32);directory->epoch=epoch; }
                }
                relative=strchr(relative,'/');if(relative) relative++;
            }
            Fingerprint *directory=ok ? directory_digest(cache,roots.items[r]) : NULL;
            if(!directory) ok=0;
            else {
                if(directory->info.st_ino && !older(changed(&directory->info),cache->started)) recordable=0;
                if(directory->epoch!=epoch) { hash_bytes(&hash,directory->digest,32);directory->epoch=epoch; }
            }
        }
    }
    free_names(&roots);free_names(&parents);
    if(ok) {
        hash_finish(&hash,digest);
        Fingerprint *item=new_fingerprint(cache,&cache->lookups,key);
        if(item) { memcpy(item->digest,digest,32);item->unsafe=!recordable; }
    }
    return ok && (!recording || recordable);
}

int c_cache_hit(CBuildCache *cache,const char *object)
{
    if(!cache) return 0;
    char path[ZIR_PATH_MAX];
    if(!suffix_path(object,".inputs",path)) return 0;
    FILE *file=fopen(path,"rb");if(!file) return 0;
    unsigned char magic[16],identity[32],object_hash[32],lookups[32],current[32];
    uint32_t count;
    int ok=fread(magic,1,16,file)==16 && !memcmp(magic,"Ziran C cache 1\n",16) &&
        fread(identity,1,32,file)==32 && !memcmp(identity,cache->identity,32) &&
        fread(object_hash,1,32,file)==32 && fread(lookups,1,32,file)==32 && read_integer(file,&count) && count && count<=MAX_ITEMS;
    Fingerprint *compiled=ok ? file_digest(cache,object) : NULL;
    if(!compiled || memcmp(compiled->digest,object_hash,32)) ok=0;
    Names dependencies={0};
    for(uint32_t i=0;ok && i<count;i++) {
        uint32_t length;char dependency[ZIR_PATH_MAX];unsigned char saved[32];
        if(!read_integer(file,&length) || !length || length>=sizeof(dependency) ||
           fread(dependency,1,length,file)!=length || memchr(dependency,0,length) || fread(saved,1,32,file)!=32) { ok=0;break; }
        dependency[length]=0;
        Fingerprint *input=file_digest(cache,dependency);
        ok=input && !input->unsafe && !memcmp(input->digest,saved,32) && add_name(&dependencies,dependency);
    }
    if(ok) ok=fgetc(file)==EOF && !ferror(file) && lookup_digest(cache,&dependencies,current,0) && !memcmp(current,lookups,32);
    fclose(file);free_names(&dependencies);return ok;
}

static int dependencies_read(const char *object,Names *names)
{
    char path[ZIR_PATH_MAX];
    if(!suffix_path(object,".d",path)) return 0;
    FILE *file=fopen(path,"rb");if(!file) return 0;
    int byte,ok=1;
    while((byte=fgetc(file))!=EOF && byte!=':') {}
    if(byte==EOF) ok=0;
    char word[ZIR_PATH_MAX];size_t used=0;
    while(ok && (byte=fgetc(file))!=EOF) {
        if(byte=='\\') {
            int next=fgetc(file);
            if(next=='\n') continue;
            if(next==EOF) { ok=0;break; }
            if(next==' ' || next=='\t' || next=='#' || next=='\\') byte=next;
            else { ungetc(next,file); }
        } else if(byte=='$') {
            int next=fgetc(file);
            if(next!='$') { ok=0;break; }
        } else if(isspace((unsigned char)byte)) {
            if(used) { word[used]=0;ok=add_name(names,word);used=0; }
            if(byte=='\n') break;
            continue;
        }
        if(used>=sizeof(word)-1) { ok=0;break; }
        word[used++]=(char)byte;
    }
    if(used && ok) { word[used]=0;ok=add_name(names,word); }
    if(ferror(file)) ok=0;
    fclose(file);return ok && names->count;
}

void c_cache_record(CBuildCache *cache,const char *object,const char *source)
{
    if(!cache) return;
    Names dependencies={0};char stamp[ZIR_PATH_MAX],temporary[ZIR_PATH_MAX]={0};
    FILE *output=NULL;int created=0;
    if(!suffix_path(object,".inputs",stamp) || !suffix_path(object,".inputs.XXXXXX",temporary) ||
       !dependencies_read(object,&dependencies)) goto done;
    int source_present=0;
    for(int i=0;i<dependencies.count;i++) {
        source_present|=!strcmp(dependencies.items[i],source);
        Fingerprint *input=file_digest(cache,dependencies.items[i]);
        if(!input || input->unsafe || !older(modified(&input->info),cache->started) || !older(changed(&input->info),cache->started)) goto done;
    }
    unsigned char lookup[32];
    if(!source_present || !lookup_digest(cache,&dependencies,lookup,1)) goto done;
    Fingerprint *compiled=file_digest(cache,object);
    if(!compiled) goto done;
    int fd=mkstemp(temporary);
    if(fd<0) goto done;
    created=1;
    output=fdopen(fd,"wb");if(!output) { close(fd);goto done; }
    int ok=fwrite("Ziran C cache 1\n",1,16,output)==16 && fwrite(cache->identity,1,32,output)==32 &&
        fwrite(compiled->digest,1,32,output)==32 && fwrite(lookup,1,32,output)==32 && write_integer(output,(uint32_t)dependencies.count);
    for(int i=0;i<dependencies.count && ok;i++) {
        const char *path=dependencies.items[i];size_t length=strlen(path);
        Fingerprint *input=file_digest(cache,path);
        ok=input && !input->unsafe && older(modified(&input->info),cache->started) && older(changed(&input->info),cache->started) &&
            write_integer(output,(uint32_t)length) && fwrite(path,1,length,output)==length && fwrite(input->digest,1,32,output)==32;
    }
    if(fclose(output)!=0) ok=0;
    output=NULL;
    if(ok && rename(temporary,stamp)==0) temporary[0]=0;
done:
    if(output) fclose(output);
    if(created && temporary[0]) unlink(temporary);
    free_names(&dependencies);
}
void c_cache_close(CBuildCache *cache)
{
    if(!cache) return;
    Fingerprint *lists[]={cache->files,cache->directories,cache->lookups};
    for(size_t i=0;i<sizeof(lists)/sizeof(*lists);i++) {
        Fingerprint *item=lists[i];
        while(item) { Fingerprint *next=item->next;free(item->path);free(item->parent);free(item);item=next; }
    }
    free_names(&cache->roots);free(cache->slots);free(cache->output);free(cache->executable);free(cache);
}
