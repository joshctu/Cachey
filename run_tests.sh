#!/bin/bash

cmake --build build
cd build && ./tests -v high 
