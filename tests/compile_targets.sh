set -eu

ziran=$1
tool_dir=$(dirname "$ziran")
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/compile-targets-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

write_case() {
    directory=$1 target=$2 os=$3 cpu=$4
    mkdir -p "$directory"
    cat > "$directory/app.zi" <<ZI
#if TARGET == .$target && OS == .$os {
    IF_VALUE :: 40
} else {
    #import "inactive_target_import_must_not_load"
    IF_VALUE :: MissingTargetBranch()
}
IFX_VALUE :: #ifx CPU == .$cpu then 2 else MissingCPUBranch()
RUN_VALUE :: #run TARGET == .$target && OS == .$os && CPU == .$cpu
OPTIONS :: #run Compiler.Options
#assert RUN_VALUE
#assert Compiler.Target == TARGET
#assert Compiler.TargetOS == OS
#assert Compiler.TargetCPU == CPU
#assert Compiler.Options.count > 0
#program_export
Answer :: () -> s32 {
    if OPTIONS.count == 0 { return 0 }
    return IF_VALUE + IFX_VALUE
}
ZI
}

# Every OS and CPU enum member is selected at least once. These checks also
# prove compound target conditions and inactive-import unloading.
for os in NONE LINUX WINDOWS MACOS ANDROID WEB; do
    cpu=X64
    if test "$os" = NONE; then cpu=NONE; fi
    if test "$os" = WEB; then cpu=WASM32; fi
    name=$(printf '%s' "$os" | tr 'A-Z' 'a-z')
    cpu_name=$(printf '%s' "$cpu" | tr 'A-Z' 'a-z')
    directory=$work/c-$name
    write_case "$directory" C "$os" "$cpu"
    "$tool_dir/zi2c" --no-main --target-os "$name" --target-cpu "$cpu_name" \
        --root "$directory" -o "$directory/out" "$directory/app.zi"
done
for cpu in NONE X64 ARM64 WASM32; do
    name=$(printf '%s' "$cpu" | tr 'A-Z' 'a-z')
    directory=$work/c-cpu-$name
    write_case "$directory" C NONE "$cpu"
    "$tool_dir/zi2c" --no-main --target-os none --target-cpu "$name" \
        --root "$directory" -o "$directory/out" "$directory/app.zi"
done

directory=$work/go
write_case "$directory" GO LINUX X64
"$tool_dir/zi2go" --no-main --target-os linux --target-cpu x64 \
    --root "$directory" -o "$directory/out" "$directory/app.zi"

directory=$work/wasm
write_case "$directory" WASM WEB WASM32
cat >> "$directory/app.zi" <<'ZI'
#assert Compiler.HostOS != OS
#assert Compiler.HostCPU != CPU
ZI
"$tool_dir/zi2asm" --root "$directory" --entry app:Answer --emit wasm \
    -o "$directory/app.wasm" "$directory/app.zi"
test -s "$directory/app.wasm"

directory=$work/zx
write_case "$directory" ZX NONE NONE
"$tool_dir/zi2zx" bundle --root "$directory" --entry app:Answer \
    -o "$directory/app.zx" "$directory/app.zi"
test "$("$tool_dir/zi2zx" run "$directory/app.zx")" = 42

# Untargeted checked IR is deliberately portable: ZX/NONE/NONE is frozen into
# the saved file and remains identical when bundled later.
portable=$work/portable
write_case "$portable" ZX NONE NONE
"$tool_dir/zi2zir" --root "$portable" -o "$portable/ir" "$portable/app.zi"
if rg -a -q '#if|#ifx|#run|TARGET|Compiler\.' "$portable/ir/app.zir"; then
    echo 'portable .zir retained target-dependent compile-time source' >&2
    exit 1
fi
"$tool_dir/zi2zx" bundle --root "$portable/ir" --entry app:Answer \
    -o "$portable/saved.zx" "$portable/ir/app.zir"
test "$("$tool_dir/zi2zx" run "$portable/saved.zx")" = 42

# PLATFORM_WEB is no longer a target selector. POSIX_THREADS remains a real
# capability supplied by the native C frontend.
cat > "$work/proxies.zi" <<'ZI'
#assert OS == Compiler.HostOS
#assert #defined(POSIX_THREADS)
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$tool_dir/zi2c" --no-main --define PLATFORM_WEB --root "$work" \
    -o "$work/proxies" "$work/proxies.zi"

cat > "$work/assertion.zi" <<'ZI'
#assert false, "target contract diagnostic"
ZI
if "$tool_dir/zi2zir" --check-only --root "$work" "$work/assertion.zi" \
    >"$work/assertion.out" 2>"$work/assertion.err"; then
    echo '#assert false unexpectedly succeeded' >&2
    exit 1
fi
rg -q '#assert failed: "target contract diagnostic"' "$work/assertion.err"

echo 'Compile-time OS/CPU/TARGET, Compiler host metadata, inactive imports and portable .zir passed'
