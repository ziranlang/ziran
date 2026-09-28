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
done

if "$tool_dir/zi2zir" --check-only --root "$work" \
    --define 'ANDROID-BUILD' "$work/main.zi" >/dev/null 2>&1; then
    echo 'accepted an invalid compiler definition' >&2
    exit 1
fi
