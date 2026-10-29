#!/bin/sh
# std/vorbis_linux against libvorbisfile, ffmpeg's libvorbis streams and the
# Xiph headers' record layout; see tests/vorbis_linux_test.py.
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$repo/tests/vorbis_linux_test.py" "$ziran"
