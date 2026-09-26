#include "manifest.hpp"
#include <filesystem>
#include <unordered_map>

ManifestManager::ManifestManager(std::filesystem::path root_dir) {
  std::filesystem::create_directories(root_dir / "database" / "manifest");
  file_path = std::filesystem::path(root_dir / "database" / "manifest" /
                                    "manifest.txt");

  if (!std::filesystem::exists(file_path)) {
    std::ofstream create(file_path);
    create.close();
  }
  manifest_file.open(file_path, std::ios::in | std::ios::out | std::ios::app |
                                    std::ios::binary);
  used = manifest_file.seekg(0, std::ios::end).tellg();
}

void ManifestManager::writeInvalidateBytesEvent(const uint32_t segment_id,
                                                const uint32_t dead_bytes) {

  const uint8_t operation_type = 0x1;
  manifest_file.write(reinterpret_cast<const char *>(&operation_type),
                      sizeof(operation_type));
  manifest_file.write(reinterpret_cast<const char *>(&segment_id),
                      sizeof(segment_id));
  manifest_file.write(reinterpret_cast<const char *>(&dead_bytes),
                      sizeof(dead_bytes));

  used += SERIALIZED_MANIFEST_RECORD_SIZE;
}

std::unordered_map<uint32_t, uint32_t>
ManifestManager::getInvalidBytesTotals() {
  std::unordered_map<uint32_t, uint32_t> invalid_bytes_per_segment;

  std::streamoff offset = 0;

  while (offset < used) {
    uint8_t operation;
    uint32_t segment_id;
    uint32_t dead_bytes;

    manifest_file.seekg(offset, std::ios::beg);
    manifest_file.read(reinterpret_cast<char *>(&operation), sizeof(operation));
    manifest_file.read(reinterpret_cast<char *>(&segment_id),
                       sizeof(segment_id));
    manifest_file.read(reinterpret_cast<char *>(&dead_bytes),
                       sizeof(dead_bytes));

    if (operation == 0x1)
      invalid_bytes_per_segment[segment_id] += dead_bytes;

    offset += SERIALIZED_MANIFEST_RECORD_SIZE;
  }

  return invalid_bytes_per_segment;
}
