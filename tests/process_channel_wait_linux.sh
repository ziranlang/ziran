#!/bin/sh
set -eu
ziran=${1:-ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=${PROCESS_CHANNEL_WAIT_OUTPUT:-$(mktemp -d)}
if test -z "${PROCESS_CHANNEL_WAIT_OUTPUT:-}"; then trap 'rm -rf "$work"' EXIT HUP INT TERM; fi
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"${CC:-cc}" -O2 -c "$root/tests/process_channel_wait_linux.c" -o "$work/fixture.o"
entry=process_channel_wait_linux
"$ziran" ir --root "$root/tests/spec" --module-path "$root/std" -o "$work/ir" "$root/tests/spec/$entry.zi"
for form in source saved; do
    input="$root/tests/spec/$entry.zi"; modules="$root/std"; source_root="$root/tests/spec"
    if test "$form" = saved; then input="$work/ir/$entry.zir"; modules="$work/ir"; source_root="$work/ir"; fi
    for target in c cpp; do
        output="$work/$form-$target"
        "$ziran" build --target="$target" --no-main --root "$source_root" --module-path "$modules" -o "$output" "$input"
        if test "$target" = c; then
            printf '#include "%s.h"\nint main(void) { return Main(); }\n' "$entry" > "$output/driver.c"
            "${CC:-cc}" -O2 -std=c11 -I"$root/include" -I"$output" "$output"/*.c "$work/fixture.o" \
                -Wl,--wrap=syscall -Wl,--wrap=poll -Wl,--wrap=clock_nanosleep -lm -o "$output/probe"
        else
            printf '#include "%s.hpp"\nint main() { return Main(); }\n' "$entry" > "$output/driver.cpp"
            "${CXX:-c++}" -O2 -std=c++17 -I"$root/include" -I"$output" "$output"/*.cpp "$work/fixture.o" \
                -Wl,--wrap=syscall -Wl,--wrap=poll -Wl,--wrap=clock_nanosleep -lm -o "$output/probe"
        fi
        "$output/probe" > "$work/$form-$target.jsonl"
        cat "$work/$form-$target.jsonl"
    done
done
