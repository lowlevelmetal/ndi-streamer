/**
 * @file test_audio_processor.cpp
 * @brief Sample format conversion, packing and timing on synthetic frames.
 */

#include "av/audio_processor.hpp"

#include <gtest/gtest.h>

#include <vector>

extern "C" {
#include <libavutil/channel_layout.h>
}

using namespace ndistreamer;
using av::AudioProcessor;
using av::AudioProcessorConfig;

namespace {

av::FramePtr MakeAudioFrame(AVSampleFormat format, int rate, int channels, int samples, int64_t pts) {
    av::FramePtr frame = av::MakeFrame();
    frame->format = format;
    frame->sample_rate = rate;
    frame->nb_samples = samples;
    frame->pts = pts;
    av_channel_layout_default(&frame->ch_layout, channels);
    av::Check(av_frame_get_buffer(frame.get(), 0), "allocating test frame");
    return frame;
}

void FillPlanarFloat(AVFrame *frame, std::initializer_list<float> per_channel) {
    int c = 0;
    for (float value : per_channel) {
        auto *plane = reinterpret_cast<float *>(frame->extended_data[c++]);
        for (int i = 0; i < frame->nb_samples; i++) plane[i] = value;
    }
}

std::vector<AudioFrame> Process(AudioProcessor &processor, const AVFrame *frame) {
    std::vector<AudioFrame> out;
    processor.Process(frame, [&](AudioFrame &&f) {
        out.push_back(std::move(f));
        return true;
    });
    return out;
}

std::vector<AudioFrame> Flush(AudioProcessor &processor) {
    std::vector<AudioFrame> out;
    processor.Flush([&](AudioFrame &&f) {
        out.push_back(std::move(f));
        return true;
    });
    return out;
}

AudioProcessorConfig Config(int rate = 0, int channels = 0, AVRational time_base = {1, 48000}) {
    AudioProcessorConfig config;
    config.sample_rate = rate;
    config.channels = channels;
    config.time_base = time_base;
    return config;
}

} // namespace

TEST(AudioProcessor, PacksPlanarFloatWithoutConversion) {
    AudioProcessor processor(Config());
    auto frame = MakeAudioFrame(AV_SAMPLE_FMT_FLTP, 48000, 2, 1024, 480);
    FillPlanarFloat(frame.get(), {0.25f, -0.5f});

    auto out = Process(processor, frame.get());
    ASSERT_EQ(out.size(), 1u);
    const AudioFrame &f = out[0];
    EXPECT_EQ(f.sample_rate, 48000);
    EXPECT_EQ(f.channels, 2);
    EXPECT_EQ(f.samples_per_channel, 1024);
    EXPECT_EQ(f.channel_stride, 1024);
    EXPECT_EQ(f.pts_us, 10000);
    EXPECT_FLOAT_EQ(f.samples[0], 0.25f);
    EXPECT_FLOAT_EQ(f.samples[1023], 0.25f);
    EXPECT_FLOAT_EQ(f.samples[1024], -0.5f);
    EXPECT_FLOAT_EQ(f.samples[2047], -0.5f);
}

TEST(AudioProcessor, ConvertsInterleavedS16) {
    AudioProcessor processor(Config());
    auto frame = MakeAudioFrame(AV_SAMPLE_FMT_S16, 48000, 2, 256, 0);
    auto *samples = reinterpret_cast<int16_t *>(frame->data[0]);
    for (int i = 0; i < 256; i++) {
        samples[2 * i] = 16384;
        samples[2 * i + 1] = -16384;
    }

    auto out = Process(processor, frame.get());
    ASSERT_EQ(out.size(), 1u);
    const AudioFrame &f = out[0];
    ASSERT_EQ(f.samples_per_channel, 256);
    EXPECT_FLOAT_EQ(f.samples[10], 0.5f);
    EXPECT_FLOAT_EQ(f.samples[f.channel_stride + 10], -0.5f);
}

