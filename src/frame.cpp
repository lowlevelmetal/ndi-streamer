/**
 * @file frame.cpp
 * @brief AVFrame related functions
 * @version 1.0
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 */

#include "frame.hpp"
#include "macro.hpp"

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

} // namespace AV::Utils