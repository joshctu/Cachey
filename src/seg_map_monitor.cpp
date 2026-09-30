#include "seg_map_monitor.hpp"
#include "segment.hpp"
#include <mutex>

SegmentMapMonitor::SegmentMapMonitor() {};

void SegmentMapMonitor::insert(uint32_t segment_id, uint32_t max_segment_size,
                               std::filesystem::path segment_path) {

  std::unique_lock<std::shared_mutex> lock(mtx);

  segment_map[segment_id] =
      std::make_unique<Segment>(segment_id, max_segment_size, segment_path);
}

Segment *SegmentMapMonitor::access(uint32_t segment_id) {
  std::shared_lock<std::shared_mutex> lock(mtx);
  auto it = segment_map.find(segment_id);
  return it != segment_map.end() ? it->second.get() : nullptr;
}

void SegmentMapMonitor::clear() {
  std::unique_lock<std::shared_mutex> lock(mtx);
  segment_map.clear();
}

int SegmentMapMonitor::size() {
  std::shared_lock<std::shared_mutex> lock(mtx);
  return segment_map.size();
}

void SegmentMapMonitor::erase(uint32_t segment_id) {
  std::unique_lock<std::shared_mutex> lock(mtx);

  segment_map.erase(segment_id);
}
