# check-cost: 420
set -eu
# Bundle the lifecycle fixtures from source as revision-28 .zx, then run the
# production WebAssembly host, libziran built by emcc with the instruction
# engine, in a private Chromium: every instance runs on the bundle Wasm tier.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(CDPATH= cd -- "$(dirname -- "$ziran")/.." && pwd)
work=$build/tests/zx-web-lifecycle
# The library build is kept between runs and rebuilt incrementally.
library=$build/tests/zx-web-lifecycle-lib
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
emar=$(dirname "$emcc")/emar

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

make -C "$repo" -j2 BUILD_DIR="$library" \
    CC="$emcc" AR="$emar" OBJCOPY=true CFLAGS='-O2' SANITIZE_FLAGS= \
    FRAMEFLAGS='-Wframe-larger-than=16384' \
    WASM_PRIVATE_HEADERS="$library/private" \
    "$library/libziran.a" > "$work/lib.log" 2>&1 || {
        tail -20 "$work/lib.log"
        exit 1
    }
"$emcc" -O2 -std=gnu99 -I"$repo/include" "$repo/tests/zx_web_lifecycle_host.c" \
    "$library/libziran.a" -o "$work/wasm.js" \
    -s MODULARIZE=1 -s EXPORT_NAME=ZxWebLifecycleWasm -s ENVIRONMENT=web \
    -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=0 -s STACK_SIZE=1048576 \
    --preload-file "$work/fixtures@/fixtures"

"$ziran" run --target=c --root "$repo/tests" "$repo/tests/zx_web_lifecycle_cases.zi" -- "$ziran" "$work" "$chromium"
