#ifndef ZIR_GO_AMD64_H
#define ZIR_GO_AMD64_H

#include "zir_asm.h"

typedef struct GoAmd64Plan GoAmd64Plan;
GoAmd64Plan *GoAmd64Prepare(const AssemblyVariant *variant, int block,
                            int by_value, const unsigned char *storage);
int GoAmd64Supported(const GoAmd64Plan *plan);
uint64_t GoAmd64Features(const GoAmd64Plan *plan);
void GoAmd64Write(FILE *declarations, FILE *instructions,
                   const GoAmd64Plan *plan, const char *name);
void GoAmd64Cpu(FILE *declarations, FILE *instructions, const char *prefix);
void GoAmd64Close(GoAmd64Plan *plan);

#endif
