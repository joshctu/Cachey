#pragma once

#include <cstdint>

struct CompactionResult {
  int segments_merged = 0;
  uint16_t records_removed = 0;
};

class IStoreObserver {
public:
  void virtual onCompactionComplete(CompactionResult &result) = 0;
  virtual ~IStoreObserver() = default;
};
