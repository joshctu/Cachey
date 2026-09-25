#include "segment.hpp"
#include "constants.hpp"
#include "exceptions.hpp"
#include <fstream>
#include <hash.hpp>
#include <ios>
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
Segment::Segment(uint32_t id, uint32_t size, std::filesystem::path file_path)
    : id(id), size(size), file_path(file_path) {
  if (!std::filesystem::exists(file_path)) {
    std::ofstream create(file_path, std::ios::binary);

    std::streamoff footer_offset =
        static_cast<std::streamoff>(size) -
        static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);

    footer_offset += sizeof(decltype(SegmentFooter::magic)) +
                     sizeof(decltype(SegmentFooter::bytes_written));

    create.seekp(footer_offset, std::ios::beg);

    uint32_t dead_bytes = 0;
    create.write(reinterpret_cast<const char *>(&dead_bytes),
                 sizeof(dead_bytes));
    create.seekp(sizeof(decltype(SegmentFooter::entry_count)), std::ios::cur);
    create.write(reinterpret_cast<char *>(&id), sizeof(id));
    create.close();
    return;
  } else {
    std::ifstream segment_file(file_path, std::ios::binary);
    uint32_t magic;

    segment_file.seekg(-static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE),
                       std::ios::end);
    segment_file.read(reinterpret_cast<char *>(&magic), sizeof(magic));

    if (magic == MAGIC)
      isSealed = true;

    segment_file.read(reinterpret_cast<char *>(&used), sizeof(used));
    segment_file.seekg(sizeof(decltype(SegmentFooter::dead_bytes)),
                       std::ios::cur);
    segment_file.read(reinterpret_cast<char *>(&entry_count),
                      sizeof(entry_count));

    segment_file.close();
  }
}

/**
 * @brief Replay all the records currently in this segment. Add all records back
 * to index map and set segment metadata.
 *
 * @param on_record Lambda passed from the store. Takes the key, segment_id, and
 * file offset to set the record in the index map
 */
uint32_t Segment::replay(const ReplayHandler &on_record) {
  std::ifstream segment_file(file_path, std::ios::in | std::ios::binary);

  if (!segment_file) {
    return 0;
  }

  std::streamoff file_limit =
      static_cast<std::streamoff>(size) -
      static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);

  std::streamoff offset = 0;
  uint32_t records_read = 0;
  bool bad_read = false;

  while (offset < file_limit) {
    uint32_t record_length;
    uint64_t stored_checksum;
    uint32_t key_size;
    uint32_t value_size;
    uint8_t operation;

    int bytes_read = 0;
    segment_file.read(reinterpret_cast<char *>(&record_length),
                      sizeof(record_length));
    bytes_read += segment_file.gcount();
    segment_file.read(reinterpret_cast<char *>(&stored_checksum),
                      sizeof(stored_checksum));
    bytes_read += segment_file.gcount();

    segment_file.read(reinterpret_cast<char *>(&key_size), sizeof(key_size));
    bytes_read += segment_file.gcount();

    segment_file.read(reinterpret_cast<char *>(&value_size),
                      sizeof(value_size));
    bytes_read += segment_file.gcount();

    segment_file.read(reinterpret_cast<char *>(&operation), sizeof(operation));
    bytes_read += segment_file.gcount();

    // Check if the read record length is a valid size. This acts as a magic
    // number for the record.
    if (record_length > file_limit ||
        record_length < (SERIALIZED_HEADER_SIZE + sizeof(uint32_t)) + key_size +
                            value_size) {
      offset += bytes_read;
      continue;
    }

    if (bytes_read == 0) {
      segment_file.close();
      break;
    }

    std::string key;
    std::string value;

    key.resize(key_size);
    value.resize(value_size);

    segment_file.read(key.data(), key.size());
    bytes_read += segment_file.gcount();
    segment_file.read(value.data(), value.size());
    bytes_read += segment_file.gcount();

    /*If we hit a bad record we have to init the segment at the current offset
      and number of records read. Any additional writes to the segment will
      overwrite the bad record.
    */
    if (!verifyChecksum(stored_checksum, key, value, operation)) {
      used = offset;
      entry_count = records_read;
      bad_read = true;
      break;
    } else {
      on_record(key, id, offset);
      records_read += 1;
      offset += bytes_read;
    }
  }

  segment_file.clear();
  segment_file.seekg(file_limit, std::ios::beg);
  uint32_t magic;
  segment_file.read(reinterpret_cast<char *>(&magic), sizeof(magic));

  if (!bad_read) {
    segment_file.read(reinterpret_cast<char *>(&used), sizeof(used));

    segment_file.seekg(sizeof(decltype(SegmentFooter::dead_bytes)),
                       std::ios::cur);

    segment_file.read(reinterpret_cast<char *>(&entry_count),
                      sizeof(entry_count));
  }

  if (magic == MAGIC)
    isSealed = true;

  segment_file.close();
  return entry_count;
}

