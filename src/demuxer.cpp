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
    default:
        return DEMUXSTR " Unknown error";
    }
}

const int DemuxerException::code() const noexcept {
    return static_cast<int>(m_errcode);
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

    return {std::nullopt, error};
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

    return {std::nullopt, error};
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

    return DemuxerError::NOERROR;
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
        av_dict_set(&m_opts, "video_size", (std::to_string(m_config.width) + "x" + std::to_string(m_config.height)).c_str(), 0);
    }

    if(!m_config.pixel_format.empty()) {
        DEBUG("Pixel format: %s", m_config.pixel_format.c_str());
        av_dict_set(&m_opts, "pixel_format", m_config.pixel_format.c_str(), 0);
    }

    // This initializes the context and opens the file
    int ret = avformat_open_input(&m_format_ctx, m_config.path.c_str(), nullptr, &m_opts);
    if (ret < 0) {
        DEBUG("avformat_open_input failed");
        return DemuxerError::OPENINPUT;
    }

    return DemuxerError::NOERROR;
}

} // namespace AV::Utils