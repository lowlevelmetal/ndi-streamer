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
            case AvErrorCode::CodecContext:
                return std::string("Failed to allocate space for codec context");
            case AvErrorCode::CodecParameters:
                return std::string("Failed to fill codec context with codec parameters");
            case AvErrorCode::CodecOpen:
                return std::string("Failed to open codec for usage");
            case AvErrorCode::FrameAlloc:
                return std::string("Failed to allocate space for frame");
            case AvErrorCode::PacketAlloc:
                return std::string("Failed to allocate space for packet");
            case AvErrorCode::PacketsClaimed:
                return std::string("Video packets have been exhausted");
            case AvErrorCode::FileNotOpened:
                return std::string("File has not been opened for processing");
            case AvErrorCode::NullPointer:
                return std::string("Null pointer exception");
            case AvErrorCode::FrameRead:
                return std::string("Frame read error");
            case AvErrorCode::PacketSend:
                return std::string("Failed to send raw packet to decoder");
            case AvErrorCode::RecieveFrame:
                return std::string("Failed to fill frame with decoded output");
            case AvErrorCode::NotVideoFrame:
                return std::string("Frame is not a video frame");
            case AvErrorCode::NotAudioFrame:
                return std::string("Frame is not an audio frame");
        }

        return std::string("Invalid AV error code");
    }

}