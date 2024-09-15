/**
 * @file decoder.hpp
 * @brief This file includes utilities for decoding media files.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

// Local dependencies
#include "averror.hpp"
#include "demuxer.hpp"

// 3rd party dependencies
extern "C" {
#include <libavcodec/avcodec.h>
}

// Standard C++ dependencies
#include <memory>
#include <string>

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
    /**
     * @brief Destroy the Decoder:: Decoder object
     */
    ~Decoder();

    /**
     * @brief Create a Decoder object
     *
     * @param codecpar codec parameters
     * @return DecoderResult
     */
    static DecoderResult Create(AVCodecParameters *codecpar);

    // Setters
    /**
     * @brief Fill the decoder with a packet
     *
     * @param packet The packet to fill the decoder with
     * @return AvException
     */
    AvException FillDecoder(AVPacket *packet);

    /**
     * @brief Decode a packet. If the decoder needs another packet, it will return
     * an error code of DECODEREXHAUSTED
     *
     * @return DecoderOutput
     */
    DecoderOutput Decode();

    // Getters
    /**
     * @brief Get the frame rate of the decoder
     *
     * @return CodecFrameRate
     */
    CodecFrameRate GetFrameRate();

private:
    AvError m_Initialize();

    // Store the codec parameters
    AVCodecParameters *m_codecpar = nullptr;

    // This is the codec context that ffmpeg uses to decode the media file
    AVCodecContext *m_codec = nullptr;

    // This is the last frame that was decoded
    AVFrame *m_last_frame = nullptr;
};

} // namespace AV::Utils