#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cat > "$work/time_test.zi" <<'ZI'
#import "date_time_linux"
#import "calendar"
#program_export
main :: () -> s32 {
    output: [64]u8
    if FormatDateTime(UtcAt(0), "%F %T", output[:]) != "1970-01-01 00:00:00" ||
        FormatDateTime(UtcAt(-1), "%F %T", output[:]) != "1969-12-31 23:59:59" ||
        FormatDateTime(UtcAt(951782400), "%F %j", output[:]) != "2000-02-29 060" { return 1 }
    // POSIX timezone rules passed by the harness: winter -5h, summer -4h.
    if FormatDateTime(LocalAt(0), "%F %T", output[:]) != "1969-12-31 19:00:00" ||
        FormatDateTime(LocalAt(1593604800), "%F %T", output[:]) != "2020-07-01 08:00:00" { return 2 }
    if UnixNow() < 1700000000 || !DateTimeValid(LocalNow()) { return 3 }
    before := UnixNow()
    milliseconds := UnixMilliseconds()
    after := UnixNow()
    // The libc seconds clock can lag the precise clock at a second boundary.
    if milliseconds < (before - 1) * 1000 || milliseconds >= (after + 2) * 1000 { return 4 }
    return 0
}
ZI
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/time_test.zi"
for input in source saved; do
    root=$work
    file=$root/time_test.zi
    if test "$input" = saved; then root=$work/ir; file=$root/time_test.zir; fi
    for target in c cpp; do
        out=$work/$input-$target
        "$ziran" build --target="$target" --root "$root" --module-path "$repo/std" -o "$out" "$file"
        if test "$target" = c; then
            cc -std=c11 -I"$out" "$out"/*.c -lm -o "$work/program"
        else
            c++ -std=c++17 -I"$out" "$out"/*.cpp -lm -o "$work/program"
        fi
        TZ='EST5EDT,M3.2.0/2,M11.1.0/2' "$work/program"
    done
done
echo 'date-time-linux-test-ok: UTC, local timezone, DST, source and saved IR'
