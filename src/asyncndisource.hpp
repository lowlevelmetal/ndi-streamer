/**
 * @file asyncndisource.hpp
 * @brief This file includes utilities for working with NDI asynchronously.
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <string>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

#include "averror.hpp"
#include "decoder.hpp"
#include "ndi.hpp"

namespace AV::Utils {

class AsyncNdiSource;
using AsyncNdiSourceResult = std::pair<std::unique_ptr<AsyncNdiSource>, const AvException>;

typedef struct VideoFrameInfo {
    AVFrame *frame;
	AVPixelFormat format;
	AVRational time_base;
	CodecFrameRate fps;
} VideoFrameInfo;

typedef struct AudioFrameInfo {
	AVFrame *frame;
	AVRational time_base;
} AudioFrameInfo;

class AsyncNdiSource : public Ndi {
private:
    AsyncNdiSource(const std::string &ndi_source_name);
    AvError m_Initialize();

    void m_VideoThread();
    void m_AudioThread();

	static void m_FreeAVFrame(AVFrame *frame);
	
	static AvError m_CopyAudioFrame(const AudioFrameInfo &src_frame, AudioFrameInfo &dst_frame);
	static AvError m_CopyVideoFrame(const VideoFrameInfo &src_frame, VideoFrameInfo &dst_frame);

public:
    ~AsyncNdiSource();

    // Factory method
    static AsyncNdiSourceResult Create(const std::string &ndi_source_name);

    AvException LoadVideoFrame(AVFrame *frame, AVPixelFormat format, const AVRational &time_base, const CodecFrameRate &fps);
    AvException LoadAudioFrame(AVFrame *frame, const AVRational &time_base);

    void Start();

private:
    std::string m_ndi_source_name;

    // These are basically shared memory buffers filled with frames
    std::deque<AudioFrameInfo> m_audio_frames;
    std::deque<VideoFrameInfo> m_video_frames;

    std::thread m_audio_thread;
    std::thread m_video_thread;

    // Use these mutexes when accessing shared memory
    std::mutex m_audio_buffer_mutex;
    std::mutex m_video_buffer_mutex;

    // Set this to true to stop the threads
    std::atomic<bool> m_shutdown = false;

    // This becomes true when the threads are started 
    bool threads_start = false;

    // NDI Source
    NDIlib_send_instance_t m_ndi_send_instance = nullptr;

    // Thread local storage
    AudioFrameInfo m_audio_frame{};
    VideoFrameInfo m_video_frame{};

    // Condition variables for sleeping
    std::mutex m_audio_sleep_mutex;
    std::mutex m_video_sleep_mutex;
    std::condition_variable m_audio_cv;
    std::condition_variable m_video_cv;
    std::atomic<bool> m_audio_thread_sleeping = false;
    std::atomic<bool> m_video_thread_sleeping = false;

    // Variable to track when video started
    std::chrono::time_point<std::chrono::high_resolution_clock> m_video_start_time;
};

} // namespace AV::Utils