/**
 * @file ndisource_test.cpp
 * @brief This file includes tests for the NdiSource class.
 * @date 2024-09-08
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "ndisource.hpp"
#include "pixelencoder.hpp"
#include "audioresampler.hpp"
#include "demuxer.hpp"
#include "decoder.hpp"


TEST(NdiSourceTest, CreateNdiSourceSimple) {
    auto [ndisource, ndisource_err] = AV::Utils::NdiSource::Create("Test NDI Source");
    EXPECT_EQ(ndisource_err.code(), 0);
}

TEST(NdiSourceTest, SendNdiVideoFrame) {
    // Create demuxer
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreams();

    // Create decoder
    auto codecpar = streams[0]->codecpar;
    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    // Create pixel format encoder
    AV::Utils::PixelEncoderConfig encoder_config;
    encoder_config.src_width = streams[0]->codecpar->width;
    encoder_config.src_height = streams[0]->codecpar->height;
    encoder_config.src_pix_fmt = (AVPixelFormat)streams[0]->codecpar->format;
    encoder_config.dst_width = streams[0]->codecpar->width;
    encoder_config.dst_height = streams[0]->codecpar->height;
    encoder_config.dst_pix_fmt = AV_PIX_FMT_YUV422P;
    auto [encoder, encoder_err] = AV::Utils::PixelEncoder::Create(encoder_config);

    // Create NDI source
    auto [ndisource, ndisource_err] = AV::Utils::NdiSource::Create("Test NDI Source");

    // Decode frames and send to NDI
    for (int i = 0; i < 3; i++) {
        // Grab correct packet
        AVPacket *pkt = nullptr;
        do {
            auto [packet, packet_err] = demuxer->ReadFrame();
            if (packet->stream_index == 0) {
                pkt = packet;
            }

        } while(pkt == nullptr);

        auto [frame, frame_err] = decoder->Decode(pkt);
        auto [encoded_frame, encoded_frame_err] = encoder->Encode(frame);
        auto send_err = ndisource->SendVideoFrame(encoded_frame, decoder->GetFrameRate(), encoder->GetPixelFormat());
        EXPECT_EQ(send_err.code(), 0);
    }
}

TEST(NdiSourceTest, SendNdiAudioFrame) {
    // Create demuxer
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreams();

    // Create decoder
    auto codecpar = streams[1]->codecpar;
    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    // Create sample format encoder
    AV::Utils::AudioResamplerConfig resampler_config;
    resampler_config.srcsamplerate = streams[1]->codecpar->sample_rate;
    resampler_config.dstsamplerate = 48000;
    resampler_config.srcchannellayout = streams[1]->codecpar->ch_layout;
    resampler_config.dstchannellayout = AV_CHANNEL_LAYOUT_STEREO;
    resampler_config.srcsampleformat = (AVSampleFormat)streams[1]->codecpar->format;
    resampler_config.dstsampleformat = AV_SAMPLE_FMT_FLT;
    auto [resampler, resampler_err] = AV::Utils::AudioResampler::Create(resampler_config);

    // Create NDI source
    auto [ndisource, ndisource_err] = AV::Utils::NdiSource::Create("Test NDI Source");

    // Decode frames and send to NDI
    for (int i = 0; i < 3; i++) {
        // Grab correct packet
        AVPacket *pkt = nullptr;
        do {
            auto [packet, packet_err] = demuxer->ReadFrame();
            if (packet->stream_index == 1) {
                pkt = packet;
            }

        } while(pkt == nullptr);

        auto [frame, frame_err] = decoder->Decode(pkt);
        auto [resampled_frame, resampled_frame_err] = resampler->Resample(frame);
        auto send_err = ndisource->SendAudioFrame(resampled_frame);
        EXPECT_EQ(send_err.code(), 0);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}