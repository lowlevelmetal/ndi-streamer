/**
 * @file ndiavserver.hpp
 * @brief This file includes utilities for working with NDI.
 * @date 2024-09-08 
 * @author Matthew Todd Geiger
 */

#pragma once

// local includes
#include "asyncndisource.hpp"
#include "demuxer.hpp"
#include "decoder.hpp"
#include "pixelencoder.hpp"
#include "audioresampler.hpp"

// Standard includes
#include <vector>

namespace AV::Utils {

class NdiAvServer;

enum DecoderType {
    VIDEO,
    AUDIO
};

using NdiAvServerResult = std::pair<std::unique_ptr<NdiAvServer>, const AvException>;

class NdiAvServer {
private:
    NdiAvServer(const std::string &ndi_name, const std::string &media_path);

public:
    ~NdiAvServer();

    // Factory methods
    static NdiAvServerResult Create(const std::string &ndi_name, const std::string &media_path);

    // Process and send next frame
    AvException ProcessNextFrame();
    double GetTargetFramerate();
    

private:
    AvError m_Initialize();

    std::unique_ptr<Demuxer> m_demuxer;
    std::unique_ptr<Decoder> m_video_decoder;
    std::unique_ptr<Decoder> m_audio_decoder;
    PixelEncoderConfig m_pixel_encoder_config;
    std::unique_ptr<PixelEncoder> m_pixel_encoder;
    AudioResamplerConfig m_audio_resampler_config;
    std::unique_ptr<AudioResampler> m_audio_resampler;
    std::unique_ptr<AsyncNdiSource> m_ndi_source;

    int m_video_stream_index = 0;
    int m_audio_stream_index = 0;

    std::string m_ndi_name;
    std::string m_media_path;

    AVRational m_video_time_base;
    AVRational m_audio_time_base;
    
};

} // namespace AV::Utils