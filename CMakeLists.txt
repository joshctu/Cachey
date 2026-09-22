cmake_minimum_required(VERSION 3.20)

project(kv_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTORCC ON)

find_package(xxHash CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

find_package(Catch2 3 REQUIRED)
qt_standard_project_setup()

add_library(kv_store_lib src/store.cpp src/segment.cpp src/hash.cpp src/store_controller.cpp src/hint_manager.cpp src/include/store_controller.hpp)

qt_add_executable(kv_store_init src/main.cpp src/gui.cpp src/include/gui.hpp)

add_executable(tests tests/test_segment.cpp)

target_link_libraries(kv_store_lib PUBLIC xxHash::xxhash)
target_link_libraries(kv_store_lib PUBLIC Qt6::Widgets)
target_link_libraries(kv_store_lib PUBLIC Qt6::Core)

target_link_libraries(kv_store_init PRIVATE kv_store_lib)

target_link_libraries(tests PRIVATE kv_store_lib Catch2::Catch2WithMain)

target_include_directories(kv_store_lib PUBLIC ${CMAKE_SOURCE_DIR}/src/include)

