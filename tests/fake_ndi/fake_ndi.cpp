/**
 * @file fake_ndi.cpp
 * @brief Stand-in for libndi.so used by the test suite.
 *
 * Implements the sending entry points ndistreamer uses and writes one line per call to the file named
 * by FAKE_NDI_LOG (opened when a sender is created):
 *
 *   create <name>|<groups>|<clock_video>|<clock_audio>
 *   metadata <xml>
 *   video <mono_ns> <xres> <yres> <fourcc> <fps_n> <fps_d> <aspect> <format_type> <timecode> <stride>
 *   audio <mono_ns> <rate> <channels> <samples> <stride_bytes> <timecode> <rms_of_channel_0>
 *   flush
 *   destroy
 *
 * Like the real runtime it keeps reading an asynchronously submitted video frame until the next
 * synchronizing call, so a frame released too early shows up as a use-after-free under AddressSanitizer.
 * Setting FAKE_NDI_UNSUPPORTED_CPU makes NDIlib_is_supported_CPU() fail.
 */

#include "ndi/abi.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

using namespace ndistreamer::ndi::abi;

namespace {

struct FakeSender {
    std::mutex mutex;
    std::FILE *log = nullptr;
    std::string full_name;
    Source source{};
    const uint8_t *pending = nullptr;
    size_t pending_size = 0;
    uint64_t checksum = 0;
};

long long NowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

size_t FrameBytes(const VideoFrameV2 *frame) {
    size_t stride = static_cast<size_t>(frame->line_stride_in_bytes);
    size_t rows = static_cast<size_t>(frame->yres);
    size_t chroma_rows = (rows + 1) / 2;
    switch (frame->FourCC) {
    case 0x3231564E: // NV12
        return stride * rows + stride * chroma_rows;
    case 0x30323449: // I420
        return stride * rows + 2 * (stride / 2) * chroma_rows;
    default: // single plane
        return stride * rows;
    }
}

// Touch every byte, as NDI's encoder would.
uint64_t Read(const uint8_t *data, size_t size) {
    uint64_t sum = 0;
    for (size_t i = 0; i < size; i++) sum += data[i];
    return sum;
}

void FinishPending(FakeSender *sender) {
    if (!sender->pending) return;
    sender->checksum += Read(sender->pending, sender->pending_size);
    sender->pending = nullptr;
}

} // namespace

#define FAKE_API extern "C" __attribute__((visibility("default")))

FAKE_API bool NDIlib_initialize() {
    return true;
}

FAKE_API void NDIlib_destroy() {}

FAKE_API const char *NDIlib_version() {
    return "FAKE NDI 6.0.0";
}

FAKE_API bool NDIlib_is_supported_CPU() {
    return std::getenv("FAKE_NDI_UNSUPPORTED_CPU") == nullptr;
}

FAKE_API SendInstance NDIlib_send_create(const SendCreate *create) {
    auto *sender = new FakeSender;
    if (const char *path = std::getenv("FAKE_NDI_LOG")) sender->log = std::fopen(path, "w");

    sender->full_name = std::string("FAKEHOST (") + (create && create->p_ndi_name ? create->p_ndi_name : "") + ")";
    sender->source.p_ndi_name = sender->full_name.c_str();
    sender->source.p_url_address = "127.0.0.1:5961";

    if (sender->log && create) {
        std::fprintf(sender->log, "create %s|%s|%d|%d\n", create->p_ndi_name ? create->p_ndi_name : "",
                     create->p_groups ? create->p_groups : "", create->clock_video, create->clock_audio);
    }
    return reinterpret_cast<SendInstance>(sender);
}

FAKE_API void NDIlib_send_destroy(SendInstance instance) {
    auto *sender = reinterpret_cast<FakeSender *>(instance);
    FinishPending(sender);
    if (sender->log) {
        std::fprintf(sender->log, "destroy\n");
        std::fclose(sender->log);
    }
    delete sender;
}

FAKE_API void NDIlib_send_send_video_async_v2(SendInstance instance, const VideoFrameV2 *frame) {
    auto *sender = reinterpret_cast<FakeSender *>(instance);
    std::lock_guard lock(sender->mutex);

    // Synchronizing call: the previous frame must still be readable until now.
    FinishPending(sender);

    if (!frame) {
        if (sender->log) std::fprintf(sender->log, "flush\n");
        return;
    }

    sender->pending = frame->p_data;
    sender->pending_size = FrameBytes(frame);

    if (sender->log) {
        const char *fourcc = reinterpret_cast<const char *>(&frame->FourCC);
        std::fprintf(sender->log, "video %lld %d %d %.4s %d %d %.4f %d %lld %d\n", NowNs(), frame->xres, frame->yres,
                     fourcc, frame->frame_rate_N, frame->frame_rate_D, frame->picture_aspect_ratio,
                     static_cast<int>(frame->frame_format_type), static_cast<long long>(frame->timecode),
                     frame->line_stride_in_bytes);
    }
}

FAKE_API void NDIlib_send_send_audio_v2(SendInstance instance, const AudioFrameV2 *frame) {
    auto *sender = reinterpret_cast<FakeSender *>(instance);
    std::lock_guard lock(sender->mutex);

    double sum = 0;
    for (int c = 0; c < frame->no_channels; c++) {
        const float *channel = reinterpret_cast<const float *>(reinterpret_cast<const uint8_t *>(frame->p_data) +
                                                               static_cast<size_t>(c) * frame->channel_stride_in_bytes);
        for (int i = 0; i < frame->no_samples; i++) {
            if (c == 0) sum += static_cast<double>(channel[i]) * channel[i];
            else sender->checksum += channel[i] != 0.0f; // read every channel
        }
    }
    double rms = frame->no_samples > 0 ? std::sqrt(sum / frame->no_samples) : 0.0;

    if (sender->log) {
        std::fprintf(sender->log, "audio %lld %d %d %d %d %lld %.5f\n", NowNs(), frame->sample_rate, frame->no_channels,
                     frame->no_samples, frame->channel_stride_in_bytes, static_cast<long long>(frame->timecode), rms);
    }
}

FAKE_API int NDIlib_send_get_no_connections(SendInstance, uint32_t) {
    return 1;
}

FAKE_API const Source *NDIlib_send_get_source_name(SendInstance instance) {
    return &reinterpret_cast<FakeSender *>(instance)->source;
}

FAKE_API void NDIlib_send_add_connection_metadata(SendInstance instance, const MetadataFrame *metadata) {
    auto *sender = reinterpret_cast<FakeSender *>(instance);
    if (sender->log && metadata && metadata->p_data) std::fprintf(sender->log, "metadata %s\n", metadata->p_data);
}
