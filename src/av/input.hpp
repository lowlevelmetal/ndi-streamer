/**
 * @file input.hpp
 * @brief Opens a media input, selects streams and reads packets on a continuous timeline.
 */

#pragma once

#include "av/ffmpeg.hpp"

#include <cstdint>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace ndistreamer::av {

struct InputConfig {
    std::string url;
    std::string format; ///< Force an input format (e.g. "v4l2"); empty to probe.
    std::vector<std::pair<std::string, std::string>> options; ///< Demuxer/protocol options.
    int video_stream = -1;                                    ///< Stream index, or -1 to choose automatically.
    int audio_stream = -1;                                    ///< Stream index, or -1 to choose automatically.
    bool disable_video = false;
    bool disable_audio = false;
};

/**
 * @brief Demuxer front end.
 *
 * Only packets of the selected video and audio streams are returned. When the input is restarted
 * for looping, timestamps of the following pass are shifted so that the timeline keeps increasing;
 * downstream stages never see a timestamp jump.
 */
class MediaInput {
public:
    /**
     * @param stop interrupts blocking I/O (network reads, device waits) when triggered
     */
    MediaInput(const InputConfig &config, std::stop_token stop);

    MediaInput(const MediaInput &) = delete;
    MediaInput &operator=(const MediaInput &) = delete;

    AVStream *VideoStream() const { return m_video; }
    AVStream *AudioStream() const { return m_audio; }

    /**
     * @brief Nominal video frame rate, falling back to 30000/1001 when it cannot be determined.
     */
    AVRational VideoFrameRate() const;

    /**
     * @brief Whether the input is a live source (stream, device, pipe) rather than a seekable file.
     * Timestamp jumps in live sources are discontinuities; in files they are real gaps.
     */
    bool IsLive() const;

    /**
     * @brief Read the next packet of a selected stream.
     * @return nullptr at the end of the input or when stopped.
     */
    PacketPtr Read();

    /**
     * @brief Seek back to the beginning for another pass.
     * @return false if the input cannot be restarted (e.g. not seekable).
     */
    bool Restart();

    /**
     * @brief Log a description of the input and the selected streams.
     */
    void LogInfo() const;

private:
    struct Track {
        bool selected = false;
        int64_t offset = 0;           ///< Timeline offset in stream time base.
        int64_t default_duration = 0; ///< Used when packets carry no duration.
    };

    static int InterruptCallback(void *opaque);

    void Open(const InputConfig &config);
    void SelectStreams(const InputConfig &config);
    AVStream *SelectStream(AVMediaType type, int requested, int related);
    void Account(AVPacket *packet, Track &track);

    std::stop_token m_stop;
    FormatContextPtr m_ctx;
    AVStream *m_video = nullptr;
    AVStream *m_audio = nullptr;
    std::vector<Track> m_tracks;

    bool m_first_pass = true;
    int64_t m_offset_us = 0;
    int64_t m_pass_start_us = INT64_MAX;
    int64_t m_pass_end_us = INT64_MIN;
};

} // namespace ndistreamer::av
