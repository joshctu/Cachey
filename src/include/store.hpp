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
  int active_id;
  Hash_Table hash_table;
  std::atomic<uint64_t> next_segment_id{1};

public:
  KV_Store();

private:
  void generateNewActiveSegment();

public:
  void encodeIndexAndSet(const std::string &key, const uint32_t segment_id,
                         const uint32_t file_offset);
  bool isLatestRecord(const std::string &key, const uint32_t segment_id,
                      const uint32_t file_offset);
  uint64_t getSegmentId();
  void set(std::string &key, std::string &value);
  std::optional<std::tuple<std::string, uint32_t>> get(std::string &key);
  bool remove(std::string &key);
  void compact();
};
