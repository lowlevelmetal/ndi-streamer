/**
 * @file video_processor.cpp
 * @brief Turns decoded video frames into NDI-ready frames.
 */

#include "av/video_processor.hpp"
#include "util/log.hpp"

#include <format>
#include <new>
#include <stdexcept>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/macros.h>
#include <libavutil/pixdesc.h>
}

namespace ndistreamer::av {

namespace {

// Extra bytes after each buffer so SIMD routines may safely overrun the last line.
constexpr size_t kBufferPadding = 64;

struct PlaneLayout {
    int planes = 0;
    size_t offset[3] = {};
    int linesize[3] = {};
    size_t size = 0;
};

// The contiguous layouts NDI expects, with SIMD-friendly line strides.
PlaneLayout ContiguousLayout(AVPixelFormat format, int width, int height) {
    PlaneLayout layout;
    int chroma_height = (height + 1) / 2;

    switch (format) {
    case AV_PIX_FMT_UYVY422:
        layout.planes = 1;
        layout.linesize[0] = FFALIGN(width * 2, 64);
        layout.size = static_cast<size_t>(layout.linesize[0]) * height;
        break;
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_BGR0:
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_RGB0:
        layout.planes = 1;
        layout.linesize[0] = FFALIGN(width * 4, 64);
        layout.size = static_cast<size_t>(layout.linesize[0]) * height;
        break;
    case AV_PIX_FMT_NV12:
        // Y plane followed immediately by interleaved UV with the same stride.
        layout.planes = 2;
        layout.linesize[0] = layout.linesize[1] = FFALIGN(width, 64);
        layout.offset[1] = static_cast<size_t>(layout.linesize[0]) * height;
        layout.size = layout.offset[1] + static_cast<size_t>(layout.linesize[1]) * chroma_height;
        break;
    case AV_PIX_FMT_YUV420P:
        // Y, then U, then V; chroma stride is exactly half the luma stride.
        layout.planes = 3;
        layout.linesize[0] = FFALIGN(width, 128);
        layout.linesize[1] = layout.linesize[2] = layout.linesize[0] / 2;
        layout.offset[1] = static_cast<size_t>(layout.linesize[0]) * height;
        layout.offset[2] = layout.offset[1] + static_cast<size_t>(layout.linesize[1]) * chroma_height;
        layout.size = layout.offset[2] + static_cast<size_t>(layout.linesize[2]) * chroma_height;
        break;
    default:
        throw std::logic_error(std::format("no NDI layout for {}", av_get_pix_fmt_name(format)));
    }
    return layout;
}

std::optional<VideoFourCC> FourCCFor(AVPixelFormat format) {
    switch (format) {
    case AV_PIX_FMT_UYVY422:
        return VideoFourCC::UYVY;
    case AV_PIX_FMT_NV12:
        return VideoFourCC::NV12;
    case AV_PIX_FMT_YUV420P:
        return VideoFourCC::I420;
    case AV_PIX_FMT_BGRA:
        return VideoFourCC::BGRA;
    case AV_PIX_FMT_BGR0:
        return VideoFourCC::BGRX;
    case AV_PIX_FMT_RGBA:
        return VideoFourCC::RGBA;
    case AV_PIX_FMT_RGB0:
        return VideoFourCC::RGBX;
    default:
        return std::nullopt;
    }
}

bool IsRgb(AVPixelFormat format) {
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(format);
    return desc && (desc->flags & AV_PIX_FMT_FLAG_RGB);
}

bool IsFullRange(const AVFrame *frame) {
    switch (frame->format) {
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUVJ444P:
    case AV_PIX_FMT_YUVJ440P:
    case AV_PIX_FMT_YUVJ411P:
        return true;
    default:
        return frame->color_range == AVCOL_RANGE_JPEG;
    }
}

// Whether the frame's planes already sit where NDI expects them, so it can be sent without copying.
bool HasNdiLayout(const AVFrame *frame) {
    const int *ls = frame->linesize;
    uint8_t *const *data = frame->data;
    if (ls[0] <= 0) return false;

    switch (frame->format) {
    case AV_PIX_FMT_NV12:
        return ls[1] == ls[0] && data[1] == data[0] + static_cast<size_t>(ls[0]) * frame->height;
    case AV_PIX_FMT_YUV420P: {
        int chroma_height = (frame->height + 1) / 2;
        return ls[0] % 2 == 0 && ls[1] == ls[0] / 2 && ls[2] == ls[1] &&
               data[1] == data[0] + static_cast<size_t>(ls[0]) * frame->height &&
               data[2] == data[1] + static_cast<size_t>(ls[1]) * chroma_height;
    }
    default:
        return true; // single plane
    }
}

int SwsColorspace(AVColorSpace colorspace, int height) {
    switch (colorspace) {
    case AVCOL_SPC_BT709:
        return SWS_CS_ITU709;
    case AVCOL_SPC_FCC:
        return SWS_CS_FCC;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        return SWS_CS_ITU601;
    case AVCOL_SPC_SMPTE240M:
        return SWS_CS_SMPTE240M;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        return SWS_CS_BT2020;
    default:
        return height >= 720 ? SWS_CS_ITU709 : SWS_CS_ITU601;
    }
}

// Whether a filtergraph is written for GPU frames. Anything else gets frames in system memory.
bool IsHardwareFilter(const std::string &filter) {
    for (const char *marker : {"hwdownload", "hwmap", "hwupload", "_vaapi", "_cuda", "_npp", "_qsv", "_vulkan",
                               "_opencl", "_vdpau"}) {
        if (filter.find(marker) != std::string::npos) return true;
    }
    return false;
}

bool IsInterlaced(const AVFrame *frame) {
#ifdef AV_FRAME_FLAG_INTERLACED
    return frame->flags & AV_FRAME_FLAG_INTERLACED;
#else
    return frame->interlaced_frame;
#endif
}

} // namespace

std::optional<OutputPixelFormat> ParseOutputPixelFormat(std::string_view name) {
    if (name == "uyvy") return OutputPixelFormat::UYVY;
    if (name == "nv12") return OutputPixelFormat::NV12;
    if (name == "i420") return OutputPixelFormat::I420;
    if (name == "bgra") return OutputPixelFormat::BGRA;
    if (name == "rgba") return OutputPixelFormat::RGBA;
    if (name == "auto") return OutputPixelFormat::Auto;
    return std::nullopt;
}

const char *ToString(OutputPixelFormat format) {
    switch (format) {
    case OutputPixelFormat::UYVY:
        return "uyvy";
    case OutputPixelFormat::NV12:
        return "nv12";
    case OutputPixelFormat::I420:
        return "i420";
    case OutputPixelFormat::BGRA:
        return "bgra";
    case OutputPixelFormat::RGBA:
        return "rgba";
    case OutputPixelFormat::Auto:
        return "auto";
    }
    return "?";
}

VideoProcessor::VideoProcessor(VideoProcessorConfig config)
    : m_config(std::move(config)), m_filter_takes_hw_frames(IsHardwareFilter(m_config.filter)) {}

AVPixelFormat VideoProcessor::TargetFormat(AVPixelFormat source, AVColorRange range) const {
    switch (m_config.pixel_format) {
    case OutputPixelFormat::UYVY:
        return AV_PIX_FMT_UYVY422;
    case OutputPixelFormat::NV12:
        return AV_PIX_FMT_NV12;
    case OutputPixelFormat::I420:
        return AV_PIX_FMT_YUV420P;
    case OutputPixelFormat::BGRA:
        return AV_PIX_FMT_BGRA;
    case OutputPixelFormat::RGBA:
        return AV_PIX_FMT_RGBA;
    case OutputPixelFormat::Auto:
        // NDI assumes limited-range YUV, so full-range sources are converted.
        if (FourCCFor(source) && (IsRgb(source) || range != AVCOL_RANGE_JPEG)) return source;
        return AV_PIX_FMT_UYVY422;
    }
    return AV_PIX_FMT_UYVY422;
}

FramePtr VideoProcessor::AllocateContiguous(AVPixelFormat format, int width, int height) {
    PlaneLayout layout = ContiguousLayout(format, width, height);

    if (!m_pool || m_pool_size != layout.size) {
        // Buffers still in flight keep the old pool alive until they are returned.
        m_pool.reset(av_buffer_pool_init(layout.size + kBufferPadding, nullptr));
        if (!m_pool) throw std::bad_alloc();
        m_pool_size = layout.size;
    }

    FramePtr frame = MakeFrame();
    frame->buf[0] = av_buffer_pool_get(m_pool.get());
    if (!frame->buf[0]) throw std::bad_alloc();

    for (int i = 0; i < layout.planes; i++) {
        frame->data[i] = frame->buf[0]->data + layout.offset[i];
        frame->linesize[i] = layout.linesize[i];
    }
    frame->format = format;
    frame->width = width;
    frame->height = height;
    return frame;
}

FramePtr VideoProcessor::Download(FramePtr frame, AVPixelFormat target) {
    const auto *frames_ctx = reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data);

