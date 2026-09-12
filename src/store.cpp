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
    segment_map[segment_id] = std::make_unique<Segment>(
        segment_id, MAX_SEGMENT_SIZE, std::filesystem::path(file),
        hash_table.HASH_SEED);

    try {
      segment_map[segment_id]->replay([this](const std::string &key,
                                             const uint32_t segment_id,
                                             const uint32_t file_offset) {
        encodeIndexAndSet(key, segment_id, file_offset);
      });
    } catch (ReadException &e) {
      std::cout << e.what() << std::endl;
    }

    if (!segment_map[segment_id]->isSealed)
      active_id = segment_id;
  }
}

uint64_t KV_Store::getSegmentId() {
  return next_segment_id.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief generate a new log segment in sequential order
 */
void KV_Store::generateNewActiveSegment() {
  uint64_t segment_id = getSegmentId();

  std::stringstream file;

  file << std::setfill('0') << std::setw(6) << segment_id << ".log";

  std::filesystem::path p =
      std::filesystem::path("./database/wal/segments") / file.str();

  segment_map[segment_id] = std::make_unique<Segment>(
      segment_id, MAX_SEGMENT_SIZE, p, hash_table.HASH_SEED);
  active_id = segment_id;
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
      auto &segment = it->second;
      segment->invalidateBytes(old_file_offset);
    }
  }

  // First 32bit value is magic length value
  uint64_t record_size =
      sizeof(uint32_t) + SERIALIZED_HEADER_SIZE + key.size() + value.size();

  if (!segment_map[active_id]->canAppend(record_size)) {
    segment_map[active_id]->seal();
    generateNewActiveSegment();
  }

  auto &active_segment = segment_map[active_id];

  uint32_t curr_file_ptr = active_segment->used;
  active_segment->append(key, value);

  // Encode file and byte offset in index
  uint64_t encode_offset = static_cast<uint64_t>(curr_file_ptr) << 32 |
                           static_cast<uint32_t>(std::stoi(
                               active_segment->file_path.stem().string()));

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
    auto &segment = it->second;
    auto value = segment->read(key, file_offset);
    auto dead_bytes = segment->readDeadBytes();

    if (not value)
      return std::nullopt;

    std::string valid_value = *value;
    return std::tuple(valid_value, dead_bytes);
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

  auto it = segment_map.find(segment_id);
  if (it != segment_map.end()) {
    auto &old_segment = it->second;
    old_segment->invalidateBytes(file_offset);
  } else {
    // TODO: Handle not found case
  }

  SegmentEntryHeader header(0, key.size(), 0x30);

  // First 32bit value is magic length number
  uint64_t record_size = sizeof(decltype(SegmentRecord::length)) +
                         SERIALIZED_HEADER_SIZE + key.size() + 0;
  auto &active_segment = segment_map[active_id];

  if (!active_segment->canAppend(record_size)) {
    active_segment->seal();
    generateNewActiveSegment();
    auto &active_segment = segment_map[active_id];
  }

  uint32_t curr_file_ptr = active_segment->used;

  active_segment->remove(key);

  // Encode file and byte offset in index
  uint64_t encode_offset =
      static_cast<uint64_t>(curr_file_ptr) << 32 |
      static_cast<uint32_t>(std::stoi(active_segment->file_path.stem()));

  hash_table.updateIndex(hash, encode_offset);

  return true;
}

void KV_Store::compact() {
  /* Step 1: a heuristic triggered this function. Maybe we reached a threshold
   segment number.

   Step 2: Comb segments starting from oldest. Read dead bytes field in footer.
   If dead / live bytes for this segment meets a threshold, add to compaction
   candidates. When we reach a certain number of compaction candidates, break.

   Step 3: Begin iterating through candidates. Skip invalid records and write
   valid records for each candidate to new files.
  */
  std::vector<std::unique_ptr<Segment>> candidates;

  for (auto &entry : segment_map) {
    auto &segment = entry.second;
    if (segment->isSealed) {
      int dead_bytes = segment->readDeadBytes();

      int dead_bytes_percent = (dead_bytes / MAX_SEGMENT_SIZE) * 100;

      if (dead_bytes_percent > 50) {
        candidates.push_back(std::move(segment));
        if (candidates.size() == COMPACTION_THRESHOLD)
          break;
      }
    }
  }

  if (candidates.size() != COMPACTION_THRESHOLD)
    return;

  /*
   * I need to now iterate through each segment looking for valid records. Lets
   * say i find one. Should the segment have a lambda that writes that record to
   * the new file?
   *
   * Say I try to write but the new file is full. I need to commit the file and
   * open a new one.ff
   */
}
