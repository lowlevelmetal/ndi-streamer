/**
 * @file audioresampler.cpp
 * @brief This file includes utilities for resampling audio.
 * @date 2024-09-07
 * @author Matthew Todd Geiger
 */

#include <chrono>

#include "audioresampler.hpp"
#include "macro.hpp"

extern "C" {
#include <libavutil/opt.h>
}

namespace AV::Utils {

/**
 * @brief Resample the audio frame
 *
 * @param src_frame The frame to resample
 */
AudioResamplerOutput AudioResampler::Resample(AVFrame *src_frame) {
FUNCTION_CALL_DEBUG();
#ifdef _DEBUG
    // Profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif

    // Unlike with the pixel encoder, the number of samples(or resolution for the pixel encoder) is not consistent
    // so we need to reset the frame each time to be safe
    //
    // On modern machines this is extermely fast, but maybe in the future we can optimize this
    // for smaller machines

    // Reset frame each time
    av_frame_unref(m_dst_frame);

    // Setup destination frame
    m_dst_frame->ch_layout = m_config.dstchannellayout;
    m_dst_frame->sample_rate = m_config.dstsamplerate;
    m_dst_frame->format = m_config.dstsampleformat;
    m_dst_frame->nb_samples = av_rescale_rnd(swr_get_delay(m_swr_context, m_config.srcsamplerate) + src_frame->nb_samples, m_config.dstsamplerate, m_config.srcsamplerate, AV_ROUND_UP);
    m_dst_frame->pts = src_frame->pts;

    // Allocate the frame buffer
    int ret = av_frame_get_buffer(m_dst_frame, 0);
    if (ret < 0) {
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvException(AvError::FRAMEALLOC)};
    }

    // Configure the context for the frames, this could also be eventually optimized
    ret = swr_config_frame(m_swr_context, m_dst_frame, src_frame);
    if (ret < 0) {
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvException(AvError::SWRCONFIG)};
    }

    // Resample the frame
    ret = swr_convert_frame(m_swr_context, m_dst_frame, src_frame);
    if (ret < 0) {
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvException(AvError::SWRCONVERT)};
    }

#ifdef _DEBUG
    // Profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("Resample time (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif

    return {m_dst_frame, AvException(AvError::NOERROR)};
}

/**
 * @brief Create a new AudioResampler object
 *
 * @param config The configuration for the AudioResampler object
 * @return AudioResamplerResult The AudioResampler object
 */
AudioResamplerResult AudioResampler::Create(const AudioResamplerConfig &config) {
    FUNCTION_CALL_DEBUG();

    AvException error;

    // Create a new audio resampler object, return nullopt if error
    try {
        return {std::unique_ptr<AudioResampler>(new AudioResampler(config)), error};
    } catch (AvError err) {
        error = AvException(err);
    }

    return {nullptr, error};
}

/**
 * @brief Construct a new AudioResampler object
 */
AudioResampler::AudioResampler(const AudioResamplerConfig &config) : m_config(config) {
    FUNCTION_CALL_DEBUG();

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}

/**
 * @brief Destroy the AudioResampler object
 */
AudioResampler::~AudioResampler() {
    FUNCTION_CALL_DEBUG();

    if (m_dst_frame) {
        DEBUG("Freeing frame");
        av_frame_free(&m_dst_frame);
    }

    if (m_swr_context) {
        DEBUG("Freeing swr context");
        swr_free(&m_swr_context);
    }
}

/**
 * @brief Initialize the audio resampler
 *
 * @return AvError
 */
AvError AudioResampler::m_Initialize() {
    FUNCTION_CALL_DEBUG();

    // Alloc space for swr context
    m_swr_context = swr_alloc();
    if (!m_swr_context) {
        return AvError::SWRALLOCS;
    }

    // Set options
    av_opt_set_chlayout(m_swr_context, "in_channel_layout", &m_config.srcchannellayout, 0);
    av_opt_set_chlayout(m_swr_context, "out_channel_layout", &m_config.dstchannellayout, 0);

    av_opt_set_int(m_swr_context, "in_sample_rate", m_config.srcsamplerate, 0);
    av_opt_set_int(m_swr_context, "out_sample_rate", m_config.dstsamplerate, 0);

    av_opt_set_sample_fmt(m_swr_context, "in_sample_fmt", m_config.srcsampleformat, 0);
    av_opt_set_sample_fmt(m_swr_context, "out_sample_fmt", m_config.dstsampleformat, 0);

    DEBUG("Source Config\n"
          "Sample Rate: %d\n"
          "Channels: %d\n"
          "Sample Format: %s\n",
          m_config.srcsamplerate, m_config.srcchannellayout.nb_channels, av_get_sample_fmt_name(m_config.srcsampleformat));

    DEBUG("Destination Config\n"
          "Sample Rate: %d\n"
          "Channels: %d\n"
          "Sample Format: %s\n",
          m_config.dstsamplerate, m_config.dstchannellayout.nb_channels, av_get_sample_fmt_name(m_config.dstsampleformat));

    // Initialize the context
    int ret = swr_init(m_swr_context);
    if (ret < 0) {
        PRINT_FFMPEG_ERR(ret);
        return AvError::SWRINIT;
    }

    // Setup the destination frame
    m_dst_frame = av_frame_alloc();
    if (!m_dst_frame) {
        return AvError::FRAMEALLOC;
    }

    return AvError::NOERROR;
}

} // namespace AV::Utils