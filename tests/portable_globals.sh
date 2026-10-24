#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/state.zi" <<'ZI'

Cell :: struct {
    value: s32
}
Store :: struct {
    cells: [64]Cell
    count: s32
}
current: Store;
unused: s32 = 7;
base :: 5
limit: s64 = 7;
scale: s32 = 3;
factor: s32 = base + 2;
computed: s32 = MakeThree();
MakeThree :: () -> s32 { return 3 }
ratio: float32 = 2.5;
name: string = "zi\u0072an";
flag: bool = true;
origin: Cell = Cell.{.value = 9};
Track :: struct {
    title: string
    weight: s32
}
tracks: [3]Track = .[Track.{.title = "fir\u0073t", .weight = 3}, Track.{.title = "second", .weight = 5}, Track.{.title = "third", .weight = 7}];
marks: [4]s32 = .[2, 4, 6, 8];
Shelf :: struct {
    first: Track
    labels: [2]string
    count: s32
}
shelf: Shelf = Shelf.{.count = 2, .first = Track.{.weight = 11, .title = "nes\u0074ed"}, .labels = string.["one", "tw\u006f"]};
emoji: string = "\U0001F642";
zero: string = "a\x00b";

#program_export
InitChecks :: () -> s32 {
    if limit != 7 { return 0 }
    if scale != 3 { return 0 }
    if factor != 7 { return 0 }
    if computed != 3 { return 0 }
    if ratio != 2.5 { return 0 }
    if flag != true { return 0 }
    if name.count != 5 { return 0 }
    if name != "ziran" { return 0 }
    if origin.value != 9 { return 0 }
    if marks[0] != 2 || marks[3] != 8 { return 0 }
    if tracks[0].weight != 3 || tracks[2].weight != 7 { return 0 }
    if tracks[1].title.count != 6 { return 0 }
    if tracks[0].title != "first" { return 0 }
    if shelf.first.title != "nested" || shelf.first.weight != 11 { return 0 }
    if shelf.labels[0] != "one" || shelf.labels[1] != "two" { return 0 }
    if shelf.count != 2 { return 0 }
    if emoji.count != 4 || emoji != "\U0001F642" { return 0 }
    if zero.count != 3 || zero != "a\x00b" { return 0 }
    return 1
}

#program_export
Install :: (next: Store) {
    current = next
}
#program_export
Put :: (index: s32, value: s32) {
    current.cells[index].value = value
    current.count += 1
}
#program_export
Read :: (index: s32) -> s32 {
    return current.cells[index].value
}
#program_export
Snapshot :: () -> Store {
    return current
}
#program_export
Count :: () -> s32 {
    return current.count
}
ZI

cat > "$work/app.zi" <<'ZI'
#import "state"

#program_export
Answer :: () -> s32 {
    if InitChecks() != 1 { return -1 }
    source: Store
    source.cells[0].value = 41
    Install(source)
    source.cells[0].value = 0
    if Read(0) != 41 { return 0 }
    iteration: s32 = 0
    while iteration < 80 {
        Put(1, iteration)
        iteration += 1
    }
    snapshot: Store = Snapshot()
    snapshot.cells[0].value = 0
    if Read(0) != 41 || Read(1) != 79 || Count() != 80 {
        return 0
    }
    Install(source)
    if Read(0) != 0 || Count() != 0 { return 0 }
    return 42
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zx" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42
cat > "$work/global_instance.c" <<'C'
#include "ziran_host.h"
#include <assert.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    for(int i = 0; i < 2; i++) {
        BundleInstance *instance = BundleInstantiate(bundle, NULL, 0);
        assert(instance != NULL);
        for(int run = 0; run < 2; run++) {
            long long value = 0;
            int has_value = 0;
            assert(BundleInstanceRun(instance, &value, &has_value));
            assert(has_value && value == 42);
        }
        BundleInstanceClose(instance);
    }
    BundleClose(bundle);
    return 0;
}
C
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    "$work/global_instance.c" "$ziran_lib" ${VM_LDFLAGS:-} \
    -o "$work/global_instance"
"$work/global_instance" "$work/source.zx"
"$work/global_instance" "$work/saved.zx"

for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
    else
        module=$work/ir/app.zir
    fi
    for target in c go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("global value") } }
GO
            GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build --target="$target" --root "$work" \
                -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        fi
    done
done

