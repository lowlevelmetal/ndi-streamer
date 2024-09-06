/**
 * @file demuxer_test.cpp
 * @brief This file includes tests for the Demuxer class.
 * @date 2024-09-05
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "demuxer.hpp"

TEST(DemuxerTest, CreateDemuxerAuto) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    EXPECT_EQ(demuxer_err.code(), 0);
}

TEST(DemuxerTest, CreateDemuxerWithConfig) {
    AV::Utils::DemuxerConfig config;
    config.path = "testcontent/rickroll.mp4";
    config.width = 1920;
    config.height = 1080;
    config.pixel_format = "yuv420p";

    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create(config);
    EXPECT_EQ(demuxer_err.code(), 0);
}

TEST(DemuxerTest, ReadSingleFrame) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto [packet, packet_err] = demuxer.value()->ReadFrame();
    EXPECT_EQ(packet_err.code(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}