/**
 * @file input.cpp
 * @brief Opens a media input, selects streams and reads packets on a continuous timeline.
 */

#include "av/input.hpp"
#include "util/log.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <unistd.h>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/intreadwrite.h>
}

namespace ndistreamer::av {

namespace {

const char *MediaTypeName(AVMediaType type) {
    return type == AVMEDIA_TYPE_VIDEO ? "a video" : "an audio";
}

// FFmpeg built with Mbed TLS (as the portable release binary is) has no default certificate store,
// so it is pointed at the system's CA bundle unless the user chose one with -o ca_file=...
const char *SystemCaBundle() {
    static const char *const kBundles[] = {
        "/etc/ssl/certs/ca-certificates.crt",                // Debian, Ubuntu, Arch, Alpine, Gentoo
        "/etc/pki/tls/certs/ca-bundle.crt",                  // Fedora, RHEL
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", // Fedora, RHEL
        "/etc/ssl/ca-bundle.pem",                            // openSUSE
        "/etc/ssl/cert.pem",
    };
    static const char *const bundle = [] {
        for (const char *path : kBundles) {
            if (access(path, R_OK) == 0) return path;
        }
        return static_cast<const char *>(nullptr);
    }();
    return bundle;
}

bool UsesMbedTls() {
    static const bool uses = std::strstr(avformat_configuration(), "--enable-mbedtls") != nullptr;
    return uses;
}

std::string DescribeCodec(const AVCodecParameters *par) {
    std::string text = avcodec_get_name(par->codec_id);
    if (const char *profile = avcodec_profile_name(par->codec_id, par->profile)) {
        text += std::format(" ({})", profile);
    }
    return text;
}

} // namespace

MediaInput::MediaInput(const InputConfig &config, std::stop_token stop) : m_stop(std::move(stop)) {
    static std::once_flag devices_registered;
    std::call_once(devices_registered, [] { avdevice_register_all(); });

    Open(config);
    SelectStreams(config);
}

int MediaInput::InterruptCallback(void *opaque) {
    return static_cast<MediaInput *>(opaque)->m_stop.stop_requested() ? 1 : 0;
}

void MediaInput::Open(const InputConfig &config) {
    AVFormatContext *ctx = avformat_alloc_context();
    if (!ctx) throw std::bad_alloc();
    ctx->interrupt_callback.callback = &MediaInput::InterruptCallback;
    ctx->interrupt_callback.opaque = this;
    ctx->flags |= AVFMT_FLAG_GENPTS;

    const AVInputFormat *format = nullptr;
    if (!config.format.empty()) {
        format = av_find_input_format(config.format.c_str());
        if (!format) {
            avformat_free_context(ctx);
            throw std::runtime_error(std::format("unknown input format '{}'", config.format));
        }
    }

    AVDictionary *options = nullptr;
    for (const auto &[key, value] : config.options) {
        av_dict_set(&options, key.c_str(), value.c_str(), 0);
    }
    bool default_ca_file = false;
    if (UsesMbedTls() && !av_dict_get(options, "ca_file", nullptr, 0)) {
        if (const char *bundle = SystemCaBundle()) {
            av_dict_set(&options, "ca_file", bundle, 0);
            default_ca_file = true;
        }
    }

    // On failure avformat_open_input frees the context itself.
    int ret = avformat_open_input(&ctx, config.url.c_str(), format, &options);
    if (ret < 0) {
        av_dict_free(&options);
        throw Error(std::format("cannot open '{}'", config.url), ret);
    }
    m_ctx.reset(ctx);

    const AVDictionaryEntry *unused = nullptr;
    while ((unused = av_dict_iterate(options, unused))) {
        if (default_ca_file && std::strcmp(unused->key, "ca_file") == 0) continue; // only used for TLS
        log::Warn("input option '{}' was not recognized", unused->key);
    }
    av_dict_free(&options);

    Check(avformat_find_stream_info(m_ctx.get(), nullptr), "reading stream information");
}

AVStream *MediaInput::SelectStream(AVMediaType type, int requested, int related) {
    if (requested >= 0) {
        if (requested >= static_cast<int>(m_ctx->nb_streams)) {
            throw std::runtime_error(std::format("stream #{} does not exist (the input has {} streams)", requested,
                                                 m_ctx->nb_streams));
        }
        AVStream *stream = m_ctx->streams[requested];
        if (stream->codecpar->codec_type != type) {
            throw std::runtime_error(std::format("stream #{} is not {} stream", requested, MediaTypeName(type)));
        }
        return stream;
    }

    int index = av_find_best_stream(m_ctx.get(), type, -1, related, nullptr, 0);
    if (index < 0) return nullptr;

    AVStream *stream = m_ctx->streams[index];
    if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
        log::Debug("ignoring cover art in stream #{}", index);
        return nullptr;
    }
    return stream;
}

void MediaInput::SelectStreams(const InputConfig &config) {
    if (!config.disable_video) m_video = SelectStream(AVMEDIA_TYPE_VIDEO, config.video_stream, -1);
    if (!config.disable_audio) {
        m_audio = SelectStream(AVMEDIA_TYPE_AUDIO, config.audio_stream, m_video ? m_video->index : -1);
    }

    if (!m_video && !m_audio) {
        throw std::runtime_error(std::format("'{}' contains no usable audio or video stream", config.url));
    }

    m_tracks.assign(m_ctx->nb_streams, Track{});
    for (unsigned i = 0; i < m_ctx->nb_streams; i++) {
        AVStream *stream = m_ctx->streams[i];
        Track &track = m_tracks[i];
        track.selected = stream == m_video || stream == m_audio;
        if (!track.selected) {
            stream->discard = AVDISCARD_ALL;
            continue;
        }

        if (stream == m_video) {
            AVRational rate = VideoFrameRate();
            track.default_duration = av_rescale_q(1, av_inv_q(rate), stream->time_base);
        } else if (stream->codecpar->frame_size > 0 && stream->codecpar->sample_rate > 0) {
            track.default_duration = av_rescale_q(stream->codecpar->frame_size,
                                                  AVRational{1, stream->codecpar->sample_rate}, stream->time_base);
        }
    }
}

