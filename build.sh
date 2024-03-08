#!/bin/sh

set -e

# Generate build files
cmake -S . -G "Unix Makefiles" -B build/Debug -D CMAKE_BUILD_TYPE=Debug

# Compile the project
# cmake --build build/Debug
cmake --build build/Debug && ./bin/engine
