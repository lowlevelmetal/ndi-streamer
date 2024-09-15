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

TEST(DemuxerTest, ReadSingleFrame) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto [packet, packet_err] = demuxer->ReadFrame();
    EXPECT_EQ(packet_err.code(), 0);
}

TEST(DemuxerTest, ReadMultipleFrames) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto [packet, packet_err] = demuxer->ReadFrame();
    EXPECT_EQ(packet_err.code(), 0);
    auto [packet2, packet_err2] = demuxer->ReadFrame();
    EXPECT_EQ(packet_err2.code(), 0);
    auto [packet3, packet_err3] = demuxer->ReadFrame();
    EXPECT_EQ(packet_err3.code(), 0);
}

TEST(DemuxerTest, GetStreamPointers) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreamPointers();
    EXPECT_EQ(streams.size(), 2);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}