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
        FormatContextAlloc,
        FormatHeader,
        FormatStreamInfo,
        StreamMissing
    };

    std::string AvErrorStr(AvErrorCode &err);

} // namespace AV