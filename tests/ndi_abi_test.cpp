/**
 * @file ndi_abi_test.cpp
 * @brief Verifies src/ndi/abi.hpp against the official NDI SDK headers.
 *
 * Every check is a static_assert, so a successful build is the test. Built only when the SDK is
 * available (configure with -DNDI_SDK_DIR=...).
 */

// The SDK headers rely on these being included first.
#include <cstddef>
#include <cstdint>

#include <Processing.NDI.Lib.h>

#include "frames.hpp"
#include "ndi/abi.hpp"

#include <type_traits>

namespace abi = ndistreamer::ndi::abi;

#define SAME_SIZE(ours, theirs) static_assert(sizeof(ours) == sizeof(theirs) && alignof(ours) == alignof(theirs))
#define SAME_FIELD(ours, theirs, ours_field, theirs_field)                                                             \
    static_assert(offsetof(ours, ours_field) == offsetof(theirs, theirs_field));                                       \
    static_assert(sizeof(std::declval<ours>().ours_field) == sizeof(std::declval<theirs>().theirs_field))

SAME_SIZE(abi::SendCreate, NDIlib_send_create_t);
SAME_FIELD(abi::SendCreate, NDIlib_send_create_t, p_ndi_name, p_ndi_name);
SAME_FIELD(abi::SendCreate, NDIlib_send_create_t, p_groups, p_groups);
SAME_FIELD(abi::SendCreate, NDIlib_send_create_t, clock_video, clock_video);
SAME_FIELD(abi::SendCreate, NDIlib_send_create_t, clock_audio, clock_audio);

SAME_SIZE(abi::VideoFrameV2, NDIlib_video_frame_v2_t);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, xres, xres);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, yres, yres);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, FourCC, FourCC);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, frame_rate_N, frame_rate_N);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, frame_rate_D, frame_rate_D);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, picture_aspect_ratio, picture_aspect_ratio);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, frame_format_type, frame_format_type);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, timecode, timecode);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, p_data, p_data);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, line_stride_in_bytes, line_stride_in_bytes);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, p_metadata, p_metadata);
SAME_FIELD(abi::VideoFrameV2, NDIlib_video_frame_v2_t, timestamp, timestamp);

SAME_SIZE(abi::AudioFrameV2, NDIlib_audio_frame_v2_t);
SAME_FIELD(abi::AudioFrameV2, NDIlib_audio_frame_v2_t, sample_rate, sample_rate);
SAME_FIELD(abi::AudioFrameV2, NDIlib_audio_frame_v2_t, no_channels, no_channels);
SAME_FIELD(abi::AudioFrameV2, NDIlib_audio_frame_v2_t, no_samples, no_samples);
SAME_FIELD(abi::AudioFrameV2, NDIlib_audio_frame_v2_t, timecode, timecode);
SAME_FIELD(abi::AudioFrameV2, NDIlib_audio_frame_v2_t, p_data, p_data);
SAME_FIELD(abi::AudioFrameV2, NDIlib_audio_frame_v2_t, channel_stride_in_bytes, channel_stride_in_bytes);
SAME_FIELD(abi::AudioFrameV2, NDIlib_audio_frame_v2_t, p_metadata, p_metadata);
SAME_FIELD(abi::AudioFrameV2, NDIlib_audio_frame_v2_t, timestamp, timestamp);

SAME_SIZE(abi::MetadataFrame, NDIlib_metadata_frame_t);
SAME_FIELD(abi::MetadataFrame, NDIlib_metadata_frame_t, length, length);
SAME_FIELD(abi::MetadataFrame, NDIlib_metadata_frame_t, timecode, timecode);
SAME_FIELD(abi::MetadataFrame, NDIlib_metadata_frame_t, p_data, p_data);

SAME_SIZE(abi::Source, NDIlib_source_t);
SAME_FIELD(abi::Source, NDIlib_source_t, p_ndi_name, p_ndi_name);
SAME_FIELD(abi::Source, NDIlib_source_t, p_url_address, p_url_address);

SAME_SIZE(abi::Tally, NDIlib_tally_t);
SAME_FIELD(abi::Tally, NDIlib_tally_t, on_program, on_program);
SAME_FIELD(abi::Tally, NDIlib_tally_t, on_preview, on_preview);

static_assert(sizeof(abi::FrameFormatType) == sizeof(NDIlib_frame_format_type_e));
static_assert(static_cast<int>(abi::FrameFormatProgressive) == NDIlib_frame_format_type_progressive);
static_assert(static_cast<int>(abi::FrameFormatInterleaved) == NDIlib_frame_format_type_interleaved);
static_assert(abi::kTimecodeSynthesize == NDIlib_send_timecode_synthesize);

using ndistreamer::VideoFourCC;
static_assert(static_cast<uint32_t>(VideoFourCC::UYVY) == NDIlib_FourCC_video_type_UYVY);
static_assert(static_cast<uint32_t>(VideoFourCC::NV12) == NDIlib_FourCC_video_type_NV12);
static_assert(static_cast<uint32_t>(VideoFourCC::I420) == NDIlib_FourCC_video_type_I420);
static_assert(static_cast<uint32_t>(VideoFourCC::BGRA) == NDIlib_FourCC_video_type_BGRA);
static_assert(static_cast<uint32_t>(VideoFourCC::BGRX) == NDIlib_FourCC_video_type_BGRX);
static_assert(static_cast<uint32_t>(VideoFourCC::RGBA) == NDIlib_FourCC_video_type_RGBA);
static_assert(static_cast<uint32_t>(VideoFourCC::RGBX) == NDIlib_FourCC_video_type_RGBX);

// Function signatures: the loader casts dlsym() results to these types.
static_assert(std::is_same_v<abi::InitializeFn, decltype(&NDIlib_initialize)>);
static_assert(std::is_same_v<abi::DestroyFn, decltype(&NDIlib_destroy)>);
static_assert(std::is_same_v<abi::VersionFn, decltype(&NDIlib_version)>);
static_assert(std::is_same_v<abi::IsSupportedCpuFn, decltype(&NDIlib_is_supported_CPU)>);
static_assert(std::is_same_v<std::invoke_result_t<decltype(&NDIlib_send_get_no_connections), NDIlib_send_instance_t,
                                                  uint32_t>,
                             std::invoke_result_t<abi::SendGetNoConnectionsFn, abi::SendInstance, uint32_t>>);
static_assert(std::is_same_v<std::invoke_result_t<decltype(&NDIlib_send_get_tally), NDIlib_send_instance_t,
                                                  NDIlib_tally_t *, uint32_t>,
                             std::invoke_result_t<abi::SendGetTallyFn, abi::SendInstance, abi::Tally *, uint32_t>>);

int main() {
    return 0;
}
