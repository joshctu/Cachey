#include "seg_id_monitor.hpp"
#include <mutex>
#include <vector>

SegmentIDMonitor::SegmentIDMonitor() {};

void SegmentIDMonitor::push_back(uint32_t new_id) {
  std::unique_lock<std::shared_mutex> lock(mtx);

  current_segment_ids.insert(new_id);
}

void SegmentIDMonitor::erase(uint32_t new_id) {

  std::unique_lock<std::shared_mutex> lock(mtx);

  auto it = current_segment_ids.find(new_id);

  if (it != current_segment_ids.end())
    current_segment_ids.erase(new_id);
}

int SegmentIDMonitor::size() {
  std::shared_lock<std::shared_mutex> lock(mtx);
  return current_segment_ids.size();
}

void SegmentIDMonitor::clear() {
  std::unique_lock<std::shared_mutex> lock(mtx);
  current_segment_ids.clear();
}

std::vector<uint32_t> SegmentIDMonitor::get_snapshot() {
  std::shared_lock<std::shared_mutex> lock(mtx);
  std::vector<uint32_t> v(current_segment_ids.begin(),
                          current_segment_ids.end());

  return v;
}
