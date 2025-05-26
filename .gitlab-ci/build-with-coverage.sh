#!/bin/sh

set -eux -o pipefail

export CFLAGS="-coverage -ftest-coverage -fprofile-arcs"

meson setup --buildtype=debug --prefix /usr _build .
meson compile -C _build
meson test -C _build
