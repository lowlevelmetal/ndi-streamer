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
#include <libavutil/samplefmt.h>
}

namespace av::transcode {

// The TranscoderConfig struct is used to configure the transcoder
typedef struct TranscoderConfig {
    std::string input_file;            // Input file
    std::string output_file;           // Output file
    enum AVPixelFormat pixel_format;   // Desired pixel format
    enum AVSampleFormat sample_format; // Desired sample format
    int width;                         // Desired width
    int height;                        // Desired height
    int sample_rate;                   // Desired sample rate
    int channels;                      // Desired number of channels
    bool skip_pix_transcode;           // Skip transcoding video
    bool skip_sample_transcode;        // Skip transcoding audio

    TranscoderConfig(std::string input_file) : input_file(""), output_file(""), skip_pix_transcode(true),
                                               skip_sample_transcode(true) {}

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
    DynamicTranscoder();
    DynamicTranscoder(TranscoderConfig &config);
    ~DynamicTranscoder();

    // Enable/disable sample/pixel format transcoding
    void EnablePixelFormatTranscode(enum AVPixelFormat pixfmt, int w, int h);
    void EnableSampleTranscode(enum AVSampleFormat smplfmt, int smplrt, int chs);
    void DisablePixelFormatTranscode();
    void DisableSampleTranscode();

private:
    int m_OpenFile(std::string &file);

    TranscoderConfig m_config;
    bool m_initialized = false;
    bool m_file_output = false;
};

} // namespace av::transcode