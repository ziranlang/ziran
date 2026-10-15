#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Pair :: struct { low: u64; high: u64; }
Bump :: (value: s64) -> s64 #asm("x86_64", "leaq 1(%[value]), %[out]", "=r", "r", "", "") #asm("aarch64", "add %[out], %[value], #1", "=r", "r", "", "") {
    return value + 1
}
Tied :: (value: s64) -> s64 #asm("x86_64", "addq $1, %[out]", "=r", "0", "cc", "") {
    return value + 1
}
Dialect :: (value: s64) -> s64 #asm("x86_64", "mov{q %[value], %[out]| %[out], %[value]}", "=r", "r", "", "") { return value }
Parts :: (value: u64) -> Pair #asm("x86_64", "movq %[value], %[out_low]\n\tmovq %[value], %[out_high]\n\tshrq $32, %[out_high]", "=&r;=&r", "r", "cc", "") {
    return Pair.{low = value, high = value >> 32}
}
Store :: (value: *s64) #asm("x86_64", "addq $1, (%[value])", "", "r", "cc;memory", "volatile") {
    value.* += 1
}
// The temporary must not shadow a source parameter, including old candidates.
Identity :: (array_result_0_0: s64, zi_assembly_result: s64) -> s64 #asm("x86_64", "movq %[array_result_0_0], %[out]", "=r", "r;r", "", "") {
    unused zi_assembly_result
    return array_result_0_0
}
Default :: (value: s64 = 41) -> s64 #asm("x86_64", "leaq 1(%[value]), %[out]", "=r", "r", "", "") {
    return value + 1
}
// Braces, escaped quotes, # directives and semicolons belong to the template.
Barrier :: () #asm("x86_64", "# { #asm \"quoted\" };\n", "", "", "memory", "volatile") {
}
#program_export
Answer :: () -> s64 {
    if Dialect(42) != 42 { return 5 }
    pair := Parts(8589934595)
    if pair.low != 8589934595 || pair.high != 2 { return 1 }
    value: s64 = 10
    Store(*value)
    if value != 11 { return 2 }
    Barrier()
    if Identity(7, 9) != 7 { return 3 }
    if Default() != 42 { return 4 }
    return Bump(17) + Tied(23)
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    root=$work
    file=$work/app.zi
    if test "$form" = saved; then root=$work/ir; file=$root/app.zir; fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$form.zib" "$file"
    test "$("$ziran" run "$work/$form.zib")" = 42
    for target in c cpp; do
        out=$work/$form-$target
        "$ziran" build --target="$target" --root "$root" -o "$out" "$file"
        compiler=${CC:-cc}; suffix=c; header=h; standard=c11
        if test "$target" = cpp; then compiler=${CXX:-c++}; suffix=cpp; header=hpp; standard=c++17; fi
        printf '#include "app.%s"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' "$header" > "$work/host.$suffix"
        for optimization in 0 2; do
            for mode in native fallback; do
                define=
                if test "$mode" = fallback; then define=-DZIRAN_NO_ASM; fi
                "$compiler" -std="$standard" -O"$optimization" $define -I"$out" -I"$repo/include" \
                    "$out/app.$suffix" "$work/host.$suffix" -o "$out/run"
                "$out/run"
            done
        done
        ndk=${ANDROID_NDK_HOME:-}
        if test -n "$ndk"; then
            for triple in aarch64-linux-android24 armv7a-linux-androideabi24 x86_64-linux-android24 i686-linux-android24; do
                tool=clang
                if test "$target" = cpp; then tool=clang++; fi
                "$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/$triple-$tool" \
                    -std="$standard" -O2 -I"$out" -I"$repo/include" \
                    -c "$out/app.$suffix" -o "$out/$triple.o"
            done
        fi
    done
    "$ziran" build --target=go --pkg main --root "$root" -o "$work/$form-go" "$file"
    cat > "$work/$form-go/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("assembly fallback") } }
GO
    if command -v go >/dev/null 2>&1; then
        (cd "$work/$form-go" && GO111MODULE=off go build -o run . && ./run)
    fi
    "$ziran" build --target=py --root "$root" -o "$work/$form-py" "$file"
    (cd "$work/$form-py" && python3 -c 'import runpy; assert runpy.run_path("__init__.py")["Answer"]() == 42')
    "$ziran" build --target=rust --root "$root" -o "$work/$form-rust" "$file"
    if command -v cargo >/dev/null 2>&1; then
        cat > "$work/$form-rust/src/main.rs" <<'RS'
fn main() { assert_eq!(ziran_generated::Answer(), 42); }
RS
        cargo run --quiet --offline --manifest-path "$work/$form-rust/Cargo.toml"
    fi
    "$ziran" build --target=plan9-c --root "$root" -o "$work/$form-plan9" "$file"
    if rg -q '__asm__|ZIRAN_NO_ASM' "$work/$form-plan9"/*.c; then
        echo 'assembly leaked into Plan 9 output' >&2; exit 1
    fi
done
cmp "$work/source.zib" "$work/saved.zib"
cmp "$work/source-c/app.c" "$work/saved-c/app.c"
cmp "$work/source-cpp/app.cpp" "$work/saved-cpp/app.cpp"

reject() {
    expected=$1
    cat > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/error" 2>&1; then
        echo 'invalid assembly was accepted' >&2; exit 1
    fi
    grep -Fq "$expected" "$work/error" || { cat "$work/error"; exit 1; }
}
reject 'unknown #asm architecture' <<'ZI'
F :: () #asm("unknown", "", "", "", "", "") { return }
ZI
reject '#asm requires exactly six literal strings' <<'ZI'
F :: () #asm("x86_64", "", "", "", "") { return }
ZI
reject '#asm options must be empty or volatile' <<'ZI'
F :: () #asm("x86_64", "", "", "", "", "magic") { return }
ZI
reject '#asm constraint counts must match' <<'ZI'
F :: (value: s64) -> s64 #asm("x86_64", "", "=r", "", "", "") { return value }
ZI
reject '#asm outputs need = constraints' <<'ZI'
F :: (value: s64) -> s64 #asm("x86_64", "", "+r", "r", "", "") { return value }
ZI
reject 'duplicate or too many #asm architecture variants' <<'ZI'
F :: () #asm("x86_64", "", "", "", "", "") #asm("x86_64", "", "", "", "", "") { return }
ZI
reject '#asm parameters must be named scalars or pointers' <<'ZI'
F :: (value: string) #asm("x86_64", "", "", "r", "", "") { unused value }
ZI
reject 'embedded NUL is forbidden' <<'ZI'
F :: () #asm("x86_64", "\0", "", "", "", "") { return }
ZI
reject 'output operand name' <<'ZI'
F :: (out: s64) -> s64 #asm("x86_64", "", "=r", "r", "", "") { return out }
ZI
reject 'invalid file-scope constant' <<'ZI'
F :: #asm("x86_64", "", "", "", "", "") () { return }
ZI
reject '#asm requires an ordinary procedure with a portable body' <<'ZI'
libc :: #system_library "libc";
F :: (value: s32) -> s32 #asm("x86_64", "", "=r", "r", "", "") #foreign libc "abs";
ZI
reject '#asm parameters must be named scalars or pointers' <<'ZI'
F :: (value: $T) -> s64 #asm("x86_64", "", "=r", "r", "", "") { return 0 }
Answer :: () -> s64 { text: string = "aggregate"; return F(text) }
ZI
# A valid native implementation cannot mask a broken fallback.
reject 'not_a_binding' <<'ZI'
F :: () -> s64 #asm("x86_64", "movq $42, %[out]", "=r", "", "", "") { return not_a_binding }
ZI
