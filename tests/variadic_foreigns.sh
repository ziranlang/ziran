#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/vargs.zi" <<'EOF'
c :: #system_library "c";
LogLine :: (level: s32, format: string, args: ..any) #foreign c "TraceLog";
FormatInto :: (out: *u8, size: s64, format: string, args: ..any) -> s32 #foreign c "snprintf";

#program_export
Answer :: () -> s32 {
    buffer: [16] u8
    LogLine(4, "checked %d and %s", 42, "ok")
    if FormatInto(*buffer[0], 16, "%d", 42) != 2 {
        return 0
    }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/vargs.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/vargs.zi"
if grep -aFq 'argument count mismatch' "$work/ir/vargs.zir"; then
    echo 'variadic call rejected' >&2
    exit 1
fi

cat > "$work/too_few.zi" <<'EOF'
c :: #system_library "c";
LogLine :: (level: s32, format: string, args: ..any) #foreign c "TraceLog";
Answer :: () -> s32 {
    LogLine(4)
    return 42
}
EOF
if "$ziran" check --root "$work" "$work/too_few.zi" \
    2> "$work/too_few.err"; then
    echo 'variadic call missing fixed arguments was accepted' >&2
    exit 1
fi
grep -Fq 'argument count mismatch' "$work/too_few.err"

cat > "$work/linked.zi" <<'EOF'
c :: #system_library "c";
LogLine :: (level: s32, format: *u8, args: ..any) #foreign c "TestLog";

#program_export
Answer :: () -> s32 {
    format: [3]u8
    format[0] = cast(u8)37
    format[1] = cast(u8)100
    format[2] = cast(u8)0
    LogLine(4, *format[0], cast(s32)42)
    return 42
}
EOF
cat > "$work/linked.c" <<'EOF'
#include "linked.h"
#include <assert.h>
#include <stdarg.h>

static int observed;
void TestLog(int32_t level, uint8_t *format, ...)
{
    va_list args;
    va_start(args, format);
    assert(level == 4 && format[0] == '%' && format[1] == 'd');
    observed = va_arg(args, int);
    va_end(args);
}
int main(void)
{
    assert(Answer() == 42);
    assert(observed == 42);
    return 0;
}
EOF
"$(dirname "$ziran")/zi2c" --no-main --root "$work" -o "$work/c" "$work/linked.zi"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$repo/include" -I"$work/c" \
    "$work/c/linked.c" "$work/linked.c" -o "$work/linked"
"$work/linked"
