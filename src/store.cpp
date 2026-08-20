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

#include "exceptions.hpp"
#include "log_file.hpp"
#include "store.hpp"
#include "xxhash.h"

Hash_Table::Hash_Table() {}

uint64_t Hash_Table::generate_hash(const std::string &key) {
  return XXH64(key.data(), key.size(), HASH_SEED);
}

/**
 * @brief Generate a checksum for a db entry
 * @param key The key being used
 * @param value The value being used
 * @param operation The operation being performed
 *
 * @return The resulting hash value
 */
uint64_t Hash_Table::generate_checksum(const std::string &key,
                                       const std::string &value,
                                       uint8_t operation) {
  XXH64_state_t *const state = XXH64_createState();

  XXH64_reset(state, HASH_SEED);

  uint32_t value_size = value.size();
  XXH64_update(state, &value_size, sizeof(value_size));

  uint32_t key_size = key.size();
  XXH64_update(state, &key_size, sizeof(key_size));

  XXH64_update(state, &operation, sizeof(operation));

  XXH64_update(state, key.data(), key.size());
  XXH64_update(state, value.data(), value.size());

  uint64_t const hash = XXH64_digest(state);
  XXH64_freeState(state);

  return hash;
}

/**
 * @brief Check a stored checksum against a computed one
 *
 * @param stored_checksum An existing checksum in the database
 * @param key The key being used to generate the new checksum
 * @param value The value being used to generate the new checksum
 * @param operation the operation being used to generate the new checksum
 *
 * @return True if the checksums match, false otherwise
 */
bool Hash_Table::verify_checksum(const uint64_t &stored_checksum,
                                 const std::string &key,
                                 const std::string &value,
                                 const uint8_t operation) {

  uint64_t computed_checksum = generate_checksum(key, value, operation);
  return computed_checksum == stored_checksum;
}

/**
 * @brief Create a new entry in the index map
 *
 * @param key The key to be hashed and generated
 * @param offset The value to be generated. The byte offset | log #  in the WAL
 *
 */
void Hash_Table::create_index(const std::string &key, const uint64_t offset) {
  uint64_t key_hash = generate_hash(key);
  lookup_table[key_hash] = offset;
};

/**
 * @brief Update an entry in the index map to point to a new entry
 *
 * @param hash The hash for the existing entry to be modified
 */
void Hash_Table::update_index(const uint64_t hash, const uint64_t offset) {
  lookup_table[hash] = offset;
}

/**
 * @brief Checks for and returns the byte offset | log # for a given key;
 *
 * @param key The key to index from
 *
 * @return The hash if an entry was found
 * @return Nullopt if no entry found
 */
std::optional<uint64_t> Hash_Table::lookup_hash(const std::string &key) {
  uint64_t key_hash = generate_hash(key);
  if (lookup_table.contains(key_hash))
    return lookup_table[key_hash];

  return std::nullopt;
}

/**
 * @brief Orchestrates getting and setting key value pairs
 *
 *
 */

KV_Store::KV_Store() {
  for (const auto &entry :
       std::filesystem::directory_iterator("./database/wal/logs/")) {
    if (entry.path().extension() == ".log") {
      LogSegment segment;
      segment.file_path = entry.path();
      segment.size = (uint64_t)std::filesystem::file_size(entry.path());
      segment.id = std::stoul(entry.path().filename(), nullptr, 10);

      logs.push_back(segment);
    }
  }

  std::sort(logs.begin(), logs.end(),
            [](LogSegment a, LogSegment b) { return a.id < b.id; });
}

/**
 * @brief generate a new log segment in sequential order
 */
void KV_Store::generate_log_segment() {
  unsigned long segment_id = logs.size() + 1;

  std::stringstream filepath;

  filepath << "./database/wal/logs/" << std::setfill('0') << std::setw(5)
           << segment_id << ".log";
  std::ofstream file(filepath.str());

  LogSegment segment;

  segment.size = 0;
  segment.file_path = filepath.str();
  segment.id = segment_id;

  logs.push_back(segment);
}

/**
 * @brief Write a new key into the database
 * @param key The key to write to the log
 * @param value The value to assign to the given key
 *
 */