TEST(AudioProcessor, ResamplesToRequestedRate) {
    AudioProcessor processor(Config(48000, 0, {1, 44100}));

    int64_t produced = 0;
    for (int i = 0; i < 10; i++) {
        auto frame = MakeAudioFrame(AV_SAMPLE_FMT_FLTP, 44100, 1, 1024, i * 1024);
        FillPlanarFloat(frame.get(), {0.1f});
        for (auto &f : Process(processor, frame.get())) {
            EXPECT_EQ(f.sample_rate, 48000);
            produced += f.samples_per_channel;
        }
    }
    for (auto &f : Flush(processor)) produced += f.samples_per_channel;

    EXPECT_NEAR(static_cast<double>(produced), 10240.0 * 48000 / 44100, 64);
}

TEST(AudioProcessor, RemixesToRequestedChannelCount) {
    AudioProcessor processor(Config(0, 2));
    auto frame = MakeAudioFrame(AV_SAMPLE_FMT_FLTP, 48000, 6, 512, 0);
    FillPlanarFloat(frame.get(), {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f});

    auto out = Process(processor, frame.get());
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].channels, 2);
    EXPECT_GT(out[0].samples[100], 0.0f);
}

TEST(AudioProcessor, KeepsAllSourceChannels) {
    AudioProcessor processor(Config());
    auto frame = MakeAudioFrame(AV_SAMPLE_FMT_FLTP, 48000, 8, 128, 0);
    auto out = Process(processor, frame.get());
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].channels, 8);
}

TEST(AudioProcessor, FramesWithoutTimestampsAreSequential) {
    AudioProcessor processor(Config());
    std::vector<int64_t> pts;
    for (int i = 0; i < 3; i++) {
        auto frame = MakeAudioFrame(AV_SAMPLE_FMT_FLTP, 48000, 2, 1024, AV_NOPTS_VALUE);
        for (auto &f : Process(processor, frame.get())) pts.push_back(f.pts_us);
    }
    EXPECT_EQ(pts, (std::vector<int64_t>{0, 21333, 42666}));
}

TEST(AudioProcessor, AdaptsWhenInputFormatChanges) {
    AudioProcessor processor(Config());
    auto s16 = MakeAudioFrame(AV_SAMPLE_FMT_S16, 48000, 2, 256, 0);
    auto fltp = MakeAudioFrame(AV_SAMPLE_FMT_FLTP, 44100, 1, 256, 256);
    FillPlanarFloat(fltp.get(), {0.75f});

    ASSERT_EQ(Process(processor, s16.get()).size(), 1u);
    auto out = Process(processor, fltp.get());
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].channels, 1);
    EXPECT_EQ(out[0].sample_rate, 44100);
    EXPECT_FLOAT_EQ(out[0].samples[0], 0.75f);
}

TEST(AudioProcessor, UnspecifiedChannelLayoutKeepsResamplerState) {
    // Plain WAV and raw PCM report channels without an order; that must not look like a format change.
    AudioProcessor processor(Config(48000, 0, {1, 44100}));

    int64_t produced = 0;
    for (int i = 0; i < 10; i++) {
        auto frame = MakeAudioFrame(AV_SAMPLE_FMT_S16, 44100, 1, 1024, i * 1024);
        av_channel_layout_uninit(&frame->ch_layout);
        frame->ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;
        frame->ch_layout.nb_channels = 1;
        for (auto &f : Process(processor, frame.get())) produced += f.samples_per_channel;
    }
    for (auto &f : Flush(processor)) produced += f.samples_per_channel;

    EXPECT_NEAR(static_cast<double>(produced), 10240.0 * 48000 / 44100, 4);
}

TEST(AudioProcessor, KeepsBufferedSamplesWhenInputChanges) {
    AudioProcessor processor(Config(48000, 0, {1, 44100}));

    int64_t produced = 0;
    auto s16 = MakeAudioFrame(AV_SAMPLE_FMT_S16, 44100, 1, 1024, 0);
    auto fltp = MakeAudioFrame(AV_SAMPLE_FMT_FLTP, 44100, 1, 1024, 1024);
    FillPlanarFloat(fltp.get(), {0.1f});
    for (auto &f : Process(processor, s16.get())) produced += f.samples_per_channel;
    for (auto &f : Process(processor, fltp.get())) produced += f.samples_per_channel;
    for (auto &f : Flush(processor)) produced += f.samples_per_channel;

    EXPECT_NEAR(static_cast<double>(produced), 2048.0 * 48000 / 44100, 4);
}
