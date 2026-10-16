#ifndef ZIRAN_ZIR_SERIAL_H
#define ZIRAN_ZIR_SERIAL_H

#include "zir.h"

/* Versioned, pointer-free checked IR serialization. */
int ProgramWrite(const ZirProgram *program, FILE *out);
/* The portable view keeps checked bodies and omits native instruction
 * templates. Measurement allocates no payload buffer; both calls borrow an
 * immutable checked program and report the exact number of serialized bytes.
 * Measure first; write the same unchanged program after successful measurement. */
int ProgramMeasurePortable(const ZirProgram *program, size_t *size);
int ProgramWritePortable(const ZirProgram *program, FILE *out, size_t *size);
ZirProgram *ProgramRead(FILE *in, const char *path);
int PathIsIR(const char *path);
ZirProgram *ProgramLoad(const char *path, const char *root);
/* Check all inputs and reject saved IR that changes during semantic checking.
 * A NULL input_paths pointer marks every program as saved IR. */
int CheckCanonicalPrograms(ZirProgram **programs, int count,
                           const char *const *input_paths);

#endif
