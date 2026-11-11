# Independent image reference data

These inputs and expected RGBA bytes were generated with Pillow 12.3.0,
using the original image test's seed 7 and pixel formulas. No Ziran image
implementation was used to generate expected values. `manifest.json` records
every file's size and SHA-256.

Seven PNG modes cover RGBA, RGB, grayscale, grayscale with alpha, palettes
with a transparent entry, monochrome, and 16-bit grayscale. As in the original
test, 16-bit grayscale expects the high byte of each sample. RGB and RGBA
inputs have nearest, box and Lanczos reference results at 16×16, 100×61 and
20×50.

The Ziran test always runs these comparisons. It checks decoding against the
frozen RGBA data, then uses FFmpeg's independent PNG decoder to read Ziran's
resized PNGs before comparing every output byte with Pillow's reference.
Pillow is not a test dependency.
