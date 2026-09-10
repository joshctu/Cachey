#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "hash.hpp"
#include "segment.hpp"

constexpr int COMPACTION_THRESHOLD = 3;

class KV_Store {

private:
  std::unordered_map<int, int> segment_map;
  std::vector<Segment> segments;
  int active_idx;
  Hash_Table hash_table;

public:
  KV_Store();

private:
  void generateNewActiveSegment();

public:
  void encodeIndexAndSet(const std::string &key, const uint32_t segment_id,
                         const uint32_t file_offset);
  bool isLatestRecord(const std::string &key, const uint32_t segment_id,
                      const uint32_t file_offset);
  void set(std::string &key, std::string &value);
  std::optional<std::tuple<std::string, uint32_t>> get(std::string &key);
  bool remove(std::string &key);
  void compact();
};
