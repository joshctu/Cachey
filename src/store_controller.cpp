#include "store_controller.hpp"
#include "exceptions.hpp"
#include <QElapsedTimer>
#include <cstdint>
#include <qelapsedtimer.h>
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

  QElapsedTimer timer;
  timer.start();
  store.set(key, value);

  long long time = timer.nsecsElapsed();
  return {true, "", time};
}

GetResult StoreController::handleGetValue(std::string &key) {

  if (key.size() == 0)
    return {false, "A key must be provided"};

  if (key.size() > UINT32_MAX)
    return {false, "Key is too large"};

  try {
    QElapsedTimer timer;
    timer.start();
    auto result = store.get(key);
    if (result) {
      long long time = timer.nsecsElapsed();
      std::string value = *result;
      return {true, "", time, value};
    }

  } catch (ReadException &e) {
    return {false, e.what()};
  }
  return {false, "Key not found"};
}
