#ifndef ZIR_PY_LOWER_H
#define ZIR_PY_LOWER_H

#include "zir.h"

/*
 * py_lower - checked ZIR -> Python backend.
 *
 * Writes one Python 3.10 source file into out_dir: __main__.py for an
 * executable, which `python3 out_dir` runs, or __init__.py for a package
 * the embedding program imports. Unsupported checked constructs fail with
 * a source diagnostic.
 */
int py_lower(const ZirProgram *const *programs, int program_count,
             const char *output_directory, const char *entry_module,
             const char *entry_function, int executable);

/* Shared libraries (LDLIBS -lNAME) the program loads before resolving its C
 * foreign functions, as a C build links them. The names must outlive
 * py_lower. */
void py_set_linked_libraries(const char *const *names, int count);

#endif
