#include <stdio.h>
static FILE *component_output;
#define main zx_component_cli
#define printf(...) fprintf(component_output, __VA_ARGS__)
#include "../zx_compare_component.c"
#undef main
#undef printf
int zx_components(FILE *out, const char *old, const char *next) {
    component_output=out;
    char *args[]={"component",(char*)old,(char*)next};
    return zx_component_cli(3,args);
}
