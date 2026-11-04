#!/bin/sh
set -eu
ziran=$1
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/unicode-data.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
export TMPDIR="$work" ZIRAN_NATIVE_DIR="$work" ZIRAN_NATIVE_CACHE=0
mkdir -p "$work/ucd"
cat > "$work/ucd/UnicodeData.txt" <<'EOF'
001C;<control>;Cc;0;B;;;;;N;INFORMATION SEPARATOR FOUR;;;;
0030;DIGIT ZERO;Nd;0;EN;;0;0;0;N;;;;;
0041;LATIN CAPITAL LETTER A;Lu;0;L;;;;;N;;;;0061;
0061;LATIN SMALL LETTER A;Ll;0;L;;;;;N;;;0041;;0041
00B2;SUPERSCRIPT TWO;No;0;EN;<super> 0032;;2;2;N;SUPERSCRIPT DIGIT TWO;;;;
00DF;LATIN SMALL LETTER SHARP S;Ll;0;L;;;;;N;;;;;
0130;LATIN CAPITAL LETTER I WITH DOT ABOVE;Lu;0;L;0049 0307;;;;N;LATIN CAPITAL LETTER I DOT;;;0069;
01C6;LATIN SMALL LETTER DZ WITH CARON;Ll;0;L;<compat> 0064 017E;;;;N;LATIN SMALL LETTER D Z HACEK;;01C4;;01C5
03A3;GREEK CAPITAL LETTER SIGMA;Lu;0;L;;;;;N;;;;03C3;
2160;ROMAN NUMERAL ONE;Nl;0;L;<compat> 0049;;;1;N;;;;2170;
3400;<CJK Ideograph Extension A, First>;Lo;0;L;;;;;N;;;;;
4DBF;<CJK Ideograph Extension A, Last>;Lo;0;L;;;;;N;;;;;
EOF
cat > "$work/ucd/SpecialCasing.txt" <<'EOF'
# SpecialCasing-17.0.0.txt
00DF; 00DF; 0053 0073; 0053 0053;
0130; 0069 0307; 0130; 0130;
03A3; 03C2; 03A3; 03A3; Final_Sigma;
0130; 0069; 0130; 0130; tr;
EOF
cat > "$work/ucd/CaseFolding.txt" <<'EOF'
# CaseFolding-17.0.0.txt
0041; C; 0061;
00DF; F; 0073 0073;
0130; F; 0069 0307;
0130; T; 0069;
1E9E; F; 0073 0073;
1E9E; S; 00DF;
EOF
cat > "$work/ucd/DerivedCoreProperties.txt" <<'EOF'
# DerivedCoreProperties-17.0.0.txt
0041; Cased
0061; Cased
00DF; Cased
0300..036F; Case_Ignorable
0345; Cased
EOF
"$ziran" build --root scripts --module-path std --target=c --exe \
    --entry unicode_data:main -o "$work/generator" "$repo/scripts/unicode_data.zi"
generator="$work/generator/unicode_data"
"$generator" "$work/ucd" "$work/tables.zi" "$work/cases.zi"
"$generator" "$work/ucd" "$work/repeat.zi" "$work/repeat-cases.zi"
cmp "$work/tables.zi" "$work/repeat.zi"
cmp "$work/cases.zi" "$work/repeat-cases.zi"
cat > "$work/probe.zi" <<'EOF'
D :: #import "tables";
Check :: (point: u32, mode: s32, expected: []u32) -> bool {
    mapping := D.Mapping(point, mode)
    if mapping.count != 13 || cast(s64)mapping[3] != expected.count { return false }
    index: s32
    while index < cast(s32)expected.count {
        if D.Point(mapping, 4 + index * 3) != expected[index] { return false }
        index += 1
    }
    return true
}
main :: () -> s32 {
    lower: [1]u32 = .[97]
    title: [1]u32 = .[453]
    sharp: [2]u32 = .[83, 115]
    fold: [2]u32 = .[115, 115]
    dotted: [2]u32 = .[105, 775]
    sigma: [1]u32 = .[963]
    if !Check(65, 1, lower[:]) || !Check(454, 2, title[:]) ||
       !Check(223, 2, sharp[:]) || !Check(223, 0, fold[:]) ||
       !Check(7838, 0, fold[:]) || !Check(304, 0, dotted[:]) ||
       !Check(304, 1, dotted[:]) || !Check(931, 1, sigma[:]) { return 1 }
    if D.Mapping(97, 1) != "" || D.Mapping(0, 0) != "" { return 2 }
    if D.Property(28) != 2 || D.Property(178) != 1 || D.Property(8544) != 1 ||
       D.Property(837) != 12 || D.Property(65) != 5 { return 3 }
    if D.Property(48) != 17 { return 5 }
    if D.Property(13312) != 1 || D.Property(19898) != 1 || D.Property(19903) != 1 ||
       D.Property(13311) != 0 || D.Property(19904) != 0 || D.Property(1114112) != 0 { return 4 }
    return 0
}
EOF
"$ziran" bundle --root "$work" --entry probe:main -o "$work/probe.zx" "$work/probe.zi"
test "$("$(dirname -- "$ziran")/zi2zx-vm" run "$work/probe.zx")" = 0
# A mismatched release or unsupported expansion must fail before replacing output.
printf 'preserve\n' > "$work/protected.zi"
cp "$work/protected.zi" "$work/expected.zi"
sed 's/17.0.0/16.0.0/' "$work/ucd/CaseFolding.txt" > "$work/old.txt"
cp "$work/ucd/CaseFolding.txt" "$work/current.txt"
cp "$work/old.txt" "$work/ucd/CaseFolding.txt"
if "$generator" "$work/ucd" "$work/protected.zi"; then exit 1; fi
cmp "$work/protected.zi" "$work/expected.zi"
cp "$work/current.txt" "$work/ucd/CaseFolding.txt"
printf '0042; F; 0061 0062 0063 0064;\n' >> "$work/ucd/CaseFolding.txt"
if "$generator" "$work/ucd" "$work/protected.zi"; then exit 1; fi
cmp "$work/protected.zi" "$work/expected.zi"
if test -n "${ZIRAN_UNICODE_UCD:-}"; then
    "$generator" "$ZIRAN_UNICODE_UCD" "$work/official.zi" "$work/official-cases.zi"
    cmp "$repo/std/unicode_data.zi" "$work/official.zi"
    cmp "$repo/tests/spec/unicode_conformance.zi" "$work/official-cases.zi"
    echo 'Unicode generator: official Unicode 17.0.0 outputs reproduce byte for byte'
fi
echo 'Unicode generator: native parsing, full mappings, range properties, deterministic output and rejection tests passed'
