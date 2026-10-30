#!/usr/bin/env python3
"""Normal package commands repair only deletion-damaged immutable cache entries."""

from pathlib import Path
import os
import sys
import tempfile

from packages import call, commit, init, write


def check_cache_repair(ziran: str, root: Path, env: dict) -> None:
    fixture = root / "cache-repair"
    fixture.mkdir()
    source, tool, app = [fixture / name for name in ("source", "tool", "app")]
    for repo in (source, tool):
        init(repo, env)
        write(repo / "VERSION", "1\n")
    write(source / "include/value.h", "int value(void);\n")
    write(source / "src/value.c", '#include "value.h"\nint value(void) { return 42; }\n')
    write(source / ".gitignore", "local-private/\n")
    commit(source, env)
    commit(tool, env)
    app.mkdir()
    write(app / "ziran.toml", f'''[package]
name = "CacheRepair"
[toolchain]
git = "{tool.as_uri()}"
ref = "master"
[dependencies.Payload]
git = "{source.as_uri()}"
ref = "master"
source = true
''')
    call(ziran, "lock", cwd=app, env=env)
    call(ziran, "fetch", "--locked", cwd=app, env=env)
    saved_lock = (app / "ziran.lock").read_bytes()

    def path(*options: str, succeed: bool = True) -> str:
        return call(ziran, "pkg", "path", "Payload", *options,
                    cwd=app, env=env, succeed=succeed).strip()

    entry = Path(path("--locked", "--offline"))
    head = call("git", "rev-parse", "HEAD", cwd=entry, env=env)

    def usable() -> None:
        assert (entry / "include/value.h").is_file()
        assert (entry / "src/value.c").is_file()
        call("cc", "-std=c99", "-Werror", "-I", str(entry / "include"), "-c",
             str(entry / "src/value.c"), "-o", str(fixture / "value.o"),
             cwd=app, env=env)
        assert (fixture / "value.o").stat().st_size > 0
        assert (app / "ziran.lock").read_bytes() == saved_lock
        assert call("git", "rev-parse", "HEAD", cwd=entry, env=env) == head
        assert call("git", "status", "--porcelain", cwd=entry, env=env) == ""

    # No ref resolution or remote access is needed after initial lock/fetch.
    source.rename(fixture / "unavailable-origin")
    stamp = (entry / "src/value.c").stat().st_mtime_ns
    assert Path(path("--locked", "--offline")) == entry
    assert (entry / "src/value.c").stat().st_mtime_ns == stamp
    usable()
    print("PASS: valid immutable cache fast path leaves payload untouched", flush=True)
    write(entry / "local-private/owner-note", "retained build data\n")
    assert Path(path("--locked", "--offline")) == entry
    assert (entry / "local-private/owner-note").read_text() == "retained build data\n"
    (entry / "local-private/owner-note").unlink()
    (entry / "local-private").rmdir()

    # Reproduce the actual curl failure: HEAD is right, all tracked files are
    # staged for deletion, and only .git remains. Normal checkout used to
    # report this entry ready without materializing any source.
    call("git", "rm", "-qr", "--", ".", cwd=entry, env=env)
    assert list(entry.iterdir()) == [entry / ".git"]
    assert call("git", "rev-parse", "HEAD", cwd=entry, env=env) == head
    assert Path(path("--locked", "--offline")) == entry
    usable()
    print("PASS: matching-HEAD staged payload deletion repaired offline; C source builds", flush=True)

    (entry / "src/value.c").unlink()
    call(ziran, "fetch", "--locked", "--offline", cwd=app, env=env)
    usable()
    print("PASS: unstaged payload deletion repaired by normal locked offline fetch", flush=True)

    def refused() -> None:
        before = call("git", "status", "--porcelain", "--ignored", cwd=entry, env=env)
        for options in (("--locked", "--offline"), ("--locked",)):
            result = path(*options, succeed=False)
            assert "package cache" in result and "preserved" in result, result
            assert before == call("git", "status", "--porcelain", "--ignored", cwd=entry, env=env)
            assert call("git", "rev-parse", "HEAD", cwd=entry, env=env) == head

    original = (entry / "src/value.c").read_bytes()
    for staged in (False, True):
        write(entry / "src/value.c", "owner modification\n")
        if staged:
            call("git", "add", "src/value.c", cwd=entry, env=env)
        refused()
        assert (entry / "src/value.c").read_text() == "owner modification\n"
        (entry / "src/value.c").write_bytes(original)
        call("git", "add", "src/value.c", cwd=entry, env=env)
    for name in ("owner-note", "local-private/owner-note"):
        write(entry / name, "owner data\n")
        (entry / "src/value.c").unlink()
        refused()
        assert (entry / name).read_text() == "owner data\n"
        (entry / name).unlink()
        if name.startswith("local-private/"):
            (entry / "local-private").rmdir()
        assert Path(path("--locked", "--offline")) == entry
        usable()
    print("PASS: modified, staged, untracked and ignored owner data refused without loss", flush=True)

    # An index flag must not conceal missing payload from the healthy path.
    call("git", "update-index", "--skip-worktree", "src/value.c", cwd=entry, env=env)
    (entry / "src/value.c").unlink()
    refused()
    call("git", "update-index", "--no-skip-worktree", "src/value.c", cwd=entry, env=env)
    assert Path(path("--locked", "--offline")) == entry
    usable()
    call("git", "config", "core.sparseCheckout", "true", cwd=entry, env=env)
    refused()
    call("git", "config", "--unset", "core.sparseCheckout", cwd=entry, env=env)
    print("PASS: sparse/flagged caches cannot masquerade as complete", flush=True)

    origin = call("git", "config", "--get", "remote.origin.url", cwd=entry, env=env).strip()
    call("git", "config", "remote.origin.url", tool.as_uri(), cwd=entry, env=env)
    refused()
    call("git", "config", "remote.origin.url", origin, cwd=entry, env=env)
    print("PASS: wrong repository identity refused without replacement", flush=True)

    # Moving a fixture cache entry, then redirecting its name, must never
    # turn that other checkout into a resolver-owned repair target.
    external = fixture / "external-checkout"
    entry.rename(external)
    (external / "src/value.c").unlink()
    entry.symlink_to(external, target_is_directory=True)
    refused()
    assert not (external / "src/value.c").exists()
    entry.unlink()
    external.rename(entry)
    assert Path(path("--locked", "--offline")) == entry
    usable()
    print("PASS: redirected cache entry is preserved and never repaired", flush=True)

    # Explicit canonical override is a user checkout, even when its HEAD is
    # right and payload is staged for deletion. No cache repair runs there.
    override = fixture / "unavailable-origin"
    call("git", "rm", "-qr", "--", ".", cwd=override, env=env)
    write(override / "owner-note", "keep this\n")
    write(app / "ziran.local.toml", f'[overrides]\nPayload = "{override}"\n')
    status = call("git", "status", "--porcelain", cwd=override, env=env)
    assert Path(path("--offline")) == override
    assert call("git", "status", "--porcelain", cwd=override, env=env) == status
    assert not (override / "src/value.c").exists()
    assert (override / "owner-note").read_text() == "keep this\n"
    print("PASS: explicit source override left untouched", flush=True)


def main() -> None:
    compiler = Path(__file__).resolve().parents[1]
    ziran = str(Path(sys.argv[1]).resolve())
    # Keep every miniature input repository and product-managed test cache
    # under the project's ignored fixture directory.
    scratch = compiler / "work/package-cache-repair"
    call("git", "check-ignore", str(scratch / "probe"), cwd=compiler, env=os.environ.copy())
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=scratch) as temporary:
        root = Path(temporary)
        env = {key: value for key, value in os.environ.items()
               if key not in ("DISPLAY", "WAYLAND_DISPLAY", "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE")}
        env.update(XDG_CACHE_HOME=str(root / "cache"), ZIRAN_TEST_ALLOW_LOCAL_GIT="1",
                   ZIRAN_PINNED_LAUNCHER="1", GIT_CONFIG_COUNT="1",
                   GIT_CONFIG_KEY_0="protocol.file.allow", GIT_CONFIG_VALUE_0="always")
        check_cache_repair(ziran, root, env)


if __name__ == "__main__":
    main()
