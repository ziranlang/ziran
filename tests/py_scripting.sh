#!/bin/sh
# Exercise scripting APIs and foreign conversion with source and saved IR.
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
export PYTHONPATH="$work${PYTHONPATH:+:$PYTHONPATH}"

cat > "$work/scripting_host.py" <<'PY'
def echo(values):
    return values

def mutate(values):
    values[0] = 'changed'
    return values

def numbers():
    return [-129, 128, 257]

def invalid_numbers():
    return [1, 'invalid']

def keyword(left, *, right):
    return f'{left}:{right}'

def callback(function, values):
    return function(values)
PY
cat > "$work/script.zi" <<'ZI'
#import "std/py_types"
#import "std/hash_map"
Collections :: #import "std/collections_py";
Maps :: #import "std/map_py";
Files :: #import "std/file_py";
JSON :: #import "std/json_py";
Regex :: #import "std/regex_py";
Process :: #import "std/process_py";
Args :: #import "std/args_py";
Text :: #import "std/text_py";
Time :: #import "std/time_py";
host :: #system_library "py:scripting_host";
types :: #system_library "py:types";
Words :: (values: []string) -> []string #foreign host "echo";
Mutate :: (values: []string) -> []string #foreign host "mutate";
Numbers :: () -> []s8 #foreign host "numbers";
BadNumbers :: struct { value: []s32; error: string }
TryNumbers :: () -> BadNumbers #py_results #foreign host "invalid_numbers";
Keyword :: (left: string, right: s64) -> string #foreign host "keyword";
Callback :: #type (values: []string) -> []string;
CallBack :: (function: Callback, values: []string) -> []string #foreign host "callback";
Namespace :: () -> Object #foreign types "SimpleNamespace";
SetName :: (object: Object, value: string) #py_field #foreign types "(SimpleNamespace).name";
Name :: (object: Object) -> string #py_field #foreign types "(SimpleNamespace).name";
order: s32;
Number :: (digit: s32) -> s64 { order = order * 10 + digit; return digit }
Word :: (digit: s32) -> string { order = order * 10 + digit; return "word" }
Receiver :: (object: Object, digit: s32) -> Object { order = order * 10 + digit; return object }
Echo :: (values: []string) -> []string { return values }

