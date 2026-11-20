#!/usr/bin/env bash
# Baut und startet den JUCE-freien Engine-Test. Benoetigt nur g++/clang++ (C++17).
set -e
cd "$(dirname "$0")"
mkdir -p /tmp/nova
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter engine_test.cpp -o /tmp/nova/engine_test
/tmp/nova/engine_test || exit 1
${CXX:-g++} -std=c++17 -O2 note_parser_test.cpp -o /tmp/nova/note_parser_test
/tmp/nova/note_parser_test
