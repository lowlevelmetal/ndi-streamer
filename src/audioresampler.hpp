/**
 * @file audioresampler.hpp
 * @brief This file includes utilities for resampling audio.
 * @date 2024-09-07
 * @author Matthew Todd Geiger
 */

#pragma once

#include <memory>

extern "C" {
#include <libswresample/swresample.h>
}

#include "averror.hpp"

namespace AV::Utils {

class AudioResampler;
using AudioResamplerResult = std::pair<std::unique_ptr<AudioResampler>, AvException>;
using AudioResamplerOutput = std::pair<AVFrame *, AvException>;


typedef struct AudioResamplerConfig {
    int srcsamplerate, dstsamplerate;
    int srclinesize, dstlinesize;
    AVChannelLayout srcchannellayout, dstchannellayout;
    AVSampleFormat srcsampleformat, dstsampleformat;
} AudioResamplerConfig, *pAudioResamplerConfig;

class AudioResampler {
private:
    AudioResampler(const AudioResamplerConfig &config);

public:
    ~AudioResampler();

    // Factory methods
    static AudioResamplerResult Create(const AudioResamplerConfig &config);

    AudioResamplerOutput Resample(AVFrame *src_frame);
    
private:
    AvError m_Initialize();

    AudioResamplerConfig m_config;
    SwrContext *m_swr_context = nullptr;
    AVFrame *m_dst_frame = nullptr;
    uint m_dst_nb_channels = 0;
    uint m_src_nb_channels = 0;
};

} // namespace AV::Utils