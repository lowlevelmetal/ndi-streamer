/**
 * @file audio_processor.cpp
 * @brief Turns decoded audio frames into planar float blocks for NDI.
 */

#include "av/audio_processor.hpp"
#include "util/log.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

namespace ndistreamer::av {

namespace {

void CopyLayout(AVChannelLayout &dst, const AVChannelLayout &src) {
    av_channel_layout_uninit(&dst);
    if (src.order == AV_CHANNEL_ORDER_UNSPEC) {
        // Resampling needs to know which channel is which; assume the default order.
        av_channel_layout_default(&dst, src.nb_channels);
    } else {
        Check(av_channel_layout_copy(&dst, &src), "copying channel layout");
    }
}

// When a block starts slightly before the previous one ends, the previous block's tail is trimmed.
// Larger backward jumps are timestamp discontinuities, which the presentation clock deals with.
constexpr int64_t kOverlapToleranceUs = 1000;
constexpr int64_t kMaxOverlapTrimUs = 100000;

int64_t EndUs(const AudioFrame &frame) {
    return frame.pts_us + av_rescale(frame.samples_per_channel, 1000000, frame.sample_rate);
}

std::string DescribeLayout(const AVChannelLayout &layout) {
    char text[128] = "unknown";
    av_channel_layout_describe(&layout, text, sizeof(text));
    return text;
}

} // namespace

AudioProcessor::AudioProcessor(AudioProcessorConfig config) : m_config(config) {}

AudioProcessor::~AudioProcessor() {
    av_channel_layout_uninit(&m_source_layout);
    av_channel_layout_uninit(&m_in_layout);
    av_channel_layout_uninit(&m_out_layout);
}

bool AudioProcessor::NeedsReconfigure(const AVFrame *frame) const {
    // Compare with the layout exactly as the decoder reported it: m_in_layout may have been completed.
    return !m_configured || frame->sample_rate != m_in_rate || frame->format != m_in_format ||
           av_channel_layout_compare(&frame->ch_layout, &m_source_layout) != 0;
}

void AudioProcessor::Configure(const AVFrame *frame) {
    if (m_configured) {
        log::Info("audio input changed to {} Hz, {} channels", frame->sample_rate, frame->ch_layout.nb_channels);
    }

    m_in_rate = frame->sample_rate;
    m_in_format = frame->format;
    av_channel_layout_uninit(&m_source_layout);
    Check(av_channel_layout_copy(&m_source_layout, &frame->ch_layout), "copying channel layout");
    CopyLayout(m_in_layout, frame->ch_layout);

    m_out_rate = m_config.sample_rate > 0 ? m_config.sample_rate : m_in_rate;
    if (m_config.channels > 0) {
        av_channel_layout_uninit(&m_out_layout);
        av_channel_layout_default(&m_out_layout, m_config.channels);
    } else {
        CopyLayout(m_out_layout, m_in_layout);
    }

    m_passthrough = frame->format == AV_SAMPLE_FMT_FLTP && m_out_rate == m_in_rate &&
                    m_out_layout.nb_channels == m_in_layout.nb_channels;

    m_swr.reset();
    if (!m_passthrough) {
        SwrContext *swr = nullptr;
        Check(swr_alloc_set_opts2(&swr, &m_out_layout, AV_SAMPLE_FMT_FLTP, m_out_rate, &m_in_layout,
                                  static_cast<AVSampleFormat>(m_in_format), m_in_rate, 0, nullptr),
              "configuring audio resampler");
        m_swr.reset(swr);
        Check(swr_init(swr), "initializing audio resampler");
    }

    if (!m_reported) {
        m_reported = true;
        std::string source;
        if (!m_passthrough) {
            source = std::format(" (converted from {}, {} Hz, {} ch)",
                                 av_get_sample_fmt_name(static_cast<AVSampleFormat>(m_in_format)), m_in_rate,
                                 m_in_layout.nb_channels);
        }
        log::Info("audio output: float {} Hz, {} ({} ch){}", m_out_rate, DescribeLayout(m_out_layout),
                  m_out_layout.nb_channels, source);
    }
    m_configured = true;
}

bool AudioProcessor::Resample(const uint8_t *const *input, int input_samples, int64_t input_pts_us,
                              const Sink &sink) {
    int capacity = swr_get_out_samples(m_swr.get(), input_samples);
    if (capacity <= 0) return true;

    // Samples still inside the resampler come out first, so the output starts that much earlier.
    int64_t delay_us = swr_get_delay(m_swr.get(), 1000000);

    AudioFrame out;
    out.channels = m_out_layout.nb_channels;
    out.sample_rate = m_out_rate;
    out.channel_stride = capacity;
    out.samples.resize(static_cast<size_t>(out.channels) * capacity);

    std::vector<uint8_t *> planes(out.channels);
    for (int c = 0; c < out.channels; c++) {
        planes[c] = reinterpret_cast<uint8_t *>(out.samples.data() + static_cast<size_t>(c) * capacity);
    }

    int converted = Check(swr_convert(m_swr.get(), planes.data(), capacity, const_cast<const uint8_t **>(input),
                                      input_samples),
                          "converting audio");
    if (converted <= 0) return true;

    out.samples_per_channel = converted;
    out.pts_us = input_pts_us - delay_us;
    return Emit(std::move(out), sink);
}

bool AudioProcessor::Emit(AudioFrame &&out, const Sink &sink) {
    std::optional<AudioFrame> previous = std::exchange(m_held, std::move(out));
    if (!previous) return true;

    int64_t overlap_us = EndUs(*previous) - m_held->pts_us;
    if (overlap_us > kOverlapToleranceUs && overlap_us < kMaxOverlapTrimUs) {
        int64_t keep = av_rescale(m_held->pts_us - previous->pts_us, previous->sample_rate, 1000000);
        previous->samples_per_channel = static_cast<int>(std::clamp<int64_t>(keep, 0, previous->samples_per_channel));
    }
    if (previous->samples_per_channel <= 0) return true;
    return sink(std::move(*previous));
}

void AudioProcessor::Process(const AVFrame *frame, const Sink &sink) {
    if (frame->nb_samples <= 0 || frame->sample_rate <= 0 || frame->ch_layout.nb_channels <= 0) return;
    if (NeedsReconfigure(frame)) {
        Flush(sink); // emit what the old resampler still holds
        Configure(frame);
    }

    int64_t ts = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;
    int64_t pts_us = ts != AV_NOPTS_VALUE ? ToMicros(ts, m_config.time_base) : m_next_in_us;
    m_next_in_us = pts_us + av_rescale(frame->nb_samples, 1000000, frame->sample_rate);

    if (!m_passthrough) {
        Resample(frame->extended_data, frame->nb_samples, pts_us, sink);
        return;
    }

    AudioFrame out;
    out.channels = frame->ch_layout.nb_channels;
    out.sample_rate = frame->sample_rate;
    out.samples_per_channel = frame->nb_samples;
    out.channel_stride = frame->nb_samples;
    out.pts_us = pts_us;
    out.samples.resize(static_cast<size_t>(out.channels) * out.channel_stride);

    // Decoders allocate each plane separately; NDI needs them in one block.
    size_t plane_bytes = static_cast<size_t>(frame->nb_samples) * sizeof(float);
    for (int c = 0; c < out.channels; c++) {
        std::memcpy(out.samples.data() + static_cast<size_t>(c) * out.channel_stride, frame->extended_data[c],
                    plane_bytes);
    }
    Emit(std::move(out), sink);
}

void AudioProcessor::Flush(const Sink &sink) {
    if (m_swr && !Resample(nullptr, 0, m_next_in_us, sink)) return;
    if (m_held) {
        AudioFrame last = std::move(*m_held);
        m_held.reset();
        sink(std::move(last));
    }
}

} // namespace ndistreamer::av
