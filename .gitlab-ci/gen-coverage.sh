#!/bin/sh

set -eux -o pipefail

mkdir -p public
grcov _build --source-dir ./ --prefix-dir ../ --output-type cobertura --branch --ignore-not-existing -o public/coverage.xml
grcov _build --source-dir ./ --prefix-dir ../ --output-type html --branch --ignore-not-existing -o public/coverage

# Print "Coverage: 42.42" so .gitlab-ci.yml will pick it up with a regex
#
# We scrape this from the HTML report, not the JSON summary, because coverage.json
# uses no decimal places, just something like "42%".

grep -Eo 'abbr title.* %' public/coverage/index.html | head -n 1 | grep -Eo '[0-9.]+ %' | grep -Eo '[0-9.]+' | awk '{ print "Coverage:", $1 }'
