#!/bin/sh
set -eu

# Usage: ci_packages.sh [--recommends] PACKAGE...
#
# Hosted runners include third-party package feeds, and their Ubuntu feed is
# an Azure mirror list that sometimes stalls for many minutes. These checks
# only need Ubuntu packages, so apt reads them from the canonical archive
# alone, with time limits. --recommends also installs recommended packages.
recommends=--no-install-recommends
if [ "${1:-}" = --recommends ]; then
    recommends=--install-recommends
    shift
fi

if [ ! -r /etc/os-release ]; then
    echo 'Ubuntu package sources are unavailable' >&2
    exit 1
fi
. /etc/os-release
if [ "${ID:-}" != ubuntu ] || [ -z "${VERSION_CODENAME:-}" ]; then
    echo 'Ubuntu package sources are unavailable' >&2
    exit 1
fi
archive=http://archive.ubuntu.com/ubuntu
if [ "$(dpkg --print-architecture)" != amd64 ]; then
    archive=http://ports.ubuntu.com/ubuntu-ports
fi

# apt reads a .sources file in the deb822 format.
sources=$(mktemp --suffix=.sources)
trap 'rm -f "$sources"' EXIT
cat > "$sources" <<EOF
Types: deb
URIs: $archive
Suites: $VERSION_CODENAME $VERSION_CODENAME-updates $VERSION_CODENAME-backports $VERSION_CODENAME-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
chmod 644 "$sources"

sudo apt-get -o Acquire::Retries=3 \
    -o Acquire::http::Timeout=20 -o Acquire::https::Timeout=20 \
    -o Dir::Etc::sourcelist="$sources" -o Dir::Etc::sourceparts=- \
    -o APT::Get::List-Cleanup=0 -o APT::Update::Error-Mode=any update
sudo apt-get -o Acquire::Retries=3 \
    -o Acquire::http::Timeout=20 -o Acquire::https::Timeout=20 \
    -o Dir::Etc::sourcelist="$sources" -o Dir::Etc::sourceparts=- \
    install -y "$recommends" "$@"
