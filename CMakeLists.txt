
cmake_minimum_required(VERSION 3.14)
project(NDIStreamer)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)  # Or 20 for C++20
set(CMAKE_CXX_STANDARD_REQUIRED True)  # Ensure the compiler respects the version
set(CMAKE_CXX_EXTENSIONS OFF)  # Disable compiler-specific extensions like gnu++17

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

# Set debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -D_DEBUG")

# Find NDI SDK
find_package(NDISDK REQUIRED)

# Find FFMPEG
find_package(FFMPEG REQUIRED)

# Include NDI SDK headers
include_directories(${NDI_INCLUDE_DIR} ${FFMPEG_INCLUDE_DIRS})

# Define sources
set(SOURCES
    src/ndi-streamer.cpp
    src/averror.cpp
    src/demuxer.cpp
    src/decoder.cpp
    src/pixelencoder.cpp
    src/audioresampler.cpp
    src/ndi.cpp
    src/ndisource.cpp)

# Set executable name
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(ndistreamer_debug ${SOURCES})
    target_link_libraries(ndistreamer_debug ${NDI_LIB} ${FFMPEG_LIBRARIES})
else()
    add_executable(ndistreamer ${SOURCES})
    target_link_libraries(ndistreamer ${NDI_LIB} ${FFMPEG_LIBRARIES})
endif()

# Enable testing
enable_testing()

# Add tests directory
add_subdirectory(tests)
