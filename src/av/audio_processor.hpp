/**
 * @file audio_processor.hpp
 * @brief Turns decoded audio frames into planar float blocks for NDI.
 */

#pragma once

#include "av/ffmpeg.hpp"
#include "frames.hpp"

#include <functional>
#include <optional>

namespace ndistreamer::av {

struct AudioProcessorConfig {
    int sample_rate = 0; ///< Output sample rate; 0 keeps the source rate.
    int channels = 0;    ///< Output channel count; 0 keeps the source channels.
    AVRational time_base{1, 1000000}; ///< Time base of incoming frame timestamps.
};

/**
 * @brief Converts any sample format/layout/rate to NDI's planar 32-bit float.
 *
 * Most decoders (AAC, MP3, AC-3, Opus...) already output planar float, in which case samples are only
 * copied into one contiguous block. All source channels are kept unless a channel count is requested.
 *
 * One block is held back so that, if the next block starts before it ends, its tail can be trimmed.
 * That happens at loop points with decoders that output encoder padding past the end of the media.
 */
class AudioProcessor {
public:
    using Sink = std::function<bool(AudioFrame &&)>;

    explicit AudioProcessor(AudioProcessorConfig config);
    ~AudioProcessor();

    AudioProcessor(const AudioProcessor &) = delete;
    AudioProcessor &operator=(const AudioProcessor &) = delete;

    void Process(const AVFrame *frame, const Sink &sink);

    /**
     * @brief Emit everything still held back, including samples buffered in the resampler.
     */
    void Flush(const Sink &sink);

private:
    bool Emit(AudioFrame &&out, const Sink &sink);
    bool NeedsReconfigure(const AVFrame *frame) const;
    void Configure(const AVFrame *frame);
    bool Resample(const uint8_t *const *input, int input_samples, int64_t input_pts_us, const Sink &sink);

    AudioProcessorConfig m_config;

    SwrPtr m_swr;
    bool m_configured = false;
    bool m_passthrough = false;
    int m_in_rate = 0;
    int m_in_format = -1;
    AVChannelLayout m_source_layout{}; ///< As reported by the decoder.
    AVChannelLayout m_in_layout{};     ///< As given to the resampler (unspecified orders completed).
    AVChannelLayout m_out_layout{};
    int m_out_rate = 0;

    int64_t m_next_in_us = 0;
    std::optional<AudioFrame> m_held; ///< The most recent block, not yet emitted.
    bool m_reported = false;
};

} // namespace ndistreamer::av
