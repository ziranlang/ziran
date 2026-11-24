#!/bin/sh
# The frontend cache: a cold build parses and stores each self-contained
# module, a warm build loads the stored parses, and neither changes what the
# compiler reports or writes. A failed build publishes nothing, an edited
# module is never served stale, --lint and spliced modules never use it, and
# a damaged entry is a miss rather than a diagnostic.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/frontend-cache-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

fail() { echo "frontend_cache: $*" >&2; exit 1; }
counter() {
    value=$(grep -o "\"$2\":[0-9]*" "$1" 2>/dev/null | head -1 | cut -d: -f2)
    echo "${value:-0}"
}
entries() { find "$1" -name '*.zfc' -type f 2>/dev/null | wc -l; }

app="$work/app"
cache="$work/cache"
mkdir -p "$app"
cat > "$app/util.zi" <<'ZI'
Twice :: (value: s32) -> s32 { return value * 2 }
ZI
cat > "$app/main.zi" <<'ZI'
U :: #import "util";
main :: () -> s32 { return U.Twice(21) - 42 }
ZI

# A cold cache parses both modules and stores both parses; a warm build
# loads both instead of parsing again.
ZIRAN_FRONTEND_CACHE_DIR="$cache" ZIRAN_PROFILE="$work/cold.jsonl" \
    "$ziran" check --root "$app" "$app/main.zi" || fail "the clean check failed"
[ "$(entries "$cache")" = 2 ] || fail "cold run stored $(entries "$cache") modules"
[ "$(counter "$work/cold.jsonl" frontend_cache_store)" = 2 ] || fail "cold run did not store two parses"
[ "$(counter "$work/cold.jsonl" frontend_cache_hit)" = 0 ] || fail "cold run hit a cache entry"
ZIRAN_FRONTEND_CACHE_DIR="$cache" ZIRAN_PROFILE="$work/warm.jsonl" \
    "$ziran" check --root "$app" "$app/main.zi" || fail "the cached check failed"
[ "$(counter "$work/warm.jsonl" frontend_cache_hit)" = 2 ] || fail "warm run did not load both parses"
[ "$(counter "$work/warm.jsonl" frontend_cache_store)" = 0 ] || fail "warm run stored a parse"

# Saved IR is byte-identical with the cache off and on.
ZIRAN_FRONTEND_CACHE=0 "$ziran" ir --root "$app" -o "$work/ir-off" "$app/main.zi" || fail "uncached IR build failed"
ZIRAN_FRONTEND_CACHE_DIR="$work/ir-cache" "$ziran" ir --root "$app" -o "$work/ir-on" "$app/main.zi" || fail "cached IR build failed"
diff -r "$work/ir-off" "$work/ir-on" >/dev/null || fail "saved IR differs with the cache"

# A failed build reports the same diagnostics and publishes nothing.
cat > "$app/bad.zi" <<'ZI'
main :: () -> s32 { return "text" }
ZI
if ZIRAN_FRONTEND_CACHE=0 "$ziran" check --root "$app" "$app/bad.zi" >"$work/bad-off.out" 2>"$work/bad-off.err"; then
    fail "the invalid check passed"
fi
if ZIRAN_FRONTEND_CACHE_DIR="$work/bad-cache" "$ziran" check --root "$app" "$app/bad.zi" >"$work/bad-on.out" 2>"$work/bad-on.err"; then
    fail "the invalid check passed with the cache"
fi
cmp "$work/bad-off.out" "$work/bad-on.out" || fail "stdout diagnostics differ with the cache"
cmp "$work/bad-off.err" "$work/bad-on.err" || fail "stderr diagnostics differ with the cache"
[ "$(entries "$work/bad-cache")" = 0 ] || fail "a failed build published a cache entry"

# An edited module is never served from the older entry.
cat > "$app/util.zi" <<'ZI'
Twice :: (value: s32) -> string { return "twice" }
ZI
if ZIRAN_FRONTEND_CACHE_DIR="$cache" "$ziran" check --root "$app" "$app/main.zi" >/dev/null 2>&1; then
    fail "the cache served a stale parse of an edited module"
fi
cat > "$app/util.zi" <<'ZI'
Twice :: (value: s32) -> s32 { return value * 2 }
ZI

# --lint needs a fresh parse, so it neither reads nor writes entries.
lintcache="$work/lint-cache"
ZIRAN_FRONTEND_CACHE_DIR="$lintcache" ZIRAN_PROFILE="$work/lint.jsonl" \
    "$ziran" check --lint --root "$app" "$app/main.zi" || fail "the lint check failed"
[ "$(entries "$lintcache")" = 0 ] || fail "--lint wrote a cache entry"
[ "$(counter "$work/lint.jsonl" frontend_cache_miss)" = 0 ] || fail "--lint read the cache"

# ZIRAN_FRONTEND_CACHE=0 disables the cache entirely.
ZIRAN_FRONTEND_CACHE=0 ZIRAN_FRONTEND_CACHE_DIR="$work/off-cache" \
    "$ziran" check --root "$app" "$app/main.zi" || fail "the disabled check failed"
[ "$(entries "$work/off-cache")" = 0 ] || fail "a disabled cache wrote an entry"

# A module that splices another file is never stored.
mkdir -p "$work/load"
cat > "$work/load/extra.zi" <<'ZI'
Extra :: 7;
ZI
cat > "$work/load/main.zi" <<'ZI'
#load "extra.zi";
main :: () -> s32 { return 0 }
ZI
ZIRAN_FRONTEND_CACHE_DIR="$work/load-cache" \
    "$ziran" check --root "$work/load" "$work/load/main.zi" || fail "the spliced check failed"
[ "$(entries "$work/load-cache")" = 0 ] || fail "a module that #loads another file was stored"

# A damaged entry is a miss: the module is parsed again and the build is
# still correct.
entry=$(find "$cache" -name '*.zfc' -type f | head -1)
printf 'not saved IR' > "$entry"
ZIRAN_FRONTEND_CACHE_DIR="$cache" "$ziran" check --root "$app" "$app/main.zi" || fail "a damaged entry broke the build"
[ "$(entries "$cache")" = 2 ] || fail "the damaged entry was not replaced"
