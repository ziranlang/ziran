#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 "$repo/tests/native_build_parallel_test.py" "$ziran" "$work"
mkdir "$work/owned"
wrap=
if test "$(uname -s)" = Linux; then wrap=-Wl,--wrap=syscall,--wrap=posix_spawnp; fi
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/include" -I"$repo/cmd/zir" -I"$repo/cmd/zir-c" \
    "$repo/tests/native_build_children_test.c" "$repo/cmd/zir-c/zir_c_build.c" "$repo/cmd/zir-c/zir_c_cache.c" \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -pthread $wrap -o "$work/children"
ZIRAN_PROFILE="$work/children.jsonl" "$work/children" "$work/owned"
python3 - "$work/children.jsonl" <<'PY'
import json,sys
sample,=map(json.loads,open(sys.argv[1]))
assert sample['counters'].get('native_process_event',0)==0
assert sample['counters']['native_process_poll']==2
if sys.platform.startswith('linux'):
    assert sample['counters'].get('native_process_spawn',0)==0
    assert sample['counters']['native_process_fork']==3
PY

# The host compiler also needs its existing Android API 21 launch path.
# Check both sides of Bionic's API 28 spawn boundary when an NDK is available.
ndk=${ANDROID_NDK_HOME:-${ZIRAN_THREAD_ANDROID_NDK:-}}
if test -n "$ndk" && test -x "$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm"; then
    ndk_bin=$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin
    cat > "$work/spawn.c" <<'C'
#include "zir_c_process.h"
#include <sys/wait.h>
void ProfileCount(const char *name) { (void)name; }
int main(void) {
    char *args[] = {"/system/bin/sh", "-c", "exit 23", NULL};
    pid_t child = c_start_process(args, -1, -1, 1);
    int status = 0;
    return child > 0 && waitpid(child, &status, 0) == child &&
           WIFEXITED(status) && WEXITSTATUS(status) == 23 ? 0 : 1;
}
C
    for target in armv7a-linux-androideabi aarch64-linux-android i686-linux-android x86_64-linux-android; do
        for api in 21 28; do
            "$ndk_bin/$target$api-clang" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror \
                -I"$repo/cmd/zir" -I"$repo/cmd/zir-c" "$work/spawn.c" -o "$work/spawn-$target-$api"
            symbols=$("$ndk_bin/llvm-nm" --undefined-only "$work/spawn-$target-$api")
            case "$api:$symbols" in
                21:*posix_spawn*) echo 'Unavailable Android spawn dependency' >&2; exit 1 ;;
                28:*posix_spawn*) ;;
                28:*) echo 'Android spawn path was not linked' >&2; exit 1 ;;
            esac
        done
    done
    echo 'Android API 21 fork and API 28 spawn paths link for all four ABIs'
fi
