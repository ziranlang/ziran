"""Dependency-owned artifact integration fixture; uses real native objects."""
from pathlib import Path
import subprocess


def check_artifacts(ziran, root, env, compiler, call, write, init, commit):
    # Use the selected test build, including build/sanitize/bin in sanitizer CI.
    zi2c = Path(ziran).resolve().with_name("zi2c")
    fixture = root / "artifacts"
    fixture.mkdir()
    provider, tool = fixture / "provider", fixture / "tool"
    for repo in (provider, tool):
        init(repo, env)
    write(tool / "VERSION", "fixture\n")
    commit(tool, env)
    # A real provider's source list is long: Entity's profile has over 64 inputs.
    wide = [f"inputs/{index}.txt" for index in range(200)]
    for name in wide:
        write(provider / name, name + "\n")
    wide_inputs = "".join(f', "{name}"' for name in wide)
    write(provider / "ziran.toml", '''[package]
name = "Provider"
[tool]
project = "build/bin/provider"
[options]
mode = "string=normal"
enabled = "bool=false"
[options.variant."*"]
path = "string!"
[artifacts.object]
inputs = ["src/value.c", "tools/provider.zi", "Makefile"WIDE_INPUTS]
outputs = ["value.o", "resource.txt", "options.txt"]
'''.replace("WIDE_INPUTS", wide_inputs))
    write(provider / ".gitignore", "build/\n")
    write(provider / "src/value.c", "int value(void) { return 42; }\n")
    write(provider / "Makefile", f'''build/bin/provider: tools/provider.zi
\t@mkdir -p build/bin
\t@LDLIBS=-lcrypto {zi2c} --exe --root tools --module-path {compiler}/cmd --module-path {compiler}/std --entry provider:main -o build/bin tools/provider.zi
''')
    write(provider / "tools/provider.zi", '''#import "package_common"
#import "std/byte_text_linux"
#import "std/c_string"
main :: (argc: s32, argv: **u8) -> s32 {
    output := PkgGetenv("ZIRAN_ARTIFACT_OUTPUT")
    if argc != 7 || TextFromCString(argv[1]) != "artifact" { return 3 }
    countfile := PkgJoin(PkgGetenv("ZIRAN_PACKAGE_ROOT"), "/build/count")
    prior := PkgRead(countfile)
    unused PkgWrite(countfile, PkgJoin(prior.text, "x"))
    args: [5]string = .["cc", "-c", PkgJoin(PkgGetenv("ZIRAN_PACKAGE_ROOT"), "/src/value.c"), "-o", PkgJoin(output, "/value.o")]
    empty: [0]string
    code := PkgRun(args[:], "", empty[:], empty[:])
    if code != 0 { return code }
    resolved := PkgRead(PkgGetenv("ZIRAN_TOOL_OPTIONS"))
    if !resolved.valid || !PkgWrite(PkgJoin(output, "/options.txt"), resolved.text) { return 5 }
    if PkgGetenv("FAIL_PROVIDER") == "1" { return 6 }
    if !PkgWrite(PkgJoin(output, "/resource.txt"), PkgGetenv("ZIRAN_ARTIFACT_TARGET")) { return 4 }
    return 0
}
''')
    commit(provider, env)
    apps = []
    for name in ("one", "two"):
        app = fixture / name
        app.mkdir()
        write(app / "ziran.toml", f'''[package]
name = "{name}"
[toolchain]
git = "{tool.as_uri()}"
[dependencies.Provider]
git = "{provider.as_uri()}"
''')
        write(app / "ziran.local.toml", f'[overrides]\nziran = "{tool}"\nProvider = "{provider}"\n')
        call(ziran, "lock", cwd=app, env=env)
        apps.append(app)
    env = {**env, "ZIRAN_PINNED_LAUNCHER": "1"}
    def build(app=apps[0], target="linux-x86_64-gnu", ok=True, selection="Provider:object", extra=None, options=()):
        return call(ziran, "pkg", "artifact", selection, "--platform", target,
                    *(arg for item in options for arg in ("--option", item)),
                    cwd=app, env={**env, **(extra or {})}, succeed=ok).strip()
    def count():
        return len((provider / "build/count").read_text())
    assert "OS-ARCH-ABI" in build(target="c", ok=False)
    first = Path(build())
    assert first.is_file(), first
    descriptor = first.read_text()
    assert "linux-x86_64-gnu" in descriptor and "value.o\t" in descriptor
    subprocess.run(["nm", str(first.parent / "value.o")], check=True, capture_output=True)
    assert count() == 1
    assert Path(build(apps[1])) == first and count() == 1
    assert Path(build()) == first and count() == 1
    other = Path(build(target="windows-x86_64-mingw"))
    assert other != first and count() == 2
    assert (other.parent / "resource.txt").read_text() == "windows-x86_64-mingw"
    changed = Path(build(extra={"CFLAGS": "-DARTIFACT_TEST"}))
    assert changed != first and count() == 3
    with (apps[0] / "ziran.toml").open("a") as stream:
        stream.write('[tool.Provider]\nmode = "different"\n')
    option = Path(build())
    assert option != first and count() == 4
    # A dirty local override must not reuse its old content key.
    write(provider / "src/value.c", "int value(void) { return 43; }\n")
    local = Path(build())
    assert local != option and count() == 5
    write(provider / "src/value.c", "invalid C\n")
    failed = build(ok=False)
    assert "artifact" in failed and count() == 6
    assert first.read_text() == descriptor and (first.parent / "value.o").is_file()
    build(ok=False)
    assert count() == 7
    write(provider / "src/value.c", "int value(void) { return 42; }\n")
    (apps[0] / "ziran.toml").write_text((apps[0] / "ziran.toml").read_text().split("[tool.Provider]")[0])
    assert Path(build()) == first and count() == 7
    assert "locked" in build(ok=False, selection="Missing:object")
    assert "recipe" in build(ok=False, selection="Provider:missing")
    original = (provider / "ziran.toml").read_text()
    write(provider / "ziran.toml", original.replace('outputs = ["value.o", "resource.txt", "options.txt"]', 'outputs = ["../escape"]'))
    assert "recipe" in build(ok=False)
    write(provider / "ziran.toml", original + '[artifacts.loop]\ninputs = ["src/value.c"]\noutputs = ["loop.o"]\nrequires = ["self:loop"]\n')
    assert "cycle" in build(ok=False, selection="Provider:loop")
    write(provider / "ziran.toml", original)
    # Toolchain identity changes even when provider input is unchanged.
    write(tool / "VERSION", "next\n")
    commit(tool, env)
    call(ziran, "update", "ziran", cwd=apps[0], env=env)
    assert Path(build()) != first and count() == 8
    # A real recipe edge is built first and supplied as a descriptor.
    write(provider / "ziran.toml", original.replace(
        'outputs = ["value.o", "resource.txt", "options.txt"]',
        'outputs = ["value.o", "resource.txt", "options.txt"]\nrequires = ["self:base"]') +
        '[artifacts.base]\ninputs = ["src/value.c"]\noutputs = ["value.o", "resource.txt", "options.txt"]\n')
    commit(provider, env)
    call(ziran, "update", "Provider", cwd=apps[0], env=env)
    dependent = Path(build())
    assert count() == 10
    dependencies = (dependent.parent / "dependencies.tsv").read_text()
    assert dependencies.startswith("self:base\t")
    assert Path(dependencies.strip().split("\t")[1]).is_file()
    assert Path(build()) == dependent and count() == 10
    # A changed dependency identity invalidates its consumer too.
    write(provider / "src/value.c", "int value(void) { return 44; }\n")
    commit(provider, env)
    call(ziran, "update", "Provider", cwd=apps[0], env=env)
    assert Path(build()) != dependent and count() == 12
    # Invocation overrides reach providers and all prerequisites without edits.
    for app in apps:
        write(app / "build/.ziran/module-map.tsv", "caller owned index\n")
    pristine = {path: path.read_bytes() for app in apps for path in app.rglob("*")
                if path.is_file() and (path.name.startswith("ziran.") or path.name == "module-map.tsv")}
    before = count()
    defaults = Path(build())
    assert Path(build(options=("Provider:mode=normal", "Provider:enabled=false"))) == defaults
    assert count() == before
    literal = "/sdk path/with=equals;$(false)"
    opts = ("provider:variant.beta.path=" + literal, "Provider:enabled=true",
            "Provider:variant.alpha.path=first", "Provider:mode=direct")
    direct = Path(build(options=opts))
    assert direct != defaults and count() == before + 2
    expected = (direct.parent / "options.txt").read_text()
    assert "enabled=true\n" in expected and "variant.beta.path=" + literal + "\n" in expected
    base = Path((direct.parent / "dependencies.tsv").read_text().strip().split("\t")[1])
    assert (base.parent / "options.txt").read_text() == expected
    assert Path(build(options=tuple(reversed(opts)))) == direct and count() == before + 2
    # Two independent consumers share equivalent invocation options.
    call(ziran, "update", "Provider", cwd=apps[1], env=env)
    call(ziran, "update", "ziran", cwd=apps[1], env=env)
    pristine = {path: path.read_bytes() for path in pristine}
    assert Path(build(apps[1], options=opts)) == direct and count() == before + 2
    changed_options = Path(build(options=("Provider:mode=play",)))
    assert changed_options != direct and count() == before + 4
    other_base = Path((changed_options.parent / "dependencies.tsv").read_text().strip().split("\t")[1])
    assert other_base != base
    for bad in (("Missing:mode=x",), ("Provider:mode",), ("Provider:mode=",),
                ("Provider:unknown=x",), ("Provider:enabled=1",), ("Provider:mode=x\ny=z",),
                ("Provider:variant.path=x",), ("Provider:variant..path=x",),
                ("Provider:mode=x", "provider:mode=y")):
        failure = build(options=bad, ok=False, extra={"CC": "must-not-be-invoked"})
        assert "option" in failure and "compiler" not in failure, failure
    for suffix in (("--option",), ("--wrong", "Provider:mode=x")):
        failure = call(ziran, "pkg", "artifact", "Provider:object", "--platform", "linux-x86_64-gnu",
                       *suffix, cwd=apps[0], env={**env, "CC": "must-not-be-invoked"}, succeed=False)
        assert "option" in failure and "compiler" not in failure, failure
    assert count() == before + 4
    saved = direct.read_bytes()
    assert "artifact" in build(options=("Provider:mode=failed",), ok=False, extra={"FAIL_PROVIDER": "1"})
    assert direct.read_bytes() == saved and (direct.parent / "value.o").is_file()
    assert all(path.read_bytes() == content for path, content in pristine.items())

    # Committed settings < local settings < invocation, with no persisted edits.
    manifest = apps[0] / "ziran.toml"
    local_manifest = apps[0] / "ziran.local.toml"
    committed, local_text = manifest.read_text(), local_manifest.read_text()
    write(manifest, committed + '[tool.Provider]\nmode = "committed"\n')
    write(local_manifest, local_text + '[tool.Provider]\nmode = "local"\n')
    local_default = Path(build())
    assert "mode=local\n" in (local_default.parent / "options.txt").read_text()
    assert Path(build(options=opts)) == direct
    assert manifest.read_text() == committed + '[tool.Provider]\nmode = "committed"\n'
    assert local_manifest.read_text() == local_text + '[tool.Provider]\nmode = "local"\n'
    write(manifest, committed)
    write(local_manifest, local_text)

    # A dependency edge's alias differs from the root consumer's alias.
    support = fixture / "support"
    init(support, env)
    for relative in (*wide, ".gitignore", "Makefile", "tools/provider.zi", "src/value.c"):
        write(support / relative, (provider / relative).read_text())
    write(support / "ziran.toml", original.replace('name = "Provider"', 'name = "Support"'))
    commit(support, env)
    write(provider / "ziran.toml", original.replace(
        'outputs = ["value.o", "resource.txt", "options.txt"]',
        'outputs = ["value.o", "resource.txt", "options.txt"]\nrequires = ["support:object"]') +
        f'[dependencies.support]\ngit = "{support.as_uri()}"\n')
    commit(provider, env)
    for app in apps:
        with (app / "ziran.toml").open("a") as stream:
            stream.write(f'[dependencies.Helper]\ngit = "{support.as_uri()}"\n[tool.Helper]\nmode = "root-default"\n')
        with (app / "ziran.local.toml").open("a") as stream:
            stream.write(f'Helper = "{support}"\n')
        call(ziran, "update", cwd=app, env=env)
    inherited = Path(build(options=("Helper:mode=inherited",)))
    support_descriptor = Path((inherited.parent / "dependencies.tsv").read_text().strip().split("\t")[1])
    assert "mode=inherited\n" in (support_descriptor.parent / "options.txt").read_text()
    assert Path(build(apps[1], options=("helper:mode=inherited",))) == inherited
    new_inherited = Path(build(options=("Helper:mode=other",)))
    assert new_inherited != inherited
    assert Path((new_inherited.parent / "dependencies.tsv").read_text().strip().split("\t")[1]) != support_descriptor
    # Exercise both public library signatures through the real resolver.
    client = fixture / "client"
    write(client / "client.zi", '''#import "package"
#import "std/byte_text_linux"
#import "std/c_string"
main :: (argc: s32, argv: **u8) -> s32 {
    if argc != 2 { return 3 }
    project := TextFromCString(argv[1])
    options: [1]string = .["Helper:mode=library"]
    if PkgBuildArtifactWithOptions(project, "Provider", "object", "linux-x86_64-gnu", options[:]) != 0 { return 4 }
    return PkgBuildArtifact(project, "Provider", "object", "linux-x86_64-gnu")
}
''')
    call(str(zi2c), "--exe", "--root", str(client), "--module-path", str(compiler / "cmd"),
         "--module-path", str(compiler / "std"), "--entry", "client:main", "-o", str(client / "build"),
         str(client / "client.zi"), cwd=apps[0], env={**env, "LDLIBS": "-lcrypto"})
    call(str(client / "build/client"), str(apps[0]), cwd=apps[0], env=env)
    print("PASS: native package artifacts, invocation options, prerequisite aliases, library APIs and failure isolation", flush=True)


def main():
    import os
    import sys
    import tempfile
    from packages import call, write, init, commit
    compiler = Path(__file__).resolve().parents[1]
    (compiler / "work").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="package-artifacts-", dir=compiler / "work") as scratch:
        root = Path(scratch)
        env = dict(os.environ, XDG_CACHE_HOME=str(root / "cache"),
                   ZIRAN_TEST_ALLOW_LOCAL_GIT="1", ZIRAN_PINNED_LAUNCHER="1",
                   GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="protocol.file.allow",
                   GIT_CONFIG_VALUE_0="always")
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        check_artifacts(str(Path(sys.argv[1]).resolve()), root, env, compiler,
                        call, write, init, commit)


if __name__ == "__main__":
    main()
