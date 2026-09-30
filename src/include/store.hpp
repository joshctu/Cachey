#pragma once

#include "hash.hpp"
#include "hint_manager.hpp"
#include "istore_observer.hpp"
#include "manifest.hpp"
#include "seg_id_monitor.hpp"
#include "seg_map_monitor.hpp"
#include "segment.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

struct ProjectConfig {
  std::filesystem::path root_dir = DEFAULT_ROOT;
  int compaction_threshold = 3;
  uint32_t max_segment_size = 256;
};

// Here is the shape of the init return. This is where metadata needed by
// parents should be written on startup

struct InitResult {
  uint32_t entry_count = 0;

  // False by default so we can try different boot methods until we get a
  // successful boot
  bool success = false;
};

// Making this a struct for now with the thought that there
// might be more information to return from this function in the future.
//
// This is a candidate for removal in favor of a single value return
struct StoreSetResult {
  bool unique_write_completed = true;
};

class KV_Store {

private:
  Hash_Table hash_table;
  ManifestManager manifest;
  std::atomic<uint32_t> next_segment_id{1};
  // Handle current segment map concurrency
  SegmentMapMonitor segment_map_monitor;
  // Handle current segment concurrency
  SegmentIDMonitor segment_id_monitor;
  // Register an observer to handle store events in other parts of program
  IStoreObserver *obs = nullptr;
  int compaction_threshold;
  std::filesystem::path root_dir;
  uint32_t max_segment_size;
  std::mutex manifest_mtx;
  uint32_t active_id;
  std::condition_variable cv;
  std::thread compaction_thread;
  std::atomic<bool> compaction_pending{false};
  std::atomic<bool> running{true};
  std::mutex compact_mtx;
  std::shared_mutex store_mtx;

public:
  KV_Store(ProjectConfig &config);

private:
  void setActiveSegment(const uint32_t segment_id);
  uint32_t generateSegmentId();
  uint32_t readSegmentId(const std::filesystem::path &file);
  InitResult bootFromWAL();
  InitResult bootFromHints(HintManager &hintManager);
  uint32_t generateNewSegment();
  void compact();
  void destroySegment(Segment *segment);
  void compactionLifecycle();

public:
  InitResult init();
  void registerObserver(IStoreObserver *new_obs);
  void encodeIndexAndSet(const std::string &key, const uint32_t segment_id,
                         const uint32_t file_offset);
  bool isLatestRecord(const std::string &key, const uint32_t segment_id,
                      const uint32_t file_offset);
  long getSegmentCount();
  StoreSetResult set(const std::string &key, const std::string &value);
  std::optional<const std::string> get(const std::string &key);
  bool remove(const std::string &key);
  ~KV_Store();
};
