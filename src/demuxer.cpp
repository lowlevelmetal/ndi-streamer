/**
 * @file demuxer.cpp
 * @brief This file includes utilities for demuxing media files.
 * @date 2024-09-04
 * @author Matthew Todd Geiger
 */

#include "demuxer.hpp"
#include "macro.hpp"

#define DEMUXSTR "[DEMUXER]"

/**
 * @brief The AV::Utils namespace contains utilities for audio and video processing.
 */
namespace AV::Utils {

/**
 * @brief Construct a new DemuxerException:: DemuxerException object
 *
 * @param errcode error code
 */
DemuxerException::DemuxerException(DemuxerError errcode) : m_errcode(errcode) {}

const char *DemuxerException::what() const noexcept {
    switch (m_errcode) {
    case DemuxerError::NOERROR:
        return DEMUXSTR " No error";
    case DemuxerError::OPENINPUT:
        return DEMUXSTR " Error opening input";
    case DemuxerError::AVDICTSET:
        return DEMUXSTR " Error setting dictionary";
    case DemuxerError::READFRAME:
        return DEMUXSTR " Error reading frame";
    case DemuxerError::PACKETALLOC:
        return DEMUXSTR " Error allocating packet";
    case DemuxerError::FINDSTREAMINFO:
        return DEMUXSTR " Error finding stream info";
    default:
        return DEMUXSTR " Unknown error";
    }
}

const int DemuxerException::code() const noexcept {
    return static_cast<int>(m_errcode);
}

/**
 * @brief Get the streams from the media file.
 * 
 * @return GetStreamResult
 */
GetStreamResult Demuxer::GetStreams() {
    std::vector<AVStream *> streams;

    for (unsigned int i = 0; i < m_format_ctx->nb_streams; i++) {
        streams.push_back(m_format_ctx->streams[i]);
    }

    return streams;
}

/**
 * @brief Read the next frame from the media file.
 *
 * @return ReadFrameResult
 */
ReadFrameResult Demuxer::ReadFrame() {
    // Reset the packet to default values
    av_packet_unref(m_packet);

    // Read the next frame
    int ret = av_read_frame(m_format_ctx, m_packet);
    if (ret < 0) {
        return {std::nullopt, DemuxerException(DemuxerError::READFRAME)};
    }

    return {m_packet, DemuxerException(DemuxerError::NOERROR)};
}

/**
 * @brief Create a Demuxer object
 *
 * @param path path to media file
 * @return std::pair<std::optional<std::shared_ptr<Demuxer>>, DemuxerException>
 */
DemuxerResult Demuxer::Create(const std::string &path) {
    DemuxerException error(DemuxerError::NOERROR);

    // Create a new demuxer object, return nullopt if error
    try {
        return {std::unique_ptr<Demuxer>(new Demuxer(path)), DemuxerException(DemuxerError::NOERROR)};
    } catch (DemuxerException err) {
        DEBUG("Demuxer error: %s", err.what());
        error = err;
    }

    return {nullptr, error};
}

/**
 * @brief Create a Demuxer object
 * 
 * @param config demuxer configuration
 * @return std::pair<std::optional<std::shared_ptr<Demuxer>>, DemuxerException>
 */
DemuxerResult Demuxer::Create(const DemuxerConfig &config) {
    DemuxerException error(DemuxerError::NOERROR);

    // Create a new demuxer object, return nullopt if error
    try {
        return {std::unique_ptr<Demuxer>(new Demuxer(config)), DemuxerException(DemuxerError::NOERROR)};
    } catch (DemuxerException err) {
        DEBUG("Demuxer error: %s", err.what());
        error = err;
    }

    return {nullptr, error};
}

/**
 * @brief Construct a new Demuxer:: Demuxer object
 *
 * @param path path to media file
 */
Demuxer::Demuxer(const std::string &path) {
    DEBUG("Constructing Demuxer object");

    m_config.path = path;

    DemuxerError err = m_InitializeAuto();
    if (err != DemuxerError::NOERROR) {
        throw err; // you can throw the error code because the compiler is smart enough to call the constructor
    }
}

/**
 * @brief Construct a new Demuxer:: Demuxer object
 * 
 * @param config demuxer configuration
 */
Demuxer::Demuxer(const DemuxerConfig &config) : m_config(config) {
    DEBUG("Constructing Demuxer object");

    m_config = config;

    DemuxerError err = m_InitializeWithConfig();
    if (err != DemuxerError::NOERROR) {
        throw err; // you can throw the error code because the compiler is smart enough to call the constructor
    }
}

/**
 * @brief Destroy the Demuxer:: Demuxer object
 */
Demuxer::~Demuxer() {
    DEBUG("Destructing Demuxer object");

    // Free the packet
    if (m_packet != nullptr) {
        av_packet_free(&m_packet);
        DEBUG("av_packet_free called");
    }

    if (m_format_ctx != nullptr) {
        avformat_close_input(&m_format_ctx);
        DEBUG("avformat_close_input called");
    }

    // Free the dictionary
    if (m_opts != nullptr) {
        av_dict_free(&m_opts);
        DEBUG("av_dict_free called");
    }
}

/**
 * @brief Initialize the demuxer.
 *
 * @return DemuxerError
 */
DemuxerError Demuxer::m_InitializeAuto() {
    // This initializes the context and opens the file
    int ret = avformat_open_input(&m_format_ctx, m_config.path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        DEBUG("avformat_open_input failed");
        return DemuxerError::OPENINPUT;
    }

    return m_Initialize();
}

/**
 * @brief Initialize the demuxer with a configuration.
 * 
 * @return DemuxerError
 */
DemuxerError Demuxer::m_InitializeWithConfig() {
    // Set the width and height if they are provided
    if (m_config.width && m_config.height) {
        DEBUG("Width: %d, Height: %d", m_config.width, m_config.height);
        if(av_dict_set(&m_opts, "video_size", (std::to_string(m_config.width) + "x" + std::to_string(m_config.height)).c_str(), 0))
            return DemuxerError::AVDICTSET;
    }

    if(!m_config.pixel_format.empty()) {
        DEBUG("Pixel format: %s", m_config.pixel_format.c_str());
        if(av_dict_set(&m_opts, "pixel_format", m_config.pixel_format.c_str(), 0))
            return DemuxerError::AVDICTSET;
    }

    // This initializes the context and opens the file
    int ret = avformat_open_input(&m_format_ctx, m_config.path.c_str(), nullptr, &m_opts);
    if (ret < 0) {
        DEBUG("avformat_open_input failed");
        return DemuxerError::OPENINPUT;
    }

    return m_Initialize();
}

DemuxerError Demuxer::m_Initialize() {

    // Create the packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        DEBUG("av_packet_alloc failed");
        return DemuxerError::PACKETALLOC;
    }

    // Make sure the stream information is loaded into the format context
    int ret = avformat_find_stream_info(m_format_ctx, nullptr);
    if (ret < 0) {
        DEBUG("avformat_find_stream_info failed");
        return DemuxerError::FINDSTREAMINFO;
    }

    return DemuxerError::NOERROR;
}

} // namespace AV::Utils