#include "store_controller.hpp"
#include "exceptions.hpp"
#include <cstdint>
#include <string>

StoreController::StoreController(KV_Store &store) : store(store) {}

SetResult StoreController::handleSetKeyValue(std::string &key,
                                             std::string &value) {

  if (key.size() == 0)
    return {false, "A key must be provided."};

  if (value.size() == 0)
    return {false, "A value must be provided"};

  if (key.size() > UINT32_MAX)
    return {false, "Key is too large."};

  store.set(key, value);

  return {true, ""};
}

GetResult StoreController::handleGetValue(std::string &key) {

  if (key.size() == 0)
    return {false, "A key must be provided"};

  if (key.size() > UINT32_MAX)
    return {false, "Key is too large"};

  try {
    auto result = store.get(key);
    if (result) {
      std::string value = *result;
      return {true, "", value};
    }

  } catch (ReadException &e) {
    return {false, e.what()};
  }
  return {false, "Key not found"};
}
