#pragma once

#include <cstdint>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

class SegmentIDMonitor {
private:
  std::unordered_set<uint32_t> current_segment_ids;
  std::shared_mutex mtx;

public:
  SegmentIDMonitor();
  int size();
  void push_back(uint32_t new_id);
  void erase(uint32_t new_id);
  void clear();
  std::vector<uint32_t> get_snapshot();
};
