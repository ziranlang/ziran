#!/bin/sh
# A `#reads` foreign procedure only reads its string and byte arguments during
# the call, so a fresh Text passed to it, directly or through a Ziran procedure
# that slices it, is freed when its block closes. An unmarked foreign one may
# keep the pointer and still keeps the string. `#reads` on a Ziran procedure is
# rejected. Source and saved IR agree; C runs allocation-counted and under
# AddressSanitizer.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/reads.zi" <<'ZI'
#import "vec"
host :: #system_library "host";
Measure :: (value: string) -> s32 #reads #foreign host "measure";
Keep :: (value: string) -> s32 #foreign host "keep";
KeptLength :: () -> s32 #foreign host "kept_length";

Make :: (a: string, b: string) -> Text {
    out: Vec(u8)
    if !BuilderAppend(out, a) || !BuilderAppend(out, b) { return .{} }
    return BuilderFinish(out)
}
Width :: (value: string) -> s32 { return Measure(value[0:2]) }

#program_export
Marked :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += cast(s64) Measure(Make("ab", "c"))
        total += cast(s64) Width(Make("xy", "z"))
        round += 1
    }
    return total
}

#program_export
Unmarked :: (rounds: s64) -> s64 {
    total: s64 = 0
    round: s64 = 0
    while round < rounds {
        total += cast(s64) Keep(Make("ke", "pt"))
        round += 1
    }
    return total
}

#program_export
Check :: () -> s32 {
    if Marked(1) != 5 || Unmarked(1) != 4 { return 1 }
    // The kept string is still readable after its block closed.
    scratch := Make("other", "bytes")
    if scratch.count != 10 || KeptLength() != 4 { return 2 }
    return 42
}
ZI
printf 'F :: (s: string) -> s64 #reads { return s.count }\n' > "$work/body.zi"
if "$ziran" check --root "$work" "$work/body.zi" 2> "$work/body.err"; then
    echo '#reads on a Ziran procedure was accepted' >&2
    exit 1
fi
grep -q '#reads requires a foreign procedure' "$work/body.err"
rm "$work/body.zi"

"$ziran" check --root "$work" --module-path "$repo/std" "$work/reads.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/reads.zi"
cat > "$work/host.c" <<'C'
#include "reads.h"
#include <stdlib.h>
#include <string.h>
static String kept;
int32_t measure(String value) { return (int32_t)value.length; }
int32_t keep(String value) { kept = value; return (int32_t)value.length; }
int32_t kept_length(void) { return kept.length == 4 && memcmp(kept.data, "kept", 4) == 0 ? 4 : -1; }
C
for kind in source saved; do
    root=$work; module=$work/reads.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/reads.zir; fi
    output=$work/$kind-c
    "$ziran" build --target=c --root "$root" --module-path "$repo/std" \
        -o "$output" "$module"
    cat > "$work/count.c" <<'C'
#include "reads.h"
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
    long before = outstanding;
    if (Marked(1000) != 5000) return 1;
    long marked = outstanding - before;
    before = outstanding;
    if (Unmarked(1000) != 4000) return 1;
    long unmarked = outstanding - before;
    printf("1000 rounds: %ld allocations left marked, %ld unmarked\n", marked, unmarked);
    if (Check() != 42) return 1;
    return marked == 0 && unmarked == 1000 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" "$output"/*.c "$work/host.c" \
        "$work/count.c" -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc \
        -Wl,--wrap=free -o "$work/count-$kind"
    "$work/count-$kind"
    printf '#include "reads.h"\nint main(void) { return Marked(50) == 250 && Check() == 42 ? 0 : 1; }\n' > "$work/plain.c"
    "${CC:-cc}" -std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -fno-omit-frame-pointer -I"$repo/include" -I"$output" "$output"/*.c \
        "$work/host.c" "$work/plain.c" -o "$work/asan-$kind"
    ASAN_OPTIONS=detect_leaks=0 "$work/asan-$kind"
done
cmp "$work/source-c/reads.c" "$work/saved-c/reads.c"
