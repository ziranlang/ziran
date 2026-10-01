#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Rebuild the command with its own build/IR commands. No handwritten command
# adapter is linked, and the release version still comes from VERSION.
printf 'Version :: "%s";\n' "$(cat "$repo/VERSION")" > "$work/version.zi"
"$ziran" ir --root "$repo/cmd" --module-path "$repo/std" \
    --module-path "$work" -o "$work/ir" "$repo/cmd/package_entry.zi"

for input in "$repo/cmd/package_entry.zi" "$work/ir/package_entry.zir"; do
    form=source
    case "$input" in *.zir) form=saved ;; esac
    output="$work/$form"
    "$ziran" build --target=c --root "$repo/cmd" --module-path "$repo/std" \
        --module-path "$work" -o "$output/c" "$input"
    mkdir -p "$output/bin"
    ${CC:-cc} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$output/c" \
        "$output/c/"*.c -o "$output/bin/ziran" -lcrypto -lm
    for tool in zi2zir zi2c zi2go zi2cpp zi2rust zi2py zi2zib zi-api zi-inspect zi-fmt; do
        tool_path=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)/$tool
        ln -s "$tool_path" "$output/bin/$tool"
    done
    for command in version --version guide 'features --json' 'capabilities --json'; do
        # The words here are fixed commands, never input or filenames.
        "$ziran" $command > "$output/expected"
        "$output/bin/ziran" $command > "$output/actual"
        cmp "$output/expected" "$output/actual"
    done
    PATH="$output/bin:$PATH" ziran --version > "$output/from-path"
    "$ziran" --version > "$output/expected"
    cmp "$output/expected" "$output/from-path"
    "$output/bin/ziran" check --root "$repo/site/examples" "$repo/site/examples/hello.zi"
    printf 'Answer::()->s32 {\nreturn 42\n}\n' > "$output/app.zi"
    "$output/bin/ziran" fmt "$output/app.zi"
    "$output/bin/ziran" fmt --check "$output/app.zi"
done
