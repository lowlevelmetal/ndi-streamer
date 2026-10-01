/**
 * @file decoder.cpp
 * @brief Audio/video decoder with optional hardware acceleration.
 */

#include "av/decoder.hpp"
#include "util/log.hpp"

#include <chrono>
#include <format>
#include <stdexcept>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace ndistreamer::av {

namespace {

// A stream that decoded before but has produced only errors for this long is considered lost.
constexpr auto kGiveUpAfter = std::chrono::seconds(30);
// How often to report errors that keep happening.
constexpr auto kErrorReportInterval = std::chrono::seconds(10);

// Device types tried by "auto", in order of preference.
constexpr AVHWDeviceType kAutoDeviceTypes[] = {
    AV_HWDEVICE_TYPE_CUDA,
    AV_HWDEVICE_TYPE_VAAPI,
    AV_HWDEVICE_TYPE_VDPAU,
};

AVPixelFormat HardwareFormatFor(const AVCodec *codec, AVHWDeviceType type) {
    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
        if (!config) return AV_PIX_FMT_NONE;
        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type == type) {
            return config->pix_fmt;
        }
    }
}

std::string AvailableDeviceTypes() {
    std::string names;
    for (AVHWDeviceType type = av_hwdevice_iterate_types(AV_HWDEVICE_TYPE_NONE); type != AV_HWDEVICE_TYPE_NONE;
         type = av_hwdevice_iterate_types(type)) {
        if (!names.empty()) names += ", ";
        names += av_hwdevice_get_type_name(type);
    }
    return names.empty() ? "none" : names;
}

} // namespace

Decoder::Decoder(const AVStream *stream, const DecoderConfig &config) {
    const AVCodecParameters *par = stream->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec) {
        throw std::runtime_error(std::format("no decoder available for codec '{}'", avcodec_get_name(par->codec_id)));
    }

    m_ctx.reset(avcodec_alloc_context3(codec));
    if (!m_ctx) throw std::bad_alloc();

    Check(avcodec_parameters_to_context(m_ctx.get(), par), "copying codec parameters");
    m_ctx->pkt_timebase = stream->time_base;
    m_ctx->thread_count = config.threads;
    m_ctx->opaque = this;

    if (par->codec_type == AVMEDIA_TYPE_VIDEO && SetupHardware(codec, config)) {
        m_ctx->hw_device_ctx = av_buffer_ref(m_hw_device.get());
        if (!m_ctx->hw_device_ctx) throw std::bad_alloc();
        m_ctx->get_format = &Decoder::GetFormat;
        // Frames may be held by filters while later ones are decoded.
        m_ctx->extra_hw_frames = 4;
    }

    Check(avcodec_open2(m_ctx.get(), codec, nullptr), std::format("opening {} decoder", codec->name));
}

int Decoder::OpenDevice(const AVCodec *codec, AVHWDeviceType type, const std::string &device) {
    AVBufferRef *ref = nullptr;
    int ret = av_hwdevice_ctx_create(&ref, type, device.empty() ? nullptr : device.c_str(), nullptr, 0);
    if (ret < 0) return ret;

    m_hw_device.reset(ref);
    m_hw_type = type;
    m_hw_format = HardwareFormatFor(codec, type);
    return 0;
}

