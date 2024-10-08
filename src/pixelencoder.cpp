/**
 * @file pixelencoder.cpp
 * @brief This file includes utilities for encoding media files.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#include <chrono>

#include "pixelencoder.hpp"
#include "macro.hpp"

extern "C" {
#include <libavutil/imgutils.h>
}

namespace AV::Utils {

/**
 * @brief Encode a frame
 * 
 * @param frame The frame to encode
 * @return PixelEncoderOutput The encoded frame
 */
PixelEncoderOutput PixelEncoder::Encode(AVFrame *frame) {
FUNCTION_CALL_DEBUG();
#ifdef _DEBUG
    // Profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif

    m_dst_frame->pts = frame->pts;
    m_dst_frame->pkt_dts = frame->pkt_dts;
    m_dst_frame->pict_type = frame->pict_type;
    m_dst_frame->quality = frame->quality;
    m_dst_frame->opaque = frame->opaque;
    m_dst_frame->best_effort_timestamp = frame->best_effort_timestamp;

    // Scale the frame into the destination frame
    int ret = sws_scale(m_sws_ctx, frame->data, frame->linesize, 0, m_config.src_height, m_dst_frame->data, m_dst_frame->linesize);
    if (ret < 0) {
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvException(AvError::SWSSCALE)};
    }

    // Print new frame metadata
    DEBUG("Pixel Encoded Frame Metadata\n"
            "  pts: %ld\n"
            "  pkt_dts: %ld\n"
            "  pict_type: %d\n"
            "  quality: %d\n"
            "  opaque: %p\n"
            "  best_effort_timestamp: %ld",
            m_dst_frame->pts, m_dst_frame->pkt_dts, m_dst_frame->pict_type, m_dst_frame->quality, m_dst_frame->opaque, m_dst_frame->best_effort_timestamp);

#ifdef _DEBUG
    // Profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("Encode time (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif

    return {m_dst_frame, AvException(AvError::NOERROR)};
}

/**
 * @brief Create a new PixelEncoder object
 * 
 * @param config The configuration for the PixelEncoder object
 * @return PixelEncoderResult The PixelEncoder object
 */
PixelEncoderResult PixelEncoder::Create(const pixelencoderconfig &config) {
    FUNCTION_CALL_DEBUG();
    AvException error;

    // Create a new pixel encoder object, return nullopt if error
    try {
        return {std::unique_ptr<PixelEncoder>(new PixelEncoder(config)), error};
    } catch (AvException e) {
        error = e;
        DEBUG("PixelEncoder error: %s", error.what());
    }

    return {nullptr, error};
}

/**
 * @brief Construct a new PixelEncoder object
 * 
 * @param config The configuration for the PixelEncoder object
 */
PixelEncoder::PixelEncoder(const pixelencoderconfig &config) : m_config(config) {
    FUNCTION_CALL_DEBUG();

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}

/**
 * @brief Destroy the PixelEncoder object
 */
PixelEncoder::~PixelEncoder() {
    FUNCTION_CALL_DEBUG();

    // Free the frame buffer
    if (m_dst_frame_buffer) {
        av_free(m_dst_frame_buffer);
        DEBUG("av_free called");
    }

    // Free the frame
    if (m_dst_frame) {
        av_frame_free(&m_dst_frame);
        DEBUG("av_frame_free called");
    }

    // Free the sws context
    if (m_sws_ctx) {
        sws_freeContext(m_sws_ctx);
        DEBUG("sws_freeContext called");
    }
}

/**
 * @brief Initialize the PixelEncoder object
 * 
 * @return AvError The error code
 */
AvError PixelEncoder::m_Initialize() {
    FUNCTION_CALL_DEBUG();

    // Create the sws context for scaling frames
    m_sws_ctx = sws_getContext(m_config.src_width, m_config.src_height, m_config.src_pix_fmt,
                               m_config.dst_width, m_config.dst_height, m_config.dst_pix_fmt,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!m_sws_ctx) {
        return AvError::SWSCONTEXT;
    }

    // Lets create a frame to store the converted image
    m_dst_frame = av_frame_alloc();
    if (!m_dst_frame) {
        return AvError::FRAMEALLOC;
    }

    // Allocate the buffer for the resize frame
    int buffersize = av_image_get_buffer_size(m_config.dst_pix_fmt, m_config.dst_width, m_config.dst_height, 1);
    m_dst_frame_buffer = (uint8_t *)av_malloc(buffersize);
    if (!m_dst_frame_buffer) {
        return AvError::AVMALLOC;
    }

    // Setup the data pointer to point to our buffer and set the linesize
    int ret = av_image_fill_arrays(m_dst_frame->data, m_dst_frame->linesize, m_dst_frame_buffer, m_config.dst_pix_fmt, m_config.dst_width, m_config.dst_height, 1);
    if (ret < 0) {
        PRINT_FFMPEG_ERR(ret);
        return AvError::IMAGEFILLARRAYS;
    }

    // Copy important metadata to the destination frame
    m_dst_frame->width = m_config.dst_width;
    m_dst_frame->height = m_config.dst_height;
    m_dst_frame->format = m_config.dst_pix_fmt;

    return AvError::NOERROR;
}

} // namespace AV::Utils