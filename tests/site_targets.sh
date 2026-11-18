#!/bin/sh
set -eu
# Verify the published snapshot and execute its real portable artifacts.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
python3 "$repo/scripts/site_targets.py" --check
python3 "$repo/scripts/site_library_targets.py" --check
python3 "$repo/scripts/site_kernel_targets.py" --check
python3 "$repo/bench/json_text.py" --check
"$ziran" run --target=c --root "$repo/scripts" --module-path "$repo" "$repo/scripts/site_benchmark_tables.zi" -- --check
"$ziran" run --target=c --root "$repo/scripts" --module-path "$repo/bench" --module-path "$repo" "$repo/scripts/site_standard_benchmarks.zi" -- --check
exec "$ziran" run --target=c --root "$repo/tests" --module-path "$repo/scripts" "$repo/tests/site_targets_cases.zi" -- "$ziran"
