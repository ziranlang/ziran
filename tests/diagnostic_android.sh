#!/bin/sh
# Exercise the real diagnostic writer with Android's log sink stubbed locally.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/work"
work=$(mktemp -d "$root/work/diagnostic-android.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir "$work/android"
cat > "$work/android/log.h" <<'HEADER'
#define ANDROID_LOG_WARN 5
#define ANDROID_LOG_ERROR 6
int __android_log_print(int priority, const char *tag, const char *format, ...);
HEADER
cat > "$work/test.c" <<'C'
#define ZIR_MEMORY_IMPLEMENTATION
#include "zir_diagnostic.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int calls;
static char message[1024];
void *AllocateOrExit(size_t size) { void *p = malloc(size); assert(p); return p; }
void CompilerFree(void *p) { free(p); }
const char *SpanPath(ZirSourceSpan span) { (void)span; return "fixture.zi"; }
int __android_log_print(int priority, const char *tag, const char *format, ...) {
    assert(!strcmp(tag, "Ziran"));
    assert(priority == (calls == 1 ? 5 : 6));
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    calls++;
    return 0;
}
int main(void) {
    ZirSourceSpan span = {.line = 7, .column = 3};
    assert(SetDiagnosticFormat("text"));
    Diagnostic(span, "zx.runtime", "portable execution failed in %s", "Frame");
#ifdef __ANDROID__
    assert(calls == 1);
    assert(!strcmp(message, "fixture.zi:7:3: portable execution failed in Frame [zx.runtime]"));
#else
    assert(calls == 0);
#endif
    Warning(span, "fixture.warning", "warning %d", 42);
    assert(SetDiagnosticFormat("json"));
    Diagnostic(span, "zx.runtime", "second failure");
#ifdef __ANDROID__
    assert(calls == 3);
    assert(strstr(message, "second failure [zx.runtime]"));
#else
    assert(calls == 0);
#endif
    return 0;
}
C
for target in native android; do
    if [ "$target" = android ]; then
        set -- -D__ANDROID__ -I"$work"
    else
        set --
    fi
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror "$@" -I"$root/cmd/zir" \
        "$root/cmd/zir/zir_diagnostic.c" "$work/test.c" -o "$work/test-$target"
    "$work/test-$target" 2> "$work/$target.stderr"
done
cmp "$work/native.stderr" "$work/android.stderr"
python3 - "$work/native.stderr" <<'PY'
import json,sys
from pathlib import Path
lines=Path(sys.argv[1]).read_text().splitlines()
assert lines[:2] == ['fixture.zi:7:3: portable execution failed in Frame',
                    'fixture.zi:7:3: warning: warning 42'], lines
assert json.loads(lines[2])['message'] == 'second failure', lines
PY
echo 'local diagnostic test: Android logcat receives errors/warnings; native stderr text and JSON preserved'
