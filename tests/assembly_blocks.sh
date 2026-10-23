#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

cat > "$work/blocks.zi" <<'ZI'
Cell :: struct { value: s64; }
#program_export
Increment :: (class: s64) -> s64 {
    result: s64
    #asm("x86_64+popcnt", "leaq 1(%[source]), %[result]", "=r:result", "r:source=class", "", "") #asm("x86_64", "leaq 1(%[source]), %[result]", "=r:result", "r:source=class", "", "") #asm("aarch64", "add %[result], %[source], #1", "=r:result", "r:source=class", "", "") {
        result = class + 1
    }
    return result
}
ziran_assembly_block_0_1_0_0 :: () -> s64 { return 0 }
#program_export
Count :: (value: u64) -> s64 {
    total: s64
    #asm("x86_64+popcnt", "popcnt %[value], %[total]", "=r:total", "r:value", "cc", "") #asm("aarch64+simd", "fmov d0, %[value]\n\tcnt v0.8b, v0.8b\n\taddv b0, v0.8b\n\tumov %w[total], v0.b[0]", "=r:total", "r:value", "v0", "") {
        bits := value
        while bits != 0 {
            total += cast(s64)(bits & 1)
            bits >>= 1
            if bits == 0 { break }
        }
    }
    return total
}
#program_export
Add :: (value: s64) -> s64 {
    total: s64 = 7
    #asm("x86_64+popcnt", "addq %[value], %[total]", "=r:total", "0:old=total;r:value", "cc", "") #asm("x86_64", "addq %[value], %[total]", "=r:total", "0:old=total;r:value", "cc", "") {
        total += value
    }
    #asm("x86_64", "addq $1, %[total]", "=r:total", "0:old=total", "cc", "") { total += 1 }
    return total
}
#program_export
Double :: (value: float64) -> float64 {
    result: float64
    #asm("x86_64", "addsd %[value], %[result]", "=x:result", "0:old=value;x:value", "", "") #asm("aarch64", "fadd %d[result], %d[old], %d[value]", "=w:result", "0:old=value;w:value", "", "") {
        result = value + value
    }
    return result
}
#program_export
Store :: (value: *s64) {
    #asm("x86_64+popcnt", "addq $1, (%[value])", "", "r:value", "cc;memory", "volatile") {
        value.* += 1
    }
    #asm("x86_64", "", "", "", "memory", "volatile") {}
}
#program_export
Pair :: (value: s64, other: s64) -> s64 {
    sum: s64
    difference: s64
    #asm("x86_64+popcnt", "movq %[value], %[sum]\n\taddq %[other], %[sum]\n\tmovq %[value], %[difference]\n\tsubq %[other], %[difference]", "=&r:sum;=&r:difference", "r:value;r:other", "cc", "") {
        sum = value + other
        difference = value - other
    }
    return sum + difference
}
// Deliberately different markers check dispatch; production variants must
// implement the same behavior as their portable body.
#program_export
Pick :: () -> s64 {
    result: s64
    #asm("x86_64+popcnt", "movq $2, %[result]", "=r:result", "", "", "") #asm("x86_64", "movq $1, %[result]", "=r:result", "", "", "") { result = 0 }
    return result
}
#program_export
Memory :: (value: s64) -> s64 {
    result: s64
    alias := *result
    #asm("x86_64+popcnt", "movq %[value], %[out]\n\tmovq (%[alias]), %%rax\n\taddq %%rax, %[out]", "=m:out=result", "r:value;r:alias", "rax;cc;memory", "volatile") {
        result = value
        result += alias.*
    }
    return result
}
#program_export
Pointer :: (value: *Cell) -> *Cell {
    result: *Cell
    #asm("x86_64+popcnt", "movq %[value], %[result]", "=r:result", "r:value", "", "") #asm("aarch64+simd", "mov %[result], %[value]", "=r:result", "r:value", "", "") { result = value }
    return result
}
#program_export
Parallel :: (class: s64, low: s64, high: s64, opaque: s64, ziran_ctx: s64, ziran_i: s64, pointer: *Cell) -> s64 {
    #parallel for 0..2 {
        result: s64
        #asm("x86_64+popcnt", "leaq 10(%[a], %[index]), %[result]\n\taddq %[b], %[result]\n\taddq %[c], %[result]\n\taddq %[d], %[result]\n\taddq %[e], %[result]\n\taddq %[f], %[result]\n\taddq (%[g]), %[result]", "=&r:result", "r:a=class;r:b=low;r:c=high;r:d=opaque;r:e=ziran_ctx;r:f=ziran_i;r:g=pointer;r:index=it", "cc;memory", "") #asm("aarch64+simd", "add %[result], %[a], #10\n\tadd %[result], %[result], %[index]\n\tadd %[result], %[result], %[b]\n\tadd %[result], %[result], %[c]\n\tadd %[result], %[result], %[d]\n\tadd %[result], %[result], %[e]\n\tadd %[result], %[result], %[f]\n\tldr x9, [%[g]]\n\tadd %[result], %[result], x9", "=&r:result", "r:a=class;r:b=low;r:c=high;r:d=opaque;r:e=ziran_ctx;r:f=ziran_i;r:g=pointer;r:index=it", "x9;memory", "") {
            result = 42 + it
        }
        if result != 42 + it { unreachable }
    }
    return 42
}
#program_export
Answer :: () -> s64 {
    if Increment(41) != 42 || Add(34) != 42 || Double(1.5) != 3.0 { return 1 }
    value: s64 = 0
    Store(*value)
    if value != 1 { return 2 }
    cell: Cell
    if Pointer(*cell) != *cell { return 3 }
    if Pair(21, 7) != 42 || Memory(21) != 42 { return 4 }
    if Parallel(17, 1, 2, 3, 4, 5, *cell) != 42 { return 5 }
    return Count(4398046511103)
}
ZI
cat > "$work/host.c" <<'C'
#include "blocks.h"
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
unsigned queries;
static int expected_pick;
static void *run(void *unused) {
    (void)unused;
    for(int i=0;i<1000;i++) {
        assert(Pick()==expected_pick);
        assert(Count(UINT64_C(0xffffffff))==32);
    }
    return 0;
}
static unsigned count(uint64_t bits) {
    unsigned total=0;while(bits) { total+=bits&1;bits>>=1; }return total;
}
int main(void) {
#if defined(__x86_64__) && !defined(ZIRAN_NO_ASM)
    __builtin_cpu_init();expected_pick=__builtin_cpu_supports("popcnt")?2:1;
#endif
    pthread_t workers[4];
    for(int i=0;i<4;i++) assert(!pthread_create(&workers[i],0,run,0));
    for(int i=0;i<4;i++) assert(!pthread_join(workers[i],0));
    unsigned previous=queries;run(0);assert(queries==previous);
    uint64_t state=UINT64_C(0x123456789abcdef);
    for(int i=0;i<1000;i++) {
        state=state*UINT64_C(6364136223846793005)+1;
        int64_t value=state&INT64_C(0x7fffffff);
        assert(Increment(value)==value+1 && Add(value)==value+8);
        assert(Count(state)==count(state));
        assert(Pair(value,7)==value*2);
        assert(Memory(value)==value*2);
        assert(Double((double)i*0.5)==i);
        Store(&value);assert(value==(int64_t)(state&INT64_C(0x7fffffff))+1);
    }
    assert(Count(0)==0 && Count(UINT64_MAX)==64 && Answer()==42);
}
C
cat > "$work/missing.h" <<'C'
#define __builtin_cpu_init() ((void)0)
extern unsigned queries;
static int block_missing_feature(const char *feature) {
    (void)feature;__atomic_fetch_add(&queries,1,__ATOMIC_RELAXED);return 0;
}
#define __builtin_cpu_supports(feature) block_missing_feature(feature)
C
"$ziran" ir --root "$work" -o "$work/ir" "$work/blocks.zi"
for form in source saved; do
    root=$work; input=$work/blocks.zi
    if test "$form" = saved; then root=$work/ir; input=$root/blocks.zir; fi
    "$ziran" bundle --root "$root" --entry blocks:Answer -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 42
    for target in c cpp; do
        output=$work/$form-$target
        "$ziran" build --target="$target" --root "$root" -o "$output" "$input"
        compiler=${CC:-cc}; suffix=c; standard=c11
        if test "$target" = cpp; then
            compiler=${CXX:-c++}; suffix=cpp; standard=c++17
            sed 's/"blocks.h"/"blocks.hpp"/' "$work/host.c" > "$work/host.cpp"
        fi
        for optimization in 0 2; do
            for mode in native missing disabled; do
                set --
                if test "$mode" = missing; then set -- -include "$work/missing.h"; fi
                if test "$mode" = disabled; then set -- -DZIRAN_NO_ASM; fi
                isa_flags=
                if test "$(uname -m)" = x86_64; then isa_flags='-march=x86-64 -mno-popcnt -mno-avx'; fi
                "$compiler" -std="$standard" -O"$optimization" $isa_flags "$@" -I"$repo/include" -I"$output" \
                    "$output/blocks.$suffix" "$work/host.$suffix" -pthread -o "$output/run"
                "$output/run"
            done
        done
        if test -n "${ANDROID_NDK_HOME:-}"; then
            tool=clang
            if test "$target" = cpp; then tool=clang++; fi
            for triple in aarch64-linux-android24 armv7a-linux-androideabi24 x86_64-linux-android24 i686-linux-android24; do
                "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/$triple-$tool" \
                    -std="$standard" -O2 -I"$repo/include" -I"$output" \
                    "$output/blocks.$suffix" "$work/host.$suffix" -pthread -o "$output/$triple"
            done
        fi
    done
    "$ziran" build --target=plan9-c --root "$root" -o "$work/$form-plan9" "$input"
    if rg -q '__asm__|__builtin_cpu|__atomic|target\("' "$work/$form-plan9"/*.c; then
        echo 'native assembly leaked into Plan 9 output' >&2;exit 1
    fi
    "$ziran" build --target=go --pkg main --root "$root" -o "$work/$form-go" "$input"
    printf 'package main\nfunc main() { if Blocks_Answer() != 42 { panic("assembly fallback") } }\n' > "$work/$form-go/main.go"
    (cd "$work/$form-go" && GO111MODULE=off go build -o run . && ./run)
    "$ziran" build --target=py --root "$root" -o "$work/$form-py" "$input"
    (cd "$work/$form-py" && python3 -c 'import runpy; assert runpy.run_path("__init__.py")["Answer"]()==42')
    "$ziran" build --target=rust --root "$root" -o "$work/$form-rust" "$input"
    printf 'fn main() { assert_eq!(ziran_generated::Answer(), 42); }\n' > "$work/$form-rust/src/main.rs"
    cargo run --quiet --offline --manifest-path "$work/$form-rust/Cargo.toml"
done
cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source-c/blocks.c" "$work/saved-c/blocks.c"
cmp "$work/source-cpp/blocks.cpp" "$work/saved-cpp/blocks.cpp"

# A native payload cannot disagree with the statement it accompanies, and
# changing both copies still requires a real lexical binding.
python3 - "$work/ir/blocks.zir" "$work" <<'PY'
from pathlib import Path
import sys
data=Path(sys.argv[1]).read_bytes();out=Path(sys.argv[2])
needle=b'r:source=class';assert data.count(needle)==6
first=data.find(needle);second=data.find(needle,first+1)
changed=data[:second]+data[second:].replace(needle,b'r:source=ghost',1)
(out/'mismatch.zir').write_bytes(changed)
(out/'unknown.zir').write_bytes(data.replace(needle,b'r:source=ghost'))
tag=(1|(1<<30)).to_bytes(4,'little');at=data.find(tag);assert at>=0
(out/'wrong-kind.zir').write_bytes(data[:at]+(3|(1<<30)).to_bytes(4,'little')+data[at+4:])
text_length=int.from_bytes(data[at+4:at+8],'little');length_at=at+8+text_length
assert 0<int.from_bytes(data[length_at:length_at+4],'little')<4096
(out/'empty.zir').write_bytes(data[:length_at]+bytes(4)+data[length_at+4:])
PY
for bad in mismatch unknown wrong-kind empty; do
    if "$ziran" check --root "$work" "$work/$bad.zir" > "$work/$bad.log" 2>&1; then
        echo 'invalid saved assembly metadata accepted' >&2;exit 1
    fi
done
grep -Fq 'metadata does not match' "$work/mismatch.log"
grep -Fq 'scalar/pointer locals' "$work/unknown.log"
grep -Fq 'assembly metadata requires a block' "$work/wrong-kind.log"
grep -Fq 'empty assembly block metadata' "$work/empty.log"

reject() {
    expected=$1
    cat > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/error" 2>&1; then
        echo 'invalid assembly block accepted' >&2;exit 1
    fi
    grep -Fq "$expected" "$work/error" || { cat "$work/error";exit 1; }
}
reject 'constraint:alias=binding' <<'ZI'
F :: () { x:s64; #asm("x86_64", "", "=r", "", "", "") { x = 1 } }
ZI
reject 'scalar/pointer locals' <<'ZI'
F :: () { #asm("x86_64", "", "=r:x", "", "", "") {} }
ZI
reject 'duplicate #asm block operand alias' <<'ZI'
F :: (x:s64) { #asm("x86_64", "", "=r:x", "r:x", "", "") {} }
ZI
reject 'same operands' <<'ZI'
F :: (x:s64,y:s64) { #asm("x86_64", "", "=r:x", "", "", "") #asm("aarch64", "", "=r:y", "", "", "") {} }
ZI
reject 'duplicate or shadowed' <<'ZI'
F :: () { #asm("x86_64", "", "", "", "", "") #asm("x86_64+popcnt", "", "", "", "", "") {} }
ZI
reject 'must fall through' <<'ZI'
F :: () -> s64 { #asm("x86_64", "", "", "", "", "") { return 1 } return 2 }
ZI
reject 'cannot leave its scope' <<'ZI'
F :: () { while true { #asm("x86_64", "", "", "", "", "") { break } } }
ZI
reject 'scalar/pointer locals' <<'ZI'
F :: () {
    {
        x: s64
    }
    #asm("x86_64", "", "=r:x", "", "", "") {}
}
ZI
reject 'scalar/pointer locals' <<'ZI'
x:s64;
F :: () { #asm("x86_64", "", "=r:x", "", "", "") {} }
ZI
reject 'portable block must fall through' <<'ZI'
F :: (x: $T) { #asm("x86_64", "", "", "", "", "") { return } }
ZI
reject 'assembly outputs need an iteration-local binding' <<'ZI'
F :: (value: s64) {
    #parallel for 0..2 {
        #asm("x86_64", "", "=r:value", "", "", "") {}
    }
}
ZI
reject 'assembly operands cannot hold pointers' <<'ZI'
F :: (value: *s64) {
    #parallel_gpu for 0..2 {
        #asm("x86_64", "", "", "r:value", "", "") {}
    }
}
ZI

# The native worker's existing capture bound must produce a diagnostic,
# rather than emitting a worker with undeclared operand references.
python3 - "$work/limit.zi" <<'PY'
from pathlib import Path
import sys
parameters=', '.join(f'arg{i}:s64' for i in range(17))
operands=';'.join(f'r:arg{i}' for i in range(17))
Path(sys.argv[1]).write_text(f'''F :: ({parameters}) {{
    #parallel for 0..2 {{
        #asm("x86_64", "", "", "{operands}", "", "") {{}}
    }}
}}
''')
PY
"$ziran" ir --root "$work" -o "$work/limit-ir" "$work/limit.zi"
for form in source saved; do
    root=$work; input=$work/limit.zi
    if test "$form" = saved; then root=$work/limit-ir; input=$root/limit.zir; fi
    for target in c cpp; do
        if "$ziran" build --target="$target" --root "$root" -o "$work/limit-$target" "$input" > "$work/limit.log" 2>&1; then
            echo 'native parallel capture limit was ignored' >&2;exit 1
        fi
        grep -Fq 'exceeds 16 captured bindings' "$work/limit.log"
    done
done
echo 'Assembly blocks preserve typed local operands, CPU selection and portable fallbacks'
