/**
 * @file VAAPIDecoder.hpp
 * @brief This file includes the VAAPIDecoder class.
 * @version 1.0
 * @date 2024-09-19
 * @author Matthew Todd Geiger
 */

#pragma once

// Local dependencies
#include "averror.hpp"
#include "demuxer.hpp"

// 3rd party dependencies
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
}

// Standard C++ dependencies
#include <memory>
#include <string>

namespace AV::Utils {

class VAAPIDecoder;
using VAAPIDecoderResult = std::pair<std::unique_ptr<VAAPIDecoder>, const AvException>;
using VAAPIDecoderOutput = std::pair<AVFrame *, const AvException>;
using CodecFrameRate = std::pair<int, int>;

/**
 * @brief The VAAPIDecoder class provides utilities for decoding media files.
 */
class VAAPIDecoder {
private:
    VAAPIDecoder(AVCodecParameters *codecpar);

public:
    /**
     * @brief Destroy the VAAPIDecoder:: VAAPIDecoder object
     */
    ~VAAPIDecoder();

    /**
     * @brief Create a VAAPIDecoder object
     *
     * @param codecpar codec parameters
     * @return VAAPIDecoderResult
     */
    static VAAPIDecoderResult Create(AVCodecParameters *codecpar);

    // Setters
    /**
     * @brief Fill the VAAPIDecoder with a packet
     *
     * @param packet The packet to fill the VAAPIDecoder with
     * @return AvException
     */
    AvException FillVAAPIDecoder(AVPacket *packet);

    /**
     * @brief Decode a packet. If the VAAPIDecoder needs another packet, it will return
     * an error code of VAAPIDecoderEXHAUSTED
     *
     * @return VAAPIDecoderOutput
     */
    VAAPIDecoderOutput Decode();

    // Getters
    /**
     * @brief Get the frame rate of the VAAPIDecoder
     *
     * @return CodecFrameRate
     */
    CodecFrameRate GetFrameRate();

    AVPixelFormat GetPixelFormat();

private:
    AvError m_Initialize();

    // Store the codec parameters
    AVCodecParameters *m_codecpar = nullptr;

    // This is the codec context that ffmpeg uses to decode the media file
    AVCodecContext *m_codec = nullptr;

    // This is the last frame that was decoded
    AVFrame *m_last_frame = nullptr;

    // Reference to the hardware device context
    AVBufferRef *m_hw_device_ctx = nullptr;

    AVPixelFormat m_hw_pix_fmt = AV_PIX_FMT_NONE;
};

} // namespace AV::Utils