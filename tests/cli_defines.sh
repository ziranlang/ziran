#!/bin/sh
set -eu

tool_dir=$(dirname "$1")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/main.zi" <<'EOF'
#import, file "platform.zi";
#program_export
Answer :: () -> s32 {
    return PlatformValue()
}
EOF
cat > "$work/platform.zi" <<'EOF'
ANDROID :: #defined(ANDROID_BUILD)
WEB :: #defined(PLATFORM_WEB)
#if ANDROID {
PlatformValue :: () -> s32 { return 17 }
} else {
    #if WEB {
    PlatformValue :: () -> s32 { return 31 }
    } else {
    PlatformValue :: () -> s32 { return 29 }
    }
}
EOF

for platform in desktop android web; do
    case "$platform" in
        desktop) expected=29; set -- ;;
        android) expected=17; set -- --define ANDROID_BUILD ;;
        web) expected=31; set -- --define PLATFORM_WEB ;;
    esac
    "$tool_dir/zi2zir" --check-only --root "$work" "$@" \
        "$work/main.zi"
    "$tool_dir/zi2c" --no-main --root "$work" "$@" \
        -o "$work/$platform" "$work/main.zi"
    if ! grep -Eq "return $expected;" "$work/$platform/platform.c"; then
        echo "wrong $platform branch in generated C" >&2
        grep -n -A 8 PlatformValue "$work/$platform/platform.c" >&2
        exit 1
    fi
    "$tool_dir/zi2zir" --root "$work" "$@" -o "$work/ir-$platform" "$work/main.zi"
    for form in source saved; do
        module_root="$work"
        input="$work/main.zi"
        if test "$form" = saved; then
            module_root="$work/ir-$platform"
            input="$module_root/main.zir"
        fi
        output="$work/c-$platform-$form"
        "$tool_dir/zi2c" --no-main --root "$module_root" "$@" -o "$output" "$input"
        cat > "$output/caller.c" <<EOF
#include "main.h"
int main(void) { return Answer() == $expected ? 0 : 1; }
EOF
        "${CC:-cc}" -std=c11 -O1 -I"$output" "$output"/*.c -o "$output/test"
        "$output/test"
        "$tool_dir/zi2zx" bundle --root "$module_root" "$@" \
            --entry main:Answer -o "$work/$platform-$form.zx" "$input"
        test "$("$tool_dir/zi2zx" run "$work/$platform-$form.zx")" = "$expected"
    done
    cmp "$work/$platform-source.zx" "$work/$platform-saved.zx"
done

if "$tool_dir/zi2zir" --check-only --root "$work" \
    --define 'ANDROID-BUILD' "$work/main.zi" >/dev/null 2>&1; then
    echo 'accepted an invalid compiler definition' >&2
    exit 1
fi

for name in 'ANDROID-BUILD' '1ANDROID' ''; do
    if "$tool_dir/zi2zx" bundle --root "$work" --entry main:Answer \
        -o "$work/invalid.zx" --define "$name" "$work/main.zi" >/dev/null 2>&1; then
        echo 'Zx accepted an invalid compiler definition' >&2
        exit 1
    fi
    if "$tool_dir/zi2c" --no-main --root "$work" -o "$work/invalid" \
        --define "$name" "$work/main.zi" >/dev/null 2>&1; then
        echo 'C accepted an invalid compiler definition' >&2
        exit 1
    fi
done
if "$tool_dir/zi2zx" bundle --root "$work" --entry main:Answer \
    -o "$work/invalid.zx" --define >/dev/null 2>&1; then
    echo 'Zx accepted --define without a name' >&2
    exit 1
fi
if "$tool_dir/zi2c" --no-main --root "$work" -o "$work/invalid" \
    --define >/dev/null 2>&1; then
    echo 'C accepted --define without a name' >&2
    exit 1
fi
echo 'Compiler definitions: C generation and C/Zx source/saved-IR execution passed'
