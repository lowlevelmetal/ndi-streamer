/**
 * @file averror.cpp
 * @brief This file includes utilities for handling errors in the AV namespace.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#include "averror.hpp"

#define DEMUXSTR "[DEMUXER]"

namespace AV::Utils {

/**
 * @brief Construct a new AvException:: AvException object
 *
 * @param errcode error code
 */
AvException::AvException(AvError errcode) : m_errcode(errcode) {}

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
    case AvError::PACKETALLOC:
        return DEMUXSTR " Error allocating packet";
    case AvError::FINDSTREAMINFO:
        return DEMUXSTR " Error finding stream info";
    case AvError::FINDDECODER:
        return DEMUXSTR " Error finding decoder";
    case AvError::DECODERALLOC:
        return DEMUXSTR " Error allocating decoder";
    default:
        return DEMUXSTR " Unknown error";
    }
}

const int AvException::code() const noexcept {
    return static_cast<int>(m_errcode);
}

} // namespace AV::Utils