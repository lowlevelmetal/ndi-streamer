
cmake_minimum_required(VERSION 3.0)
project(NDIStreamer)

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
    src/ndi.cpp
    src/ndierror.cpp
    src/ndisource.cpp)

# Set executable name
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(ndistreamer_debug ${SOURCES})
    target_link_libraries(ndistreamer_debug ${NDI_LIB} ${FFMPEG_LIBRARIES})
else()
    add_executable(ndistreamer ${SOURCES})
    target_link_libraries(ndistreamer ${NDI_LIB} ${FFMPEG_LIBRARIES})
endif()
