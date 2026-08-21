#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

constexpr uint32_t MAX_SEGMENT_SIZE = 256 * 1024 * 1024;

// 0x10 -> GET, 0x20 -> SET, 0x30 -> DELETE
struct SegmentEntryHeader {
  uint64_t checksum;
  uint32_t value_size;
  uint32_t key_size;
  uint8_t operation;

  SegmentEntryHeader(uint32_t value_size, uint32_t key_size, uint8_t operation)
      : key_size(key_size), value_size(value_size), operation(operation) {}
};

class Segment {
public:
  uint8_t HASH_SEED;
  uint32_t id;
  uint32_t size;
  uint32_t used = 0;
  bool isActive = true;
  std::filesystem::path file_path;

public:
  Segment(uint32_t id, uint32_t size, std::filesystem::path file_path,
          uint8_t HASH_SEED);

public:
  uint64_t generateChecksum(const std::string &key, const std::string &value,
                            uint8_t operation);
  bool verifyChecksum(const uint64_t &stored_checksum, const std::string &key,
                      const std::string &value, const uint8_t operation);
  bool canAppend(uint32_t const entry_size);
  void append(std::string &key, std::string &value);
  std::optional<std::string> read(std::string &key, uint32_t const offset);
  void remove(std::string &key);
};
