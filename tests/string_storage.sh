#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/empty_builder.zi" <<'ZI'
#import "vec"
#program_export
Answer :: () -> s32 {
    index: s32 = 0
    while index < 10000 {
        builder: Vec(u8)
        if !BuilderAppend(builder, "some allocated bytes") { return 1 }
        VecClear(builder)
        text := BuilderFinish(builder)
        if text.count != 0 { return 2 }
        index += 1
    }
    return 42
}
ZI
python3 - "$work/string_storage.zi" <<'PY'
import pathlib, sys
pathlib.Path(sys.argv[1]).write_text('''#import "vec"
host_api :: #system_library "host_api";
Tail :: (text: string) -> string #foreign host_api;
Measure :: () -> s64 #foreign host_api;
saved: [2]string;
current: string;
Box :: struct { text: string }
Make :: (label: string) -> string {
    builder: Vec(u8)
    BuilderAppend(builder, label)
    BuilderAppend(builder, "''' + 'x' * 1024 + '''")
    return BuilderFinish(builder)
}
#program_export
Answer :: () -> s32 {
    index: s32 = 0
    while index < 10000 {
        label := "later"
        if index == 0 { label = "first" }
        text := Make(label)
        copy := text
        box: Box = .{text = copy}
        if index == 0 {
            saved[0] = box.text[0:5]
            saved[1] = Tail(box.text)
        }
        current = Tail(text)
        if index % 100 == 0 { Measure() }
        index += 1
    }
    if saved[0] != "first" || saved[1] != "irst" || current != "ater" { return 1 }
    return 42
}
''')
PY
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" \
    "$work/empty_builder.zi" "$work/string_storage.zi"
${CC:-cc} ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/vm_string_storage_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -lm -pthread -o "$work/vm-test"
for form in source saved; do
    root=$work
    extension=zi
    if [ "$form" = saved ]; then root=$work/ir; extension=zir; fi
    "$ziran" bundle --root "$root" --module-path "$repo/std" --entry string_storage:Answer \
        -o "$work/$form.zib" "$root/string_storage.$extension"
    "$work/vm-test" "$work/$form.zib"
    for target in c cpp; do
        output=$work/$form-$target
        "$ziran" build "--target=$target" --entry empty_builder:Answer --root "$root" \
            --module-path "$repo/std" -o "$output" "$root/empty_builder.$extension"
        suffix=c
        compiler=${CC:-cc}
        standard=c11
        header=empty_builder.h
        if [ "$target" = cpp ]; then
            suffix=cpp; compiler=${CXX:-c++}; standard=c++17; header=empty_builder.hpp
        fi
        printf '#include "%s"\n' "$header" > "$output/main.$suffix"
        cat >> "$output/main.$suffix" <<'C'
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *__real_realloc(void *, size_t);
void __real_free(void *);
static int outstanding;
void *__wrap_realloc(void *p, size_t size) {
    void *result = __real_realloc(p, size);
    if(p == NULL && result != NULL) outstanding++;
    return result;
}
void __wrap_free(void *p) {
    if(p != NULL) outstanding--;
    __real_free(p);
}
#ifdef __cplusplus
}
#endif
int main(void) { return Answer() == 42 && outstanding == 0 ? 0 : 1; }
C
        "$compiler" ${VM_CFLAGS:-} "-std=$standard" -I"$repo/include" -I"$output" \
            "$output/"*."$suffix" -Wl,--wrap=realloc -Wl,--wrap=free -o "$output/app"
        "$output/app"
    done
    output=$work/$form-rust
    "$ziran" build --target=rust --exe --entry empty_builder:Answer --root "$root" \
        --module-path "$repo/std" -o "$output" "$root/empty_builder.$extension"
    python3 - "$output/src/main.rs" <<'PY'
import pathlib, sys
path = pathlib.Path(sys.argv[1])
source = path.read_text().replace('fn main() {', 'fn main() {\n    let before = LIVE.load(std::sync::atomic::Ordering::SeqCst);')
source = source.replace('std::process::exit(code);', '''assert_eq!(code, 42);
    assert_eq!(LIVE.load(std::sync::atomic::Ordering::SeqCst), before);''')
source += '''
struct CountingAllocator;
static LIVE: std::sync::atomic::AtomicI64 = std::sync::atomic::AtomicI64::new(0);
unsafe impl std::alloc::GlobalAlloc for CountingAllocator {
    unsafe fn alloc(&self, layout: std::alloc::Layout) -> *mut u8 {
        let result = std::alloc::System.alloc(layout);
        if !result.is_null() { LIVE.fetch_add(1, std::sync::atomic::Ordering::SeqCst); }
        result
    }
    unsafe fn dealloc(&self, data: *mut u8, layout: std::alloc::Layout) {
        LIVE.fetch_sub(1, std::sync::atomic::Ordering::SeqCst);
        std::alloc::System.dealloc(data, layout);
    }
    unsafe fn realloc(&self, data: *mut u8, layout: std::alloc::Layout, size: usize) -> *mut u8 {
        std::alloc::System.realloc(data, layout, size)
    }
}
#[global_allocator]
static ALLOCATOR: CountingAllocator = CountingAllocator;
'''
path.write_text(source)
PY
    CARGO_TARGET_DIR=$work/rust-target cargo build --quiet --manifest-path "$output/Cargo.toml"
    "$work/rust-target/debug/ziran_generated"
done
