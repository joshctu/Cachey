#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

constexpr uint32_t MAX_SEGMENT_SIZE = 256;
constexpr uint32_t SERIALIZED_HEADER_SIZE = 17;
constexpr uint32_t SERIALIZED_FOOTER_SIZE = 10;
constexpr uint32_t MAGIC = 0xDEAD2BAD;

struct SegmentFooter {
  uint32_t magic;
  uint32_t bytes_written;
  uint16_t entry_count;
};

// 0x10 -> GET, 0x20 -> SET, 0x30 -> DELETE
struct SegmentEntryHeader {
  uint64_t checksum;
  uint32_t key_size;
  uint32_t value_size;
  uint8_t operation;

  SegmentEntryHeader(uint32_t key_size = 0, uint32_t value_size = 0,
                     uint8_t operation = 0x0)
      : key_size(key_size), value_size(value_size), operation(operation) {}
};

struct SegmentRecord {
  SegmentEntryHeader header;
  std::string key;
  std::string value;

  SegmentRecord(SegmentEntryHeader header, std::string key, std::string value)
      : header(header), key(key), value(value) {}
};

class Segment {
public:
  using RecordHandler = std::function<void(
      const std::string &key, uint32_t segment_id, uint32_t file_offset)>;

public:
  uint8_t HASH_SEED;
  uint32_t id;
  uint32_t size;
  uint32_t used = 0;
  uint16_t entry_count = 0;
  bool isSealed = false;
  std::filesystem::path file_path;

public:
  Segment(uint32_t id, uint32_t size, std::filesystem::path file_path,
          uint8_t HASH_SEED);

public:
  void replay(const RecordHandler &on_record);
  uint64_t generateChecksum(const std::string &key, const std::string &value,
                            uint8_t operation);
  void seal();
  bool verifyChecksum(const uint64_t &stored_checksum, const std::string &key,
                      const std::string &value, const uint8_t operation);
  bool canAppend(uint32_t const entry_size);
  void append(std::string &key, std::string &value);
  std::optional<std::string> read(std::string &key, uint32_t const offset);
  void remove(std::string &key);
};
