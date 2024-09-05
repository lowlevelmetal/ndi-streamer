/**
 * @file demuxer.hpp
 * @brief This file includes utilities for demuxing media files.
 * @date 2024-09-02
 * @author Matthew Todd Geiger
 */

#include <string>
#include <memory>
#include <optional>

// ffmpeg is a C library
extern "C" {
#include <libavformat/avformat.h>
}

#pragma once

/**
 * @brief The AV::Utils namespace contains utilities for audio and video processing.
 */
namespace AV::Utils {

// Forward declarations
class Demuxer;
class DemuxerException;
using DemuxerResult = std::pair<std::optional<std::unique_ptr<Demuxer>>, DemuxerException>;

/**
 * @brief The DemuxerError enum class represents the possible errors that can occur when demuxing a media file.
 */
enum class DemuxerError {
    NOERROR,
    OPENINPUT
};

/**
 * @brief The DemuxerException class represents an exception that is thrown when an error occurs while demuxing a media file.
 */
class DemuxerException : public std::exception {
public:
    DemuxerException(DemuxerError errcode);

    const char *what() const noexcept override;
    const int code() const noexcept;

private:
    DemuxerError m_errcode;
};

/**
 * @brief The Demuxer class provides utilities for demuxing media files.
 */
class Demuxer {
private:
    Demuxer(const std::string &path); // I'm keeping the constructor private so that it can only be called by the factory method

public:
    ~Demuxer();
    Demuxer(const Demuxer&) = delete;
    Demuxer& operator=(const Demuxer&) = delete;

    // Factory method
    static DemuxerResult Create(const std::string& path);

private:
    DemuxerError m_Initialize();

    std::string m_path;
    AVFormatContext* m_format_ctx = nullptr;
};

} // namespace AV::Utils
