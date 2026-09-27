#include "hint_manager.hpp"
#include "constants.hpp"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

HintManager::HintManager(std::filesystem::path root_path) {
  std::filesystem::create_directories(root_path / "database/hints/");
  file_path = std::filesystem::path(root_path / "database/hints/hint.txt");

  if (std::filesystem::exists(file_path)) {
    std::ifstream hint_file(file_path, std::ios::in | std::ios::binary);
    hint_file.seekg(-(sizeof(uint64_t) * 3), std::ios::end);
    hint_file.read(reinterpret_cast<char *>(&file_size), sizeof(file_size));
    hint_file.read(reinterpret_cast<char *>(&record_count),
                   sizeof(record_count));

    hint_file.close();
  } else {
    std::ofstream hint_file(file_path);
    hint_file.close();
  }
  XXH64_reset(state, HASH_SEED);
};

void HintManager::removeHintFile() { std::filesystem::remove_all(file_path); }

/**
 * @brief This function provides a quick way to determine if a boot from the
 * hint file should be attempted. This function says nothing about the validity
 * of the hint file checksum.
 *
 * @return True if we can attempt to boot from hints, false otherwise.
 */
bool HintManager::isValidHintFile() {
  return std::filesystem::exists(file_path) and
         std::filesystem::file_size(file_path) > SERIALIZED_HINTS_FOOTER_SIZE;
}

void HintManager::writeHint(const std::string &key, const uint32_t &segment_id,
                            const uint32_t &file_offset) {

  std::fstream hint_file(file_path,
                         std::ios::in | std::ios::out | std::ios::binary);

  if (!hint_file.is_open()) {
    return;
  }

  HintRecord rec(key, segment_id, file_offset);
  hint_file.seekp(file_ptr, std::ios::beg);
  hint_file.write(reinterpret_cast<const char *>(&rec.record_length),
                  sizeof(rec.record_length));
  XXH64_update(state, &rec.record_length, sizeof(rec.record_length));
  hint_file.write(reinterpret_cast<const char *>(&rec.segment_id),
                  sizeof(rec.segment_id));
  XXH64_update(state, &rec.segment_id, sizeof(rec.segment_id));
  hint_file.write(reinterpret_cast<const char *>(&rec.file_offset),
                  sizeof(rec.file_offset));
  XXH64_update(state, &rec.file_offset, sizeof(rec.file_offset));
  hint_file.write(reinterpret_cast<const char *>(&rec.key_size),
                  sizeof(rec.key_size));
  XXH64_update(state, &rec.key_size, sizeof(rec.key_size));
  hint_file.write(rec.key.data(), rec.key_size);
  XXH64_update(state, rec.key.data(), rec.key_size);

  file_ptr = hint_file.tellp();
  file_size += rec.record_length;
  record_count += 1;

  hint_file.close();
}

bool HintManager::nextValidHint(Cursor &c, HintRecord &rec) {

  std::ifstream hint_file(file_path, std::ios::in | std::ios::binary);

  c.file_offset = c.next_record;

  if (c.file_offset >= file_size) {
    return false;
  }

  hint_file.seekg(c.file_offset, std::ios::beg);
  hint_file.read(reinterpret_cast<char *>(&rec.record_length),
                 sizeof(rec.record_length));
  XXH64_update(state, &rec.record_length, sizeof(rec.record_length));
  hint_file.read(reinterpret_cast<char *>(&rec.segment_id),
                 sizeof(rec.segment_id));
  XXH64_update(state, &rec.segment_id, sizeof(rec.segment_id));
  hint_file.read(reinterpret_cast<char *>(&rec.file_offset),
                 sizeof(rec.file_offset));
  XXH64_update(state, &rec.file_offset, sizeof(rec.file_offset));
  hint_file.read(reinterpret_cast<char *>(&rec.key_size), sizeof(rec.key_size));
  XXH64_update(state, &rec.key_size, sizeof(rec.key_size));

  rec.key.resize(rec.key_size);
  hint_file.read(reinterpret_cast<char *>(rec.key.data()), rec.key_size);
  XXH64_update(state, rec.key.data(), rec.key_size);

  c.next_record += rec.record_length;
  return true;
}

bool HintManager::validateChecksum() {
  std::ifstream hint_file(file_path, std::ios::in | std::ios::binary);

  XXH64_update(state, &file_size, sizeof(file_size));
  XXH64_update(state, &record_count, sizeof(record_count));

  uint64_t computed_hash = XXH64_digest(state);
  uint64_t stored_hash;

  hint_file.seekg(-sizeof(uint64_t), std::ios::end);
  hint_file.read(reinterpret_cast<char *>(&stored_hash), sizeof(stored_hash));

  hint_file.close();
  return stored_hash == computed_hash;
}

void HintManager::writeFooter() {
  std::fstream hint_file(file_path,
                         std::ios::in | std::ios::out | std::ios::binary);

  hint_file.seekp(file_ptr, std::ios::beg);

  hint_file.write(reinterpret_cast<const char *>(&file_size),
                  sizeof(file_size));
  XXH64_update(state, &file_size, sizeof(file_size));
  hint_file.write(reinterpret_cast<const char *>(&record_count),
                  sizeof(record_count));
  XXH64_update(state, &record_count, sizeof(record_count));

  uint64_t const hash = XXH64_digest(state);
  hint_file.write(reinterpret_cast<const char *>(&hash), sizeof(hash));

  hint_file.close();
}
