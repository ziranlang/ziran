#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
#scope_export
Required :: (value: s32) -> s32 #must { return value }
Poly :: (value: $T) -> T #must { return value }
Optional :: () -> s32 { return 0 }
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
Library :: #import "lib";
#program_export
Answer :: () -> s32 {
    first: s32 = Required(20)
    second: s32 = Library.Poly(cast(s32)20)
    Optional()
    return first + second + 2
}
ZI
"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zx" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/saved.zx")" = 42

mkdir "$work/mixed"
cp "$work/ir/lib.zir" "$work/mixed/lib.zir"
for case_name in local named polymorphic wrapped unused; do
    case "$case_name" in
        local) expression='Required(1)' ;;
        named) expression='Library.Required(1)' ;;
        polymorphic) expression='Library.Poly(cast(s32)1)' ;;
        wrapped) expression='Library.Required(1) + 1' ;;
        unused) expression='unused Library.Required(1)' ;;
    esac
    cat > "$work/mixed/app.zi" <<ZI
#import "lib"
Library :: #import "lib";
Answer :: () -> s32 {
    $expression
    return 42
}
ZI
    if "$ziran" check --root "$work/mixed" "$work/mixed/app.zi" \
        2> "$work/$case_name.err"; then
        echo "ignored #must result passed: $case_name" >&2
        exit 1
    fi
    grep -Fq '#must return value is ignored' "$work/$case_name.err"
done

for target in c go; do
    "$ziran" build "--target=$target" --root "$work" \
        -o "$work/$target-source" "$work/app.zi"
    "$ziran" build "--target=$target" --root "$work/ir" \
        -o "$work/$target-saved" "$work/ir/app.zir"
    if "$ziran" build "--target=$target" --root "$work/mixed" \
        -o "$work/$target-invalid" "$work/mixed/app.zi" \
        2> "$work/$target-invalid.err"; then
        echo "$target accepted an ignored #must result" >&2
        exit 1
    fi
    grep -Fq '#must return value is ignored' "$work/$target-invalid.err"
done

mkdir "$work/optional"
cat > "$work/optional/lib.zi" <<'ZI'
#scope_export
Required :: (value: s32) -> s32 { return value }
ZI
cat > "$work/optional/app.zi" <<'ZI'
#import "lib"
Answer :: () -> s32 {
    Required(1)
    return 42
}
ZI
"$ziran" ir --root "$work/optional" -o "$work/optional/ir" \
    "$work/optional/app.zi"
cp "$work/optional/ir/app.zir" "$work/mixed/app.zir"
if "$ziran" check --root "$work/mixed" "$work/mixed/app.zir" \
    2> "$work/saved_ignored.err"; then
    echo 'saved IR ignored a newly required #must result' >&2
    exit 1
fi
grep -Fq '#must return value is ignored' "$work/saved_ignored.err"

cat > "$work/foreign.zi" <<'ZI'
host_api :: #system_library "host_api";
Foreign :: (value: s32) -> s32 #must #foreign host_api;
Answer :: () -> s32 {
    Foreign(1)
    return 42
}
ZI
if "$ziran" check --root "$work" "$work/foreign.zi" \
    2> "$work/foreign.err"; then
    echo 'ignored foreign #must result passed' >&2
    exit 1
fi
grep -Fq '#must return value is ignored' "$work/foreign.err"

cat > "$work/foreign_ok.zi" <<'ZI'
host_api :: #system_library "host_api";
Foreign :: (value: s32) -> s32 #must #foreign host_api;
Answer :: () -> s32 {
    result: s32 = Foreign(42)
    return result
}
ZI
"$ziran" ir --root "$work" -o "$work/foreign_ir" \
    "$work/foreign_ok.zi"
"$ziran" check --root "$work/foreign_ir" \
    "$work/foreign_ir/foreign_ok.zir"

cat > "$work/void.zi" <<'ZI'
Invalid :: () -> void #must { return }
ZI
if "$ziran" check --root "$work" "$work/void.zi" \
    2> "$work/void.err"; then
    echo 'void #must procedure passed' >&2
    exit 1
fi
grep -Fq '#must requires a return value' "$work/void.err"

cat > "$work/duplicate.zi" <<'ZI'
Invalid :: () -> s32 #must #must { return 1 }
ZI
if "$ziran" check --root "$work" "$work/duplicate.zi" \
    2> "$work/duplicate.err"; then
    echo 'duplicate #must modifier passed' >&2
    exit 1
fi
grep -Fq 'duplicate #must' "$work/duplicate.err"

cat > "$work/must_argument.zi" <<'ZI'
Invalid :: () -> s32 #must() { return 1 }
ZI
if "$ziran" check --root "$work" "$work/must_argument.zi" \
    2> "$work/must_argument.err"; then
    echo '#must accepted arguments' >&2
    exit 1
fi
grep -Fq '#must does not take arguments' "$work/must_argument.err"
