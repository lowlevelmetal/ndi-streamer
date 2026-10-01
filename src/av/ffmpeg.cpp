/**
 * @file ffmpeg.cpp
 * @brief RAII ownership and error helpers for FFmpeg objects.
 */

#include "av/ffmpeg.hpp"

#include <format>
#include <new>

extern "C" {
#include <libavutil/error.h>
}

namespace ndistreamer::av {

FramePtr MakeFrame() {
    FramePtr frame(av_frame_alloc());
    if (!frame) throw std::bad_alloc();
    return frame;
}

PacketPtr MakePacket() {
    PacketPtr packet(av_packet_alloc());
    if (!packet) throw std::bad_alloc();
    return packet;
}

std::string ErrorString(int errnum) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errnum, buffer, sizeof(buffer));
    return buffer;
}

Error::Error(std::string_view operation, int code)
    : std::runtime_error(std::format("{}: {}", operation, ErrorString(code))), m_code(code) {}

std::string FormatDuration(int64_t us) {
    if (us < 0) us = 0;
    int64_t ms = us / 1000;
    return std::format("{:02}:{:02}:{:02}.{:03}", ms / 3600000, (ms / 60000) % 60, (ms / 1000) % 60, ms % 1000);
}

} // namespace ndistreamer::av
