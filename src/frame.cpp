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
uint8_t *ConvertNV12BufferedToNV12(const AVFrame *frame) {
    FUNCTION_CALL_DEBUG();


    // Calculate correct buffer size
    uint target_y_size = frame->width * frame->height;
    uint target_uv_size = (frame->width / 2) * (frame->height / 2) * 2; // Each UV plane is half width and height, but interleaved so * 2
    uint target_size = target_y_size + target_uv_size;

    // Allocate new buffer
    uint8_t *target_buffer = new uint8_t[target_size];

    // Copy Y plane
    memcpy(target_buffer, frame->data[0], target_y_size);

    // Fix UV plane and copy into the correct structure
    uint8_t *src_uv = frame->data[1];  // UV plane from source frame
    uint8_t *dst_uv = target_buffer + target_y_size;  // Destination for UV in target buffer

    // Loop through the source UV data and remove null bytes between U and V
    for (int i = 0, j = 0; i < target_uv_size / 2; i++, j += 2) {
        dst_uv[i * 2] = src_uv[j];       // Copy U byte
        dst_uv[i * 2 + 1] = src_uv[j + 1]; // Copy V byte
    }

    return target_buffer;
}



} // namespace AV::Utils