find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(WARNING "GoogleTest not found; tests are disabled")
    return()
endif()
include(GoogleTest)

# Fake NDI runtime -------------------------------------------------------------
# Built as libndi.so.6 so the real loader code paths are exercised.
add_library(fake_ndi SHARED fake_ndi/fake_ndi.cpp)
target_include_directories(fake_ndi PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(fake_ndi PROPERTIES
    OUTPUT_NAME ndi
    VERSION 6.0.0
    SOVERSION 6
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fake_ndi)

add_library(not_ndi SHARED fake_ndi/not_ndi.cpp)
set_target_properties(not_ndi PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/not_ndi)

# Test media -----------------------------------------------------------------
set(TEST_MEDIA_DIR ${CMAKE_CURRENT_BINARY_DIR}/media)
find_program(FFMPEG_EXECUTABLE ffmpeg)
if(FFMPEG_EXECUTABLE)
    add_test(NAME generate_test_media
        COMMAND ${CMAKE_COMMAND} -DFFMPEG=${FFMPEG_EXECUTABLE} -DOUTPUT_DIR=${TEST_MEDIA_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/media/generate_media.cmake)
    set_tests_properties(generate_test_media PROPERTIES FIXTURES_SETUP media)
else()
    message(WARNING "ffmpeg CLI not found; tests that need media files will be skipped")
endif()

# Unit and integration tests ---------------------------------------------------
add_executable(ndistreamer_tests
    support/main.cpp
    support/test_support.cpp
    test_audio_processor.cpp
    test_clock.cpp
    test_input.cpp
    test_log.cpp
    test_options.cpp
    test_queue.cpp
    test_runtime.cpp
    test_streamer.cpp
    test_video_processor.cpp)
target_include_directories(ndistreamer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndistreamer_tests PRIVATE ndistreamer_core ndistreamer_warnings GTest::gtest)
target_compile_definitions(ndistreamer_tests PRIVATE
    FAKE_NDI_LIBRARY="$<TARGET_FILE:fake_ndi>"
    FAKE_NDI_DIR="$<TARGET_FILE_DIR:fake_ndi>"
    NOT_NDI_LIBRARY="$<TARGET_FILE:not_ndi>"
    TEST_MEDIA_DIR="${TEST_MEDIA_DIR}")
add_dependencies(ndistreamer_tests fake_ndi not_ndi)

gtest_discover_tests(ndistreamer_tests
    PROPERTIES FIXTURES_REQUIRED media
    DISCOVERY_TIMEOUT 30)

# Command line smoke tests -----------------------------------------------------
add_test(NAME cli_help COMMAND ndistreamer --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")

add_test(NAME cli_version COMMAND ndistreamer --version)
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "ndistreamer ${PROJECT_VERSION}")

add_test(NAME cli_rejects_bad_usage COMMAND ndistreamer --bogus)
set_tests_properties(cli_rejects_bad_usage PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli_reports_missing_input
    COMMAND ndistreamer -i ${TEST_MEDIA_DIR}/does-not-exist.mp4 --ndi-lib $<TARGET_FILE:fake_ndi>)
set_tests_properties(cli_reports_missing_input PROPERTIES WILL_FAIL TRUE)

if(FFMPEG_EXECUTABLE)
    add_test(NAME cli_streams_file
        COMMAND ndistreamer -i ${TEST_MEDIA_DIR}/av.mp4 --ndi-lib $<TARGET_FILE:fake_ndi> --stats=1)
    set_tests_properties(cli_streams_file PROPERTIES
        FIXTURES_REQUIRED media
        PASS_REGULAR_EXPRESSION "end of input reached")
endif()

# Tests against the real NDI SDK (optional) ------------------------------------
find_package(NDISDK)
if(NDISDK_FOUND)
    message(STATUS "NDI SDK found: ${NDISDK_INCLUDE_DIR}; enabling ABI and end-to-end tests")

    # Compiling this verifies src/ndi/abi.hpp against the official headers.
    add_executable(ndi_abi_test ndi_abi_test.cpp)
    target_include_directories(ndi_abi_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${NDISDK_INCLUDE_DIR})
    target_link_libraries(ndi_abi_test PRIVATE PkgConfig::FFMPEG)
    add_test(NAME ndi_abi COMMAND ndi_abi_test)

    add_executable(ndi_receiver e2e/ndi_receiver.cpp)
    target_link_libraries(ndi_receiver PRIVATE NDISDK::NDISDK)
    get_filename_component(_ndi_lib_dir "${NDISDK_LIBRARY}" DIRECTORY)
    set_target_properties(ndi_receiver PROPERTIES BUILD_RPATH "${_ndi_lib_dir}")

    if(NDISDK_BIN_DIR AND FFMPEG_EXECUTABLE)
        add_test(NAME ndi_end_to_end
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_e2e.sh
                    $<TARGET_FILE:ndistreamer> $<TARGET_FILE:ndi_receiver> ${_ndi_lib_dir}
                    ${NDISDK_BIN_DIR}/ndi-discovery-server ${TEST_MEDIA_DIR}/av.mp4
                    ${CMAKE_CURRENT_BINARY_DIR}/e2e)
        set_tests_properties(ndi_end_to_end PROPERTIES FIXTURES_REQUIRED media TIMEOUT 60 RUN_SERIAL TRUE)
    endif()
endif()
