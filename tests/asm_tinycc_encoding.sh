#!/bin/sh
# TinyCC's x86-64 assembler, which `ziran run` uses for #asm variants,
# must encode every SSE instruction it accepts exactly as GNU as does.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:?pass the ziran command}
build=$(CDPATH= cd -- "$(dirname -- "$ziran")/.." && pwd)
case $(uname -m) in x86_64) ;; *) echo "asm-tinycc-encoding: not an x86-64 host; skipped"; exit 0 ;; esac
test -f "$build/obj/tinycc.o" || { echo "asm-tinycc-encoding: $build/obj/tinycc.o is missing" >&2; exit 1; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/instructions.s" <<'ASM'
movdqa (%rsi), %xmm0
movdqa %xmm9, 16(%r8)
movdqa %xmm15, %xmm8
movdqu 32(%rsi,%rcx,2), %xmm3
movdqu %xmm12, (%rdi)
movaps %xmm1, %xmm10
movups (%rax), %xmm2
movapd %xmm3, %xmm4
movupd %xmm5, (%rdx)
movntdq %xmm6, (%rdi)
movq %xmm9, %rax
movq %rcx, %xmm13
movd %xmm11, %r9d
movd (%rsi), %xmm14
punpcklqdq %xmm1, %xmm2
punpckhqdq (%r9), %xmm10
punpcklwd %xmm8, %xmm1
punpckhwd %xmm2, %xmm15
pslldq $4, %xmm9
psrldq $8, %xmm2
psraw $3, %xmm12
psrad $12, %xmm1
pslld $1, %xmm13
psrlq $32, %xmm0
pshufd $0x4e, %xmm8, %xmm9
pshuflw $0xb1, (%rsi), %xmm1
pshufhw $0x1b, %xmm2, %xmm11
pmulhuw %xmm1, %xmm2
pmulhw %xmm10, %xmm3
pmullw (%rcx), %xmm12
pmaddwd %xmm14, %xmm15
pmovmskb %xmm9, %eax
pmovmskb %xmm1, %r10d
psadbw %xmm2, %xmm3
paddq %xmm4, %xmm11
psubq %xmm12, %xmm5
paddsw %xmm8, %xmm9
psubsw (%rsi), %xmm9
packssdw %xmm10, %xmm11
packuswb %xmm1, %xmm2
pavgb %xmm1, %xmm9
pavgw (%rdi), %xmm2
pmaxsw %xmm3, %xmm4
pminsw %xmm12, %xmm13
pcmpeqw %xmm1, %xmm1
pcmpgtd %xmm2, %xmm9
pxor %xmm15, %xmm15
pand %xmm1, %xmm8
pinsrw $3, %eax, %xmm9
pinsrw $1, (%rsi), %xmm1
pextrw $5, %xmm10, %ecx
pextrw $2, %xmm1, (%rdi)
cvtdq2ps %xmm1, %xmm2
cvtps2dq %xmm9, %xmm3
cvttps2dq (%rax), %xmm12
shufps $0x88, %xmm1, %xmm2
unpcklps %xmm9, %xmm10
unpckhps %xmm1, %xmm2
movlhps %xmm1, %xmm9
movhlps %xmm10, %xmm2
addpd %xmm1, %xmm2
subpd %xmm9, %xmm12
mulpd (%rsi), %xmm3
pshufb %xmm1, %xmm2
pshufb (%r10), %xmm11
phaddw %xmm1, %xmm2
phaddd %xmm9, %xmm3
phaddsw %xmm1, %xmm2
pmaddubsw %xmm12, %xmm13
phsubw %xmm1, %xmm2
phsubd %xmm1, %xmm2
phsubsw %xmm1, %xmm2
psignb %xmm1, %xmm2
psignw %xmm1, %xmm2
psignd %xmm1, %xmm2
pmulhrsw %xmm8, %xmm9
pmulhrsw 16(%rsi,%rax,8), %xmm1
pabsb %xmm1, %xmm2
pabsw %xmm10, %xmm11
pabsd (%rdi), %xmm2
palignr $8, %xmm1, %xmm2
palignr $4, %xmm14, %xmm15
pblendvb %xmm1, %xmm2
blendvps %xmm9, %xmm3
blendvpd %xmm1, %xmm2
ptest %xmm1, %xmm2
pmovsxbw (%rsi), %xmm1
pmovsxbd %xmm2, %xmm3
pmovsxbq %xmm2, %xmm3
pmovsxwd %xmm9, %xmm10
pmovsxwq %xmm2, %xmm3
pmovsxdq %xmm2, %xmm3
pmuldq %xmm2, %xmm3
pcmpeqq %xmm2, %xmm3
movntdqa (%rsi), %xmm1
packusdw %xmm2, %xmm11
pmovzxbw %xmm2, %xmm3
pmovzxbd %xmm2, %xmm3
pmovzxbq %xmm2, %xmm3
pmovzxwd (%r8), %xmm3
pmovzxwq %xmm2, %xmm3
pmovzxdq %xmm2, %xmm3
pminsb %xmm2, %xmm3
pminsd %xmm12, %xmm3
pminuw %xmm2, %xmm3
pminud %xmm2, %xmm3
pmaxsb %xmm2, %xmm3
pmaxsd %xmm2, %xmm13
pmaxuw %xmm2, %xmm3
pmaxud %xmm2, %xmm3
pmulld %xmm9, %xmm10
phminposuw %xmm2, %xmm3
roundps $1, %xmm2, %xmm3
roundpd $2, %xmm2, %xmm3
roundss $3, %xmm2, %xmm3
roundsd $4, (%rsi), %xmm3
blendps $5, %xmm2, %xmm3
blendpd $1, %xmm2, %xmm3
pblendw $0xaa, %xmm9, %xmm3
pextrb $3, %xmm2, %eax
pextrb $1, %xmm10, (%rdi)
pextrd $2, %xmm11, %r8d
pextrq $1, %xmm2, %rax
pextrq $0, %xmm12, %r11
extractps $1, %xmm2, %ecx
pinsrb $7, %eax, %xmm2
pinsrd $3, (%rsi), %xmm9
pinsrq $1, %rax, %xmm2
pinsrq $0, %r9, %xmm13
insertps $0x10, %xmm2, %xmm3
dpps $0xff, %xmm2, %xmm3
dppd $0x31, %xmm2, %xmm3
mpsadbw $0, %xmm2, %xmm3
pcmpgtq %xmm2, %xmm3
pcmpestrm $0x0c, %xmm2, %xmm3
pcmpestri $0x0c, %xmm2, %xmm3
pcmpistrm $0x0c, %xmm2, %xmm3
pcmpistri $0x0c, (%rsi), %xmm10
crc32b %al, %eax
crc32b (%rsi), %r9d
crc32w %cx, %edx
crc32l %ecx, %eax
crc32l (%rdi), %eax
crc32q %rcx, %rax
crc32q (%rsi), %r10
crc32 %dl, %eax
crc32 %ecx, %eax
popcntq %rcx, %rax
popcntl %r9d, %eax
lzcntq %rdi, %rsi
ASM

{
    printf '__asm__(".globl zt_start\\nzt_start:\\n"\n'
    sed -e 's/.*/"&\\n"/' "$work/instructions.s"
    printf '".globl zt_end\\nzt_end:\\n");\n'
} > "$work/source.c"
cat > "$work/harness.c" <<'C'
#include "libtcc.h"
#include <stdio.h>
#include <stdlib.h>
static void error(void *opaque, const char *message) { (void)opaque; fprintf(stderr, "%s\n", message); }
int main(int argc, char **argv)
{
    FILE *file = fopen(argv[1], "rb");
    static char source[1 << 20];
    size_t used = file ? fread(source, 1, sizeof(source) - 1, file) : 0;
    if(!file || argc < 4) return 2;
    source[used] = 0;
    TCCState *s = tcc_new();
    tcc_set_error_func(s, NULL, error);
    tcc_set_options(s, "-nostdlib");
    tcc_add_sysinclude_path(s, argv[3]);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    if(tcc_compile_string(s, source) < 0 || tcc_relocate(s) < 0) return 1;
    unsigned char *start = tcc_get_symbol(s, "zt_start"), *end = tcc_get_symbol(s, "zt_end");
    FILE *out = fopen(argv[2], "wb");
    if(!start || !end || !out || fwrite(start, 1, (size_t)(end - start), out) != (size_t)(end - start)) return 1;
    fclose(out);
    return 0;
}
C
${CC:-cc} -O1 -I"$repo/third_party/tinycc" "$work/harness.c" "$build/obj/tinycc.o" -ldl -lpthread -lm -o "$work/harness"
${CC:-cc} -c "$work/instructions.s" -o "$work/gnu.o"
objcopy -O binary -j .text "$work/gnu.o" "$work/gnu.bin"
if ! "$work/harness" "$work/source.c" "$work/tinycc.bin" "$repo/third_party/tinycc/include" 2> "$work/error" ||
   ! cmp -s "$work/tinycc.bin" "$work/gnu.bin"; then
    # Name the first instruction that differs.
    line=0
    while IFS= read -r instruction; do
        line=$((line + 1))
        printf '%s\n' "$instruction" > "$work/one.s"
        { printf '__asm__(".globl zt_start\\nzt_start:\\n"\n'; sed -e 's/.*/"&\\n"/' "$work/one.s"
          printf '".globl zt_end\\nzt_end:\\n");\n'; } > "$work/one.c"
        "$work/harness" "$work/one.c" "$work/one-tcc.bin" "$repo/third_party/tinycc/include" || { echo "TinyCC rejects: $instruction" >&2; exit 1; }
        ${CC:-cc} -c "$work/one.s" -o "$work/one.o"
        objcopy -O binary -j .text "$work/one.o" "$work/one-gnu.bin"
        if ! cmp -s "$work/one-tcc.bin" "$work/one-gnu.bin"; then
            echo "TinyCC encodes '$instruction' as $(od -An -tx1 "$work/one-tcc.bin"), GNU as as $(od -An -tx1 "$work/one-gnu.bin")" >&2
            exit 1
        fi
    done < "$work/instructions.s"
    echo "asm-tinycc-encoding: the encodings differ" >&2
    exit 1
fi
echo "TinyCC encodes $(wc -l < "$work/instructions.s") SSE2-SSE4.2 instructions as GNU as does"
