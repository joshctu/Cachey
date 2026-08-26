#include "store.hpp"
#include "hash.hpp"
#include "segment.hpp"
#include "xxhash.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Orchestrates getting and setting key value pairs
 *
 *
 */
KV_Store::KV_Store() {
  std::vector<std::filesystem::path> log_files;
  for (const auto &entry :
       std::filesystem::directory_iterator("./database/wal/segments/")) {
    if (entry.path().extension() == ".log")
      log_files.push_back(entry.path());
  }

  if (log_files.empty()) {
    generateNewActiveSegment();
    return;
  }

  std::sort(log_files.begin(), log_files.end(),
            [](std::filesystem::path const &a, std::filesystem::path const &b) {
              return std::stoi(a.stem().string()) <
                     std::stoi(b.stem().string());
            });

  for (const auto &file : log_files) {
    uint32_t segment_id = std::stoi(file.stem().string());
    segments.emplace_back(segment_id, MAX_SEGMENT_SIZE, file,
                          hash_table.HASH_SEED);
    Segment &segment = segments.back();
    segment.replay([this](const std::string &key, uint32_t segment_id,
                          uint32_t file_offset) {
      encodeIndexAndSet(key, segment_id, file_offset);
    });

    segment_map[segment_id] = segments.size() - 1;

    if (!segment.isSealed)
      active_idx = segments.size() - 1;
  }
}

/**
 * @brief generate a new log segment in sequential order
 */
void KV_Store::generateNewActiveSegment() {
  unsigned long segment_id = segments.size() + 1;

  std::stringstream file;

  file << std::setfill('0') << std::setw(5) << segment_id << ".log";

  std::filesystem::path p =
      std::filesystem::path("./database/wal/segments") / file.str();

  segments.emplace_back(segment_id, MAX_SEGMENT_SIZE, p, hash_table.HASH_SEED);
  segment_map[segment_id] = segments.size() - 1;
  active_idx = segments.size() - 1;
}

void KV_Store::encodeIndexAndSet(const std::string &key,
                                 const uint32_t segment_id,
                                 const uint32_t file_offset) {
  uint64_t entry_loc = ((uint64_t)segment_id << 32) | file_offset;
  hash_table.createIndex(key, entry_loc);
}

/**
 * @brief Write a new key into the database
 * @param key The key to write to the log
 * @param value The value to assign to the given key
 *
 */
void KV_Store::set(std::string &key, std::string &value) {

  uint64_t record_size = SERIALIZED_HEADER_SIZE + key.size() + value.size();

  if (!segments[active_idx].canAppend(record_size)) {
    segments[active_idx].seal();
    generateNewActiveSegment();
  }

  Segment &active_segment = segments[active_idx];

  uint32_t curr_file_ptr = active_segment.used;
  active_segment.append(key, value);

  // Encode file and byte offset in index
  uint64_t encode_offset = static_cast<uint64_t>(curr_file_ptr) << 32 |
                           static_cast<uint32_t>(std::stoi(
                               active_segment.file_path.stem().string()));

  hash_table.createIndex(key, encode_offset);
}

/**
 * @brief Given a key, return the cooresponding value from the database
 * @param key The key to retrive the associated value
 *
 * @return The associated value of the given key
 * @return If the key is not found, returns nullopt
 *
 */
std::optional<std::string> KV_Store::get(std::string &key) {
  uint64_t hash = hash_table.generateHash(key);
  std::optional<uint64_t> location = hash_table.lookupHash(key);

  if (not location) {
    return std::nullopt;
  }

  uint64_t valid_location = *location;

  uint32_t file_offset = static_cast<uint32_t>(valid_location >> 32);
  uint32_t segment_id = static_cast<uint32_t>(valid_location);

  auto it = segment_map.find(segment_id);
  if (it != segment_map.end()) {
    uint32_t idx = it->second;
    if (idx < segments.size()) {
      Segment found_segment = segments[idx];
      auto result = found_segment.read(key, file_offset);

      if (not result)
        return std::nullopt;

      std::string valid_result = *result;
      return valid_result;
    }
  }
  return std::nullopt;
};

bool KV_Store::remove(std::string &key) {
  uint64_t hash = hash_table.generateHash(key);

  std::optional<uint64_t> offset = hash_table.lookupHash(key);
  if (not offset) {
    return false;
  }

  SegmentEntryHeader header(0, key.size(), 0x30);

  uint64_t record_size = SERIALIZED_HEADER_SIZE + key.size() + 0;
  Segment &active_segment = segments[active_idx];

  if (!active_segment.canAppend(record_size)) {
    active_segment.seal();
    generateNewActiveSegment();
    active_segment = segments[active_idx];
  }

  uint32_t curr_file_ptr = active_segment.used;

  active_segment.remove(key);

  // Encode file and byte offset in index
  uint64_t encode_offset =
      static_cast<uint64_t>(curr_file_ptr) << 32 |
      static_cast<uint32_t>(std::stoi(active_segment.file_path.stem()));

  hash_table.updateIndex(hash, encode_offset);

  return true;
}
