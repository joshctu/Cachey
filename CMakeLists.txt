cmake_minimum_required(VERSION 3.20)

project(kv_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(xxHash_DIR "/home/lean/cpp/kv_store/vcpkg_installed/x64-linux/share/xxhash")
find_package(xxHash CONFIG REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

qt_add_executable(kv_store_init src/main.cpp src/store.cpp src/gui.cpp src/include/gui.hpp)

target_link_libraries(kv_store_init PRIVATE xxHash::xxhash)
target_link_libraries(kv_store_init PRIVATE Qt6::Widgets)
target_link_libraries(kv_store_init PRIVATE Qt6::Core)

target_include_directories(kv_store_init PRIVATE ${CMAKE_SOURCE_DIR}/src/include)

