#include "manifest.hpp"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

ManifestManager::ManifestManager(std::filesystem::path root_dir)
    : root_dir(root_dir) {
  std::filesystem::create_directories(root_dir / "database" / "manifest");
  std::filesystem::path manifest_file_path = std::filesystem::path(
      root_dir / "database" / "manifest" / "manifest.txt");

  if (!std::filesystem::exists(manifest_file_path)) {
    std::ofstream create(manifest_file_path);
    create.close();
  }
  manifest_file.open(manifest_file_path, std::ios::in | std::ios::out |
                                             std::ios::app | std::ios::binary);
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

void ManifestManager::compactManifest(
    std::unordered_set<uint32_t> &&processed_segment_ids,
    std::unordered_map<uint32_t, uint32_t> &&invalid_bytes_per_segment) {
  // I need to iterate the current manifest. if a records segment_id is in the
  // list of seen segments, we don't write that record. by default all deleted
  // segment ids are in the seen list. When we come on a new segment, we simply
  // write one single event using the dead byte total we recieved at the the
  // start of compaction. We will need to spawn a new manifest file at the
  // beginning of this process to write to. At the successful conclusion of the
  // process we delete the old file and set the new file reference to the class
  // member. We may need to do a file rename as well.
  //
  std::ofstream manifest_swp_file(root_dir / "database" / "manifest" /
                                      "manifest_swp.txt",
                                  std::ios::app | std::ios::binary);

  std::streamoff offset = 0;

  while (offset < used) {
    uint8_t operation;
    uint32_t segment_id;

    manifest_file.seekg(offset, std::ios::beg);
    manifest_file.read(reinterpret_cast<char *>(&operation), sizeof(operation));
    manifest_file.read(reinterpret_cast<char *>(&segment_id),
                       sizeof(segment_id));

    if (processed_segment_ids.find(segment_id) != processed_segment_ids.end()) {
      offset += SERIALIZED_MANIFEST_RECORD_SIZE;
      continue;
    } else {
      manifest_swp_file.write(reinterpret_cast<char *>(&operation),
                              sizeof(operation));
      manifest_swp_file.write(reinterpret_cast<char *>(&segment_id),
                              sizeof(segment_id));
      manifest_swp_file.write(
          reinterpret_cast<char *>(&invalid_bytes_per_segment[segment_id]),
          sizeof(invalid_bytes_per_segment[segment_id]));
      processed_segment_ids.insert(segment_id);
      offset += SERIALIZED_MANIFEST_RECORD_SIZE;
    }
  }

  manifest_swp_file.close();
  std::filesystem::remove(root_dir / "database" / "manifest" / "manifest.txt");
  std::filesystem::rename(root_dir / "database" / "manifest" /
                              "manifest_swp.txt",
                          root_dir / "database" / "manifest" / "manifest.txt");
}
