#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int zx_compare_run(const char*,FILE*,int64_t,int,int,int);
void zx_progress(int trial,int kind,int variant) {(void)trial;(void)kind;(void)variant;}
int main(int argc,char **argv) {
    if(argc!=7) return 2;
    FILE *out=fopen(argv[2],"w");if(!out) return 3;
    int ret=zx_compare_run(argv[1],out,atoll(argv[3]),atoi(argv[4]),atoi(argv[5]),atoi(argv[6]));
    fclose(out);return ret;
}
