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
// Named zones, run only where tzdata is installed.
#program_export
CheckZones :: () -> s32 {
    output: [64]u8
    if ZoneOffsetAt("Asia/Tokyo", 1700000000).seconds != 32400 || !ZoneOffsetAt("Asia/Tokyo", 1700000000).valid { return 12 }
    if ZoneOffsetAt("Asia/Kolkata", 1700000000).seconds != 19800 || ZoneOffsetAt("Asia/Kathmandu", 1700000000).seconds != 20700 { return 13 }
    if ZoneOffsetAt("America/New_York", 1700000000).seconds != -18000 ||
        ZoneOffsetAt("America/New_York", 1689000000).seconds != -14400 || !ZoneOffsetAt("UTC", 0).valid { return 14 }
    if ZoneOffsetAt("Not/AZone", 0).valid || ZoneOffsetAt("../etc/passwd", 0).valid || ZoneOffsetAt("", 0).valid ||
        ZoneOffsetAt("/etc/localtime", 0).valid { return 15 }
    // The process's own TZ is back: the harness rule, winter -5h.
    if FormatDateTime(LocalAt(0), "%F %T", output[:]) != "1969-12-31 19:00:00" { return 16 }
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
#include "time_test.h"
#include "date_time_linux.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

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
    if (access("/usr/share/zoneinfo/Asia/Tokyo", F_OK) == 0) {
        result = CheckZones();
        if (result != 0) return result;
    } else {
        fputs("tzdata unavailable; named zone checks skipped\n", stderr);
    }
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
    out=$work/$input-c
    "$ziran" build --target=c --no-main --root "$root" --module-path "$repo/std" -o "$out" "$file" "$provider"
    cp "$work/entry.c" "$out/entry.c"
    cc -std=c11 -I"$out" "$out"/*.c -lm -pthread -o "$work/program"
    TZ='EST5EDT,M3.2.0/2,M11.1.0/2' "$work/program"
done
# ClockValue is struct timespec and the seconds are time_t: both are
# long-sized, so 32-bit on armeabi-v7a, where 64-bit fields read garbage.
cat > "$work/layout.c" <<'C'
#define ZIRAN_DATE_TIME_LINUX_H_PRIVATE 1
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "date_time_linux.h"
_Static_assert(sizeof(ClockValue) == sizeof(struct timespec), "ClockValue size");
_Static_assert(offsetof(ClockValue, nanoseconds) == offsetof(struct timespec, tv_nsec), "ClockValue nanoseconds");
_Static_assert(sizeof(intptr_t) == sizeof(time_t), "time_t is pointer-sized");
_Static_assert(offsetof(TimeFields, utc_offset) == offsetof(struct tm, tm_gmtoff), "struct tm offset");
_Static_assert(sizeof(TimeFields) == sizeof(struct tm), "struct tm size");
C
layout=$work/source-c
"${CC:-cc}" -std=c11 -D_DEFAULT_SOURCE -I"$repo/include" -I"$layout" -c "$work/layout.c" -o "$work/layout.o"
if test -n "${ZIRAN_THREAD_ANDROID_NDK:-}"; then
    bin="$ZIRAN_THREAD_ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
    for triple in armv7a-linux-androideabi24 aarch64-linux-android24; do
        "$bin/$triple-clang" -std=c11 -I"$repo/include" -I"$layout" -c "$work/layout.c" -o "$work/layout-$triple.o"
    done
fi
# Emscripten's time_t is 64-bit on wasm32 while long stays 32-bit. A 32-bit
# time() binding there is a signature mismatch that traps on the first call,
# so link with fatal linker warnings and run the clocks under node.
emcc=${EMCC:-emcc}
if ! command -v "$emcc" >/dev/null 2>&1 && test -x "$HOME/emsdk/upstream/emscripten/emcc"; then
    emcc=$HOME/emsdk/upstream/emscripten/emcc
fi
if command -v "$emcc" >/dev/null 2>&1 && command -v node >/dev/null 2>&1; then
    cat > "$work/web_time.zi" <<'ZI'
#import "date_time_linux"
#import "timer_linux"
#import "calendar"
#program_export
CheckWebTime :: () -> s32 {
    output: [64]u8
    if FormatDateTime(UtcAt(0), "%F %T", output[:]) != "1970-01-01 00:00:00" ||
        FormatDateTime(UtcAt(-1), "%F %T", output[:]) != "1969-12-31 23:59:59" ||
        FormatDateTime(UtcAt(cast(s64)2147483648), "%F %T", output[:]) != "2038-01-19 03:14:08" { return 1 }
    if UnixNow() < 1700000000 || !DateTimeValid(LocalNow()) || !DateTimeValid(LocalAt(UnixNow())) { return 2 }
    if LocalAt(cast(s64)9223372036854775807).valid { return 3 }
    before := UnixNow()
    milliseconds := UnixMilliseconds()
    after := UnixNow()
    if milliseconds < (before - 1) * 1000 || milliseconds >= (after + 2) * 1000 { return 4 }
    if ZoneOffsetAt("Asia/Tokyo", 1700000000).seconds != 32400 || !ZoneOffsetAt("Asia/Tokyo", 1700000000).valid { return 8 }
    if ZoneOffsetAt("Asia/Kolkata", 1700000000).seconds != 19800 || ZoneOffsetAt("Asia/Kathmandu", 1700000000).seconds != 20700 { return 9 }
    if ZoneOffsetAt("America/New_York", 1700000000).seconds != -18000 ||
        ZoneOffsetAt("America/New_York", 1689000000).seconds != -14400 || !ZoneOffsetAt("UTC", 0).valid { return 10 }
    if ZoneOffsetAt("Not/AZone", 0).valid || ZoneOffsetAt("../etc/passwd", 0).valid { return 11 }
    // Monotonic milliseconds keep sub-second precision.
    first := MonotonicMilliseconds()
    if first < 0 { return 5 }
    tries: s32 = 0
    while tries < 2000000 {
        now := MonotonicMilliseconds()
        if now < first { return 6 }
        if now % 1000 != 0 && now != first { return 0 }
        tries += 1
    }
    return 7
}
ZI
    # Each module's private timespec and struct tm layouts, checked against
    # Emscripten's time.h in a build that imports only that module.
    printf '#import "date_time_linux"\n#program_export\nWebDateTimeLayout :: () -> s64 { return UnixNow() }\n' > "$work/web_layout_date.zi"
    printf '#import "timer_linux"\n#program_export\nWebTimerLayout :: () -> s64 { return MonotonicMilliseconds() }\n' > "$work/web_layout_timer.zi"
    cat > "$work/web_layout_date.c" <<'C'
#define _DEFAULT_SOURCE 1
#define ZIRAN_DATE_TIME_LINUX_H_PRIVATE 1
#include <stddef.h>
#include <time.h>
#include "date_time_linux.h"
_Static_assert(sizeof(time_t) == 8, "Emscripten time_t is 64-bit");
_Static_assert(sizeof(ClockValue) == sizeof(struct timespec), "ClockValue size");
_Static_assert(offsetof(ClockValue, nanoseconds) == offsetof(struct timespec, tv_nsec), "ClockValue nanoseconds");
_Static_assert(offsetof(TimeFields, utc_offset) == offsetof(struct tm, tm_gmtoff), "struct tm offset");
_Static_assert(sizeof(TimeFields) == sizeof(struct tm), "struct tm size");
C
    cat > "$work/web_layout_timer.c" <<'C'
#define _DEFAULT_SOURCE 1
#define ZIRAN_TIMER_LINUX_H_PRIVATE 1
#include <stddef.h>
#include <time.h>
#include "timer_linux.h"
_Static_assert(sizeof(ClockValue) == sizeof(struct timespec), "timer ClockValue size");
_Static_assert(offsetof(ClockValue, nanoseconds) == offsetof(struct timespec, tv_nsec), "timer ClockValue nanoseconds");
C
    for module in date timer; do
        "$ziran" build --target=c --define PLATFORM_WEB --no-main --root "$work" \
            --module-path "$repo/std" -o "$work/web-layout-$module" "$work/web_layout_$module.zi"
        "$emcc" -std=c11 -I"$repo/include" -I"$work/web-layout-$module" -c "$work/web_layout_$module.c" \
            -o "$work/web-layout-$module.o"
    done
    cat > "$work/web_entry.c" <<'C'
#define _DEFAULT_SOURCE 1
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include "web_time.h"
#include "date_time_linux.h"
int main(void)
{
    int result = CheckWebTime();
    if (result != 0) { fprintf(stderr, "web time check %d failed\n", result); return result; }
    const time_t cases[] = {0, -1, 951782400, 1593604800, 2147483648LL, 4107542400LL, 13574649522LL};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        struct tm local, utc;
        if (localtime_r(&cases[i], &local) == NULL || gmtime_r(&cases[i], &utc) == NULL) return 20;
        LocalDateTime got = LocalAt(cases[i]);
        LocalDateTime want = UtcAt(cases[i]);
        if (!got.valid || got.year != local.tm_year + 1900 || got.month != local.tm_mon + 1 ||
            got.day != local.tm_mday || got.hour != local.tm_hour || got.minute != local.tm_min ||
            got.second != local.tm_sec) return 21;
        if (!want.valid || want.year != utc.tm_year + 1900 || want.day != utc.tm_mday ||
            want.hour != utc.tm_hour || want.second != utc.tm_sec) return 22;
    }
    return 0;
}
C
    out=$work/web-c
    "$ziran" build --target=c --define PLATFORM_WEB --no-main --root "$work" \
        --module-path "$repo/std" -o "$out" "$work/web_time.zi"
    compiler=$emcc; extension=c; standard=c11
    cp "$work/web_entry.c" "$out/web_entry.$extension"
    "$compiler" -std=$standard -I"$repo/include" -I"$out" \
        "$out"/*."$extension" --js-library "$repo/web/ziran_web.js" -Wl,--fatal-warnings \
        -sENVIRONMENT=node -sEXIT_RUNTIME=1 -o "$out/test.js"
    node "$out/test.js"
    printf 'web date-time checks passed: 64-bit time_t, timespec and struct tm layouts, libc agreement, named zones through Intl, sub-second monotonic clock\n'
else
    printf 'Emscripten or node unavailable; web date-time checks skipped.\n'
fi
echo 'date-time-linux-test-ok: UTC, DST, post-2038, named zones, failed conversions, concurrent readings, portable host, C source and saved IR'
