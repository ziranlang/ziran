#!/bin/sh
set -eu
ulimit -c 0

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Readability limits must not retain a large result for every sequential
# assignment. This is the value-update pattern used by incremental parsers.
cat > "$work/update.zi" <<'ZI'
State :: struct { payload: [16384]s32; count: s32; }
Holder :: struct { state: State; }
LabelledIncrement :: (value: State, label: string, amount: s32) -> State {
    value.count += amount
    value.payload[0] = value.count
    value.payload[16383] = cast(s32)label.count
    return value
}
IncrementAmount :: (amount: s32) -> s32 { return amount }
Update :: (value: State) -> State {
ZI
for step in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
    printf '%s\n' '    value = LabelledIncrement(value, "a long descriptive label that must not increase aggregate storage", IncrementAmount(1))' >> "$work/update.zi"
done
cat >> "$work/update.zi" <<'ZI'
    return value
}
#program_export
CheckFields :: () -> s32 {
    holder: Holder
    unrelated: [4]s32
    other_view := unrelated[:]
    other_address := *unrelated[0]
    unused other_view
    unused other_address
ZI
for step in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
    printf '%s\n' '    holder.state = LabelledIncrement(holder.state, "a long descriptive label that must not increase aggregate storage", IncrementAmount(1))' >> "$work/update.zi"
done
cat >> "$work/update.zi" <<'ZI'
    return holder.state.count + holder.state.payload[0] + holder.state.payload[16383]
}
#program_export
Check :: () -> s32 {
    initial: State
    result := Update(initial)
    return result.count + result.payload[0] + result.payload[16383]
}
Observe :: (value: State, amount: s32) -> s32 {
    return value.payload[0] * 10 + amount
}
Mutate :: (view: []s32) -> s32 {
    view[0] = 99
    return 1
}
#program_export
CheckAliasedArguments :: () -> s32 {
    holder: Holder
    holder.state.payload[0] = 10
    view := holder.state.payload[:]
    return Observe(holder.state, Mutate(view))
}
ZI

cat > "$work/main.c" <<'C'
#include "update.h"
#include <assert.h>
#include <pthread.h>

static void *run(void *argument)
{
    (void)argument;
    assert(Check() == 97);
    assert(CheckFields() == 97);
    assert(CheckAliasedArguments() == 101);
    return NULL;
}

int main(void)
{
    pthread_attr_t attributes;
    pthread_t thread;
    assert(pthread_attr_init(&attributes) == 0);
    assert(pthread_attr_setstacksize(&attributes, 512 * 1024) == 0);
    assert(pthread_create(&thread, &attributes, run, NULL) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert(pthread_attr_destroy(&attributes) == 0);
    return 0;
}
C
"$ziran" ir --root "$work" -o "$work/ir" "$work/update.zi"
for input in source saved; do
    root=$work
    module=$work/update.zi
    if test "$input" = saved; then
        root=$work/ir
        module=$root/update.zir
    fi
    for target in c cpp; do
        output=$work/$input-$target
        "$ziran" build --target="$target" \
            --root "$root" -o "$output" "$module"
        if test "$target" = c; then
            compiler=${CC:-cc}
            standard=c99
            suffix=c
            cp "$work/main.c" "$output/main.c"
        else
            compiler=${CXX:-c++}
            standard=c++17
            suffix=cpp
            sed 's/update.h/update.hpp/' "$work/main.c" > "$output/main.cpp"
        fi
        # Forbid accumulated result frames independently of whether the host
        # optimizer happens to inline the incremental parser.
        "$compiler" -std="$standard" -O2 -fno-inline \
            -Werror=frame-larger-than=262144 -pthread -I"$output" \
            "$output"/*.$suffix -o "$output/program"
        env -u DISPLAY -u WAYLAND_DISPLAY "$output/program"
    done
    "$ziran" bundle --entry update:Check --root "$root" \
        -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 97
    "$ziran" bundle --entry update:CheckFields --root "$root" \
        -o "$work/$input-fields.zib" "$module"
    test "$("$ziran" run "$work/$input-fields.zib")" = 97
    "$ziran" bundle --entry update:CheckAliasedArguments --root "$root" \
        -o "$work/$input-alias.zib" "$module"
    test "$("$ziran" run "$work/$input-alias.zib")" = 101
done
echo 'record updates: C, C++ and bundles preserve values on a small native stack'
