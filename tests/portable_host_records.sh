#!/bin/sh
set -eu

ziran=$1
host_test=$2
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
include=${ZIRAN_INCLUDE:-"$root/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/shapes.zi" <<'EOF'
Tone :: enum {
    ToneNormal :: 0;
    ToneAccent :: 1;
}
Point :: struct {
    x: s32
    y: s32
}
Packet :: struct {
    point: Point
    label: string
    tone: Tone
}
State :: struct { values: [2]s32; }
EOF
cat > "$work/record_host.zi" <<'EOF'
host_api :: #system_library "host_api";
#import "shapes"
TransformHost :: (packet: Packet) -> Packet #foreign host_api;
SumSliceHost :: (points: []Point) -> s32 #foreign host_api;
ObserveHost :: (state: State, amount: s32) -> s32 #foreign host_api;
MutateState :: (values: []s32) -> s32 {
    values[0] = 99
    return 1
}
#program_export
Answer :: () -> s32 {
    using Tone;
    packet: Packet
    packet.point.x = 40
    packet.point.y = 1
    packet.label = "in"
    packet.tone = cast(Tone)ToneNormal
    transformed: Packet = TransformHost(packet)
    if transformed.point.x != 41 || transformed.point.y != 1 ||
       transformed.label != "ok" ||
       transformed.tone != cast(Tone)ToneAccent { return 0 }
    points: [2]Point
    points[0] = Point.{.x = 3, .y = 4}
    points[1] = transformed.point
    view: []Point = points[0:2]
    if SumSliceHost(view) != 49 { return 0 }
    state: State
    state.values[0] = 10
    values := state.values[:]
    if ObserveHost(state, MutateState(values)) != 101 { return 0 }
    return 42
}
EOF

"$ziran" ir --root "$work" -o "$work/ir" "$work/record_host.zi"
"$ziran" bundle --root "$work" --entry record_host:Answer \
    -o "$work/source.zib" "$work/record_host.zi"
"$ziran" bundle --root "$work/ir" --entry record_host:Answer \
    -o "$work/saved.zib" "$work/ir/record_host.zir"
cmp "$work/source.zib" "$work/saved.zib"
"$host_test" "$work/source.zib"
"$host_test" "$work/saved.zib"

for input in source saved; do
    if test "$input" = source; then
        module=$work/record_host.zi
        root=$work
    else
        module=$work/ir/record_host.zir
        root=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --root "$root" \
            -o "$output" "$module"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "record_host.h"
Packet TransformHost(Packet packet) {
    packet.point.x += 1;
    packet.label = StringView("ok", 2);
    packet.tone = (Tone)1;
    return packet;
}
int32_t SumSliceHost(Slice view) {
    int64_t total = 0;
    const Point *points = view.data;
    for(int64_t i = 0; i < view.length; i++)
        total += points[i].x + points[i].y;
    return (int32_t)total;
}
int32_t ObserveHost(State state, int32_t amount) {
    return state.values[0] * 10 + amount;
}
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$output" -I"$include" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "record_host.hpp"
extern "C" Packet TransformHost(Packet packet) {
    packet.point.x += 1;
    packet.label = StringView("ok", 2);
    packet.tone = (Tone)1;
    return packet;
}
extern "C" int32_t SumSliceHost(Slice view) {
    int64_t total = 0;
    const Point *points = static_cast<const Point *>(view.data);
    for(int64_t i = 0; i < view.length; i++)
        total += points[i].x + points[i].y;
    return static_cast<int32_t>(total);
}
extern "C" int32_t ObserveHost(State state, int32_t amount) {
    return state.values[0] * 10 + amount;
}
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$output" \
                -I"$include" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.go" <<'GO'
package ziran
type testHost struct{}
func (testHost) TransformHost(packet Packet) Packet {
    packet.Point.X++
    packet.Label = "ok"
    packet.Tone = ToneAccent
    return packet
}
func (testHost) SumSliceHost(points []Point) int32 {
    total := int64(0)
    for _, p := range points {
        total += int64(p.X + p.Y)
    }
    return int32(total)
}
func (testHost) ObserveHost(state State, amount int32) int32 {
    return state.Values[0] * 10 + amount
}
func init() { SetRecordHostHost(testHost{}) }
GO
            cat > "$output/record_host_test.go" <<'GO'
package ziran
import "testing"
func TestRecordHost(t *testing.T) {
    if RecordHost_Answer() != 42 { t.Fatal("record host result") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
