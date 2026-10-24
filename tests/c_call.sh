#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/callback.zi" <<'ZI'
Callback :: #type (context: *void, value: s32) -> s32 #c_call;
Holder :: struct {
    callback: Callback
    context: *void
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/callback.zi"
cat > "$work/c_pointer_spelling.zi" <<'ZI'
Callback :: #type (text: *const u8) -> void #c_call;
ZI
if "$ziran" check --root "$work" "$work/c_pointer_spelling.zi" \
    > "$work/c_pointer_spelling.out" 2>&1; then
    echo 'C const pointer syntax unexpectedly passed checking' >&2
    exit 1
fi
grep -Fq 'const qualifier is not Jai syntax' "$work/c_pointer_spelling.out"
for input in source saved; do
    if test "$input" = source; then
        source="$work/callback.zi"
    else
        source="$work/ir/callback.zir"
    fi
    output="$work/$input-c"
    "$ziran" build --target=c --root "$work" \
        -o "$output" "$source"
    cat > "$work/smoke.c" <<'C'
#include "callback.h"
static int32_t add_one(void *context, int32_t value) {
    (void)context;
    return value + 1;
}
int main(void) {
    Holder holder = {add_one, 0};
    return holder.callback(holder.context, 41) == 42 ? 0 : 1;
}
C
    "${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" \
        -I"$output" "$work/smoke.c" -o "$output/app"
    "$output/app"
done

if "$ziran" build --target=go --root "$work" -o "$work/go" \
    "$work/callback.zi" > "$work/go.out" 2>&1; then
    echo '#c_call unexpectedly built for Go' >&2
    exit 1
fi
grep -Fq 'target does not support #c_call procedure types' "$work/go.out"

# A Ziran function with an exact C callback signature can fill a native
# callback field. Portable bundles and Go must still reject that field.
cat > "$work/native.zi" <<'ZI'
Callback :: #type (context: *void, value: s32) -> s32 #c_call;
Binding :: struct {
    call: Callback
}

Increment :: (context: *void, value: s32) -> s32 {
    return value + 1
}

#program_export
Make :: () -> Binding {
    callback: Callback = Increment
    result: Binding
    result.call = callback
    return result
}

#program_export
Invoke :: (binding: Binding, context: *void, value: s32) -> s32 {
    callback := binding.call
    return callback(context, value)
}

#program_export
Answer :: () -> s32 {
    binding: Binding = Make()
    unused binding
    return 42
}
ZI

output="$work/native-c"
"$ziran" build --target=c --root "$work" -o "$output" \
    "$work/native.zi"
cat > "$output/smoke.c" <<'C'
#include "native.h"
int main(void) {
    Binding binding = Make();
    return binding.call(0, 41) == 42 && Invoke(binding, 0, 41) == 42 ? 0 : 1;
}
C
"${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" -I"$output" \
    "$output/native.c" "$output/smoke.c" -o "$output/app"
"$output/app"

if "$ziran" bundle --root "$work" --entry native:Answer \
    -o "$work/native.zx" "$work/native.zi" > "$work/bundle.out" 2>&1; then
    echo '#c_call function value unexpectedly entered a portable bundle' >&2
    exit 1
fi
grep -Fq 'target does not support #c_call procedure types' "$work/bundle.out"
if "$ziran" build --target=go --root "$work" -o "$work/native-go" \
    "$work/native.zi" > "$work/native-go.out" 2>&1; then
    echo '#c_call function value unexpectedly built for Go' >&2
    exit 1
fi
grep -Fq 'target does not support #c_call procedure types' \
    "$work/native-go.out"

# Native callbacks are plain function pointers. Closure slots retain their
# callable/context representation, including when null is the left operand.
cat > "$work/compare.zi" <<'ZI'
Callback :: #type (value: s32) -> s32 #c_call;
Closure :: #type (value: s32) -> s32;
Callbacks :: struct {
    native: Callback
    closure: Closure
}
callback_calls: s32;
empty_callback: Callback;

Increment :: (value: s32) -> s32 {
    return value + 1
}

Different :: (value: s32) -> s32 {
    return value + 2
}

Empty :: () -> Callback {
    callback_calls += 1
    return empty_callback
}

#program_export
main :: () -> s32 {
    callbacks: Callbacks
    if callbacks.native != null || null != callbacks.native ||
        !(callbacks.native == null) || !(null == callbacks.native) {
        return 1
    }
    callbacks.native = Increment
    if callbacks.native == null || null == callbacks.native ||
        !(callbacks.native != null) || !(null != callbacks.native) {
        return 2
    }
    same: Callback = Increment
    other: Callback = Different
    if callbacks.native != same || callbacks.native == other ||
        !(callbacks.native == same) || !(callbacks.native != other) {
        return 3
    }
    if callbacks.native(41) != 42 {
        return 4
    }
    if !(null == Empty()) || Empty() != null || callback_calls != 2 {
        return 5
    }
    if callbacks.closure != null || null != callbacks.closure ||
        !(callbacks.closure == null) || !(null == callbacks.closure) {
        return 6
    }
    callbacks.closure = Increment
    if callbacks.closure == null || null == callbacks.closure ||
        !(callbacks.closure != null) || !(null != callbacks.closure) {
        return 7
    }
    return 0
}
ZI
"$ziran" ir --root "$work" -o "$work/compare-ir" "$work/compare.zi"
for input in source saved; do
    source="$work/compare.zi"
    if test "$input" = saved; then
        source="$work/compare-ir/compare.zir"
    fi
    output="$work/compare-$input-c"
    "$ziran" build --target=c --root "$work" -o "$output" "$source"
    "${CC:-cc}" -std=c11 -Wall -Werror -I"$repo/include" \
        -iquote "$output" "$output/compare.c" -o "$output/app"
    "$output/app"
done
