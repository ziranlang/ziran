#!/bin/sh
# check-cost: 4
set -eu
ziran=${1:?pass the ziran command}
zi2zx=${2:-$(dirname -- "$ziran")/zi2zx}
zx2native=${3:-$(dirname -- "$ziran")/zx2native}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/machine-elf-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0

"$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" \
    "$repo/tests/machine_elf_cases.zi"
"$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" \
    "$repo/tests/native_services_cases.zi"

cat > "$work/app.zi" <<'ZI'
state: s64;

Factorial :: (value: s64) -> s64 {
    if value <= 1 { return 1 }
    return value * Factorial(value - 1)
}

Step :: (value: s64) -> s64 {
    if value < 5 { return value + 1 }
    return value - 1
}

#program_export
Answer :: () -> s32 {
    state = 0
    current: s64 = 0
    while current < 6 {
        state += Step(current)
        current += 1
    }
    if state != 19 { return 1 }
    if Factorial(5) != 120 { return 2 }
    return 42
}
ZI

run_status() {
    executable=$1
    expected=$2
    set +e
    "$executable"
    actual=$?
    set -e
    if test "$actual" -ne "$expected"; then
        echo "machine ELF exited $actual, expected $expected" >&2
        exit 1
    fi
}

compile_native() {
    output=$1
    root=$2
    entry=$3
    input=$4
    wire=$output.zx
    "$zi2zx" --root "$root" --entry "$entry" -o "$wire" "$input"
    "$zx2native" --machine -o "$output" "$wire"
}

# This is the exact VS Code Native inspection pipeline: checked IR, verified
# ZX, then optimized map/ELF from the same image identity.
"$ziran" ir --root "$repo/examples" --entry hello:main \
    -o "$work/hello-checked" "$repo/examples/hello.zi"
"$zi2zx" --root "$work/hello-checked" --entry hello:main \
    -o "$work/hello.zx" "$work/hello-checked/hello.zir"
"$zx2native" --emit map -o "$work/hello.map" "$work/hello.zx"
"$zx2native" -o "$work/hello.elf" "$work/hello.zx"
nm -S "$work/hello.elf" > "$work/hello.nm"
if grep -Eq '(^|[[:space:]])U[[:space:]]' "$work/hello.nm"; then
    echo "static hello has undefined symbols" >&2
    exit 1
fi
grep -Fq ' T hello:main' "$work/hello.nm"
grep -Fq ' T _zir_trap' "$work/hello.nm"
"$ziran" run --target=c --root "$repo/tests" --module-path "$repo/cmd" \
    "$repo/tests/native_services_cases.zi" -- "$work/hello.elf"
set +e
"$work/hello.elf" > "$work/hello.out"
hello_status=$?
set -e
test "$hello_status" -eq 0
printf 'Hello, World!\n' | cmp - "$work/hello.out"
file "$work/hello.elf" > "$work/hello.file"
grep -Fq 'statically linked' "$work/hello.file"
readelf -lW "$work/hello.elf" > "$work/hello.program-headers"
if grep -Eq '(^|[[:space:]])(INTERP|DYNAMIC)([[:space:]]|$)' "$work/hello.program-headers"; then
    echo "native hello unexpectedly depends on an interpreter or dynamic loader" >&2
    exit 1
fi

for target_platform in 'aarch64 linux' 'armv7 android'; do
    set -- $target_platform
    target=$1
    platform=$2
    row=$target-$platform
    printf 'preserve-output\n' > "$work/$row.elf"
    if ZIRAN_DIAGNOSTICS=json "$zx2native" --target "$target" --platform "$platform" \
        -o "$work/$row.elf" "$work/app.zx" > "$work/$row.log" 2>&1; then
        echo "native ELF accepted unsupported target row $row" >&2
        exit 1
    fi
    grep -Fq 'assembly.gap.native-platform-service-unavailable' "$work/$row.log"
    grep -Fq "$target" "$work/$row.log"
    grep -Fq "$platform" "$work/$row.log"
    printf 'preserve-output\n' | cmp - "$work/$row.elf"
done

compile_native "$work/source-a" "$work" app:Answer "$work/app.zi"
compile_native "$work/source-b" "$work" app:Answer "$work/app.zi"
cmp "$work/source-a" "$work/source-b"
run_status "$work/source-a" 42

"$ziran" ir --root "$work" -o "$work/checked" "$work/app.zi"
compile_native "$work/checked-elf" "$work/checked" app:Answer "$work/checked/app.zir"
run_status "$work/checked-elf" 42

"$zi2zx" --root "$work" --entry app:Answer -o "$work/app.zx" "$work/app.zi"
"$zx2native" --machine -o "$work/zx-elf" "$work/app.zx"
run_status "$work/zx-elf" 42
cmp "$work/source-a" "$work/checked-elf"
cmp "$work/source-a" "$work/zx-elf"

