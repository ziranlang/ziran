#!/bin/sh
# Cross-build the ordinary portable runtime for Android embedding.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
ndk=${ANDROID_NDK_HOME:-${ANDROID_HOME:-$HOME/Android/Sdk}/ndk/28.2.13676358}
tools=$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin
test -x "$tools/llvm-ar"
for abi in ${ZIRAN_ANDROID_ABIS:-arm64-v8a armeabi-v7a}; do
    case "$abi" in
        arm64-v8a) target=aarch64-linux-android28 ;;
        armeabi-v7a) target=armv7a-linux-androideabi28 ;;
        x86_64) target=x86_64-linux-android28 ;;
        x86) target=i686-linux-android28 ;;
        *) echo "Unsupported Android ABI: $abi" >&2; exit 2 ;;
    esac
    make -C "$root" BOOTSTRAP=1 BUILD_DIR="build/android-host/$abi" \
        CC="$tools/$target-clang" AR="$tools/llvm-ar" OBJCOPY="$tools/llvm-objcopy" \
        CFLAGS='-O2 -fPIC -ffunction-sections -fdata-sections' \
        "build/android-host/$abi/libziran.a"
done
