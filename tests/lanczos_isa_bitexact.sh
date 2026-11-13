#!/bin/sh
# std/resample: every SSE2, AVX2, NEON (AArch64 and ARMv7) and WebAssembly
# SIMD128 row kernel makes the same bytes as the scalar reference, and the
# scalar reference the same as std/image's Pillow resizes, for Lanczos,
# bicubic and box over odd sizes, boxes, extreme pictures and split row
# ranges. Runs natively at -O0 and -O2, in each #target copy of the x86-64
# kernels (ZIRAN_SIMD_TIER 1 to 3) and with ZIRAN_NO_ASM. With the
# Android NDK (ANDROID_NDK_HOME) it also builds AArch64 and ARMv7, running
# each under the qemu-user named by QEMU_AARCH64 / QEMU_ARM, or ARMv7 on the
# device ARMV7_ADB_SERIAL names. Static Bionic binaries need an NDK that
# aligns arm64 TLS to 64 bytes and, on 32-bit ARM, host PIDs up to 65535, so
# the runs are opt-in. So is WebAssembly: with WASM_RUN, a command given an
# Emscripten module's .js that prints its output (such as node), or WASI_RUN,
# one given a standalone .wasm (such as wasmtime run), it builds with emcc
# (EMCC, or ~/emsdk's) with and without -msimd128 and runs each.
set -eu

ziran=$1
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lanczos_isa.zi" <<'ZI'
#import "image"
#import "resample"
using Vectors :: #import "vec"
#import "stdio_linux"

seed: u64 = 88172645463325252;
Next :: (limit: s32) -> s32 {
    seed = seed * 6364136223846793005 + 1442695040888963407
    return cast(s32)((seed >> 33) % cast(u64)limit)
}

// Random noise, hard 0/255 checks that ring and clip, soft alpha edges.
Picture :: (width: s32, height: s32, kind: s32) -> Image {
    image := NewImage(width, height)
    for y: 0..height - 1 {
        for x: 0..width - 1 {
            at := (cast(s64)y * width + x) * 4
            for channel: cast(s64)0..3 {
                value := Next(256)
                if kind == 1 { value = ifx ((x / 2 + y + cast(s32)channel) % 2) == 0 then 0 else 255 }
                if kind == 2 && channel == 3 { value = ifx x * 3 < width then 0 else (ifx x * 3 < width * 2 then Next(256) else 255) }
                image.pixels[at + channel] = cast(u8)value
            }
        }
    }
    return image
}

// One resize of the box through the plan at the current kernels, its first
// pass split at SPLIT rows and the second in three ranges.
Run :: (image: *Image, x: float64, y: float64, box_width: float64, box_height: float64,
    width: s32, height: s32, filter: s32, split: s32, result: *Vec(u8)) -> bool {
    work: Vec(u8)
    if !VecResize(work, image.pixels.count) { return false }
    for at: cast(s64)0..image.pixels.count - 1 { work[at] = image.pixels[at] }
    ResamplePremultiply(*work[0], cast(s64)image.width * image.height)
    plan: ResamplePlan
    if !ResamplePlanInto(*plan, image.width, image.height, x, y, box_width, box_height, width, height, filter) { return false }
    if !VecResize(result.*, cast(s64)width * height * 4) { return false }
    stride := cast(s64)image.width * 4
    target_stride := cast(s64)width * 4
    first := ResamplePlanFirstRows(*plan)
    cut := ifx split < first then split else first
    ResamplePlanFirst(*plan, *work[0], stride, *result.*[0], target_stride, cut, first)
    ResamplePlanFirst(*plan, *work[0], stride, *result.*[0], target_stride, 0, cut)
    second := ResamplePlanSecondRows(*plan)
    ResamplePlanSecond(*plan, *work[0], stride, *result.*[0], target_stride, second / 2, second)
    ResamplePlanSecond(*plan, *work[0], stride, *result.*[0], target_stride, 0, second / 3)
    ResamplePlanSecond(*plan, *work[0], stride, *result.*[0], target_stride, second / 3, second / 2)
    return true
}

Same :: (a: *Vec(u8), b: *Vec(u8)) -> bool {
    if a.count != b.count { return false }
    for at: cast(s64)0..a.count - 1 { if a.*[at] != b.*[at] { return false } }
    return true
}

Expected :: (image: *Image, width: s32, height: s32, filter: s32) -> Image {
    if filter == 1 { return ResizeLanczos(image, width, height) }
    if filter == 2 { return ResizeBicubic(image, width, height) }
    return ResizeBox(image, width, height)
}

LEVELS :: 5
CASES :: 420

