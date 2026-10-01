cmake_minimum_required(VERSION 3.20)

project(ndistreamer
    VERSION 2.0.1
    DESCRIPTION "Stream any media file, network stream or capture device as an NDI source"
    HOMEPAGE_URL "https://github.com/lowlevelmetal/ndi-streamer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(NDISTREAMER_BUILD_TESTS "Build the test suite (requires GoogleTest)" ${PROJECT_IS_TOP_LEVEL})
option(NDISTREAMER_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(NDISTREAMER_WERROR "Treat compiler warnings as errors" OFF)
option(NDISTREAMER_STATIC
    "Link FFmpeg and the C++ runtime statically for a portable binary (see packaging/build-static-deps.sh)" OFF)
set(NDI_SDK_DIR "" CACHE PATH
    "Optional NDI SDK root. Not needed to build; enables ABI verification and end-to-end tests")

# Dependencies ---------------------------------------------------------------

find_package(PkgConfig REQUIRED)
# FFmpeg 6.0 or newer.
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat>=60
    libavcodec>=60
    libavfilter>=9
    libavdevice>=60
    libavutil>=58
    libswscale>=7
    libswresample>=4)
find_package(Threads REQUIRED)

if(NDISTREAMER_STATIC)
    # Static FFmpeg, e.g. from packaging/build-static-deps.sh. Only glibc stays dynamic: the NDI
    # runtime is loaded with dlopen, which a fully static executable cannot do.
    # libatomic is a GCC runtime library that not every system has, so link its archive too.
    list(TRANSFORM FFMPEG_STATIC_LDFLAGS REPLACE "^-latomic$" "-l:libatomic.a")
    add_library(ndistreamer_ffmpeg INTERFACE)
    target_include_directories(ndistreamer_ffmpeg SYSTEM INTERFACE ${FFMPEG_STATIC_INCLUDE_DIRS})
    target_compile_options(ndistreamer_ffmpeg INTERFACE ${FFMPEG_STATIC_CFLAGS_OTHER})
    target_link_libraries(ndistreamer_ffmpeg INTERFACE ${FFMPEG_STATIC_LDFLAGS})
    set(NDISTREAMER_FFMPEG ndistreamer_ffmpeg)
    add_link_options(-static-libstdc++ -static-libgcc)
else()
    set(NDISTREAMER_FFMPEG PkgConfig::FFMPEG)
endif()

# Newer FFmpeg lets filter sources carry color properties.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${FFMPEG_INCLUDE_DIRS})
check_cxx_source_compiles([[
    #include <cstdint>
    extern "C" {
    #include <libavfilter/buffersrc.h>
    }
    int main() { AVBufferSrcParameters p{}; p.color_space = AVCOL_SPC_BT709; p.color_range = AVCOL_RANGE_MPEG; return 0; }
]] NDISTREAMER_HAVE_BUFFERSRC_COLOR)
unset(CMAKE_REQUIRED_INCLUDES)

# Compiler settings ----------------------------------------------------------

add_library(ndistreamer_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ndistreamer_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
        $<$<BOOL:${NDISTREAMER_WERROR}>:-Werror>)
endif()

if(NDISTREAMER_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# Targets --------------------------------------------------------------------

configure_file(src/version.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp @ONLY)

add_library(ndistreamer_core STATIC
    src/av/audio_processor.cpp
    src/av/decoder.cpp
    src/av/ffmpeg.cpp
    src/av/input.cpp
    src/av/video_processor.cpp
    src/frames.cpp
    src/ndi/runtime.cpp
    src/ndi/sender.cpp
    src/options.cpp
    src/streamer.cpp
    src/util/clock.cpp
    src/util/log.cpp)
target_include_directories(ndistreamer_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(ndistreamer_core
    PUBLIC ${NDISTREAMER_FFMPEG} Threads::Threads ${CMAKE_DL_LIBS}
    PRIVATE ndistreamer_warnings)
target_compile_definitions(ndistreamer_core PRIVATE
    $<$<BOOL:${NDISTREAMER_HAVE_BUFFERSRC_COLOR}>:NDISTREAMER_HAVE_BUFFERSRC_COLOR>)

add_executable(ndistreamer src/main.cpp)
target_link_libraries(ndistreamer PRIVATE ndistreamer_core ndistreamer_warnings)

include(GNUInstallDirs)
install(TARGETS ndistreamer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Tests ----------------------------------------------------------------------

if(NDISTREAMER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
