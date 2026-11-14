#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/string-storage-XXXXXX")
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
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/empty_builder.zi"
for form in source saved; do
    root=$work
    extension=zi
    if [ "$form" = saved ]; then root=$work/ir; extension=zir; fi
    output=$work/$form-c
    "$ziran" build --target=c --entry empty_builder:Answer --root "$root" \
        --module-path "$repo/std" -o "$output" "$root/empty_builder.$extension"
    suffix=c
    compiler=${CC:-cc}
    standard=c11
    header=empty_builder.h
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
