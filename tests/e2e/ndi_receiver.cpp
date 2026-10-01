/**
 * @file ndi_receiver.cpp
 * @brief Test helper: receive an NDI source and report what arrived.
 *
 * Usage: ndi_receiver SOURCE_NAME SECONDS
 *
 * Finds a source whose name contains SOURCE_NAME, receives for SECONDS and prints one line per frame:
 *   video <recv_ms> <xres> <yres> <fourcc> <fps_n> <fps_d> <timecode> <format_type>
 *   audio <recv_ms> <sample_rate> <channels> <samples> <timecode> <rms>
 * Exits 1 if the source cannot be found. Built only when the NDI SDK is available.
 */

// The SDK headers rely on these being included first.
#include <cstddef>
#include <cstdint>

#include <Processing.NDI.Lib.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s SOURCE_NAME SECONDS\n", argv[0]);
        return 2;
    }
    const std::string wanted = argv[1];
    const double seconds = std::atof(argv[2]);

    if (!NDIlib_initialize()) return 1;

    NDIlib_find_instance_t finder = NDIlib_find_create_v2();
    const NDIlib_source_t *match = nullptr;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!match && std::chrono::steady_clock::now() < deadline) {
        NDIlib_find_wait_for_sources(finder, 500);
        uint32_t count = 0;
        const NDIlib_source_t *sources = NDIlib_find_get_current_sources(finder, &count);
        for (uint32_t i = 0; i < count; i++) {
            if (std::strstr(sources[i].p_ndi_name, wanted.c_str())) match = &sources[i];
        }
    }
    if (!match) {
        std::fprintf(stderr, "source containing '%s' not found\n", wanted.c_str());
        return 1;
    }
    std::fprintf(stderr, "connecting to %s (%s)\n", match->p_ndi_name, match->p_url_address);

    NDIlib_recv_create_v3_t create;
    create.source_to_connect_to = *match;
    create.color_format = NDIlib_recv_color_format_fastest;
    create.bandwidth = NDIlib_recv_bandwidth_highest;
    NDIlib_recv_instance_t receiver = NDIlib_recv_create_v3(&create);
    NDIlib_find_destroy(finder);
    if (!receiver) return 1;

    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::duration<double>(seconds);
    while (std::chrono::steady_clock::now() < end) {
        NDIlib_video_frame_v2_t video;
        NDIlib_audio_frame_v2_t audio;
        double ms = 0;
        switch (NDIlib_recv_capture_v2(receiver, &video, &audio, nullptr, 100)) {
        case NDIlib_frame_type_video:
            ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            std::printf("video %.1f %d %d %.4s %d %d %lld %d\n", ms, video.xres, video.yres,
                        reinterpret_cast<const char *>(&video.FourCC), video.frame_rate_N, video.frame_rate_D,
                        static_cast<long long>(video.timecode), static_cast<int>(video.frame_format_type));
            NDIlib_recv_free_video_v2(receiver, &video);
            break;
        case NDIlib_frame_type_audio: {
            ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            double sum = 0;
            for (int i = 0; i < audio.no_samples; i++) sum += audio.p_data[i] * audio.p_data[i];
            double rms = audio.no_samples ? std::sqrt(sum / audio.no_samples) : 0;
            std::printf("audio %.1f %d %d %d %lld %.4f\n", ms, audio.sample_rate, audio.no_channels,
                        audio.no_samples, static_cast<long long>(audio.timecode), rms);
            NDIlib_recv_free_audio_v2(receiver, &audio);
            break;
        }
        default:
            break;
        }
        std::fflush(stdout);
    }

    NDIlib_recv_destroy(receiver);
    NDIlib_destroy();
    return 0;
}
