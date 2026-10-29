#!/bin/sh
# Checks std/image, std/png_linux, std/zlib_linux and std/jpeg: PNG round
# trips in every written format, Pillow's compositing arithmetic, resizing,
# rotation, palettes and JPEG structure. With Python's Pillow installed it
# also compares decoding and resizing with Pillow's own pictures.
set -eu

ziran=$1
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/std_image_linux.zi" <<'ZI'
#import "image"
#import "png_linux"
#import "jpeg"
#import "zlib_linux"
#import "file"
#import "byte_text_linux"
#import "text_tools"

Pixel :: (image: *Image, x: s32, y: s32, r: u8, g: u8, b: u8, a: u8) {
    at := (cast(s64)y * image.width + x) * 4
    image.pixels[at] = r; image.pixels[at + 1] = g; image.pixels[at + 2] = b; image.pixels[at + 3] = a
}

Is :: (image: *Image, x: s32, y: s32, r: u8, g: u8, b: u8, a: u8) -> bool {
    at := (cast(s64)y * image.width + x) * 4
    return image.pixels[at] == r && image.pixels[at + 1] == g && image.pixels[at + 2] == b && image.pixels[at + 3] == a
}

Same :: (a: *Image, b: *Image) -> bool {
    if a.width != b.width || a.height != b.height || a.pixels.count != b.pixels.count { return false }
    for index: 0..a.pixels.count - 1 { if a.pixels[index] != b.pixels[index] { return false } }
    return true
}

Number :: (text: string) -> s32 { value, ok := TextInteger(text); return cast(s32)value }

// Tool mode for the Pillow comparison: decode IN to raw RGBA, or resize it.
Tool :: (argc: s32, argv: **u8) -> s32 {
    command := TextFromCString(argv[1])
    image := ReadPNGFile(TextFromCString(argv[2]))
    if !ImageValid(*image) { return 2 }
    if command == "decode" {
        raw: Vec(u8)
        for index: 0..image.pixels.count - 1 { VecPush(raw, image.pixels[index]) }
        return ifx WriteEntireFile(TextFromCString(argv[3]), BuilderFinish(raw)) then 0 else 3
    }
    mode := TextFromCString(argv[3])
    width := Number(TextFromCString(argv[4]))
    height := Number(TextFromCString(argv[5]))
    out := TextFromCString(argv[6])
    if mode == "nearest" { result := ResizeNearest(*image, width, height); return ifx WritePNGFile(out, *result) then 0 else 4 }
    if mode == "box" { result := ResizeBox(*image, width, height); return ifx WritePNGFile(out, *result) then 0 else 4 }
    result := ResizeLanczos(*image, width, height)
    return ifx WritePNGFile(out, *result) then 0 else 4
}

#program_export
main :: (argc: s32, argv: **u8) -> s32 {
    if argc > 1 { return Tool(argc, argv) }
    // PNG round trips: RGBA, RGB and palette.
    picture := NewImage(3, 2, 10, 20, 30, 255)
    Pixel(*picture, 1, 0, 200, 100, 50, 128)
    Pixel(*picture, 2, 1, 0, 0, 0, 0)
    bytes: Vec(u8)
    if !WritePNG(*picture, *bytes) { return 1 }
    again := ReadPNG(VecSlice(bytes, 0, bytes.count))
    if !Same(*picture, *again) { return 2 }
    opaque: Vec(u8)
    if !WritePNG(*picture, *opaque, false) { return 3 }
    flat := ReadPNG(VecSlice(opaque, 0, opaque.count))
    if !Is(*flat, 1, 0, 200, 100, 50, 255) || !Is(*flat, 2, 1, 0, 0, 0, 255) { return 4 }
    indexed := QuantizeImage(*picture, 192, true)
    if indexed.palette.count != 9 || indexed.indices.count != 6 { return 5 }
    palette_bytes: Vec(u8)
    if !WriteIndexedPNG(*indexed, *palette_bytes) { return 6 }
    restored := ReadPNG(VecSlice(palette_bytes, 0, palette_bytes.count))
    if !Is(*restored, 0, 0, 10, 20, 30, 255) || !Is(*restored, 1, 0, 200, 100, 50, 255) || !Is(*restored, 2, 1, 0, 0, 0, 255) { return 7 }
    // Damaged files are refused.
    bytes[20] = bytes[20] ^ 1
    broken := ReadPNG(VecSlice(bytes, 0, 12))
    if ImageValid(*broken) { return 8 }

    // Pillow's Image.alpha_composite arithmetic.
    base := NewImage(2, 1, 10, 20, 30, 255)
    Pixel(*base, 1, 0, 10, 20, 30, 100)
    over := NewImage(2, 1, 200, 100, 50, 128)
    AlphaComposite(*base, *over, 0, 0)
    if !Is(*base, 0, 0, 105, 60, 40, 255) || !Is(*base, 1, 0, 147, 78, 44, 178) { return 9 }
    // Only part of the source, as alpha_composite's source box.
    atlas := NewImage(4, 1, 1, 2, 3, 255)
    Pixel(*atlas, 2, 0, 200, 100, 50, 255)
    target := NewImage(3, 1)
    AlphaComposite(*target, *atlas, 1, 0, 2, 0, 1, 1)
    if !Is(*target, 1, 0, 200, 100, 50, 255) || !Is(*target, 0, 0, 0, 0, 0, 0) || !Is(*target, 2, 0, 0, 0, 0, 0) { return 17 }
    // Paste, rotation and nearest resizing.
    canvas := NewImage(4, 3)
    PasteImage(*canvas, *picture, 2, 2)
    if !Is(*canvas, 2, 2, 10, 20, 30, 255) || !Is(*canvas, 3, 2, 200, 100, 50, 128) || !Is(*canvas, 0, 0, 0, 0, 0, 0) { return 10 }
    turned := RotateLeft(*picture)
    if turned.width != 2 || turned.height != 3 || !Is(*turned, 0, 1, 200, 100, 50, 128) || !Is(*turned, 1, 0, 0, 0, 0, 0) { return 11 }
    wide := ResizeNearest(*picture, 6, 4)
    if !Is(*wide, 2, 0, 200, 100, 50, 128) || !Is(*wide, 3, 1, 200, 100, 50, 128) || !Is(*wide, 5, 3, 0, 0, 0, 0) { return 12 }
    // Filtered resizing keeps a flat colour flat.
    flat_colour := NewImage(9, 7, 40, 80, 120, 255)
    smaller := ResizeLanczos(*flat_colour, 4, 3)
    boxed := ResizeBox(*flat_colour, 5, 2)
    if !Is(*smaller, 3, 2, 40, 80, 120, 255) || !Is(*boxed, 4, 1, 40, 80, 120, 255) { return 13 }

    // zlib streams, and a JPEG's structure.
    text := "zlib zlib zlib zlib"
    plain: Vec(u8)
    for text { VecPush(plain, it) }
    packed: Vec(u8)
    unpacked: Vec(u8)
    if !ZlibCompress(VecSlice(plain, 0, plain.count), *packed, 9, 31) ||
        !ZlibDecompress(VecSlice(packed, 0, packed.count), *unpacked, 64, 47) || BuilderFinish(unpacked) != text { return 14 }
    jpeg: Vec(u8)
    large := NewImage(37, 21, 90, 160, 200, 255)
    if !WriteJPEG(*large, *jpeg, 82) || jpeg.count < 200 { return 15 }
    if jpeg[0] != 255 || jpeg[1] != 216 || jpeg[jpeg.count - 2] != 255 || jpeg[jpeg.count - 1] != 217 { return 16 }
    return 0
}
ZI

