/**
 * @file frame.cpp
 * @brief AVFrame related functions
 * @version 1.0
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 */

#include "frame.hpp"
#include "macro.hpp"

#include <chrono>

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

/**
 * @brief Print the picture type
 * 
 * @param type The picture type
 */
void PrintPictType(AVPictureType type) {
    FUNCTION_CALL_DEBUG();

    switch (type) {
        case AV_PICTURE_TYPE_I:
            PRINT("Frame Type: I");
            break;
        case AV_PICTURE_TYPE_P:
            PRINT("Frame Type: P");
            break;
        case AV_PICTURE_TYPE_B:
            PRINT("Frame Type: B");
            break;
        case AV_PICTURE_TYPE_S:
            PRINT("Frame Type: S");
            break;
        case AV_PICTURE_TYPE_SI:
            PRINT("Frame Type: SI");
            break;
        case AV_PICTURE_TYPE_NONE:
            PRINT("Frame Type: NONE");
            break;
        case AV_PICTURE_TYPE_SP:
            PRINT("Frame Type: SP");
            break;
        default:
            PRINT("Unkown Frame Type: %d", type);
            break;
    }
}

uint8_t *CombinePlanesNV12(const AVFrame *frame, uint planes) {
    FUNCTION_CALL_DEBUG();

#ifdef _DEBUG
    // profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif

    // Get width and height of each plane
    uint widths[2];
    uint heights[2];

    widths[0] = frame->width;
    heights[0] = frame->height;

    widths[1] = frame->width;
    heights[1] = frame->height / 2;

    // Calculate correct buffer size
    uint target_size = 0;
    for(uint i = 0; i < planes; i++) {
        target_size += frame->linesize[i] * heights[i];
    }

    // Allocate new buffer
    uint8_t *target_buffer = new uint8_t[target_size];

    // Copy each plane into the target buffer
    uint offset = 0;
    for(uint i = 0; i < planes; i++) {
        memcpy(target_buffer + offset, frame->data[i], frame->linesize[i] * heights[i]);
        offset += frame->linesize[i] * heights[i];
    }

#ifdef _DEBUG
    // profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("Combine planes time (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif

    return target_buffer;
}

} // namespace AV::Utils