void KV_Store::set(std::string &key, std::string &value) {

  LogEntryHeader header(value.size(), key.size(), 0x20);
  header.checksum = hash_table.generate_checksum(key, value, 0x20);

  uint64_t record_size = sizeof(LogEntryHeader) + key.size() + value.size();

  if (logs.empty() || record_size + logs.back().size > MAX_SEGMENT_SIZE)
    generate_log_segment();

  LogSegment &cur_log = logs.back();

  std::fstream file(cur_log.file_path,
                    std::ios::in | std::ios::out | std::ios::binary);

  // Encode file and byte offset in index
  uint64_t encode_offset =
      static_cast<uint64_t>(cur_log.size) << 32 |
      static_cast<uint32_t>(std::stoi(cur_log.file_path.stem()));

  hash_table.create_index(key, encode_offset);

  file.seekp(cur_log.size);
  file.write(reinterpret_cast<char *>(&header.checksum),
             sizeof(header.checksum));
  file.write(reinterpret_cast<char *>(&header.value_size),
             sizeof(header.value_size));
  file.write(reinterpret_cast<char *>(&header.key_size),
             sizeof(header.key_size));
  file.write(reinterpret_cast<char *>(&header.operation),
             sizeof(header.operation));
  file.write(key.data(), key.size());
  file.write(value.data(), value.size());

  file.close();

  cur_log.size += sizeof(header) + key.size() + value.size();
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
  uint64_t hash = hash_table.generate_hash(key);

  std::optional<uint64_t> offset = hash_table.lookup_hash(key);

  if (not offset) {
    return std::nullopt;
  }

  uint64_t valid_offset = *offset;

  uint32_t file_offset = static_cast<uint32_t>(valid_offset >> 32);
  uint32_t log_file = static_cast<uint32_t>(valid_offset);

  std::stringstream filepath;

  filepath << "./database/wal/logs/" << std::setfill('0') << std::setw(5)
           << log_file << ".log";

  std::fstream file(filepath.str(),
                    std::ios::binary | std::ios::in | std::ios::out);

  file.seekg(file_offset);

  // Read back the stored checksum
  uint64_t stored_checksum;
  uint32_t value_size;
  file.read(reinterpret_cast<char *>(&stored_checksum),
            sizeof(stored_checksum));
  file.read(reinterpret_cast<char *>(&value_size), sizeof(value_size));

  // Skip to value
  file.seekg(sizeof(uint32_t) + sizeof(uint8_t) + key.size(), std::ios::cur);

  std::string value;
  value.resize(value_size);
  file.read(value.data(), value_size);
  file.close();

  if (hash_table.verify_checksum(stored_checksum, key, value, 0x20))
    return value;
  else if (hash_table.verify_checksum(stored_checksum, key, value, 0x30))
    return "Key not found";

  throw ReadException("Record checksum invalid for key: " + key);
};

bool KV_Store::remove(std::string &key) {
  uint64_t hash = hash_table.generate_hash(key);

  std::optional<uint64_t> offset = hash_table.lookup_hash(key);
  if (not offset) {
    return false;
  }

  LogEntryHeader header(0, key.size(), 0x30);
  header.checksum = hash_table.generate_checksum(key, "", 0x30);

  uint64_t record_size = sizeof(LogEntryHeader) + key.size() + 0;

  if (logs.empty() || record_size + logs.back().size > MAX_SEGMENT_SIZE)
    generate_log_segment();

  LogSegment &cur_log = logs.back();

  std::fstream file(cur_log.file_path,
                    std::ios::in | std::ios::out | std::ios::binary);

  // Encode file and byte offset in index
  uint64_t encode_offset =
      static_cast<uint64_t>(cur_log.size) << 32 |
      static_cast<uint32_t>(std::stoi(cur_log.file_path.stem()));

  file.seekp(cur_log.size);
  file.write(reinterpret_cast<char *>(&header.checksum),
             sizeof(header.checksum));
  file.write(reinterpret_cast<char *>(&header.value_size),
             sizeof(header.value_size));
  file.write(reinterpret_cast<char *>(&header.key_size),
             sizeof(header.key_size));
  file.write(reinterpret_cast<char *>(&header.operation),
             sizeof(header.operation));
  file.write(key.data(), key.size());
  cur_log.size += sizeof(header) + key.size() + 0;

  hash_table.update_index(hash, encode_offset);

  return true;
}
