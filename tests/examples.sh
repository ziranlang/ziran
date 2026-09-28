#!/bin/sh
set -eu

# Every site example with a main prints the same text on the portable VM
# and as C, C++, Go, Rust, and (with plan9port installed) Plan 9 C.
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
examples=$repo/site/examples
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

plan9=${PLAN9:-}
if test -z "$plan9"; then
    for candidate in "$HOME/Projects/plan9port" /usr/local/plan9 /usr/lib/plan9; do
        if test -x "$candidate/bin/9c"; then plan9=$candidate; break; fi
    done
fi

checked=0
for source in "$examples"/*.zi; do
    name=$(basename "$source" .zi)
    grep -q '^main :: ()' "$source" || continue
    out=$work/$name
    mkdir -p "$out"

    "$ziran" bundle --root "$examples" --entry "$name:main" -o "$out/$name.zib" "$source"
    "$ziran" run "$out/$name.zib" > "$out/expected"
    test -s "$out/expected" || { echo "$name printed nothing" >&2; exit 1; }

    "$ziran" build --target=c --root "$examples" -o "$out/c" "$source"
    printf '#include "%s.h"\nint main(void) { %s_main(); return 0; }\n' "$name" "$name" \
        > "$out/c/run.c"
    "${CC:-cc}" -std=c99 -I"$out/c" "$out/c/$name.c" "$out/c/run.c" -o "$out/c/program"
    "$out/c/program" > "$out/c.out"

    "$ziran" build --target=cpp --root "$examples" -o "$out/cpp" "$source"
    printf '#include "%s.hpp"\nint main() { %s_main(); return 0; }\n' "$name" "$name" \
        > "$out/cpp/run.cpp"
    "${CXX:-c++}" -std=c++17 -I"$out/cpp" "$out/cpp/$name.cpp" "$out/cpp/run.cpp" \
        -o "$out/cpp/program"
    "$out/cpp/program" > "$out/cpp.out"

    "$ziran" build --target=go --pkg main --exe --entry "$name:main" \
        --root "$examples" -o "$out/go" "$source"
    (cd "$out/go" && GO111MODULE=off go run .) > "$out/go.out"

    "$ziran" build --target=rust --exe --entry "$name:main" \
        --root "$examples" -o "$out/rust" "$source"
    CARGO_TARGET_DIR=$work/rust-target cargo build --quiet \
        --manifest-path "$out/rust/Cargo.toml"
    "$work/rust-target/debug/ziran_generated" > "$out/rust.out"

    for target in c cpp go rust; do
        if ! cmp -s "$out/expected" "$out/$target.out"; then
            echo "$name: $target output differs from the portable VM" >&2
            diff "$out/expected" "$out/$target.out" >&2 || true
            exit 1
        fi
    done

    if test -n "$plan9"; then
        "$ziran" build --target=plan9-c --root "$examples" -o "$out/plan9" "$source"
        printf '#include <u.h>\n#include <libc.h>\n#include "%s.h"\nvoid main(int argc, char **argv) { USED(argc); USED(argv); %s_main(); exits(nil); }\n' \
            "$name" "$name" > "$out/plan9/run.c"
        (cd "$out/plan9" &&
            PLAN9=$plan9 "$plan9/bin/9c" -I. "$name.c" run.c 2> 9c.log &&
            PLAN9=$plan9 "$plan9/bin/9l" -o program "$name.o" run.o) ||
            { cat "$out/plan9/9c.log" >&2; exit 1; }
        "$out/plan9/program" > "$out/plan9.out"
        cmp -s "$out/expected" "$out/plan9.out" || {
            echo "$name: plan9-c output differs from the portable VM" >&2
            diff "$out/expected" "$out/plan9.out" >&2 || true
            exit 1
        }
    fi
    checked=$((checked + 1))
done
test "$checked" -ge 10
