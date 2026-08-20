#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

class Hash_Table {

private:
  static constexpr uint8_t HASH_SEED = 0;
  std::unordered_map<uint64_t, uint64_t> lookup_table;

public:
  Hash_Table();

  uint64_t generate_hash(const std::string &key);
  uint64_t generate_checksum(const std::string &key, const std::string &value,
                             uint8_t operation);
  bool verify_checksum(const uint64_t &stored_checksum, const std::string &key,
                       const std::string &value, const uint8_t operation);

  void create_index(const std::string &key, const uint64_t offset);
  void update_index(const uint64_t hash, const uint64_t offset);
  std::optional<uint64_t> lookup_hash(const std::string &key);
};
