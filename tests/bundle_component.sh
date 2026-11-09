set -eu
# Build two independently replaced components from checked .zi source into
# portable .zx bundles, then exercise typed ABI, scalar state migration,
# transactional rollback and generationed handles through the C embedding API.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

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

mkdir -p "$work/old" "$work/new" "$work/wrong" "$work/init" \
    "$work/mismatch" "$work/resource" "$work/unrelated"
write_component "$work/old" 7 '' ''
write_component "$work/new" 0 '    open: bool' '    if state.open { result += 1000 }'
cat > "$work/wrong/component.zi" <<'ZI'
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
cat > "$work/init/component.zi" <<'ZI'
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
cat > "$work/mismatch/component.zi" <<'ZI'
host_api :: #system_library "host_api";
Adjust :: (value: s64) -> s64 #foreign host_api;
Item :: struct {
    title: string
    weight: s64
}
State :: struct {
    count: s64
    first: Item
    history: [2]s64
}
state: State = State.{.count = 7, .first = Item.{.title = "old", .weight = 3}, .history = s64.[1, 2]};
bonus: s64 = 2;
#program_export
Next :: () -> s64 {
    return state.count * 10 + state.first.weight + state.history[0] +
        cast(s64)state.first.title.count + bonus
}
ZI
cat > "$work/resource/component.zi" <<'ZI'
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
state: State = State.{.count = 7, .first = Item.{.title = "old", .weight = 3}, .history = s64.[1, 2, 3]};
handle: *void = null;
bonus: s64 = 2;
#program_export
Next :: () -> s64 {
    state.count = Adjust(state.count)
    return state.count * 10 + state.first.weight +
        state.history[0] + cast(s64)state.first.title.count + bonus
}
ZI
cat > "$work/unrelated/other.zi" <<'ZI'
count: s64 = 100;
#program_export
Next :: () -> s64 {
    count += 1
    return count
}
ZI

for name in old new wrong init mismatch resource; do
    "$ziran" bundle --root "$work/$name" --entry component:Next \
        -o "$work/$name.zx" "$work/$name/component.zi" > /dev/null
done
"$ziran" bundle --root "$work/unrelated" --entry other:Next \
    -o "$work/unrelated.zx" "$work/unrelated/other.zi" > /dev/null

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    "$repo/tests/bundle_component_test.c" \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -lpthread -o "$work/test"
"$work/test" "$work/old.zx" "$work/new.zx" "$work/wrong.zx" \
    "$work/init.zx" "$work/mismatch.zx" "$work/resource.zx" \
    "$work/unrelated.zx"