/**
 * @brief Generate a checksum for a db entry
 * @param key The key being used
 * @param value The value being used
 * @param operation The operation being performed
 *
 * @return The resulting hash value
 */
uint64_t Segment::generateRecordChecksum(const std::string &key,
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

  uint64_t computed_checksum = generateRecordChecksum(key, value, operation);
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
 * @brief Reads the size of the record at the provided offset and adds it to the
 * sum of invalidated bytes in this segment.
 *
 * @param file_offset The offset into the segment for the record that has been
 * invalidated
 */
void Segment::invalidateBytes(const uint32_t file_offset) {
  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  file.seekg(file_offset, std::ios::beg);

  uint32_t record_length;
  file.read(reinterpret_cast<char *>(&record_length), sizeof(record_length));

  // footer start
  std::streamoff footer_offset =
      static_cast<std::streamoff>(size) -
      static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);

  // skip to dead bytes
  footer_offset += static_cast<std::streamoff>(
      sizeof(decltype(SegmentFooter::magic)) +
      sizeof(decltype(SegmentFooter::bytes_written)));

  file.seekg(footer_offset, std::ios::beg);

  uint32_t dead_bytes;
  file.read(reinterpret_cast<char *>(&dead_bytes), sizeof(dead_bytes));

  dead_bytes += record_length;
  file.seekp(footer_offset, std::ios::beg);
  file.write(reinterpret_cast<char *>(&dead_bytes), sizeof(dead_bytes));

  file.close();
}

bool Segment::nextValidRecord(Cursor &c, SegmentRecordView &rec) {

  if (c.next_record >= used)
    return false;

  c.file_offset = c.next_record;

  SegmentEntryHeader header{};
  std::ifstream file(file_path, std::ios::in | std::ios::binary);

  file.seekg(c.next_record, std::ios::beg);
  file.read(reinterpret_cast<char *>(&rec.length), sizeof(rec.length));
  file.read(reinterpret_cast<char *>(&header.checksum),
            sizeof(header.checksum));
  file.read(reinterpret_cast<char *>(&header.key_size),
            sizeof(header.key_size));
  file.read(reinterpret_cast<char *>(&header.value_size),
            sizeof(header.value_size));
  file.read(reinterpret_cast<char *>(&header.operation),
            sizeof(header.operation));

  rec.key.resize(header.key_size);
  rec.value.resize(header.value_size);

  file.read(reinterpret_cast<char *>(rec.key.data()), header.key_size);
  file.read(reinterpret_cast<char *>(rec.value.data()), header.value_size);

  rec.header = header;

  if (!verifyChecksum(header.checksum, rec.key, rec.value, header.operation)) {
    std::cout << "We actually hit an invalid record" << std::endl;
    return false;
  }

  c.next_record += rec.length;
  return true;
}

void Segment::iterateCurrentRecords(const HintHandler &commitRecord,
                                    const IndexHandler &is_latest_record) {

  std::ifstream segment_file(file_path, std::ios::binary);

  if (!segment_file) {
    segment_file.close();
    return;
  }

  std::streamoff offset = 0;

  while (offset < used) {

    uint32_t record_length;
    uint32_t key_size;
    uint32_t value_size;
    std::string key;

    segment_file.read(reinterpret_cast<char *>(&record_length),
                      sizeof(record_length));
    segment_file.seekg(sizeof(decltype(SegmentEntryHeader::checksum)),
                       std::ios::cur);

    segment_file.read(reinterpret_cast<char *>(&key_size), sizeof(key_size));
    segment_file.read(reinterpret_cast<char *>(&value_size),
                      sizeof(value_size));
    segment_file.seekg(sizeof(decltype(SegmentEntryHeader::operation)),
                       std::ios::cur);

    key.resize(key_size);
    segment_file.read(reinterpret_cast<char *>(key.data()), key_size);
    segment_file.seekg(value_size, std::ios::cur);

    if (is_latest_record(key.data(), id, offset)) {
      commitRecord(key, id, offset);
    }

    offset += record_length;
  }

  segment_file.close();
}

/**
 * @brief Write a key/value to pair to the end of the segment
 *
 * @param key The key to write
 * @param value The value to write
 */
