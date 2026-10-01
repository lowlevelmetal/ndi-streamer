/**
 * @file video_processor.hpp
 * @brief Turns decoded video frames into NDI-ready frames.
 */

#pragma once

#include "av/ffmpeg.hpp"
#include "frames.hpp"

#include <functional>
#include <optional>
#include <string>

namespace ndistreamer::av {

/**
 * @brief Pixel layout requested for NDI output.
 */
enum class OutputPixelFormat {
    Auto, ///< Pass through any layout NDI accepts; convert everything else to UYVY.
    UYVY, ///< 8-bit 4:2:2.
    NV12, ///< 8-bit 4:2:0 semi-planar.
    I420, ///< 8-bit 4:2:0 planar.
    BGRA, ///< 8-bit RGB with alpha.
    RGBA, ///< 8-bit RGB with alpha.
};

std::optional<OutputPixelFormat> ParseOutputPixelFormat(std::string_view name);
const char *ToString(OutputPixelFormat format);

struct VideoProcessorConfig {
    std::string filter;                                  ///< Optional FFmpeg filtergraph, e.g. "yadif".
    OutputPixelFormat pixel_format = OutputPixelFormat::Auto;
    AVRational time_base{1, 1000000};                    ///< Time base of incoming frame timestamps.
    AVRational frame_rate{30000, 1001};                  ///< Nominal input frame rate.
    AVBufferRef *hw_device = nullptr;                    ///< Device for hardware filters (borrowed).
};

/**
 * @brief Filter, download and convert decoded frames.
 *
 * Steps, each skipped when not needed:
 *   1. the user filtergraph (built lazily from the first frame, rebuilt if the input changes);
 *      GPU frames are downloaded first unless the graph uses hardware filters,
 *   2. download from GPU memory, directly into NDI layout when the formats match,
 *   3. pixel format conversion, or repacking planes into the contiguous layout NDI requires.
 * Single-plane frames that already match are passed to NDI without copying.
 */
class VideoProcessor {
public:
    /// Receives each finished frame; returning false stops processing (e.g. the queue was closed).
    using Sink = std::function<bool(VideoFrame &&)>;

    explicit VideoProcessor(VideoProcessorConfig config);

    VideoProcessor(const VideoProcessor &) = delete;
    VideoProcessor &operator=(const VideoProcessor &) = delete;

    /**
     * @brief Process one decoded frame. Timestamps are in the configured time base.
     */
    void Process(FramePtr frame, const Sink &sink);

    /**
     * @brief Flush frames buffered in the filtergraph at the end of the stream.
     */
    void Flush(const Sink &sink);

private:
    struct FilterInput {
        int width = 0;
        int height = 0;
        int format = -1;
        const void *hw_frames = nullptr;
    };

    void BuildFilter(const AVFrame *frame);
    bool DrainFilter(const Sink &sink);
    bool Emit(FramePtr frame, AVRational time_base, AVRational frame_rate, const Sink &sink);

    AVPixelFormat TargetFormat(AVPixelFormat source, AVColorRange range) const;
    FramePtr Download(FramePtr frame, AVPixelFormat target);
    FramePtr Convert(FramePtr frame, AVPixelFormat target, const char *&path);
    FramePtr AllocateContiguous(AVPixelFormat format, int width, int height);

    VideoProcessorConfig m_config;
    const bool m_filter_takes_hw_frames;

    FilterGraphPtr m_graph;
    AVFilterContext *m_buffersrc = nullptr;
    AVFilterContext *m_buffersink = nullptr;
    FilterInput m_filter_input;
    AVRational m_filter_rate{0, 1};

    struct SwsKey {
        int width = 0;
        int height = 0;
        AVPixelFormat source = AV_PIX_FMT_NONE;
        AVPixelFormat target = AV_PIX_FMT_NONE;
        int colorspace = 0;
        int range = 0;
        bool operator==(const SwsKey &) const = default;
    };

    SwsPtr m_sws;
    SwsKey m_sws_key;

    BufferPoolPtr m_pool;
    size_t m_pool_size = 0;

    int64_t m_next_in_pts = AV_NOPTS_VALUE;
    int64_t m_next_out_us = 0;
    bool m_reported = false;
};

} // namespace ndistreamer::av
