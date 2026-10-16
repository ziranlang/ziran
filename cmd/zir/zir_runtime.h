#ifndef ZIRAN_ZIR_RUNTIME_H
#define ZIRAN_ZIR_RUNTIME_H

/* A runtime header generated C and C++ include, embedded in the tool. */
typedef struct ZirRuntimeHeader {
    const char *name;
    const char *text;
} ZirRuntimeHeader;

extern const ZirRuntimeHeader ZirRuntimeHeaders[];

/* Copy runtime headers and their dependencies into out_dir, so the generated
 * output builds without Ziran's include directory. */
int EmitRuntimeHeaders(const char *out_dir, const char *generated_path);

#endif
