#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/hello.zi" <<'EOF'
#program_export
Image :: () -> s32 {
    return 42
}
#program_export
web_context_click_in_bounds :: () -> s32 {
    return Image()
}
#program_export
Answer :: () -> s32 {
    return web_context_click_in_bounds()
}
EOF

"$ziran" check --root "$work" "$work/hello.zi"
"$ziran" fmt --check "$work/hello.zi"
cat > "$work/wrapped.zi" <<'EOF'
#program_export
Add :: (first: s32,
    second: s32) -> s32 {
    return first + second
}
EOF
"$ziran" fmt --check "$work/wrapped.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/hello.zi"
"$ziran" ir --root "$work" -o "$work/ir-again" "$work/hello.zi"
cmp "$work/ir/hello.zir" "$work/ir-again/hello.zir"
test -s "$work/ir/hello.zir"
python3 - "$work/ir/hello.zir" "$repo/cmd/zir/zir_serial.c" <<'PY'
from pathlib import Path
import re
import sys
data = Path(sys.argv[1]).read_bytes()
version = int(re.search(r'^#define ZIR_FORMAT_VERSION (\d+)u$',
                        Path(sys.argv[2]).read_text(), re.M).group(1))
assert data[:8] == b'ZIR\0' + version.to_bytes(4, 'little'), data[:8]
PY
"$ziran" build --target=c --root "$work" -o "$work/c" "$work/hello.zi"
test -s "$work/c/hello.c"
grep -Fqx "#ifndef ZIRAN_HELLO_H" "$work/c/hello.h"
grep -Fq 'Image(' "$work/c/hello.c"
if grep -Fq 'RenderImage(' "$work/c/hello.c"; then
    echo 'generic Image call was rewritten as a UI host call' >&2
    exit 1
fi
cat > "$work/main.c" <<'EOF'
#include "hello.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
${CC:-cc} -Iinclude -I"$work/c" "$work/c/hello.c" "$work/main.c" -o "$work/hello"
"$work/hello"
"$ziran" build --target=c --root "$work" -o "$work/c-ir" "$work/ir/hello.zir"
${CC:-cc} -Iinclude -I"$work/c-ir" "$work/c-ir/hello.c" "$work/main.c" \
    -o "$work/hello-from-ir"
"$work/hello-from-ir"
"$ziran" build --target=cpp --root "$work" -o "$work/cpp" \
    "$work/hello.zi"
grep -Fqx "#ifndef ZIRAN_HELLO_H" "$work/cpp/hello.hpp"
cat > "$work/cpp/main.cpp" <<'EOF'
#include "hello.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
EOF
${CXX:-c++} -Iinclude -I"$work/cpp" "$work/cpp/hello.cpp" \
    "$work/cpp/main.cpp" -o "$work/hello-cpp"
"$work/hello-cpp"
"$ziran" build --target=cpp --root "$work" -o "$work/cpp-ir" \
    "$work/ir/hello.zir"
cp "$work/cpp/main.cpp" "$work/cpp-ir/main.cpp"
${CXX:-c++} -Iinclude -I"$work/cpp-ir" "$work/cpp-ir/hello.cpp" \
    "$work/cpp-ir/main.cpp" -o "$work/hello-cpp-ir"
"$work/hello-cpp-ir"
"$ziran" build --target=go --pkg main --root "$work" -o "$work/go" "$work/hello.zi"
test -s "$work/go/hello.go"
cat > "$work/go/main.go" <<'EOF'
package main
func main() { if Hello_Answer() != 42 { panic("wrong result") } }
EOF
GO111MODULE=off go run "$work/go/hello.go" "$work/go/main.go"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$work/go-ir" "$work/ir/hello.zir"
cp "$work/go/main.go" "$work/go-ir/main.go"
GO111MODULE=off go run "$work/go-ir/hello.go" "$work/go-ir/main.go"
"$ziran" build --target=go --root "$work" \
    -o "$work/go-default-package" "$work/hello.zi"
grep -Fxq 'package ziran' "$work/go-default-package/hello.go"

cat > "$work/ordinary_names.zi" <<'EOF'
ModalProps :: struct {
    text: string
}
TextFieldProps :: struct {
    text: string
    text_size: s32
}
TableViewProps :: struct {
    copy_text: string
}
Canvas :: struct {
    value: s32
}
CanvasResult :: struct {
    value: s32
}
tree_calls: s32;
#program_export
BeginTree :: () {
    tree_calls += 1
}
#program_export
EndTree :: () {
    tree_calls += 1
}
#program_export
NumericFloat :: () -> s32 {
    return 20
}
#program_export
DragSingle :: () -> s32 {
    return 22
}
#program_export
Answer :: () -> s32 {
    modal: ModalProps
    modal.text = "hello"
    field: TextFieldProps
    field.text = "hello"
    field.text_size = 5
    table: TableViewProps
    table.copy_text = "hello"
    if modal.text.count != 5 || cast(s32)field.text.count != field.text_size ||
        table.copy_text.count != 5 { return 0 }
    tree_calls = 0
    BeginTree()
    EndTree()
    canvas: Canvas
    canvas.value = NumericFloat() + DragSingle() - 2
    result: CanvasResult
    result.value = canvas.value + tree_calls
    return result.value
}
EOF
"$ziran" ir --root "$work" -o "$work/ordinary-ir" \
    "$work/ordinary_names.zi"
