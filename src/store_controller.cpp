#include "store_controller.hpp"
#include "exceptions.hpp"
#include <QElapsedTimer>
#include <cstdint>
#include <qelapsedtimer.h>
#include <string>
#include <tuple>

StoreController::StoreController(KV_Store *store) : store(store) {
  value_ticker = 0;
  store->registerObserver(this);
}

InitResult StoreController::handleStoreInit() {
  InitResult init_metadata = store->init();

  return init_metadata;
}

void StoreController::onCompactionComplete(CompactionResult &result) {

  emit signalCompactionComplete(result.segments_merged, result.records_removed);
};

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
  store->set(key, value);

  double time = timer.nsecsElapsed() * 0.000001;
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
    std::optional<std::tuple<std::string, uint32_t>> result = store->get(key);
    if (result) {
      double time = timer.nsecsElapsed() * 0.000001;
      std::tuple<std::string, uint32_t> valid_result = *result;
      return {true, "", time, std::get<0>(valid_result),
              std::get<1>(valid_result)};
    }

  } catch (ReadException &e) {
    return {false, e.what()};
  }
  return {false, "Key not found"};
}

DeleteResult StoreController::handleDeleteKey(std::string &key) {
  if (key.size() == 0)
    return {false, "A key must be provided"};

  if (key.size() > UINT32_MAX)
    return {false, "Key is too large"};

  try {
    QElapsedTimer timer;
    timer.start();
    auto result = store->remove(key);
    if (result) {
      double time = timer.nsecsElapsed() * 0.000001;
      return {true, "", time};
    }
  } catch (ReadException &e) {
    return {false, e.what()};
  }

  return {false, "Key not found"};
}

BulkSetResult StoreController::handleSeedEntries(int number_of_entries) {
  QElapsedTimer timer;
  long writes_completed = 0;
  double avg_time_accumulator = 0;

  for (int i = 0; i < number_of_entries; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(value_ticker);

    timer.restart();
    store->set(key, value);
    writes_completed += 1;
    avg_time_accumulator += timer.nsecsElapsed();
    value_ticker += 1;
  }

  return {true, "", number_of_entries, writes_completed,
          (avg_time_accumulator / writes_completed) * 0.000001};
}

StoreController::~StoreController() = default;
