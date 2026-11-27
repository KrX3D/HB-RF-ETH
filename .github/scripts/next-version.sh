#!/bin/sh
# Prints the version for this build: version.txt if it is higher than the
# newest v* tag, otherwise the newest tag with the patch number + 1.
# Every merge to master gets its own release this way; bump minor/major by
# editing version.txt.
set -eu
base=$(tr -d ' \r\n' < version.txt)
latest=$(git tag -l 'v[0-9]*' | sed 's/^v//' | sort -V | tail -n1)
if [ -z "$latest" ] || { [ "$base" != "$latest" ] && [ "$(printf '%s\n%s\n' "$base" "$latest" | sort -V | tail -n1)" = "$base" ]; }; then
  echo "$base"
else
  echo "$latest" | awk -F. '{ printf "%d.%d.%d\n", $1, $2, $3 + 1 }'
fi
