# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# Download test video
set(VIDEO_URL "http://castus-public-resources.s3-website-us-west-2.amazonaws.com/content/videos/mp4/rickroll.mp4")
set(VIDEO_DEST "${CMAKE_CURRENT_BINARY_DIR}/testcontent/rickroll.mp4")

file(DOWNLOAD ${VIDEO_URL} ${VIDEO_DEST}
    SHOW_PROGRESS
    STATUS video_download_status)

# Make sure the video is downloaded before running the tests
add_custom_target(download_video ALL
    DEPENDS ${VIDEO_DEST})

find_package(GTest REQUIRED)

add_executable(demuxer_test demuxer_test.cpp ../src/demuxer.cpp)

add_dependencies(demuxer_test download_video)

include_directories(${FFMPEG_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS} ../src)

target_link_libraries(demuxer_test PRIVATE GTest::gtest GTest::gtest_main ${FFMPEG_LIBRARIES})

add_test(NAME demuxer_test COMMAND demuxer_test)