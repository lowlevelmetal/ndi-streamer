/**
 * @file averror.cpp
 * @brief This file includes utilities for handling errors in the AV namespace.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#include "averror.hpp"

#define DEMUXSTR "[AVERROR]"

namespace AV::Utils {

/**
 * @brief Construct a new AvException:: AvException object
 *
 * @param errcode error code
 */
AvException::AvException(AvError errcode) : m_errcode(errcode) {}

/**
 * @brief Construct a new AvException:: AvException object
 */
AvException::AvException() : m_errcode(AvError::NOERROR) {}

/**
 * @brief Get the error message
 *
 * @return const char*
 */
const char *AvException::what() const noexcept {
    switch (m_errcode) {
    case AvError::NOERROR:
        return DEMUXSTR " No error";
    case AvError::OPENINPUT:
        return DEMUXSTR " Error opening input";
    case AvError::AVDICTSET:
        return DEMUXSTR " Error setting dictionary";
    case AvError::READFRAME:
        return DEMUXSTR " Error reading frame";
    case AvError::FRAMEALLOC:
        return DEMUXSTR " Error allocating frame";
    case AvError::PACKETALLOC:
        return DEMUXSTR " Error allocating packet";
    case AvError::FINDSTREAMINFO:
        return DEMUXSTR " Error finding stream info";
    case AvError::FINDDECODER:
        return DEMUXSTR " Error finding decoder";
    case AvError::DECODERALLOC:
        return DEMUXSTR " Error allocating decoder";
    case AvError::DECPARAMS:
        return DEMUXSTR " Error with codec parameters";
    case AvError::SENDPACKET:
        return DEMUXSTR " Error sending packet to decoder";
    case AvError::RECIEVEFRAME:
        return DEMUXSTR " Error receiving frame from decoder";
    case AvError::DECODEREXHAUSTED:
        return DEMUXSTR " Decoder is exhausted";
    case AvError::SWSCONTEXT:
        return DEMUXSTR " Error creating sws context";
    case AvError::SWSSCALE:
        return DEMUXSTR " Error scaling frame";
    case AvError::AVMALLOC:
        return DEMUXSTR " Error allocating memory";
    case AvError::IMAGEFILLARRAYS:
        return DEMUXSTR " Error filling image arrays";
    case AvError::SWRALLOCS:
        return DEMUXSTR " Error allocating swr context";
    case AvError::SWRINIT:
        return DEMUXSTR " Error initializing swr context";
    case AvError::AVSAMPLESALLOC:
        return DEMUXSTR " Error allocating samples";
    case AvError::SWRCONVERT:
        return DEMUXSTR " Error converting samples";
    case AvError::NDISENDINSTANCE:
        return DEMUXSTR " Error creating NDI send instance";
    case AvError::NDIINVALIDPIXFMT:
        return DEMUXSTR " Invalid NDI pixel format";
    case AvError::FRAMEGETBUFFER:
        return DEMUXSTR " Error getting frame buffer";
    case AvError::SWRCONFIG:
        return DEMUXSTR " Error configuring frame";
    case AvError::STREAMCOUNT:
        return DEMUXSTR " Incorrect number of streams";
    case AvError::NDISOURCECREATE:
        return DEMUXSTR " Error creating NDI source";
    case AvError::FRAMEREF:
        return DEMUXSTR " Error referencing frame";
    case AvError::SAMPLECOPY:
        return DEMUXSTR " Error copying samples";
    case AvError::BUFFERFULL:
        return DEMUXSTR " Buffer is full";
    case AvError::PACKETREF:
        return DEMUXSTR " Error referencing packet";
    case AvError::BUFFEREMPTY:
        return DEMUXSTR " Buffer is empty";
    case AvError::INVALIDFRAME:
        return DEMUXSTR " Invalid frame";
    default:
        return DEMUXSTR " Unknown error";
    }
}

/**
 * @brief Get the error code
 *
 * @return const int
 */
const int AvException::code() const noexcept {
    return static_cast<int>(m_errcode);
}

} // namespace AV::Utils