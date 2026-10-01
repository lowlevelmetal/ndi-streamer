/**
 * @file test_log.cpp
 * @brief Logging, including FFmpeg messages routed through the logger.
 */

#include "util/log.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include <libavutil/log.h>
}

using namespace ndistreamer;

namespace {

std::vector<std::string> Lines(const std::string &text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    return lines;
}

} // namespace

TEST(Log, FiltersByLevel) {
    testing::internal::CaptureStderr();
    log::Warn("shown {}", 1);
    log::Debug("hidden {}", 2);
    auto lines = Lines(testing::internal::GetCapturedStderr());
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].find("[warn ] shown 1"), std::string::npos) << lines[0];
}

TEST(Log, KeepsLongFFmpegMessagesSeparate) {
    std::string long_text(3000, 'x');
    testing::internal::CaptureStderr();
    av_log(nullptr, AV_LOG_ERROR, "%s\n", long_text.c_str());
    av_log(nullptr, AV_LOG_ERROR, "second %s", "message");
    av_log(nullptr, AV_LOG_ERROR, " continued\n");
    auto lines = Lines(testing::internal::GetCapturedStderr());

    ASSERT_EQ(lines.size(), 2u);
    EXPECT_NE(lines[0].find("ffmpeg: " + long_text), std::string::npos);
    EXPECT_NE(lines[1].find("ffmpeg: second message continued"), std::string::npos) << lines[1];
}
