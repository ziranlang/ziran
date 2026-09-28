#ifndef ZIR_PY_RUNTIME_H
#define ZIR_PY_RUNTIME_H

/* One top-level definition of the Python support code. imports lists the
 * standard modules it needs, separated by spaces. */
typedef struct PyRuntimeItem {
    const char *name;
    const char *imports;
    const char *text;
} PyRuntimeItem;

/* Ends with an item whose name is NULL. */
extern const PyRuntimeItem py_runtime_items[];

#endif