#program_export
main :: (argc: s32, argv: **u8) -> s32 {
    names: [LEVELS]string = .["scalar", "sse2", "avx2", "neon", "simd128"]
    ran: [LEVELS]bool
    ran[0] = true
    for level: cast(s32)1..LEVELS - 1 { ran[level] = ResampleUseKernels(level) }
    unused ResampleUseKernels(-1)
    checked := 0
    for index: cast(s32)0..CASES - 1 {
        // Small odd sizes, then heavy reductions, enlargements and Jungle's
        // 75% pictures shown at full size.
        source_width := 1 + Next(70); source_height := 1 + Next(40)
        width := 1 + Next(90); height := 1 + Next(50)
        if index % 7 == 1 { source_width = 300 + Next(120); source_height = 150 + Next(80); width = 1 + Next(9); height = 1 + Next(9) }
        if index % 7 == 2 { source_width = 1 + Next(6); source_height = 1 + Next(6); width = 120 + Next(200); height = 60 + Next(90) }
        if index % 7 == 3 { source_width = 120 + Next(40); source_height = 90 + Next(30); width = source_width * 4 / 3; height = source_height * 4 / 3 }
        filter := index % 3
        image := Picture(source_width, source_height, index % 3)
        x := 0.0; y := 0.0
        box_width := cast(float64)source_width; box_height := cast(float64)source_height
        whole := index % 4 != 0
        if !whole {
            x = cast(float64)Next(source_width * 8) / 16.0
            y = cast(float64)Next(source_height * 8) / 16.0
            box_width = (cast(float64)source_width - x) * (0.25 + cast(float64)Next(64) / 85.0)
            box_height = (cast(float64)source_height - y) * (0.25 + cast(float64)Next(64) / 85.0)
            if box_width <= 0.0 || x + box_width > cast(float64)source_width { box_width = cast(float64)source_width - x }
            if box_height <= 0.0 || y + box_height > cast(float64)source_height { box_height = cast(float64)source_height - y }
        }
        split := Next(height + 2)
        unused ResampleUseKernels(RESAMPLE_SCALAR)
        reference: Vec(u8)
        if !Run(*image, x, y, box_width, box_height, width, height, filter, split, *reference) {
            print("plan failed: case %\n", index); return 1
        }
        // The scalar reference is std/image's resize, Pillow's arithmetic.
        tall := filter == 2 && cast(s64)source_height > cast(s64)source_width * 100 && height < source_height
        if whole && !tall {
            expected := Expected(*image, width, height, filter)
            straight: Vec(u8)
            unused VecResize(straight, reference.count)
            for at: cast(s64)0..reference.count - 1 { straight[at] = reference[at] }
            ResampleUnpremultiply(*straight[0], cast(s64)width * height)
            if !Same(*straight, *expected.pixels) {
                print("scalar differs from std/image: case % filter % %x% -> %x%\n", index, filter, source_width, source_height, width, height)
                return 2
            }
        }
        for level: cast(s32)1..LEVELS - 1 {
            if !ran[level] { continue }
            unused ResampleUseKernels(level)
            result: Vec(u8)
            if !Run(*image, x, y, box_width, box_height, width, height, filter, split, *result) || !Same(*result, *reference) {
                print("% differs from scalar: case % filter % %x% -> %x%\n", names[level], index, filter, source_width, source_height, width, height)
                return 3
            }
        }
        checked += 1
    }
    unused ResampleUseKernels(-1)
    print("levels:")
    for level: 0..LEVELS - 1 { if ran[level] { print(" %", names[level]) } }
    print("\nbest: %\ncases: %\n", names[ResampleKernels()], checked)
    return 0
}
ZI

"$ziran" build --target=c --entry lanczos_isa:main --root "$work" \
    --module-path "$root/std" -o "$work/c" "$work/lanczos_isa.zi"
compiler=${CC:-cc}

expect_levels() {
    output=$1; shift
    for level in "$@"; do
        case " $(sed -n 's/^levels://p' "$output") " in
            *" $level "*) ;;
            *) echo "missing $level kernels:"; cat "$output"; exit 1 ;;
        esac
    done
}

