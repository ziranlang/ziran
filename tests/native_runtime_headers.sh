#!/bin/sh
# Exported ABI headers and standalone output need only the runtime they use.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/native_helpers_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -lm -pthread -o "$work/helpers"
"$work/helpers" "$work/invalid.c"

cat > "$work/plain.zi" <<'ZI'
#program_export
Add :: (a: s32, b: s32) -> s32 { return a + b }
ZI
cat > "$work/types.zi" <<'ZI'
#program_export
Borrow :: (text: string) -> string { return text }
#program_export
Location :: (loc: Source_Code_Location) -> s64 { return loc.line_number }
#program_export
Count :: (words: []string) -> s64 { return words.count }
ZI
cat > "$work/operations.zi" <<'ZI'
#program_export
Same :: (a: string, b: string) -> bool { return a == b }
#program_export
Part :: (text: string, low: s64, high: s64) -> string { return text[low:high] }
#program_export
Show :: () { print("%\n", 0.125) }
ZI
cat > "$work/print_only.zi" <<'ZI'
#program_export
Show :: () { print("%\n", 42) }
ZI
cat > "$work/allocate.zi" <<'ZI'
#program_export
Answer :: () -> s32 {
    value := New(s32)
    <<value = 42
    result := <<value
    free(value)
    return result
}
ZI

for name in plain types operations print_only allocate; do
    "$ziran" ir --root "$work" -o "$work/ir" "$work/$name.zi"
done
for form in source saved; do
    root=$work; suffix=zi
    if test "$form" = saved; then root=$work/ir; suffix=zir; fi
    for target in c cpp; do
        if test "$target" = c; then
            compiler=${CC:-cc}; extension=c; header=h; standard=c11
        else
            compiler=${CXX:-c++}; extension=cpp; header=hpp; standard=c++17
        fi
        for name in plain types operations print_only allocate; do
            output=$work/$form-$target-$name
            "$ziran" build --target="$target" --no-main --root "$root" \
                -o "$output" "$root/$name.$suffix"
            case "$name" in
                plain)
                    test ! -e "$output/zir_string.h"
                    test ! -e "$output/zir_bounds.h"
                    "$compiler" -std="$standard" -M "$output/plain.$extension" > "$work/deps"
                    if rg -q '/(stdio|stdlib|string)\.h\b' "$work/deps"; then
                        echo 'numeric-only output pulled in heavy runtime headers' >&2; exit 1
                    fi
                    body='int main(void) { return Add(19, 23) == 42 ? 0 : 1; }'
                    ;;
                types)
                    body='int main(void) {
                        String text = StringLiteral("a\0z");
                        String words[] = {text};
                        Slice values = {words, 1};
                        Source_Code_Location loc = {text, 42};
                        return Borrow(text).length == 3 && Location(loc) == 42 && Count(values) == 1 ? 0 : 1;
                    }'
                    ;;
                operations)
                    # Full helpers remain usable after the thin generated ABI.
                    body='#include "zir_string.h"
                    int main(void) {
                        String text = StringLiteral("a\0z");
                        if (!Same(text, StringView("a\0z", 3)) || !StringEqual(Part(text, 1, 3), StringView("\0z", 2))) return 1;
                        Show(); return 0;
                    }'
                    ;;
                print_only) body='int main(void) { Show(); return 0; }' ;;
                allocate) body='int main(void) { return Answer() == 42 ? 0 : 1; }' ;;
            esac
            printf '#include "%s.%s"\n%s\n' "$name" "$header" "$body" > "$output/host.$extension"
            "$compiler" -std="$standard" -O2 -Werror \
                -I"$output" "$output"/*."$extension" -o "$output/program"
            "$output/program" > "$work/result"
            case "$name" in
                operations) test "$(cat "$work/result")" = 0.125 ;;
                print_only) test "$(cat "$work/result")" = 42 ;;
            esac
        done
    done
done

# A consumer can include just the full runtime header. Copy its transitive
# dependency too, and retain timestamps when both files are unchanged.
mkdir "$work/copied"
cat > "$work/copy.c" <<'C'
#include "zir_runtime.h"
int main(int argc, char **argv) {
    return argc == 3 && EmitRuntimeHeaders(argv[1], argv[2]) ? 0 : 1;
}
C
cat > "$work/copied/use.c" <<'C'
#include "zir_string.h"
int main(void) { return StringEqual(StringLiteral("a\0z"), StringView("a\0z", 3)) ? 0 : 1; }
C
sh "$repo/scripts/embed_headers.sh" "$repo/include/zir_string.h" \
    "$repo/include/zir_string_types.h" > "$work/embedded.c"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$repo/cmd/zir" \
    "$work/copy.c" "$repo/cmd/zir/zir_runtime.c" "$work/embedded.c" \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -o "$work/copy"
"$work/copy" "$work/copied" "$work/copied/use.c"
python3 - "$work" <<'PY'
import os, pathlib, subprocess, sys
root = pathlib.Path(sys.argv[1]); headers = list((root/'copied').glob('*.h'))
assert {p.name for p in headers} == {'zir_string.h', 'zir_string_types.h'}
for p in headers: os.utime(p, ns=(1000000000, 1000000000))
subprocess.run([root/'copy', root/'copied', root/'copied/use.c'], check=True)
assert all(p.stat().st_mtime_ns == 1000000000 for p in headers)
PY
python3 "$repo/tests/native_c_large_output_test.py" "$ziran" "$work/large-output"
"${CC:-cc}" -std=c11 -Werror=implicit-function-declaration "$work/copied/use.c" -o "$work/copied/program"
"$work/copied/program"
echo 'Standalone native runtime headers: source, saved IR, C and C++ passed'
