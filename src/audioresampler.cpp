/**
 * @file audioresampler.cpp
 * @brief This file includes utilities for resampling audio.
 * @date 2024-09-07
 * @author Matthew Todd Geiger
 */

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

    DEBUG("Source Frame\n"
          "Sample Rate: %d\n"
          "Samples: %d\n"
          "Channels: %d\n"
          "Sample Format: %s\n",
          src_frame->sample_rate, src_frame->nb_samples, m_src_nb_channels, av_get_sample_fmt_name((AVSampleFormat)src_frame->format));

    // Reset frame each time
    av_frame_unref(m_dst_frame);

    // Setup destination frame
    m_dst_frame->ch_layout = m_config.dstchannellayout;
    m_dst_frame->sample_rate = m_config.dstsamplerate;
    m_dst_frame->format = m_config.dstsampleformat;
    m_dst_frame->nb_samples = av_rescale_rnd(swr_get_delay(m_swr_context, m_config.srcsamplerate) + src_frame->nb_samples, m_config.dstsamplerate, m_config.srcsamplerate, AV_ROUND_UP);

    // Allocate the frame
    int ret = av_frame_get_buffer(m_dst_frame, 0);
    if (ret < 0) {
        return {nullptr, AvException(AvError::FRAMEALLOC)};
    }

    // Configure the context for the frames
    ret = swr_config_frame(m_swr_context, m_dst_frame, src_frame);
    if (ret < 0) {
#ifdef _DEBUG
        char errbuf[AV_ERROR_MAX_STRING_SIZE];    // AV_ERROR_MAX_STRING_SIZE is defined in FFmpeg
        av_strerror(ret, errbuf, sizeof(errbuf)); // Use av_strerror to copy the error message to errbuf
        DEBUG("swr_config_frame failed: %s", errbuf);
#endif
        return {nullptr, AvException(AvError::SWRCONFIG)};
    }

    // Resample the frame
    ret = swr_convert_frame(m_swr_context, m_dst_frame, src_frame);
    if (ret < 0) {
#ifdef _DEBUG
        char errbuf[AV_ERROR_MAX_STRING_SIZE];    // AV_ERROR_MAX_STRING_SIZE is defined in FFmpeg
        av_strerror(ret, errbuf, sizeof(errbuf)); // Use av_strerror to copy the error message to errbuf
        DEBUG("swr_convert_frame failed: %s", errbuf);
#endif
        return {nullptr, AvException(AvError::SWRCONVERT)};
    }

    DEBUG("Resampled Frame\n"
          "Sample Rate: %d\n"
          "Samples: %d\n"
          "Channels: %d\n"
          "Sample Format: %s\n",
          m_dst_frame->sample_rate, m_dst_frame->nb_samples, m_dst_nb_channels, av_get_sample_fmt_name((AVSampleFormat)m_dst_frame->format));

    return {m_dst_frame, AvException(AvError::NOERROR)};
}

/**
 * @brief Create a new AudioResampler object
 *
 * @param config The configuration for the AudioResampler object
 * @return AudioResamplerResult The AudioResampler object
 */
AudioResamplerResult AudioResampler::Create(const AudioResamplerConfig &config) {
    DEBUG("AudioResampler factory called");
    AvException error(AvError::NOERROR);

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
    DEBUG("Constructing AudioResampler object");
    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}

/**
 * @brief Destroy the AudioResampler object
 */
AudioResampler::~AudioResampler() {
    DEBUG("Destroying AudioResampler object");

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
    m_swr_context = swr_alloc();
    if (!m_swr_context) {
        return AvError::SWRALLOCS;
    }

    // Store number of channels
    m_src_nb_channels = m_config.srcchannellayout.nb_channels;
    m_dst_nb_channels = m_config.dstchannellayout.nb_channels;

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
          m_config.srcsamplerate, m_src_nb_channels, av_get_sample_fmt_name(m_config.srcsampleformat));

    DEBUG("Destination Config\n"
          "Sample Rate: %d\n"
          "Channels: %d\n"
          "Sample Format: %s\n",
          m_config.dstsamplerate, m_dst_nb_channels, av_get_sample_fmt_name(m_config.dstsampleformat));

    int ret = swr_init(m_swr_context);
    if (ret < 0) {
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