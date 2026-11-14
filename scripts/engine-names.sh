#!/bin/sh
# Writes the engine's renaming header: each function or variable the engine
# modules' generated headers declare gets the ziran_engine_ prefix, so a
# program with its own copies of those modules links beside the engine on
# every object format. A name a header also offers as an inline function
# stays a call to that inline function outside its module, so it is renamed
# only where ZIRAN_ENGINE_SOURCE_<module> says the module itself compiles.
# usage: engine-names.sh DIR MODULE...
set -eu
dir=$1
shift
for module in "$@"; do
    awk -v module="$module" '
        /^#define [A-Za-z_][A-Za-z0-9_]*\(\.\.\.\) [A-Za-z_][A-Za-z0-9_]*_ziran_inline\(/ {
            inline[substr($2, 1, index($2, "(") - 1)] = 1
            next
        }
        /^(static|typedef) / { next }
        /^extern [^"]/ || /^[A-Za-z][^=]*\);$/ {
            line = $0
            sub(/\(.*/, "", line)
            sub(/\[.*/, "", line)
            sub(/;$/, "", line)
            count = split(line, words, /[ *]+/)
            if (!(words[count] in seen)) order[++names] = words[count]
            seen[words[count]] = 1
        }
        END {
            for (i = 1; i <= names; i++) {
                name = order[i]
                if (name in inline) printf "#ifdef ZIRAN_ENGINE_SOURCE_%s\n", module
                printf "#define %s ziran_engine_%s\n", name, name
                if (name in inline) print "#endif"
            }
        }
    ' "$dir/$module.h"
done
