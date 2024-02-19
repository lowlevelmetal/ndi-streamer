
cmake_minimum_required(VERSION 3.0)
project(NDIStreamer)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")


# Find NDI SDK
find_package(NDISDK REQUIRED)

# Include NDI SDK headers
include_directories(${NDI_INCLUDE_DIR})

# Add executable
add_executable(ndistreamer src/ndi-streamer.cpp)

# Link NDI SDK library
target_link_libraries(ndistreamer ${NDI_LIB})
