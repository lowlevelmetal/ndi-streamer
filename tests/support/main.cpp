/**
 * @file main.cpp
 * @brief Test runner: keeps pipeline and FFmpeg logging quiet unless NDISTREAMER_TEST_LOG is set.
 */

#include "util/log.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string_view>

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    using ndistreamer::log::Level;
    Level level = Level::Warn;
    if (const char *env = std::getenv("NDISTREAMER_TEST_LOG")) {
        std::string_view name = env;
        if (name == "info") level = Level::Info;
        if (name == "debug") level = Level::Debug;
        if (name == "trace") level = Level::Trace;
    }
    ndistreamer::log::SetLevel(level);
    ndistreamer::log::InstallFFmpegBridge();

    return RUN_ALL_TESTS();
}
