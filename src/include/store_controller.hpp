#pragma once

#include "store.hpp"
#include <string>

struct SetResult {
  bool success;
  std::string error;
  double time_elapsed;
};

struct BulkSetResult {
  bool success;
  std::string error;
  long writes_requested;
  long writes_completed;
  double avg_time_per_write;
};

struct GetResult {
  bool success;
  std::string error;
  double time_elapsed;
  std::string value;
};

struct DeleteResult {
  bool success;
  std::string error;
  double time_elapsed;
};

class StoreController {
private:
  KV_Store store;

public:
  StoreController(KV_Store &store);

  SetResult handleSetKeyValue(std::string &key, std::string &value);
  GetResult handleGetValue(std::string &key);
  DeleteResult handleDeleteKey(std::string &key);

  BulkSetResult handleSeedEntries(int number_of_entries);
};
