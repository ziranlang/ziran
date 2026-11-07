/* Writes every typed Go std/simd value helper beside its reference form
 * into the directory given, for tests/simd_go.sh. */
#include "zir.h"
#include "zir_emit.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if(argc != 2) {
        fputs("usage: simd_go_test DIRECTORY\n", stderr);
        return 2;
    }
    return GoSimdWriteCheck(argv[1], "main") != 0;
}
