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
Portable :: #import "date_time";
#program_export
CheckTime :: () -> s32 {
    output: [64]u8
    if FormatDateTime(UtcAt(0), "%F %T", output[:]) != "1970-01-01 00:00:00" ||
        FormatDateTime(UtcAt(-1), "%F %T", output[:]) != "1969-12-31 23:59:59" ||
        FormatDateTime(UtcAt(951782400), "%F %j", output[:]) != "2000-02-29 060" { return 1 }
    // POSIX timezone rules passed by the harness: winter -5h, summer -4h.
    if FormatDateTime(LocalAt(0), "%F %T", output[:]) != "1969-12-31 19:00:00" ||
        FormatDateTime(LocalAt(1593604800), "%F %T", output[:]) != "2020-07-01 08:00:00" { return 2 }
    if UnixNow() < 1700000000 || !DateTimeValid(LocalNow()) { return 3 }
    if Portable.UnixNow() < 1700000000 || !DateTimeValid(Portable.LocalNow()) { return 5 }
    if FormatDateTime(UtcAt(cast(s64)2147483648), "%F %T", output[:]) != "2038-01-19 03:14:08" { return 6 }
    if LocalAt(cast(s64)9223372036854775807).valid ||
        UtcAt(cast(s64)9223372036854775807).valid { return 7 }
    before := UnixNow()
    milliseconds := UnixMilliseconds()
    after := UnixNow()
    // The libc seconds clock can lag the precise clock at a second boundary.
    if milliseconds < (before - 1) * 1000 || milliseconds >= (after + 2) * 1000 { return 4 }
    return 0
}
ZI
cat > "$work/provider.zi" <<'ZI'
#import "date_time_types"
Native :: #import "date_time_linux";

#program_export
LocalNowHost :: () -> LocalDateTime { return Native.LocalNow() }

#program_export
UnixNowHost :: () -> s64 { return Native.UnixNow() }
ZI
cat > "$work/entry.c" <<'C'
#define _POSIX_C_SOURCE 200809L
#ifdef __cplusplus
#include "time_test.hpp"
#include "date_time_linux.hpp"
#else
#include "time_test.h"
#include "date_time_linux.h"
#endif
#include <pthread.h>
#include <stdio.h>
#include <time.h>

struct Case {
    time_t seconds;
    struct tm local;
    struct tm utc;
    int failed;
};

static int same_time(LocalDateTime got, const struct tm *expected)
{
    return got.valid && got.year == expected->tm_year + 1900 &&
        got.month == expected->tm_mon + 1 && got.day == expected->tm_mday &&
        got.day_of_year == expected->tm_yday && got.hour == expected->tm_hour &&
        got.minute == expected->tm_min && got.second == expected->tm_sec;
}

static void *read_clocks(void *input)
{
    struct Case *test = (struct Case *)input;
    for (int i = 0; i < 4000; ++i) {
        if (!same_time(LocalAt(test->seconds), &test->local) ||
            !same_time(UtcAt(test->seconds), &test->utc)) {
            test->failed = 1;
            break;
        }
    }
    return NULL;
}

int main(void)
{
    int result = CheckTime();
    if (result != 0) return result;
    const time_t seconds[] = {0, -1, 951782400, 1593604800,
        2147483648LL, 4107542400LL, 13574649522LL, 1699165800};
    struct Case cases[8] = {0};
    pthread_t threads[8];
    for (int i = 0; i < 8; ++i) {
        cases[i].seconds = seconds[i];
        if (localtime_r(&cases[i].seconds, &cases[i].local) == NULL ||
            gmtime_r(&cases[i].seconds, &cases[i].utc) == NULL) return 8;
    }
    for (int i = 0; i < 8; ++i) {
        if (pthread_create(&threads[i], NULL, read_clocks, &cases[i]) != 0) return 9;
    }
    for (int i = 0; i < 8; ++i) {
        if (pthread_join(threads[i], NULL) != 0) return 10;
        if (cases[i].failed) {
            fputs("concurrent clock readings changed civil fields\n", stderr);
            return 11;
        }
    }
    return 0;
}
C
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/time_test.zi" "$work/provider.zi"
for input in source saved; do
    root=$work
    file=$root/time_test.zi
    provider=$root/provider.zi
    if test "$input" = saved; then root=$work/ir; file=$root/time_test.zir; provider=$root/provider.zir; fi
    for target in c cpp; do
        out=$work/$input-$target
        "$ziran" build --target="$target" --no-main --root "$root" --module-path "$repo/std" -o "$out" "$file" "$provider"
        if test "$target" = c; then
            cp "$work/entry.c" "$out/entry.c"
            cc -std=c11 -I"$out" "$out"/*.c -lm -pthread -o "$work/program"
        else
            cp "$work/entry.c" "$out/entry.cpp"
            c++ -std=c++17 -I"$out" "$out"/*.cpp -lm -pthread -o "$work/program"
        fi
        TZ='EST5EDT,M3.2.0/2,M11.1.0/2' "$work/program"
    done
done
echo 'date-time-linux-test-ok: UTC, DST, post-2038, failed conversions, concurrent readings, portable host, C/C++ source and saved IR'
