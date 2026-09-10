#include "store.hpp"
#include "exceptions.hpp"
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
    try {
      segment.replay([this](const std::string &key, const uint32_t segment_id,
                            const uint32_t file_offset) {
        encodeIndexAndSet(key, segment_id, file_offset);
      });
    } catch (ReadException &e) {
      std::cout << e.what() << std::endl;
    }

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

/**
 * @brief Creates a hash entry for a current record
 *
 * @param key The key for this record
 * @param segment_id The id for this log file
 * @param file_offset The byte offset for this record
 */
void KV_Store::encodeIndexAndSet(const std::string &key,
                                 const uint32_t segment_id,
                                 const uint32_t file_offset) {
  uint64_t entry_loc = ((uint64_t)file_offset << 32) | segment_id;
  hash_table.createIndex(key, entry_loc);
}

bool KV_Store::isLatestRecord(const std::string &key, const uint32_t segment_id,
                              const uint32_t file_offset) {
  uint64_t entry_loc = ((uint64_t)file_offset << 32) | segment_id;

  return hash_table.lookupHash(key) == entry_loc;
}

/**
 * @brief Write a new key into the database
 * @param key The key to write to the log
 * @param value The value to assign to the given key
 *
 */
void KV_Store::set(std::string &key, std::string &value) {

  std::optional<uint64_t> location = hash_table.lookupHash(key);

  if (location) {
    uint64_t valid_location = *location;
    uint32_t old_file_offset = static_cast<uint32_t>(valid_location >> 32);
    uint32_t old_segment_id = static_cast<uint32_t>(valid_location);

    auto it = segment_map.find(old_segment_id);

    if (it != segment_map.end()) {
      uint32_t idx = it->second;
      if (idx < segment_map.size()) {
        Segment &old_segment = segments[idx];
        old_segment.invalidateBytes(old_file_offset);
      }
    }
  }
  // First 32bit value is magic length value
  uint64_t record_size =
      sizeof(uint32_t) + SERIALIZED_HEADER_SIZE + key.size() + value.size();

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
std::optional<std::tuple<std::string, uint32_t>>
KV_Store::get(std::string &key) {
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
      Segment &found_segment = segments[idx];
      auto value = found_segment.read(key, file_offset);
      auto dead_bytes = found_segment.readDeadBytes();

      if (not value)
        return std::nullopt;

      std::string valid_value = *value;
      return std::tuple(valid_value, dead_bytes);
    }
  }
  return std::nullopt;
};

/**
 * @brief Removes a record by creating a tombstone and updating the hash record
 *
 * @param key The key to remove
 * @return True on removal success, false otherwise
 */
bool KV_Store::remove(std::string &key) {
  uint64_t hash = hash_table.generateHash(key);

  std::optional<uint64_t> location = hash_table.lookupHash(key);
  if (not location) {
    return false;
  }

  uint64_t valid_location = *location;

  uint32_t file_offset = static_cast<uint32_t>(valid_location >> 32);
  uint32_t segment_id = static_cast<uint32_t>(valid_location);

  Segment &old_segment = segments[segment_id];
  old_segment.invalidateBytes(file_offset);

  SegmentEntryHeader header(0, key.size(), 0x30);

  // First 32bit value is magic length number
  uint64_t record_size = sizeof(decltype(SegmentRecord::length)) +
                         SERIALIZED_HEADER_SIZE + key.size() + 0;
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

void KV_Store::compact() {
  /* Step 1: a heuristic triggered this function. Maybe we reached a threshold
   segment number.

   Step 2: Comb segments starting from oldest. tally dead bytes for each
   segment. If dead / live bytes for this segment meets a threshold, add to
   compaction candidates. When we reach a certain number of compaction
   candidates, break.

   Step 3: Begin iterating through candidates. Skip invalid records and write
   valid records for each candidate to new files.
  */
  std::vector<Segment> candidates;

  for (auto &segment : segments) {
    if (segment.isSealed) {
      int dead_bytes_percent = segment.computeDeadBytes(
          [this](const std::string &key, const uint32_t segment_id,
                 const uint32_t file_offset) {
            return isLatestRecord(key, segment_id, file_offset);
          });

      if (dead_bytes_percent > 50) {
        candidates.push_back(segment);
        if (candidates.size() == COMPACTION_THRESHOLD)
          break;
      }
    }
  }
}