    FramePtr sw;
    if (target != AV_PIX_FMT_NONE && frames_ctx->sw_format == target) {
        // Transfer straight into the buffer NDI will read from.
        sw = AllocateContiguous(target, frame->width, frame->height);
    } else {
        sw = MakeFrame(); // FFmpeg picks the device's preferred download format
    }

    Check(av_hwframe_transfer_data(sw.get(), frame.get(), 0), "downloading a frame from the GPU");
    Check(av_frame_copy_props(sw.get(), frame.get()), "copying frame properties");
    return sw;
}

FramePtr VideoProcessor::Convert(FramePtr frame, AVPixelFormat target, const char *&path) {
    if (frame->hw_frames_ctx) frame = Download(std::move(frame), target);

    if (frame->format == target) {
        if (HasNdiLayout(frame.get())) {
            path = "zero-copy";
            return frame;
        }
        path = "repacked";
        FramePtr packed = AllocateContiguous(target, frame->width, frame->height);
        Check(av_frame_copy(packed.get(), frame.get()), "repacking a frame");
        return packed;
    }

    path = "converted";
    auto source = static_cast<AVPixelFormat>(frame->format);
    SwsKey key{frame->width, frame->height, source, target, SwsColorspace(frame->colorspace, frame->height),
               IsFullRange(frame.get()) ? 1 : 0};
    if (!m_sws || key != m_sws_key) {
        m_sws.reset(sws_getContext(key.width, key.height, source, key.width, key.height, target, SWS_BICUBIC, nullptr,
                                   nullptr, nullptr));
        if (!m_sws) {
            throw std::runtime_error(std::format("cannot convert {} to {}", av_get_pix_fmt_name(source),
                                                 av_get_pix_fmt_name(target)));
        }
        // NDI interprets YUV as limited range, BT.709 for HD and BT.601 for SD.
        const int *dst_table = sws_getCoefficients(key.height >= 720 ? SWS_CS_ITU709 : SWS_CS_ITU601);
        int dst_range = IsRgb(target) ? 1 : 0;
        sws_setColorspaceDetails(m_sws.get(), sws_getCoefficients(key.colorspace), key.range, dst_table, dst_range, 0,
                                 1 << 16, 1 << 16);
        m_sws_key = key;
    }

    FramePtr converted = AllocateContiguous(target, frame->width, frame->height);
    Check(sws_scale(m_sws.get(), frame->data, frame->linesize, 0, frame->height, converted->data, converted->linesize),
          "converting pixel format");
    return converted;
}

