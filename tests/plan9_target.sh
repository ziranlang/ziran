#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
zi2c="${ziran%/*}/zi2c"
work=$(mktemp -d "$repo/build/test/plan9_target.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

mkdir "$work/src"
cat > "$work/src/main.zi" <<'EOF'
Inner :: struct { value: s32 }
Props :: struct { inner: Inner; omitted: s32; scale: s32 }
Make :: () -> Props { return .{inner = .{value = 40}, scale = 2} }
Read :: (props: Props) -> s32 { return props.inner.value + props.scale }
foreign_libc :: #system_library "libc";
ForeignClose :: (descriptor: s32) -> s32 #foreign foreign_libc "close";
#program_export
main :: () -> s32 {
    wide: u64 = cast(u64)18446744073709551615
    signed_max: s64 = 9223372036854775807
    signed_min: s64 = -9223372036854775808
    if (wide >> cast(u64)32) != cast(u64)4294967295 ||
        signed_max / 2147483647 != 4294967298 ||
        signed_min + signed_max != -1 { return 2 }
    local := Make()
    partial: Props = .{scale = 2, inner = .{value = 40}}
    bytes: [3]u8 = .[97, 98, 99]
    text: string = "abc"
    part: string = text[0:2]
    if Read(local) != 42 || local.omitted != 0 || Read(partial) != 42 ||
        partial.omitted != 0 || ForeignClose(-1) != -1 || text.count != 3 ||
        text[0] != bytes[0] || part != "ab" { return 1 }
    return 0
}
EOF

"$ziran" build --target=plan9-c --root "$work/src" \
    -o "$work/generated" "$work/src/main.zi"

test -f "$work/generated/main.c"
test -f "$work/generated/zir_plan9_runtime.h"
rg -q -F '18446744073709551615ULL' "$work/generated/main.c"
rg -q -F '9223372036854775807LL' "$work/generated/main.c"
rg -q -F '4294967298LL' "$work/generated/main.c"

# The source includes its generated header. Native 8c rejects repeated
# macro definitions even when their replacement text is identical.
cat > "$work/src/constants.zi" <<'EOF'
PublicValue :: 40;
#scope_file
PrivateValue :: 2;
#scope_export
#program_export
ConstantAnswer :: () -> s32 { return PublicValue + PrivateValue }
EOF
"$ziran" build --target=plan9-c --root "$work/src" \
    -o "$work/constants" "$work/src/constants.zi"
rg -q '^#define PublicValue 40$' "$work/constants/constants.h"
rg -q '^#define .*PrivateValue 2$' "$work/constants/constants.h"
if rg -q '^#define .*Value ' "$work/constants/constants.c"; then
    echo 'plan9-c source repeats constants from its own header' >&2
    exit 1
fi
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
if rg -n 'static inline|__auto_type|\{\s*\.|for\s*\(\s*(int|s32|u32)' \
        "$work/generated"/*.c "$work/generated"/*.h; then
    echo 'plan9-c output retained unsupported C constructs' >&2
    exit 1
fi
if rg -n '__asm__' "$work/generated"/*.c "$work/generated"/*.h; then
    echo 'plan9-c retained unsupported assembler-name syntax' >&2
    exit 1
fi
rg -q -F '    return close(descriptor);' "$work/generated/main.h"

rg -q '^int32_t ziran_plan9_main\(void\);$' "$work/generated/main.h"
rg -q '^void main\(void\);$' "$work/generated/main.h"
rg -q '^ziran_plan9_main\(void\)$' "$work/generated/main.c"
rg -q -F '    exits(status_text);' "$work/generated/main.c"

if rg -n 'plan9-c' "$work/generated"/*.c "$work/generated"/*.h; then
    echo 'plan9-c leaked dispatcher metadata into generated C' >&2
    exit 1
fi

mkdir -p "$work/plan9-include"
cat > "$work/plan9-include/u.h" <<'EOF'
#ifdef FAKE_U_H
#error native u.h was included twice
#endif
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
extern int close(int);
extern void exits(const char *);
extern void abort(void);
#endif
EOF

# Argument-bearing entries need the same native status conversion as main().
# Parameter names are arbitrary; the wrapper passes the actual argc/argv.
cat > "$work/src/arguments.zi" <<'EOF'
#program_export
main :: (count: s32, words: **s8) -> s32 {
    if count == 2 && words[1][0] == 102 { return 7 }
    if count != 3 { return 8 }
    expected := "space's $value; document"
    index: s64 = 0
    while index < expected.count {
        if words[2][index] != cast(s8)expected[index] { return 9 }
        index += 1
    }
    if words[2][index] != 0 { return 10 }
    return 0
}
EOF
"$ziran" ir --root "$work/src" -o "$work/arguments-ir" "$work/src/arguments.zi"
cat > "$work/arguments-host.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "arguments.h"
void exits(const char *status) { exit(status ? atoi(status) : 0); }
int fprint(int descriptor, const char *format, ...) {
    va_list args;
    int result;
    va_start(args, format);
    result = vfprintf(descriptor == 2 ? stderr : stdout, format, args);
    va_end(args);
    return result;
}
int snprint(char *text, int size, const char *format, ...) {
    va_list args;
    int result;
    va_start(args, format);
    result = vsnprintf(text, size, format, args);
    va_end(args);
    return result;
}
#undef main
int main(int argc, char **argv) { NativeEntry(argc, argv); return 99; }
EOF
for input in source saved; do
    root=$work/src
    entry=$work/src/arguments.zi
    if test "$input" = saved; then root=$work/arguments-ir; entry=$root/arguments.zir; fi
    output=$work/arguments-$input
    "$ziran" build --target=plan9-c --root "$root" -o "$output" "$entry"
    rg -q '^void main\(int argc, char\*\* argv\);$' "$output/arguments.h"
    rg -q -F 'status = ziran_plan9_main((int32_t)argc, (int8_t**)argv);' "$output/arguments.c"
    "${CC:-cc}" -std=c99 -Dmain=NativeEntry -I"$work/plan9-include" -I"$output" \
        "$output"/arguments.c "$work/arguments-host.c" -o "$output/run"
    "$output/run" good 'space'"'"'s $value; document'
    status=0
    "$output/run" fail || status=$?
    test "$status" = 7
done
cmp "$work/arguments-source/arguments.h" "$work/arguments-saved/arguments.h"

cat > "$work/src/helper_first.zi" <<'EOF'
#program_export
HelperFirst :: (value: float) -> s32 { return cast(s32)(value + 0.5) }
EOF
cat > "$work/src/helper_second.zi" <<'EOF'
#import "helper_first"
#program_export
HelperSecond :: (value: float) -> s32 {
    return HelperFirst(value) + cast(s32)(value * 2.0)
}
EOF
cat > "$work/src/helper_main.zi" <<'EOF'
#import "helper_second"
#program_export
HelperMain :: () -> s32 { return HelperSecond(20.0) }
EOF
"$ziran" build --target=plan9-c --root "$work/src" \
    -o "$work/generated-helpers" "$work/src/helper_main.zi"
for module in helper_first helper_second; do
    rg -q '^static int64_t SignedBits\(' "$work/generated-helpers/$module.c"
    rg -q '^static uint64_t FloatToInt\(' "$work/generated-helpers/$module.c"
done
if rg -n '^(int64_t|uint64_t) (SignedBits|FloatToInt)\(' \
        "$work/generated-helpers"/*.c; then
    echo 'plan9-c lowered inline helpers to duplicate external definitions' >&2
    exit 1
fi
cat > "$work/helper-runner.c" <<'EOF'
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <u.h>
#include <libc.h>
#define ZIR_PLAN9_NATIVE_HEADERS_INCLUDED 1
#include "helper_main.h"
int fprint(int fd, const char *format, ...) {
    (void)format;
    return write(fd, "plan9 helper failed\n", 20);
}
int main(void) {
    int result = HelperMain();
    printf("%d\n", result);
    return result == 60 ? 0 : 1;
}
EOF
"${CC:-cc}" -std=c11 -I"$work/plan9-include" \
    -I"$work/generated-helpers" "$work/generated-helpers"/*.c \
    "$work/helper-runner.c" -o "$work/helper-runner"
test "$("$work/helper-runner")" = 60

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
rg -q '^int32_t ziran_plan9_main\(void\);' \
    "$work/generated-status/status_main.h"
if rg -n '^#include <string\.h>' "$work/generated-status"/*.c; then
    echo 'plan9-c c_string output retained hosted string header' >&2
    exit 1
fi
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

# File bindings and implementation-only module aliases are part of the
# standard Plan 9 output gate. Native execution runs in the Taiji guest.
sh "$repo/tests/file_plan9.sh" "$ziran"
sh "$repo/tests/process_plan9.sh" "$ziran"
sh "$repo/tests/date_time_plan9.sh" "$ziran"
sh "$repo/tests/private_module_imports.sh" "$ziran"

# Odd-sized local byte arrays must be zeroed without touching live records.
"$ziran" ir --root "$repo/tests/spec" -o "$work/zero-ir" \
    "$repo/tests/spec/plan9_wide_compare_test.zi"
for form in source saved; do
    root=$repo/tests/spec
    input=$root/plan9_wide_compare_test.zi
    if test "$form" = saved; then root=$work/zero-ir; input=$root/plan9_wide_compare_test.zir; fi
    output=$work/zero-$form
    "$ziran" build --target=plan9-c --root "$root" -o "$output" "$input"
    rg -q -F 'memset(bytes, 0, sizeof(bytes));' "$output/plan9_wide_compare_test.c"
    if rg -q 'bytes\[19\] = \{0\}' "$output/plan9_wide_compare_test.c"; then
        echo 'plan9-c retained the unsafe partial local array initializer' >&2
        exit 1
    fi
    "${CC:-cc}" -std=c11 -Dprint=printf -include stdio.h -I"$work/plan9-include" -I"$output" \
        "$output"/*.c "$work/fake-plan9-exits.c" -o "$output/run"
    "$output/run"
done
