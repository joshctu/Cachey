#pragma once

#include <filesystem>
#include <stdint.h>

// 256 MB
constexpr uint32_t MAX_SEGMENT_SIZE = 256 * 1024 * 1024;

struct LogSegment {
  unsigned long id;
  std::filesystem::path file_path;
  uint32_t size;
};

// 0x10 -> GET, 0x20 -> SET, 0x30 -> DELETE
struct LogEntryHeader {
  uint64_t checksum;
  uint32_t value_size;
  uint32_t key_size;
  uint8_t operation;

  LogEntryHeader(uint32_t value_size, uint32_t key_size, uint8_t operation)
      : key_size(key_size), value_size(value_size), operation(operation) {}
};
