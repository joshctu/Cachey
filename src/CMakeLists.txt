add_library(kv_store_lib store.cpp segment.cpp hash.cpp
store_controller.cpp hint_manager.cpp manifest.cpp
include/store_controller.hpp seg_map_monitor.cpp seg_id_monitor.cpp)

target_include_directories(kv_store_lib PUBLIC include)

target_link_libraries(kv_store_lib PUBLIC xxHash::xxhash)
target_link_libraries(kv_store_lib PUBLIC Qt6::Widgets Qt6::Core)

target_compile_definitions(kv_store_lib PUBLIC DEFAULT_ROOT="${CMAKE_SOURCE_DIR}")
