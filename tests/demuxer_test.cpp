/**
 * @file demuxer_test.cpp
 * @brief This file includes tests for the Demuxer class.
 * @date 2024-09-05
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "demuxer.hpp"

TEST(DemuxerTest, CreateDemuxer) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    EXPECT_EQ(demuxer_err.code(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}