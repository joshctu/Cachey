#pragma once

#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class Hash_Table {
private:
  std::unordered_map<uint64_t, uint64_t> lookup_table;
  std::shared_mutex hash_mtx;

public:
  Hash_Table();

  uint64_t generateHash(const std::string &key);

  void createIndex(const std::string &key, const uint64_t offset);
  std::optional<uint64_t> lookupHash(const std::string &key);
};
