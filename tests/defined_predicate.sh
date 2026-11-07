#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/predicate.zi" <<'EOF'
#if #defined(PLATFORM_WEB) || #defined(ANDROID_BUILD) {
PlatformPick :: 20
} else {
PlatformPick :: 2
}

PRESENT :: true;

#if #defined(PRESENT) {
ProbePick :: 100
} else {
ProbePick :: 200
}

#if #defined(NEVER_DECLARED) {
MissingPick :: MissingValue()
} else {
MissingPick :: 7
}

WEB :: #defined(PLATFORM_WEB);

#if WEB {
WebPick :: 1000
} else {
WebPick :: 3
}

#program_export
Answer :: () -> s32 {
    result: s32 = PlatformPick + ProbePick + MissingPick + WebPick
    #if !#defined(PLATFORM_WEB) && !#defined(ANDROID_BUILD) {
        result += 1
    } else {
        result += 1000
    }
    return result
}
EOF

"$ziran" check --root "$work" "$work/predicate.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/predicate.zi"
if grep -aFq 'MissingValue' "$work/ir/predicate.zir"; then
    echo 'undefined probe selected its branch' >&2
    exit 1
fi
"$ziran" bundle --root "$work" --entry predicate:Answer \
    -o "$work/source.zx" "$work/predicate.zi"
"$ziran" bundle --root "$work/ir" --entry predicate:Answer \
    -o "$work/saved.zx" "$work/ir/predicate.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 113
test "$("$ziran" run "$work/saved.zx")" = 113

"$ziran" build --target=c --root "$work" \
    -o "$work/c" "$work/predicate.zi"
cat > "$work/c/main.c" <<'EOF'
#include "predicate.h"
int main(void) { return Answer() == 113 ? 0 : 1; }
EOF
${CC:-cc} -Iinclude -I"$work/c" "$work/c/predicate.c" \
    "$work/c/main.c" -o "$work/c/app"
"$work/c/app"

# A predicate constant may start with a negation or a group.
cat > "$work/negated.zi" <<'EOF'
NATIVE :: !#defined(PLATFORM_WEB) && !#defined(ANDROID_BUILD)
GROUPED :: !(#defined(PLATFORM_WEB) || #defined(ANDROID_BUILD))

#program_export
Answer :: () -> s32 {
    result: s32 = 0
    #if NATIVE {
        result += 1
    }
    #if GROUPED {
        result += 10
    }
    return result
}
EOF
"$ziran" bundle --root "$work" --entry negated:Answer \
    -o "$work/native.zx" "$work/negated.zi"
test "$("$ziran" run "$work/native.zx")" = 11
"$(dirname "$ziran")/zi2c" --no-main --define PLATFORM_WEB --root "$work" \
    -o "$work/web" "$work/negated.zi"
cat > "$work/web/main.c" <<'EOF'
#include "negated.h"
int main(void) { return Answer() == 0 ? 0 : 1; }
EOF
${CC:-cc} -Iinclude -I"$work/web" "$work/web/negated.c" \
    "$work/web/main.c" -o "$work/web/app"
"$work/web/app"

# Probes follow --define in both spellings, alone, negated and combined.
cat > "$work/flags.zi" <<'ZI'
#if #defined(FIRST) {
FirstPick :: 1
} else {
FirstPick :: 2
}

#if !#defined(FIRST) && #defined(SECOND) {
BothPick :: 10
} else #if #defined(FIRST) || #defined(SECOND) {
BothPick :: 20
} else {
BothPick :: 30
}

#program_export
Answer :: () -> s32 {
    result: s32 = FirstPick + BothPick
    #if defined(FIRST) && !defined(SECOND) {
        result += 100
    } else {
        result += 200
    }
    return result
}
ZI
for flags in 232 '121 FIRST' '212 SECOND' '221 FIRST SECOND'; do
    set -- $flags
    expected=$1
    shift
    defines=
    for name in "$@"; do defines="$defines --define $name"; done
    "$ziran" bundle --root "$work" $defines --entry flags:Answer \
        -o "$work/flags.zx" "$work/flags.zi"
    test "$("$ziran" run "$work/flags.zx")" = "$expected"
    rm -rf "$work/flags"
    "$(dirname "$ziran")/zi2c" --no-main --root "$work" $defines \
        -o "$work/flags" "$work/flags.zi"
    cat > "$work/flags/main.c" <<EOM
#include "flags.h"
int main(void) { return Answer() == $expected ? 0 : 1; }
EOM
    ${CC:-cc} -Iinclude -I"$work/flags" "$work/flags/flags.c" \
        "$work/flags/main.c" -o "$work/flags/app"
    "$work/flags/app"
done
