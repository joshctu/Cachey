#include "store.hpp"
#include "exceptions.hpp"
#include "hash.hpp"
#include "istore_observer.hpp"
#include "segment.hpp"
#include "xxhash.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
KV_Store::KV_Store() {}

/**
 * @brief Sets a new active segment id
 *
 * @param segment_id The segment id of the new active segment
 */
void KV_Store::setActiveSegment(const uint32_t segment_id) {
  active_id = segment_id;
}

/**
 * @brief Generates a new segment_id
 *
 * @return The next monotonically increasing segment_id
 */
uint32_t KV_Store::generateSegmentId() {
  return next_segment_id.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief Given a file path, find and return the segment id metadata
 *
 * @param path The path to the log file
 *
 * @return The segment id of this segment
 */
uint32_t KV_Store::readSegmentId(const std::filesystem::path &path) {
  std::ifstream segment_file(path, std::ios::in | std::ios::binary);

  std::streamoff offset = static_cast<std::streamoff>(MAX_SEGMENT_SIZE) -
                          static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);

  offset += sizeof(decltype(SegmentFooter::magic)) +
            sizeof(decltype(SegmentFooter::bytes_written)) +
            sizeof(decltype(SegmentFooter::dead_bytes)) +
            sizeof(decltype(SegmentFooter::entry_count));

  segment_file.seekg(offset, std::ios::beg);
  uint32_t segment_id;

  segment_file.read(reinterpret_cast<char *>(&segment_id), sizeof(segment_id));
  return segment_id;
}

/**
 * @brief generate a new log segment in sequential order
 *
 * @return The segment_id of the new segment.
 */