AVRational MediaInput::VideoFrameRate() const {
    if (m_video) {
        AVRational rate = av_guess_frame_rate(m_ctx.get(), m_video, nullptr);
        if (rate.num > 0 && rate.den > 0) return rate;
    }
    return AVRational{30000, 1001};
}

void MediaInput::Account(AVPacket *packet, Track &track) {
    const AVStream *stream = m_ctx->streams[packet->stream_index];
    AVRational time_base = stream->time_base;

    // Discarded packets (e.g. encoder priming) are never presented, so they do not extend the pass.
    if (packet->pts != AV_NOPTS_VALUE && !(packet->flags & AV_PKT_FLAG_DISCARD)) {
        int64_t duration = packet->duration > 0 ? packet->duration : track.default_duration;
        int64_t start = packet->pts;
        int64_t end = packet->pts + duration;

        // Samples the decoder trims (encoder delay and padding, e.g. in MP3) are not presented either.
        size_t size = 0;
        const uint8_t *skip = av_packet_get_side_data(packet, AV_PKT_DATA_SKIP_SAMPLES, &size);
        int sample_rate = stream->codecpar->sample_rate;
        if (skip && size >= 8 && sample_rate > 0) {
            start += av_rescale_q(AV_RL32(skip), AVRational{1, sample_rate}, time_base);
            end -= av_rescale_q(AV_RL32(skip + 4), AVRational{1, sample_rate}, time_base);
        }

        if (end > start) {
            if (m_first_pass) m_pass_start_us = std::min(m_pass_start_us, ToMicros(start, time_base));
            m_pass_end_us = std::max(m_pass_end_us, ToMicros(end, time_base));
        }
    }

    if (track.offset != 0) {
        if (packet->pts != AV_NOPTS_VALUE) packet->pts += track.offset;
        if (packet->dts != AV_NOPTS_VALUE) packet->dts += track.offset;
    }
}

bool MediaInput::IsLive() const {
    bool seekable = m_ctx->pb && (m_ctx->pb->seekable & AVIO_SEEKABLE_NORMAL);
    bool bounded = m_ctx->duration > 0; // AV_NOPTS_VALUE is negative
    return !bounded || (m_ctx->pb && !seekable);
}

PacketPtr MediaInput::Read() {
    PacketPtr packet = MakePacket();

    while (!m_stop.stop_requested()) {
        int ret = av_read_frame(m_ctx.get(), packet.get());
        if (ret == AVERROR(EAGAIN)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (ret == AVERROR_EOF) return nullptr;
        if (ret < 0) {
            if (m_stop.stop_requested()) return nullptr;
            if (m_ctx->pb && avio_feof(m_ctx->pb)) return nullptr;
            throw Error("reading input", ret);
        }

        int index = packet->stream_index;
        if (index < 0 || index >= static_cast<int>(m_tracks.size()) || !m_tracks[index].selected) {
            av_packet_unref(packet.get());
            continue;
        }

        Account(packet.get(), m_tracks[index]);
        return packet;
    }

    return nullptr;
}

bool MediaInput::Restart() {
    if (m_pass_end_us == INT64_MIN || m_pass_start_us == INT64_MAX || m_pass_end_us <= m_pass_start_us) {
        log::Warn("cannot loop: the input has no usable timestamps");
        return false;
    }

    int64_t target = m_ctx->start_time != AV_NOPTS_VALUE ? m_ctx->start_time : 0;
    int ret = avformat_seek_file(m_ctx.get(), -1, INT64_MIN, target, target, 0);
    if (ret < 0) {
        log::Warn("cannot loop: seeking to the start failed ({})", ErrorString(ret));
        return false;
    }

    m_offset_us += m_pass_end_us - m_pass_start_us;
    m_first_pass = false;
    m_pass_end_us = INT64_MIN;

    for (unsigned i = 0; i < m_tracks.size(); i++) {
        if (m_tracks[i].selected) m_tracks[i].offset = FromMicros(m_offset_us, m_ctx->streams[i]->time_base);
    }

    log::Debug("restarted input; timeline offset is now {}", FormatDuration(m_offset_us));
    return true;
}

void MediaInput::LogInfo() const {
    std::string duration = m_ctx->duration > 0 ? FormatDuration(m_ctx->duration) : "unknown";
    log::Info("input: {} ({}), duration {}{}", m_ctx->url ? m_ctx->url : "?", m_ctx->iformat->name, duration,
              IsLive() ? ", live" : "");

    if (m_video) {
        const AVCodecParameters *par = m_video->codecpar;
        AVRational rate = VideoFrameRate();
        log::Info("  video: stream #{} {}, {}x{}, {:.3f} fps", m_video->index, DescribeCodec(par), par->width,
                  par->height, av_q2d(rate));
    }
    if (m_audio) {
        const AVCodecParameters *par = m_audio->codecpar;
        char layout[128] = "unknown";
        av_channel_layout_describe(&par->ch_layout, layout, sizeof(layout));
        log::Info("  audio: stream #{} {}, {} Hz, {}", m_audio->index, DescribeCodec(par), par->sample_rate, layout);
    }
}

} // namespace ndistreamer::av
