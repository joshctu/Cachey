#!/bin/bash

cmake --build build

rm -rf ./tests/database
cd build && ./tests -v high 
