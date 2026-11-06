#!/bin/sh
# Owned Text from std/vec frees its bytes with its owner: a local, a record
# field or a Vec element. TextView borrows it in place, and a Text place
# passed or stored where a string is expected lends the same view. The view
# cannot outlive its Text, and the Text cannot move or change while a view
# is live. Source and saved IR agree on C (allocation-counted and under
# AddressSanitizer leak checking), Go and portable `.zx`.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/texts.zi" <<'ZI'
#import "vec"
#import "option"

Group :: struct { name: Text; parts: Vec(Text); }

Length :: (value: string) -> s64 { return value.count }

MakeGroup :: (name: string) -> Group {
    parts: Vec(Text)
    VecPush(parts, TextCopy("p"))
    return .{name = TextCopy(name), parts = parts}
}

Echo :: (value: string) -> string { return value }

#program_export
Check :: () -> s32 {
    t: Text = TextCopy("hello")
    if TextView(t) != "hello" || Length(t) != 5 { return 1 }
    BuilderAppend(t.bytes, " world")
    {
        view := TextView(t)
        if view.count != 11 || view[6] != #char "w" { return 2 }
        lent: string = t
        if Echo(lent) != "hello world" { return 2 }
    }
    names: Vec(Text)
    VecPush(names, t)
    VecPush(names, TextCopy("x"))
    if Length(names[0]) != 11 || TextView(names[1]) != "x" { return 3 }
    total: s64 = 0
    for * names { total += TextView(it).count }
    if total != 12 { return 4 }
    groups: Vec(Group)
    VecPush(groups, MakeGroup("one"))
    if TextView(groups[0].name) != "one" || Length(groups[0].parts[0]) != 1 { return 5 }
    VecPush(groups[0].parts, TextCopy("q"))
    if TextView(groups[0].parts[1]) != "q" { return 5 }
    empty: Text
    if TextView(empty).count != 0 || Length(empty) != 0 { return 6 }
    popped := VecPop(names)
    if !popped.has_value || TextView(popped.value) != "x" { return 7 }
    VecClear(groups)
    return 42
}
ZI

reject() {
    name=$1
    message=$2
    cat > "$work/$name.zi"
    if "$ziran" check --root "$work" --module-path "$repo/std" \
        "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$name was accepted" >&2
        exit 1
    fi
    grep -Fq "$message" "$work/$name.err" || {
        cat "$work/$name.err" >&2
        exit 1
    }
}

reject returned_view 'returned text view borrows local or temporary storage' <<'ZI'
#import "vec"
F :: () -> string { t: Text = TextCopy("x"); return TextView(t) }
ZI
reject returned_lend 'returned text view borrows local or temporary storage' <<'ZI'
#import "vec"
F :: () -> string { t: Text = TextCopy("x"); return t }
ZI
reject mutated_backing 'mutating text backing storage while its view is live' <<'ZI'
#import "vec"
F :: () -> s64 {
    t: Text = TextCopy("x")
    view := TextView(t)
    BuilderAppend(t.bytes, "y")
    return view.count
}
ZI
reject moved_backing 'moving owned storage while its text view is live' <<'ZI'
#import "vec"
Holder :: struct { s: string; }
F :: () -> s64 {
    t: Text = TextCopy("x")
    h: Holder
    h.s = TextView(t)
    u := t
    return h.s.count
}
ZI
reject fresh_text 'TextView borrows a Text in place; bind the Text to a local first' <<'ZI'
#import "vec"
F :: () -> s64 { return TextView(TextCopy("x")).count }
ZI
reject escaped_global 'text view assignment may escape its backing storage' <<'ZI'
#import "vec"
saved: string;
F :: () { t: Text = TextCopy("x"); saved = t }
ZI
reject copied_element 'owned Vec elements cannot be copied' <<'ZI'
#import "vec"
F :: () -> s64 { names: Vec(Text); VecPush(names, TextCopy("x")); n := names[0]; return 1 }
ZI
reject borrowed_into_text 'a string does not own its bytes' <<'ZI'
#import "vec"
F :: () -> Text { return "x" }
ZI

"$ziran" check --root "$work" --module-path "$repo/std" "$work/texts.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/texts.zi"
for kind in source saved; do
    if test "$kind" = source; then
        root=$work
        module=$work/texts.zi
    else
        root=$work/ir
        module=$work/ir/texts.zir
    fi
    "$ziran" bundle --entry texts:Check --root "$root" \
        --module-path "$repo/std" -o "$work/$kind.zx" "$module"
    test "$("$ziran" run "$work/$kind.zx")" = 42

    c_output=$work/$kind-c
    "$ziran" build --target=c --entry texts:Check --root "$root" \
        --module-path "$repo/std" -o "$c_output" "$module"
    cat > "$work/count.c" <<'C'
#include "texts.h"
#include <stddef.h>
#include <stdlib.h>
void *__real_realloc(void *, size_t);
void __real_free(void *);
static int outstanding;
void *__wrap_realloc(void *p, size_t size) {
    void *result = __real_realloc(p, size);
    if (p == NULL && result != NULL) outstanding++;
    return result;
}
void __wrap_free(void *p) {
    if (p != NULL) outstanding--;
    __real_free(p);
}
int main(void) {
    if (Check() != 42) return 1;
    return outstanding == 0 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$c_output" \
        "$c_output"/*.c "$work/count.c" -Wl,--wrap=realloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    cat > "$work/plain.c" <<'C'
#include "texts.h"
int main(void) { return Check() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined \
        -fno-sanitize-recover=undefined -fno-omit-frame-pointer \
        -I"$repo/include" -I"$c_output" "$c_output"/*.c "$work/plain.c" \
        -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=1 "$work/asan-$kind"

    go_output=$work/$kind-go
    "$ziran" build --target=go --pkg main --entry texts:Check \
        --root "$root" --module-path "$repo/std" \
        -o "$go_output" "$module"
    cat > "$go_output/main.go" <<'GO'
package main
func main() { if Texts_Check() != 42 { panic("owned text") } }
GO
    GO111MODULE=off go run "$go_output"/*.go
done

cmp "$work/source.zx" "$work/saved.zx"
cmp "$work/source-c/texts.c" "$work/saved-c/texts.c"
cmp "$work/source-go/texts.go" "$work/saved-go/texts.go"
