/**
 * @file decoder.hpp
 * @brief Audio/video decoder with optional hardware acceleration.
 */

#pragma once

#include "av/ffmpeg.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace ndistreamer::av {

struct DecoderConfig {
    /// "none", "auto", or an FFmpeg hardware device type such as "vaapi" or "cuda".
    /// Only used for video streams.
    std::string hwaccel = "none";
    std::string hwaccel_device; ///< Optional device, e.g. "/dev/dri/renderD128" or a CUDA ordinal.
    int threads = 0;            ///< Decoder threads; 0 picks automatically.
};

/**
 * @brief Thin wrapper over the send/receive decoding API.
 *
 * Hardware decoding falls back to software, per stream, when the device cannot decode the stream's
 * codec or profile. Decoded hardware frames are returned as-is (still on the GPU).
 *
 * Corrupt input is skipped. A decoder that has worked but then fails continuously for 30 s throws.
 */
class Decoder {
public:
    Decoder(const AVStream *stream, const DecoderConfig &config);

    Decoder(const Decoder &) = delete;
    Decoder &operator=(const Decoder &) = delete;

    /**
     * @brief Submit a packet, or nullptr to start draining.
     * @return false if the decoder is full and frames must be received first.
     */
    bool Send(const AVPacket *packet);

    /**
     * @brief Fetch the next decoded frame.
     * @return nullptr when more input is required or the decoder is fully drained.
     */
    FramePtr Receive();

    /**
     * @brief Reset a drained decoder so it accepts input again (used at loop boundaries).
     */
    void Reset();

    /**
     * @brief Name of the hardware device type in use, or empty for software decoding.
     */
    std::string HardwareName() const;

    /**
     * @brief Hardware device context (may be null). Shared with hardware filters.
     */
    AVBufferRef *HardwareDevice() const { return m_hw_device.get(); }

    std::string CodecName() const;

private:
    bool SetupHardware(const AVCodec *codec, const DecoderConfig &config);
    int OpenDevice(const AVCodec *codec, AVHWDeviceType type, const std::string &device);
    void NoteError(const char *operation, int ret);

    static AVPixelFormat GetFormat(AVCodecContext *ctx, const AVPixelFormat *formats);

    CodecContextPtr m_ctx;
    BufferRefPtr m_hw_device;
    AVHWDeviceType m_hw_type = AV_HWDEVICE_TYPE_NONE;
    AVPixelFormat m_hw_format = AV_PIX_FMT_NONE;
    std::atomic<bool> m_fallback_reported{false};
    uint64_t m_frames_decoded = 0;
    int m_consecutive_errors = 0;
    std::chrono::steady_clock::time_point m_errors_since{};
    std::chrono::steady_clock::time_point m_last_error_report{};
};

} // namespace ndistreamer::av
