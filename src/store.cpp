#include "store.hpp"
#include "exceptions.hpp"
#include "hash.hpp"
#include "hint_manager.hpp"
#include "istore_observer.hpp"
#include "segment.hpp"
#include "xxhash.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Orchestrates getting and setting key value pairs
 *
 *
 */
KV_Store::KV_Store(ProjectConfig &config)
    : root_dir(config.root_dir),
      compaction_threshold(config.compaction_threshold),
      max_segment_size(config.max_segment_size), manifest(config.root_dir) {
  std::filesystem::create_directories(root_dir / "database/wal/segments");
  compaction_thread = std::thread(&KV_Store::compactionLifecycle, this);
}

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

  std::streamoff offset = static_cast<std::streamoff>(max_segment_size) -
                          static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);

  offset += sizeof(decltype(SegmentFooter::magic)) +
            sizeof(decltype(SegmentFooter::bytes_written)) +
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
  segment_id_monitor.push_back(segment_id);

  std::stringstream file;

  file << std::setfill('0') << std::setw(6) << segment_id << ".log";

  std::filesystem::path p =
      std::filesystem::path(root_dir / "database/wal/segments") / file.str();

  segment_map_monitor.insert(segment_id, max_segment_size, p);

  if (segment_map_monitor.size() % 20 == 0) {
    compaction_pending.store(true);
    cv.notify_one();
  }

  return segment_id;
}

