#!/bin/bash

EXPORT_FILE=$1

cmake --build build || exit 1
rm -rf benchmarks/database
cd build

if [[ -n "$EXPORT_FILE" ]]; then
	./benchmarks --benchmark_out="../benchmarks/bench_results/$EXPORT_FILE"
else
	./benchmarks
fi


