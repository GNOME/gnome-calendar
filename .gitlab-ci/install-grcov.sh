#!/bin/bash

set -eux -o pipefail

wget https://github.com/mozilla/grcov/releases/download/v0.10.0/grcov-x86_64-unknown-linux-gnu.tar.bz2
tar xvf grcov-x86_64-unknown-linux-gnu.tar.bz2
mkdir -p /usr/local/bin
mv grcov /usr/local/bin/grcov

