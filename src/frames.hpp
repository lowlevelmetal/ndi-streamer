/**
 * @file frames.hpp
 * @brief Uncompressed frames in a layout NDI accepts directly.
 */

#pragma once

#include "av/ffmpeg.hpp"

#include <cstdint>
#include <vector>

namespace ndistreamer {

/**
 * @brief NDI video pixel layouts produced by the pipeline.
 * Values are the NDI FourCC codes, so they can be passed to the SDK unchanged.
 */
enum class VideoFourCC : uint32_t {
    UYVY = 0x59565955, // 'U','Y','V','Y'
    NV12 = 0x3231564E, // 'N','V','1','2'
    I420 = 0x30323449, // 'I','4','2','0'
    BGRA = 0x41524742, // 'B','G','R','A'
    BGRX = 0x58524742, // 'B','G','R','X'
    RGBA = 0x41424752, // 'R','G','B','A'
    RGBX = 0x58424752, // 'R','G','B','X'
};

const char *ToString(VideoFourCC fourcc);

/**
 * @brief A video frame ready to hand to NDI.
 *
 * The pixel memory is owned by @c storage (an AVFrame reference), so the frame can be moved between
 * threads and kept alive while NDI processes it asynchronously. Multi-planar layouts (NV12, I420)
 * are contiguous, as NDI requires.
 */
struct VideoFrame {
    av::FramePtr storage;
    uint8_t *data = nullptr;
    int stride = 0; ///< Bytes per line of the first plane.
    int width = 0;
    int height = 0;
    VideoFourCC fourcc = VideoFourCC::UYVY;
    int frame_rate_num = 30000;
    int frame_rate_den = 1001;
    float aspect_ratio = 0.0f; ///< Display aspect ratio; 0 means square pixels.
    bool interlaced = false;
    int64_t pts_us = 0;
    int64_t duration_us = 0;
};

/**
 * @brief A block of planar 32-bit float audio ready to hand to NDI.
 * Channel @c c starts at samples[c * channel_stride].
 */
struct AudioFrame {
    std::vector<float> samples;
    int sample_rate = 0;
    int channels = 0;
    int samples_per_channel = 0;
    int channel_stride = 0; ///< Distance between channel starts, in samples.
    int64_t pts_us = 0;
};

} // namespace ndistreamer
