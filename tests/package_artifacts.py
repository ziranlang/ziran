"""Dependency-owned artifact integration fixture; uses real native objects."""
from pathlib import Path
import subprocess


def check_artifacts(ziran, root, env, compiler, call, write, init, commit):
    fixture = root / "artifacts"
    fixture.mkdir()
    provider, tool = fixture / "provider", fixture / "tool"
    for repo in (provider, tool):
        init(repo, env)
    write(tool / "VERSION", "fixture\n")
    commit(tool, env)
    write(provider / "ziran.toml", '''[package]
name = "Provider"
[tool]
project = "build/bin/provider"
[options]
mode = "string=normal"
[artifacts.object]
inputs = ["src/value.c", "tools/provider.zi", "Makefile"]
outputs = ["value.o", "resource.txt"]
''')
    write(provider / ".gitignore", "build/\n")
    write(provider / "src/value.c", "int value(void) { return 42; }\n")
    write(provider / "Makefile", f'''build/bin/provider: tools/provider.zi
\t@mkdir -p build/bin
\t@LDLIBS=-lcrypto {compiler}/build/bin/zi2c --exe --root tools --module-path {compiler}/cmd --module-path {compiler}/std --entry provider:main -o build/bin tools/provider.zi
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
    def build(app=apps[0], target="linux-x86_64-gnu", ok=True, selection="Provider:object", extra=None):
        return call(ziran, "pkg", "artifact", selection, "--platform", target,
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
    write(provider / "ziran.toml", original.replace('outputs = ["value.o", "resource.txt"]', 'outputs = ["../escape"]'))
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
        'outputs = ["value.o", "resource.txt"]',
        'outputs = ["value.o", "resource.txt"]\nrequires = ["self:base"]') +
        '[artifacts.base]\ninputs = ["src/value.c"]\noutputs = ["value.o", "resource.txt"]\n')
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
    print("PASS: native package artifacts reuse, target/options/inputs/toolchain invalidation and failure isolation", flush=True)
