#!/bin/sh
# Runs the host unit tests. Needs a C++ compiler and the ESP-IDF cJSON sources.
set -e
cd "$(dirname "$0")"
IDF="${IDF_PATH:-$HOME/.platformio/packages/framework-espidf}"
CJSON="$IDF/components/json/cJSON"
c++ -std=gnu++17 -Wall -I../../include -I"$CJSON" test_updatehelpers.cpp "$CJSON/cJSON.c" -o /tmp/test_updatehelpers
/tmp/test_updatehelpers release_upstream.json
