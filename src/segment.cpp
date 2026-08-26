#include "segment.hpp"
#include "exceptions.hpp"
#include <fstream>
#include <hash.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <xxhash.h>

/**
 * @brief Manage each database segment. Constructor should init new segment file
 * if none found. Otherwise it should init all it's fields using existing log
 * data.
 *
 * @param id The segment_id for this segment.
 * @param size The total size of this segment in bytes.
 * @param file_path Path to the log file this segment manages from project root
 * @param HASH_SEED The hash seed this segment should use to generate checksums.
 */
Segment::Segment(uint32_t id, uint32_t size, std::filesystem::path file_path,
                 uint8_t HASH_SEED)
    : id(id), size(size), file_path(file_path), HASH_SEED(HASH_SEED) {
  std::ofstream segmentFile(file_path);
  segmentFile.close();
}

void Segment::replay(const RecordHandler &on_record) {
  std::ifstream segmentFile(file_path, std::ios::binary);

  if (!segmentFile) {
    segmentFile.close();
    return;
  }

  std::streamoff file_limit =
      static_cast<std::streamoff>(MAX_SEGMENT_SIZE) -
      static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);

  std::streamoff offset = 0;

  while (offset < file_limit) {

    uint64_t stored_checksum;
    uint32_t key_size;
    uint32_t value_size;
    uint8_t operation;
    segmentFile.read(reinterpret_cast<char *>(&stored_checksum),
                     sizeof(stored_checksum));
    segmentFile.read(reinterpret_cast<char *>(&key_size), sizeof(key_size));
    segmentFile.read(reinterpret_cast<char *>(&value_size), sizeof(value_size));
    segmentFile.read(reinterpret_cast<char *>(&operation), sizeof(operation));

    if (segmentFile.gcount() == 0)
      break;

    if (segmentFile.gcount() != SERIALIZED_HEADER_SIZE) {
      throw ReadException("Incomplete record header");
    }

    std::string key;
    std::string value;

    key.resize(key_size);
    value.resize(value_size);

    segmentFile.read(key.data(), key.size());
    segmentFile.read(value.data(), value.size());

    if (!verifyChecksum(stored_checksum, key, value, operation)) {
      std::cout << "Checksum error" << std::endl;
      throw ReadException("Data corrupted");
    }

    on_record(key, id, offset);

    offset += static_cast<std::streamoff>(key.size()) +
              static_cast<std::streamoff>(value.size()) +
              static_cast<std::streamoff>(SERIALIZED_HEADER_SIZE);
  }

  segmentFile.seekg(file_limit, std::ios::beg);
  uint32_t magic;
  segmentFile.read(reinterpret_cast<char *>(&magic), sizeof(magic));
  if (magic == MAGIC)
    isSealed = true;
}

/**
 * @brief Generate a checksum for a db entry
 * @param key The key being used
 * @param value The value being used
 * @param operation The operation being performed
 *
 * @return The resulting hash value
 */
uint64_t Segment::generateChecksum(const std::string &key,
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
bool Segment::verifyChecksum(const uint64_t &stored_checksum,
                             const std::string &key, const std::string &value,
                             const uint8_t operation) {

  uint64_t computed_checksum = generateChecksum(key, value, operation);
  return computed_checksum == stored_checksum;
}

/**
 * @brief Determine if a record can be added to this segment
 *
 * @param entry_size The size of the entry to add
 *
 * @return True is the record can be added, false otherwise
 */
bool Segment::canAppend(uint32_t const entry_size) {
  return !isSealed && entry_size <= size - used - SERIALIZED_FOOTER_SIZE;
}

/**
 * @brief Write a key/value to pair to the end of the segment
 *
 * @param key The key to write
 * @param value The value to write
 */
void Segment::append(std::string &key, std::string &value) {

  SegmentEntryHeader header(key.size(), value.size(), 0x20);
  header.checksum = generateChecksum(key, value, 0x20);
  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  file.seekp(used);
  file.write(reinterpret_cast<char *>(&header.checksum),
             sizeof(header.checksum));
  file.write(reinterpret_cast<char *>(&header.key_size),
             sizeof(header.key_size));
  file.write(reinterpret_cast<char *>(&header.value_size),
             sizeof(header.value_size));
  file.write(reinterpret_cast<char *>(&header.operation),
             sizeof(header.operation));
  file.write(key.data(), key.size());
  file.write(value.data(), value.size());

  file.close();
  used += SERIALIZED_HEADER_SIZE + key.size() + value.size();
  entry_count += 1;
}

/**
 * @brief Read a value back from an entry in the segment
 *
 * @param key The key of the entry to find
 * @param offset The byte offset into this segment
 *
 * @return The value if found, nullopt if nothing found
 */
std::optional<std::string> Segment::read(std::string &key,
                                         uint32_t const offset) {
  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  file.seekg(offset);

  // Read back the stored checksum
  uint64_t stored_checksum;
  uint32_t value_size;

  file.read(reinterpret_cast<char *>(&stored_checksum),
            sizeof(stored_checksum));
  file.seekg(sizeof(uint32_t), std::ios::cur);
  file.read(reinterpret_cast<char *>(&value_size), sizeof(value_size));

  // Skip to value
  file.seekg(sizeof(uint8_t) + key.size(), std::ios::cur);

  std::string value;
  value.resize(value_size);
  file.read(value.data(), value_size);

  file.close();

  if (verifyChecksum(stored_checksum, key, value, 0x20))
    return value;

  // Tombstone found
  if (verifyChecksum(stored_checksum, key, value, 0x30))
    return "No record found";

  throw ReadException("Checksum invalid");
}

/**
 * @brief "Remove" an entry by adding a tombstone for the given key
 *
 * @param key The key to be removed
 */
void Segment::remove(std::string &key) {

  SegmentEntryHeader header(key.size(), 0, 0x30);
  header.checksum = generateChecksum(key, "", 0x30);

  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  file.seekp(used);
  file.write(reinterpret_cast<const char *>(&header.checksum),
             sizeof(header.checksum));
  file.write(reinterpret_cast<const char *>(&header.key_size),
             sizeof(header.key_size));
  file.write(reinterpret_cast<const char *>(&header.value_size),
             sizeof(header.value_size));
  file.write(reinterpret_cast<const char *>(&header.operation),
             sizeof(header.operation));
  file.write(key.data(), key.size());
  used += SERIALIZED_HEADER_SIZE + key.size() + 0;

  file.close();
  entry_count += 1;
}

void Segment::seal() {
  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  std::cout << "file: " << file_path.stem() << "sealed with " << used
            << " bytes remaining" << std::endl;
  std::streamoff offset = static_cast<std::streamoff>(MAX_SEGMENT_SIZE) -
                          static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);
  file.seekp(offset);
  file.write(reinterpret_cast<const char *>(&MAGIC), sizeof(MAGIC));
  file.write(reinterpret_cast<char *>(&used), sizeof(used));
  file.write(reinterpret_cast<const char *>(&entry_count), sizeof(entry_count));

  used += offset + SERIALIZED_FOOTER_SIZE;

  isSealed = true;
  file.close();
}
