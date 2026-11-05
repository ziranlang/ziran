#ifndef ZIR_EDIT_H
#define ZIR_EDIT_H
#include "zir_string_types.h"
#include <stdint.h>
int ZiranEditJsonEnabled(void);
const unsigned char *ZiranToolDigest(String text);
int ZiranToolWriteMap(String path, String text);
unsigned char *ZiranToolReadText(String path, int64_t limit);
int ZiranEditDefine(void *handle, String name);
int ZiranEditDiagnosticFormat(String value);
void *ZiranEditOpen(String root, String path, String hash);
int ZiranEditModulePath(void *handle, String path);
int ZiranEditAdd(void *handle, int64_t start, int64_t end, String original, String replacement, String semantic);
const unsigned char *ZiranEditBuild(void *handle);
const unsigned char *ZiranEditSource(void *handle);
int64_t ZiranEditSize(void *handle);
int64_t ZiranEditSourceSize(void *handle);
int ZiranEditValidate(void *handle);
int ZiranEditFinish(void *handle, String formatted, int apply);
void ZiranEditClose(void *handle);
#endif
