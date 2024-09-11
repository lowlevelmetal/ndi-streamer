/**
 * @file ndisource.cpp
 * @brief This file includes utilities for working with NDI.
 * @date 2024-09-08
 * @author Matthew Todd Geiger
 */

#include <chrono>

#include "ndisource.hpp"
#include "macro.hpp"

namespace AV::Utils {

AvException NdiSource::SendVideoFrame(AVFrame *frame, AVPixelFormat format, const AVRational &time_base, const CodecFrameRate &fps) {
#ifdef _DEBUG
    // Profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif

    // Setup the video frame
    NDIlib_video_frame_v2_t video_frame{};

    DEBUG("Sending video frame\n"
        "Width: %d\n"
        "Height: %d\n"
        "Format: %d\n"
        "Linesize: %d\n"
        "Timebase: %d/%d\n"
        "PTS: %ld\n"
        "FPS: %d/%d\n",
        frame->width, frame->height, format, frame->linesize[0], time_base.num, time_base.den, frame->pts, fps.first, fps.second);

    switch (format) {
    case AV_PIX_FMT_UYVY422:
        video_frame.FourCC = NDIlib_FourCC_type_UYVY;
        break;
    default:
        return AvException(AvError::NDIINVALIDPIXFMT);
    }

    if (frame->pts != AV_NOPTS_VALUE) {
        DEBUG("Using PTS for video timing");
        double pts_in_seconds = frame->pts * av_q2d(time_base);
        int64_t ndi_timecode = (int64_t)(pts_in_seconds * 10000000.0);
        video_frame.timecode = ndi_timecode;
    } else if (fps.first != 0 && fps.second != 0) {
        DEBUG("Using FPS for video timing");
        video_frame.frame_rate_N = fps.first;
        video_frame.frame_rate_D = fps.second;
    } else {
        DEBUG("No PTS or FPS for video timecode");
        video_frame.timecode = NDIlib_send_timecode_synthesize;
    }

    video_frame.xres = frame->width;
    video_frame.yres = frame->height;
    video_frame.p_data = frame->data[0];
    video_frame.line_stride_in_bytes = frame->linesize[0];

    //NDIlib_send_send_video_async_v2(m_send_instance, &video_frame);
    NDIlib_send_send_video_v2(m_send_instance, &video_frame);

#ifdef _DEBUG
    // Profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("Video Send time (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif
    return AvException(AvError::NOERROR);
}

AvException NdiSource::SendAudioFrameFLTPlanar(AVFrame *frame) {
#ifdef _DEBUG
    // Profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif

    DEBUG("Sending audio frame\n"
        "Sample rate: %d\n"
        "Channels: %d\n"
        "Samples: %d\n"
        "no_samples * sizeof(float): %ld\n"
        "Linesize: %d\n",
        frame->sample_rate, frame->ch_layout.nb_channels, frame->nb_samples, frame->nb_samples * sizeof(float), frame->linesize[0]);

    // Setup the audio frame
    NDIlib_audio_frame_v3_t audio_frame;
    audio_frame.sample_rate = frame->sample_rate;               // Sample rate
    audio_frame.no_channels = frame->ch_layout.nb_channels;     // Number of channels
    audio_frame.no_samples = frame->nb_samples;                 // Number of samples per channel
    audio_frame.channel_stride_in_bytes = sizeof(float) * audio_frame.no_samples;   

#ifdef _DEBUG
    // Profile copy
    auto time_copy_start = std::chrono::high_resolution_clock::now();
#endif

    // Create planar audio buffer
    float *audio_buffer = new float[audio_frame.no_channels * audio_frame.no_samples];
    for (int i = 0; i < audio_frame.no_channels; i++) {
        memcpy(audio_buffer + i * audio_frame.no_samples, frame->data[i], sizeof(float) * audio_frame.no_samples);
    }

    audio_frame.p_data = (uint8_t *)audio_buffer;

#ifdef _DEBUG
    // Profile copy
    auto time_copy_end = std::chrono::high_resolution_clock::now();
    DEBUG("Audio Copy time (seconds): %f", std::chrono::duration<double>(time_copy_end - time_copy_start).count());
#endif

    NDIlib_send_send_audio_v3(m_send_instance, &audio_frame);

    delete[] audio_buffer;

#ifdef _DEBUG
    // Profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("Audio Send time (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif

    return AvException(AvError::NOERROR);
}

AvException NdiSource::SendAudioFrameS16(AVFrame *frame, const AVRational &time_base) {
#ifdef _DEBUG
    // Profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif


    DEBUG("Sending audio frame\n"
        "Sample rate: %d\n"
        "Channels: %d\n"
        "Samples: %d\n"
        "no_samples * sizeof(int16_t): %ld\n"
        "Linesize: %d\n"
        "Timebase: %d/%d\n"
        "PTS: %ld\n",
        frame->sample_rate, frame->ch_layout.nb_channels, frame->nb_samples, frame->nb_samples * sizeof(int16_t), frame->linesize[0], time_base.num, time_base.den, frame->pts);

    // Setup the audio frame
    NDIlib_audio_frame_interleaved_16s_t audio_frame{};

    if (frame->pts != AV_NOPTS_VALUE) {
        DEBUG("Using PTS for audio timecode");
        double pts_in_seconds = frame->pts * av_q2d(time_base);
        int64_t ndi_timecode = (int64_t)(pts_in_seconds * 10000000.0);
        audio_frame.timecode = ndi_timecode;
    } else {
        DEBUG("No PTS for audio timecode");
        audio_frame.timecode = NDIlib_send_timecode_synthesize;
    }

    audio_frame.sample_rate = frame->sample_rate;               // Sample rate
    audio_frame.no_channels = frame->ch_layout.nb_channels;     // Number of channels
    audio_frame.no_samples = frame->nb_samples;                 // Number of samples per channel


    // Create audio buffer
    int16_t *audio_buffer = new int16_t[audio_frame.no_channels * audio_frame.no_samples];
    memcpy(audio_buffer, frame->data[0], sizeof(int16_t) * audio_frame.no_channels * audio_frame.no_samples);

    audio_frame.p_data = audio_buffer;
    
    NDIlib_util_send_send_audio_interleaved_16s(m_send_instance, &audio_frame);

    delete[] audio_buffer;

#ifdef _DEBUG
    // Profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("Audio Send time S16 (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif

    return AvException(AvError::NOERROR);
}

NdiSourceResult NdiSource::Create(const std::string &ndi_name) {
    DEBUG("NdiSource factory called");
    AvException error(AvError::NOERROR);

    // Create a new NdiSource object, return nullopt if error
    try {
        return {std::unique_ptr<NdiSource>(new NdiSource(ndi_name)), AvException(AvError::NOERROR)};
    } catch (AvException err) {
        DEBUG("NdiSource error: %s", err.what());
        error = err;
    }

    return {nullptr, error};
}

NdiSource::NdiSource(const std::string &ndi_name) : m_name(ndi_name) {
    DEBUG("Constructing NdiSource object");

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}

NdiSource::~NdiSource() {
    DEBUG("Destroying NdiSource object");

    if (m_send_instance) {
        DEBUG("Destroying NDI send instance");
        NDIlib_send_send_video_async_v2(m_send_instance, nullptr); // Make sure buffers are flushed before shutting down
        NDIlib_send_destroy(m_send_instance);
    }
}

AvError NdiSource::m_Initialize() {
    // Create a new send instance
    DEBUG("Creating NDI send instance");
    NDIlib_send_create_t send_create_desc;
    send_create_desc.p_ndi_name = m_name.c_str();
    send_create_desc.clock_video = true;
    //send_create_desc.clock_audio = true;
    m_send_instance = NDIlib_send_create(&send_create_desc);
    if (!m_send_instance) {
        return AvError::NDISENDINSTANCE;
    }

    return AvError::NOERROR;
}

} // namespace AV::Utils