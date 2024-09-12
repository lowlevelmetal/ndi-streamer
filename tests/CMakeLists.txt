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

add_executable(demuxer_test demuxer_test.cpp ../src/demuxer.cpp ../src/averror.cpp)
add_executable(decoder_test decoder_test.cpp ../src/decoder.cpp ../src/averror.cpp ../src/demuxer.cpp)
add_executable(pixelencoder_test pixelencoder_test.cpp ../src/pixelencoder.cpp ../src/decoder.cpp ../src/averror.cpp ../src/demuxer.cpp)
add_executable(audioresampler_test audioresampler_test.cpp ../src/audioresampler.cpp ../src/averror.cpp ../src/decoder ../src/demuxer.cpp)
add_executable(ndisource_test ndisource_test.cpp ../src/ndisource.cpp ../src/ndi.cpp ../src/averror.cpp ../src/demuxer.cpp ../src/decoder.cpp ../src/pixelencoder.cpp ../src/audioresampler.cpp)
add_executable(asyncndisource_test asyncndisource_test.cpp ../src/asyncndisource.cpp ../src/ndisource.cpp ../src/ndi.cpp ../src/averror.cpp ../src/demuxer.cpp ../src/decoder.cpp ../src/pixelencoder.cpp ../src/audioresampler.cpp)

add_dependencies(demuxer_test download_video)
add_dependencies(decoder_test download_video)
add_dependencies(pixelencoder_test download_video)
add_dependencies(audioresampler_test download_video)
add_dependencies(ndisource_test download_video)
add_dependencies(asyncndisource_test download_video)

include_directories(${NDI_INCLUDE_DIR} ${FFMPEG_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS} ../src)

target_link_libraries(demuxer_test PRIVATE GTest::gtest GTest::gtest_main ${FFMPEG_LIBRARIES})
target_link_libraries(decoder_test PRIVATE GTest::gtest GTest::gtest_main ${FFMPEG_LIBRARIES})
target_link_libraries(pixelencoder_test PRIVATE GTest::gtest GTest::gtest_main ${FFMPEG_LIBRARIES})
target_link_libraries(audioresampler_test PRIVATE GTest::gtest GTest::gtest_main ${FFMPEG_LIBRARIES})
target_link_libraries(ndisource_test PRIVATE GTest::gtest GTest::gtest_main ${FFMPEG_LIBRARIES} ${NDI_LIB})
target_link_libraries(asyncndisource_test PRIVATE GTest::gtest GTest::gtest_main ${FFMPEG_LIBRARIES} ${NDI_LIB})

# Set up demuxer tests
add_test(NAME demuxer_test COMMAND demuxer_test)
add_test(NAME valgrind_demuxer_test
         COMMAND valgrind --leak-check=full --error-exitcode=1 --show-reachable=no $<TARGET_FILE:demuxer_test>)

# Set up decoder tests
add_test(NAME decoder_test COMMAND decoder_test)
add_test(NAME valgrind_decoder_test
         COMMAND valgrind --leak-check=full --error-exitcode=1 --show-reachable=no $<TARGET_FILE:decoder_test>)

# Set up pixelencoder tests
add_test(NAME pixelencoder_test COMMAND pixelencoder_test)
add_test(NAME valgrind_pixelencoder_test
         COMMAND valgrind --leak-check=full --error-exitcode=1 --show-reachable=no $<TARGET_FILE:pixelencoder_test>)

# Set up audioresampler tests
add_test(NAME audioresampler_test COMMAND audioresampler_test)
add_test(NAME valgrind_audioresampler_test
         COMMAND valgrind --leak-check=full --error-exitcode=1 --show-reachable=no $<TARGET_FILE:audioresampler_test>)

# Set up ndisource tests
add_test(NAME ndisource_test COMMAND ndisource_test)
add_test(NAME valgrind_ndisource_test
         COMMAND valgrind --leak-check=full --error-exitcode=1 --show-reachable=no $<TARGET_FILE:ndisource_test>)

# Set up asyncndisource tests
add_test(NAME asyncndisource_test COMMAND asyncndisource_test)

