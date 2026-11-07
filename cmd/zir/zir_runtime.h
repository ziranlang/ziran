#ifndef ZIRAN_ZIR_RUNTIME_H
#define ZIRAN_ZIR_RUNTIME_H

/* A runtime header generated C includes, embedded in the tool. */
typedef struct ZirRuntimeHeader {
    const char *name;
    const char *text;
    unsigned includes; /* bit j: text includes ZirRuntimeHeaders[j] */
} ZirRuntimeHeader;

extern const ZirRuntimeHeader ZirRuntimeHeaders[];

struct ZirSimdUses; /* zir_simd.h */

/* Copy runtime headers and their dependencies into out_dir, so the generated
 * output builds without Ziran's include directory. With simd, the SIMD
 * operations header is left to EmitSimdOps and noted in simd->header. */
int EmitRuntimeHeaders(const char *out_dir, const char *generated_path, struct ZirSimdUses *simd);

/* Write out_dir/ziran_simd_ops.h with the helpers uses names and the ones
 * they call, in their order in the full header: the same code as the full
 * header gives the program. */
int EmitSimdOps(const char *out_dir, const struct ZirSimdUses *uses);

#endif
