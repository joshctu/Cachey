#!/bin/bash

mkdir build
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DQt6_DIR=/home/lean/Qt/6.11.1/gcc_64/lib/cmake/Qt6
