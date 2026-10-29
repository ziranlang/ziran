#!/usr/bin/env python3
"""std/vorbis_linux decodes real Ogg Vorbis files exactly as libvorbis does,
and std/vorbis probes them without decoding.

Streams come from ffmpeg's libvorbis encoder and are compared with ffmpeg's
own decode of the same file; damaged, chained, empty and over-long streams
must be refused. With the Xiph headers available (system or VORBIS_INCLUDE),
the mirrored records are also compared with the C compiler's layout.
"""
import array
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
ZIRAN = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build/bin/ziran'
CC = os.environ.get('CC', 'cc')
RECORDS = ('OggSyncState', 'OggStreamState', 'OggPackBuffer', 'VorbisInfo', 'VorbisDspState',
           'VorbisBlock', 'VorbisCallbacks', 'VorbisFile')
C_TYPES = ('ogg_sync_state', 'ogg_stream_state', 'oggpack_buffer', 'vorbis_info', 'vorbis_dsp_state',
           'vorbis_block', 'ov_callbacks', 'OggVorbis_File')

CLIENT = r'''
#import "vorbis_linux"
#import "vorbis"
#import "file"
#import "number_text"
#import "text_tools"
#import "byte_text_linux"

Append :: (out: *Vec(u8), value: s64) {
    digits: [24]u8
    text := FormatInteger(value, digits[:])
    for at: 0..text.count - 1 { VecPush(<<out, text[at]) }
    VecPush(<<out, cast(u8)10)
}

#program_export
main :: (argc: s32, argv: **u8) -> s32 {
    report: Vec(u8)
    if TextFromCString(argv[1]) == "sizes" {
        Append(*report, size_of(OggSyncState)); Append(*report, size_of(OggStreamState))
        Append(*report, size_of(OggPackBuffer)); Append(*report, size_of(VorbisInfo))
        Append(*report, size_of(VorbisDspState)); Append(*report, size_of(VorbisBlock))
        Append(*report, size_of(VorbisCallbacks)); Append(*report, size_of(VorbisFile))
        return ifx WriteEntireFile(TextFromCString(argv[2]), BuilderFinish(report)) then 0 else 3
    }
    input: Vec(u8)
    if TextFromCString(argv[1]) == "probe" {
        if !ReadFileInto(TextFromCString(argv[2]), *input) { return 3 }
        declared: VorbisAudio
        if !VorbisProbe(VecSlice(input, 0, input.count), *declared) {
            return ifx declared.frames == 0 && declared.channels == 0 then 2 else 4
        }
        Append(*report, declared.channels); Append(*report, declared.rate); Append(*report, declared.frames)
        return ifx WriteEntireFile(TextFromCString(argv[3]), BuilderFinish(report)) then 0 else 3
    }
    if !ReadFileInto(TextFromCString(argv[1]), *input) { return 3 }
    limit, ok := TextInteger(TextFromCString(argv[3]))
    if !ok { return 3 }
    pcm: Vec(u8)
    audio: VorbisAudio
    if !VorbisDecode(VecSlice(input, 0, input.count), *pcm, limit, *audio) {
        if pcm.count != 0 || audio.frames != 0 { return 4 }
        return 2
    }
    if pcm.count != audio.frames * audio.channels * 2 { return 4 }
    Append(*report, audio.channels); Append(*report, audio.rate); Append(*report, audio.frames)
    if !WriteEntireFile(TextFromCString(argv[4]), BuilderFinish(report)) { return 3 }
    return ifx WriteEntireFile(TextFromCString(argv[2]), BuilderFinish(pcm)) then 0 else 3
}
'''

PROBE = r'''
#include <stdio.h>
#include <vorbis/vorbisfile.h>
int main(void) {
    printf("%zu\n%zu\n%zu\n%zu\n%zu\n%zu\n%zu\n%zu\n", sizeof(ogg_sync_state), sizeof(ogg_stream_state),
           sizeof(oggpack_buffer), sizeof(vorbis_info), sizeof(vorbis_dsp_state), sizeof(vorbis_block),
           sizeof(ov_callbacks), sizeof(OggVorbis_File));
    return 0;
}
'''


def tone(path, seconds, rate=22050, channels=1, frequency=440.0):
    samples = array.array('h')
    for index in range(int(seconds * rate)):
        value = int(12000 * math.sin(2 * math.pi * frequency * index / rate))
        samples.extend([value] * channels if channels == 1 else [value, -value // 2][:channels])
    raw = path.with_suffix('.raw')
    raw.write_bytes(samples.tobytes())
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 's16le', '-ar', str(rate), '-ac', str(channels),
                    '-i', str(raw), '-c:a', 'libvorbis', '-q:a', '4', '-fflags', '+bitexact',
                    '-flags:a', '+bitexact', '-map_metadata', '-1', str(path)], check=True)
    return samples


def reference(path):
    result = subprocess.run(['ffmpeg', '-v', 'error', '-c:a', 'libvorbis', '-i', str(path), '-f', 's16le',
                             '-acodec', 'pcm_s16le', '-'], check=True, capture_output=True)
    return array.array('h', result.stdout)


