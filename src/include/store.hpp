#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hash_table.hpp"
#include "log_file.hpp"

class KV_Store {

private:
  std::vector<LogSegment> logs;
  Hash_Table hash_table;

public:
  KV_Store();

private:
  void generate_log_segment();

public:
  void set(std::string &key, std::string &value);
  std::optional<std::string> get(std::string &key);
  bool remove(std::string &key);
};
