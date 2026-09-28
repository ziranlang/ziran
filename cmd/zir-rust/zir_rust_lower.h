#ifndef ZIR_RUST_LOWER_H
#define ZIR_RUST_LOWER_H

#include "zir.h"

int rust_lower(const ZirProgram *const *programs, int program_count,
               const char *output_directory, const char *entry_module,
               const char *entry_function, int executable);

#endif
