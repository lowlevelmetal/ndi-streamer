/**
 * @file averror.hpp
 * @brief This file includes utilities for handling errors in the AV namespace.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

#include <exception>

namespace AV::Utils {

/**
 * @brief The DemuxerError enum class represents the possible errors that can occur when demuxing a media file.
 */
enum class AvError {
    NOERROR,
    OPENINPUT,
    AVDICTSET,
    READFRAME,
    PACKETALLOC,
    FRAMEALLOC,
    FINDSTREAMINFO,
    FINDDECODER,
    DECODERALLOC,
    DECPARAMS,
    SENDPACKET,
    RECIEVEFRAME,
    DECODEREXHAUSTED,
    SWSCONTEXT,
    SWSSCALE,
    AVMALLOC,
    IMAGEFILLARRAYS,
    
};

/**
 * @brief The DemuxerException class represents an exception that is thrown when an error occurs while demuxing a media file.
 */
class AvException : public std::exception {
public:
    AvException(AvError errcode);

    const char *what() const noexcept override;
    const int code() const noexcept;

private:
    AvError m_errcode;
};

} // namespace AV::Utils