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
    auto streams = demuxer->GetStreamPointers();

    AVCodecParameters *codecpar = streams[0]->codecpar;

    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);
    EXPECT_EQ(decoder_err.code(), 0);
}

TEST(DecoderTest, DecodeSinglePacket) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreamPointers();

    AVCodecParameters *codecpar = streams[0]->codecpar;

    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    // Grab correct packet
    AVPacket *pkt = nullptr;
    do {
        auto [packet, packet_err] = demuxer->ReadFrame();
        if (packet->stream_index == 0) {
            pkt = packet;
        }

    } while (pkt == nullptr);

    // Fill Decoder
    auto fill_err = decoder->FillDecoder(pkt);
    EXPECT_EQ(fill_err.code(), 0);

    while (true) {
        auto [frame, frame_err] = decoder->Decode();
        if (frame_err.code() == (int)AV::Utils::AvError::DECODEREXHAUSTED) {
            break;
        }
        EXPECT_EQ(frame_err.code(), 0);
    }
}

TEST(DecoderTest, DecodeMultiplePackets) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreamPointers();

    AVCodecParameters *codecpar = streams[0]->codecpar;

    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    for (int i = 0; i < 3; i++) {
        // Grab correct packet
        AVPacket *pkt = nullptr;
        do {
            auto [packet, packet_err] = demuxer->ReadFrame();
            if (packet->stream_index == 0) {
                pkt = packet;
            }

        } while (pkt == nullptr);

        // Fill Decoder
        auto fill_err = decoder->FillDecoder(pkt);
        EXPECT_EQ(fill_err.code(), 0);

        while (true) {
            auto [frame, frame_err] = decoder->Decode();
            if (frame_err.code() == (int)AV::Utils::AvError::DECODEREXHAUSTED) {
                break;
            }
            EXPECT_EQ(frame_err.code(), 0);
        }
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}