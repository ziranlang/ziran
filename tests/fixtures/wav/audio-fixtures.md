# ADPCM WAV test fixtures

Generated on 2026-11-17 with FFmpeg 7.1.5 (Debian 7.1.5-0+deb13u1).
Each mono (`-1`) or stereo (`-2`) input has 1555 PCM16 frames at 48000 Hz.
Sample `(frame, channel)` is `(frame * 7919 + channel * 17777) % 60001 - 30000`.
The abrupt changes exercise predictor adaptation, saturation and channel order.

The encoded files were made with:

```sh
ffmpeg -v error -y -threads 1 -i input-CHANNELS.wav -c:a adpcm_ima_wav -block_size 256 ima-CHANNELS.wav
ffmpeg -v error -y -threads 1 -i input-CHANNELS.wav -c:a adpcm_ms -block_size 256 ms-CHANNELS.wav
ffmpeg -v error -y -threads 1 -i ms-CHANNELS.wav -af atrim=end_sample=1555 -c:a pcm_s16le ms-CHANNELS-pcm.wav
```

`ima-CHANNELS-pcm.wav` is libsndfile 1.2.2's independent decode, read with
`sf_open(path, SFM_READ, &info)` and `sf_readf_short(file, samples, 1555)`,
then written as PCM16 WAV. Its IMA rounding follows the Microsoft reference:
add the separately shifted step contributions. FFmpeg 7.1.5's four-bit IMA
fast path multiplies before shifting, which gives slightly different samples;
it is therefore not the exact IMA reference. MS reference samples come from
FFmpeg. Both reference files stop at the 1555-frame `fact` count; compressed
blocks contain additional padding samples.

The retained Ziran contracts compare every sample exactly, play and seek both
formats through the ordinary mixer, and check fact trimming, optional fact,
short blocks, invalid headers, coefficients, bounds and preserved output on
failure. No external decoder is needed to run the tests.

Format references: Microsoft's [Multimedia Data Standards Update](https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/RIFFNEW.pdf)
and the independent [libsndfile IMA decoder](https://github.com/libsndfile/libsndfile/blob/master/src/ima_adpcm.c)
and [FFmpeg ADPCM decoder](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/adpcm.c).
