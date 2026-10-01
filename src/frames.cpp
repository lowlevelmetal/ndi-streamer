/**
 * @file frames.cpp
 * @brief Uncompressed frames in a layout NDI accepts directly.
 */

#include "frames.hpp"

namespace ndistreamer {

const char *ToString(VideoFourCC fourcc) {
    switch (fourcc) {
    case VideoFourCC::UYVY:
        return "UYVY";
    case VideoFourCC::NV12:
        return "NV12";
    case VideoFourCC::I420:
        return "I420";
    case VideoFourCC::BGRA:
        return "BGRA";
    case VideoFourCC::BGRX:
        return "BGRX";
    case VideoFourCC::RGBA:
        return "RGBA";
    case VideoFourCC::RGBX:
        return "RGBX";
    }
    return "?";
}

} // namespace ndistreamer
