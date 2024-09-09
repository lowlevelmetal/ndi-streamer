/**
 * @file demuxer.cpp
 * @brief This file includes utilities for demuxing media files.
 * @date 2024-09-04
 * @author Matthew Todd Geiger
 */

#include "demuxer.hpp"
#include "macro.hpp"

/**
 * @brief The AV::Utils namespace contains utilities for audio and video processing.
 */
namespace AV::Utils {

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
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvException(AvError::READFRAME)};
    }

    return {m_packet, AvException(AvError::NOERROR)};
}

/**
 * @brief Create a Demuxer object
 *
 * @param path path to media file
 * @return std::pair<std::optional<std::shared_ptr<Demuxer>>, AvException>
 */
DemuxerResult Demuxer::Create(const std::string &path) {
    DEBUG("Demuxer factory called");
    AvException error(AvError::NOERROR);

    // Create a new demuxer object, return nullopt if error
    try {
        return {std::unique_ptr<Demuxer>(new Demuxer(path)), AvException(AvError::NOERROR)};
    } catch (AvException err) {
        DEBUG("Demuxer error: %s", err.what());
        error = err;
    }

    return {nullptr, error};
}

/**
 * @brief Create a Demuxer object
 *
 * @param config demuxer configuration
 * @return std::pair<std::optional<std::shared_ptr<Demuxer>>, AvException>
 */
DemuxerResult Demuxer::Create(const DemuxerConfig &config) {
    DEBUG("Demuxer factory called");
    AvException error(AvError::NOERROR);

    // Create a new demuxer object, return nullopt if error
    try {
        return {std::unique_ptr<Demuxer>(new Demuxer(config)), AvException(AvError::NOERROR)};
    } catch (AvException err) {
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

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
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

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
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
 * @brief Initialize the demuxer
 *
 * @return AvError
 */
AvError Demuxer::m_Initialize() {

    // Set the width and height if they are provided
    if (m_config.width && m_config.height) {
        DEBUG("Width: %d, Height: %d", m_config.width, m_config.height);
        if (av_dict_set(&m_opts, "video_size", (std::to_string(m_config.width) + "x" + std::to_string(m_config.height)).c_str(), 0))
            return AvError::AVDICTSET;
    }

    if (!m_config.pixel_format.empty()) {
        DEBUG("Pixel format: %s", m_config.pixel_format.c_str());
        if (av_dict_set(&m_opts, "pixel_format", m_config.pixel_format.c_str(), 0))
            return AvError::AVDICTSET;
    }

    // This initializes the context and opens the file
    //
    // NOTE: It is safe to pass &m_opts even if it is nullptr
    int ret = avformat_open_input(&m_format_ctx, m_config.path.c_str(), nullptr, &m_opts);
    if (ret < 0) {
        DEBUG("avformat_open_input failed");
        PRINT_FFMPEG_ERR(ret);
        return AvError::OPENINPUT;
    }

    // Create the packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        DEBUG("av_packet_alloc failed");
        return AvError::PACKETALLOC;
    }

    // Make sure the stream information is loaded into the format context
    ret = avformat_find_stream_info(m_format_ctx, nullptr);
    if (ret < 0) {
        DEBUG("avformat_find_stream_info failed");
        PRINT_FFMPEG_ERR(ret);
        return AvError::FINDSTREAMINFO;
    }

    return AvError::NOERROR;
}

} // namespace AV::Utils