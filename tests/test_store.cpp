#include <catch2/catch_test_macros.hpp>
#include <store.hpp>

ProjectConfig makeConfig() {
  ProjectConfig c;
  c.root_dir = std::filesystem::path(DEFAULT_ROOT "/tests");
  return c;
}

struct StoreFixture {
  ProjectConfig config = makeConfig();
  std::unique_ptr<KV_Store> testStore;

  StoreFixture() {
    testStore = std::make_unique<KV_Store>(config);
    testStore->init();
  }

  ~StoreFixture() {
    // Need the store destructor to run here before the fixture destructor
    testStore.reset();
    std::filesystem::remove_all(config.root_dir / "database");
  }
};

TEST_CASE_METHOD(StoreFixture, "Store can perform basic operations",
                 "[store]") {

  SECTION("Store can write values and read them back") {
    for (auto i = 0; i < 1000; ++i) {
      std::string key = "key" + std::to_string(i);
      std::string value = "value" + std::to_string(i);

      testStore->set(key, value);
    }

    for (auto i = 0; i < 1000; ++i) {
      std::string key = "key" + std::to_string(i);
      std::string value = "value" + std::to_string(i);

      REQUIRE(testStore->get(key).value() == value);
    }
  }

  SECTION("Store can update existing values") {
    for (auto i = 0; i < 10; ++i) {
      std::string key = "key" + std::to_string(i);
      std::string value = "value" + std::to_string(i);

      testStore->set(key, value);
    }

    std::string testKey = "key5";
    std::string testValue = "new_value";

    testStore->set(testKey, testValue);
    REQUIRE(testStore->get(testKey).value() == testValue);
  }

  SECTION("Store can delete values and find their tombstones") {
    for (auto i = 0; i < 10; ++i) {
      std::string key = "key" + std::to_string(i);
      std::string value = "value" + std::to_string(i);

      testStore->set(key, value);
    }

    std::string removeKey = "key5";
    testStore->remove(removeKey);

    REQUIRE(testStore->get(removeKey).value() == "No record found");
  }
}

TEST_CASE_METHOD(StoreFixture, "Store can correctly perform compaction",
                 "[store]") {
  for (auto i = 0; i < 100; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(i);

    testStore->set(key, value);
  }

  auto segment_benchmark = testStore->getSegmentCount();

  for (auto i = 0; i < 100; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string new_value = "new_value" + std::to_string(i);

    testStore->set(key, new_value);
  }

  REQUIRE(testStore->getSegmentCount() < segment_benchmark * 2);

  for (auto i = 0; i < 100; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string new_value = "new_value" + std::to_string(i);

    REQUIRE(testStore->get(key).value() == new_value);
  }
}

SCENARIO("Store can boot properly from hints when present", "[store]") {

  GIVEN("A fresh store with some new data") {

    ProjectConfig config;
    config.root_dir = std::filesystem::path(DEFAULT_ROOT "/tests");

    KV_Store *testStore1 = new KV_Store(config);
    testStore1->init();

    for (auto i = 0; i < 20; ++i) {
      std::string key = "key" + std::to_string(i);
      std::string value = "value" + std::to_string(i);

      testStore1->set(key, value);
    }

    WHEN("The store is shutdown and all the segment files are removed") {
      delete testStore1;
      std::filesystem::remove_all(config.root_dir / "database" / "wal");

      THEN("On fresh startup segment_ids are properly loaded into the store") {
        KV_Store *testStore2 = new KV_Store(config);
        testStore2->init();

        // Really just need 1 present segment to confirm we booted from the
        // hints file. Can't confirm values as those still require the log files
        REQUIRE(testStore2->getSegmentCount() > 2);
      }
    }
  }
}
SCENARIO("Store operations commence as normal when booting from hints") {

  GIVEN("A store that has had a mix of operations performed on it") {
    ProjectConfig config;
    config.root_dir = std::filesystem::path(DEFAULT_ROOT "/tests");

    KV_Store *testStore1 = new KV_Store(config);
    testStore1->init();

    for (auto i = 0; i < 10; ++i) {
      std::string key = "key" + std::to_string(i);
      std::string value = "value" + std::to_string(i);

      testStore1->set(key, value);
    }

    for (auto i = 0; i < 10; ++i) {
      std::string key = "new_key" + std::to_string(i);
      std::string value = "new_value" + std::to_string(i);

      testStore1->set(key, value);
    }

    std::string update_key = "key1";
    std::string update_value = "updated_value1";
    testStore1->set(update_key, update_value);

    std::string delete_key_1 = "key5";
    std::string delete_key_2 = "new_key5";

    testStore1->remove(delete_key_1);
    testStore1->remove(delete_key_2);

    WHEN("The store is shutdown and brought back up") {
      delete testStore1;
      KV_Store *testStore2 = new KV_Store(config);
      testStore2->init();

      THEN("Previous operations are reflected in a new store") {

        for (auto i = 0; i < 10; ++i) {
          if (i == 5)
            continue;

          std::string key = "new_key" + std::to_string(i);
          std::string value = "new_value" + std::to_string(i);
          REQUIRE(testStore2->get(key).value() == value);
        }

        REQUIRE(testStore2->get(update_key).value() == update_value);

        REQUIRE(testStore2->get(delete_key_1).value() == "No record found");
        REQUIRE(testStore2->get(delete_key_2).value() == "No record found");

        delete testStore2;
        std::filesystem::remove_all(config.root_dir / "database");
      }
    }
  }
}
