#!/usr/bin/env python3
"""A stale local override names its mismatch; git trace output is ignored."""

from pathlib import Path
import os
import sys
import tempfile

from packages import call, commit, init, write


def check_diagnostics(ziran: str, root: Path, env: dict) -> None:
    fixture = root / "diagnostics"
    fixture.mkdir()
    leaf, provider, tool, app = [fixture / n for n in ("leaf", "provider", "tool", "app")]
    for repo in (leaf, provider, tool):
        init(repo, env)
        write(repo / "VERSION", "1\n")
    write(leaf / "ziran.toml", '[package]\nname = "Leaf"\nmodule_roots = ["src"]\n[exports]\nValue = "src/Value.zi"\n')
    write(leaf / "src/Value.zi", "Value :: 1;\n")
    provider_manifest = f'''[package]
name = "Provider"
module_roots = ["src"]
[dependencies.Leaf]
git = "{leaf.as_uri()}"
ref = "master"
[exports]
Value = "src/Value.zi"
'''
    write(provider / "ziran.toml", provider_manifest)
    write(provider / "src/Value.zi", "Value :: 1;\n")
    for repo in (leaf, provider, tool):
        commit(repo, env)
    app.mkdir()
    write(app / "ziran.toml", f'''[package]
name = "Diagnostics"
[toolchain]
git = "{tool.as_uri()}"
ref = "master"
[dependencies.Provider]
git = "{provider.as_uri()}"
ref = "master"
''')
    call(ziran, "lock", cwd=app, env=env)
    call(ziran, "fetch", "--locked", cwd=app, env=env)

    # A healthy cache entry stays usable while git traces to stderr.
    traced = dict(env, GIT_TRACE="1", GIT_TRACE2="1", GIT_TRACE_SETUP="1")
    entry = Path(call(ziran, "pkg", "path", "Provider", "--locked", "--offline",
                      cwd=app, env=traced).strip().splitlines()[-1])
    assert entry.is_dir() and (entry / "src/Value.zi").is_file()
    assert call("git", "status", "--porcelain", cwd=entry, env=env) == ""
    call(ziran, "fetch", "--locked", "--offline", cwd=app, env=traced)
    print("PASS: clean cache entry accepted with GIT_TRACE set", flush=True)

    # The override declares another Leaf ref than ziran.lock pins.
    override = fixture / "provider-override"
    call("git", "clone", "-q", provider.as_uri(), str(override), cwd=fixture, env=env)
    write(override / "ziran.toml", provider_manifest.replace('ref = "master"', 'ref = "next"'))
    write(app / "ziran.local.toml", f'[overrides]\nProvider = "{override}"\n')
    result = call(ziran, "fetch", cwd=app, env=env, succeed=False)
    assert "cannot fetch locked packages" not in result, result
    for part in ("Provider", f"local override {override}", "declares Leaf next",
                 str(leaf.as_uri()), "ziran.lock pins Leaf", "ziran lock"):
        assert part in result, (part, result)
    print("PASS: stale local override names package, source and mismatched pin", flush=True)


def main() -> None:
    compiler = Path(__file__).resolve().parents[1]
    ziran = str(Path(sys.argv[1]).resolve())
    scratch = compiler / "work/package-diagnostics"
    call("git", "check-ignore", str(scratch / "probe"), cwd=compiler, env=os.environ.copy())
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=scratch) as temporary:
        root = Path(temporary)
        env = {key: value for key, value in os.environ.items()
               if key not in ("DISPLAY", "WAYLAND_DISPLAY", "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE")}
        env.update(XDG_CACHE_HOME=str(root / "cache"), ZIRAN_TEST_ALLOW_LOCAL_GIT="1",
                   ZIRAN_PINNED_LAUNCHER="1", GIT_CONFIG_COUNT="1",
                   GIT_CONFIG_KEY_0="protocol.file.allow", GIT_CONFIG_VALUE_0="always")
        check_diagnostics(ziran, root, env)


if __name__ == "__main__":
    main()
