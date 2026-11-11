#!/bin/sh
# A returned slice, text view or record of views may borrow storage only the
# caller still owns after the call: a slice or string parameter, a view
# stored in a pointed-to record, or static text. A pointer parameter's own
# array storage is lent only for the call, so returning a view of it is
# rejected with a diagnostic naming that parameter. Accepted programs run the
# same from source and saved IR on the VM, C and Go.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

mkdir "$work/reject"
cat > "$work/reject/direct.zi" <<'ZI'
Owner :: struct { values: [4]u16; }
Borrow :: (owner: *Owner) -> []u16 { return owner.values[:] }
ZI
cat > "$work/reject/wrapped.zi" <<'ZI'
Owner :: struct { values: [4]u16; }
View :: struct { values: []u16; }
Borrow :: (owner: *Owner) -> View {
    result: View
    result.values = owner.values[:]
    return result
}
ZI
cat > "$work/reject/literal.zi" <<'ZI'
Owner :: struct { values: [4]u16; }
View :: struct { values: []u16; }
Borrow :: (owner: *Owner) -> View { return View.{values = owner.values[1:]} }
ZI
cat > "$work/reject/alias.zi" <<'ZI'
Owner :: struct { values: [4]u16; }
Borrow :: (owner: *Owner) -> []u16 { other := owner; return other.values[:] }
ZI
cat > "$work/reject/nested.zi" <<'ZI'
Inner :: struct { values: [4]u16; }
Outer :: struct { inner: [2]Inner; }
Borrow :: (outer: *Outer, at: s64) -> []u16 { return outer.inner[at].values[1:3] }
ZI
cat > "$work/reject/text.zi" <<'ZI'
Owner :: struct { bytes: [4]u8; }
Borrow :: (owner: *Owner) -> string { return TextView(owner.bytes[:]) }
ZI
cat > "$work/reject/value.zi" <<'ZI'
Owner :: struct { values: [4]u16; }
Borrow :: (owner: Owner) -> []u16 { return owner.values[:] }
ZI
cat > "$work/reject/caller.zi" <<'ZI'
Rows :: (values: []u16) -> []u16 { return values[1:] }
Borrow :: () -> []u16 { values: [4]u16; return Rows(values[:]) }
ZI
cat > "$work/reject/heap.zi" <<'ZI'
Owner :: struct { values: [4]u16; }
Borrow :: () -> []u16 { owner := New(Owner); return owner.values[:] }
ZI

# NAME PARAMETER-OR-MESSAGE: the check fails with check.slice_lifetime; a
# pointer case names the parameter in the message and in a related note.
expect_rejected() {
    if "$ziran" check --diagnostics=json --root "$work/reject" \
        "$work/reject/$1.zi" > "$work/$1.out" 2> "$work/$1.err"; then
        echo "$1 returned a view that outlives its storage" >&2
        exit 1
    fi
    "$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- lifetime-return "$work/$1.err" "$2"
}
expect_rejected direct pointer:owner
expect_rejected wrapped pointer:owner
expect_rejected literal pointer:owner
expect_rejected alias pointer:owner
expect_rejected nested pointer:outer
expect_rejected text pointer:owner
expect_rejected value 'returned slice borrows local or temporary storage'
expect_rejected caller 'returned slice borrows local or temporary storage'
expect_rejected heap 'through a pointer whose lifetime cannot be proved'
grep -q '"message":"returned text view borrows' "$work/text.err"
grep -q '"message":"returned value borrows' "$work/wrapped.err"

# Every target shares the rejection; no saved IR is written for it.
cat >> "$work/reject/direct.zi" <<'ZI'
main :: () -> s32 { owner: Owner; view := Borrow(*owner); return cast(s32) view[0] }
ZI
for command in "ir -o $work/direct-ir" "bundle --entry direct:main -o $work/direct.zx" \
    "build --target=c --entry direct:main --exe -o $work/direct-c" \
    "build --target=go --pkg main --entry direct:main --exe -o $work/direct-go"; do
    # shellcheck disable=SC2086
    if "$ziran" $command --root "$work/reject" "$work/reject/direct.zi" 2> "$work/target.err"; then
        echo "ziran $command accepted a borrowed pointer return" >&2
        exit 1
    fi
    grep -q 'behind pointer parameter owner' "$work/target.err"
done
test ! -e "$work/direct-ir/direct.zir"

# Return views of storage the caller keeps; use the pointee's own storage
# only inside the call.
mkdir "$work/accept"
cat > "$work/accept/borrowed.zi" <<'ZI'
Owner :: struct { values: [4]u16; label: string; rows: []u16; }
View :: struct { values: []u16; }
Rest :: (values: []u16) -> []u16 { return values[1:] }
Wrap :: (values: []u16) -> View {
    result: View
    result.values = values[2:]
    return result
}
Label :: (owner: *Owner) -> string { return owner.label }
Rows :: (owner: *Owner) -> []u16 { return owner.rows }
Fill :: (owner: *Owner) {
    view := owner.values[:]
    wrapped := View.{values = owner.values[:]}
    view[0] = 5
    wrapped.values[2] = 6
}
main :: () -> s32 {
    owner: Owner
    storage: [2]u16
    owner.label = "cdf"
    owner.rows = storage[:]
    rest := Rest(owner.values[:])
    wrapped := Wrap(owner.values[:])
    Fill(*owner)
    rest[0] = 7
    wrapped.values[1] = 9
    rows := Rows(*owner)
    rows[1] = 4
    if owner.values[0] != 5 || owner.values[1] != 7 || owner.values[2] != 6 ||
       owner.values[3] != 9 { return 1 }
    if Label(*owner) != "cdf" || storage[1] != 4 { return 2 }
    return 0
}
ZI
"$ziran" ir --root "$work/accept" -o "$work/accept-ir" "$work/accept/borrowed.zi"
for form in source saved; do
    input=$work/accept/borrowed.zi root=$work/accept
    if test "$form" = saved; then
        input=$work/accept-ir/borrowed.zir root=$work/accept-ir
    fi
    "$ziran" bundle --root "$root" --entry borrowed:main -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 0
    "$ziran" build --target=c --root "$root" --entry borrowed:main --exe \
        -o "$work/$form-c" "$input"
    "$work/$form-c/borrowed"
    "$ziran" build --target=go --pkg main --root "$root" --entry borrowed:main --exe \
        -o "$work/$form-go" "$input"
    GO111MODULE=off go run "$work/$form-go"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'Returned views name the pointer parameter that only lends its storage, and views of caller-owned storage run on every target'
