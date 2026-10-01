/**
 * @file ffmpeg.hpp
 * @brief RAII ownership and error helpers for FFmpeg objects.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace ndistreamer::av {

struct FrameDeleter {
    void operator()(AVFrame *frame) const { av_frame_free(&frame); }
};
struct PacketDeleter {
    void operator()(AVPacket *packet) const { av_packet_free(&packet); }
};
struct CodecContextDeleter {
    void operator()(AVCodecContext *ctx) const { avcodec_free_context(&ctx); }
};
struct FormatContextDeleter {
    void operator()(AVFormatContext *ctx) const { avformat_close_input(&ctx); }
};
struct BufferRefDeleter {
    void operator()(AVBufferRef *ref) const { av_buffer_unref(&ref); }
};
struct BufferPoolDeleter {
    void operator()(AVBufferPool *pool) const { av_buffer_pool_uninit(&pool); }
};
struct FilterGraphDeleter {
    void operator()(AVFilterGraph *graph) const { avfilter_graph_free(&graph); }
};
struct SwrDeleter {
    void operator()(SwrContext *ctx) const { swr_free(&ctx); }
};
struct SwsDeleter {
    void operator()(SwsContext *ctx) const { sws_freeContext(ctx); }
};

using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
using BufferRefPtr = std::unique_ptr<AVBufferRef, BufferRefDeleter>;
using BufferPoolPtr = std::unique_ptr<AVBufferPool, BufferPoolDeleter>;
using FilterGraphPtr = std::unique_ptr<AVFilterGraph, FilterGraphDeleter>;
using SwrPtr = std::unique_ptr<SwrContext, SwrDeleter>;
using SwsPtr = std::unique_ptr<SwsContext, SwsDeleter>;

/**
 * @brief Allocate an empty frame; throws std::bad_alloc on failure.
 */
FramePtr MakeFrame();

/**
 * @brief Allocate an empty packet; throws std::bad_alloc on failure.
 */
PacketPtr MakePacket();

/**
 * @brief Human readable description of an FFmpeg error code.
 */
std::string ErrorString(int errnum);

/**
 * @brief An FFmpeg call failed. what() includes the operation and FFmpeg's description.
 */
class Error : public std::runtime_error {
public:
    Error(std::string_view operation, int code);
    int code() const noexcept { return m_code; }

private:
    int m_code;
};

/**
 * @brief Throw av::Error if @p ret is negative; otherwise return it.
 */
inline int Check(int ret, std::string_view operation) {
    if (ret < 0) throw Error(operation, ret);
    return ret;
}

/**
 * @brief Convert a timestamp to microseconds.
 */
inline int64_t ToMicros(int64_t ts, AVRational time_base) {
    return av_rescale_q(ts, time_base, AVRational{1, 1000000});
}

/**
 * @brief Convert microseconds to a timestamp in @p time_base.
 */
inline int64_t FromMicros(int64_t us, AVRational time_base) {
    return av_rescale_q(us, AVRational{1, 1000000}, time_base);
}

/**
 * @brief Format microseconds as HH:MM:SS.mmm.
 */
std::string FormatDuration(int64_t us);

} // namespace ndistreamer::av