void Segment::append(const std::string &key, const std::string &value) {

  SegmentEntryHeader header(key.size(), value.size(), 0x20);
  uint32_t record_length = sizeof(decltype(SegmentRecord::length)) +
                           SERIALIZED_HEADER_SIZE + key.size() + value.size();
  header.checksum = generateRecordChecksum(key, value, 0x20);
  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  file.seekp(used);
  file.write(reinterpret_cast<char *>(&record_length), sizeof(record_length));
  file.write(reinterpret_cast<char *>(&header.checksum),
             sizeof(header.checksum));
  file.write(reinterpret_cast<char *>(&header.key_size),
             sizeof(header.key_size));
  file.write(reinterpret_cast<char *>(&header.value_size),
             sizeof(header.value_size));
  file.write(reinterpret_cast<char *>(&header.operation),
             sizeof(header.operation));
  file.write(key.data(), header.key_size);
  file.write(value.data(), header.value_size);

  used += record_length;
  entry_count += 1;

  std::streamoff footer_offset =
      static_cast<std::streamoff>(size) -
      static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);
  file.seekp(footer_offset, std::ios::beg);
  // Skip magic
  file.seekp(sizeof(decltype(SegmentFooter::magic)), std::ios::cur);

  file.write(reinterpret_cast<char *>(&used), sizeof(used));
  // Skip dead bytes
  file.seekp(sizeof(decltype(SegmentFooter::dead_bytes)), std::ios::cur);

  file.write(reinterpret_cast<char *>(&entry_count), sizeof(entry_count));
  file.close();
}

/**
 * @brief Read a value back from an entry in the segment
 *
 * @param key The key of the entry to find
 * @param offset The byte offset into this segment
 *
 * @return The value if found, nullopt if nothing found
 */
std::optional<const std::string> Segment::read(const std::string &key,
                                               uint32_t const offset) {
  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  file.seekg(offset);

  // Read back the stored checksum
  uint64_t stored_checksum;
  uint32_t value_size;

  // Skip the record length here. It's not relevant in this function.
  file.seekg(sizeof(uint32_t), std::ios::cur);

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
 * @brief Returns the current number of dead bytes for this segment
 *
 * @return An unsigned 32bit integer representing the number of dead bytes in
 * this segment.
 */
uint32_t Segment::readDeadBytes() {
  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  std::streamoff footer_offset =
      static_cast<std::streamoff>(size) -
      static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);

  uint32_t dead_bytes;
  footer_offset += sizeof(decltype(SegmentFooter::magic)) +
                   sizeof(decltype(SegmentFooter::bytes_written));
  file.seekg(footer_offset);
  file.read(reinterpret_cast<char *>(&dead_bytes), sizeof(dead_bytes));

  file.close();
  return dead_bytes;
}

/**
 * @brief "Remove" an entry by adding a tombstone for the given key
 *
 * @param key The key to be removed
 */
void Segment::remove(const std::string &key) {

  SegmentEntryHeader header(key.size(), 0, 0x30);
  uint32_t record_length = sizeof(decltype(SegmentRecord::length)) +
                           SERIALIZED_HEADER_SIZE + key.size();
  header.checksum = generateRecordChecksum(key, "", 0x30);

  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  file.seekp(used);
  file.write(reinterpret_cast<char *>(&record_length), sizeof(record_length));
  file.write(reinterpret_cast<const char *>(&header.checksum),
             sizeof(header.checksum));
  file.write(reinterpret_cast<const char *>(&header.key_size),
             sizeof(header.key_size));
  file.write(reinterpret_cast<const char *>(&header.value_size),
             sizeof(header.value_size));
  file.write(reinterpret_cast<const char *>(&header.operation),
             sizeof(header.operation));
  file.write(key.data(), key.size());

  used += record_length;
  entry_count += 1;

  std::streamoff footer_offset =
      static_cast<std::streamoff>(size) -
      static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);
  file.seekp(footer_offset);
  // Skip magic
  file.seekp(sizeof(decltype(SegmentFooter::magic)), std::ios::cur);

  file.write(reinterpret_cast<char *>(&used), sizeof(used));
  // Skip dead bytes
  file.seekp(sizeof(decltype(SegmentFooter::dead_bytes)), std::ios::cur);

  file.write(reinterpret_cast<char *>(&entry_count), sizeof(entry_count));
  file.close();
}

void Segment::seal() {
  std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);

  std::streamoff offset = static_cast<std::streamoff>(size) -
                          static_cast<std::streamoff>(SERIALIZED_FOOTER_SIZE);
  file.seekp(offset, std::ios::beg);
  file.write(reinterpret_cast<const char *>(&MAGIC), sizeof(MAGIC));
  file.write(reinterpret_cast<char *>(&used), sizeof(used));
  // Skip over dead bytes field
  file.seekp(sizeof(decltype(SegmentFooter::dead_bytes)), std::ios::cur);
  file.write(reinterpret_cast<char *>(&entry_count), sizeof(entry_count));

  isSealed = true;
  file.close();
}

/**
 * @brief This function simply deletes the associated log file
 */
void Segment::deleteFile() { std::filesystem::remove(file_path); }