for input in "$work/ordinary_names.zi" "$work/ordinary-ir/ordinary_names.zir"; do
    output="$work/ordinary-go-$(basename "$input")"
    "$ziran" build --target=go --pkg main --root "$work" \
        -o "$output" "$input"
    if grep -Fq 'github.com/waozixyz/kryon' "$output/ordinary_names.go"; then
        echo 'ordinary names pulled in the Kryon Go runtime' >&2
        exit 1
    fi
    cat > "$output/main.go" <<'GO'
package main
func main() { if OrdinaryNames_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output/ordinary_names.go" "$output/main.go"
done
for input in "$work/ordinary_names.zi" "$work/ordinary-ir/ordinary_names.zir"; do
    for target in c cpp; do
        output="$work/ordinary-$target-$(basename "$input")"
        "$ziran" build --target="$target" --root "$work" \
            -o "$output" "$input"
        if rg -q 'ui_inspect_props.generated.h|__kryonContextClick' "$output"; then
            echo 'ordinary module received a Kryon compiler dependency' >&2
            exit 1
        fi
    done
done
for input in "$work/ordinary_names.zi" "$work/ordinary-ir/ordinary_names.zir"; do
    output="$work/ordinary-$(basename "$input").zib"
    "$ziran" bundle --root "$work" --entry ordinary_names:Answer \
        -o "$output" "$input"
    test "$("$ziran" run "$output")" = 42
done

cat > "$work/pointer_record.zi" <<'EOF'
Props :: struct {
    values: *s32
    label: *u8
}
#program_export
Answer :: () -> s32 {
    return 42
}
EOF
"$ziran" ir --root "$work" -o "$work/pointer-ir" \
    "$work/pointer_record.zi"
for input in "$work/pointer_record.zi" "$work/pointer-ir/pointer_record.zir"; do
    output="$work/pointer-cpp-$(basename "$input")"
    "$ziran" build --target=cpp --root "$work" \
        -o "$output" "$input"
    grep -Fq 'int32_t* values;' "$output/pointer_record.hpp"
    grep -Fq 'uint8_t* label;' "$output/pointer_record.hpp"
    cat > "$output/main.cpp" <<'CPP'
#include "pointer_record.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    ${CXX:-c++} -Iinclude -I"$output" \
        "$output/pointer_record.cpp" "$output/main.cpp" -o "$output/app"
    "$output/app"
done

cat > "$work/ffi_direct.zi" <<'EOF'
go_bits :: #system_library "math/bits";
#program_export
Reverse :: (value: u32) -> u32 #foreign go_bits "Reverse32";
#program_export
Answer :: () -> s32 {
    return cast(s32)Reverse(cast(u32)0x54000000)
}
EOF
cat > "$work/ffi_host.zi" <<'EOF'
host_api :: #system_library "host_api";
#program_export
Value :: () -> s32 #foreign host_api;
#program_export
Answer :: () -> s32 {
    return Value() + 1
}
EOF
"$ziran" ir --root "$work" -o "$work/ffi-ir" \
    "$work/ffi_direct.zi" "$work/ffi_host.zi"
for input in source ir; do
    if test "$input" = source; then
        extension=zi
        input_dir=$work
    else
        extension=zir
        input_dir=$work/ffi-ir
    fi
    "$ziran" build --target=go --pkg main --root "$work" \
        -o "$work/ffi-go-$input" "$input_dir/ffi_direct.$extension" \
        "$input_dir/ffi_host.$extension"
    if grep -Fq 'github.com/waozixyz/kryon' \
        "$work/ffi-go-$input/ffi_direct.go" \
        "$work/ffi-go-$input/ffi_host.go"; then
        echo 'Go FFI imported Kryon implicitly' >&2
        exit 1
    fi
    cat > "$work/ffi-go-$input/main.go" <<'GO'
package main
type host struct{}
func (host) Value() int32 { return 41 }
func main() {
    SetFfiHostHost(host{})
    if FfiDirect_Answer() != 42 || FfiHost_Answer() != 42 {
        panic("wrong FFI result")
    }
}
GO
    GO111MODULE=off go run "$work/ffi-go-$input/ffi_direct.go" \
        "$work/ffi-go-$input/ffi_host.go" "$work/ffi-go-$input/main.go"
done
if "$ziran" build --target=go --runtime-implementation --root "$work" \
    -o "$work/legacy-go" "$work/hello.zi" 2> "$work/legacy-go.err"; then
    echo 'legacy Kryon runtime mode unexpectedly passed' >&2
    exit 1
fi

cat > "$work/library.zi" <<'EOF'
#program_export
Button :: (value: s32) -> s32 {
    return value + 1
}
EOF
cat > "$work/app.zi" <<'EOF'
#import "library"
#program_export
Answer :: () -> s32 {
    return Button(41)
}
EOF
"$ziran" build --target=c --root "$work" -o "$work/modules" \
    "$work/library.zi" "$work/app.zi"
cat > "$work/modules/main.c" <<'EOF'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
${CC:-cc} -Iinclude -I"$work/modules" "$work/modules/library.c" \
    "$work/modules/app.c" "$work/modules/main.c" -o "$work/modules/app"
"$work/modules/app"

"$ziran" bundle --root "$work" --entry app:Answer -o "$work/app.zib" \
    "$work/library.zi" "$work/app.zi"
test "$("$ziran" run "$work/app.zib")" = 42
"$ziran" ir --root "$work" -o "$work/modules-ir" \
    "$work/library.zi" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer -o "$work/app-from-ir.zib" \
    "$work/modules-ir/library.zir" "$work/modules-ir/app.zir"
cmp "$work/app.zib" "$work/app-from-ir.zib"
test "$("$ziran" run "$work/app-from-ir.zib")" = 42
"$ziran" build --target=c --root "$work" -o "$work/mixed-modules" \
    "$work/library.zi" "$work/modules-ir/app.zir"
cp "$work/modules/main.c" "$work/mixed-modules/main.c"
${CC:-cc} -Iinclude -I"$work/mixed-modules" \
    "$work/mixed-modules/library.c" "$work/mixed-modules/app.c" \
    "$work/mixed-modules/main.c" -o "$work/mixed-modules/app"
"$work/mixed-modules/app"
cat > "$work/local_bundle.zi" <<'EOF'
#program_export
Answer :: () -> s32 {
    value: s32 = 40
    value = value + 2
    return value
}
EOF
"$ziran" bundle --root "$work" --entry local_bundle:Answer \
    -o "$work/local-bundle.zib" "$work/local_bundle.zi"
test "$("$ziran" run "$work/local-bundle.zib")" = 42
cat > "$work/flow.zi" <<'EOF'
#program_export
Answer :: () -> s32 {
    value: s32 = 0
    index: s32 = 0
    while index < 10 {
        index = index + 1
        if index < 3 {
            continue
        }
        if index > 5 {
            break
        }
        value = value + index
    }
    if value == 0 {
        return 0
    } else if value == 12 {
        return 42
    } else {
        return 0
    }
}
EOF
"$ziran" build --target=c --root "$work" -o "$work/flow-c" \
    "$work/flow.zi"
cat > "$work/flow-c/main.c" <<'EOF'
#include "flow.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
${CC:-cc} -Iinclude -I"$work/flow-c" "$work/flow-c/flow.c" \
    "$work/flow-c/main.c" -o "$work/flow-c/app"
"$work/flow-c/app"
"$ziran" build --target=cpp --root "$work" -o "$work/flow-cpp" \
    "$work/flow.zi"
cat > "$work/flow-cpp/main.cpp" <<'EOF'
#include "flow.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
EOF
${CXX:-c++} -Iinclude -I"$work/flow-cpp" "$work/flow-cpp/flow.cpp" \
    "$work/flow-cpp/main.cpp" -o "$work/flow-cpp/app"
"$work/flow-cpp/app"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$work/flow-go" "$work/flow.zi"
cat > "$work/flow-go/main.go" <<'EOF'
package main
func main() { if Flow_Answer() != 42 { panic("wrong result") } }
EOF
GO111MODULE=off go run "$work/flow-go/flow.go" "$work/flow-go/main.go"
"$ziran" bundle --root "$work" --entry flow:Answer \
    -o "$work/flow.zib" "$work/flow.zi"
test "$("$ziran" run "$work/flow.zib")" = 42
"$ziran" ir --root "$work" -o "$work/flow-ir" "$work/flow.zi"
"$ziran" bundle --root "$work" --entry flow:Answer \
    -o "$work/flow-from-ir.zib" "$work/flow-ir/flow.zir"
cmp "$work/flow.zib" "$work/flow-from-ir.zib"
test "$("$ziran" run "$work/flow-from-ir.zib")" = 42
cat > "$work/reals.zi" <<'EOF'
#program_export
Scale :: (value: float32) -> float32 {
    return value * 2.0
}
#program_export
Half :: (value: float64) -> float64 {
    return value / 2.0
}
Never :: () -> s32 {
    unreachable
}
#program_export
Answer :: () -> s32 {
    scaled: float32 = Scale(1.25)
    half: float64 = Half(7.0)
    if scaled != 2.5 || half != 3.5 { return 0 }
    if cast(s32)(scaled + 0.5) != 3 { return 0 }
    positive: bool = ifx scaled > 2.0 then true else false
    if !positive || 1.0 / 2.0 != 0.5 { return 0 }
    selected: s32 = ifx positive 42 else Never()
    if selected != 42 { return 0 }
    zero: s32 = ifx false then Never() else 0
    if zero != 0 { return 0 }
    return 42
}
EOF
"$ziran" build --target=c --root "$work" -o "$work/reals-c" \
    "$work/reals.zi"
cat > "$work/reals-c/main.c" <<'EOF'
#include "reals.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
${CC:-cc} -Iinclude -I"$work/reals-c" "$work/reals-c/reals.c" \
    "$work/reals-c/main.c" -o "$work/reals-c/app"
"$work/reals-c/app"
"$ziran" build --target=cpp --root "$work" -o "$work/reals-cpp" \
    "$work/reals.zi"
cat > "$work/reals-cpp/main.cpp" <<'EOF'
#include "reals.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
EOF
${CXX:-c++} -Iinclude -I"$work/reals-cpp" "$work/reals-cpp/reals.cpp" \
    "$work/reals-cpp/main.cpp" -o "$work/reals-cpp/app"
"$work/reals-cpp/app"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$work/reals-go" "$work/reals.zi"
cat > "$work/reals-go/main.go" <<'EOF'
package main
func main() { if Reals_Answer() != 42 { panic("wrong real result") } }
EOF
GO111MODULE=off go run "$work/reals-go/reals.go" "$work/reals-go/main.go"
"$ziran" bundle --root "$work" --entry reals:Answer \
    -o "$work/reals.zib" "$work/reals.zi"
test "$("$ziran" run "$work/reals.zib")" = 42
"$ziran" ir --root "$work" -o "$work/reals-ir" "$work/reals.zi"
"$ziran" bundle --root "$work" --entry reals:Answer \
    -o "$work/reals-from-ir.zib" "$work/reals-ir/reals.zir"
cmp "$work/reals.zib" "$work/reals-from-ir.zib"
test "$("$ziran" run "$work/reals-from-ir.zib")" = 42
cat > "$work/reachable_library.zi" <<'EOF'
Props :: struct {
    value: s32
}
#program_export
UnusedRecord :: (props: Props) -> s32 {
    return props.value
}
#program_export
Scale :: (value: float32) -> float32 {
    return value * 1.5
}
EOF
cat > "$work/dead_library.zi" <<'EOF'
#program_export
UnusedDead :: () -> s32 {
    return 99
}
EOF
cat > "$work/reachable_app.zi" <<'EOF'
#import "reachable_library"
#program_export
Answer :: () -> s32 {
    if Scale(2.0) == 3.0 { return 42 }
    return 0
}
EOF
"$ziran" bundle --root "$work" --entry reachable_app:Answer \
    -o "$work/reachable.zib" "$work/reachable_library.zi" \
    "$work/dead_library.zi" "$work/reachable_app.zi"
test "$("$ziran" run "$work/reachable.zib")" = 42
"$ziran" ir --root "$work" -o "$work/reachable-ir" \
    "$work/reachable_library.zi" "$work/dead_library.zi" \
    "$work/reachable_app.zi"
"$ziran" bundle --root "$work" --entry reachable_app:Answer \
    -o "$work/reachable-from-ir.zib" \
    "$work/reachable-ir/reachable_library.zir" \
    "$work/reachable-ir/dead_library.zir" \
    "$work/reachable-ir/reachable_app.zir"
cmp "$work/reachable.zib" "$work/reachable-from-ir.zib"
test "$("$ziran" run "$work/reachable-from-ir.zib")" = 42
python3 - "$work/reachable.zib" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
assert b'UnusedRecord' not in data
assert b'dead_library' not in data
assert b'UnusedDead' not in data
PY
cat > "$work/record_shapes.zi" <<'EOF'
Point :: struct {
    x: s32
}
Rectangle :: struct {
    origin: Point
    width: s32
}
Unused :: struct {
    value: s32
}
EOF
cat > "$work/record_operations.zi" <<'EOF'
#import "record_shapes"
#program_export
Measure :: (rect: Rectangle) -> s32 {
    return rect.origin.x + rect.width
}
#program_export
Widen :: (rect: Rectangle) -> Rectangle {
    out: Rectangle = rect
    out.width = 100
    return out
}
EOF
cat > "$work/record_application.zi" <<'EOF'
#import "record_shapes"
#import "record_operations"
#program_export
Answer :: () -> s32 {
    rect: Rectangle
    rect.origin.x = 2
    rect.width = 40
    wide: Rectangle = Widen(rect)
    if rect.width != 40 || wide.width != 100 { return 0 }
    return Measure(rect)
}
EOF
"$ziran" bundle --root "$work" --entry record_application:Answer \
    -o "$work/record.zib" "$work/record_shapes.zi" \
    "$work/record_operations.zi" "$work/record_application.zi"
test "$("$ziran" run "$work/record.zib")" = 42
"$ziran" ir --root "$work" -o "$work/record-ir" \
    "$work/record_shapes.zi" "$work/record_operations.zi" \
    "$work/record_application.zi"
"$ziran" bundle --root "$work" --entry record_application:Answer \
    -o "$work/record-from-ir.zib" \
    "$work/record-ir/record_shapes.zir" \
    "$work/record-ir/record_operations.zir" \
    "$work/record-ir/record_application.zir"
cmp "$work/record.zib" "$work/record-from-ir.zib"
test "$("$ziran" run "$work/record-from-ir.zib")" = 42
cat > "$work/modes.zi" <<'EOF'
Mode :: enum {
    Off :: -1;
    On;
    Later :: On + 4;
}
Settings :: struct {
    mode: Mode
    number: s32
}
#program_export
Bump :: (settings: Settings) -> Settings {
    out: Settings = settings
    out.number += 2
    return out
}
EOF
cat > "$work/mode_app.zi" <<'EOF'
#import "modes"
#program_export
Answer :: () -> s32 {
    using Mode;
    if Off != -1 || On != 0 || Later != 4 { return 0 }
    settings: Settings = Settings.{mode = cast(Mode)Later, number = 40}
    updated: Settings = Bump(settings)
    if settings.number != 40 || updated.number != 42 { return 0 }
    if updated.mode != cast(Mode)Later { return 0 }
    return updated.number
}
EOF
"$ziran" bundle --root "$work" --entry mode_app:Answer \
    -o "$work/modes.zib" "$work/modes.zi" "$work/mode_app.zi"
test "$("$ziran" run "$work/modes.zib")" = 42
"$ziran" ir --root "$work" -o "$work/modes-ir" \
    "$work/modes.zi" "$work/mode_app.zi"
"$ziran" bundle --root "$work" --entry mode_app:Answer \
    -o "$work/modes-from-ir.zib" \
    "$work/modes-ir/modes.zir" "$work/modes-ir/mode_app.zir"
cmp "$work/modes.zib" "$work/modes-from-ir.zib"
test "$("$ziran" run "$work/modes-from-ir.zib")" = 42
cat > "$work/direct_enum.zi" <<'EOF'
Step :: enum {
    First :: 3;
    Second;
    Last :: Second + 4;
}
#program_export
Answer :: () -> s32 {
    using Step;
    return Last
}
EOF
"$ziran" bundle --root "$work" --entry direct_enum:Answer \
    -o "$work/direct-enum.zib" "$work/direct_enum.zi"
test "$("$ziran" run "$work/direct-enum.zib")" = 8
cat > "$work/unsigned_bits.zi" <<'EOF'
#program_export
Answer :: () -> s32 {
    value: u32 = cast(u32)0x80000000
    if cast(u32)1 << cast(u32)31 != value { return 0 }
    if value >> cast(u32)31 != cast(u32)1 { return 0 }
    value |= cast(u32)3
    if value != cast(u32)0x80000003 { return 0 }
    value &= ~cast(u32)1
    if value != cast(u32)0x80000002 { return 0 }
    value ^= cast(u32)2
    if value != cast(u32)0x80000000 { return 0 }
    value >>= cast(u32)31
    byte: u8 = cast(u8)258
    if byte != cast(u8)2 { return 0 }
    negative: s32 = -8
    if negative >> 2 != -2 { return 0 }
    return cast(s32)(value + cast(u32)41)
}
EOF
"$ziran" bundle --root "$work" --entry unsigned_bits:Answer \
    -o "$work/unsigned-bits.zib" "$work/unsigned_bits.zi"
test "$("$ziran" run "$work/unsigned-bits.zib")" = 42
"$ziran" ir --root "$work" -o "$work/unsigned-bits-ir" \
    "$work/unsigned_bits.zi"
"$ziran" bundle --root "$work" --entry unsigned_bits:Answer \
    -o "$work/unsigned-bits-from-ir.zib" \
    "$work/unsigned-bits-ir/unsigned_bits.zir"
cmp "$work/unsigned-bits.zib" "$work/unsigned-bits-from-ir.zib"
test "$("$ziran" run "$work/unsigned-bits-from-ir.zib")" = 42
"$ziran" build --target=c --root "$work" \
    -o "$work/unsigned-bits-c" "$work/unsigned_bits.zi"
cat > "$work/unsigned-bits-c/main.c" <<'EOF'
#include "unsigned_bits.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
${CC:-cc} -Iinclude -I"$work/unsigned-bits-c" \
    "$work/unsigned-bits-c/unsigned_bits.c" \
    "$work/unsigned-bits-c/main.c" -o "$work/unsigned-bits-c/app"
"$work/unsigned-bits-c/app"
"$ziran" build --target=cpp --root "$work" \
    -o "$work/unsigned-bits-cpp" "$work/unsigned_bits.zi"
cat > "$work/unsigned-bits-cpp/main.cpp" <<'EOF'
#include "unsigned_bits.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
EOF
${CXX:-c++} -Iinclude -I"$work/unsigned-bits-cpp" \
    "$work/unsigned-bits-cpp/unsigned_bits.cpp" \
    "$work/unsigned-bits-cpp/main.cpp" -o "$work/unsigned-bits-cpp/app"
"$work/unsigned-bits-cpp/app"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$work/unsigned-bits-go" "$work/unsigned_bits.zi"
cat > "$work/unsigned-bits-go/main.go" <<'EOF'
package main
func main() { if UnsignedBits_Answer() != 42 { panic("wrong bits result") } }
EOF
GO111MODULE=off go run "$work/unsigned-bits-go/unsigned_bits.go" \
    "$work/unsigned-bits-go/main.go"
cat > "$work/u64_boundary.zi" <<'EOF'
#program_export
Answer :: () -> s32 {
    high: u64 = cast(u64)0x8000000000000000
    maximum: u64 = cast(u64)0xffffffffffffffff
    if high <= cast(u64)0x7fffffffffffffff { return 0 }
    if maximum <= high { return 0 }
    if maximum + cast(u64)1 != cast(u64)0 { return 0 }
    if high >> cast(u64)63 != cast(u64)1 { return 0 }
    if cast(u64)1 << cast(u64)63 != high { return 0 }
    if (maximum & high) != high { return 0 }
    if (maximum ^ high) != cast(u64)0x7fffffffffffffff { return 0 }
    value: u64 = maximum
    value -= cast(u64)41
    if value / cast(u64)2 != cast(u64)9223372036854775787 { return 0 }
    if cast(u32)maximum != cast(u32)0xffffffff { return 0 }
    return 42
}
EOF
"$ziran" ir --root "$work" -o "$work/u64-ir" "$work/u64_boundary.zi"
"$ziran" bundle --root "$work" --entry u64_boundary:Answer \
    -o "$work/u64.zib" "$work/u64_boundary.zi"
"$ziran" bundle --root "$work" --entry u64_boundary:Answer \
    -o "$work/u64-ir.zib" "$work/u64-ir/u64_boundary.zir"
cmp "$work/u64.zib" "$work/u64-ir.zib"
test "$("$ziran" run "$work/u64.zib")" = 42
test "$("$ziran" run "$work/u64-ir.zib")" = 42
for input in source ir; do
    if test "$input" = source; then
        extension=zi
        input_dir=$work
    else
        extension=zir
        input_dir=$work/u64-ir
    fi
    "$ziran" build --target=c --root "$work" \
        -o "$work/u64-c-$input" "$input_dir/u64_boundary.$extension"
    cat > "$work/u64-c-$input/main.c" <<'C'
#include "u64_boundary.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    ${CC:-cc} -Iinclude -I"$work/u64-c-$input" \
        "$work/u64-c-$input/u64_boundary.c" \
        "$work/u64-c-$input/main.c" -o "$work/u64-c-$input/app"
    "$work/u64-c-$input/app"
    "$ziran" build --target=cpp --root "$work" \
        -o "$work/u64-cpp-$input" "$input_dir/u64_boundary.$extension"
    cat > "$work/u64-cpp-$input/main.cpp" <<'CPP'
#include "u64_boundary.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    ${CXX:-c++} -Iinclude -I"$work/u64-cpp-$input" \
        "$work/u64-cpp-$input/u64_boundary.cpp" \
        "$work/u64-cpp-$input/main.cpp" -o "$work/u64-cpp-$input/app"
    "$work/u64-cpp-$input/app"
    "$ziran" build --target=go --pkg main --root "$work" \
        -o "$work/u64-go-$input" "$input_dir/u64_boundary.$extension"
    cat > "$work/u64-go-$input/main.go" <<'GO'
package main
func main() { if U64Boundary_Answer() != 42 { panic("wrong u64 result") } }
GO
    GO111MODULE=off go run "$work/u64-go-$input/u64_boundary.go" \
        "$work/u64-go-$input/main.go"
done
python3 - "$work/flow-ir/flow.zir" "$work/changed-text.zir" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
assert data.count(b'return 42') == 1
assert data.count(b'else if value == 12') == 1
data = data.replace(b'return 42', b'return xx', 1)
Path(sys.argv[2]).write_bytes(data.replace(b'else if value == 12',
                                             b'xxxx if value == 12', 1))
PY
for target in c cpp go; do
    if test "$target" = go; then
        "$ziran" build --target=go --pkg main --root "$work" \
            -o "$work/changed-text-go" "$work/changed-text.zir"
    else
        "$ziran" build "--target=$target" --root "$work" \
            -o "$work/changed-text-$target" "$work/changed-text.zir"
    fi
done
cp "$work/flow-c/main.c" "$work/changed-text-c/main.c"
${CC:-cc} -Iinclude -I"$work/changed-text-c" \
    "$work/changed-text-c/flow.c" "$work/changed-text-c/main.c" \
    -o "$work/changed-text-c/app"
"$work/changed-text-c/app"
cp "$work/flow-cpp/main.cpp" "$work/changed-text-cpp/main.cpp"
${CXX:-c++} -Iinclude -I"$work/changed-text-cpp" \
    "$work/changed-text-cpp/flow.cpp" "$work/changed-text-cpp/main.cpp" \
    -o "$work/changed-text-cpp/app"
"$work/changed-text-cpp/app"
cp "$work/flow-go/main.go" "$work/changed-text-go/main.go"
GO111MODULE=off go run "$work/changed-text-go/flow.go" \
    "$work/changed-text-go/main.go"
"$ziran" bundle --root "$work" --entry flow:Answer \
    -o "$work/changed-text.zib" "$work/changed-text.zir"
test "$("$ziran" run "$work/changed-text.zib")" = 42
python3 - "$work/flow.zib" "$work/changed-embedded-text.zib" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
assert data.count(b'return 42') == 1
assert data.count(b'else if value == 12') == 1
data = data.replace(b'return 42', b'return xx', 1)
Path(sys.argv[2]).write_bytes(data.replace(b'else if value == 12',
                                             b'xxxx if value == 12', 1))
PY
test "$("$ziran" run "$work/changed-embedded-text.zib")" = 42
python3 - "$work/flow-ir/flow.zir" "$work/inconsistent-flow.zir" <<'PY'
from pathlib import Path
import sys
data = bytearray(Path(sys.argv[1]).read_bytes())
assert data.count(b'42') == 2
position = data.rfind(b'42')
assert position > data.index(b'return 42') + len(b'return 42')
data[position:position + 2] = b'43'
Path(sys.argv[2]).write_bytes(data)
PY
for target in c cpp go; do
    if test "$target" = go; then
        "$ziran" build --target=go --pkg main --root "$work" \
            -o "$work/inconsistent-go" "$work/inconsistent-flow.zir"
    else
        "$ziran" build "--target=$target" --root "$work" \
            -o "$work/inconsistent-$target" "$work/inconsistent-flow.zir"
    fi
done
sed 's/42/43/g' "$work/flow-c/main.c" > "$work/inconsistent-c/main.c"
${CC:-cc} -Iinclude -I"$work/inconsistent-c" \
    "$work/inconsistent-c/flow.c" "$work/inconsistent-c/main.c" \
    -o "$work/inconsistent-c/app"
"$work/inconsistent-c/app"
sed 's/42/43/g' "$work/flow-cpp/main.cpp" > "$work/inconsistent-cpp/main.cpp"
${CXX:-c++} -Iinclude -I"$work/inconsistent-cpp" \
    "$work/inconsistent-cpp/flow.cpp" "$work/inconsistent-cpp/main.cpp" \
    -o "$work/inconsistent-cpp/app"
"$work/inconsistent-cpp/app"
sed 's/42/43/g' "$work/flow-go/main.go" > "$work/inconsistent-go/main.go"
GO111MODULE=off go run "$work/inconsistent-go/flow.go" \
    "$work/inconsistent-go/main.go"
"$ziran" bundle --root "$work" --entry flow:Answer \
    -o "$work/inconsistent-flow.zib" "$work/inconsistent-flow.zir"
test "$("$ziran" run "$work/inconsistent-flow.zib")" = 43
python3 - "$work/flow.zib" "$work/inconsistent-embedded.zib" <<'PY'
from pathlib import Path
import sys
data = bytearray(Path(sys.argv[1]).read_bytes())
assert data.count(b'42') == 2
data[data.rfind(b'42'):data.rfind(b'42') + 2] = b'43'
Path(sys.argv[2]).write_bytes(data)
PY
test "$("$ziran" run "$work/inconsistent-embedded.zib")" = 43
python3 - "$work/app.zib" "$work/bad-bundle.zib" "$work/old-bundle.zib" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
assert data[:8] == b'ZIB\0\x19\0\0\0', data[:8]
Path(sys.argv[2]).write_bytes(data[:17])
old = bytearray(data)
old[4] = 19
Path(sys.argv[3]).write_bytes(old)
PY
if "$ziran" run "$work/bad-bundle.zib" 2> "$work/bad-bundle.err"; then
    echo 'truncated bundle unexpectedly ran' >&2
    exit 1
fi
grep -Fq 'invalid or truncated bundle' "$work/bad-bundle.err"
if "$ziran" run "$work/old-bundle.zib" 2> "$work/old-bundle.err"; then
    echo 'unsupported bundle version unexpectedly ran' >&2
    exit 1
fi
grep -Fq 'unsupported ZIB version' "$work/old-bundle.err"
cat > "$work/unsupported_bundle.zi" <<'EOF'
#program_export
Answer :: () -> s32 {
    value: *s32 = null
    if value == null { return 42 }
    return value.*
}
EOF
# Pointers at portable storage, null included, run in bundles.
"$ziran" bundle --root "$work" --entry unsupported_bundle:Answer \
    -o "$work/unsupported-bundle.zib" "$work/unsupported_bundle.zi"
test "$("$ziran" run "$work/unsupported-bundle.zib")" = 42

cat > "$work/blocklib.zi" <<'EOF'
Props :: struct {
    value: s32
}
#program_export
Button :: (props: Props) -> s32 {
    return props.value + 1
}
EOF
cat > "$work/blockapp.zi" <<'EOF'
#import "blocklib"
#program_export
Answer :: () -> s32 {
    Button(Props.{value = 1})
    chosen: s32 = Button(Props.{value = 41})
    return chosen
}
EOF
"$ziran" build --target=c --root "$work" -o "$work/blocks" \
    "$work/blocklib.zi" "$work/blockapp.zi"
grep -Fq 'Button((Props){.value = 41})' "$work/blocks/blockapp.c"
cat > "$work/blocks/main.c" <<'EOF'
#include "blockapp.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
${CC:-cc} -Iinclude -I"$work/blocks" "$work/blocks/blocklib.c" \
    "$work/blocks/blockapp.c" "$work/blocks/main.c" -o "$work/blocks/app"
"$work/blocks/app"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$work/blocks-go" "$work/blocklib.zi" "$work/blockapp.zi"
grep -Fq 'Blocklib_Button(' "$work/blocks-go/blockapp.go"
if grep -Fq 'github.com/waozixyz/kryon/go/kryon' \
    "$work/blocks-go/blocklib.go" "$work/blocks-go/blockapp.go"; then
    echo 'ordinary Go record call pulled in the legacy Kryon runtime' >&2
    exit 1
fi
cat > "$work/blocks-go/main.go" <<'EOF'
package main
func main() { if Blockapp_Answer() != 42 { panic("wrong result") } }
EOF
GO111MODULE=off go run "$work/blocks-go/blocklib.go" \
    "$work/blocks-go/blockapp.go" "$work/blocks-go/main.go"

"$ziran" ir --root "$work" -o "$work/blocks-ir" \
    "$work/blocklib.zi" "$work/blockapp.zi"
"$ziran" build --target=c --root "$work" -o "$work/blocks-from-ir" \
    "$work/blocks-ir/blocklib.zir" "$work/blocks-ir/blockapp.zir"
cp "$work/blocks/main.c" "$work/blocks-from-ir/main.c"
${CC:-cc} -Iinclude -I"$work/blocks-from-ir" \
    "$work/blocks-from-ir/blocklib.c" "$work/blocks-from-ir/blockapp.c" \
    "$work/blocks-from-ir/main.c" -o "$work/blocks-from-ir/app"
"$work/blocks-from-ir/app"
"$ziran" bundle --root "$work" --entry blockapp:Answer \
    -o "$work/blocks-source.zib" "$work/blocklib.zi" "$work/blockapp.zi"
"$ziran" bundle --root "$work/blocks-ir" --entry blockapp:Answer \
    -o "$work/blocks-saved.zib" "$work/blocks-ir/blocklib.zir" \
    "$work/blocks-ir/blockapp.zir"
cmp "$work/blocks-source.zib" "$work/blocks-saved.zib"
test "$("$ziran" run "$work/blocks-source.zib")" = 42
test "$("$ziran" run "$work/blocks-saved.zib")" = 42
cat > "$work/void_block.zi" <<'EOF'
Props :: struct {
    value: s32
}
#program_export
Apply :: (props: Props) {
}
Main :: () {
    Apply named: {
        value = 1
    }
}
EOF
if "$ziran" check --root "$work" "$work/void_block.zi" \
    2> "$work/void_block.err"; then
    echo 'named block call unexpectedly passed' >&2
    exit 1
fi
grep -Fq 'block calls are not Jai syntax' "$work/void_block.err"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$work/blocks-go-ir" "$work/blocks-ir/blocklib.zir" \
    "$work/blocks-ir/blockapp.zir"
cp "$work/blocks-go/main.go" "$work/blocks-go-ir/main.go"
GO111MODULE=off go run "$work/blocks-go-ir/blocklib.go" \
    "$work/blocks-go-ir/blockapp.go" "$work/blocks-go-ir/main.go"
"$ziran" build --target=cpp --root "$work" -o "$work/blocks-cpp" \
    "$work/blocklib.zi" "$work/blockapp.zi"
cat > "$work/blocks-cpp/main.cpp" <<'EOF'
#include "blockapp.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
EOF
${CXX:-c++} -Iinclude -I"$work/blocks-cpp" \
    "$work/blocks-cpp/blocklib.cpp" "$work/blocks-cpp/blockapp.cpp" \
    "$work/blocks-cpp/main.cpp" -o "$work/blocks-cpp/app"
"$work/blocks-cpp/app"
"$ziran" build --target=cpp --root "$work" -o "$work/blocks-cpp-ir" \
    "$work/blocks-ir/blocklib.zir" "$work/blocks-ir/blockapp.zir"
cp "$work/blocks-cpp/main.cpp" "$work/blocks-cpp-ir/main.cpp"
${CXX:-c++} -Iinclude -I"$work/blocks-cpp-ir" \
    "$work/blocks-cpp-ir/blocklib.cpp" "$work/blocks-cpp-ir/blockapp.cpp" \
    "$work/blocks-cpp-ir/main.cpp" -o "$work/blocks-cpp-ir/app"
"$work/blocks-cpp-ir/app"

python3 - "$work/blocks-ir/blockapp.zir" "$work/corrupt.zir" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
Path(sys.argv[2]).write_bytes(data[:17])
PY
if "$ziran" build --target=c --root "$work" -o "$work/corrupt-out" \
    "$work/corrupt.zir" 2> "$work/corrupt.err"; then
    echo 'truncated IR unexpectedly passed' >&2
    exit 1
fi
grep -Eq 'truncated|invalid' "$work/corrupt.err"
python3 - "$work/blocks-ir/blockapp.zir" "$work/version.zir" <<'PY'
from pathlib import Path
import sys
data = bytearray(Path(sys.argv[1]).read_bytes())
data[4] = 31
Path(sys.argv[2]).write_bytes(data)
PY
if "$ziran" build --target=c --root "$work" -o "$work/version-out" \
    "$work/version.zir" 2> "$work/version.err"; then
    echo 'unsupported IR version unexpectedly passed' >&2
    exit 1
fi
grep -Fq 'unsupported ZIR version' "$work/version.err"

cat > "$work/bad.zi" <<'EOF'
#assert 0, "expected failure"
EOF
if "$ziran" check --diagnostics=json --root "$work" "$work/bad.zi" \
    2> "$work/diagnostic.json"; then
    echo 'invalid source unexpectedly passed' >&2
    exit 1
fi
python3 -m json.tool "$work/diagnostic.json" > /dev/null

cat > "$work/ui_mode.zi" <<'EOF'
Screen :: () #ui {
}
EOF
if "$ziran" check --root "$work" "$work/ui_mode.zi" \
    2> "$work/ui_mode.err"; then
    echo '#ui unexpectedly passed in Ziran' >&2
    exit 1