void KV_Store::compactionLifecycle() {

  while (running) {
    std::unique_lock lock(compact_mtx);
    cv.wait(lock, [this] { return compaction_pending.load() || !running; });

    if (!running)
      break;

    compaction_pending.store(false);

    lock.unlock();
    compact();
  }
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
 * @brief Initialize the store with the existing log segments. Makes the
 * decision to attempt boot from hints or WAL.
 *
 * @return InitResult object. Includes the number of total number of records
 * that were in the logs
 */
InitResult KV_Store::init() {

  InitResult res;
  HintManager hintManager(root_dir);

  if (hintManager.isValidHintFile())
    res = bootFromHints(hintManager);

  if (!res.success)
    res = bootFromWAL();

  hintManager.removeHintFile();
  return res;
}

InitResult KV_Store::bootFromHints(HintManager &hintManager) {
  InitResult res;

  Cursor c;
  HintRecord rec;
  int cur_segment_id = -1;

  while (hintManager.nextValidHint(c, rec)) {
    if (rec.segment_id != cur_segment_id) {
      segment_id_monitor.push_back(rec.segment_id);
      std::stringstream file;

      file << std::setfill('0') << std::setw(6) << rec.segment_id << ".log";

      std::filesystem::path file_path =
          std::filesystem::path(root_dir / "database/wal/segments") /
          file.str();
      segment_map_monitor.insert(rec.segment_id, max_segment_size, file_path);
      auto segment_ptr = segment_map_monitor.access(rec.segment_id);
      cur_segment_id = rec.segment_id;
      if (segment_ptr && !segment_ptr->isSealed)
        setActiveSegment(rec.segment_id);
    }
    encodeIndexAndSet(rec.key.data(), rec.segment_id, rec.file_offset);
    res.entry_count += 1;
  }
  if (hintManager.validateChecksum()) {
    next_segment_id.store(cur_segment_id + 1, std::memory_order_relaxed);
    res.success = true;
  } else {
    segment_map_monitor.clear();
    segment_id_monitor.clear();
  }
  return res;
}

/**
 * @brief Boot the application using the full WAL
 *
 * @return An object containing total records found and whether the boot was
 * successful.
 *
 */
InitResult KV_Store::bootFromWAL() {
  std::vector<std::filesystem::path> log_files;
  for (const auto &entry : std::filesystem::directory_iterator(
           root_dir / "database/wal/segments/")) {
    if (entry.path().extension() == ".log")
      log_files.push_back(entry.path());
  }

  if (log_files.empty()) {
    uint32_t segment_id = generateNewSegment();
    setActiveSegment(segment_id);
    return {0, true};
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
    segment_id_monitor.push_back(segment_id);
    max_segment_id = std::max(max_segment_id, segment_id);
    segment_map_monitor.insert(segment_id, max_segment_size,
                               std::filesystem::path(file));

    auto segment_ptr = segment_map_monitor.access(segment_id);

    try {
      uint32_t segment_entries = segment_ptr->replay(
          [this](const std::string &key, const uint32_t segment_id,
                 const uint32_t file_offset) {
            encodeIndexAndSet(key, segment_id, file_offset);
          });
      total_entries += segment_entries;
    } catch (ReadException &e) {
      std::cout << e.what() << std::endl;
      return {total_entries, false};
    }

    if (segment_ptr && !segment_ptr->isSealed)
      setActiveSegment(segment_id);
  }

  // We start operation with the 1 + the max segment id seen on startup.
  next_segment_id.store(max_segment_id + 1, std::memory_order_relaxed);
  return {total_entries, true};
}

/**
 * @brief Write a new key into the database
 * @param key The key to write to the log
 * @param value The value to assign to the given key
 *
 */
StoreSetResult KV_Store::set(const std::string &key, const std::string &value) {

  std::unique_lock<std::shared_mutex> lock(store_mtx);
  std::optional<uint64_t> location = hash_table.lookupHash(key);
  StoreSetResult res;
  bool existed = false;

  if (location) {
    uint64_t valid_location = *location;
    uint32_t old_file_offset = static_cast<uint32_t>(valid_location >> 32);
    uint32_t old_segment_id = static_cast<uint32_t>(valid_location);

    auto segment_ptr = segment_map_monitor.access(old_segment_id);

    if (segment_ptr != nullptr) {
      uint32_t record_size = segment_ptr->readRecordSize(old_file_offset);
      manifest.writeInvalidateBytesEvent(old_segment_id, record_size);
      res.unique_write_completed = false;
      existed = true;
    }
  }

  // First 32bit value is magic length value
  uint64_t record_size =
      sizeof(uint32_t) + SERIALIZED_HEADER_SIZE + key.size() + value.size();

  auto active_ptr = segment_map_monitor.access(active_id);
  if (!active_ptr->canAppend(record_size)) {
    active_ptr->seal();
    uint32_t segment_id = generateNewSegment();
    setActiveSegment(segment_id);
  }

  auto active_segment_ptr = segment_map_monitor.access(active_id);

  uint32_t curr_file_ptr = active_segment_ptr->used;

  active_segment_ptr->append(key, value);

  // Encode file and byte offset in index
  uint64_t encode_offset = static_cast<uint64_t>(curr_file_ptr) << 32 |
                           static_cast<uint32_t>(std::stoi(
                               active_segment_ptr->file_path.stem().string()));

  hash_table.createIndex(key, encode_offset);
  return res;
}

/**
 * @brief Given a key, return the cooresponding value from the database
 * @param key The key to retrive the associated value
 *
 * @return The associated value of the given key
 * @return If the key is not found, returns nullopt
 *
 */
std::optional<const std::string> KV_Store::get(const std::string &key) {
  std::shared_lock<std::shared_mutex> lock(store_mtx);
  std::optional<uint64_t> location = hash_table.lookupHash(key);

  if (not location) {
    return std::nullopt;
  }

  uint64_t valid_location = *location;

  uint32_t file_offset = static_cast<uint32_t>(valid_location >> 32);
  uint32_t segment_id = static_cast<uint32_t>(valid_location);

  auto segment_ptr = segment_map_monitor.access(segment_id);
  auto value = segment_ptr->read(key, file_offset);

  if (not value)
    return std::nullopt;

  std::string valid_value = *value;
  return valid_value;
};

/**
 * @brief Removes a record by creating a tombstone and updating the hash record
 *
 * @param key The key to remove
 * @return True on removal success, false otherwise
 */
bool KV_Store::remove(const std::string &key) {
  std::unique_lock<std::shared_mutex> lock(store_mtx);
  uint64_t hash = hash_table.generateHash(key);

  std::optional<uint64_t> location = hash_table.lookupHash(key);
  if (not location) {
    return false;
  }

  uint64_t valid_location = *location;

  const uint32_t old_file_offset = static_cast<uint32_t>(valid_location >> 32);
  const uint32_t old_segment_id = static_cast<uint32_t>(valid_location);

  auto old_segment_ptr = segment_map_monitor.access(old_segment_id);
  if (old_segment_ptr) {
    uint32_t old_record_size = old_segment_ptr->readRecordSize(old_file_offset);
    manifest.writeInvalidateBytesEvent(old_segment_id, old_record_size);
  }

  SegmentEntryHeader header(0, key.size(), 0x30);

  // First 32bit value is magic length number
  uint64_t record_size = sizeof(decltype(SegmentRecord::length)) +
                         SERIALIZED_HEADER_SIZE + key.size() + 0;

  auto active_segment_ptr = segment_map_monitor.access(active_id);
  if (!active_segment_ptr->canAppend(record_size)) {
    active_segment_ptr->seal();
    uint32_t new_segment_id = generateNewSegment();
    setActiveSegment(new_segment_id);
  }

  active_segment_ptr = segment_map_monitor.access(active_id);

  uint32_t curr_file_ptr = active_segment_ptr->used;

  active_segment_ptr->remove(key);

  // Encode file and byte offset in index
  uint64_t encode_offset =
      static_cast<uint64_t>(curr_file_ptr) << 32 |
      static_cast<uint32_t>(std::stoi(active_segment_ptr->file_path.stem()));

  hash_table.createIndex(key, encode_offset);

  return true;
}

/**
 * @brief This function calls the manifest manager looking for segments
 * with a dead_bytes percentage of over 50%. Compaction threshold (number of
 * segments required to trigger compaction) is configurable through config
 * arguments to the store. Original segments and their log files are deleted.
 * Manifest is compacted.
 */
void KV_Store::compact() {
  std::unordered_set<uint32_t> candidates;
  std::vector<uint32_t> segment_snapshot = segment_id_monitor.get_snapshot();

  std::sort(segment_snapshot.begin(), segment_snapshot.end());
  std::unordered_map<uint32_t, uint32_t> invalid_bytes =
      manifest.getInvalidBytesTotals();

  for (auto segment_id : segment_snapshot) {
    auto segment_ptr = segment_map_monitor.access(segment_id);
    if (!segment_ptr)
      continue;
    if (segment_ptr->isSealed) {
      auto it = invalid_bytes.find(segment_id);
      int dead_bytes = (it != invalid_bytes.end()) ? it->second : 0;
      int dead_bytes_percent = ((float)dead_bytes / max_segment_size) * 100;
      if (dead_bytes_percent > 50) {
        candidates.insert(segment_id);
        if (candidates.size() == compaction_threshold)
          break;
      }
    }
  }

  if (candidates.size() != compaction_threshold) {
    return;
  }

  CompactionResult result;
  auto cur_writeable_segment_ptr =
      segment_map_monitor.access(generateNewSegment());

  std::unique_lock<std::shared_mutex> lock(store_mtx);
  for (auto &candidate : candidates) {
    Cursor c{};
    SegmentRecordView rec;
    auto candidate_ptr = segment_map_monitor.access(candidate);
    while (candidate_ptr && candidate_ptr->nextValidRecord(c, rec)) {

      if (!isLatestRecord(rec.key.data(), candidate_ptr->id, c.file_offset)) {
        result.records_removed += 1;
        continue;
      }

      if (!cur_writeable_segment_ptr->canAppend(rec.length)) {
        cur_writeable_segment_ptr->seal();
        cur_writeable_segment_ptr =
            segment_map_monitor.access(generateNewSegment());
      }

      uint32_t file_offset = cur_writeable_segment_ptr->used;
      encodeIndexAndSet(rec.key, cur_writeable_segment_ptr->id, file_offset);
      cur_writeable_segment_ptr->append(rec.key, rec.value);
    }
    result.segments_merged += 1;
  }

  if (cur_writeable_segment_ptr->used > 0) {
    cur_writeable_segment_ptr->seal();
  } else {
    destroySegment(cur_writeable_segment_ptr);
  }

  for (auto &candidate : candidates) {
    auto candidate_ptr = segment_map_monitor.access(candidate);
    destroySegment(candidate_ptr);
  }

  manifest.compactManifest(std::move(candidates), std::move(invalid_bytes));

  if (obs) {
    obs->onCompactionComplete(result);
  }
}

long KV_Store::getSegmentCount() { return segment_id_monitor.size(); }

/**
 * @brief Completely destroy a segment log file and all its in-memory references
 *
 * @param segment The unique pointer to the segment to delete
 */
void KV_Store::destroySegment(Segment *segment) {
  segment->deleteFile();

  segment_id_monitor.erase(segment->id);
  segment_map_monitor.erase(segment->id);
}

// Attempt to generate hints file on destruction
KV_Store::~KV_Store() {
  running.store(false);
  cv.notify_one();
  compaction_thread.join();

  HintManager hintManager(root_dir);

  std::vector<uint32_t> segment_snapshot = segment_id_monitor.get_snapshot();
  std::sort(segment_snapshot.begin(), segment_snapshot.end());

  for (auto &segment_id : segment_snapshot) {
    auto segment_ptr = segment_map_monitor.access(segment_id);
    if (!segment_ptr)
      continue;
    segment_ptr->iterateCurrentRecords(
        [&hintManager](const std::string &key, const uint32_t segment_id,
                       const uint32_t file_offset) {
          return hintManager.writeHint(key, segment_id, file_offset);
        },
        [this](const std::string &key, const uint32_t segment_id,
               const uint32_t file_offset) {
          return isLatestRecord(key, segment_id, file_offset);
        });
  }
  hintManager.writeFooter();
}