bool Decoder::SetupHardware(const AVCodec *codec, const DecoderConfig &config) {
    const std::string &name = config.hwaccel;
    if (name.empty() || name == "none" || name == "software") return false;

    if (name == "auto") {
        for (AVHWDeviceType type : kAutoDeviceTypes) {
            if (HardwareFormatFor(codec, type) == AV_PIX_FMT_NONE) continue;
            int ret = OpenDevice(codec, type, config.hwaccel_device);
            if (ret == 0) return true;
            log::Debug("{} device unavailable: {}", av_hwdevice_get_type_name(type), ErrorString(ret));
        }
        log::Info("no hardware decoder available for {}; using software decoding", codec->name);
        return false;
    }

    AVHWDeviceType type = av_hwdevice_find_type_by_name(name.c_str());
    if (type == AV_HWDEVICE_TYPE_NONE) {
        throw std::runtime_error(
            std::format("unknown hardware acceleration '{}' (this FFmpeg supports: {})", name, AvailableDeviceTypes()));
    }

    if (HardwareFormatFor(codec, type) == AV_PIX_FMT_NONE) {
        log::Warn("{} cannot decode {}; using software decoding", name, codec->name);
        return false;
    }

    int ret = OpenDevice(codec, type, config.hwaccel_device);
    if (ret < 0) {
        std::string device = config.hwaccel_device.empty() ? "" : " '" + config.hwaccel_device + "'";
        throw std::runtime_error(std::format("cannot open {} device{}: {}", name, device, ErrorString(ret)));
    }
    return true;
}

AVPixelFormat Decoder::GetFormat(AVCodecContext *ctx, const AVPixelFormat *formats) {
    auto *self = static_cast<Decoder *>(ctx->opaque);

    for (const AVPixelFormat *format = formats; *format != AV_PIX_FMT_NONE; format++) {
        if (*format == self->m_hw_format) return *format;
    }

    if (!self->m_fallback_reported.exchange(true)) {
        log::Warn("{} cannot decode this stream; falling back to software decoding",
                  av_hwdevice_get_type_name(self->m_hw_type));
    }

    for (const AVPixelFormat *format = formats; *format != AV_PIX_FMT_NONE; format++) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*format);
        if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) return *format;
    }
    return AV_PIX_FMT_NONE;
}

void Decoder::NoteError(const char *operation, int ret) {
    auto now = std::chrono::steady_clock::now();
    if (m_consecutive_errors++ == 0) m_errors_since = now;

    // Errors before the first frame are expected when joining a stream mid-way: the decoder waits for
    // parameter sets and a keyframe. Only report those if they go on for a while.
    if (m_consecutive_errors == 1 && m_frames_decoded > 0) {
        log::Warn("{} decoder: {} failed: {}", m_ctx->codec->name, operation, ErrorString(ret));
    } else {
        log::Debug("{} decoder: {} failed: {}", m_ctx->codec->name, operation, ErrorString(ret));
    }

    if (now - m_errors_since >= kErrorReportInterval && now - m_last_error_report >= kErrorReportInterval) {
        m_last_error_report = now;
        log::Warn("{} decoder: {} errors in a row ({}); {}", m_ctx->codec->name, m_consecutive_errors,
                  ErrorString(ret), m_frames_decoded > 0 ? "the stream may be damaged" : "waiting for a keyframe");
    }

    if (m_frames_decoded > 0 && now - m_errors_since >= kGiveUpAfter) {
        throw Error(std::format("{} decoder produced nothing but errors for {} s", m_ctx->codec->name,
                                std::chrono::duration_cast<std::chrono::seconds>(kGiveUpAfter).count()),
                    ret);
    }
}

bool Decoder::Send(const AVPacket *packet) {
    int ret = avcodec_send_packet(m_ctx.get(), packet);
    if (ret == AVERROR(EAGAIN)) return false;
    if (ret == AVERROR_EOF) return true; // already draining
    if (ret < 0) {
        // Corrupt packets are skipped; the decoder resynchronizes on the next keyframe.
        NoteError("sending a packet", ret);
        return true;
    }
    return true;
}

FramePtr Decoder::Receive() {
    FramePtr frame = MakeFrame();
    int ret = avcodec_receive_frame(m_ctx.get(), frame.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) {
        NoteError("decoding", ret);
        return nullptr;
    }

    m_consecutive_errors = 0;
    m_frames_decoded++;
    return frame;
}

void Decoder::Reset() {
    avcodec_flush_buffers(m_ctx.get());
}

std::string Decoder::HardwareName() const {
    if (!m_hw_device || m_fallback_reported.load()) return {};
    return av_hwdevice_get_type_name(m_hw_type);
}

std::string Decoder::CodecName() const {
    return m_ctx->codec->name;
}

} // namespace ndistreamer::av
