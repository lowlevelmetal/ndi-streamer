/*
 * ndi-streamer
 * averror.hpp
 *
 * 09-20-2024
 * Matthew Todd Geiger
 */

#pragma once

// Standard includes
#include <string>

namespace AV {

    enum class AvErrorCode {
        NoError,
        DecoderNotInitialized,
        FileNotOpened,
        FormatContextAlloc,
        FormatHeader,
        FormatStreamInfo,
        StreamMissing,
        CodecContext,
        CodecParameters,
        CodecOpen,
        FrameAlloc,
        PacketAlloc,
        PacketsClaimed,
        FrameRead,
        NullPointer
    };

    std::string AvErrorStr(AvErrorCode &err);

} // namespace AV