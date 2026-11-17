# Ogg Vorbis test fixtures

Generated on 2026-11-16 with FFmpeg 7.1.5 (Debian 7.1.5-0+deb13u1) and its
libvorbis encoder (Debian libvorbis 1.3.7). Each `NAME.wav` is FFmpeg's own
decode of `NAME.ogg` (PCM16, its native `vorbis` decoder), the independent
reference that `tests/vorbis_contracts.zi` compares sample values against.
`long.ogg` has no reference: its seeks are checked against this package's
own linear decode.

FFmpeg's Ogg demuxer counts the first packet's discarded half block, so its
decode does not end at the final granule position: it runs a few dozen
frames long (no end trim) or, for `comments.ogg`, 128 frames short. The
contracts compare the frames both decodes hold. The exact length is the
final granule position, which `ffprobe -show_entries stream=duration_ts`
reports and which the system libvorbisfile 1.3.7 (`ov_pcm_total`, and the
frame count `ov_read_float` returns) confirmed when these fixtures were
made; its samples matched FFmpeg's within 0.5 PCM16 steps.

The 0.3 s source is a 440 Hz left / 660 Hz right tone plus seeded noise, as
for the MP3 fixtures:

```sh
SRC='sine=f=440:d=0.3:sample_rate=44100[a];sine=f=660:d=0.3:sample_rate=44100[b];anoisesrc=d=0.3:a=0.05:r=44100:seed=7[n];[a][b]amerge=inputs=2[s];[s][n]amix=inputs=2:normalize=0,volume=0.8'
ff() { ffmpeg -hide_banner -loglevel error -y -bitexact "$@"; }
ff -filter_complex "$SRC" -ac 2 -ar 44100 -c:a libvorbis -q:a 4 stereo.ogg
ff -filter_complex "$SRC" -ac 1 -ar 44100 -c:a libvorbis -q:a 2 mono.ogg
ff -filter_complex "$SRC" -ac 2 -ar 22050 -c:a libvorbis -q:a 3 rate22.ogg
ff -filter_complex "$SRC" -ac 2 -ar 48000 -c:a libvorbis -q:a 5 rate48.ogg
ff -filter_complex "$SRC" -ac 1 -ar 32000 -c:a libvorbis -q:a 1 -metadata title="Ziran fixture" \
    -metadata artist="Ziran audio" -metadata comment="Vorbis comments, ünïcödé" comments.ogg
ff -f lavfi -i "sine=f=330:d=8:sample_rate=22050,volume=0.5" -f lavfi -i "anoisesrc=d=8:a=0.08:r=22050:seed=3" \
    -filter_complex amix=inputs=2:normalize=0 -ac 1 -ar 22050 -c:a libvorbis -q:a 0 long.ogg
for f in stereo mono rate22 rate48 comments; do ff -i "$f.ogg" -map_metadata -1 -c:a pcm_s16le "$f.wav"; done
```

| File | Stream | Granule frames | FFmpeg frames |
| --- | --- | --- | --- |
| `stereo.ogg` | 44.1 kHz stereo, `-q:a 4` | 13230 | 13312 |
| `mono.ogg` | 44.1 kHz mono, `-q:a 2` | 13230 | 13312 |
| `rate22.ogg` | 22.05 kHz stereo, `-q:a 3` | 6615 | 6656 |
| `rate48.ogg` | 48 kHz stereo, `-q:a 5` | 14400 | 14464 |
| `comments.ogg` | 32 kHz mono, `-q:a 1`; vendor `ffmpeg`, `encoder`, `title`, `artist` and a UTF-8 `DESCRIPTION` (FFmpeg's name for `comment`) | 9600 | 9472 |
| `long.ogg` | 22.05 kHz mono, `-q:a 0`, 8 s | 176400 | — |
