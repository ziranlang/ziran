#include "zir.h"
#include "zir_serial.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Mutate a saved certificate through the actual serializer. A reader must
 * check the graph and steps again instead of accepting cached evidence. */
int main(int argc, char **argv)
{
    assert(argc == 4);
    FILE *in = fopen(argv[1], "rb"); assert(in);
    ZirProgram *p = ProgramRead(in, argv[1]); fclose(in); assert(p);
    ZirModule *m = &p->modules[0];
    assert(m->law_count && m->proof_count);
    ZirLaw *law = &m->laws[0];
    assert(law->claim);
    ZirExpr *goal = &law->claim->exprs[law->claim->stmts[0].expr_root];
    if(!strcmp(argv[3], "goal")) strcpy(goal->op, "!=");
    else if(!strcmp(argv[3], "cycle")) goal->left = law->claim->stmts[0].expr_root;
    else if(!strcmp(argv[3], "index")) goal->right = law->claim->expr_count + 1;
    else if(!strcmp(argv[3], "step")) m->proofs[0].steps[0].kind = 999;
    else if(!strcmp(argv[3], "evidence")) {
        strcpy(law->evidence.method, "forged"); law->evidence.cases_checked = 123;
    } else assert(0);
    FILE *out = fopen(argv[2], "wb"); assert(out);
    assert(ProgramWrite(p, out)); fclose(out); ProgramFree(p);
    return 0;
}
