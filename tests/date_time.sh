#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "date_time"

#program_export
Answer :: () -> s32 {
    local: LocalDateTime = LocalNow()
    if !local.valid || UnixNow() != cast(s64)13574649522 ||
       local.year != 2400 || local.month != 2 ||
       local.day != 29 || local.day_of_year != 59 ||
       local.hour != 23 || local.minute != 58 || local.second != 42 {
        return -1
    }
    return 42
}
ZI
cat > "$work/provider.zi" <<'ZI'
#import "date_time"

#program_export
FakeNow :: () -> LocalDateTime {
    local: LocalDateTime
    local.year = 2400
    local.month = 2
    local.day = 29
    local.day_of_year = 59
    local.hour = 23
    local.minute = 58
    local.second = 42
    local.valid = true
    return local
}

#program_export
FakeUnix :: () -> s64 {
    return cast(s64)13574649522
}
ZI

"$ziran" ir --root "$work" --module-path std -o "$work/ir" \
    "$work/app.zi" "$work/provider.zi"
"$ziran" bundle --root "$work" --module-path std \
    --entry app:Answer --bind date_time:LocalNowHost=provider:FakeNow \
    --bind date_time:UnixNowHost=provider:FakeUnix \
    -o "$work/source.zx" "$work/app.zi" "$work/provider.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer --bind date_time:LocalNowHost=provider:FakeNow \
    --bind date_time:UnixNowHost=provider:FakeUnix \
    -o "$work/saved.zx" "$work/ir/app.zir" "$work/ir/provider.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42
