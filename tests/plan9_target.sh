#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
zi2c="${ziran%/*}/zi2c"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

mkdir "$work/src"
cat > "$work/src/main.zi" <<'EOF'
Inner :: struct { value: s32 }
Props :: struct { inner: Inner; scale: s32 }
Make :: () -> Props { return .{inner = .{value = 40}, scale = 2} }
Read :: (props: Props) -> s32 { return props.inner.value + props.scale }
#program_export
main :: () -> s32 {
    local := Make()
    bytes: [3]u8 = .[97, 98, 99]
    text: string = "abc"
    part: string = text[0:2]
    if Read(local) != 42 || text.count != 3 ||
        text[0] != bytes[0] || part != "ab" { return 1 }
    return 0
}
EOF

"$ziran" build --target=plan9-c --root "$work/src" \
    -o "$work/generated" "$work/src/main.zi"

test -f "$work/generated/main.c"
test -f "$work/generated/zir_plan9_runtime.h"
if rg -n '^#include <(stdint|stddef|stdbool|stdlib)\.h>' \
        "$work/generated"/*.c "$work/generated"/*.h; then
    echo 'plan9-c output retained hosted C headers' >&2
    exit 1
fi
runtime_include_count=0
for header in "$work/generated"/*.h; do
    header_count=$(rg -c '^#include "zir_plan9_runtime\.h"$' \
        "$header" || true)
    runtime_include_count=$((runtime_include_count + \
        ${header_count:-0}))
done
if [ "$runtime_include_count" -ne 1 ]; then
    echo 'plan9-c emitted duplicate runtime includes' >&2
    exit 1
fi
if rg -n 'static inline|__auto_type|\{\s*\.|for\s*\(\s*(int|s32|u32)|[0-9]U?LL' \
        "$work/generated"/*.c "$work/generated"/*.h; then
    echo 'plan9-c output retained unsupported C constructs' >&2
    exit 1
fi

rg -q '^int32_t ziran_plan9_main\(void\);$' "$work/generated/main.h"
rg -q '^void main\(void\);$' "$work/generated/main.h"
rg -q '^ziran_plan9_main\(void\)$' "$work/generated/main.c"
rg -q '^[[:space:]]*exits\(status_text\);$' "$work/generated/main.c"

if rg -n 'plan9-c' "$work/generated"/*.c "$work/generated"/*.h; then
    echo 'plan9-c leaked dispatcher metadata into generated C' >&2
    exit 1
fi

mkdir -p "$work/plan9-include"
cat > "$work/plan9-include/u.h" <<'EOF'
#ifndef FAKE_U_H
#define FAKE_U_H
typedef signed char schar;
typedef unsigned char uchar;
typedef short ushort;
typedef unsigned int uint;
typedef long long vlong;
typedef unsigned long long uvlong;
typedef unsigned long usize;
#endif
EOF
cat > "$work/plan9-include/libc.h" <<'EOF'
#ifndef FAKE_LIBC_H
#define FAKE_LIBC_H
extern void *realloc(void *, unsigned long);
extern void free(void *);
extern void *memmove(void *, const void *, unsigned long);
extern void *memcpy(void *, const void *, unsigned long);
extern void *memset(void *, int, unsigned long);
extern int memcmp(const void *, const void *, unsigned long);
extern int fprint(int, const char *, ...);
extern int snprint(char *, int, const char *, ...);
extern void exits(const char *);
extern void abort(void);
#endif
EOF

cat > "$work/src/status_main.zi" <<'EOF'
#import "c_string"
#program_export
main :: () -> s32 {
    output: [8]u8
    if !CopyCString("42", output[:]) { return 2 }
    return 42
}
EOF

"$ziran" build --target=plan9-c --root "$work/src" \
    --module-path "$repo/std" -o "$work/generated-status" \
    "$work/src/status_main.zi"
rg -q '^int32_t ziran_plan9_main\(void\);$' \
    "$work/generated-status/status_main.h"
rg -q '^void main\(void\);$' "$work/generated-status/status_main.h"
cat > "$work/fake-plan9-exits.c" <<'EOF'
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int fprint(int fd, const char *format, ...) {
    char buffer[512];
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    return result < 0 ? result :
        (int)write(fd, buffer, (unsigned long)result);
}
int snprint(char *buffer, int size, const char *format, ...) {
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vsnprintf(buffer, (size_t)size, format, arguments);
    va_end(arguments);
    return result;
}
void exits(const char *status) {
    if(status == NULL || strcmp(status, "42") == 0)
        exit(0);
    exit(1);
}
EOF
"${CC:-cc}" -std=c11 -I"$work/plan9-include" \
    -I"$work/generated-status" "$work/generated-status"/*.c \
    "$work/fake-plan9-exits.c" -o "$work/plan9-status-runner"
"$work/plan9-status-runner"

cat > "$work/src/vec_main.zi" <<'EOF'
#import "vec"
Location :: (location: Source_Code_Location = #caller_location) -> Source_Code_Location { return location }
#program_export
VecMain :: () -> s32 {
    values: Vec(s32)
    if !VecPush(values, 42) { VecFree(values); return 1 }
    if Location().line_number <= 0 { VecFree(values); return 2 }
    answer := values[0]
    VecFree(values)
    return answer
}
EOF

"$ziran" build --target=plan9-c --root "$work/src" --module-path "$repo/std" \
    -o "$work/generated-vec" "$work/src/vec_main.zi"
if rg -n '^#include <(stdint|stddef|stdbool|stdlib)\.h>' \
        "$work/generated-vec"/*.c "$work/generated-vec"/*.h; then
    echo 'plan9-c vector output retained hosted C headers' >&2
    exit 1
fi
if rg -n 'zir_vec\.h|zir_string\.h|zir_bounds\.h' \
        "$work/generated-vec"/*.c "$work/generated-vec"/*.h; then
    echo 'plan9-c vector output retained hosted runtime headers' >&2
    exit 1
fi
rg -q 'ZirVecReserve' "$work/generated-vec/zir_plan9_runtime.h"
if rg -q '^void main\(void\);$' "$work/generated-vec"/*.h; then
    echo 'plan9-c wrapped a non-main export as main' >&2
    exit 1
fi
# Compile the Plan 9 dialect with a minimal fake libc. This checks emitted C
# syntax and runtime semantics when no Plan 9 host compiler is installed.
cat > "$work/plan9-runner.c" <<'EOF'
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
int VecMain(void);
int fprint(int fd, const char *format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    int count = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return count < 0 ? count : (int)write(fd, buffer, (unsigned long)count);
}
int main(void) {
    int result = VecMain();
    printf("%d\n", result);
    return result == 42 ? 0 : 1;
}
EOF
"${CC:-cc}" -std=c11 -I"$work/plan9-include" \
    -I"$work/generated-vec" "$work/generated-vec"/*.c \
    "$work/plan9-runner.c" -o "$work/plan9-runner"
test "$("$work/plan9-runner")" = 42

"$zi2c" --target=plan9-c --root "$work/src" --module-path "$repo/std" \
    -o "$work/direct-vec" "$work/src/vec_main.zi"
for output in vec vec_main; do
    cmp "$work/generated-vec/$output.c" "$work/direct-vec/$output.c"
    cmp "$work/generated-vec/$output.h" "$work/direct-vec/$output.h"
done
cmp "$work/generated-vec/zir_plan9_runtime.h" \
    "$work/direct-vec/zir_plan9_runtime.h"

"$zi2c" --target=plan9-c --root "$work/src" \
    -o "$work/direct" "$work/src/main.zi"
cmp "$work/generated/main.c" "$work/direct/main.c"
cmp "$work/generated/main.h" "$work/direct/main.h"
cmp "$work/generated/zir_plan9_runtime.h" \
    "$work/direct/zir_plan9_runtime.h"
