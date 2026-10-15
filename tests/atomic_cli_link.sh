#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
link_job=
cleanup() {
    if test -n "$link_job"; then touch "$work/release"; wait "$link_job" || true; fi
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
mkdir "$work/bin"
printf '#!/bin/sh\nprintf "old\\n"\n' > "$work/bin/ziran"
chmod u+x "$work/bin/ziran"
cat > "$work/cc" <<'SH'
#!/bin/sh
set -eu
output=
previous=
for argument; do
    if test "$previous" = -o; then output=$argument; fi
    previous=$argument
done
case "$output" in
    "$LINK_TEST_ROOT/bin/"*) ;;
    *) exec "$LINK_TEST_CC" "$@" ;;
esac
# Simulate an incomplete linker output without execute permission. Only the
# disposable BUILD_DIR is touched, including when this test catches a bug.
printf '#!/bin/sh\nprintf "new\\n"\n' > "$output"
chmod 600 "$output"
touch "$LINK_TEST_ROOT/ready"
attempt=0
while test ! -e "$LINK_TEST_ROOT/release"; do
    attempt=$((attempt+1))
    if test "$attempt" -ge 500; then exit 1; fi
    sleep 0.01
done
test "${LINK_TEST_FAIL:-0}" = 0
SH
chmod u+x "$work/cc"
export LINK_TEST_ROOT=$work LINK_TEST_CC=${CC:-cc}
make --no-print-directory -f "$repo/Makefile" BUILD_DIR="$work" PACKAGE_OBJECTS= \
    CC="$work/cc" package-link > "$work/build.log" 2>&1 &
link_job=$!
attempt=0
while test ! -e "$work/ready"; do
    if ! kill -0 "$link_job" 2>/dev/null; then cat "$work/build.log"; exit 1; fi
    attempt=$((attempt+1))
    if test "$attempt" -ge 500; then cat "$work/build.log"; exit 1; fi
    sleep 0.01
done
test "$("$work/bin/ziran")" = old
touch "$work/release"
wait "$link_job"
link_job=
test "$("$work/bin/ziran")" = new
if LINK_TEST_FAIL=1 make --no-print-directory -f "$repo/Makefile" BUILD_DIR="$work" \
    PACKAGE_OBJECTS= CC="$work/cc" package-link > "$work/failed.log" 2>&1; then
    echo 'failed link unexpectedly succeeded' >&2; exit 1
fi
test "$("$work/bin/ziran")" = new
test -z "$(find "$work/bin" -name '.ziran.*' -print)"
echo 'CLI linking preserves the prior executable during pending and failed links'
