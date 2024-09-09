/**
 * @file pixelencoder_test.cpp
 * @brief This file includes tests for the PixelEncoder class.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "decoder.hpp"
#include "demuxer.hpp"
#include "pixelencoder.hpp"

TEST(PixelEncoderTest, CreatePixelEncoderSimple) {
    AV::Utils::PixelEncoderConfig config;
    config.src_width = 1920;
    config.src_height = 1080;
    config.src_pix_fmt = AV_PIX_FMT_YUV420P;
    config.dst_width = 1920;
    config.dst_height = 1080;
    config.dst_pix_fmt = AV_PIX_FMT_RGB24;

    auto [encoder, encoder_err] = AV::Utils::PixelEncoder::Create(config);
    EXPECT_EQ(encoder_err.code(), 0);
}

TEST(PixelEncoderTest, EncodeMultipleFrames) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreams();

    AVCodecParameters *codecpar = streams[0]->codecpar;

    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    AV::Utils::PixelEncoderConfig config;
    config.src_width = streams[0]->codecpar->width;
    config.src_height = streams[0]->codecpar->height;
    config.src_pix_fmt = (AVPixelFormat)streams[0]->codecpar->format;
    config.dst_width = 1920;
    config.dst_height = 1080;
    config.dst_pix_fmt = AV_PIX_FMT_YUV422P;

    auto [encoder, encoder_err] = AV::Utils::PixelEncoder::Create(config);

    for (int i = 0; i < 6; i++) {
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
            auto [encoded_frame, encoded_frame_err] = encoder->Encode(frame);
            EXPECT_EQ(encoded_frame_err.code(), 0);
            EXPECT_EQ(encoded_frame->width, 1920);
            EXPECT_EQ(encoded_frame->height, 1080);
        }
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}