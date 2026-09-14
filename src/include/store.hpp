#pragma once

#include "hash.hpp"
#include "segment.hpp"
#include <atomic>
#include <optional>
#include <string>
#include <unordered_map>

constexpr int COMPACTION_THRESHOLD = 3;

class KV_Store {

private:
  std::unordered_map<uint32_t, std::unique_ptr<Segment>> segment_map;
  std::vector<uint32_t> cur_segment_ids;
  uint32_t active_id;
  Hash_Table hash_table;
  std::atomic<uint32_t> next_segment_id{1};

public:
  KV_Store();

private:
  uint32_t generateNewSegment();

public:
  void encodeIndexAndSet(const std::string &key, const uint32_t segment_id,
                         const uint32_t file_offset);
  bool isLatestRecord(const std::string &key, const uint32_t segment_id,
                      const uint32_t file_offset);
  void setActiveSegment(const uint32_t segment_id);
  uint32_t generateSegmentId();
  uint32_t readSegmentId(const std::filesystem::path &file);
  void set(std::string &key, std::string &value);
  std::optional<std::tuple<std::string, uint32_t>> get(std::string &key);
  bool remove(std::string &key);
  void compact();
  void destroySegment(std::unique_ptr<Segment> &segment);
};
