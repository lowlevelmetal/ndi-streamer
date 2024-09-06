/**
 * @file decoder_test.cpp
 * @brief This file includes tests for the Decoder class.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "decoder.hpp"

TEST(DecoderTest, CreateDecoderSimple) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreams();

    AVCodecID codec_id = streams[0]->codecpar->codec_id;
    AVCodecParameters *codecpar = streams[0]->codecpar;

    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codec_id, codecpar);
    EXPECT_EQ(decoder_err.code(), 0);
}

TEST(DecoderTest, CreateDecoderWithConfig) {
    AV::Utils::DecoderConfig config;
    
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreams();

    config.codec_id = streams[0]->codecpar->codec_id;
    config.codecpar = streams[0]->codecpar;

    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(config);
    EXPECT_EQ(decoder_err.code(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}