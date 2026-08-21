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
  for (const auto &entry :
       std::filesystem::directory_iterator("./database/wal/segments/")) {
    if (entry.path().extension() == ".log") {
      std::filesystem::path file_path = entry.path();
      uint32_t size = (uint32_t)std::filesystem::file_size(entry.path());
      unsigned long id = std::stoul(entry.path().filename(), nullptr, 10);

      Segment segment(id, size, file_path, hash_table.HASH_SEED);

      segments.push_back(segment);
    }
  }

  std::sort(segments.begin(), segments.end(),
            [](Segment a, Segment b) { return a.id < b.id; });
}

/**
 * @brief generate a new log segment in sequential order
 */
void KV_Store::generateSegment() {
  unsigned long segment_id = segments.size() + 1;

  std::stringstream file;

  file << std::setfill('0') << std::setw(5) << segment_id << ".log";

  std::filesystem::path p =
      std::filesystem::path("./database/wal/segments") / file.str();

  Segment segment(segment_id, MAX_SEGMENT_SIZE, p, hash_table.HASH_SEED);

  segments.push_back(segment);
}

/**
 * @brief Write a new key into the database
 * @param key The key to write to the log
 * @param value The value to assign to the given key
 *
 */
void KV_Store::set(std::string &key, std::string &value) {

  uint64_t record_size = sizeof(SegmentEntryHeader) + key.size() + value.size();

  if (segments.empty() || !segments.back().canAppend(record_size))
    generateSegment();

  Segment &latest_segment = segments.back();
  uint32_t curr_file_ptr = latest_segment.used;
  latest_segment.append(key, value);

  // Encode file and byte offset in index
  uint64_t encode_offset =
      static_cast<uint64_t>(curr_file_ptr) << 32 |
      static_cast<uint32_t>(std::stoi(latest_segment.file_path.stem()));

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

  Segment &segment = segments[segment_id - 1];
  auto result = segment.read(key, file_offset);

  if (not result)
    return std::nullopt;

  std::string valid_result = *result;
  return valid_result;
};

bool KV_Store::remove(std::string &key) {
  uint64_t hash = hash_table.generateHash(key);

  std::optional<uint64_t> offset = hash_table.lookupHash(key);
  if (not offset) {
    return false;
  }

  SegmentEntryHeader header(0, key.size(), 0x30);

  uint64_t record_size = sizeof(SegmentEntryHeader) + key.size() + 0;

  if (segments.empty() || !segments.back().canAppend(record_size))
    generateSegment();

  Segment &latest_segment = segments.back();
  uint32_t curr_file_ptr = latest_segment.used;

  latest_segment.remove(key);

  // Encode file and byte offset in index
  uint64_t encode_offset =
      static_cast<uint64_t>(curr_file_ptr) << 32 |
      static_cast<uint32_t>(std::stoi(latest_segment.file_path.stem()));

  hash_table.updateIndex(hash, encode_offset);

  return true;
}
