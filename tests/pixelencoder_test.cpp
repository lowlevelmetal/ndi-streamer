/**
 * @file pixelencoder_test.cpp
 * @brief This file includes tests for the PixelEncoder class.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "pixelencoder.hpp"
#include "decoder.hpp"
#include "demuxer.hpp"

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

TEST(PixelEncoderTest, EncodeSingleFrame) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreams();

    AVCodecID codec_id = streams[0]->codecpar->codec_id;
    AVCodecParameters *codecpar = streams[0]->codecpar;

    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codec_id, codecpar);
    
    // Grab correct packet
    AVPacket *pkt = nullptr;
    do {
        auto [packet, packet_err] = demuxer->ReadFrame();
        if (packet->stream_index == 0) {
            pkt = packet;
        }

    } while(pkt == nullptr);

    auto [frame, frame_err] = decoder->Decode(pkt);

    AV::Utils::PixelEncoderConfig config;
    config.src_width = frame->width;
    config.src_height = frame->height;
    config.src_pix_fmt = (AVPixelFormat)frame->format;
    config.dst_width = frame->width;
    config.dst_height = frame->height;
    config.dst_pix_fmt = AV_PIX_FMT_YUV422P;

    auto [encoder, encoder_err] = AV::Utils::PixelEncoder::Create(config);
    auto [encoded_frame, encoded_frame_err] = encoder->Encode(frame);
    EXPECT_EQ(encoded_frame_err.code(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}