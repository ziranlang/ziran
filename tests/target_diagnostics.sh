#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/target-diagnostics-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/plain.zi" <<'ZI'
#program_export
Answer :: () -> s32 { return 42 }
ZI
cat > "$work/go_api.zi" <<'ZI'
native :: #system_library "go:strings";
Count :: (value: string, part: string) -> s32 #foreign native "Count";
ZI
cat > "$work/go_type.zi" <<'ZI'
native :: #system_library "go:time";
Clock :: #type #foreign native "Time";
ZI
cat > "$work/c_api.zi" <<'ZI'
native :: #system_library "libc";
Abs :: (value: s32) -> s32 #foreign native "abs";
ZI
cat > "$work/c_callback.zi" <<'ZI'
Callback :: #type (value: s32) -> s32 #c_call;
ZI
cat > "$work/aggregate_union.zi" <<'ZI'
Item :: struct { value: s64 }
Payload :: union { item: Item; value: s64 }
#program_export
Answer :: () -> s32 { payload: Payload; return cast(s32)payload.item.value }
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/plain.zi" \
    "$work/go_api.zi" "$work/go_type.zi" "$work/c_api.zi" \
    "$work/aggregate_union.zi" "$work/c_callback.zi"

for form in source saved; do
    root=$work
    extension=zi
    if [ "$form" = saved ]; then root=$work/ir; extension=zir; fi
    for target in c go zx; do
        "$ziran" check "--target=$target" --root "$root" "$root/plain.$extension"
        for fixture in go_api go_type c_api c_callback; do
            case "$fixture:$target" in
                go_*:go|c_api:c|c_api:go|c_callback:c)
                    "$ziran" check "--target=$target" --root "$root" "$root/$fixture.$extension"
                    continue ;;
            esac
            diagnostics=$work/$form-$target-$fixture.jsonl
            if "$ziran" check "--target=$target" --diagnostics=json --root "$root" \
                "$root/$fixture.$extension" > "$work/stdout" 2> "$diagnostics"; then
                echo "$target accepted unsupported $fixture ($form)" >&2
                exit 1
            fi
            capability=ffi.go
            case "$fixture" in
                go_type) capability=types.go ;;
                c_api) capability=ffi.c ;;
                c_callback) capability=procedures.c-call ;;
            esac
            "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- target-unsupported "$diagnostics" "$target" "$capability"
        done
        if [ "$target" = go ]; then
            output=$work/$form-callback-output
            if "$ziran" build --target=go --diagnostics=json --root "$root" \
                -o "$output" "$root/plain.$extension" "$root/c_callback.$extension" \
                > "$work/stdout" 2> "$work/callback.jsonl"; then
                echo 'Go emitted an unsupported C callback type' >&2
                exit 1
            fi
            test ! -d "$output" || test -z "$(find "$output" -type f -print -quit)"
        fi
        # Preflight all inputs before emitting even the first valid module.
        if [ "$target" != zx ]; then
            fixture=c_callback
            if [ "$target" = c ]; then fixture=go_type; fi
            output=$work/$form-$target-output
            if "$ziran" build "--target=$target" --diagnostics=json --root "$root" \
                -o "$output" "$root/plain.$extension" "$root/$fixture.$extension" \
                > "$work/stdout" 2> "$work/build.jsonl"; then
                echo "$target emitted an unsupported foreign type" >&2
                exit 1
            fi
            test ! -d "$output" || test -z "$(find "$output" -type f -print -quit)"
            "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- target-build "$work/build.jsonl" "$target"
        fi
    done
    for target in go zx; do
        if "$ziran" check "--target=$target" --diagnostics=json --root "$root" \
            "$root/aggregate_union.$extension" > "$work/stdout" 2> "$work/union.jsonl"; then
            echo "$target accepted an aggregate union during checking" >&2
            exit 1
        fi
        "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- target-union "$work/union.jsonl" "$target"
    done
done

