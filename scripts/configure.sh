#!/bin/bash

mkdir build
cmake -S . -B build --preset default  \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DQt6_DIR=/home/lean/Qt/6.11.1/gcc_64/lib/cmake/Qt6
