#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 - "$repo" "$1" <<'PY'
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

repo = Path(sys.argv[1])
binary = Path(sys.argv[2]).resolve()
env = dict(os.environ)
for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS"):
    env.pop(key, None)
make = shutil.which("gmake") or shutil.which("make")
with tempfile.TemporaryDirectory(prefix="ziran-install-") as folder:
    work = Path(folder)
    share = work / "bootstrap"
    command = [make, "--no-print-directory", "-s", "-o", "all", "install-user",
               "BUILD_DIR=" + str(binary.parent.parent),
               "USER_BIN=" + str(work / "bin"), "USER_SHARE=" + str(share)]
    # The check runner already built this exact toolchain. Avoid rebuilding it
    # while other compiler checks are running in parallel.
    subprocess.run(command, cwd=repo, env=env, check=True)
    source = work / "live.zi"
    source.write_text('#program_export\nmain :: () -> s32 { while true {} return 0 }\n')
    bundle = work / "live.zib"
    subprocess.run([binary, "bundle", "--root", work, "--entry", "live:main",
                    "-o", bundle, source], env=env, check=True)
    installed = share / "build/bin/zi2zib"
    inode = installed.stat().st_ino
    process = subprocess.Popen([installed, "run", bundle], env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        time.sleep(0.1)
        assert process.poll() is None, process.communicate()
        subprocess.run(command, cwd=repo, env=env, check=True)
        assert installed.stat().st_ino != inode
        assert process.poll() is None, "installation interrupted the running compiler"
        assert installed.read_bytes() == (binary.parent / "zi2zib").read_bytes()
        assert subprocess.check_output([work / "bin/ziran", "version"], env=env).startswith(b"ziran ")
    finally:
        if process.poll() is None:
            process.terminate()
        process.communicate(timeout=10)
print("Compiler installation replaces busy binaries and preserves running processes")
PY
