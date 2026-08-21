#include "segment.hpp"
#include "exceptions.hpp"
#include <fstream>
#include <hash.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <xxhash.h>

Segment::Segment(uint32_t id, uint32_t size, std::filesystem::path file_path,
                 uint8_t HASH_SEED)
    : id(id), size(size), file_path(file_path), HASH_SEED(HASH_SEED) {
  std::ofstream SegmentFile(file_path);
  SegmentFile.close();
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
  return isActive && entry_size <= size - used;
}

/**
 * @brief Write a key/value to pair to the end of the segment
 *
 * @param key The key to write
 * @param value The value to write
 */
void Segment::append(std::string &key, std::string &value) {

  SegmentEntryHeader header(value.size(), key.size(), 0x20);
  header.checksum = generateChecksum(key, value, 0x20);
  uint64_t record_size = sizeof(SegmentEntryHeader) + key.size() + value.size();

  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  file.seekp(used);
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

  used += sizeof(header) + key.size() + value.size();
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
  std::fstream file(file_path, std::ios::binary | std::ios::in | std::ios::out);

  file.seekg(offset);
  std::cout << offset << std::endl;

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

  std::cout << value << std::endl;

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

  SegmentEntryHeader header(0, key.size(), 0x30);
  header.checksum = generateChecksum(key, "", 0x30);

  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  // Encode file and byte offset in index
  uint64_t encode_offset = static_cast<uint64_t>(size) << 32 |
                           static_cast<uint32_t>(std::stoi(file_path.stem()));

  file.seekp(used);
  file.write(reinterpret_cast<char *>(&header.checksum),
             sizeof(header.checksum));
  file.write(reinterpret_cast<char *>(&header.value_size),
             sizeof(header.value_size));
  file.write(reinterpret_cast<char *>(&header.key_size),
             sizeof(header.key_size));
  file.write(reinterpret_cast<char *>(&header.operation),
             sizeof(header.operation));
  file.write(key.data(), key.size());
  used += sizeof(header) + key.size() + 0;
}