uint32_t KV_Store::generateNewSegment() {
  uint32_t segment_id = generateSegmentId();
  cur_segment_ids.push_back(segment_id);

  std::stringstream file;

  file << std::setfill('0') << std::setw(6) << segment_id << ".log";

  std::filesystem::path p =
      std::filesystem::path("./database/wal/segments") / file.str();

  segment_map[segment_id] = std::make_unique<Segment>(
      segment_id, MAX_SEGMENT_SIZE, p, hash_table.HASH_SEED);

  if (segment_map.size() % 20 == 0)
    compact();
  return segment_id;
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

void KV_Store::registerObserver(IStoreObserver *new_obs) { obs = new_obs; }

/**
 * @brief Initialize the store with the existing log segments
 *
 * @return InitResult object. Includes the number of total number of records
 * that were in the logs
 */
InitResult KV_Store::init() {
  std::vector<std::filesystem::path> log_files;
  for (const auto &entry :
       std::filesystem::directory_iterator("./database/wal/segments/")) {
    if (entry.path().extension() == ".log")
      log_files.push_back(entry.path());
  }

  if (log_files.empty()) {
    uint32_t segment_id = generateNewSegment();
    setActiveSegment(segment_id);
    return {0};
  }

  uint32_t total_entries = 0;

  std::sort(log_files.begin(), log_files.end(),
            [](std::filesystem::path const &a, std::filesystem::path const &b) {
              return std::stoi(a.stem().string()) <
                     std::stoi(b.stem().string());
            });

  uint32_t max_segment_id = 0;
  for (const auto &file : log_files) {
    uint32_t segment_id = readSegmentId(file);
    cur_segment_ids.push_back(segment_id);
    max_segment_id = std::max(max_segment_id, segment_id);
    segment_map[segment_id] = std::make_unique<Segment>(
        segment_id, MAX_SEGMENT_SIZE, std::filesystem::path(file),
        hash_table.HASH_SEED);

    try {
      uint32_t segment_entries = segment_map[segment_id]->replay(
          [this](const std::string &key, const uint32_t segment_id,
                 const uint32_t file_offset) {
            encodeIndexAndSet(key, segment_id, file_offset);
          });
      //  std::cout << "Segment entries for id: " << segment_id << ": "
      //          << segment_entries << std::endl;
      total_entries += segment_entries;
    } catch (ReadException &e) {
      std::cout << e.what() << std::endl;
    }

    if (!segment_map[segment_id]->isSealed)
      setActiveSegment(segment_id);
  }

  // We start operation with the 1 + the max segment id seen on startup.
  next_segment_id.store(max_segment_id + 1, std::memory_order_relaxed);
  return {total_entries};
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
    uint32_t segment_id = generateNewSegment();
    setActiveSegment(segment_id);
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
std::optional<std::tuple<const std::string, const uint32_t>>
KV_Store::get(const std::string &key) {
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
bool KV_Store::remove(const std::string &key) {
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

  if (!segment_map[active_id]->canAppend(record_size)) {
    segment_map[active_id]->seal();
    uint32_t new_segment_id = generateNewSegment();
    setActiveSegment(new_segment_id);
  }

  auto &active_segment = segment_map[active_id];

  uint32_t curr_file_ptr = active_segment->used;

  active_segment->remove(key);

  // Encode file and byte offset in index
  uint64_t encode_offset =
      static_cast<uint64_t>(curr_file_ptr) << 32 |
      static_cast<uint32_t>(std::stoi(active_segment->file_path.stem()));

  hash_table.updateIndex(hash, encode_offset);

  return true;
}

/**
 * @brief This function iterates through current log files looking for files
 * with a dead_bytes percentage of over 50%. Currently when 3 candidate files
 * are found they are compacted. Original segments and their log files are
 * deleted. Then the function returns.
 *
 */
void KV_Store::compact() {
  if (is_compacting)
    return;
  is_compacting = true;
  /* Step 1: a heuristic triggered this function. Maybe we reached a threshold
   segment number.

   Step 2: Comb segments starting from oldest. Read dead bytes field in footer.
   If dead / live bytes for this segment meets a threshold, add to compaction
   candidates. When we reach a certain number of compaction candidates, break.

   Step 3: Begin iterating through candidates. Skip invalid records and write
   valid records for each candidate to new files.
  */
  std::vector<uint32_t> candidates;

  std::sort(cur_segment_ids.begin(), cur_segment_ids.end());

  for (auto segment_id : cur_segment_ids) {
    auto &segment = segment_map[segment_id];
    if (segment->isSealed) {
      int dead_bytes = segment->readDeadBytes();

      int dead_bytes_percent = ((float)dead_bytes / MAX_SEGMENT_SIZE) * 100;

      if (dead_bytes_percent > 50) {
        candidates.push_back(segment_id);
        if (candidates.size() == COMPACTION_THRESHOLD)
          break;
      }
    }
  }

  if (candidates.size() != COMPACTION_THRESHOLD) {
    is_compacting = false;
    return;
  }

  /*
   * I need to now iterate through each segment looking for valid records. Lets
   * say i find one. Should the segment have a lambda that writes that record to
   * the new file?
   *
   * Say I try to write but the new file is full. I need to commit the file and
   * open a new one.
   */

  CompactionResult result;
  auto cur_writeable_segment_id = generateNewSegment();

  for (auto &candidate : candidates) {
    Cursor c{};
    SegmentRecordView rec;
    while (segment_map[candidate]->nextValidRecord(c, rec)) {

      if (!isLatestRecord(rec.key.data(), segment_map[candidate]->id,
                          c.file_offset)) {
        result.records_removed += 1;
        continue;
      }

      if (!segment_map[cur_writeable_segment_id]->canAppend(rec.length)) {
        segment_map[cur_writeable_segment_id]->seal();
        cur_writeable_segment_id = generateNewSegment();
      }

      uint32_t file_offset = segment_map[cur_writeable_segment_id]->used;
      encodeIndexAndSet(rec.key, segment_map[cur_writeable_segment_id]->id,
                        file_offset);
      segment_map[cur_writeable_segment_id]->append(rec.key, rec.value);
    }
    result.segments_merged += 1;
  }

  if (segment_map[cur_writeable_segment_id]->used > 0) {
    segment_map[cur_writeable_segment_id]->seal();
  } else {
    destroySegment(segment_map[cur_writeable_segment_id]);
  }

  for (auto &candidate : candidates) {
    destroySegment(segment_map[candidate]);
  }

  is_compacting = false;

  if (obs) {
    obs->onCompactionComplete(result);
  }
}

/**
 * @brief Completely destroy a segment log file and all its in-memory references
 *
 * @param segment The unique pointer to the segment to delete
 */
void KV_Store::destroySegment(std::unique_ptr<Segment> &segment) {
  segment->deleteFile();
  auto it =
      std::find(cur_segment_ids.begin(), cur_segment_ids.end(), segment->id);

  if (it != cur_segment_ids.end())
    cur_segment_ids.erase(it);

  segment_map.erase(segment->id);
}
