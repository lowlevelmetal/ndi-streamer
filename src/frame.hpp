/**
 * @file frame.hpp
 * @brief AVFrame related functions
 * @version 1.0
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 */

#pragma once

// 3rd Party Dependencies
extern "C" {
#include <libavformat/avformat.h>
}

namespace AV::Utils {

/**
 * @brief Copy an AVFrame
 * @param frame The frame to copy
 * @return AVFrame* The copied frame
 */
AVFrame *CopyFrame(AVFrame *frame);

/**
 * @brief Create an NV12 buffer from an AVFrame
 * @param frame The frame to create the buffer from
 * @return uint8_t* The NV12 buffer
 */
uint8_t *ConvertNV12BufferedToNV12(const AVFrame *frame);

/**
 * 
 */
void PrintPictType(AVPictureType type);

} // namespace AV::Utils