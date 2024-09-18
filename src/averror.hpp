/**
 * @file averror.hpp
 * @brief This file includes utilities for handling errors in the AV namespace.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

#include <atomic>
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
    SWRALLOCS,
    SWRINIT,
    AVSAMPLESALLOC,
    SWRCONVERT,
    NDISENDINSTANCE,
    NDIINVALIDPIXFMT,
    FRAMEGETBUFFER,
    SWRCONFIG,
    STREAMCOUNT,
    NDISOURCECREATE,
    FRAMEREF,
    SAMPLECOPY,
    BUFFERFULL,
    PACKETREF,
    BUFFEREMPTY,
    INVALIDFRAME,
    INVALIDSMPLFMT,
    DEMUXEREOF,
    HWDEVICECTXALLOC,
    HWDEVICEGETBUF,
    FILTERGRAPHALLOC,
    INOUTALLOC,
    FILTER_GRAPH_ALLOC,
    FILTER_GRAPH_PARSE,
    FILTER_GRAPH_CONFIG,
    FILTER_GRAPH_CREATE_FILTER,
    BUFFERSRC_ADD_FRAME,
    BUFFERSINK_GET_FRAME
};

/**
 * @brief The DemuxerException class represents an exception that is thrown when an error occurs while demuxing a media file.
 */
class AvException : public std::exception {
public:
    /**
     * @brief Construct a new AvException:: AvException object
     *
     * @param errcode error code
     */
    AvException(AvError errcode);

    /**
     * @brief Construct a new AvException:: AvException object
     */
    AvException();

    /**
     * @brief Get the error message
     *
     * @return const char*
     */
    const char *what() const noexcept override;

    /**
     * @brief Get the error code
     *
     * @return const int
     */
    const int code() const noexcept;

private:
    AvError m_errcode;
};

} // namespace AV::Utils