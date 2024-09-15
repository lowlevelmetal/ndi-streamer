/**
 * @file pixelencoder.hpp
 * @brief This file includes utilities for encoding media files.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

#include "averror.hpp"

extern "C" {
#include <libswscale/swscale.h>
}

#include <memory>
#include <string>

namespace AV::Utils {

// Forward declarations and type definitions
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
    /**
     * @brief Destroy the PixelEncoder object
     */
    ~PixelEncoder();

    // Factory methods
    /**
     * @brief Create a new PixelEncoder object
     *
     * @param config The configuration for the PixelEncoder object
     * @return PixelEncoderResult The PixelEncoder object
     */
    static PixelEncoderResult Create(const pixelencoderconfig &config);

    // Getters
    /**
     * @brief Encode a frame
     *
     * @param frame The frame to encode
     * @return PixelEncoderOutput The encoded frame
     */
    PixelEncoderOutput Encode(AVFrame *frame);

private:
    AvError m_Initialize();

    // Store the configuration
    pixelencoderconfig m_config;

    // Store the sws context for scaling frames
    SwsContext *m_sws_ctx = nullptr;

    // Store the destination frame
    AVFrame *m_dst_frame = nullptr;

    // Store the destination frame buffer;
    uint8_t *m_dst_frame_buffer = nullptr;
};

} // namespace AV::Utils