cat > "$work/invalid.zi" <<'ZI'
counter: s32;
initial: s32 = compute();
compute :: () -> s32 { counter += 1; return counter }
#program_export
Answer :: () -> s32 {
    return initial
}
ZI
"$ziran" ir --root "$work" -o "$work/runtime-ir" "$work/invalid.zi"
for input in "$work/invalid.zi" "$work/runtime-ir/invalid.zir"; do
    case "$input" in
        *.zi) root=$work ;;
        *) root=$work/runtime-ir ;;
    esac
    "$ziran" bundle --root "$root" --entry invalid:Answer \
        -o "$work/invalid.zx" "$input"
    test "$("$ziran" run "$work/invalid.zx")" = 1
done

cat > "$work/runtime_record.zi" <<'ZI'
Cell :: struct { value: s32; }
counter: s32;
Next :: () -> s32 { counter += 1; return counter }
cell: Cell = Cell.{.value = Next()};
#program_export
Answer :: () -> s32 { return cell.value }
ZI
cat > "$work/runtime_array.zi" <<'ZI'
Cell :: struct { value: s32; }
counter: s32;
Next :: () -> s32 { counter += 1; return counter }
cells: [1]Cell = Cell.[Cell.{.value = Next()}];
#program_export
Answer :: () -> s32 { return cells[0].value }
ZI
for name in runtime_record runtime_array; do
    "$ziran" ir --root "$work" -o "$work/runtime-ir" "$work/$name.zi"
    for input in "$work/$name.zi" "$work/runtime-ir/$name.zir"; do
        case "$input" in
            *.zi) root=$work ;;
            *) root=$work/runtime-ir ;;
        esac
        "$ziran" bundle --root "$root" --entry "$name:Answer" \
            -o "$work/$name.zx" "$input"
        test "$("$ziran" run "$work/$name.zx")" = 1
    done
done

cat > "$work/wrong_string.zi" <<'ZI'
bad: string = 42;
ZI
cat > "$work/wrong_bool.zi" <<'ZI'
bad: bool = 1;
ZI
cat > "$work/wrong_field.zi" <<'ZI'
Box :: struct { name: string; }
bad: Box = Box.{.name = 42};
ZI
cat > "$work/wrong_array.zi" <<'ZI'
bad: [1]string = string.[42];
ZI
for name in wrong_string wrong_bool wrong_field wrong_array; do
    if "$ziran" check --root "$work" "$work/$name.zi" \
        2> "$work/$name.err"; then
        echo "mismatched global initializer passed checking: $name" >&2
        exit 1
    fi
    rg -q 'initializer type mismatch|initializer field type mismatch|array initializer element type mismatch' \
        "$work/$name.err"
done

cat > "$work/session.zi" <<'ZI'
counter: s32;
#program_export
Answer :: () -> s32 {
    counter += 1
    return counter
}
ZI
"$ziran" ir --root "$work" -o "$work/session-ir" "$work/session.zi"
"$ziran" bundle --root "$work" --entry session:Answer \
    -o "$work/session-source.zx" "$work/session.zi"
"$ziran" bundle --root "$work/session-ir" --entry session:Answer \
    -o "$work/session-saved.zx" "$work/session-ir/session.zir"
cmp "$work/session-source.zx" "$work/session-saved.zx"
cat > "$work/instance.c" <<'C'
#include "ziran_host.h"
#include <assert.h>

static void check(BundleInstance *instance, long long expected)
{
    long long value = 0;
    int has_value = 0;
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == expected);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    BundleInstance *first = BundleInstantiate(bundle, NULL, 0);
    BundleInstance *second = BundleInstantiate(bundle, NULL, 0);
    assert(first != NULL && second != NULL);
    check(first, 1);
    check(first, 2);
    check(second, 1);
    check(first, 3);
    BundleInstanceClose(first);
    BundleInstanceClose(second);
    long long value = 0;
    int has_value = 0;
    assert(BundleRun(bundle, NULL, 0, &value, &has_value));
    assert(has_value && value == 1);
    assert(BundleRun(bundle, NULL, 0, &value, &has_value));
    assert(has_value && value == 1);
    BundleClose(bundle);
    return 0;
}
C
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    "$work/instance.c" "$ziran_lib" ${VM_LDFLAGS:-} -o "$work/instance"
"$work/instance" "$work/session-source.zx"
"$work/instance" "$work/session-saved.zx"
