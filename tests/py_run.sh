#!/bin/sh
# Script arguments, interpreter selection, source locations, and cache reuse.
set -eu
ziran=$(realpath "${1:?pass the ziran command}")
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
export XDG_CACHE_HOME="$work/cache"
export PYTHONDONTWRITEBYTECODE=1
export PYTHONPATH="$work${PYTHONPATH:+:$PYTHONPATH}"
export PATH="$(dirname "$ziran"):$PATH"

cat > "$work/helper.zi" <<'ZI'
Value :: () -> s32 { return 41 }
ZI
cat > "$work/local_host.py" <<'PY'
def value():
    return 9
PY
cat > "$work/script.zi" <<'ZI'
#!/usr/bin/env -S ziran run --target=py
#import "helper"
Args :: #import "std/args_py";
Text :: #import "std/text_py";
local :: #system_library "py:local_host";
Local :: () -> s32 #foreign local "value";
#program_export
main :: () -> s32 {
    print("%:%:%:%\n", Args.ScriptFile(), Text.Join("|", Args.Arguments()), Value(), Local())
    return 0
}
ZI
chmod +x "$work/script.zi"
cd "$work"
expected="$work/script.zi:quotes ' \" ; \$(literal) * spaces|--offline|--project|--root|x.zi:41:9"
result=$("$ziran" run --target=py script.zi "quotes ' \" ; \$(literal) * spaces" --offline --project --root x.zi)
test "$result" = "$expected"
ready=$(rg --files --hidden -g .ready "$XDG_CACHE_HOME/ziran/python")
test "$(printf '%s\n' "$ready" | wc -l)" -eq 1
output=${ready%/.ready}/__main__.py
before=$(stat -c %y "$output")
test "$("$ziran" run --target=py script.zi -- a b)" = "$work/script.zi:a|b:41:9"
test "$(stat -c %y "$output")" = "$before"
test "$(./script.zi shebang)" = "$work/script.zi:shebang:41:9"
test "$(stat -c %y "$output")" = "$before"

# Python source imports use the original script directory, even though the
# executable is cached elsewhere. Python-only edits need no recompilation.
sed -i 's/return 9/return 7/' local_host.py
test "$("$ziran" run --target=py script.zi)" = "$work/script.zi::41:7"
test "$(stat -c %y "$output")" = "$before"
"$ziran" run --target=py --no-cache script.zi >/dev/null
test ! -e "$ready"
"$ziran" run --target=py script.zi >/dev/null
test -f "$ready"

# A dependency edit invalidates the compiled script. A failed rebuild never
# runs the output from the previous successful dependency revision.
sed -i 's/return 41/return 42/' helper.zi
test "$("$ziran" run --target=py script.zi)" = "$work/script.zi::42:7"
test "$(rg --files --hidden -g .ready "$XDG_CACHE_HOME/ziran/python" | wc -l)" -eq 2
printf 'Value :: () -> s32 { return missing }\n' > helper.zi
if "$ziran" run --target=py script.zi > failed.out 2> failed.err; then
    echo 'failed script compilation succeeded' >&2; exit 1
fi
test ! -s failed.out
grep -Fq missing failed.err
printf 'Value :: () -> s32 { return 42 }\n' > helper.zi
test "$("$ziran" run --target=py script.zi)" = "$work/script.zi::42:7"

# Explicit interpreter paths are passed as one argument.
mkdir 'interpreter with spaces'
cat > 'interpreter with spaces/python' <<'SH'
#!/bin/sh
printf used > "$ZIRAN_TEST_PYTHON_MARKER"
exec python3 "$@"
SH
chmod +x 'interpreter with spaces/python'
export ZIRAN_TEST_PYTHON_MARKER="$work/interpreter-used"
test "$("$ziran" run --target=py --python "$work/interpreter with spaces/python" script.zi selected)" = "$work/script.zi:selected:42:7"
test "$(cat "$ZIRAN_TEST_PYTHON_MARKER")" = used
rm "$ZIRAN_TEST_PYTHON_MARKER"
ZIRAN_PYTHON="$work/interpreter with spaces/python" "$ziran" run --target=py script.zi >/dev/null
test -f "$ZIRAN_TEST_PYTHON_MARKER"

