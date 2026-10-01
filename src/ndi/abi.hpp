/**
 * @file abi.hpp
 * @brief The subset of the NDI C ABI used for sending.
 *
 * The NDI runtime is loaded at run time, so the NDI SDK is not needed to build. These declarations
 * mirror the SDK's structures for interoperability; tests/ndi_abi_test.cpp checks them against the
 * official headers whenever the SDK is available (configure with -DNDI_SDK_DIR=...).
 */

#pragma once

#include <cstdint>

namespace ndistreamer::ndi::abi {

using SendInstance = struct SendInstanceType *;

/// Mirrors NDIlib_send_create_t.
struct SendCreate {
    const char *p_ndi_name;
    const char *p_groups;
    bool clock_video;
    bool clock_audio;
};

/// Mirrors NDIlib_frame_format_type_e.
enum FrameFormatType : int32_t {
    FrameFormatInterleaved = 0,
    FrameFormatProgressive = 1,
};

/// Mirrors NDIlib_video_frame_v2_t.
struct VideoFrameV2 {
    int xres;
    int yres;
    uint32_t FourCC;
    int frame_rate_N;
    int frame_rate_D;
    float picture_aspect_ratio;
    FrameFormatType frame_format_type;
    int64_t timecode;
    uint8_t *p_data;
    int line_stride_in_bytes;
    const char *p_metadata;
    int64_t timestamp;
};

/// Mirrors NDIlib_audio_frame_v2_t (planar 32-bit float).
struct AudioFrameV2 {
    int sample_rate;
    int no_channels;
    int no_samples;
    int64_t timecode;
    float *p_data;
    int channel_stride_in_bytes;
    const char *p_metadata;
    int64_t timestamp;
};

/// Mirrors NDIlib_metadata_frame_t.
struct MetadataFrame {
    int length;
    int64_t timecode;
    char *p_data;
};

/// Mirrors NDIlib_source_t.
struct Source {
    const char *p_ndi_name;
    const char *p_url_address;
};

/// Mirrors NDIlib_tally_t.
struct Tally {
    bool on_program;
    bool on_preview;
};

/// Mirrors NDIlib_send_timecode_synthesize.
inline constexpr int64_t kTimecodeSynthesize = INT64_MAX;

using InitializeFn = bool (*)();
using DestroyFn = void (*)();
using VersionFn = const char *(*)();
using IsSupportedCpuFn = bool (*)();
using SendCreateFn = SendInstance (*)(const SendCreate *);
using SendDestroyFn = void (*)(SendInstance);
using SendVideoAsyncV2Fn = void (*)(SendInstance, const VideoFrameV2 *);
using SendAudioV2Fn = void (*)(SendInstance, const AudioFrameV2 *);
using SendGetNoConnectionsFn = int (*)(SendInstance, uint32_t);
using SendGetTallyFn = bool (*)(SendInstance, Tally *, uint32_t);
using SendGetSourceNameFn = const Source *(*)(SendInstance);
using SendAddConnectionMetadataFn = void (*)(SendInstance, const MetadataFrame *);

} // namespace ndistreamer::ndi::abi
