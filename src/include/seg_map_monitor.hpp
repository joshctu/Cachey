#pragma once

#include "segment.hpp"
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>

class SegmentMapMonitor {

private:
  std::unordered_map<uint32_t, std::unique_ptr<Segment>> segment_map;
  std::shared_mutex mtx;

public:
  SegmentMapMonitor();
  void insert(uint32_t segment_id, uint32_t max_segment_size,
              std::filesystem::path segment_path);
  Segment *access(uint32_t segment_id);
  void clear();
  int size();
  void erase(uint32_t segment_id);
};
