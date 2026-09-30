
add_library(network_lib TCPServer.cpp TCPClient.cpp)

target_include_directories(network_lib PUBLIC include)

