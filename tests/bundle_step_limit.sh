#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-$repo/build}/bundle-step-limit.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
runtime_libs=-lm
case $(uname -s) in Linux) runtime_libs="$runtime_libs -ldl -pthread" ;; esac
cat > "$work/loop.zi" <<'EOF'
Spin :: () -> s32 { while true {} return 0 }
value: s32 = Spin();
main :: () -> s32 { return value }
EOF
cat > "$work/finite.zi" <<'EOF'
api :: #system_library "host_api";
Tick :: () #foreign api;
Work :: () -> s32 {
    count: s32 = 0
    while count < 100 { Tick(); count += 1 }
    return 7
}
value: s32 = Work();
main :: () -> s32 { return value + Work() }
EOF
"$ziran" bundle --root "$work" --entry loop:main -o "$work/loop.zx" "$work/loop.zi"
"$ziran" bundle --root "$work" --entry finite:main -o "$work/finite.zx" "$work/finite.zi"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    "$repo/tests/bundle_step_limit_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" $runtime_libs -o "$work/host"
timeout 20 "$work/host" "$work/loop.zx" "$work/finite.zx"

# A component is a positive-budget instance whose bundle is replaced with its
# state carried over: typed ABI, state export, migration and import, rollback
# on failure and generationed handles.
write_component() {
    directory=$1; version=$2; extra_state=$3; open_body=$4
    mkdir -p "$directory"
    cat > "$directory/component.zi" <<ZI
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
write_component "$work/old" 7 '' ''
write_component "$work/new" 0 '    open: bool' '    if state.open { result += 1000 }'
# A different capability signature.
mkdir -p "$work/wrong"
cat > "$work/wrong/component.zi" <<'ZI'
host_api :: #system_library "host_api";
Adjust :: (value: s64, extra: s64) -> s64 #foreign host_api;
count: s64 = 7;
#program_export
Next :: () -> s64 {
    count = Adjust(count, 2)
    return count * 10
}
ZI
# An initializer beyond a small budget.
write_component "$work/init" 7 '' ''
sed 's/state: State = State.{.count = 7,/Seed :: () -> s64 {\n    value: s64 = 0\n    while value < 2000 { value += 1 }\n    return value\n}\nstate: State = State.{.count = Seed(),/' \
    "$work/init/component.zi" > "$work/init/seeded.zi"
mv "$work/init/seeded.zi" "$work/init/component.zi"
# A state record of another shape that needs no capability, and state that
# is not portable.
mkdir -p "$work/mismatch"
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
write_component "$work/resource" 7 '' '    if handle != null { result += 1 }'
sed 's/^bonus: s64 = 2;/handle: *s64;\nbonus: s64 = 2;/' \
    "$work/resource/component.zi" > "$work/resource/handle.zi"
mv "$work/resource/handle.zi" "$work/resource/component.zi"
mkdir -p "$work/unrelated"
cat > "$work/unrelated/other.zi" <<'ZI'
count: s64 = 100;
#program_export
Next :: () -> s64 {
    count += 1
    return count
}
ZI
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" "$repo/tests/bundle_component_test.c" \
    "${ZIRAN_LIB:-$repo/build/libziran.a}" $runtime_libs -o "$work/component"
for name in old new wrong init mismatch resource unrelated; do
    module=component; source=$work/$name/component.zi
    if test "$name" = unrelated; then module=other; source=$work/$name/other.zi; fi
    "$ziran" bundle --root "$work/$name" --entry "$module:Next" -o "$work/$name.zx" "$source"
done
"$work/component" "$work/old.zx" "$work/new.zx" "$work/wrong.zx" "$work/init.zx" \
    "$work/mismatch.zx" "$work/resource.zx" "$work/unrelated.zx"