bool VideoProcessor::Emit(FramePtr frame, AVRational time_base, AVRational frame_rate, const Sink &sink) {
    if (frame_rate.num <= 0 || frame_rate.den <= 0) frame_rate = m_config.frame_rate;

    // Capture metadata before conversion replaces the frame.
    int64_t frame_duration_us = av_rescale(1000000, frame_rate.den, frame_rate.num);
    int64_t pts_us = frame->pts != AV_NOPTS_VALUE ? ToMicros(frame->pts, time_base) : m_next_out_us;
    int64_t duration_us = frame->duration > 0 ? ToMicros(frame->duration, time_base) : frame_duration_us;
    m_next_out_us = pts_us + duration_us;

    AVRational sar = frame->sample_aspect_ratio;
    float aspect = 0.0f;
    if (sar.num > 0 && sar.den > 0 && frame->height > 0) {
        aspect = static_cast<float>(static_cast<double>(frame->width) * sar.num / (frame->height * sar.den));
    }
    bool interlaced = IsInterlaced(frame.get());

    AVPixelFormat source = static_cast<AVPixelFormat>(frame->format);
    if (frame->hw_frames_ctx) {
        source = reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data)->sw_format;
    }
    AVPixelFormat target = TargetFormat(source, frame->color_range);

    const char *path = "";
    FramePtr ready = Convert(std::move(frame), target, path);

    VideoFrame out;
    out.data = ready->data[0];
    out.stride = ready->linesize[0];
    out.width = ready->width;
    out.height = ready->height;
    out.fourcc = *FourCCFor(target);
    out.frame_rate_num = frame_rate.num;
    out.frame_rate_den = frame_rate.den;
    out.aspect_ratio = aspect;
    out.interlaced = interlaced;
    out.pts_us = pts_us;
    out.duration_us = duration_us;
    out.storage = std::move(ready);

    if (!m_reported) {
        m_reported = true;
        log::Info("video output: {} {}x{} @ {:.3f} fps{} ({} from {})", ToString(out.fourcc), out.width, out.height,
                  av_q2d(frame_rate), interlaced ? " interlaced" : "", path, av_get_pix_fmt_name(source));
    }

    return sink(std::move(out));
}

