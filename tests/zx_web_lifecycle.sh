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

# A host that suspends through Asyncify: the bundle runs on the Wasm tier,
# which unwinds and rewinds with the page's stack.
if command -v node >/dev/null 2>&1; then
    # Suspensions inside an initializer, and in a helper called in a loop
    # with a record, a string and a frame array live across the host call.
    mkdir -p "$work/source/deep"
    cat > "$work/source/deep/component.zi" <<'ZI'
host_api :: #system_library "host_api";
Adjust :: (value: s64) -> s64 #foreign host_api;
Item :: struct {
    title: string
    weight: s64
}
Seed :: () -> s64 { return Adjust(1) }
total: s64 = Seed();
Step :: (item: Item, round: s64) -> s64 {
    kept: [4]s64 = s64.[round, round + 1, round + 2, round + 3]
    value := Adjust(item.weight + kept[round % 4] - round + round % 4)
    return value + cast(s64)item.title.count + kept[3] - round - 3
}
#program_export
Next :: () -> s64 {
    sum: s64 = total
    item := Item.{.title = "abc", .weight = 5}
    for round: 0..9 { sum += Step(item, round) + round }
    return sum
}
ZI
    "$ziran" bundle --root "$work/source/deep" --entry component:Next \
        -o "$work/fixtures/deep.zx" "$work/source/deep/component.zi" > /dev/null
    "$emcc" -O2 -std=gnu99 -I"$repo/include" "$repo/tests/zx_web_suspending_host.c" \
        "$library/libziran.a" -o "$work/suspending.js" -s ASYNCIFY=1 -s ENVIRONMENT=node \
        -s ALLOW_MEMORY_GROWTH=1 -s STACK_SIZE=1048576 \
        --embed-file "$work/fixtures/old.zx@/old.zx" --embed-file "$work/fixtures/deep.zx@/deep.zx"
    node "$work/suspending.js" /old.zx /deep.zx > "$work/suspending.out" 2> "$work/suspending.err" &&
        ZIRAN_WASM_TIER=0 node "$work/suspending.js" /old.zx /deep.zx engine > "$work/suspending-engine.out" 2>&1 || {
        cat "$work/suspending.out" "$work/suspending.err" "$work/suspending-engine.out"
        exit 1
    }
    wasm=$(cat "$work/suspending.out"); engine=$(cat "$work/suspending-engine.out")
    if grep -q 'zx wasm tier\|RuntimeError' "$work/suspending.err" ||
       test "$wasm" != "${engine%tier 0}tier 1" || test "${wasm#results 89 99 }" = "$wasm"; then
        echo "zx-web-lifecycle: suspending hosts gave '$wasm' on the tier and '$engine' on the engine"
        cat "$work/suspending.err"
        exit 1
    fi
    echo "zx-web-lifecycle: an Asyncify host that suspends runs bundles on the Wasm tier"
elif test "$require" = 1; then
    echo 'zx-web-lifecycle: node is unavailable for the suspending-host check' >&2
    exit 1
fi
