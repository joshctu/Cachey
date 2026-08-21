#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hash.hpp"
#include "segment.hpp"

class KV_Store {

private:
  std::vector<Segment> segments;
  Hash_Table hash_table;

public:
  KV_Store();

private:
  void generateSegment();

public:
  void set(std::string &key, std::string &value);
  std::optional<std::string> get(std::string &key);
  bool remove(std::string &key);
};
