/**
 * @file frame.cpp
 * @brief AVFrame related functions
 * @version 1.0
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 */

#include "frame.hpp"
#include "macro.hpp"

extern "C" {
#include <libavutil/imgutils.h>
}

namespace AV::Utils {

/**
 * @brief Copy an AVFrame
 * @param frame The frame to copy
 * @return AVFrame* The copied frame
 */
AVFrame *CopyFrame(AVFrame *frame) {
    FUNCTION_CALL_DEBUG();

    AVFrame *new_frame = av_frame_alloc();
    if (!new_frame) {
        return nullptr;
    }

    if (av_frame_ref(new_frame, frame) < 0) {
        av_frame_free(&new_frame);
        return nullptr;
    }

    return new_frame;
}

/**
 * @brief Combine all planes of NV12 into single buffer
 * 
 * @param frame The frame to create the buffer from
 * @return uint8_t* The NV12 buffer
 */
uint8_t *CreateNV12Buffer(const AVFrame *frame) {
    FUNCTION_CALL_DEBUG();

    // NV12: Y plane followed by interleaved UV plane
    uint y_plane_size = frame->linesize[0] * frame->height;        // Y plane size (full height)
    uint uv_plane_size = frame->linesize[1] * (frame->height / 2); // UV plane size (half height)
    uint buffer_size = y_plane_size + uv_plane_size;               // Total buffer size

    // Allocate buffer
    uint8_t *buffer = new uint8_t[buffer_size];

    // Copy Y plane (no need to adjust since linesizes are the same for both planes)
    memcpy(buffer, frame->data[0], y_plane_size);

    // Copy UV plane
    memcpy(buffer + y_plane_size, frame->data[1], uv_plane_size);

    // Debug output to inspect the first few bytes of each plane
    DEBUG("First Y plane bytes: %02x %02x %02x %02x", buffer[0], buffer[1], buffer[2], buffer[3]);
    DEBUG("First UV plane bytes: %02x %02x %02x %02x", buffer[y_plane_size], buffer[y_plane_size + 1], buffer[y_plane_size + 2], buffer[y_plane_size + 3]);

    return buffer;
}



} // namespace AV::Utils