#!/bin/sh
# A sibling import keeps its owner beside an unrelated same-leaf module.
set -eu
ziran=${1:?pass the ziran command}
mode=${2:-}
case $ziran in /*) ;; *) ziran=$PWD/$ziran ;; esac
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
mkdir -p "$work/app" "$work/std" "$work/library"
: > "$work/std/text.zi"
cat > "$work/std/shared.zi" <<'ZI'
Token :: struct { value: s32 }
Value :: () -> s32 { return 19 }
StandardOnly :: (token: Token) -> s32 { return token.value + Value() }
ZI
cat > "$work/library/shared.zi" <<'ZI'
Token :: struct { value: s32 }
Value :: () -> s32 { return 23 }
LibraryOnly :: (token: Token) -> s32 { return token.value + Value() }
ZI
cat > "$work/library/Exports.zi" <<'ZI'
using Local :: #import "shared";
ZI
cat > "$work/app/baseline.zi" <<'ZI'
#import "library/Exports"
#program_export
Answer :: () -> s32 { return LibraryOnly(Token.{value = 19}) }
ZI
"$ziran" check --root "$work/app" --module-path "$work/std" --module-path "std=$work/std" --module-path "$work/library" --module-path "library=$work/library" --module-path "synonym=$work/library" "$work/app/baseline.zi"
for roots in named-only standard-first library-first; do
    case $roots in
        named-only) set -- --module-path "$work/std" --module-path "std=$work/std" --module-path "library=$work/library" --module-path "synonym=$work/library"; short=19 ;;
        standard-first) set -- --module-path "$work/std" --module-path "std=$work/std" --module-path "$work/library" --module-path "library=$work/library" --module-path "synonym=$work/library"; short=19 ;;
        library-first) set -- --module-path "$work/library" --module-path "library=$work/library" --module-path "synonym=$work/library" --module-path "$work/std" --module-path "std=$work/std"; short=23 ;;
    esac
for order in standard-first library-first bare-first; do
    app=$work/app/app.zi
    if test "$order" = standard-first; then
        printf 'S :: #import "std/shared";\nL :: #import "library/Exports";\n' > "$app"
    elif test "$order" = library-first; then
        printf 'L :: #import "library/Exports";\nS :: #import "std/shared";\n' > "$app"
    else
        printf 'Short :: #import "shared";\nS :: #import "std/shared";\nL :: #import "library/Exports";\n' > "$app"
    fi
    if test "$order" != bare-first; then
        printf 'Short :: #import "shared";\n' >> "$app"
    fi
    cat >> "$app" <<'ZI'
Canonical :: #import "library/shared";
Alias :: #import "synonym/shared";
#program_export
Answer :: () -> s32 {
    standard: S.Token = .{value = 0}
    library: L.Token = .{value = 0}
    return S.StandardOnly(standard) + L.LibraryOnly(library) +
        Canonical.LibraryOnly(library) - Alias.Value()
}
ZI
    printf '#program_export\nShortAnswer :: () -> s32 { return Short.Value() - %s }\n' "$short" >> "$app"
    echo "module import identity: $roots roots, $order imports"
    "$ziran" check --root "$work/app" "$@" "$app"
    test "$mode" != --source-only || continue
    "$ziran" ir --root "$work/app" "$@" -o "$work/ir" "$app"
    modules=$(find "$work/ir" -name '*.zir' | wc -l)
    test "$modules" = 4 || { echo "expected four canonical modules, got $modules" >&2; exit 1; }
    for input in source saved; do
        if test "$input" = source; then
            native_root=$work/app
            file=$app
        else
            native_root=$work/ir
            file=$work/ir/app.zir
        fi
        "$ziran" bundle --root "$native_root" --entry app:Answer -o "$work/app.zx" "$@" "$file"
        test "$("$ziran" run "$work/app.zx")" = 42
        test "$("$ziran" run --vm "$work/app.zx")" = 42
        "$ziran" bundle --root "$native_root" --entry app:ShortAnswer -o "$work/short.zx" "$@" "$file"
        test "$("$ziran" run "$work/short.zx")" = 0
        test "$("$ziran" run --vm "$work/short.zx")" = 0
        "$ziran" build --root "$native_root" --target=c -o "$work/c" "$@" "$file"
        cat > "$work/c/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 && ShortAnswer() == 0 ? 0 : 1; }
C
        "${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/c" "$work/c"/*.c -o "$work/c/program"
        "$work/c/program"
    done
done
done
# Standard siblings must follow renamed identities with ordinary module roots.
cat > "$work/std/reader.zi" <<'ZI'
Shared :: #import "shared";
Read :: () -> s32 { return Shared.Value() }
ZI
set -- --module-path "$work/library" --module-path "$work/std"
for order in standard-first reader-first; do
    app=$work/app/app.zi
    case $order in
        standard-first) printf 'S :: #import "std/shared";\nR :: #import "std/reader";\n' > "$app" ;;
        reader-first) printf 'R :: #import "std/reader";\nS :: #import "std/shared";\n' > "$app" ;;
    esac
    cat >> "$app" <<'ZI'
#program_export
Answer :: () -> s32 { return S.Value() + R.Read() + 4 }
#program_export
ShortAnswer :: () -> s32 { return S.Value() - R.Read() }
ZI
    echo "plain standard identity: $order imports"
    "$ziran" check --root "$work/app" "$@" "$app"
    test "$mode" != --source-only || continue
    rm -rf "$work/ir"
    "$ziran" ir --root "$work/app" "$@" -o "$work/ir" "$app"
    test "$(find "$work/ir" -name '*.zir' | wc -l)" = 3
    for input in source saved; do
        if test "$input" = source; then
            native_root=$work/app
            file=$app
        else
            native_root=$work/ir
            file=$work/ir/app.zir
        fi
        "$ziran" bundle --root "$native_root" --entry app:Answer -o "$work/app.zx" "$@" "$file"
        test "$("$ziran" run "$work/app.zx")" = 42
        test "$("$ziran" run --vm "$work/app.zx")" = 42
        rm -rf "$work/c"
        "$ziran" build --root "$native_root" --target=c -o "$work/c" "$@" "$file"
        cat > "$work/c/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 && ShortAnswer() == 0 ? 0 : 1; }
C
        "${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/c" "$work/c"/*.c -o "$work/c/program"
        "$work/c/program"
    done
done
# Restore named roots for the rejection fixtures.
set -- --module-path "$work/std" --module-path "std=$work/std" --module-path "$work/library" --module-path "library=$work/library"
# Distinct module identities do not relax the existing rejection rules.
cat > "$work/app/ambiguous.zi" <<'ZI'
#import "std/shared"
#import "library/shared"
Answer :: () -> s32 { return Value() }
ZI
cat > "$work/app/duplicate_alias.zi" <<'ZI'
Same :: #import "std/shared";
Same :: #import "library/shared";
ZI
cat > "$work/app/wrong_owner.zi" <<'ZI'
S :: #import "std/shared";
L :: #import "library/shared";
Answer :: () -> s32 { return L.LibraryOnly(S.Token.{value = 0}) }
ZI
for reject in ambiguous duplicate_alias wrong_owner; do
    if "$ziran" check --root "$work/app" "$@" "$work/app/$reject.zi" > "$work/reject.log" 2>&1; then
        echo "accepted $reject" >&2
        exit 1
    fi
    cat "$work/reject.log"
done
mkdir -p "$work/a" "$work/b"
printf 'First :: () {}\n' > "$work/a/duplicate.zi"
printf 'Second :: () {}\n' > "$work/b/duplicate.zi"
if "$ziran" check --root "$work/a" "$work/a/duplicate.zi" "$work/b/duplicate.zi" > "$work/reject.log" 2>&1; then
    echo 'accepted duplicate module identities' >&2
    exit 1
fi
grep 'duplicate module identity' "$work/reject.log"
echo 'module import identity: independent same-leaf modules keep their symbols'