#program_export
main :: () -> s32 {
    words: [2]string = .["café", "你好"]
    back := Words(words[:])
    if back.count != 2 || back[0] != words[0] || back[1] != words[1] { return 1 }
    changed := Mutate(words[:])
    if words[0] != "café" || changed[0] != "changed" { return 2 }
    empty: [0]string
    if Words(empty[:]).count != 0 { return 3 }
    numbers := Numbers()
    if numbers.count != 3 || numbers[0] != cast(s8)127 || numbers[1] != cast(s8)-128 || numbers[2] != cast(s8)1 { return 4 }
    if TryNumbers().error == "" { return 6 }
    callback := CallBack(Echo, words[:])
    if callback.count != 2 || callback[1] != "你好" { return 7 }
    if Keyword(right = Number(2), left = Word(1)) != "word:2" || order != 21 { return 8 }
    object := Namespace()
    order = 0
    SetName(value = Word(1), object = Receiver(object, 2))
    if Name(object = object) != "word" || order != 12 { return 9 }
    loaded := JSON.Load("[null, 42]")
    if loaded.error != "" { return 10 }
    items := Collections.Items(loaded.value)
    if items.count != 2 || items[0] != null { return 11 }
    iterator := Collections.Iterate(loaded.value)
    first := Collections.Next(iterator)
    second := Collections.Next(iterator)
    if first.done || first.value != null || second.done || Collections.Text(second.value) != "42" || !Collections.Next(iterator).done { return 12 }
    ages: HashMap(string, s64)
    HashMapSet(*ages, "ada", cast(s64)36)
    dictionary := Maps.ToDictionary(*ages)
    copy := Maps.IntegerMap(dictionary)
    age: s64
    if !HashMapGet(*copy, "ada", *age) || age != 36 { return 13 }
    Collections.SetInteger(dictionary, "ada", 37)
    if !HashMapGet(*ages, "ada", *age) || age != 36 { return 14 }
    HashMapFree(*ages); HashMapFree(*copy)
    names: HashMap(string, string)
    HashMapSet(*names, "name", "café")
    named := Maps.StringMap(Maps.ToDictionary(*names))
    if Collections.GetText(Maps.ToDictionary(*named), "name") != "café" { return 15 }
    HashMapFree(*names); HashMapFree(*named)
    text := JSON.Dump(JSON.Load("{\"z\": 2, \"a\": \"café\"}").value, sort_keys = true)
    if text.error != "" || text.value != "{\"a\": \"café\", \"z\": 2}" || JSON.Dump(dictionary, pretty = true).value == "" { return 16 }
    if JSON.Load("{bad").error == "" { return 17 }
    if Text.Upper("straße") != "STRASSE" || Text.Lower("É") != "é" || Text.Strip("  zi \n") != "zi" { return 18 }
    if Text.Join("-", Text.Split("a:b", ":")) != "a-b" || Text.Lines("a\nb\n").count != 2 || Text.Replace("aba", "a", "é") != "ébé" { return 19 }
    matches := Regex.FindTexts("[a-z]+", "ab 12 cd")
    if matches.count != 2 || matches[0] != "ab" || matches[1] != "cd" { return 20 }
    if Regex.Replace("[0-9]+", "x", "a12b34", count = 1).value != "axb34" || Regex.Search("[", "x").error == "" { return 21 }
    groups := Regex.FindAll("([a-z])([0-9])", "a1 b2")
    if groups.error != "" || Collections.Length(groups.value) != 2 { return 22 }
    directory := Files.TemporaryDirectory(prefix = "ziran-py-")
    if directory.error != "" || !Files.DirectoryExists(directory.value) { return 23 }
    defer Files.RemoveTree(directory.value)
    subdirectory := Files.Join(directory.value, "nested")
    if Files.CreateDirectory(subdirectory).error != "" || Files.CreateDirectory(subdirectory).error != "" { return 24 }
    file := Files.Join(subdirectory, "café.txt")
    if Files.WriteText(file, "你好 café").error != "" || Files.ReadText(file).value != "你好 café" { return 25 }
    binary: [4]u8 = .[0, 255, 1, 128]
    binary_file := Files.Join(subdirectory, "bytes.bin")
    if Files.WriteBytes(binary_file, binary[:]).error != "" { return 26 }
    bytes := Files.ReadBytes(binary_file)
    if bytes.error != "" || bytes.value.count != 4 || bytes.value[1] != cast(u8)255 { return 27 }
    paths := Files.Glob(Files.Join(directory.value, "**/*.txt"), recursive = true)
    if paths.count != 1 || paths[0] != file || Files.Parent(file) != subdirectory { return 28 }
    copied := Files.Join(subdirectory, "copy.txt")
    renamed := Files.Join(subdirectory, "renamed.txt")
    if Files.Copy(file, copied).error != "" || Files.Rename(copied, renamed).error != "" || !Files.PathExists(renamed) { return 29 }
    if Files.Remove(renamed).error != "" || Files.ReadText(renamed).error == "" { return 30 }
    python: [3]string = .["python3", "-c", "import sys;print('out');print('err',file=sys.stderr)"]
    process := Process.Run(python[:])
    if process.code != 0 || process.error != "" || process.stdout != "out\n" || process.stderr != "err\n" { return 31 }
    python[2] = "import sys;print('failed');print('reason',file=sys.stderr);sys.exit(17)"
    failure := Process.Run(python[:])
    if failure.code != 17 || failure.exception == null || failure.error == "" || failure.stdout != "failed\n" || failure.stderr != "reason\n" { return 32 }
    allowed := Process.Run(python[:], .{allow_failure = true})
    if allowed.code != 17 || allowed.error != "" { return 33 }
    python[2] = "import sys;print(sys.stdin.read().upper(),end='')"
    if Process.Run(python[:], .{input = "café"}).stdout != "CAFÉ" { return 34 }
    python[2] = "import os;print(os.getcwd())"
    if Text.Strip(Process.Run(python[:], .{cwd = directory.value}).stdout) != directory.value { return 35 }
    environment := Process.Environment()
    Collections.SetText(environment, "DISPLAY", ":ziran-test")
    Collections.SetText(environment, "WAYLAND_DISPLAY", "ziran-test")
    python[2] = "import os;print('DISPLAY' in os.environ or 'WAYLAND_DISPLAY' in os.environ)"
    if Process.Run(python[:], .{environment = environment}).stdout != "False\n" || Collections.GetText(environment, "DISPLAY") != ":ziran-test" { return 36 }
    python[2] = "import time;time.sleep(5)"
    timeout := Process.Run(python[:], .{timeout_seconds = 0.05})
    if timeout.exception == null || timeout.error == "" { return 37 }
    missing: [1]string = .["/ziran-no-such-command"]
    if Process.Run(missing[:]).error == "" || Process.Run(empty[:]).error == "" { return 38 }
    parser := Args.Parser("scripting test")
    Args.AddFlag(parser, "--enabled")
    Args.AddInteger(parser, "--count", fallback = 2)
    Args.AddText(parser, "--name", fallback = "zi")
    Args.AddTexts(parser, "--item")
    arguments: [9]string = .["--enabled", "--count", "42", "--name", "café", "--item", "a", "--item", "b"]
    parsed := Args.Parse(parser, arguments[:])
    if !Args.Flag(parsed, "enabled") || Args.Integer(parsed, "count") != 42 || Args.Text(parsed, "name") != "café" { return 39 }
    repeated := Args.Texts(parsed, "item")
    if repeated.count != 2 || repeated[0] != "a" || repeated[1] != "b" { return 39 }
    defaults := Args.Parse(parser, empty[:])
    if Args.Flag(defaults, "enabled") || Args.Integer(defaults, "count") != 2 || Args.Text(defaults, "name") != "zi" { return 40 }
    if Args.Texts(defaults, "item").count != 0 || Process.Find("/ziran-no-such-command") != "" || Process.Find("python3") == "" { return 40 }
    before := Time.Monotonic()
    Time.Sleep(0.001)
    stamp := Time.UTCISO()
    if Time.Monotonic() < before || Time.Now() <= 0.0 || stamp[stamp.count - 6:] != "+00:00" { return 41 }
    print("scripting APIs passed\n")
    return 0
}
ZI

"$ziran" check --root "$work" "$work/script.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/script.zi"
for form in source saved; do
    file=$work/script.zi
    root=$work
    if test "$form" = saved; then file=$work/ir/script.zir; root=$work/ir; fi
    "$ziran" build --target=py --exe --entry script:main --root "$root" -o "$work/$form" "$file"
    test "$(python3 "$work/$form")" = 'scripting APIs passed'
done
cmp "$work/source/__main__.py" "$work/saved/__main__.py"

# General Ziran nested slices are not supported; foreign signatures must
# reject them too rather than claiming they can be used in Ziran code.
cat > "$work/nested.zi" <<'ZI'
host :: #system_library "py:scripting_host";
Nested :: (values: [][]s32) -> [][]s32 #foreign host "echo";
ZI
if "$ziran" check --root "$work" "$work/nested.zi" 2> "$work/nested.err"; then
    echo 'nested foreign slices were accepted' >&2; exit 1
fi
grep -Fq 'Python foreign parameters must be' "$work/nested.err"
