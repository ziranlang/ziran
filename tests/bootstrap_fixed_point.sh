#!/bin/sh
# The bootstrap fixed-point proof: seed comparison, the stage-2 fixed point,
# stale seed files, the runtime boundary, and update refusal/publish/recovery.
# A fake compiler and make keep the check fast; the real proof runs from
# `make check-bootstrap`.
set -eu
ziran=${1:-}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
script="$repo/scripts/bootstrap_fixed_point.py"
work=$(mktemp -d "$repo/build/tests/bootstrap-fixed-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/bin" "$work/source" "$work/seeds/compiler_a" "$work/seeds/compiler_b"

cat > "$work/source/a.zi" <<'ZI'
A :: 1;
ZI
cat > "$work/source/b.zi" <<'ZI'
B :: 2;
ZI
relative=${work#"$repo"/}
cat > "$work/manifest" <<EOF
a	compiler	$relative/source/a.zi	a.c a.h
b	compiler	$relative/source/b.zi	b.c b.h
EOF

cat > "$work/bin/compiler" <<'SH'
#!/bin/sh
out=""
src=""
while test $# -gt 0; do
    case "$1" in
        -o) out=$2; shift 2 ;;
        --entry|--root|--module-path|--target-os|--target-cpu|--define) shift 2 ;;
        --*) shift ;;
        *) src=$1; shift ;;
    esac
done
test -n "$out" && test -n "$src"
test -f "$src"
test "${FAKE_FAIL:-0}" = 0 || { echo "fake compiler failure" >&2; exit 1; }
name=$(basename "$src" .zi)
generation=1
if test "${FAKE_STAGE2:-0}" = 1 && test "${FAKE_MISMATCH:-0}" = 1; then generation=2; fi
mkdir -p "$out"
printf '/* fake %s generation=%s */\n' "$name" "$generation" > "$out/$name.c"
printf '/* fake %s header generation=%s */\n' "$name" "$generation" > "$out/$name.h"
SH
chmod +x "$work/bin/compiler"

cat > "$work/bin/make" <<'SH'
#!/bin/sh
build=""
for argument in "$@"; do
    case "$argument" in
        BUILD_DIR=*) build=${argument#BUILD_DIR=} ;;
    esac
done
test -n "$build"
mkdir -p "$build/bin"
cat > "$build/bin/zi2c" <<EOF
#!/bin/sh
FAKE_STAGE2=1 exec "$FAKE_COMPILER" "\$@"
EOF
chmod +x "$build/bin/zi2c"
SH
chmod +x "$work/bin/make"

seed() {
    printf '/* fake %s generation=1 */\n' "$1" > "$work/seeds/compiler_$1/$1.c"
    printf '/* fake %s header generation=1 */\n' "$1" > "$work/seeds/compiler_$1/$1.h"
}
seed a
seed b

FAKE_COMPILER="$work/bin/compiler"
export FAKE_COMPILER
run() {
    python3 "$script" "$@" --manifest "$work/manifest" --zi2c "$work/bin/compiler" \
        --seed-root "$work/seeds" --scratch "$work/scratch" --make "$work/bin/make"
}
expect_ok() {
    label=$1
    shift
    if ! "$@" > "$work/$label.log" 2>&1; then
        echo "$label: expected success" >&2
        cat "$work/$label.log" >&2
        exit 1
    fi
}
expect_fail() {
    label=$1
    pattern=$2
    shift 2
    if "$@" > "$work/$label.log" 2>&1; then
        echo "$label: expected failure" >&2
        exit 1
    fi
    if ! grep -q "$pattern" "$work/$label.log"; then
        echo "$label: no '$pattern' in output" >&2
        cat "$work/$label.log" >&2
        exit 1
    fi
}

expect_ok clean run check
FAKE_MISMATCH=1 expect_fail mismatch 'stage 2 does not reproduce stage 1' run check

printf '/* stale */\n' > "$work/seeds/compiler_a/old.c"
expect_fail stale 'stale seed file old.c' run check
rm -f "$work/seeds/compiler_a/old.c"

mkdir -p "$work/runtime-ok" "$work/runtime-bad"
cat > "$work/runtime-ok/ziran_bootstrap_runtime.h" <<'C'
int ZiranBootstrapStart(int argc, char **argv);
C
cat > "$work/runtime-ok/ziran_bootstrap_runtime.c" <<'C'
#include "ziran_bootstrap_runtime.h"
int ZiranBootstrapStart(int argc, char **argv) { (void)argc; (void)argv; return 0; }
C
expect_ok runtime run check --runtime "$work/runtime-ok"
cp "$work/runtime-ok/"* "$work/runtime-bad/"
cat >> "$work/runtime-bad/ziran_bootstrap_runtime.c" <<'C'
int ParseProgram(char *text) { (void)text; return 0; }
C
expect_fail boundary 'ParseProgram is not declared' run check --runtime "$work/runtime-bad"

before=$(cksum "$work/seeds/compiler_a/a.c")
expect_fail refused 'refusing to update' run update --check-command false
test "$before" = "$(cksum "$work/seeds/compiler_a/a.c")"
test ! -e "$work/seeds.next"

# A stale proof stage must not publish either, and leaves the seeds intact.
FAKE_FAIL=1 expect_fail stage-fail 'exited 1' run update --check-command true
test "$before" = "$(cksum "$work/seeds/compiler_a/a.c")"

# An interrupted publish leaves its pending tree behind; the next update
# discards it and publishes the proved stage.
printf '/* fake a generation=0 */\n' > "$work/seeds/compiler_a/a.c"
mkdir -p "$work/seeds.next/compiler_a"
printf 'garbage\n' > "$work/seeds.next/compiler_a/a.c"
expect_ok publish run update --check-command true
test ! -e "$work/seeds.next"
grep -q 'generation=1' "$work/seeds/compiler_a/a.c"
cmp "$work/seeds/compiler_a/a.h" "$work/scratch/stage1/compiler_a/a.h"
expect_ok republish run check

echo 'bootstrap fixed point: manifest order, seed match, stage-2 reproduction, stale seeds, runtime boundary, refusal and atomic update passed'
