#!/bin/sh
set -eu

# Hosted runners include third-party package feeds. These checks only need
# Ubuntu packages; refreshing unrelated feeds can exhaust the job budget.
sources=/etc/apt/sources.list.d/ubuntu.sources
if test ! -r "$sources"; then
    echo 'Expected Ubuntu 24.04 package sources are unavailable' >&2
    exit 1
fi

sudo apt-get -o Acquire::Retries=2 \
    -o Acquire::http::Timeout=20 -o Acquire::https::Timeout=20 \
    -o Dir::Etc::sourcelist="$sources" -o Dir::Etc::sourceparts=- \
    -o APT::Get::List-Cleanup=0 -o APT::Update::Error-Mode=any update
sudo apt-get -o Acquire::Retries=2 \
    -o Acquire::http::Timeout=20 -o Acquire::https::Timeout=20 \
    -o Dir::Etc::sourcelist="$sources" -o Dir::Etc::sourceparts=- \
    install -y --no-install-recommends "$@"
