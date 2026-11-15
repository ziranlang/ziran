#ifndef ZIR_C_PRUNE_H
#define ZIR_C_PRUNE_H

#include <stdio.h>

struct ZirSimdUses;

/* An entry-linked program with its own C main is closed: every file that
 * includes one of its module headers is another of its generated files. The
 * C emitter writes each header declaration of such a program as a marked
 * chunk, and PruneProgram keeps in a header only the chunks another file
 * names. The module's own source takes the declarations only it needs,
 * inline copies that no other module calls are dropped, and a header that
 * keeps nothing is not written at all. */

/* A mark is one line: PRUNE_MARK, the chunk kind, then the C names the
 * chunk declares, separated by spaces. */
#define PRUNE_MARK '\001'
enum {
    PRUNE_FRAME = 'F',       /* linkage text every written header keeps */
    PRUNE_MODULES = 'U',     /* where the header's module includes go */
    PRUNE_DECLARATION = 'D', /* kept when named elsewhere, else moved */
    PRUNE_ROOT = 'R',        /* named by code outside the program */
    PRUNE_HELPER = 'H',      /* a runtime include or macro the text uses */
    PRUNE_INLINE = 'I',      /* an inline copy's declaration */
    PRUNE_INLINE_BODY = 'J', /* an inline copy's body */
    PRUNE_NAME = 'N',        /* one more name of the current chunk */
    PRUNE_END = 'E'          /* the header's closing text */
};

/* Where a module source takes its inline-source define, and its includes
 * followed by the declarations moved out of its header. */
#define PRUNE_SOURCE_MARK "\001S\n"
#define PRUNE_INCLUDES_MARK "\001U\n"

typedef struct ZirCPruneModule {
    char *stem, *guard;     /* owned */
    int vector_operations;  /* the inline section includes zir_vec.h */
    char *header;           /* marked header text, owned */
    char *source;           /* marked source text, owned; NULL without one */
    /* The modules, by index, whose headers this module's header and source
     * could see before pruning, in include order. */
    int *header_scope, header_scope_count;
    int *source_scope, source_scope_count;
    /* Results, owned: the header is NULL when nothing in it is kept. */
    char *final_header;
    char *final_source;
} ZirCPruneModule;

/* Writes a chunk mark of KIND declaring NAMES, which may be NULL. */
void PruneMark(FILE *out, int kind, const char *names);

/* Writes a header's inline section around DECLARATIONS and BODIES. */
int HeaderInlineSection(FILE *out, const char *stem, const char *guard,
                        int vector_operations, const char *declarations,
                        const char *bodies, struct ZirSimdUses *simd);

/* Fills each module's final header and source. Returns 0 on failure. */
int PruneProgram(ZirCPruneModule *modules, int count, struct ZirSimdUses *simd);

void PruneModuleFree(ZirCPruneModule *module);

#endif
