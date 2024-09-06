/**
 * @file decoder_test.cpp
 * @brief This file includes tests for the Decoder class.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "decoder.hpp"

TEST(DecoderTest, CreateDecoderSimple) {
    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(AV_CODEC_ID_MPEG4);
    EXPECT_EQ(decoder_err.code(), 0);
}

TEST(DecoderTest, CreateDecoderWithConfig) {
    AV::Utils::DecoderConfig config;
    config.codec_id = AV_CODEC_ID_MPEG4;

    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(config);
    EXPECT_EQ(decoder_err.code(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}