void VideoProcessor::BuildFilter(const AVFrame *frame) {
    m_graph.reset(avfilter_graph_alloc());
    if (!m_graph) throw std::bad_alloc();

    m_buffersrc = avfilter_graph_alloc_filter(m_graph.get(), avfilter_get_by_name("buffer"), "in");
    m_buffersink = avfilter_graph_alloc_filter(m_graph.get(), avfilter_get_by_name("buffersink"), "out");
    if (!m_buffersrc || !m_buffersink) throw std::bad_alloc();

    AVBufferSrcParameters *params = av_buffersrc_parameters_alloc();
    if (!params) throw std::bad_alloc();
    params->format = frame->format;
    params->width = frame->width;
    params->height = frame->height;
    params->time_base = m_config.time_base;
    params->frame_rate = m_config.frame_rate;
    params->sample_aspect_ratio = frame->sample_aspect_ratio.num > 0 ? frame->sample_aspect_ratio : AVRational{1, 1};
    params->hw_frames_ctx = frame->hw_frames_ctx;
#ifdef NDISTREAMER_HAVE_BUFFERSRC_COLOR
    params->color_space = frame->colorspace;
    params->color_range = frame->color_range;
#endif
    int ret = av_buffersrc_parameters_set(m_buffersrc, params);
    av_free(params);
    Check(ret, "configuring the filter source");
    Check(avfilter_init_str(m_buffersrc, nullptr), "initializing the filter source");
    Check(avfilter_init_str(m_buffersink, nullptr), "initializing the filter sink");

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    if (!outputs || !inputs) {
        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        throw std::bad_alloc();
    }
    outputs->name = av_strdup("in");
    outputs->filter_ctx = m_buffersrc;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = m_buffersink;

    ret = avfilter_graph_parse_ptr(m_graph.get(), m_config.filter.c_str(), &inputs, &outputs, nullptr);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    Check(ret, std::format("parsing video filter '{}'", m_config.filter));

    if (m_config.hw_device) {
        for (unsigned i = 0; i < m_graph->nb_filters; i++) {
            AVFilterContext *filter = m_graph->filters[i];
            if (!filter->hw_device_ctx) filter->hw_device_ctx = av_buffer_ref(m_config.hw_device);
        }
    }

    Check(avfilter_graph_config(m_graph.get(), nullptr), std::format("configuring video filter '{}'", m_config.filter));

    m_filter_rate = av_buffersink_get_frame_rate(m_buffersink);
    m_filter_input = {frame->width, frame->height, frame->format,
                      frame->hw_frames_ctx ? frame->hw_frames_ctx->data : nullptr};
    log::Debug("video filter configured: {}", m_config.filter);
}

bool VideoProcessor::DrainFilter(const Sink &sink) {
    AVRational time_base = av_buffersink_get_time_base(m_buffersink);
    while (true) {
        FramePtr filtered = MakeFrame();
        int ret = av_buffersink_get_frame(m_buffersink, filtered.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return true;
        Check(ret, "filtering video");
        if (!Emit(std::move(filtered), time_base, m_filter_rate, sink)) return false;
    }
}

void VideoProcessor::Process(FramePtr frame, const Sink &sink) {
    // Give every frame a timestamp; streams without them are timed by frame rate.
    int64_t pts = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;
    if (pts == AV_NOPTS_VALUE) pts = m_next_in_pts != AV_NOPTS_VALUE ? m_next_in_pts : 0;
    frame->pts = pts;
    int64_t duration = frame->duration > 0 ? frame->duration
                                           : av_rescale_q(1, av_inv_q(m_config.frame_rate), m_config.time_base);
    m_next_in_pts = pts + duration;

    if (m_config.filter.empty()) {
        Emit(std::move(frame), m_config.time_base, m_config.frame_rate, sink);
        return;
    }

    if (frame->hw_frames_ctx && !m_filter_takes_hw_frames) frame = Download(std::move(frame), AV_PIX_FMT_NONE);

    const void *hw_frames = frame->hw_frames_ctx ? frame->hw_frames_ctx->data : nullptr;
    if (!m_graph || m_filter_input.width != frame->width || m_filter_input.height != frame->height ||
        m_filter_input.format != frame->format || m_filter_input.hw_frames != hw_frames) {
        if (m_graph) {
            log::Info("video input changed to {}x{}; rebuilding filter", frame->width, frame->height);
            Flush(sink); // emit what the old graph still holds
        }
        BuildFilter(frame.get());
    }

    // Ownership of the frame's data passes to the filter.
    Check(av_buffersrc_add_frame_flags(m_buffersrc, frame.get(), 0), "filtering video");
    DrainFilter(sink);
}

void VideoProcessor::Flush(const Sink &sink) {
    if (!m_graph) return;
    Check(av_buffersrc_add_frame_flags(m_buffersrc, nullptr, 0), "flushing video filter");
    DrainFilter(sink);
}

} // namespace ndistreamer::av