"$ziran" build --target=c --entry std_image_linux:main \
    --root "$work" --module-path "$root/std" \
    -o "$work/c" "$work/std_image_linux.zi"
"${CC:-cc}" -std=c99 -I"$root/include" -I"$work/c" \
    "$work/c"/*.c -lz -lm -o "$work/program"
(cd "$work" && ./program)

# Pillow's own pictures, when Pillow is installed.
if python3 -c 'import PIL' 2>/dev/null; then
    python3 - "$work" <<'PY'
import os, random, struct, subprocess, sys
from PIL import Image
work = sys.argv[1]
program = os.path.join(work, 'program')
random.seed(7)
def picture(mode, size):
    image = Image.new(mode, size)
    for y in range(size[1]):
        for x in range(size[0]):
            if mode == 'RGBA': value = (random.randrange(256), (x * 7) % 256, (y * 13) % 256, random.choice((0, 40, 128, 255)))
            elif mode == 'RGB': value = (random.randrange(256), (x * 7) % 256, (y * 13) % 256)
            elif mode == 'LA': value = ((x * 3) % 256, (y * 5) % 256)
            elif mode == 'I;16': value = (x * 997 + y * 31) % 65536
            elif mode == 'P': value = (x + y) % 40
            elif mode == '1': value = (x ^ y) & 1
            else: value = (x * y) % 256
            image.putpixel((x, y), value)
    if mode == 'P':
        image.putpalette([random.randrange(256) for _ in range(120)])
    return image
for index, (mode, size) in enumerate((('RGBA', (37, 23)), ('RGB', (31, 17)), ('L', (19, 11)), ('LA', (13, 9)),
                                      ('P', (21, 14)), ('1', (15, 7)), ('I;16', (9, 6)))):
    path = os.path.join(work, f'in{index}.png')
    source = picture(mode, size)
    source.save(path, **({'transparency': 3} if mode == 'P' else {}))
    raw = os.path.join(work, 'raw')
    subprocess.run([program, 'decode', path, raw], check=True)
    expected = Image.open(path)
    if mode == 'I;16':
        values = struct.unpack('<%dH' % (size[0] * size[1]), source.tobytes('raw', 'I;16'))
        expected = Image.frombytes('L', size, bytes(value >> 8 for value in values)).convert('RGBA')
    assert open(raw, 'rb').read() == expected.convert('RGBA').tobytes(), f'{mode} decodes as Pillow decodes it'
    if mode in ('RGBA', 'RGB'):
        for name, method in (('nearest', Image.Resampling.NEAREST), ('box', Image.Resampling.BOX), ('lanczos', Image.Resampling.LANCZOS)):
            for width, height in ((16, 16), (100, 61), (20, 50)):
                out = os.path.join(work, 'resized.png')
                subprocess.run([program, 'resize', path, name, str(width), str(height), out], check=True)
                want = Image.open(path).convert('RGBA').resize((width, height), method).tobytes()
                assert Image.open(out).convert('RGBA').tobytes() == want, f'{mode} {name} {width}x{height}'
PY
fi
