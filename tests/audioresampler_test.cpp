/**
 * @file audioresampler_test.cpp
 * @brief This file includes tests for the AudioResampler class.
 * @date 2024-09-07
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "audioresampler.hpp"
#include "decoder.hpp"
#include "demuxer.hpp"

TEST(AudioResamplerTest, ResampleMultipleFrames) {
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreamPointers();

    AVCodecParameters *codecpar = streams[1]->codecpar;

    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    AV::Utils::AudioResamplerConfig config;
    config.srcsamplerate = streams[1]->codecpar->sample_rate;
    config.dstsamplerate = 48000;
    config.srcchannellayout = streams[1]->codecpar->ch_layout;
    config.dstchannellayout = AV_CHANNEL_LAYOUT_STEREO;
    config.srcsampleformat = (AVSampleFormat)streams[1]->codecpar->format;
    config.dstsampleformat = AV_SAMPLE_FMT_FLTP;

    auto [resampler, resampler_err] = AV::Utils::AudioResampler::Create(config);

    for (int i = 0; i < 6; i++) {
        // Grab correct packet
        AVPacket *pkt = nullptr;
        do {
            auto [packet, packet_err] = demuxer->ReadFrame();
            if (packet->stream_index == 1) {
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

            auto [resampled_frame, resampled_frame_err] = resampler->Resample(frame);
            EXPECT_EQ(resampled_frame_err.code(), 0);
        }
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}