fi
grep -Fq 'unknown function modifier: #ui' "$work/ui_mode.err"

cat > "$work/extern_ui_mode.zi" <<'EOF'
host_api :: #system_library "host_api";
Effect :: () #foreign host_api; #ui
EOF
if "$ziran" check --root "$work" "$work/extern_ui_mode.zi" \
    2> "$work/extern_ui_mode.err"; then
    echo '#ui extern unexpectedly passed in Ziran' >&2
    exit 1
fi
grep -Fq 'unknown directive: #ui' "$work/extern_ui_mode.err"

cat > "$work/unknown_directive.zi" <<'EOF'
#style defaults
EOF
if "$ziran" check --root "$work" "$work/unknown_directive.zi" \
    2> "$work/unknown_directive.err"; then
    echo 'unknown directive unexpectedly passed in Ziran' >&2
    exit 1
fi
grep -Fq 'unknown directive: #style defaults' "$work/unknown_directive.err"

cat > "$work/unknown_top_level.zi" <<'EOF'
route home {
}
EOF
if "$ziran" check --root "$work" "$work/unknown_top_level.zi" \
    2> "$work/unknown_top_level.err"; then
    echo 'unknown top-level declaration unexpectedly passed in Ziran' >&2
    exit 1
fi
grep -Fq 'invalid top-level declaration' "$work/unknown_top_level.err"

