#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/transport.zi" <<'ZI'
#import "package_process"
#import "std/byte_text_linux"
#import "std/file_linux"

Check :: (directory: string) -> s32 {
    output: [4096]u8
    names: [1]string = .["ZIRAN_PROCESS_TEST"]
    values: [1]string = .["child ; $literal"]
    none: [0]string
    args: [4]string = .["python3", "-c", "import sys;sys.stdout.write(sys.argv[1])",
        "quotes ' \" ; $(literal) * spaces"]
    if RunProcess(args[:], "", none[:], none[:], output[:]) != 0 { return 1 }
    if TextFromCString(*output[0]) != args[3] { return 2 }
    args[2] = "import os;print(os.environ['ZIRAN_PROCESS_TEST'])"
    if RunProcess(args[:], "", names[:], values[:], output[:]) != 0 { return 3 }
    if TextFromCString(*output[0]) != "child ; $literal\n" { return 4 }
    if RunProcess(args[:], "", none[:], none[:], output[:]) != 0 { return 5 }
    if TextFromCString(*output[0]) != "parent\n" { return 6 }
    pwd: [1]string = .["/bin/pwd"]
    if RunProcess(pwd[:], directory, none[:], none[:], output[:]) != 0 { return 7 }
    if TextFromCString(*output[0])[0:directory.count] != directory { return 8 }
    shell: [3]string = .["/bin/sh", "-c", "printf out; printf err >&2; exit 17"]
    if RunProcess(shell[:], "", none[:], none[:], output[:]) != 17 { return 9 }
    if TextFromCString(*output[0]) != "outerr" { return 10 }
    shell[2] = "kill -TERM $$"
    if RunProcess(shell[:], "", none[:], none[:], output[:]) != 143 { return 11 }
    missing: [1]string = .["/ziran-test-no-such-executable"]
    if RunProcess(missing[:], "", none[:], none[:], output[:]) != 127 { return 12 }
    if RunProcess(pwd[:], "/ziran-test-no-such-directory", none[:], none[:], output[:]) != 127 { return 13 }
    args[2] = "import os,sys;os.write(1,b'x'*1000000);open(sys.argv[1]+'/drained','w').write('done')"
    args[3] = directory
    small: [4]u8
    if RunProcess(args[:], "", none[:], none[:], small[:]) != -1 { return 14 }
    if TextFromCString(*small[0]) != "xxx" { return 15 }
    one: [1]u8
    shell[2] = "printf x"
    if RunProcess(shell[:], "", none[:], none[:], one[:]) != -1 || one[0] != 0 { return 16 }
    shell[2] = "exit 0"
    one[0] = 99
    if RunProcess(shell[:], "", none[:], none[:], one[:]) != 0 || one[0] != 0 { return 17 }
    empty: [0]u8
    if RunProcess(shell[:], "", none[:], none[:], empty[:]) != 0 { return 18 }
    if RunProcess(none[:], "", none[:], none[:], output[:]) != -1 { return 19 }
    excessive: [257]string
    if RunProcess(excessive[:], "", none[:], none[:], output[:]) != -1 { return 20 }
    if RunProcess(pwd[:], "", names[:], none[:], output[:]) != -1 { return 21 }
    nul: [3]u8 = .[65, 0, 66]
    missing[0] = TextFromBytes(nul[:])
    if RunProcess(missing[:], "", none[:], none[:], output[:]) != -1 { return 22 }
    if RunProcess(pwd[:], TextFromBytes(nul[:]), none[:], none[:], output[:]) != -1 { return 23 }
    values[0] = TextFromBytes(nul[:])
    if RunProcess(pwd[:], "", names[:], values[:], output[:]) != -1 { return 24 }
    return 42
}

main :: (argc: s32, argv: **u8) -> s32 {
    if argc != 2 { return 1 }
    result := Check(TextFromCString(argv[1]))
    print("%\n", result)
    if result == 42 { return 0 }
    return 1
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/cmd" -o "$work/ir" "$work/transport.zi"
for input in "$work/transport.zi" "$work/ir/transport.zir"; do
    form=source
    case "$input" in *.zir) form=saved ;; esac
    "$ziran" build --target=c --exe --entry transport:main --root "$work" \
        --module-path "$repo/cmd" -o "$work/$form" "$input"
    mkdir -p "$work/$form/cwd with spaces"
    ZIRAN_PROCESS_TEST=parent "$work/$form/transport" "$work/$form/cwd with spaces" > "$work/$form.out"
    test "$(cat "$work/$form.out")" = 42
    test "$(cat "$work/$form/cwd with spaces/drained")" = done
done
