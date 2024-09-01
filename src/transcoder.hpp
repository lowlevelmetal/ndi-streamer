/*
 * ndi-streamer
 * transcoder.hpp
 *
 * 09-01-2024
 * Matthew Todd Geiger
 */

#pragma once

// Standard includes
#include <cstdint>
#include <string>

// 3rd party includes
extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace AV::Transcode {

enum StreamType {
    Video,
    Audio,
    None,
};

enum TranscoderErrorCode {
    NoError,
    InvalidInputFile,
    UnsupportedVideoCodec,
    UnsupportedAudioCodec,
    SwsContextError,
    SwrContextError,
    FrameAllocError,
    AvMallocError,
    NotInitialized,
    FrameReadError,
};

// The TranscoderConfig struct is used to configure the transcoder
typedef struct TranscoderConfig {
    std::string input_file;            // Input file          // Output file
    enum AVPixelFormat pixel_format;   // Desired pixel format
    enum AVSampleFormat sample_format; // Desired sample format
    int width;                         // Desired width
    int height;                        // Desired height
    int sample_rate;                   // Desired sample rate
    int channels;                      // Desired number of channels
    bool skip_pix_transcode;           // Skip transcoding video
    bool skip_sample_transcode;        // Skip transcoding audio

    TranscoderConfig(std::string infile) : input_file(infile), skip_pix_transcode(true),
                                               skip_sample_transcode(true), width(0), height(0) {}

    void EnablePixelFormatTranscode(enum AVPixelFormat pixfmt, int w, int h) {
        skip_pix_transcode = false;
        pixel_format = pixfmt;
        width = w;
        height = h;
    }

    void EnableSampleTranscode(enum AVSampleFormat smplfmt, int smplrt, int chs) {
        skip_sample_transcode = false;
        sample_format = smplfmt;
        sample_rate = smplrt;
        channels = chs;
    }

} TranscoderConfig;

// The DynamicTranscoder class is reponsible for decoding and encoding frames
// of audio and video data. This class is used to transcode media files into
// formats acceptable by the NDI SDK, or whatever this might be used for in the future.
//
// This class is designed to be used in a dynamic fashion, mission critial transcoding
// should be done in a more controlled environment.
class DynamicTranscoder {
public:
    DynamicTranscoder(TranscoderConfig &config);
    ~DynamicTranscoder();

    // Tell the transcoder to process a frame then return stream type
    // If you have transcoding enabled, this will also happen at this stage
    StreamType ProcessFrame();

    // Get the video frame buffer
    uint8_t *GetVideoFrameBuffer();
    int GetVideoFrameWidth();
    int GetVideoFrameHeight();
    int GetVideoFrameFrNum();
    int GetVideoFrameFrDen();

    // Get the audio frame buffer
    uint8_t *GetAudioFrameBuffer();

    // Enable/disable sample/pixel format transcoding
    void EnablePixelFormatTranscode(enum AVPixelFormat pixfmt, int w, int h);
    void EnableSampleTranscode(enum AVSampleFormat smplfmt, int smplrt, int chs);
    void DisablePixelFormatTranscode();
    void DisableSampleTranscode();

    // Get the last error code
    TranscoderErrorCode GetLastError();

    // Check if the transcoder is initialized
    bool IsInitialized();

private:
    TranscoderErrorCode m_InitializeTranscoder();

    TranscoderConfig m_config;
    bool m_initialized = false;
    TranscoderErrorCode m_last_error = TranscoderErrorCode::NoError;

    AVFrame *m_video_frame = nullptr;
    AVFrame *m_transcoded_video_frame = nullptr;
    uint8_t *m_transcoded_video_buffer = nullptr;

    // This is the FFMPEG context for the entire input file
    AVFormatContext *m_pformat_context = nullptr;

    // This is the packet that will be used to read frames
    AVPacket m_packet;

    // Here we need to create the codec context for the video and audio streams
    AVCodecContext *m_pvideo_codec_context = nullptr;
    AVCodecContext *m_paudio_codec_context = nullptr;

    // This is the video and audio stream index
    // This is used to get the correct stream from the FFMPEG context
    int m_video_stream_index = -1;
    int m_audio_stream_index = -1;

    // Transcoding contexts
    SwsContext *m_psws_context = nullptr; // Pixel format
    SwrContext *m_pswr_context = nullptr; // Sample format
    
};

} // namespace av::transcode