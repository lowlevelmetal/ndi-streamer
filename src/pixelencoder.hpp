/**
 * @file pixelencoder.hpp
 * @brief This file includes utilities for encoding media files.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

#include <string>
#include <memory>

extern "C" {
    #include <libswscale/swscale.h>
}

#include "averror.hpp"

namespace AV::Utils {

class PixelEncoder;
using PixelEncoderResult = std::pair<std::unique_ptr<PixelEncoder>, const AvException>;
using PixelEncoderOutput = std::pair<AVFrame *, const AvException>;

/**
 * @brief The PixelEncoderConfig struct represents the configuration for the PixelEncoder object.
 */
typedef struct PixelEncoderConfig {
    int src_width{}, src_height{};
    int dst_width{}, dst_height{};
    AVPixelFormat src_pix_fmt{}, dst_pix_fmt{};
} pixelencoderconfig, *ppixelencoderconfig;

/**
 * @brief The PixelEncoder class represents an object that encodes frames.
 */
class PixelEncoder {
private:
    PixelEncoder(const pixelencoderconfig &config);

public:
    ~PixelEncoder();

    // Factory methods
    static PixelEncoderResult Create(const pixelencoderconfig &config);

    // Encode frames
    PixelEncoderOutput Encode(AVFrame *frame);

private:
    AvError m_Initialize();

    pixelencoderconfig m_config;
    SwsContext *m_sws_ctx = nullptr;
    AVFrame *m_dst_frame = nullptr;
    uint8_t *m_dst_frame_buffer = nullptr;
};

} // namespace AV::Utils