/**
 * @file test_support.hpp
 * @brief Shared helpers: generated media, the fake NDI runtime and its log.
 */

#pragma once

#include "options.hpp"
#include "streamer.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace test {

/// Path of a clip produced by tests/media/generate_media.cmake.
std::string MediaPath(const std::string &name);

/// Skip the current test when the media fixture is missing (no ffmpeg CLI at build time).
#define REQUIRE_MEDIA(name)                                                                                            \
    do {                                                                                                               \
        if (!std::filesystem::exists(test::MediaPath(name))) {                                                         \
            GTEST_SKIP() << "test media '" << (name) << "' is missing; is the ffmpeg CLI installed?";                  \
        }                                                                                                              \
    } while (0)

struct VideoEvent {
    int64_t ns = 0;
    int width = 0;
    int height = 0;
    std::string fourcc;
    int fps_num = 0;
    int fps_den = 0;
    double aspect = 0;
    int format_type = -1;
    int64_t timecode = 0;
    int stride = 0;
};

struct AudioEvent {
    int64_t ns = 0;
    int sample_rate = 0;
    int channels = 0;
    int samples = 0;
    int stride_bytes = 0;
    int64_t timecode = 0;
    double rms = 0;
};

/// Everything the fake NDI runtime recorded for one sender.
struct FakeLog {
    std::string create;
    std::vector<std::string> metadata;
    std::vector<VideoEvent> video;
    std::vector<AudioEvent> audio;
    int flushes = 0;
    bool destroyed = false;

    int64_t TotalAudioSamples() const;
};

/**
 * @brief Points the fake runtime at a fresh log file for the lifetime of the object.
 */
class FakeNdiLog {
public:
    FakeNdiLog();
    ~FakeNdiLog();
    FakeLog Read() const;

private:
    std::filesystem::path m_path;
};

/// Options that stream @p media through the fake NDI runtime.
ndistreamer::Options MakeOptions(const std::string &media);

/**
 * @brief Stream to completion, or stop after @p stop_after, and return what was sent.
 * Rethrows pipeline errors.
 */
FakeLog Stream(const ndistreamer::Options &options,
               std::optional<std::chrono::milliseconds> stop_after = std::nullopt);

/// Median of a non-empty list.
double Median(std::vector<double> values);

} // namespace test
