#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/lib/native_answer.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Node :: struct { value: s64; }
Carrier :: struct { using node: *Node; }
Read :: (carrier: Carrier) -> s64 { return carrier.value }
#program_export
Answer :: () -> s64 {
    node := Node.{.value = 42}
    carrier := Carrier.{.node = *node}
    carrier.value = 41
    carrier.value += 1
    return Read(carrier)
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) suffix=source; root=$work ;;
        *) suffix=saved; root=$work/ir ;;
    esac
    for target in c go; do
        out="$work/$target-$suffix"
        native_answer_42 "$target" "$root" "$out" "$input" \
            app App_Answer 'using pointer'
    done
done
