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
    # Under `make check -jN` the jobserver descriptors do not reach this
    # process, so a nested make would warn into output the tests compare.
    env = {k: v for k, v in env.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")}
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


def git_env(env: dict, redirects: list) -> dict:
    """Maps public URL prefixes to local fixtures with git's insteadOf."""
    result = env.copy()
    entries = [("protocol.file.allow", "always")]
    entries += [(f"url.{target}.insteadOf", prefix) for prefix, target in redirects]
    result["GIT_CONFIG_COUNT"] = str(len(entries))
    for index, (key, value) in enumerate(entries):
        result[f"GIT_CONFIG_KEY_{index}"] = key
        result[f"GIT_CONFIG_VALUE_{index}"] = value
    return result


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
    # The compiler repository is the one holding this test, wherever the
    # binary under test was built.
    compiler = Path(__file__).resolve().parents[1]
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

        # Any spelling of a Git repository can be added directly. SSH and
        # HTTPS name the same package, and modules import as NAME/Module.
        url_app = root / "url-app"
        url_app.mkdir()
        write(url_app / "ziran.toml", f'''[package]
name = "UrlConsumer"
entry = "src/app.zi"
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
''')
        write(url_app / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(url_app / "src/app.zi", '''#import "ssh_library/Value"
#import "std/text"
#program_export
main :: () -> s32 {
    if Value() == 2 && LowerASCII(cast(u8)65) == cast(u8)97 { return 0 }
    return 1
}
''')
        url_env = git_env(env, [
            ("git@example.invalid:owner/ssh-library.git", library.as_uri()),
            ("https://example.invalid/owner/ssh-library.git", library.as_uri()),
            # SSH to fallback.git fails; its HTTPS spelling works.
            ("git@example.invalid:owner/fallback", "file:///nonexistent-ssh-transport/"),
            ("https://example.invalid/owner/fallback.git", library.as_uri()),
        ])
        added = call(ziran, "add", "git@example.invalid:owner/ssh-library.git",
                     cwd=url_app, env=url_env)
        assert "added ssh_library from git@example.invalid:owner/ssh-library.git" in added, added
        assert '#import "ssh_library/Value"' in added, added
        manifest = (url_app / "ziran.toml").read_text()
        assert 'git = "git@example.invalid:owner/ssh-library.git"' in manifest
        compile_app(ziran, url_app, root / "url-c", compiler, url_env,
                    url_app / "src/app.zi", True)
        # The HTTPS spelling of the same repository keeps the lock valid.
        write(url_app / "ziran.toml", manifest.replace(
            "git@example.invalid:owner/ssh-library.git",
            "https://example.invalid/owner/ssh-library"))
        call(ziran, "check", "--project", cwd=url_app, env=url_env)
        duplicate = call(ziran, "add", "https://example.invalid/owner/ssh-library.git",
                         cwd=url_app, env=url_env, succeed=False)
        assert "already the dependency ssh_library" in duplicate, duplicate
        write(url_app / "ziran.toml", manifest)
        fallback = call(ziran, "add", "git@example.invalid:owner/fallback.git",
                        "--name", "fallback", cwd=url_app, env=url_env)
        assert "added fallback" in fallback, fallback
        listed = call(ziran, "pkg", "list", cwd=url_app, env=url_env)
        assert '#import "fallback/Value"' in listed, listed
        assert '#import "std/NAME"' in listed, listed
        for bad in ("https://user:secret@example.invalid/owner/repo.git",
                    "-oProxyCommand=touch@example.invalid:owner/repo.git",
                    "git@example.invalid:../escape.git", "not a url"):
            rejected = call(ziran, "add", bad, cwd=url_app, env=url_env,
                            succeed=False)
            assert "not a Git repository URL" in rejected, rejected
        # Two dependencies naming one repository over different transports
        # share one locked package, so its types are the same everywhere.
        write(url_app / "ziran.toml", manifest +
              '\n[dependencies.over_https]\ngit = "https://example.invalid/owner/ssh-library.git"\n')
        call(ziran, "lock", cwd=url_app, env=url_env)
        shared = json.loads((url_app / "ziran.lock").read_text())
        assert len(shared["packages"]) == 1, shared
        assert len(set(shared["root"]["dependencies"].values())) == 1, shared
        write(url_app / "ziran.toml", manifest)
        call(ziran, "lock", cwd=url_app, env=url_env)

        # Without a project, std/NAME finds NAME on the module path and a
        # dependency import explains that it needs a project.
        standalone = root / "standalone"
        write(standalone / "main.zi", '''#import "std/text"
#program_export
main :: () -> s32 { return cast(s32)LowerASCII(cast(u8)65) - 97 }
''')
        call(ziran, "check", "--root", str(standalone), "--module-path",
             str(compiler / "std"), str(standalone / "main.zi"), cwd=root, env=env)
        write(standalone / "other.zi", '#import "kryon/Widgets"\n')
        needs = call(ziran, "check", "--root", str(standalone),
                     str(standalone / "other.zi"), cwd=root, env=env, succeed=False)
        assert "needs a project" in needs, needs

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

        # A cache entry left at another commit, as an interrupted fetch
        # leaves it, is repaired by the next online fetch.
        entry = Path(call(ziran, "pkg", "path", "A", "--offline",
                          cwd=app, env=env).strip())
        head = call("git", "-C", str(entry), "rev-parse", "HEAD",
                    cwd=app, env=env).strip()
        call("git", "-C", str(entry), "-c", "user.name=t", "-c", "user.email=t@t",
             "commit", "--allow-empty", "-qm", "stale", cwd=app, env=env)
        call(ziran, "fetch", "--offline", cwd=app, env=env, succeed=False)
        call(ziran, "fetch", cwd=app, env=env)
        assert call("git", "-C", str(entry), "rev-parse", "HEAD",
                    cwd=app, env=env).strip() == head
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
        # The project's pinned toolchain runs what it bundled.
        assert call(ziran, "run", "--project", str(root / "source.zib"),
                    cwd=app, env=env).strip() == "0"

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
        assert "exported by more than one dependency; import one of A/A, B/A" in collision, collision
        # The package-qualified names need no module_aliases.
        write(app / "src/Public/module.zi", '''using First :: #import "A/A";
using Second :: #import "B/A";
''')
        call(ziran, "check", "--project", cwd=app, env=env)
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

        # Overrides may name a dependency by its alias; lock and fetch agree.
        write(app / "ziran.toml", sourced +
              f'\n[dependencies.pinned_library]\ngit = "{library.as_uri()}"\nref = "v2"\n')
        call(ziran, "lock", cwd=app, env=env)
        write(app / "ziran.local.toml",
              f'[overrides]\nziran = "{compiler}"\npinned_library = "{library}"\n')
        overridden = call(ziran, "pkg", "path", "pinned_library", cwd=app,
                          env=env).strip()
        assert Path(overridden).resolve() == library.resolve()
        write(app / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(app / "ziran.toml", sourced)
        call(ziran, "lock", cwd=app, env=env)

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

        # A bridge makes the root's app module visible to direct dependencies,
        # such as a UI host that calls the app's frame. A dependency that has
        # its own app module (a library with a demo) keeps using that one.
        host = root / "host"
        init(host, env)
        write(host / "ziran.toml", """[package]
name = "Host"
module_roots = ["src"]
[exports]
Host = "src/Host.zi"
""")
        write(host / "src/Host.zi", '#import "app"\nRunHost :: () -> s32 { return AppFrame() }\n')
        commit(host, env)
        demo = root / "demo"
        init(demo, env)
        write(demo / "ziran.toml", """[package]
name = "Demo"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]
[exports]
Demo = "src/Demo.zi"
""")
        write(demo / "src/app.zi", "DemoFrame :: () -> s32 { return 1 }\n")
        write(demo / "src/Demo.zi", '#import "app"\nDemoValue :: () -> s32 { return DemoFrame() + 1 }\n')
        commit(demo, env)
        bridged = root / "bridged"
        bridged.mkdir()
        write(bridged / "ziran.toml", f'''[package]
name = "Bridged"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
[dependencies.Host]
git = "{host.as_uri()}"
ref = "master"
[dependencies.Demo]
git = "{demo.as_uri()}"
ref = "master"
''')
        write(bridged / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(bridged / "src/app.zi", '''#import "Host"
#import "Demo"
AppFrame :: () -> s32 { return 40 }
#program_export
main :: () -> s32 {
    if RunHost() + DemoValue() == 42 { return 0 }
    return 1
}
''')
        call(ziran, "lock", cwd=bridged, env=env)
        compile_app(ziran, bridged, root / "bridged-c", compiler, env,
                    bridged / "src/app.zi", True)

        # A test host binds a capability that a dependency declares in a
        # module it does not export, naming it by its import path.
        hosted = root / "hosted"
        init(hosted, env)
        write(hosted / "ziran.toml", """[package]
name = "Hosted"
module_roots = ["src"]
[exports]
Hosted = "src/Hosted.zi"
""")
        write(hosted / "src/measure.zi", '''host_api :: #system_library "host_api";
Measure :: (value: s32) -> s32 #foreign host_api;
Measured :: (value: s32) -> s32 { return Measure(value) }
''')
        write(hosted / "src/Hosted.zi",
              '#import "measure"\nHostedValue :: () -> s32 { return Measured(20) + 1 }\n')
        commit(hosted, env)
        host_app = root / "host-app"
        host_app.mkdir()
        write(host_app / "ziran.toml", f'''[package]
name = "HostApp"
entry = "src/app.zi"
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
[dependencies.hosted]
git = "{hosted.as_uri()}"
ref = "master"
''')
        write(host_app / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(host_app / "src/app.zi", '''#import "hosted/Hosted"
#program_export
Doubled :: (value: s32) -> s32 { return value * 2 }
#program_export
main :: () -> s32 { return HostedValue() }
''')
        call(ziran, "lock", cwd=host_app, env=env)
        call(ziran, "bundle", "--project", "--bind",
             "hosted/measure:Measure=app:Doubled", "--entry", "app:main",
             "-o", str(root / "hosted.zib"), str(host_app / "src/app.zi"),
             cwd=host_app, env=env)
        assert call(ziran, "run", str(root / "hosted.zib"), cwd=host_app,
                    env=env).strip() == "41"
        unbound = call(ziran, "bundle", "--project", "--bind",
                       "hosted/missing:Measure=app:Doubled", "--entry", "app:main",
                       "-o", str(root / "missing.zib"), str(host_app / "src/app.zi"),
                       cwd=host_app, env=env, succeed=False)
        assert "no direct dependency has" in unbound, unbound

        # Templates can come from a local package, substitute names in paths
        # and contents, and produce a project whose plain `ziran run` builds
        # and executes the declared program.
        template_package = root / "template-package"
        template_package.mkdir()
        write(template_package / "ziran.toml", """[package]
name = "Templates"
[templates]
app = "templates/app"
""")
        write(template_package / "templates/app/ziran.toml", f'''[package]
name = "{{{{name}}}}"
entry = "src/{{{{module}}}}.zi"
module_roots = ["src"]
links = ["m"]
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
''')
        write(template_package / "templates/app/src/{{module}}.zi",
              '#program_export\nmain :: () -> s32 { print("7\\n"); return 0 }\n')
        templated = root / "templated-app"
        created = call(ziran, "new", str(templated), "--template",
                       str(template_package), "--name", "demo-app",
                       cwd=root, env=env)
        assert "created demo-app from Templates:app" in created, created
        assert (templated / "src/demo_app.zi").is_file()
        write(templated / "ziran.local.toml",
              f'[overrides]\nziran = "{compiler}"\n')
        assert call(ziran, "run", cwd=templated, env=env).strip() == "7"

        # `ziran init` merges a template manifest and never overwrites an
        # existing source file.
        initialized = root / "initialized-app"
        initialized.mkdir()
        write(initialized / "ziran.toml", """[package]
name = "Existing"
module_roots = ["src"]
""")
        write(initialized / "src/Existing.zi", "Value :: 11\n")
        applied = call(ziran, "init", "--template", str(template_package),
                       "--name", "Existing", cwd=initialized, env=env)
        assert "applied Templates:app" in applied, applied
        assert (initialized / "src/Existing.zi").read_text() == "Value :: 11\n"
        assert 'entry = "src/Existing.zi"' in (initialized / "ziran.toml").read_text()

        # A declared link reaches the native linker. The invalid name makes
        # this a negative test without depending on a platform library list.
        write(templated / "ziran.toml",
              (templated / "ziran.toml").read_text().replace(
                  'links = ["m"]', 'links = ["ziran-missing-link"]'))
        missing_link = call(ziran, "build", cwd=templated, env=env, succeed=False)
        assert "ziran-missing-link" in missing_link, missing_link

        # A package declares its tool's options; ziran merges the app's
        # [tool.ALIAS] tables with ziran.local.toml and hands the tool one
        # KEY=VALUE file. With [package] tool, `ziran run`, `build`, `check`,
        # and `install` without a file hand the project to that tool.
        tooling = root / "tooling"
        init(tooling, env)
        write(tooling / "ziran.toml", """[package]
name = "Tooling"
module_roots = ["src"]
[exports]
Tooling = "src/Tooling.zi"
[tool]
project = "build/tool"
[options]
mode = "string!"
fast = "bool=false"
[options.targets."*"]
kind = "string!"
level = "string=1"
[options.install]
label = "string"
""")
        write(tooling / "src/Tooling.zi", "ToolingValue :: 1;\n")
        write(tooling / "tool.sh", """#!/bin/sh
cat "$ZIRAN_TOOL_OPTIONS"
echo "project=$ZIRAN_PROJECT_NAME entry=$ZIRAN_PROJECT_ENTRY args=$*"
echo "install=${ZIRAN_INSTALL_PREFIX:-} ${ZIRAN_INSTALL_BIN:-}"
echo "pinned=${ZIRAN_PINNED_LAUNCHER:-}"
""")
        write(tooling / "Makefile",
              "build/tool: tool.sh\n\tmkdir -p build\n\tcp tool.sh build/tool\n\tchmod 755 build/tool\n")
        commit(tooling, env)
        tooled = root / "tooled"
        tooled.mkdir()
        tooled_manifest = f'''[package]
name = "Tooled"
entry = "src/app.zi"
tool = "Tooling"
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
[dependencies.Tooling]
git = "{tooling.as_uri()}"
ref = "master"
[install]
bin = "tooled"
[tool.Tooling]
mode = "fast"
[tool.Tooling.targets.b]
kind = "x"
[tool.Tooling.targets.a]
kind = "y"
level = "3"
'''
        write(tooled / "ziran.toml", tooled_manifest)
        write(tooled / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(tooled / "src/app.zi", "Main :: () -> s32 { return 0 }\n")
        call(ziran, "lock", cwd=tooled, env=env)
        output = call(ziran, "tool", "Tooling", "build", "--fast", cwd=tooled, env=env)
        assert output.splitlines() == [
            "mode=fast", "fast=false", "targets=b a",
            "targets.b.kind=x", "targets.b.level=1",
            "targets.a.kind=y", "targets.a.level=3",
            "project=Tooled entry=src/app.zi args=build --fast", "install= ",
            "pinned=",
        ], output
        # Another launcher, such as an older installed ziran, hands the
        # project command to the pinned toolchain's own launcher.
        handed = call(str(bootstrap / "ziran"), "tool", "Tooling", "build",
                      cwd=tooled, env=env).splitlines()
        assert "pinned=1" in handed, handed
        write(tooled / "ziran.local.toml",
              f'[overrides]\nziran = "{compiler}"\n[tool.Tooling]\nmode = "slow"\n')
        assert "mode=slow" in call(ziran, "tool", "Tooling", "build",
                                   cwd=tooled, env=env).splitlines()
        write(tooled / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        installed = call(ziran, "install", "--prefix", "local", "--quick",
                         cwd=tooled, env=env).splitlines()
        assert f"install={tooled.resolve()}/local tooled" in installed, installed
        assert "project=Tooled entry=src/app.zi args=install --quick" in installed
        for verb in ("run", "build", "check"):
            handed = call(ziran, verb, "desktop", cwd=tooled, env=env).splitlines()
            assert f"project=Tooled entry=src/app.zi args={verb} desktop" in handed, handed
        # Arguments after `--` reach the tool as they are.
        handed = call(ziran, "run", "--", "--locked", cwd=tooled, env=env).splitlines()
        assert "project=Tooled entry=src/app.zi args=run -- --locked" in handed, handed
        # A named file keeps the command on that file; --target keeps
        # `ziran build` a code generator.
        checked = call(ziran, "check", "--project", "src/app.zi", cwd=tooled, env=env)
        assert "project=Tooled" not in checked, checked
        # --root and --module-path keep a command standalone inside a
        # project, as a package's own build of a helper program needs.
        write(tooled / "helper/helper.zi", '#import "helper_util"\nmain :: () -> s32 { return Util() }\n')
        write(tooled / "helper/helper_util.zi", "Util :: () -> s32 { return 0 }\n")
        call(ziran, "build", "--target=c", "--root", "helper", "-o",
             str(root / "helper-c"), "helper/helper.zi", cwd=tooled, env=env)
        call(ziran, "check", "--root", "helper", "--module-path", str(compiler / "std"),
             "helper/helper.zi", cwd=tooled, env=env)
        generated = call(ziran, "build", "--target=c", "--entry", "app:Main",
                         "-o", str(root / "tooled-c"), cwd=tooled, env=env)
        assert "project=Tooled" not in generated, generated
        assert (root / "tooled-c/app.c").exists()
        # The old [install] tool spelling is reported, not ignored.
        write(tooled / "ziran.toml", tooled_manifest.replace(
            'tool = "Tooling"\n', '', 1).replace('[install]\n', '[install]\ntool = "Tooling"\n'))
        moved = call(ziran, "install", cwd=tooled, env=env, succeed=False)
        assert '[install] tool moved: write tool = "Tooling" under [package]' in moved, moved
        write(tooled / "ziran.toml", tooled_manifest)
        for broken, message in (
            (tooled_manifest.replace('mode = "fast"', 'mode = "fast"\nspeed = "x"'),
             "ziran.toml:15: unknown Tooling option speed"),
            (tooled_manifest.replace('kind = "x"\n', ''),
             "missing required Tooling option targets.b.kind"),
            (tooled_manifest.replace('[tool.Tooling.targets.b]', '[tool.Tooling.targets]'),
             "name the table: [tool.Tooling.targets.NAME]"),
            (tooled_manifest.replace('mode = "fast"', 'mode = "fast"\nfast = "yes"'),
             "fast must be a non-empty bool"),
        ):
            write(tooled / "ziran.toml", broken)
            failure = call(ziran, "tool", "Tooling", "build", cwd=tooled,
                           env=env, succeed=False)
            assert message in failure, failure
        write(tooled / "ziran.toml", tooled_manifest)

        # A package can export many modules: an icon set has one per icon,
        # far more than a handful of entry points.
        wide = root / "wide"
        init(wide, env)
        exports = "".join(f'w{index} = "src/w{index}.zi"\n' for index in range(700))
        write(wide / "ziran.toml", f'''[package]
name = "Wide"
module_roots = ["src"]
[exports]
{exports}''')
        for index in range(700):
            write(wide / f"src/w{index}.zi",
                  f"W{index} :: () -> s32 {{ return {index} }}\n")
        commit(wide, env)
        wide_app = root / "wide-app"
        wide_app.mkdir()
        write(wide_app / "ziran.toml", f'''[package]
name = "WideApp"
entry = "src/app.zi"
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
[dependencies.Wide]
git = "{wide.as_uri()}"
ref = "master"
''')
        write(wide_app / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(wide_app / "src/app.zi",
              '#import "w699"\n#program_export\nmain :: () -> s32 { return W699() - 699 }\n')
        call(ziran, "lock", cwd=wide_app, env=env)
        compile_app(ziran, wide_app, root / "wide-c", compiler, env,
                    wide_app / "src/app.zi", True)

        # A dependency may be pinned by a full commit hash instead of a
        # branch or tag.
        pinned_commit = call("git", "rev-parse", "HEAD", cwd=wide, env=env).strip()
        pinned = root / "pinned-app"
        pinned.mkdir()
        write(pinned / "ziran.toml", f'''[package]
name = "PinnedApp"
entry = "src/app.zi"
[toolchain]
git = "{compiler.as_uri()}"
ref = "master"
[dependencies.Wide]
git = "{wide.as_uri()}"
ref = "{pinned_commit}"
''')
        write(pinned / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
        write(pinned / "src/app.zi",
              '#import "w7"\n#program_export\nmain :: () -> s32 { return W7() - 7 }\n')
        call(ziran, "lock", cwd=pinned, env=env)
        assert pinned_commit in (pinned / "ziran.lock").read_text()
        compile_app(ziran, pinned, root / "pinned-c", compiler, env,
                    pinned / "src/app.zi", True)

        templates(ziran, root, compiler, env)


def templates(ziran: str, root: Path, compiler: Path, env: dict) -> None:
    """Built-in and Git templates, and the program lifecycle of a project
    without a tool: `ziran run`, `build`, `check`, and `install`."""
    # Templates name the public toolchain; the fixture serves it locally.
    env = git_env(env, [("https://github.com/ziranlang/ziran.git", compiler.as_uri())])
    work = root / "templates"
    work.mkdir()
    created = call(ziran, "new", "greeter", cwd=work, env=env)
    assert "created greeter from cli" in created, created
    assert "next: cd greeter && ziran run" in created, created
    greeter = work / "greeter"
    manifest = (greeter / "ziran.toml").read_text()
    assert 'name = "greeter"' in manifest and "{{" not in manifest, manifest
    assert (greeter / ".gitignore").exists() and (greeter / "ziran.lock").exists()
    write(greeter / "ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
    assert call(ziran, "run", cwd=greeter, env=env) == "Hello, world!\n"
    assert call(ziran, "run", "--", "Ada", "--locked", cwd=greeter,
                env=env) == "Hello, Ada!\n"
    call(ziran, "build", cwd=greeter, env=env)
    assert call(str(greeter / "build/greeter"), "Grace", cwd=greeter,
                env=env) == "Hello, Grace!\n"
    call(ziran, "check", cwd=greeter, env=env)
    installed = call(ziran, "install", "--prefix", str(work / "prefix"),
                     cwd=greeter, env=env)
    assert f"installed {work / 'prefix/bin/greeter'}" in installed, installed
    assert call(str(work / "prefix/bin/greeter"), cwd=work, env=env) == "Hello, world!\n"
    # A failing program's status is the command's status.
    write(greeter / "src/main.zi", "main :: () -> s32 { return 3 }\n")
    status = subprocess.run([ziran, "run"], cwd=greeter, env=env).returncode
    assert status == 3, status
    # An occupied directory is not overwritten.
    refused = call(ziran, "new", "greeter", cwd=work, env=env, succeed=False)
    assert "is not empty; use ziran init inside it" in refused, refused

    library = call(ziran, "new", "tiny-lib", "--template", "lib", cwd=work, env=env)
    assert "next: cd tiny-lib && ziran check" in library, library
    assert 'tiny_lib = "src/tiny_lib.zi"' in (work / "tiny-lib/ziran.toml").read_text()
    assert (work / "tiny-lib/src/tiny_lib.zi").exists()
    write(work / "tiny-lib/ziran.local.toml", f'[overrides]\nziran = "{compiler}"\n')
    call(ziran, "check", cwd=work / "tiny-lib", env=env)
    unknown = call(ziran, "new", "x", "--template", "nosuch", cwd=work, env=env,
                   succeed=False)
    assert "no built-in template nosuch" in unknown, unknown

    # A Git package lists templates under [templates]; the first is the
    # default and SOURCE:NAME picks another.
    kit = root / "kit"
    init(kit, env)
    write(kit / "ziran.toml", """[package]
name = "Kit"
module_roots = ["src"]
[templates]
window = "templates/window"
panel = "templates/panel"
""")
    write(kit / "templates/window/ziran.toml", """[package]
name = "{{name}}"
entry = "src/app.zi"
module_roots = ["src", "tests"]
bridge_modules = ["app"]

[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"

# Window settings.
[window]
title = "{{name}}"
""")
    write(kit / "templates/window/src/app.zi",
          '// {{name}} as {{module}}\nmain :: () -> s32 { return 0 }\n')
    write(kit / "templates/window/tests/app_test.zi",
          'AppTest :: () -> s32 { return 42 }\n')
    write(kit / "templates/panel/ziran.toml", """[package]
name = "{{name}}"
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
""")
    write(kit / "templates/panel/panel.txt", "panel for {{name}}\n")
    commit(kit, env)
    kit_env = git_env(env, [("https://github.com/ziranlang/ziran.git", compiler.as_uri()),
                            ("https://example.invalid/owner/kit.git", kit.as_uri())])
    windowed = call(ziran, "new", "my-app", "--template", "example.invalid/owner/kit",
                    cwd=work, env=kit_env)
    assert "created my-app from Kit:window" in windowed, windowed
    assert (work / "my-app/src/app.zi").read_text().startswith("// my-app as my_app")
    call(ziran, "new", "paneled", "--template",
         "https://example.invalid/owner/kit.git:panel", cwd=work, env=kit_env)
    assert (work / "paneled/panel.txt").read_text() == "panel for paneled\n"
    missing = call(ziran, "new", "other", "--template", "../kit:nosuch",
                   cwd=work, env=kit_env, succeed=False)
    assert "package Kit has no template nosuch; it has window, panel" in missing, missing

    # `ziran init` merges a template into an existing project: missing
    # tables and keys are added, arrays grow, and the project's values stay.
    write(greeter / "src/main.zi", "main :: () -> s32 { return 0 }\n")
    merged = call(ziran, "init", "--template", str(kit), cwd=greeter, env=kit_env)
    after = (greeter / "ziran.toml").read_text()
    assert 'kept entry = "src/main.zi" in [package]; the template has "src/app.zi"' in merged, merged
    assert "extended module_roots in [package]" in merged, merged
    assert "added bridge_modules to [package]" in merged, merged
    assert "added [window]" in merged, merged
    assert 'entry = "src/main.zi"' in after, after
    assert 'module_roots = ["src", "tests"]' in after, after
    assert '# Window settings.\n[window]\ntitle = "greeter"' in after, after
    assert (greeter / "src/app.zi").exists()
    again = call(ziran, "init", "--template", str(kit), cwd=greeter, env=kit_env)
    assert (greeter / "ziran.toml").read_text() == after, again
    assert "kept src/app.zi (already present)" in again, again
    call(ziran, "run", cwd=greeter, env=kit_env)


if __name__ == "__main__":
    main()
