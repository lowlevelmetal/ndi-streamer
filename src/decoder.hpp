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
using DecoderOutput = std::pair<AVFrame *, const AvException>;
using CodecFrameRate = std::pair<int, int>;

/**
 * @brief The Decoder class provides utilities for decoding media files.
 */
class Decoder {
private:
    Decoder(AVCodecParameters *codecpar);

public:
    ~Decoder();

    // Factory methods
    static DecoderResult Create(AVCodecParameters *codecpar);

    // Decode frames
    AvException FillDecoder(AVPacket *packet);
    DecoderOutput Decode();

    // Get framerate from context
    CodecFrameRate GetFrameRate();

private:
    AvError m_Initialize();

    AVCodecParameters *m_codecpar = nullptr;
    AVCodecContext *m_codec = nullptr;
    AVFrame *m_last_frame = nullptr;
};

} // namespace AV::Utils