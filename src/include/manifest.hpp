#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <unordered_map>

static constexpr uint32_t SERIALIZED_MANIFEST_RECORD_SIZE = 9;

struct ManifestEvent {
  // 0x1 - invalidate bytes
  uint8_t event_type;
  uint32_t segment_id;
  uint32_t dead_bytes;
};

class ManifestManager {
private:
  std::filesystem::path file_path;
  std::fstream manifest_file;
  uint32_t used = 0;

public:
  ManifestManager(std::filesystem::path root_dir);

  void writeInvalidateBytesEvent(const uint32_t segment_id,
                                 const uint32_t dead_bytes);

  std::unordered_map<uint32_t, uint32_t> getInvalidBytesTotals();
};
