#include "catch2/catch_test_macros.hpp"
#include "store.hpp"

TEST_CASE("Store can set new key/value pairs and read them back", "[store]") {
  ProjectConfig testConfig;
  testConfig.root_dir = std::filesystem::path(DEFAULT_ROOT "/tests");

  KV_Store testStore(testConfig);
  testStore.init();

  for (auto i = 0; i < 1000; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(i);
    testStore.set(key, value);
  }

  for (auto i = 0; i < 1000; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(i);

    REQUIRE(value == testStore.get(key).value());
  }
}
