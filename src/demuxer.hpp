/**
 * @file demuxer.hpp
 * @brief This file includes utilities for demuxing media files.
 * @date 2024-09-02
 * @author Matthew Todd Geiger
 */

#pragma once

#include <string>
#include <memory>
#include <optional>
#include <vector>

// ffmpeg is a C library
extern "C" {
#include <libavformat/avformat.h>
}

#include "averror.hpp"

/**
 * @brief The AV::Utils namespace contains utilities for audio and video processing.
 */
namespace AV::Utils {

// Forward declarations
class Demuxer;
using DemuxerResult = std::pair<std::unique_ptr<Demuxer>, const AvException>;
using ReadFrameResult = std::pair<AVPacket*, const AvException>;

typedef struct DemuxerConfig {
    std::string path{""};
    int width{0}, height{0};
    std::string pixel_format{""};
} demuxerconfig, *pdemuxerconfig;

/**
 * @brief The Demuxer class provides utilities for demuxing media files.
 */
class Demuxer {
private:
    Demuxer(const std::string &path); // I'm keeping the constructor private so that it can only be called by the factory method

public:
    ~Demuxer();

    // For now we'll disable copying and assignment.
    // In the future we will make sure that these are implemented correctly.
    Demuxer(const Demuxer&) = delete;
    Demuxer& operator=(const Demuxer&) = delete;

    // Factory methods
    static DemuxerResult Create(const std::string& path);

    // Getters
    ReadFrameResult ReadFrame();
    std::vector<AVStream *> GetStreamPointers();


private:
    AvError m_Initialize();

    std::string m_path;
    AVFormatContext* m_format_ctx = nullptr;
    AVPacket *m_packet = nullptr;
};

} // namespace AV::Utils
