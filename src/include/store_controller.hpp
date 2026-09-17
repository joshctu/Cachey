#pragma once

#include "QObject"
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
  // temp dev field
  uint32_t dead_bytes;
};

struct DeleteResult {
  bool success;
  std::string error;
  double time_elapsed;
};

class StoreController : public QObject, public IStoreObserver {
  Q_OBJECT

private:
  KV_Store *store;

  // this is a dev member. It allows me to seed new entries with the same keys
  // but different values. Good for quickly setting up compaction scenerios
  int value_ticker;

public:
  StoreController(KV_Store *store);

  InitResult handleStoreInit();
  SetResult handleSetKeyValue(std::string &key, std::string &value);
  GetResult handleGetValue(std::string &key);
  DeleteResult handleDeleteKey(std::string &key);
  BulkSetResult handleSeedEntries(int number_of_entries);
  void onCompactionComplete(CompactionResult &result) override;

  ~StoreController();

signals:
  void signalCompactionComplete(int segments_merged, uint16_t records_removed);
};
