#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/program.zi" <<'ZI'
Bump :: (value: s64) -> s64 #asm("x86_64", "# NATIVE_FN_CANARY\nadd $1, %[out]", "=r", "0", "cc", "") { return value + 1 }
#law Pure effect Bump == pure;
#law Increase theorem x: s64 => Bump(x) == x + 1;
#proof Increase { unfold Bump; ring; }
state: s64;
#program_export
Answer :: () -> s64 {
    text := "literal #asm survives"
    if text.count != 21 { return 1 }
    state += 1
    result: s64 = 40
    #asm("x86_64", "# NATIVE_BLOCK_CANARY\nadd $1, %[result]", "=r:result", "0:old=result", "cc", "") { result += 1 }
    return Bump(result) + state - 1
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/program.zi" > "$work/laws.json"
"${CC:-cc}" ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/cmd/zir" -I"$repo/include" \
    "$repo/tests/zib_portable_write_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -o "$work/write"
python3 - "$work" "$ziran" <<'PY'
from pathlib import Path
import json, subprocess, sys
work=Path(sys.argv[1]);ziran=sys.argv[2];command=[str(work/'write'),str(work/'ir/program.zir')]
subprocess.run([*command,'file',str(work/'file.zib')],check=True)
prefix=b'unchanged prefix\0'
(work/'append.zib').write_bytes(prefix)
subprocess.run([*command,'append',str(work/'append.zib')],check=True)
assert (work/'append.zib').read_bytes()==prefix+(work/'file.zib').read_bytes()
# PIPE makes stdout unseekable; writing to a regular redirected file would
# fail to test that property.
pipe=subprocess.run([*command,'pipe','unused'],stdout=subprocess.PIPE,check=True)
assert pipe.stdout==(work/'file.zib').read_bytes()
(work/'pipe.zib').write_bytes(pipe.stdout)
for mode in ('readonly','invalid','oversized'):
    subprocess.run([*command,mode,str(work/(mode+'.zib'))],check=True)
assert (work/'invalid.zib').stat().st_size==0
assert (work/'oversized.zib').stat().st_size==0
if Path('/dev/full').exists():subprocess.run([*command,'full','/dev/full'],check=True)
for name in ('file','pipe'):
    run=subprocess.run([ziran,'run',str(work/(name+'.zib'))],capture_output=True,text=True)
    assert run.returncode==0,run.stderr
    assert run.stdout.strip()=='42',run.stdout
    assert (work/(name+'.zib')).read_bytes().count(b'NATIVE_FN_CANARY')==0
    # The asset still contains the same bytes that were removed as native
    # metadata from the IR; only the intended fields are transformed.
    assert (work/(name+'.zib')).read_bytes().count(b'NATIVE_BLOCK_CANARY')==1
print('Portable bundle writing passed files, append streams, pipes, write errors and immutable-IR checks')
PY
