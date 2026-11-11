# check-cost: 420
set -eu
# Build the lifecycle fixtures from source through checked IR and .zx, then
# execute the production WebAssembly and linked hosts in a private Chromium.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(CDPATH= cd -- "$(dirname -- "$ziran")/.." && pwd)
zi2zx=$build/bin/zi2zx
work=$build/tests/zx-web-lifecycle
rm -rf "$work"
mkdir -p "$work/fixtures"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

require=${ZIRAN_REQUIRE_ZX_WEB_LIFECYCLE:-0}
chromium=${ZIRAN_CHROMIUM:-chromium}
emcc=${ZIRAN_WASM_EMCC:-}
if command -v "$chromium" >/dev/null 2>&1; then :; else
    message="zx-web-lifecycle: Chromium command '$chromium' is unavailable"
    test "$require" != 1
    echo "$message; skipping"
    exit 0
fi
if command -v "$emcc" >/dev/null 2>&1; then :;
elif [ -x "$HOME/emsdk/upstream/emscripten/emcc" ]; then
    emcc=$HOME/emsdk/upstream/emscripten/emcc
else
    message="zx-web-lifecycle: Emscripten compiler is unavailable"
    test "$require" != 1
    echo "$message; skipping"
    exit 0
fi
if ! "$zi2zx" 2>&1 | grep -q native-c; then
    message="zx-web-lifecycle: $zi2zx has no native-C engine"
    test "$require" != 1
    echo "$message; skipping"
    exit 0
fi
emar=$(dirname "$emcc")/emar
emnm=$(dirname "$emcc")/emnm

write_component() {
    directory=$1; version=$2; extra_state=$3; open_body=$4
    mkdir -p "$directory"
    cat > "$directory/component.zi" <<ZI
host_api :: #system_library "host_api";
Adjust :: (value: s64) -> s64 #foreign host_api;
count: s64 = $version;
Item :: struct {
    title: string
    weight: s64
}
State :: struct {
    count: s64
    first: Item
    history: [3]s64
$extra_state
}
state: State = State.{.count = $version, .first = Item.{.title = "old", .weight = 3}, .history = s64.[1, 2, 3]};
bonus: s64 = 2;
#program_export
Next :: () -> s64 {
    state.count = Adjust(state.count)
    result := state.count * 10 + state.first.weight +
        state.history[0] + cast(s64)state.first.title.count + bonus
$open_body
    return result
}
ZI
}

for name in old new wrong init unrelated; do
    mkdir -p "$work/source/$name"
done
write_component "$work/source/old" 7 '' ''
write_component "$work/source/new" 0 '    open: bool' '    if state.open { result += 1000 }'
cat > "$work/source/wrong/component.zi" <<'ZI'
host_api :: #system_library "host_api";
Adjust :: (value: s64, extra: s64) -> s64 #foreign host_api;
count: s64 = 7;
bonus: s64 = 2;
#program_export
Next :: () -> s64 {
    count = Adjust(count, 2)
    return count * 10 + bonus
}
ZI
cat > "$work/source/init/component.zi" <<'ZI'
host_api :: #system_library "host_api";
Adjust :: (value: s64) -> s64 #foreign host_api;
Item :: struct {
    title: string
    weight: s64
}
State :: struct {
    count: s64
    first: Item
    history: [3]s64
}
Seed :: () -> s64 {
    value: s64 = 0
    while value < 2000 { value += 1 }
    return value
}
state: State = State.{.count = Seed(), .first = Item.{.title = "old", .weight = 3}, .history = s64.[1, 2, 3]};
bonus: s64 = 2;
#program_export
Next :: () -> s64 {
    state.count = Adjust(state.count)
    return state.count * 10 + state.first.weight +
        state.history[0] + cast(s64)state.first.title.count + bonus
}
ZI
cat > "$work/source/unrelated/component.zi" <<'ZI'
count: s64 = 100;
#program_export
Next :: () -> s64 {
    count += 1
    return count
}
ZI

for name in old new wrong init unrelated; do
    "$ziran" bundle --root "$work/source/$name" --entry component:Next \
        -o "$work/fixtures/$name.zx" "$work/source/$name/component.zi" > /dev/null
done
test "$("$ziran" run "$work/fixtures/unrelated.zx")" = 101

make -C "$repo" -j2 BOOTSTRAP=1 BUILD_DIR="$work/vm-lib" \
    CC="$emcc" AR="$emar" OBJCOPY=true CFLAGS='-O2' SANITIZE_FLAGS= \
    FRAMEFLAGS='-Wframe-larger-than=16384' \
    WASM_PRIVATE_HEADERS="$work/vm-private" \
    "$work/vm-lib/libziran-vm.a" > "$work/vm-lib.log" 2>&1 || {
        tail -20 "$work/vm-lib.log"
        exit 1
    }
"$emcc" -O2 -std=gnu99 -I"$repo/include" -I"$repo/cmd/zir" \
    -I"$repo/bootstrap/compiler_ir" "$repo/tests/zx_web_lifecycle_host.c" \
    "$work/vm-lib/libziran-vm.a" -o "$work/wasm.js" \
    -s MODULARIZE=1 -s EXPORT_NAME=ZxWebLifecycleWasm -s ENVIRONMENT=web \
    -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=0 -s STACK_SIZE=1048576 \
    --preload-file "$work/fixtures@/fixtures"

"$zi2zx" native-c -o "$work/linked-programs" "$work/fixtures/old.zx" > /dev/null
python3 "$repo/scripts/linked-zx.py" "$work/linked-programs" \
    --output "$work/linked-objects" --cc "$emcc" --nm "$emnm" \
    --cflags=-O2 > "$work/linked-objects.txt" 2> /dev/null
make -C "$repo" -j2 BOOTSTRAP=1 BUILD_DIR="$work/linked-lib" \
    CC="$emcc" AR="$emar" OBJCOPY=true CFLAGS='-O2' SANITIZE_FLAGS= \
    FRAMEFLAGS='-Wframe-larger-than=16384' \
    WASM_PRIVATE_HEADERS="$work/linked-private" \
    "$work/linked-lib/libziran.a" > "$work/linked-lib.log" 2>&1 || {
        tail -20 "$work/linked-lib.log"
        exit 1
    }
# shellcheck disable=SC2046
"$emcc" -O2 -std=gnu99 -I"$repo/include" -I"$repo/cmd/zir" \
    "$repo/tests/zx_web_lifecycle_host.c" $(cat "$work/linked-objects.txt") \
    "$work/linked-lib/libziran.a" -o "$work/linked.js" \
    -s MODULARIZE=1 -s EXPORT_NAME=ZxWebLifecycleLinked -s ENVIRONMENT=web \
    -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=0 -s STACK_SIZE=1048576 \
    --preload-file "$work/fixtures@/fixtures"

"$ziran" run --target=c --root "$repo/tests" "$repo/tests/zx_web_lifecycle_cases.zi" -- "$ziran" "$work" "$chromium"
