/**
 * @file decoder.hpp
 * @brief This file includes utilities for decoding media files.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

#include <string>
#include <memory>

extern "C" {
    #include <libavcodec/avcodec.h>
}

#include "averror.hpp"
#include "demuxer.hpp"

namespace AV::Utils {

class Decoder;
using DecoderResult = std::pair<std::unique_ptr<Decoder>, const AvException>;

typedef struct DecoderConfig {
    AVCodecID codec_id{};
    AVCodecParameters *codecpar{};
} decoderconfig, *pdecoderconfig;

/**
 * @brief The Decoder class provides utilities for decoding media files.
 */
class Decoder {
private:
    Decoder(AVCodecID codec_id, AVCodecParameters *codecpar);
    Decoder(const DecoderConfig &config);

public:
    ~Decoder();

    // Factory methods
    static DecoderResult Create(AVCodecID codec_id, AVCodecParameters *codecpar);
    static DecoderResult Create(const DecoderConfig &config);

private:
    AvError m_Initialize();

    DecoderConfig m_config;
    AVCodecContext *m_codec;
};

} // namespace AV::Utils