# Direct assembly target registry and preflight. Every described row has an
# explicit state and only implemented rows pass; no full machine row has
# passed its encoder and runtime cutover yet, so every row fails. A rejected
# row fails with its stable code before any input is read or output written,
# identically for source and saved IR, and before feature or service checks.
"$ziran" capabilities --json > "$work/capabilities.json"
for row in x86_64-linux:sysv:64:gap x86_64-android:sysv:64:gap \
    x86_64-windows:win64:64:gap x86_64-macos:apple64:64:gap \
    armv7-linux:aapcs32:32:gap armv7-android:aapcs32:32:gap \
    aarch64-linux:aapcs64:64:gap aarch64-android:aapcs64:64:gap \
    aarch64-windows:win64:64:gap aarch64-macos:apple64:64:gap \
    wasm32-browser:wasm:32:gap wasm32-wasi:wasm:32:gap; do
    id=${row%%:*}
    rest=${row#*:}
    abi=${rest%%:*}
    rest=${rest#*:}
    bits=${rest%%:*}
    state=${rest#*:}
    arch=${id%%-*}
    os=${id#*-}
    grep -Fq "{\"id\":\"$id\",\"arch\":\"$arch\",\"os\":\"$os\",\"abi\":\"$abi\",\"pointer_bits\":$bits,\"state\":\"$state\",\"diagnostic\":\"assembly.gap.target-unimplemented\"," "$work/capabilities.json" || {
        echo "capabilities misreports assembly row $id" >&2
        exit 1
    }
done
test "$(grep -o '{"id":"[a-z0-9_]*-[a-z]*"' "$work/capabilities.json" | wc -l)" -eq 12
grep -Fq '"preflight_passes":["implemented"]' "$work/capabilities.json"
if grep -Fq '"state":"implemented"' "$work/capabilities.json"; then
    echo 'capabilities claims an implemented assembly row' >&2
    exit 1
fi
for code in assembly.gap.target-unimplemented assembly.gap.cpu-feature assembly.gap.os-service; do
    grep -Fq "\"$code\"" "$work/capabilities.json"
    "$ziran" explain "$code" --json | grep -Fq "\"code\":\"$code\""
done

mkdir -p "$work/preflight"
before=$(find "$work" -path "$work/preflight" -prune -o -type f -print | sort | xargs cksum)
for case in x86_64-linux:gap x86_64-linux+avx2:gap x86_64-linux/threads:gap \
    aarch64-linux+crc32:gap armv7-android+neon:gap wasm32-browser+simd128:gap \
    x86_64-windows/executable-memory:gap aarch64-windows:gap; do
    spec=${case%%:*}
    state=${case#*:}
    service=
    case "$spec" in */*) service=--os-service=${spec#*/}; spec=${spec%%/*} ;; esac
    name=$(printf '%s' "$spec$service" | tr '+/=' '___')
    for form in source saved; do
        input=$work/plain.zi
        if [ "$form" = saved ]; then input=$work/ir/plain.zir; fi
        result=$work/preflight/$name-$form
        if "$ziran" capabilities --diagnostics=json "--assembly-target=$spec" $service \
            "$input" > "$result.out" 2> "$result.jsonl"; then
            echo "assembly preflight accepted $spec $service ($form)" >&2
            exit 1
        fi
        test ! -s "$result.out"
        test "$(wc -l < "$result.jsonl")" -eq 1
        grep -Fq '"code":"assembly.gap.target-unimplemented"' "$result.jsonl"
        grep -Fq "selected target ${spec%%+*} engine assembly state $state:" "$result.jsonl"
        grep -Fq 'no other engine was selected' "$result.jsonl"
        sed 's/ Input [^ ]* ([a-z IR]*) was not read;//' "$result.jsonl" > "$result.normal"
    done
    cmp "$work/preflight/$name-source.normal" "$work/preflight/$name-saved.normal"
done
grep -Fq "Input $work/ir/plain.zir (saved IR) was not read" "$work/preflight/x86_64-linux-saved.jsonl"
grep -Fq "Input $work/plain.zi (source) was not read" "$work/preflight/x86_64-linux-source.jsonl"
after=$(find "$work" -path "$work/preflight" -prune -o -type f -print | sort | xargs cksum)
test "$before" = "$after"

for bad in x86_64-linux+avx3 x86_64-linux+ wasm32-linux riscv64-linux x86_64; do
    if "$ziran" capabilities "--assembly-target=$bad" > "$work/stdout" 2> "$work/stderr"; then
        echo "assembly preflight accepted unknown target $bad" >&2
        exit 1
    fi
    if grep -Fq 'assembly.gap' "$work/stderr"; then
        echo "unknown target $bad reported a registry gap" >&2
        exit 1
    fi
done
for arguments in '--assembly-target=x86_64-linux --os-service=gpu' \
    '--target=c --assembly-target=x86_64-linux' '--os-service=threads' \
    "$work/plain.zi"; do
    # shellcheck disable=SC2086
    if "$ziran" capabilities $arguments > "$work/stdout" 2> "$work/stderr"; then
        echo "capabilities accepted an invalid assembly selection: $arguments" >&2
        exit 1
    fi
done
