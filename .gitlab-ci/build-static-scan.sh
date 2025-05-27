#!/bin/sh

set -eux -o pipefail

meson setup --buildtype=debug  --prefix /usr _scan_build .
ninja -C _scan_build scan-build
mkdir -p public/
mv _scan_build/meson-logs/scanbuild public/scanbuild
