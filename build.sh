#!/bin/sh
set -e

# Generate build files
cmake -S . -G "Unix Makefiles" -B build/Debug -D CMAKE_BUILD_TYPE=Debug

# Compile and run
cmake --build build/Debug --target all && ./bin/engine
