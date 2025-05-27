#!/bin/sh

set -eux -o pipefail

meson setup --buildtype=debug  --prefix /usr _scan_build .
ninja -C _scan_build scan-build
mkdir -p public/

# The scan-build command creates _scan_build/meson-logs/scanbuild/UUID which is ugly,
# so we copy it to a friendly name instead.
cp -r _scan_build/meson-logs/scanbuild/* public/scanbuild
