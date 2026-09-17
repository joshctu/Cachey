#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

constexpr uint32_t MAX_SEGMENT_SIZE = 256;
constexpr uint32_t SERIALIZED_HEADER_SIZE = 17;
constexpr uint32_t SERIALIZED_FOOTER_SIZE = 18;
constexpr uint32_t MAGIC = 0xDEAD2BAD;

struct Cursor {
  std::streamoff file_offset = 0;
  std::streamoff next_record = 0;
};

struct SegmentFooter {
  uint32_t magic;
  uint32_t bytes_written;
  uint32_t dead_bytes;
  uint16_t entry_count;
  uint32_t segment_id;
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
  uint32_t length;
  SegmentEntryHeader header;
  std::string key;
  std::string value;

  SegmentRecord(SegmentEntryHeader header, std::string key, std::string value)
      : header(header), key(key), value(value) {
    length = sizeof(decltype(SegmentRecord::length)) + SERIALIZED_HEADER_SIZE +
             key.size() + value.size();
  }
};

// This differs from the above by requiring no constructor. This struct should
// be used only to return existing records for read access.
struct SegmentRecordView {
  uint32_t length = 0;
  SegmentEntryHeader header{};
  std::string key;
  std::string value;
};

class Segment {
public:
  using ReplayHandler =
      std::function<void(const std::string &key, const uint32_t segment_id,
                         const uint32_t file_offset)>;
  using IndexHandler =
      std::function<bool(const std::string &key, const uint32_t segment_id,
                         const uint32_t file_offset)>;

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
  uint32_t replay(const ReplayHandler &on_record);
  uint64_t generateChecksum(const std::string &key, const std::string &value,
                            const uint8_t operation);
  void seal();
  bool verifyChecksum(const uint64_t &stored_checksum, const std::string &key,
                      const std::string &value, const uint8_t operation);
  bool canAppend(const uint32_t entry_size);
  void invalidateBytes(const uint32_t file_offset);
  uint32_t readDeadBytes();
  int computeDeadBytes(const IndexHandler &is_latest_record);
  bool nextValidRecord(Cursor &c, SegmentRecordView &rec);

  void append(const std::string &key, const std::string &value);
  std::optional<const std::string> read(const std::string &key,
                                        uint32_t const offset);
  void remove(const std::string &key);
  void deleteFile();
};
