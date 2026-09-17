#pragma once

#include "hash.hpp"
#include "istore_observer.hpp"
#include "segment.hpp"
#include <atomic>
#include <optional>
#include <string>
#include <unordered_map>

constexpr int COMPACTION_THRESHOLD = 3;

// Here is the shape of the init return. This is where metadata needed by
// parents should be written on startup
struct InitResult {
  uint32_t entry_count;
};

class KV_Store {

private:
  std::unordered_map<uint32_t, std::unique_ptr<Segment>> segment_map;
  std::vector<uint32_t> cur_segment_ids;
  uint32_t active_id;
  Hash_Table hash_table;
  std::atomic<uint32_t> next_segment_id{1};
  // Register an observer to handle store events in other parts of program
  IStoreObserver *obs = nullptr;
  bool is_compacting = false;

public:
  KV_Store();

private:
  uint32_t generateNewSegment();
  void setActiveSegment(const uint32_t segment_id);
  uint32_t generateSegmentId();
  uint32_t readSegmentId(const std::filesystem::path &file);

public:
  InitResult init();
  void registerObserver(IStoreObserver *new_obs);
  void encodeIndexAndSet(const std::string &key, const uint32_t segment_id,
                         const uint32_t file_offset);
  bool isLatestRecord(const std::string &key, const uint32_t segment_id,
                      const uint32_t file_offset);
  void set(std::string &key, std::string &value);
  std::optional<std::tuple<const std::string, const uint32_t>>
  get(const std::string &key);
  bool remove(const std::string &key);
  void compact();
  void destroySegment(std::unique_ptr<Segment> &segment);
};
