#!/bin/sh
# A Vec or Option of a #scope_module record is private to its module, not to
# the file that first spells it: every #load-ed file of the module reaches the
# same instance, can store it in its own records, and can index it.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/parts.zi" <<'ZI'
#import "vec"
#import "option"
#load "parts_a.zi";
#load "parts_b.zi";
Answer :: () -> s32 {
    parser: Parser
    c: Compiler
    c.p = *parser
    h: Holder
    unused VecPush(h.items, .{1, 4})
    return Use(*c) + Mirror(*h)
}
ZI
# Holder spells Vec(Instr) first, in parts_a.zi.
cat > "$work/parts_a.zi" <<'ZI'
#scope_module
Holder :: struct { items: Vec(Instr); }
Instr :: struct { op: u8; a: s32; }
Node :: struct { kind: u8; c: s32; str: s32; }
Parser :: struct { depth: s32; nodes: Vec(Node); }
AddNode :: (p: *Parser) -> s32 {
    at := p.nodes.count
    VecResize(p.nodes, at + 1)
    p.nodes[at].c = 1
    return cast(s32) at
}
ZI
# parts_b.zi stores, indexes and pops the instances parts_a.zi created.
cat > "$work/parts_b.zi" <<'ZI'
#scope_module
FS :: struct { code: Vec(Instr); depth: s32; }
Compiler :: struct { p: *Parser; depth: s32; }
Use :: (c: *Compiler) -> s32 {
    decl := AddNode(c.p)
    c.p.nodes[decl].str = 3
    p := c.p
    return p.nodes[decl].str + p.nodes[decl].c
}
Mirror :: (h: *Holder) -> s32 {
    f: FS
    unused VecPush(f.code, h.items[0])
    f.code[0].a = f.code[0].a * 8
    last := VecPop(f.code)
    if !last.has_value { return 0 }
    return last.value.a + cast(s32) last.value.op
}
ZI
cat > "$work/main.zi" <<'ZI'
#import "parts"
#program_export
Main :: () -> s32 { return Answer() }
ZI

"$ziran" check --root "$work" "$work/main.zi"
"$ziran" bundle --root "$work" --entry main:Main -o "$work/app.zx" "$work/main.zi"
test "$("$ziran" run "$work/app.zx")" = 37

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/main.zi"
cat > "$work/c/entry.c" <<'C'
#include "main.h"
int main(void) { return Main() == 37 ? 0 : 1; }
C
"${CC:-cc}" -Iinclude -I"$work/c" "$work/c"/*.c -o "$work/c/app"
"$work/c/app"
