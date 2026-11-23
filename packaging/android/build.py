#!/usr/bin/env python3
"""Build the ZX Runner APK from this checkout.

The runner is the same Ziran program as the desktop `zx` (cmd/zx_runner*.zi),
compiled with ZX_ANDROID so std/guest_window is answered by the activity's
EGL glue in packaging/android/native. Cross-building the engine is the slow
step and is reused until --rebuild-engine.

    python3 packaging/android/build.py                 # both ABIs
    python3 packaging/android/build.py --arch aarch64
    python3 packaging/android/build.py --install       # build and adb install
"""
import argparse
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = "org.ziranlang.zx"
ABIS = {
    "armv7": ("armeabi-v7a", "armv7a-linux-androideabi"),
    "aarch64": ("arm64-v8a", "aarch64-linux-android"),
}


def run(argv, allow_failure=False):
    argv = [str(item) for item in argv]
    print("+ " + " ".join(argv), flush=True)
    status = subprocess.run(argv, cwd=ROOT).returncode
    if status != 0 and not allow_failure:
        raise SystemExit("failed: " + " ".join(argv))
    return status


def android_home():
    for name in ("ANDROID_HOME", "ANDROID_SDK_ROOT"):
        value = os.environ.get(name)
        if value:
            return Path(value)
    candidate = Path.home() / "Android/Sdk"
    if candidate.is_dir():
        return candidate
    raise SystemExit("set ANDROID_HOME to the Android SDK")


def newest(directory, names=None):
    entries = [path for path in directory.iterdir() if path.is_dir()]
    if names:
        entries = [path for path in entries if path.name in names]
    if not entries:
        raise SystemExit(f"nothing usable under {directory}")

    def key(path):
        parts = []
        for piece in path.name.replace("-", ".").split("."):
            parts.append(int(piece) if piece.isdigit() else 0)
        return parts

    return sorted(entries, key=key)[-1]


def find_ndk(sdk):
    for name in ("ANDROID_NDK_HOME", "ANDROID_NDK"):
        value = os.environ.get(name)
        if value:
            return Path(value)
    directory = sdk / "ndk"
    if directory.is_dir():
        return newest(directory)
    raise SystemExit("set ANDROID_NDK_HOME to an NDK (28.x is tested)")


def tools_for(sdk, ndk, abi):
    build_tools = Path(os.environ.get("ANDROID_BUILD_TOOLS") or newest(sdk / "build-tools"))
    platform = Path(os.environ.get("ANDROID_PLATFORM_JAR") or (sdk / "platforms" / "android-34" / "android.jar"))
    if not platform.is_file():
        platform = newest(sdk / "platforms") / "android.jar"
    if not platform.is_file():
        raise SystemExit("no android.jar under " + str(sdk / "platforms"))
    toolchain = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin"
    _, triple = ABIS[abi]
    compiler = toolchain / f"{triple}28-clang"
    return {
        "aapt2": build_tools / "aapt2",
        "apksigner": build_tools / "apksigner",
        "zipalign": build_tools / "zipalign",
        "android_jar": platform,
        "cc": compiler,
        "ar": toolchain / "llvm-ar",
        "objcopy": toolchain / "llvm-objcopy",
        "glue": ndk / "sources/android/native_app_glue",
    }


def engine_build(out, tools, abi, rebuild):
    engine = out / f"engine-{abi}"
    library = engine / "libziran.a"
    if library.is_file() and not rebuild:
        print(f"reusing {library.relative_to(ROOT)}", flush=True)
        return library
    if engine.is_dir():
        shutil.rmtree(engine)
    engine.mkdir(parents=True, exist_ok=True)
    zi2c = ROOT / "build/bin/zi2c"
    if not zi2c.is_file():
        raise SystemExit("build/bin/zi2c is missing; run make first")
    jobs = os.environ.get("BUILD_QUEUE_JOBS") or "2"
    run([
        "make", "--no-print-directory", "-j" + jobs,
        "BUILD_DIR=" + str(engine),
        "CC=" + str(tools["cc"]),
        "AR=" + str(tools["ar"]),
        "OBJCOPY=" + str(tools["objcopy"]),
        "GEN_ZI2C=" + str(zi2c),
        "CFLAGS=-O2 -fPIC -ffunction-sections -fdata-sections",
        str(library),
        str(engine / "bin/zi2zx"),
    ])
    return library


def runner_source(out, abi):
    generated = out / f"runner-c-{abi}"
    zi2c = ROOT / "build/bin/zi2c"
    if not zi2c.is_file():
        raise SystemExit("build/bin/zi2c is missing; run make first")
    if generated.is_dir():
        shutil.rmtree(generated)
    generated.mkdir(parents=True)
    machine = "__arm__" if abi == "armv7" else "__aarch64__"
    run([
        zi2c, "--entry", "zx_runner:main", "--define", "ZX_ANDROID",
        "--target-os", "android", "--define", machine,
        "--root", "cmd", "--module-path", "std",
        "-o", generated, "cmd/zx_runner.zi",
    ])
    return generated


