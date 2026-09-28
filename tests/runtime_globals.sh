#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/library.zi" <<'ZI'
count: s32;
#scope_export
Advance :: () -> s32 { count += 1; return count }
#scope_export
ready: s32 = Advance();
#scope_export
Read :: () -> s32 { return ready }
#scope_export
Count :: () -> s32 { return count }
ZI
cat > "$work/side.zi" <<'ZI'
#import "library";
observed: s32 = Advance();
ZI
cat > "$work/relay.zi" <<'ZI'
#import "side";
ZI
cat > "$work/app.zi" <<'ZI'
#import "library";
#import "relay";
Cell :: struct { value: s32; }
count: s32;
Next :: () -> s32 { count += 1; return count }
first: s32 = Next();
second: Cell = Cell.{.value = Next()};
third: [1]Cell = Cell.[Cell.{.value = Next()}];
imported: s32 = ready;
imported_call: s32 = Read();
#program_export
Answer :: () -> s32 {
    if count != 3 || first != 1 || second.value != 2 ||
       third[0].value != 3 || imported != 1 || imported_call != 1 ||
       Count() != 2 { return 0 }
    return 42
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/app.zi
    else
        root=$work/ir
        file=$work/ir/app.zir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go rust; do
        out=$work/$target-$input
        if test "$target" = rust; then
            "$ziran" build --target="$target" --root "$root" \
                --entry app:Answer --exe \
                -o "$out" "$file"
        else
            "$ziran" build --target="$target" --root "$root" \
                --entry app:Answer \
                -o "$out" "$file"
        fi
        case "$target" in
            c)
                cat > "$out/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 && Answer() == 42 ? 0 : 1; }
C
                "${CC:-cc}" -std=c11 -pedantic-errors -I"$repo/include" \
                    -I"$out" "$out"/*.c -o "$out/program"
                "$out/program"
                ;;
            cpp)
                cat > "$out/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 && Answer() == 42 ? 0 : 1; }
CPP
                "${CXX:-c++}" -std=c++17 -I"$repo/include" \
                    -I"$out" "$out"/*.cpp -o "$out/program"
                "$out/program"
                ;;
            rust)
                "$ziran" build --target=rust --entry app:Answer --root "$root" \
                    --exe -o "$out" "$file"
                cargo build --quiet --manifest-path "$out/Cargo.toml"
                set +e
                "$out/target/debug/ziran_generated"
                status=$?
                set -e
                test "$status" -eq 42
                ;;
            go)
                cat > "$out/main_test.go" <<'GO'
package ziran
import "testing"
func TestRuntimeGlobals(t *testing.T) {
    if App_Answer() != 42 || App_Answer() != 42 { t.Fatal("startup order") }
}
GO
                GO111MODULE=off go test "$out"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zib" "$work/saved.zib"

cat > "$work/instance.c" <<'C'
#include "ziran_host.h"
#include <assert.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    for(int i = 0; i < 2; i++) {
        BundleInstance *instance = BundleInstantiate(bundle, NULL, 0);
        assert(instance != NULL);
        for(int run = 0; run < 2; run++) {
            long long value = 0;
            int has_result = 0;
            assert(BundleInstanceRun(instance, &value, &has_result));
            assert(has_result && value == 42);
        }
        BundleInstanceClose(instance);
    }
    BundleClose(bundle);
    return 0;
}
C
"${CC:-cc}" -std=c11 -I"$repo/include" "$work/instance.c" \
    "$repo/build/libziran.a" -o "$work/instance"
"$work/instance" "$work/source.zib"
"$work/instance" "$work/saved.zib"
