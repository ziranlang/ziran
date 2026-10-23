#!/bin/sh
set -eu
ziran=$1
script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
backend=rust
backend_extra() { :; }
. "$script_dir/lib/native_backend.sh"
