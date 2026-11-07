#!/bin/sh
# `#reads` follows a string through field paths: a record parameter copied
# into a field of a record local, as in `wrapped.data = command`, is followed
# to that local's later uses, and `=` into the path afterwards in the same
# block replaces the string, so a later store of the record keeps nothing.
# A fresh Text in a record local that is only read is freed with its block.
# Keeping paths are still rejected: a store after the copy, a replacement
# that may not run, a loop that reads the old copy again, a nested field
# kept, or a pointer to the record handed on. Source and saved IR agree; C
# runs allocation-counted and under AddressSanitizer.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/paths.zi" <<'ZI'
#import "std/vec"
Hook :: #type (value: string) -> s64 #reads;
Command :: struct { value: string; x: s32; }
Kept :: struct { command: Command; start: s64; }
Wrapped :: struct { data: Command; flag: bool; }
kept_list: Vec(Kept);

Make :: (a: string) -> Text { return TextCopy(a) }
// Copies the record into a local, blanks the string, then stores it.
Append :: (command: Command) -> s64 {
    kept: Kept
    kept.command = command
    kept.start = command.value.count
    kept.command.value = ""
    unused VecPush(kept_list, kept)
    return kept.start
}
Read :: (wrapped: Wrapped) -> s64 { return wrapped.data.value.count }
// Only reads, through a record copied into an outer record's field.
Record :: (command: Command) -> s64 {
    wrapped: Wrapped
    wrapped.data = command
    return Read(wrapped)
}
Draw :: (value: string) -> s64 {
    return Append(Command.{.value = value, .x = 1}) + Record(Command.{.value = value})
}

#program_export
Run :: (rounds: s64) -> s64 {
    hook: Hook = Draw
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += hook(Make("abc"))
        // A record local holding a fresh Text that is only read.
        local := Wrapped.{.data = Command.{.value = Make("de")}}
        total += Read(local) + local.data.value.count
        round += 1
    }
    return total
}

#program_export
Stored :: () -> s64 { return kept_list.count }
ZI
"$ziran" check --root "$work" --module-path "$repo" "$work/paths.zi"
"$ziran" ir --root "$work" --module-path "$repo" -o "$work/ir" "$work/paths.zi"
for kind in source saved; do
    root=$work; module=$work/paths.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/paths.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo" -o "$output" "$module"
    cat > "$work/count.c" <<'C'
#include "paths.h"
#include <stddef.h>
#include <stdio.h>
void *__real_realloc(void *, size_t);
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
static long outstanding;
void *__wrap_malloc(size_t size) { void *r = __real_malloc(size); if (r) outstanding++; return r; }
void *__wrap_calloc(size_t n, size_t size) { void *r = __real_calloc(n, size); if (r) outstanding++; return r; }
void *__wrap_realloc(void *p, size_t size) { void *r = __real_realloc(p, size); if (!p && r) outstanding++; return r; }
void __wrap_free(void *p) { if (p) outstanding--; __real_free(p); }
int main(void) {
    /* (3 + 3) + (2 + 2) for each round; kept_list holds one Vec buffer. */
    if (Run(1000) != 10000 || Stored() != 1000) return 1;
    printf("1000 rounds: %ld allocations left\n", outstanding);
    return outstanding == 1 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c "$work/count.c" \
        -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free \
        -o "$work/count-$kind"
    "$work/count-$kind"
    printf '#include "paths.h"\nint main(void) { return Run(50) == 500 && Stored() == 50 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
done
cmp "$work/source-c/paths.c" "$work/saved-c/paths.c"

# Each of these keeps the string, so it cannot be stored in a #reads type.
mkdir "$work/bad"
reject() {
    name=$1
    cat > "$work/bad/$name.zi"
    if "$ziran" check --root "$work/bad" --module-path "$repo" "$work/bad/$name.zi" 2> "$work/bad/$name.err"; then
        echo "$name was accepted" >&2
        exit 1
    fi
    grep -q 'Draw keeps its parameter value, but Hook is #reads' "$work/bad/$name.err"
}
prelude='#import "std/vec"
Hook :: #type (value: string) -> () #reads;
Command :: struct { value: string; x: s32; }
Kept :: struct { command: Command; start: s64; }
Wrapped :: struct { data: Command; flag: bool; }
kept_list: Vec(Kept);
kept_text: string;
main :: () -> s32 { hook: Hook = Draw; hook("a"); return 0 }'
{ echo "$prelude"; cat <<'ZI'; } | reject stored_first
Append :: (command: Command) {
    kept: Kept
    kept.command = command
    unused VecPush(kept_list, kept)
    kept.command.value = ""
}
Draw :: (value: string) { Append(Command.{.value = value}) }
ZI
{ echo "$prelude"; cat <<'ZI'; } | reject maybe_replaced
Append :: (command: Command) {
    kept: Kept
    kept.command = command
    if command.x > 0 { kept.command.value = "" }
    unused VecPush(kept_list, kept)
}
Draw :: (value: string) { Append(Command.{.value = value}) }
ZI
{ echo "$prelude"; cat <<'ZI'; } | reject loop_again
Append :: (command: Command) {
    kept: Kept
    round := 0
    while round < 2 {
        if round > 0 { unused VecPush(kept_list, kept) }
        kept.command = command
        round += 1
    }
}
Draw :: (value: string) { Append(Command.{.value = value}) }
ZI
{ echo "$prelude"; cat <<'ZI'; } | reject nested_kept
Keep :: (wrapped: Wrapped) { kept_text = wrapped.data.value }
Draw :: (value: string) { Keep(Wrapped.{.data = Command.{.value = value}}) }
ZI
{ echo "$prelude"; cat <<'ZI'; } | reject pointer
Request :: struct { name: string; ok: bool; }
Run :: (argument: *void) { request := cast(*Request)argument; request.ok = request.name.count > 0 }
Draw :: (value: string) {
    request := Request.{.name = value}
    Run(cast(*void)*request)
}
ZI
# A #reads type with a record parameter checks the strings it holds nested.
cat > "$work/bad/nested_slot.zi" <<'ZI'
Command :: struct { value: string; x: s32; }
Wrapped :: struct { data: Command; flag: bool; }
Painter :: #type (wrapped: Wrapped) -> () #reads;
kept_text: string;
Keep :: (wrapped: Wrapped) { kept_text = wrapped.data.value }
main :: () -> s32 { paint: Painter = Keep; paint(Wrapped.{}); return 0 }
ZI
if "$ziran" check --root "$work/bad" "$work/bad/nested_slot.zi" 2> "$work/bad/nested_slot.err"; then
    echo 'a procedure keeping a nested string was stored in a #reads type' >&2
    exit 1
fi
grep -q 'Keep keeps its parameter wrapped.data.value, but Painter is #reads' "$work/bad/nested_slot.err"
echo 'Text through #reads field paths: copies into record locals followed, replaced paths dropped, 1 allocation left (the Vec); keeping paths rejected; source and saved IR agree; ASan clean'
