#!/bin/sh
# Statements are structured at parse time and again once imports are linked,
# before any procedure body is checked. A syntax error, and a C-style cast
# that names an imported type, are therefore reported ahead of a type error
# in an earlier procedure.
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cat > "$work/shapes.zi" <<'EOF'
Shape :: struct { width: s32; }
EOF
first_error() {
    cat > "$work/main.zi"
    if "$ziran" check --root "$work" "$work/main.zi" 2> "$work/err"; then
        echo "broken program accepted" >&2
        exit 1
    fi
    first=$(sed -n '1{s#^[^:]*/main\.zi:#main.zi:#;p;}' "$work/err")
    if [ "$first" != "$1" ]; then
        printf 'first error: %s\nexpected:    %s\n' "$first" "$1" >&2
        exit 1
    fi
}
first_error 'main.zi:7:5: Jai has no increment or decrement operators; use += 1 or -= 1' <<'EOF'
#import "shapes";
Early :: () -> s32 {
    return "text"
}
Late :: () -> s32 {
    value: s32 = 1;
    value++;
    return value
}
EOF
first_error 'main.zi:7:5: C-style cast or literal is not valid Jai syntax; use cast(Type) value or Type.{...}' <<'EOF'
#import "shapes";
Early :: () -> s32 {
    return "text"
}
Late :: () -> s32 {
    value: s32 = 1;
    shape := (Shape) value;
    return 0
}
EOF
first_error 'main.zi:3:5: return type mismatch: Early (expected s32, found string)' <<'EOF'
#import "shapes";
Early :: () -> s32 {
    return "text"
}
Late :: () -> s32 {
    value: s32 = 1;
    shape := cast(Shape) value;
    return 0
}
EOF
echo 'Syntax errors and imported-type casts are reported before procedure bodies are checked'
