#include "store.hpp"
#include <benchmark/benchmark.h>
#include <filesystem>

static void BM_readThouroughput_4MB(benchmark::State &state) {
  ProjectConfig benchConfig;
  benchConfig.root_dir = std::filesystem::path(DEFAULT_ROOT "/benchmarks");
  benchConfig.max_segment_size = 4 * 1024;

  KV_Store benchStore(benchConfig);
  benchStore.init();

  for (auto i = 0; i < 10000; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(i);
    benchStore.set(key, value);
  }

  for (auto _ : state) {
    for (auto i = 0; i < 10000; ++i) {
      std::string key = "key" + std::to_string(i);
      benchStore.get(key);
    }
  }

  std::filesystem::remove_all(benchConfig.root_dir / "database");
}

static void BM_readThouroughput_50MB(benchmark::State &state) {
  ProjectConfig benchConfig;
  benchConfig.root_dir = std::filesystem::path(DEFAULT_ROOT "/benchmarks");
  benchConfig.max_segment_size = 50 * 1024;

  KV_Store benchStore(benchConfig);
  benchStore.init();

  for (auto i = 0; i < 10000; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(i);
    benchStore.set(key, value);
  }

  for (auto _ : state) {
    for (auto i = 0; i < 10000; ++i) {
      std::string key = "key" + std::to_string(i);
      benchStore.get(key);
    }
  }

  std::filesystem::remove_all(benchConfig.root_dir / "database");
}

static void BM_readThouroughput_100MB(benchmark::State &state) {
  ProjectConfig benchConfig;
  benchConfig.root_dir = std::filesystem::path(DEFAULT_ROOT "/benchmarks");
  benchConfig.max_segment_size = 100 * 1024;

  KV_Store benchStore(benchConfig);
  benchStore.init();

  for (auto i = 0; i < 10000; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(i);
    benchStore.set(key, value);
  }

  for (auto _ : state) {
    for (auto i = 0; i < 10000; ++i) {
      std::string key = "key" + std::to_string(i);
      benchStore.get(key);
    }
  }

  std::filesystem::remove_all(benchConfig.root_dir / "database");
}

static void BM_readThouroughput_256MB(benchmark::State &state) {
  ProjectConfig benchConfig;
  benchConfig.root_dir = std::filesystem::path(DEFAULT_ROOT "/benchmarks");
  benchConfig.max_segment_size = 256 * 1024;

  KV_Store benchStore(benchConfig);
  benchStore.init();

  for (auto i = 0; i < 10000; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(i);
    benchStore.set(key, value);
  }

  for (auto _ : state) {
    for (auto i = 0; i < 10000; ++i) {
      std::string key = "key" + std::to_string(i);
      benchStore.get(key);
    }
  }

  std::filesystem::remove_all(benchConfig.root_dir / "database");
}

BENCHMARK(BM_readThouroughput_4MB);
BENCHMARK(BM_readThouroughput_50MB);
BENCHMARK(BM_readThouroughput_100MB);
BENCHMARK(BM_readThouroughput_256MB);
