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

    // Allocate buffer
    uint8_t *buffer = new uint8_t[frame->width * frame->height * 3 / 2];

    // Copy Y plane
    for (int i = 0; i < frame->height; i++) {
        memcpy(buffer + i * frame->width, frame->data[0] + i * frame->linesize[0], frame->width);
    }

    // Copy UV plane
    for (int i = 0; i < frame->height / 2; i++) {
        memcpy(buffer + frame->width * frame->height + i * frame->width, frame->data[1] + i * frame->linesize[1], frame->width);
    }

    return buffer;
}



} // namespace AV::Utils