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
 * @brief Construct a new DemuxerException:: DemuxerException object
 *
 * @param errcode error code
 */
DemuxerException::DemuxerException(DemuxerError errcode) : m_errcode(errcode) {}

const char *DemuxerException::what() const noexcept {
    switch (m_errcode) {
    case DemuxerError::NOERROR:
        return "[Demuxer] No error";
    case DemuxerError::OPENINPUT:
        return "[Demuxer] Error opening input";
    default:
        return "[Demuxer] Unknown error";
    }
}

const int DemuxerException::code() const noexcept {
    return static_cast<int>(m_errcode);
}

/**
 * @brief Create a Demuxer object
 *
 * @param path path to media file
 * @param error error code
 * @return std::pair<std::optional<std::shared_ptr<Demuxer>>, DemuxerException>
 */
DemuxerResult Demuxer::Create(const std::string &path) {
    DemuxerError error = DemuxerError::NOERROR;

    // Create a new demuxer object, return nullopt if error
    try {
        return {std::unique_ptr<Demuxer>(new Demuxer(path)), DemuxerException(DemuxerError::NOERROR)};
    } catch (DemuxerError err) {
        DEBUG("Demuxer error: %d", static_cast<int>(err));
        error = err;
    }

    return {std::nullopt, DemuxerException(error)};
}

/**
 * @brief Construct a new Demuxer:: Demuxer object
 *
 * @param path path to media file
 */
Demuxer::Demuxer(const std::string &path) : m_path(path) {
    DEBUG("Constructing Demuxer object");

    DemuxerError err = m_Initialize();
    if (err != DemuxerError::NOERROR) {
        throw err;
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
}

/**
 * @brief Initialize the demuxer.
 *
 * @return DemuxerError
 */
DemuxerError Demuxer::m_Initialize() {
    int ret = avformat_open_input(&m_format_ctx, m_path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        DEBUG("avformat_open_input failed");
        return DemuxerError::OPENINPUT;
    }

    return DemuxerError::NOERROR;
}

} // namespace AV::Utils