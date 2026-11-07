#!/bin/sh
# std's C bindings give size_t, ssize_t and nfds_t values pointer-sized Ziran
# types (usize, isize). A fixed-width u64 or s64 there is the wrong C type on
# wasm32 and other 32-bit targets: wasm-ld reports a function signature
# mismatch and the call traps. Each listed C function names the positions of
# its pointer-sized parameters ("r" is its result, "*" a pointer to one).
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 - "$repo/std" <<'PY'
import pathlib, re, sys

SIZED = {
    "read": {2: "usize", "r": "isize"}, "write": {2: "usize", "r": "isize"},
    "pread": {2: "usize", "r": "isize"}, "pwrite": {2: "usize", "r": "isize"},
    "pread64": {2: "usize", "r": "isize"}, "pwrite64": {2: "usize", "r": "isize"},
    "send": {2: "usize", "r": "isize"}, "recv": {2: "usize", "r": "isize"},
    "getcwd": {1: "usize"}, "readlink": {2: "usize", "r": "isize"},
    "getrandom": {1: "usize", "r": "isize"},
    "bcopy": {2: "usize"}, "mempcpy": {2: "usize"}, "memcpy": {2: "usize"},
    "memmove": {2: "usize"}, "memset": {2: "usize"}, "memcmp": {2: "usize"},
    "strlen": {"r": "usize"}, "strnlen": {1: "usize", "r": "usize"},
    "malloc": {0: "usize"}, "calloc": {0: "usize", 1: "usize"},
    "realloc": {1: "usize"}, "mmap": {1: "usize"}, "mmap64": {1: "usize"},
    "munmap": {1: "usize"}, "poll": {1: "usize"},
    "curl_ws_recv": {2: "usize", 3: "*usize"},
}
DECLARATION = re.compile(
    r'(\w+)\s*::\s*\(([^)]*)\)\s*(?:->\s*([^#{;]*?))?\s*#foreign\s+(\w+)(?:\s+"([^"]+)")?')

def parameter_types(text):
    types = []
    for parameter in filter(None, (p.strip() for p in text.split(","))):
        types.append(parameter.split(":", 1)[1].strip() if ":" in parameter else "")
    return types

problems = []
checked = 0
for path in sorted(pathlib.Path(sys.argv[1]).glob("*.zi")):
    source = path.read_text(encoding="utf-8")
    for match in DECLARATION.finditer(source):
        name, parameters, result, library, symbol = match.groups()
        symbol = symbol or name
        # Windows and Go bindings follow their own platform types.
        if library in ("kernel32", "ws2_32", "crt") or symbol not in SIZED:
            continue
        types = parameter_types(" ".join(parameters.split()))
        checked += 1
        line = source.count("\n", 0, match.start()) + 1
        for position, wanted in SIZED[symbol].items():
            actual = (result or "").strip() if position == "r" else (
                types[position] if position < len(types) else "")
            if actual != wanted:
                where = "result" if position == "r" else "parameter %d" % (position + 1)
                problems.append("%s:%d: %s %s is %s; C uses a pointer-sized type: %s"
                                % (path.name, line, symbol, where, actual or "missing", wanted))
if problems:
    print("\n".join(problems), file=sys.stderr)
    sys.exit(1)
if checked < 15:
    sys.exit("only %d std foreign bindings matched; the pattern no longer reads them" % checked)
print("std foreign size_t bindings: %d checked, passed" % checked)
PY