with tempfile.TemporaryDirectory(prefix='ziran-vorbis-') as temporary:
    work = Path(temporary)
    if not shutil.which('ffmpeg'):
        raise SystemExit('vorbis test needs ffmpeg with libvorbis to make its streams')
    (work / 'client.zi').write_text(CLIENT)
    subprocess.run([str(ZIRAN), 'build', '--target=c', '--entry', 'client:main', '--root', str(work),
                    '--module-path', str(ROOT / 'std'), '-o', str(work / 'c'), str(work / 'client.zi')], check=True)
    client = work / 'client'
    subprocess.run([CC, '-std=c11', '-O1', '-I' + str(ROOT / 'include'), '-I' + str(work / 'c'),
                    *map(str, (work / 'c').glob('*.c')), '-l:libvorbisfile.so.3', '-lm', '-o', str(client)],
                   check=True)

    def probe(path, valid=True):
        result = subprocess.run([str(client), 'probe', str(path), str(work / 'probe.info')])
        assert result.returncode == (0 if valid else 2), (path.name, 'probe', result.returncode)
        return tuple(map(int, (work / 'probe.info').read_text().split())) if valid else None

    def decode(path, limit, valid=True):
        pcm, info = work / 'out.pcm', work / 'out.info'
        for stale in (pcm, info):
            stale.unlink(missing_ok=True)
        result = subprocess.run([str(client), str(path), str(pcm), str(limit), str(info)])
        assert result.returncode == (0 if valid else 2), (path.name, limit, result.returncode)
        if not valid:
            return None
        channels, rate, frames = map(int, info.read_text().split())
        samples = array.array('h', pcm.read_bytes())
        assert sys.byteorder == 'little'
        assert len(samples) == frames * channels, path.name
        return channels, rate, frames, samples

    # ABI: every mirrored record is the size the official headers give it.
    subprocess.run([str(client), 'sizes', str(work / 'sizes')], check=True)
    sizes = list(map(int, (work / 'sizes').read_text().split()))
    include = os.environ.get('VORBIS_INCLUDE')
    probe_flags = ['-I' + include] if include else []
    (work / 'probe.c').write_text(PROBE)
    built = subprocess.run([CC, *probe_flags, str(work / 'probe.c'), '-o', str(work / 'probe')], capture_output=True)
    if built.returncode == 0:
        expected = list(map(int, subprocess.check_output([str(work / 'probe')], text=True).split()))
        assert sizes == expected, dict(zip(C_TYPES, zip(sizes, expected)))
        print('Vorbis records match the Xiph headers: ' + ', '.join(f'{name} {size}' for name, size in zip(RECORDS, sizes)))
    else:
        print('Xiph headers not installed (set VORBIS_INCLUDE); record sizes checked by the decode guard only')

    # Real streams decode to ffmpeg's samples at the right rate and length.
    cases = [('speech', 1.5, 22050, 1), ('short', 0.05, 22050, 1), ('stereo', 0.7, 44100, 2)]
    for name, seconds, rate, channels in cases:
        path = work / (name + '.ogg')
        source = tone(path, seconds, rate, channels)
        channels_out, rate_out, frames, samples = decode(path, rate * 60)
        assert (channels_out, rate_out) == (channels, rate), name
        assert probe(path) == (channels, rate, frames), name
        assert frames == len(source) // channels, (name, frames, len(source))
        # ffmpeg trims very short streams by up to a block; every sample it
        # does give must match, at the start or the end of the stream.
        expected = reference(path)
        assert 0 < len(expected) <= len(samples) and len(samples) - len(expected) <= 2048 * channels, name
        assert min(max(abs(a - b) for a, b in zip(samples[offset:], expected))
                   for offset in {0, len(samples) - len(expected)}) <= 1, name
        # Lossy but faithful: the tone survives encoding.
        middle = slice(len(source) // 4, 3 * len(source) // 4)
        error = sum((a - b) ** 2 for a, b in zip(samples[middle], source[middle]))
        power = sum(a * a for a in source[middle])
        assert error < power / 100, (name, error, power)
        # Exact frame limit: the length itself passes, one less is refused.
        decode(path, frames)
        decode(path, frames - 1, False)

    # Damaged, foreign, chained and empty input is refused, never crashes.
    original = (work / 'speech.ogg').read_bytes()
    middle = len(original) // 2
    flipped = bytearray(original)
    flipped[middle] ^= 0xFF
    short_bytes = (work / 'short.ogg').read_bytes()
    for label, data in [('empty', b''), ('text', b'not an ogg stream' * 40), ('header only', original[:200]),
                        ('truncated', original[:middle]), ('flipped', bytes(flipped)),
                        ('chained', original + short_bytes), ('trailing', original + b'\x00' * 16),
                        ('wav', b'RIFF' + bytes(4000))]:
        path = work / 'bad.ogg'
        path.write_bytes(data)
        decode(path, 22050 * 60, False)
        if label != 'flipped':
            probe(path, False)
    # The identification header lies in the first page; 28 bytes of page header.
    for offset, value in ((28, ord('V')), (28 + 29, 0), (28 + 7, 1)):
        damaged = bytearray(original)
        damaged[offset] = value
        (work / 'bad.ogg').write_bytes(bytes(damaged))
        probe(work / 'bad.ogg', False)
        decode(work / 'bad.ogg', 22050 * 60, False)
    decode(work / 'speech.ogg', 0, False)
    decode(work / 'speech.ogg', -5, False)
    print('Native Vorbis: probe, ffmpeg-matching decode, rates, channels, exact frame bounds and malformed input passed')
