#!/bin/sh
# std/vorbis_linux against libvorbisfile, ffmpeg's libvorbis streams and the
# Xiph headers' record layout; see tests/vorbis_cases.zi.
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/vorbis_cases.zi" -- "$ziran"