# Saved IR has the same entry convention and retains source locations.
"$ziran" ir --root "$work" -o "$work/ir" script.zi
test "$("$ziran" run --target=py ir/script.zir saved)" = "$work/ir/script.zir:saved:42:7"
cat > exit.zi <<'ZI'
#!/usr/bin/env -S ziran run --target=py
#program_export
main :: () -> s32 { return 17 }
ZI
code=0
"$ziran" run --target=py exit.zi || code=$?
test "$code" -eq 17
cat > broken.zi <<'ZI'
#!/usr/bin/env -S ziran run --target=py
json :: #system_library "py:json";
#import "std/py_types"
Load :: (text: string) -> Object #foreign json "loads";
#program_export
main :: () -> s32 {
    Load("{bad")
    return 0
}
ZI
if "$ziran" run --target=py broken.zi > broken.out 2> broken.err; then
    echo 'uncaught script exception succeeded' >&2; exit 1
fi
grep -Fq "$work/broken.zi\", line 7" broken.err
grep -Fq JSONDecodeError broken.err
"$ziran" ir --root "$work" -o "$work/broken-ir" broken.zi
if "$ziran" run --target=py broken-ir/broken.zir > saved.out 2> saved.err; then
    echo 'uncaught saved script exception succeeded' >&2; exit 1
fi
grep -Fq 'broken.zi", line 7' saved.err
if grep -Fq "$work/broken-ir/broken.zi" saved.err; then
    echo 'traceback invented a source file beside the saved IR' >&2; exit 1
fi

# Named module roots participate in invalidation, including a newly added
# module that shadows an earlier fallback.
mkdir imports root
cat > imports/value.zi <<'ZI'
Answer :: () -> s32 { return 1 }
ZI
cat > root/app.zi <<'ZI'
#import "value"
#program_export
main :: () -> s32 { print("%\n", Answer()); return 0 }
ZI
test "$("$ziran" run --target=py --module-path "$work/imports" root/app.zi)" = 1
sed -i 's/return 1/return 2/' imports/value.zi
test "$("$ziran" run --target=py --module-path "$work/imports" root/app.zi)" = 2
cat > root/value.zi <<'ZI'
Answer :: () -> s32 { return 3 }
ZI
test "$("$ziran" run --target=py --module-path "$work/imports" root/app.zi)" = 3

# Files loaded outside the root bypass cache publication, so they cannot
# reuse code compiled against an older external file.
mkdir loaded
printf 'Answer :: () -> s32 { return 4 }\n' > external.zi
cat > loaded/app.zi <<'ZI'
#load "../external.zi";
#program_export
main :: () -> s32 { print("%\n", Answer()); return 0 }
ZI
test "$("$ziran" run --target=py loaded/app.zi)" = 4
sed -i 's/return 4/return 5/' external.zi
test "$("$ziran" run --target=py loaded/app.zi)" = 5

# Projects use the local canonical toolchain override and project entry.
mkdir -p project/src
cat > project/ziran.toml <<'TOML'
[package]
name = "PythonScriptTest"
entry = "src/main.zi"
module_roots = ["src"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
TOML
printf '[overrides]\nziran = "%s"\n' "$repo" > project/ziran.local.toml
cat > project/src/main.zi <<'ZI'
Args :: #import "std/args_py";
#program_export
main :: () -> s32 {
    arguments := Args.Arguments()
    if arguments.count != 1 || arguments[0] != "--project" { return 1 }
    print("project script\n")
    return 0
}
ZI
cd project
"$ziran" lock
test "$("$ziran" run --target=py -- --project)" = 'project script'
test "$("$ziran" run --target=py --project src/main.zi -- --project)" = 'project script'
if "$ziran" run --target=py --locked -- --project > locked.out 2> locked.err; then
    echo 'locked mode accepted local overrides' >&2; exit 1
fi
grep -Fq 'local overrides are not allowed with --locked' locked.err
