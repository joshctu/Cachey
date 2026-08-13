#pragma once

#include "store.hpp"
#include <string>

struct SetResult {
  bool success;
  std::string error;
};

struct GetResult {
  bool success;
  std::string error;
  std::string value;
};

class StoreController {
private:
  KV_Store store;

public:
  StoreController(KV_Store &store);

  SetResult handleSetKeyValue(std::string &key, std::string &value);
  GetResult handleGetValue(std::string &key);
};