def native_library(out, tools, abi, engine, generated):
    abi_name, _ = ABIS[abi]
    target = out / abi_name
    target.mkdir(parents=True, exist_ok=True)
    library = target / "libzxrunner.so"
    sources = sorted(generated.glob("*.c")) + [
        ROOT / "packaging/android/native/zx_android.c",
        tools["glue"] / "android_native_app_glue.c",
    ]
    command = [
        tools["cc"], "-O2", "-fPIC", "-D_GNU_SOURCE", "-std=c11",
        "-I" + str(ROOT / "include"),
        "-I" + str(generated),
        "-I" + str(tools["glue"]),
        "-shared", "-o", library,
        *sources, engine,
        "-Wl,-u,ANativeActivity_onCreate",
        "-Wl,--build-id=sha1",
        "-llog", "-landroid", "-lEGL", "-lGLESv2", "-lm", "-pthread", "-ldl",
    ]
    run(command)
    print(f"built {library.relative_to(ROOT)} ({library.stat().st_size} bytes)", flush=True)
    return library


def java_dex(out, tools):
    classes = out / "classes"
    dex = out / "dex"
    if classes.is_dir():
        shutil.rmtree(classes)
    if dex.is_dir():
        shutil.rmtree(dex)
    classes.mkdir(parents=True)
    dex.mkdir(parents=True)
    sources = sorted((ROOT / "packaging/android/java").rglob("*.java"))
    run([
        "javac", "-source", "8", "-target", "8", "-Xlint:-options",
        "-classpath", tools["android_jar"], "-d", classes, *sources,
    ])
    compiled = sorted(str(path) for path in classes.rglob("*.class"))
    d8 = Path(tools["aapt2"]).parent / "d8"
    run([
        d8, "--min-api", "28", "--lib", tools["android_jar"],
        "--output", dex, *compiled,
    ])
    return dex / "classes.dex"


def package_apk(out, tools, libraries, dex):
    resources = out / "resources.zip"
    base = out / "zx-runner-base.apk"
    unsigned = out / "zx-runner-unsigned.apk"
    APK = out / "zx-runner.apk"
    if resources.exists():
        resources.unlink()
    run([tools["aapt2"], "compile", "--dir", ROOT / "packaging/android/res", "-o", resources])
    run([
        tools["aapt2"], "link", "-o", base, "-I", tools["android_jar"],
        "--manifest", ROOT / "packaging/android/AndroidManifest.xml",
        "--auto-add-overlay", resources,
    ])
    if unsigned.exists():
        unsigned.unlink()
    with zipfile.ZipFile(base, "r") as source, zipfile.ZipFile(unsigned, "w") as target:
        for item in source.infolist():
            target.writestr(item, source.read(item.filename))
        target.write(dex, "classes.dex", compress_type=zipfile.ZIP_DEFLATED)
        for abi, library in libraries.items():
            target.write(library, f"lib/{abi}/libzxrunner.so", compress_type=zipfile.ZIP_STORED)
    aligned = out / "zx-runner-aligned.apk"
    if aligned.exists():
        aligned.unlink()
    run([tools["zipalign"], "-f", "4", unsigned, aligned])
    key = Path.home() / ".android/debug.keystore"
    key.parent.mkdir(parents=True, exist_ok=True)
    if not key.exists():
        run([
            "keytool", "-genkeypair", "-keystore", key, "-storepass", "android",
            "-keypass", "android", "-alias", "androiddebugkey",
            "-dname", "CN=Android Debug,O=Android,C=US", "-keyalg", "RSA", "-validity", "10000",
        ])
    run([
        tools["apksigner"], "sign", "--ks", key, "--ks-pass", "pass:android",
        "--out", APK, aligned,
    ])
    run([tools["apksigner"], "verify", "--verbose", APK])
    return APK


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--arch", choices=["all", *ABIS], default="all")
    parser.add_argument("--rebuild-engine", action="store_true")
    parser.add_argument("--install", action="store_true")
    parser.add_argument("--serial", default=os.environ.get("ANDROID_SERIAL", ""))
    parser.add_argument("--out", default=str(ROOT / "build/android-zx"))
    arguments = parser.parse_args()

    sdk = android_home()
    ndk = find_ndk(sdk)
    out = Path(arguments.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    architectures = list(ABIS) if arguments.arch == "all" else [arguments.arch]

    libraries = {}
    for abi in architectures:
        tools = tools_for(sdk, ndk, abi)
        generated = runner_source(out, abi)
        engine = engine_build(out, tools, abi, arguments.rebuild_engine)
        library = native_library(out, tools, abi, engine, generated)
        libraries[ABIS[abi][0]] = library
    tools = tools_for(sdk, ndk, architectures[0])
    dex = java_dex(out, tools)
    apk = package_apk(out, tools, libraries, dex)
    print(f"\nZX Runner APK: {apk}", flush=True)

    if arguments.install:
        adb = ["adb"] + (["-s", arguments.serial] if arguments.serial else [])
        run([*adb, "install", "-r", apk])
        print("installed", flush=True)


if __name__ == "__main__":
    main()
