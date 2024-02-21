/*
 * ndi-streamer
 * averror.cpp
 *
 * 09-20-2024
 * Matthew Todd Geiger
 */

// Local includes
#include "averror.hpp"

namespace AV {
    std::string AvErrorStr(AvErrorCode &err) {
        switch(err) {
            case AvErrorCode::NoError:
                return std::string("No AV error code detected");
            case AvErrorCode::DecoderNotInitialized:
                return std::string("AV decoder is not yet initialized");
            case AvErrorCode::FormatContextAlloc:
                return std::string("AV failed to allocate memory for format context");
            case AvErrorCode::FormatHeader:
                return std::string("Failed to read file format header");
            case AvErrorCode::FormatStreamInfo:
                return std::string("Failed to get av file stream information");
            case AvErrorCode::StreamMissing:
                return std::string("Failed to find AV stream in file");
        }

        return std::string("Invalid AV error code");
    }

}