for optimization in 0 2; do
    "$compiler" -std=c99 -O"$optimization" -I"$root/include" -I"$work/c" \
        "$work/c"/*.c -lz -lm -o "$work/native-O$optimization"
    "$work/native-O$optimization" > "$work/native-O$optimization.out"
    cat "$work/native-O$optimization.out"
    if test "$(uname -m)" = x86_64; then
        expect_levels "$work/native-O$optimization.out" scalar sse2
        if grep -qw avx2 /proc/cpuinfo 2>/dev/null; then
            expect_levels "$work/native-O$optimization.out" avx2
        fi
    fi
done
if test "$(uname -m)" = x86_64; then
    # Below AVX2 the AVX2 probe fails too, so the SSE2 kernels are the best.
    { echo 'levels: scalar sse2'; echo 'best: sse2'; grep '^cases:' "$work/native-O2.out"; } > "$work/tiers.out"
    for tier in 1 2 3; do
        ZIRAN_SIMD_TIER=$tier "$work/native-O2" > "$work/tier$tier.out"
        cmp -s "$work/tier$tier.out" "$work/tiers.out" ||
            { echo "ZIRAN_SIMD_TIER=$tier:"; cat "$work/tier$tier.out"; exit 1; }
    done
    echo "SSE2, SSSE3 and SSE4.1 copies agree"
fi
"$compiler" -std=c99 -O2 -DZIRAN_NO_ASM -I"$root/include" -I"$work/c" \
    "$work/c"/*.c -lz -lm -o "$work/portable"
"$work/portable" > "$work/portable.out"
grep -qx 'levels: scalar' "$work/portable.out"
grep -qx 'best: scalar' "$work/portable.out"

if test -n "${ANDROID_NDK_HOME:-}"; then
    bin=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin
    for triple in aarch64-linux-android24 armv7a-linux-androideabi24; do
        "$bin/$triple-clang" -std=c99 -O2 -static -Wno-parentheses-equality -I"$root/include" -I"$work/c" \
            "$work/c"/*.c -lz -lm -o "$work/$triple"
    done
    qemu=${QEMU_AARCH64:-}
    if test -n "$qemu"; then
        "$qemu" "$work/aarch64-linux-android24" > "$work/aarch64.out"
        sed 's/^/aarch64: /' "$work/aarch64.out"
        expect_levels "$work/aarch64.out" scalar neon
        grep -qx 'best: neon' "$work/aarch64.out"
    else
        echo "AArch64 built; set QEMU_AARCH64 to run it"
    fi
    qemu=${QEMU_ARM:-}
    device=${ARMV7_ADB_SERIAL:-}
    if test -n "$qemu"; then
        "$qemu" "$work/armv7a-linux-androideabi24" > "$work/armv7.out"
    elif test -n "$device"; then
        remote=/data/local/tmp/ziran-lanczos-isa
        adb -s "$device" push "$work/armv7a-linux-androideabi24" "$remote" >/dev/null
        adb -s "$device" shell "chmod 755 $remote && $remote; status=\$?; rm -f $remote; exit \$status" > "$work/armv7.out"
    fi
    if test -f "$work/armv7.out"; then
        sed 's/^/armv7: /' "$work/armv7.out"
        expect_levels "$work/armv7.out" scalar neon
        grep -qx 'best: neon' "$work/armv7.out"
    else
        echo "ARMv7 built; set QEMU_ARM or ARMV7_ADB_SERIAL to run it"
    fi
fi

# SIMD128 is chosen when the module is built: the -msimd128 module must run
# the SIMD128 kernels, and the other two only the scalar ones. WASI_RUN, a
# command given a standalone module (such as wasmtime run), runs them
# without JavaScript; standalone memory cannot grow, so it starts at 256 MB.
emcc=${EMCC:-$HOME/emsdk/upstream/emscripten/emcc}
run=${WASM_RUN:-}
wasi=${WASI_RUN:-}
if test -n "$run$wasi" && command -v "$emcc" >/dev/null 2>&1; then
    "$ziran" build --target=c --target-os web --target-cpu wasm32 --entry lanczos_isa:main --root "$work" \
        --module-path "$root/std" -o "$work/web-c" "$work/lanczos_isa.zi"
    mkdir -p "$work/web"
    wasm_build() {
        name=$1; shift
        # Unquoted on purpose: the runners may carry options.
        if test -n "$wasi"; then
            "$emcc" -std=c99 -O2 -Wno-parentheses-equality -I"$root/include" -I"$work/web-c" "$@" -sSTANDALONE_WASM \
                -sINITIAL_MEMORY=268435456 "$work/web-c"/*.c -o "$work/web/$name.wasm"
            $wasi "$work/web/$name.wasm" > "$work/web/$name.out"
        else
            "$emcc" -std=c99 -O2 -Wno-parentheses-equality -I"$root/include" -I"$work/web-c" "$@" -sEXIT_RUNTIME=1 \
                -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,node "$work/web-c"/*.c -o "$work/web/$name.js"
            $run "$work/web/$name.js" > "$work/web/$name.out"
        fi
        sed "s/^/wasm $name: /" "$work/web/$name.out"
    }
    wasm_build simd128 -msimd128
    expect_levels "$work/web/simd128.out" scalar simd128
    grep -qx 'best: simd128' "$work/web/simd128.out"
    wasm_build baseline
    grep -qx 'levels: scalar' "$work/web/baseline.out"
    grep -qx 'best: scalar' "$work/web/baseline.out"
    wasm_build portable -msimd128 -DZIRAN_NO_ASM
    grep -qx 'levels: scalar' "$work/web/portable.out"
    grep -qx 'best: scalar' "$work/web/portable.out"
elif test -n "$run$wasi"; then
    echo "lanczos-isa: WASM_RUN and WASI_RUN need emcc; none at $emcc" >&2
    exit 1
else
    echo "WebAssembly skipped; set WASM_RUN or WASI_RUN to run it"
fi
