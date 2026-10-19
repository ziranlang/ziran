#include "../cmd/zir-c/zir_c_cache.c"
#include <assert.h>

static void digest(const char *text, unsigned char result[32])
{
    Hash hash = hash_new();
    assert(hash_probe_output(&hash, text));
    hash_finish(&hash, result);
}

static void same(const char *left, const char *right, int equal)
{
    unsigned char a[32], b[32];
    digest(left, a); digest(right, b);
    assert((memcmp(a, b, sizeof(a)) == 0) == equal);
}

#define COMPILER "\tcompiled by GNU C version 99.0.0, "
#define LIBRARIES "GMP version 6.3.0, MPFR version 4.2.1, MPC version 1.3.1, isl version 0.27"
#define ASSEMBLER "GNU assembler version 2.44 (target) using BFD version 2.44"
#define OPTIONS " /tools/cc1 -DVALUE=1 -DVALUE=2 -o - |\n /tools/as -o /dev/null\n"

int main(void)
{
    const char *ordinary = OPTIONS COMPILER LIBRARIES "\n" ASSEMBLER "\n";
    const char *interleaved = OPTIONS COMPILER ASSEMBLER "\n" LIBRARIES "\n";
    same(ordinary, interleaved, 1);
    same(ordinary, ASSEMBLER "\n" OPTIONS COMPILER LIBRARIES "\n", 1);
    same(COMPILER LIBRARIES "\n" ASSEMBLER,
         COMPILER ASSEMBLER "\n" LIBRARIES, 1);
    same(COMPILER LIBRARIES "\r\n" ASSEMBLER "\r\n",
         COMPILER ASSEMBLER "\r\n" LIBRARIES "\r\n", 1);
    same(ordinary, OPTIONS "\tcompiled by GNU C version 99.0.1, " LIBRARIES "\n" ASSEMBLER "\n", 0);
    same(ordinary, OPTIONS COMPILER "GMP version 7.0.0, MPFR version 4.2.1, MPC version 1.3.1, isl version 0.27\n" ASSEMBLER "\n", 0);
    same(ordinary, OPTIONS COMPILER LIBRARIES "\nGNU assembler version 2.45 (target) using BFD version 2.44\n", 0);
    same(ordinary, OPTIONS COMPILER LIBRARIES "\nGNU assembler version 2.44 (target) using BFD version 2.45\n", 0);
    same(ordinary, OPTIONS "\tcompiled by GNU C version 99.0.1, " ASSEMBLER "\n" LIBRARIES "\n", 0);
    same(ordinary, OPTIONS COMPILER ASSEMBLER "\nGMP version 7.0.0, MPFR version 4.2.1, MPC version 1.3.1, isl version 0.27\n", 0);
    same(ordinary, OPTIONS COMPILER "GNU assembler version 2.45 (target) using BFD version 2.44\n" LIBRARIES "\n", 0);
    same(ordinary, OPTIONS COMPILER "GNU assembler version 2.44 (target) using BFD version 2.45\n" LIBRARIES "\n", 0);
    same(ordinary, " /tools/cc1 -DVALUE=2 -DVALUE=1 -o - |\n /tools/as -o /dev/null\n" COMPILER LIBRARIES "\n" ASSEMBLER "\n", 0);
    same(ordinary, OPTIONS COMPILER LIBRARIES "\n" ASSEMBLER "\nextra metadata\n", 0);
    same(ordinary, OPTIONS COMPILER LIBRARIES "\n" ASSEMBLER "\n" ASSEMBLER "\n", 0);
    same(ordinary, OPTIONS COMPILER ASSEMBLER "\nUnknown library metadata\n", 0);
    same(ordinary, OPTIONS "unrecognized prefix, " ASSEMBLER "\n" LIBRARIES "\n", 0);
    same("Other compiler version, " LIBRARIES "\n" ASSEMBLER "\n",
         "Other compiler version, " ASSEMBLER "\n" LIBRARIES "\n", 0);
    same(COMPILER "Other library metadata\n" ASSEMBLER "\n",
         COMPILER ASSEMBLER "\nOther library metadata\n", 0);
    same(COMPILER LIBRARIES "\nGNU assembler version 2.44 (target)\n",
         COMPILER "GNU assembler version 2.44 (target)\n" LIBRARIES "\n", 0);
    same(" cc1 -D'TEXT=" COMPILER ASSEMBLER "'\n" LIBRARIES "\n",
         " cc1 -D'TEXT=" COMPILER LIBRARIES "'\n" ASSEMBLER "\n", 0);
    same(COMPILER ASSEMBLER "\n" LIBRARIES "\n" COMPILER ASSEMBLER "\n" LIBRARIES "\n",
         COMPILER LIBRARIES "\n" ASSEMBLER "\n" COMPILER LIBRARIES "\n" ASSEMBLER "\n", 1);
    /* cc1 writes its checksum one byte at a time, then its newline. */
#define CHECKSUM "Compiler executable checksum: "
#define DIGITS "0051dfc1fa4e89ba97760da93f2546f3"
    const char *checked = OPTIONS CHECKSUM DIGITS "\n" ASSEMBLER "\n";
    same(checked, OPTIONS CHECKSUM DIGITS ASSEMBLER "\n\n", 1);
    same(checked, OPTIONS CHECKSUM "0051df" ASSEMBLER "\nc1fa4e89ba97760da93f2546f3\n", 1);
    same(checked, OPTIONS CHECKSUM ASSEMBLER "\n" DIGITS "\n", 1);
    same(CHECKSUM DIGITS "\r\n" ASSEMBLER "\r\n", CHECKSUM DIGITS ASSEMBLER "\r\n\r\n", 1);
    same(checked, OPTIONS CHECKSUM "0051df" ASSEMBLER "\nc1fa4e89ba97760da93f2546f4\n", 0);
    same(checked, OPTIONS CHECKSUM "0051df" ASSEMBLER "\nc1fa4e89ba97760da93f2546f\n", 0);
    same(checked, OPTIONS CHECKSUM DIGITS ASSEMBLER "\nextra metadata\n", 0);
    same(checked, OPTIONS CHECKSUM DIGITS "GNU assembler version 2.44 (target)\n\n", 0);
    same(checked, OPTIONS CHECKSUM DIGITS "GNU assembler version 2.45 (target) using BFD version 2.44\n\n", 0);
    same(checked, OPTIONS "Other checksum: " DIGITS ASSEMBLER "\n\n", 0);
    char *changed = strdup(interleaved);
    assert(changed);
    for(size_t i = 0; changed[i]; i++) {
        char saved = changed[i];
        if(saved == '\n') continue;
        changed[i] = saved == 'X' ? 'Y' : 'X';
        same(ordinary, changed, 0);
        changed[i] = saved;
    }
    free(changed);
    changed = strdup(OPTIONS CHECKSUM "0051df" ASSEMBLER "\nc1fa4e89ba97760da93f2546f3\n");
    assert(changed);
    for(size_t i = 0; changed[i]; i++) {
        char saved = changed[i];
        if(saved == '\n') continue;
        changed[i] = saved == 'X' ? 'Y' : 'X';
        same(checked, changed, 0);
        changed[i] = saved;
    }
    free(changed);
    puts("Probe fingerprints retain tool, library and ordered-option changes across recognized version interleaving");
    return 0;
}