# A loop carries a fuel check. Exhaustion must take the in-image hard trap and
# return the fixed status rather than invoking or switching to another engine.
cat > "$work/fuel.zi" <<'ZI'
#program_export
Spin :: () -> s32 {
    value: s64 = 0
    while value >= 0 { value += 1 }
    return 0
}
ZI
compile_native "$work/fuel-elf" "$work" fuel:Spin "$work/fuel.zi"
run_status "$work/fuel-elf" 125

reject_gap() {
    expected=$1
    source=$2
    printf 'preserve-output\n' > "$work/preserved"
    cp "$work/preserved" "$work/output"
    "$zi2zx" --root "$work" --entry "$source:Answer" -o "$work/$source.zx" "$work/$source.zi"
    if ZIRAN_DIAGNOSTICS=json "$zx2native" --machine \
        -o "$work/output" "$work/$source.zx" > "$work/$source.log" 2>&1; then
        echo "machine ELF accepted unsupported $source input" >&2
        exit 1
    fi
    if ! grep -Fq "$expected" "$work/$source.log"; then
        echo "machine ELF reported the wrong gap for $source; expected $expected:" >&2
        cat "$work/$source.log" >&2
        exit 1
    fi
    cmp "$work/preserved" "$work/output"
}

cat > "$work/string_gap.zi" <<'ZI'
#program_export
Answer :: () -> s32 { message := "machine gap"; return cast(s32)message.count }
ZI
reject_gap 'assembly.gap.machine-string' string_gap

cat > "$work/global_init_gap.zi" <<'ZI'
state: s64 = 7;
#program_export
Answer :: () -> s32 { return cast(s32)state }
ZI
reject_gap 'assembly.gap.machine-global-init' global_init_gap

cat > "$work/slice_gap.zi" <<'ZI'
First :: (values: []s64) -> s64 { return values[0] }
#program_export
Answer :: () -> s32 { values: [1]s64 = .[42]; return cast(s32)First(values[:]) }
ZI
reject_gap 'assembly.gap.machine-slice' slice_gap

cat > "$work/host_gap.zi" <<'ZI'
host_api :: #system_library "host_api";
HostValue :: () -> s32 #foreign host_api;
#program_export
Answer :: () -> s32 { return HostValue() }
ZI
reject_gap 'assembly.gap.machine-host' host_gap

cat > "$work/print_gap.zi" <<'ZI'
#program_export
Answer :: () -> s32 { print("Hello, World!\n"); return 0 }
ZI
reject_gap 'assembly.gap.machine-platform-service-unavailable' print_gap

cat > "$work/float_print_gap.zi" <<'ZI'
#program_export
Answer :: () -> s32 { print("%\n", 0.1); return 0 }
ZI
reject_gap 'assembly.gap.machine-float-format' float_print_gap

# The direct native path also rejects an arbitrary host capability with its
# stable platform-service diagnostic and leaves an existing output untouched.
cat > "$work/native_host_gap.zi" <<'ZI'
host_api :: #system_library "host_api";
Input :: (key: s64) -> s64 #foreign host_api;
#program_export
Answer :: () -> s32 { return cast(s32)Input(1) }
ZI
"$ziran" ir --root "$work" -o "$work/native-host-ir" "$work/native_host_gap.zi"
"$zi2zx" --root "$work/native-host-ir" --entry native_host_gap:Answer \
    -o "$work/native-host.zx" "$work/native-host-ir/native_host_gap.zir"
printf 'preserve-output\n' > "$work/native-host.elf"
if ZIRAN_DIAGNOSTICS=json "$zx2native" \
    -o "$work/native-host.elf" "$work/native-host.zx" > "$work/native-host.log" 2>&1; then
    echo "native ELF accepted an unavailable platform service" >&2
    exit 1
fi
grep -Fq 'assembly.gap.native-platform-service-unavailable' "$work/native-host.log"
printf 'preserve-output\n' | cmp - "$work/native-host.elf"

# After all fixtures are prepared, poison every external compiler, assembler
# and alternate language engine name. The explicit machine command must still
# produce an executable and the poison log must remain empty.
mkdir "$work/no-tools"
: > "$work/invoked"
for tool in cc c++ gcc g++ clang clang++ as ld tcc go; do
    cat > "$work/no-tools/$tool" <<SCRIPT
#!/bin/sh
printf '%s\n' '$tool' >> '$work/invoked'
exit 97
SCRIPT
    chmod 755 "$work/no-tools/$tool"
done
PATH="$work/no-tools:$PATH" "$zx2native" --machine -o "$work/no-tools-elf" "$work/app.zx"
test ! -s "$work/invoked"
run_status "$work/no-tools-elf" 42
