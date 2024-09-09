/**
 * @file demuxer.hpp
 * @brief This file includes utilities for demuxing media files.
 * @date 2024-09-02
 * @author Matthew Todd Geiger
 */

#include <string>
#include <memory>
#include <optional>
#include <vector>

// ffmpeg is a C library
extern "C" {
#include <libavformat/avformat.h>
}

#include "averror.hpp"

#pragma once

/**
 * @brief The AV::Utils namespace contains utilities for audio and video processing.
 */
namespace AV::Utils {

// Forward declarations
class Demuxer;
using DemuxerResult = std::pair<std::unique_ptr<Demuxer>, const AvException>;
using ReadFrameResult = std::pair<AVPacket*, const AvException>;
using GetStreamResult = std::vector<AVStream *>;

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
    Demuxer(const DemuxerConfig &config);

public:
    ~Demuxer();
    Demuxer(const Demuxer&) = delete;
    Demuxer& operator=(const Demuxer&) = delete;

    // Factory methods
    static DemuxerResult Create(const std::string& path);
    static DemuxerResult Create(const DemuxerConfig& config);

    // Read frames
    ReadFrameResult ReadFrame();

    // Getters
    GetStreamResult GetStreams();


private:
    AvError m_Initialize();

    DemuxerConfig m_config;
    AVFormatContext* m_format_ctx = nullptr;
    AVDictionary *m_opts = nullptr;
    AVPacket *m_packet = nullptr;
};

} // namespace AV::Utils
