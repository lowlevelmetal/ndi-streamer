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

TEST(AudioResamplerTest, CreateAudioResamplerSimple) {
    AV::Utils::DemuxerConfig demuxer_config;
    demuxer_config.path = "testcontent/rickroll.mp4";

    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create(demuxer_config);
    auto streams = demuxer->GetStreams();

    auto codecpar = streams[1]->codecpar;
    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    // Grab correct packet
    AVPacket *pkt = nullptr;
    do {
        auto [packet, packet_err] = demuxer->ReadFrame();
        if (packet->stream_index == 1) {
            pkt = packet;
        }

    } while(pkt == nullptr);

    auto [frame, frame_err] = decoder->Decode(pkt);

    AV::Utils::AudioResamplerConfig resampler_config;
    resampler_config.srcsamplerate = frame->sample_rate;
    resampler_config.dstsamplerate = frame->sample_rate;
    resampler_config.srcchannellayout = frame->ch_layout;
    resampler_config.dstchannellayout = AV_CHANNEL_LAYOUT_STEREO;
    resampler_config.srcsampleformat = (AVSampleFormat)frame->format;
    resampler_config.dstsampleformat = AV_SAMPLE_FMT_FLT;

    auto [resampler, resampler_err] = AV::Utils::AudioResampler::Create(resampler_config);
    EXPECT_EQ(resampler_err.code(), 0);
}

TEST(AudioResamplerTest, ResampleSingleFrame) {
    AV::Utils::DemuxerConfig demuxer_config;
    demuxer_config.path = "testcontent/rickroll.mp4";

    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create(demuxer_config);
    auto streams = demuxer->GetStreams();

    auto codecpar = streams[1]->codecpar;
    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    // Grab correct packet
    AVPacket *pkt = nullptr;
    do {
        auto [packet, packet_err] = demuxer->ReadFrame();
        if (packet->stream_index == 1) {
            pkt = packet;
        }

    } while(pkt == nullptr);

    auto [frame, frame_err] = decoder->Decode(pkt);

    AV::Utils::AudioResamplerConfig resampler_config;
    resampler_config.srcsamplerate = frame->sample_rate;
    resampler_config.dstsamplerate = frame->sample_rate;
    resampler_config.srcchannellayout = frame->ch_layout;
    resampler_config.dstchannellayout = AV_CHANNEL_LAYOUT_STEREO;
    resampler_config.srcsampleformat = (AVSampleFormat)frame->format;
    resampler_config.dstsampleformat = AV_SAMPLE_FMT_FLT;

    auto [resampler, resampler_err] = AV::Utils::AudioResampler::Create(resampler_config);
    auto [resampled_frame, resampled_frame_err] = resampler->Resample(frame);
    EXPECT_EQ(resampled_frame_err.code(), 0);
    EXPECT_EQ(resampled_frame->sample_rate, frame->sample_rate);
    EXPECT_EQ(resampled_frame->ch_layout.nb_channels, 2);
    EXPECT_EQ(resampled_frame->format, resampler_config.dstsampleformat);
}

TEST(AudioResamplerTest, ResampleMultipleFrames) {
    AV::Utils::DemuxerConfig demuxer_config;
    demuxer_config.path = "testcontent/rickroll.mp4";

    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create(demuxer_config);
    auto streams = demuxer->GetStreams();

    auto codecpar = streams[1]->codecpar;
    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    // Create resampler
    AV::Utils::AudioResamplerConfig resampler_config;
    resampler_config.srcsamplerate = streams[1]->codecpar->sample_rate;
    resampler_config.dstsamplerate = 48000;
    resampler_config.srcchannellayout = streams[1]->codecpar->ch_layout;
    resampler_config.dstchannellayout = AV_CHANNEL_LAYOUT_STEREO;
    resampler_config.srcsampleformat = (AVSampleFormat)streams[1]->codecpar->format;
    resampler_config.dstsampleformat = AV_SAMPLE_FMT_FLT;

    auto [resampler, resampler_err] = AV::Utils::AudioResampler::Create(resampler_config);

    for (int i = 0; i < 6; i++) {
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
        EXPECT_EQ(resampled_frame_err.code(), 0);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}