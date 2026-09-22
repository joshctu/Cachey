#pragma once

#include "segment.hpp"
#include "xxhash.h"
#include <cstdint>
#include <filesystem>

static constexpr uint64_t SERIALIZED_HINTS_FOOTER_SIZE = 24;

struct HintRecord {
  uint64_t record_length;
  uint32_t segment_id;
  uint32_t file_offset;
  uint32_t key_size;
  std::string key;

  HintRecord(const std::string &key = "", const uint32_t segment_id = 0,
             const uint32_t file_offset = 0)
      : segment_id(segment_id), file_offset(file_offset), key(key) {
    key_size = key.size();
    record_length = sizeof(record_length) + sizeof(segment_id) +
                    sizeof(file_offset) + sizeof(key_size) + key_size;
  };
};

class HintManager {
private:
  uint64_t file_ptr = 0;
  uint64_t file_size = 0;
  uint64_t record_count = 0;
  std::filesystem::path file_path;
  XXH64_state_t *const state = XXH64_createState();

public:
  HintManager(std::filesystem::path root_path);

  bool validateChecksum();
  void removeHintDir();
  bool nextValidHint(Cursor &c, HintRecord &rec);
  bool isValidHintFile();
  void writeHint(const std::string &key, const uint32_t &segment_id,
                 const uint32_t &file_offset);
  void writeFooter();
};
