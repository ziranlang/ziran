#!/usr/bin/env python3
"""Local Git fixtures for package locking, aliases, and two dependency revisions."""

from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile


def call(*args: str, cwd: Path, env: dict, succeed: bool = True) -> str:
    result = subprocess.run(args, cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if succeed != (result.returncode == 0):
        raise AssertionError(f"{' '.join(args)} returned {result.returncode}:\n{result.stdout}")
    return result.stdout


def write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def commit(repo: Path, env: dict) -> None:
    call("git", "add", ".", cwd=repo, env=env)
    call("git", "-c", "user.name=Ziran Test", "-c", "user.email=test@example.invalid",
         "commit", "-qm", "fixture", cwd=repo, env=env)


def init(repo: Path, env: dict) -> None:
    repo.mkdir()
    call("git", "init", "-qb", "master", cwd=repo, env=env)


def compile_app(ziran: str, app: Path, output: Path, compiler: Path,
                env: dict, source: Path, project: bool) -> None:
    args = [ziran, "build", "--target=c"]
    if project:
        args.append("--project")
    else:
        args.extend(["--root", str(source.parent)])
    args.extend(["--entry", "app:main", "-o", str(output), str(source)])
    call(*args, cwd=app, env=env)
    call("cc", "-std=c99", "-pedantic-errors", f"-I{compiler / 'include'}",
         f"-I{output}", *(str(path) for path in sorted(output.glob("*.c"))),
         "-lm", "-o", str(output / "app"), cwd=app, env=env)
    call(str(output / "app"), cwd=app, env=env)


def main() -> None:
    ziran = str(Path(sys.argv[1]).resolve())
    compiler = Path(ziran).parents[2]
    with tempfile.TemporaryDirectory(prefix="ziran-packages-") as scratch:
        root = Path(scratch)
        env = os.environ.copy()
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        env["XDG_CACHE_HOME"] = str(root / "cache")
        env["ZIRAN_TEST_ALLOW_LOCAL_GIT"] = "1"
        env["GIT_CONFIG_COUNT"] = "1"
        env["GIT_CONFIG_KEY_0"] = "protocol.file.allow"
        env["GIT_CONFIG_VALUE_0"] = "always"

        library = root / "library"
        init(library, env)
        write(library / "ziran.toml", """[package]
name = "Library"
module_roots = ["src"]
[exports]
Value = "src/Value.zi"
value = "src/module.zi"
""")
        write(library / "src/Value.zi", "Value :: () -> s32 { return 1 }\n")
        write(library / "src/module.zi", '''using Original :: #import "Value";
LowerValue :: () -> s32 { return Value() }
''')
        commit(library, env)
        call("git", "branch", "v1", cwd=library, env=env)
        write(library / "src/Value.zi", "Value :: () -> s32 { return 2 }\n")
        commit(library, env)
        call("git", "branch", "v2", cwd=library, env=env)

        providers = []
        for name, revision in (("A", "v1"), ("B", "v2")):
            provider = root / name
            providers.append(provider)
            init(provider, env)
            write(provider / "ziran.toml", f'''[package]
name = "{name}"
module_roots = ["src"]
[dependencies.Library]
git = "{library.as_uri()}"
ref = "{revision}"
[exports]
{name} = "src/{name}.zi"
''')
            imported = "Value" if name == "A" else "value"
            called = "Value" if name == "A" else "LowerValue"
            write(provider / f"src/{name}.zi",
                  f'#import "{imported}"\nFrom{name} :: () -> s32 {{ return {called}() }}\n')
            commit(provider, env)

        app = root / "app"
        app.mkdir()
        add_app = root / "add-app"
        add_app.mkdir()
        write(add_app / "ziran.toml", f'''[package]
name = "AddConsumer"
entry = "src/app.zi"
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
''')
        write(add_app / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(add_app / "src/app.zi", '#import "value"\n')
        add_env = env.copy()
        add_env["GIT_CONFIG_COUNT"] = "2"
        add_env["GIT_CONFIG_KEY_1"] = f"url.{library.as_uri()}.insteadOf"
        add_env["GIT_CONFIG_VALUE_1"] = "https://github.com/example/library.git"
        call(ziran, "add", "example/library", cwd=add_app, env=add_env)
        assert '[dependencies.library]' in (add_app / "ziran.toml").read_text()
        call(ziran, "check", "--project", cwd=add_app, env=add_env)

        # A package whose modules live at its root checks from that root.
        flat_app = root / "flat-app"
        flat_app.mkdir()
        write(flat_app / "ziran.toml", f'''[package]
name = "Flat"
module_roots = ["."]
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
''')
        write(flat_app / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(flat_app / "helper.zi", "Helper :: () -> s32 { return 0 }\n")
        write(flat_app / "flat.zi", '#import "helper"\nFlat :: () -> s32 { return Helper() }\n')
        call(ziran, "lock", cwd=flat_app, env=env)
        call(ziran, "check", "--project", "flat.zi", cwd=flat_app, env=env)
        base_manifest = f'''[package]
name = "App"
entry = "src/app.zi"
module_roots = ["src"]
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
[dependencies.A]
git = "{providers[0].as_uri()}"
ref = "master"
[dependencies.B]
git = "{providers[1].as_uri()}"
ref = "master"
'''
        write(app / "ziran.toml", base_manifest)
        write(app / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(app / "src/Public/module.zi", '''using First :: #import "A";
using Second :: #import "B";
''')
        write(app / "src/app.zi", '''using Both :: #import "Public";
#program_export
main :: () -> s32 {
    if FromA() + FromB() == 3 { return 0 }
    return 1
}
''')
        call(ziran, "lock", cwd=app, env=env)
        saved_lock = (app / "ziran.lock").read_bytes()
        call(ziran, "lock", cwd=app, env=env)
        assert (app / "ziran.lock").read_bytes() == saved_lock
        write(app / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\nA = "{providers[1]}"\n')
        call(ziran, "update", "ziran", cwd=app, env=env)
        assert (app / "ziran.lock").read_bytes() == saved_lock
        write(app / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        call(ziran, "update", cwd=app, env=env)
        assert (app / "ziran.lock").read_bytes() == saved_lock
        call(ziran, "fetch", "--offline", cwd=app, env=env)
        call(ziran, "check", "--project", cwd=app, env=env)

        bootstrap = root / "bootstrap/build/bin"
        bootstrap.mkdir(parents=True)
        shutil.copy2(compiler / "build/bin/ziran", bootstrap / "ziran")
        call(str(bootstrap / "ziran"), "check", "--project", cwd=app,
             env=env)
        module_map = (app / "build/.ziran/module-map.tsv").read_text()
        assert len([line for line in module_map.splitlines()
                    if line.startswith("M\t") and line.split("\t")[2] == "Value"]) == 2
        compile_app(ziran, app, root / "source-c", compiler, env,
                    app / "src/app.zi", True)
        call(ziran, "ir", "--project", "--entry", "app:main", "-o",
             str(root / "ir"), str(app / "src/app.zi"), cwd=app, env=env)
        assert len(list((root / "ir").glob("p*_Value.zir"))) == 2
        compile_app(ziran, app, root / "saved-c", compiler, env,
                    root / "ir/app.zir", False)
        if shutil.which("c++"):
            cpp = root / "cpp"
            call(ziran, "build", "--project", "--target=cpp", "--entry",
                 "app:main", "-o", str(cpp), str(app / "src/app.zi"),
                 cwd=app, env=env)
            for source in sorted(cpp.glob("*.cpp")):
                call("c++", "-std=c++17", f"-I{compiler / 'include'}",
                     f"-I{cpp}", "-fsyntax-only", str(source), cwd=app,
                     env=env)
        if shutil.which("go"):
            go = root / "go"
            call(ziran, "build", "--project", "--target=go", "--pkg",
                 "main", "--entry", "app:main", "-o", str(go),
                 str(app / "src/app.zi"), cwd=app, env=env)
            go_env = {**env, "GO111MODULE": "off"}
            call("go", "test", ".", cwd=go, env=go_env)
        call(ziran, "bundle", "--project", "--entry", "app:main",
             "-o", str(root / "source.zib"), str(app / "src/app.zi"),
             cwd=app, env=env)
        call(ziran, "bundle", "--root", str(root / "ir"),
             "--entry", "app:main", "-o", str(root / "saved.zib"),
             str(root / "ir/app.zir"), cwd=app, env=env)
        assert (root / "source.zib").read_bytes() == (root / "saved.zib").read_bytes()
        assert call(ziran, "run", str(root / "source.zib"), cwd=app,
                    env=env).strip() == "0"

        locked_failure = call(ziran, "check", "--project", "--locked",
                              cwd=app, env=env, succeed=False)
        assert "local overrides" in locked_failure
        write(app / "ziran.toml", base_manifest + '\n[dependencies.Extra]\ngit = "https://example.com/extra.git"\n')
        drift = call(ziran, "check", "--project", cwd=app, env=env,
                     succeed=False)
        assert "differ from ziran.lock" in drift
        write(app / "ziran.toml", base_manifest)

        # Both direct packages exporting A must fail before compiler lookup.
        write(providers[1] / "src/A.zi",
              '#import "Value"\nFromB :: () -> s32 { return Value() }\n')
        write(providers[1] / "ziran.toml", f'''[package]
name = "B"
module_roots = ["src"]
[dependencies.Library]
git = "{library.as_uri()}"
ref = "v2"
[exports]
A = "src/A.zi"
''')
        commit(providers[1], env)
        call(ziran, "update", "b", cwd=app, env=env)
        collision = call(ziran, "check", "--project", cwd=app, env=env,
                         succeed=False)
        assert "exported by both" in collision
        aliased = base_manifest.replace(
            f'[dependencies.B]\ngit = "{providers[1].as_uri()}"\nref = "master"',
            f'[dependencies.B]\ngit = "{providers[1].as_uri()}"\nref = "master"\nmodule_aliases = {{ A = "OtherA" }}')
        write(app / "ziran.toml", aliased)
        write(app / "src/Public/module.zi", '''using First :: #import "A";
using Second :: #import "OtherA";
''')
        write(app / "src/app.zi", '''using Both :: #import "Public";
#program_export
main :: () -> s32 {
    if FromA() + FromB() == 3 { return 0 }
    return 1
}
''')
        call(ziran, "check", "--project", cwd=app, env=env)

        # A targeted update refreshes the selected package's dependency tree.
        before = json.loads((app / "ziran.lock").read_text())
        old_library = next(item["commit"] for item in before["packages"]
                           if item["name"] == "Library" and item["ref"] == "v1")
        call("git", "checkout", "v1", cwd=library, env=env)
        write(library / "src/Value.zi", "Value :: () -> s32 { return 3 }\n")
        commit(library, env)
        call("git", "checkout", "master", cwd=library, env=env)
        call(ziran, "update", "A", cwd=app, env=env)
        after = json.loads((app / "ziran.lock").read_text())
        new_library = next(item["commit"] for item in after["packages"]
                           if item["name"] == "Library" and item["ref"] == "v1")
        assert new_library != old_library

        # A backend is fetched from the locked package checkout on demand.
        backend_source = root / "backend-source"
        init(backend_source, env)
        write(backend_source / "src/raylib.h", "/* pinned backend */\n")
        commit(backend_source, env)
        backend_url = "https://example.invalid/backend-source.git"
        env["GIT_CONFIG_COUNT"] = "2"
        env["GIT_CONFIG_KEY_1"] = f"url.{backend_source.as_uri()}.insteadOf"
        env["GIT_CONFIG_VALUE_1"] = backend_url
        backend = root / "backend"
        init(backend, env)
        write(backend / "ziran.toml", """[package]
name = "Backend"
module_roots = ["src"]
[exports]
Backend = "src/Backend.zi"
""")
        write(backend / "src/Backend.zi", "BackendValue :: 1;\n")
        call("git", "submodule", "add", backend_url, "vendor/raylib",
             cwd=backend, env=env)
        commit(backend, env)
        write(app / "ziran.toml", aliased +
              f'\n[dependencies.Backend]\ngit = "{backend.as_uri()}"\nref = "master"\n')
        call(ziran, "lock", cwd=app, env=env)
        missing = call(ziran, "pkg", "path", "Backend", "--submodules", "--offline",
                       cwd=app, env=env, succeed=False)
        assert "submodules" in missing and "missing" in missing
        backend_path = Path(call(ziran, "pkg", "path", "Backend", "--submodules",
                                 cwd=app, env=env).strip())
        assert (backend_path / "vendor/raylib/src/raylib.h").is_file()
        offline_path = Path(call(ziran, "pkg", "path", "Backend", "--submodules",
                                 "--offline", cwd=app, env=env).strip())
        assert offline_path == backend_path

        # A source package pins a repository without a Ziran manifest, such as
        # a C library that a platform build compiles from source.
        native = root / "native"
        init(native, env)
        write(native / "native.c", "int native_value(void) { return 7; }\n")
        commit(native, env)
        sourced = aliased + (
            f'\n[dependencies.Backend]\ngit = "{backend.as_uri()}"\nref = "master"\n'
            f'\n[dependencies.native]\ngit = "{native.as_uri()}"\nref = "master"\nsource = true\n')
        write(app / "ziran.toml", sourced)
        call(ziran, "lock", cwd=app, env=env)
        locked = json.loads((app / "ziran.lock").read_text())
        native_entry = next(item for item in locked["packages"]
                            if item["name"] == "native")
        assert native_entry["source"] is True and native_entry["dependencies"] == {}
        assert all("source" not in item for item in locked["packages"]
                   if item["name"] != "native")
        native_path = Path(call(ziran, "pkg", "path", "native", cwd=app,
                                env=env).strip())
        assert (native_path / "native.c").is_file()
        call(ziran, "fetch", "--offline", cwd=app, env=env)
        call(ziran, "check", "--project", cwd=app, env=env)
        saved_lock = (app / "ziran.lock").read_bytes()
        call(ziran, "lock", cwd=app, env=env)
        assert (app / "ziran.lock").read_bytes() == saved_lock
        write(app / "ziran.toml", sourced.replace("source = true\n", ""))
        drift = call(ziran, "check", "--project", cwd=app, env=env,
                     succeed=False)
        assert "differ from ziran.lock" in drift
        write(app / "ziran.toml", sourced.replace("source = true", "source = yes"))
        invalid = call(ziran, "lock", cwd=app, env=env, succeed=False)
        assert "cannot resolve" in invalid
        write(app / "ziran.toml", sourced)

        native_env = env.copy()
        native_env["GIT_CONFIG_COUNT"] = "2"
        native_env["GIT_CONFIG_KEY_1"] = f"url.{native.as_uri()}.insteadOf"
        native_env["GIT_CONFIG_VALUE_1"] = "https://example.invalid/native.git"
        call(ziran, "add", "native", "--git", "https://example.invalid/native.git",
             "--source", cwd=add_app, env=native_env)
        assert "[dependencies.native]" in (add_app / "ziran.toml").read_text()
        assert "source = true" in (add_app / "ziran.toml").read_text()
        added = Path(call(ziran, "pkg", "path", "native", cwd=add_app,
                          env=native_env).strip())
        assert (added / "native.c").is_file()


if __name__ == "__main__":
    main()
