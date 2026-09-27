#!/bin/bash

EXPORT_FILE=$1

cd build && ./benchmarks --benchmark_out="../benchmarks/bench_results/$EXPORT_FILE"