cat > "$work/unknown_app.zi" <<'EOF'
app main {
}
EOF
if "$ziran" check --root "$work" "$work/unknown_app.zi" \
    2> "$work/unknown_app.err"; then
    echo 'old app block unexpectedly passed in Ziran' >&2
    exit 1
fi
grep -Fq 'invalid top-level declaration' "$work/unknown_app.err"

cat > "$work/intrinsic_mode.zi" <<'EOF'
web_context_click_in_bounds :: (x0: s32, y0: s32, x1: s32,
    y1: s32) -> int #intrinsic "web"
EOF
if "$ziran" check --root "$work" "$work/intrinsic_mode.zi" \
    2> "$work/intrinsic_mode.err"; then
    echo '#intrinsic unexpectedly passed in Ziran' >&2
    exit 1
fi
grep -Fq 'unknown function modifier: #intrinsic' "$work/intrinsic_mode.err"

cat > "$work/instance_mode.zi" <<'EOF'
State :: struct {
    value: s32
}
Main :: () {
    state: State #instance(7)
}
EOF
if "$ziran" check --root "$work" "$work/instance_mode.zi" \
    2> "$work/instance_mode.err"; then
    echo '#instance unexpectedly passed in Ziran' >&2
    exit 1
fi
grep -Fq 'unknown declaration modifier: #instance' "$work/instance_mode.err"

cat > "$work/unknown_block.zi" <<'EOF'
Main :: () {
    Missing: {
        value = 41
    }
}
EOF
if "$ziran" check --root "$work" "$work/unknown_block.zi" \
    2> "$work/unknown_block.err"; then
    echo 'unknown block call unexpectedly passed' >&2
    exit 1
fi
grep -Fq 'block calls are not Jai syntax' "$work/unknown_block.err"
