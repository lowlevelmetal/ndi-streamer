/**
 * @file asyncndisource.cpp
 * @brief This file includes utilities for working with NDI asynchronously.
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#include "asyncndisource.hpp"
#include "macro.hpp"

extern "C" {
#include <libavutil/imgutils.h>
}

namespace AV::Utils {

AsyncNdiSource::AsyncNdiSource(const std::string &ndi_source_name) : m_ndi_source_name(ndi_source_name) {
    auto err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw AvException(err);
    }
}

AsyncNdiSource::~AsyncNdiSource() {
    DEBUG("AsyncNdiSource destructor called");
    
    // Stop the threads
    m_shutdown = true;
    
    if(m_audio_thread.joinable()) {
        m_audio_thread.join();
        DEBUG("Audio thread joined");
    }

    if(m_video_thread.joinable()) {
        m_video_thread.join();
        DEBUG("Video thread joined");
    }

    // Shutdown ndi source
    if(m_ndi_send_instance) {
        NDIlib_send_destroy(m_ndi_send_instance);
        DEBUG("NDI send instance destroyed");
    }
}


AsyncNdiSourceResult AsyncNdiSource::Create(const std::string &ndi_source_name) {
    AvException err;

    try {
        return {std::unique_ptr<AsyncNdiSource>(new AsyncNdiSource(ndi_source_name)), AvException(AvError::NOERROR)};
    } catch (const AvException &e) {
        err = e;
        DEBUG("Error creating AsyncNdiSource: %s", e.what());
    }

    return {nullptr, err};
}

void AsyncNdiSource::Start() {
    if(threads_start)
        return;
        
    m_audio_thread = std::thread(&AsyncNdiSource::m_AudioThread, this);
    m_video_thread = std::thread(&AsyncNdiSource::m_VideoThread, this);

    threads_start = true;
}

AvException AsyncNdiSource::LoadVideoFrame(AVFrame *frame, AVPixelFormat format, const AVRational &time_base, const CodecFrameRate &fps) {
    VideoFrameInfo video_frame{};

    // Copy to queue
    m_video_buffer_mutex.lock();

    if(m_video_frames.size() >= MAX_FRAMES_IN_BUFFER) {
        //DEBUG("Video frame buffer is full");
        m_video_buffer_mutex.unlock();
        return AvException(AvError::BUFFERFULL);
    }

    m_video_buffer_mutex.unlock();

    auto ret = m_CopyVideoFrame({frame, format, time_base, fps}, video_frame);
    if(ret != AvError::NOERROR) {
        DEBUG("Failed to copy video frame to queue");
        m_video_buffer_mutex.unlock();
        return AvException(ret);
    }

    m_video_buffer_mutex.lock();

    m_video_frames.push_back(video_frame);

    m_video_buffer_mutex.unlock();

    DEBUG("Video frame locked and loaded");

    return AvException(AvError::NOERROR);
}

AvException AsyncNdiSource::LoadAudioFrame(AVFrame *frame, const AVRational &time_base) {
    AudioFrameInfo audio_frame{};

    // Copy to queue
    m_audio_buffer_mutex.lock();

    if(m_audio_frames.size() >= MAX_FRAMES_IN_BUFFER) {
        //DEBUG("Audio frame buffer is full");
        m_audio_buffer_mutex.unlock();
        return AvException(AvError::BUFFERFULL);
    }

    m_audio_buffer_mutex.unlock();

    auto ret = m_CopyAudioFrame({frame, time_base}, audio_frame);
    if(ret != AvError::NOERROR) {
        DEBUG("Failed to copy audio frame to queue");
        m_audio_buffer_mutex.unlock();
        return AvException(ret);
    }

    m_audio_buffer_mutex.lock();

    m_audio_frames.push_back(audio_frame);

    m_audio_buffer_mutex.unlock();

    DEBUG("Audio frame locked and loaded");

    return AvException(AvError::NOERROR);
}

AvError AsyncNdiSource::m_Initialize() {
    // Setup ndi send instance
    NDIlib_send_create_t send_create_desc;
    send_create_desc.p_ndi_name = m_ndi_source_name.c_str();
    m_ndi_send_instance = NDIlib_send_create(&send_create_desc);
    if (!m_ndi_send_instance) {
        return AvError::NDISOURCECREATE;
    }

    return AvError::NOERROR;
}

void AsyncNdiSource::m_FreeAVFrame(AVFrame *frame) {
    if(frame) {
        av_frame_free(&frame);
    }
}

AvError AsyncNdiSource::m_CopyAudioFrame(const AudioFrameInfo &frame, AudioFrameInfo &m_audio_frame) {
    // First lets allocate a frame
    m_audio_frame.frame = av_frame_alloc();
    if(!m_audio_frame.frame) {
        DEBUG("Failed to allocate audio frame");
        return AvError::FRAMEALLOC;
    }

    // Copy frame metadata
    m_audio_frame.frame->nb_samples = frame.frame->nb_samples;
    m_audio_frame.frame->sample_rate = frame.frame->sample_rate;
    m_audio_frame.frame->format = frame.frame->format;
    m_audio_frame.frame->ch_layout = frame.frame->ch_layout;
    m_audio_frame.frame->pts = frame.frame->pts;

    // Allocate new buffer for frame data
    int ret = av_frame_get_buffer(m_audio_frame.frame, 0);
    if(ret < 0) {
        DEBUG("Failed to allocate audio frame buffer");
        m_FreeAVFrame(m_audio_frame.frame);
        return AvError::FRAMEGETBUFFER;
    }

    // Copy samples
    ret = av_samples_copy(m_audio_frame.frame->data, frame.frame->data, 0, 0,
                        frame.frame->nb_samples, frame.frame->ch_layout.nb_channels, (AVSampleFormat)frame.frame->format);
    if(ret < 0) {
        DEBUG("Failed to copy audio frame samples");
        m_FreeAVFrame(m_audio_frame.frame);
        return AvError::SAMPLECOPY;
    }

    // Copy the rest
    m_audio_frame.time_base = frame.time_base;

    return AvError::NOERROR;
}

AvError AsyncNdiSource::m_CopyVideoFrame(const VideoFrameInfo &frame, VideoFrameInfo &m_video_frame) {
    // First lets allocate a frame
    m_video_frame.frame = av_frame_alloc();
    if(!m_video_frame.frame) {
        DEBUG("Failed to allocate video frame");
        return AvError::FRAMEALLOC;
    }

    // Copy frame metadata
    m_video_frame.frame->width = frame.frame->width;
    m_video_frame.frame->height = frame.frame->height;
    m_video_frame.frame->format = frame.frame->format;
    m_video_frame.frame->pts = frame.frame->pts;
    m_video_frame.frame->format = frame.format;

    // Allocate new buffer for frame data
    int ret = av_frame_get_buffer(m_video_frame.frame, 32);
    if(ret < 0) {
        DEBUG("Failed to allocate video frame buffer");
        m_FreeAVFrame(m_video_frame.frame);
        return AvError::FRAMEGETBUFFER;
    }

    // Copy frame data
    av_image_copy(m_video_frame.frame->data, m_video_frame.frame->linesize,
                        (const uint8_t **)frame.frame->data, frame.frame->linesize,
                        frame.format, frame.frame->width, frame.frame->height);

    // Copy other data
    m_video_frame.format = frame.format;
    m_video_frame.time_base = frame.time_base;

    return AvError::NOERROR;
}

void AsyncNdiSource::m_VideoThread() {
    // Loop until shutdown is true and the queue is empty
    while(!m_shutdown || [this](){ std::unique_lock<std::mutex> lock(m_video_buffer_mutex); return !m_video_frames.empty(); }()) {
        // Copy video frame out of shared memory
        m_video_buffer_mutex.lock();
        
        // If queue is empty, unlock and continue
        if(m_video_frames.empty()) {
            //DEBUG("Video frame queue is empty");
            m_video_buffer_mutex.unlock();
            continue;
        }

        auto frame_ref = m_video_frames.front();

        // Grab a copy from shared memory
        if(m_CopyVideoFrame(frame_ref, m_video_frame) != AvError::NOERROR) {
            DEBUG("Failed to copy video frame to TLS");
            m_video_frames.pop_front();
            m_video_buffer_mutex.unlock();
            continue;
        }

        // Clean up the frame that was in the queue
        m_FreeAVFrame(frame_ref.frame);

        // Pop the front of the queue
        m_video_frames.pop_front();

        m_video_buffer_mutex.unlock();

        DEBUG("Popped video frame from queue"
            "Sample rate: %d\n"
            "Channels: %d\n"
            "Samples: %d\n"
            "no_samples * sizeof(int16_t): %ld\n"
            "Linesize: %d\n"
            "Timebase: %d/%d\n"
            "PTS: %ld\n",
            m_video_frame.frame->sample_rate, m_video_frame.frame->ch_layout.nb_channels, m_video_frame.frame->nb_samples, m_video_frame.frame->nb_samples * sizeof(int16_t), m_video_frame.frame->linesize[0], m_video_frame.time_base.num, m_video_frame.time_base.den, m_video_frame.frame->pts);

        // Create video frame
        NDIlib_video_frame_v2_t video_frame{};

        switch(m_video_frame.format) {
            case AV_PIX_FMT_UYVY422:
                video_frame.FourCC = NDIlib_FourCC_type_UYVY;
                break;
            default:
                DEBUG("Invalid pixel format");
                m_FreeAVFrame(m_video_frame.frame);
                continue;
        }

        if(m_video_frame.frame->pts != AV_NOPTS_VALUE) {
            DEBUG("Using PTS for video timing");
            double pts_in_seconds = m_video_frame.frame->pts * av_q2d(m_video_frame.time_base);
            int64_t ndi_timecode = (int64_t)(pts_in_seconds * 10000000.0);
            video_frame.timecode = ndi_timecode;
        } else if (m_video_frame.fps.first != 0 && m_video_frame.fps.second != 0) {
            DEBUG("Using FPS for video timing");
            video_frame.frame_rate_N = m_video_frame.fps.first;
            video_frame.frame_rate_D = m_video_frame.fps.second;
        } else {
            DEBUG("No PTS or FPS for video timecode");
            video_frame.timecode = NDIlib_send_timecode_synthesize;
        }

        video_frame.xres = m_video_frame.frame->width;
        video_frame.yres = m_video_frame.frame->height;
        video_frame.p_data = m_video_frame.frame->data[0];
        video_frame.line_stride_in_bytes = m_video_frame.frame->linesize[0];

        NDIlib_send_send_video_v2(m_ndi_send_instance, &video_frame);

        // Free the frame
        m_FreeAVFrame(m_video_frame.frame);
    }

    DEBUG("Video thread shutdown");
}

void AsyncNdiSource::m_AudioThread() {

    // Loop until shutdown is true and the queue is empty
    while(!m_shutdown || [this](){ std::unique_lock<std::mutex> lock(m_audio_buffer_mutex); return !m_audio_frames.empty(); }()) {
        // Copy audio frame out of shared memory
        m_audio_buffer_mutex.lock();
        
        // If queue is empty, unlock and continue
        if(m_audio_frames.empty()) {
            //DEBUG("Audio queue is empty, continuing");
            m_audio_buffer_mutex.unlock();
            continue;
        }

        auto frame_ref = m_audio_frames.front();

        // Grab a copy from shared memory
        if(m_CopyAudioFrame(frame_ref, m_audio_frame) != AvError::NOERROR) {
            DEBUG("Failed to copy audio frame to TLS");
            m_audio_frames.pop_front();
            m_audio_buffer_mutex.unlock();
            continue;
        }

        // Clean up the frame that was in the queue
        m_FreeAVFrame(frame_ref.frame);

        // Pop the front of the queue
        m_audio_frames.pop_front();

        m_audio_buffer_mutex.unlock();

        DEBUG("Popped audio frame from queue"
            "Sample rate: %d\n"
            "Channels: %d\n"
            "Samples: %d\n"
            "no_samples * sizeof(int16_t): %ld\n"
            "Linesize: %d\n"
            "Timebase: %d/%d\n"
            "PTS: %ld\n",
            m_audio_frame.frame->sample_rate, m_audio_frame.frame->ch_layout.nb_channels, m_audio_frame.frame->nb_samples, m_audio_frame.frame->nb_samples * sizeof(int16_t), m_audio_frame.frame->linesize[0], m_audio_frame.time_base.num, m_audio_frame.time_base.den, m_audio_frame.frame->pts);

        // Setup the audio frame
        NDIlib_audio_frame_interleaved_16s_t audio_frame{};

        if(m_audio_frame.frame->pts != AV_NOPTS_VALUE) {
            DEBUG("Using PTS for audio timecode");
            double pts_in_seconds = m_audio_frame.frame->pts * av_q2d(m_audio_frame.time_base);
            int64_t ndi_timecode = (int64_t)(pts_in_seconds * 10000000.0);
            audio_frame.timecode = ndi_timecode;
        } else {
            DEBUG("No PTS for audio timecode");
            audio_frame.timecode = NDIlib_send_timecode_synthesize;
        }

        audio_frame.sample_rate = m_audio_frame.frame->sample_rate;               // Sample rate
        audio_frame.no_channels = m_audio_frame.frame->ch_layout.nb_channels;     // Number of channels
        audio_frame.no_samples = m_audio_frame.frame->nb_samples;                 // Number of samples per channel

        // Create interleaved audio buffer
        int16_t *audio_buffer = new int16_t[audio_frame.no_channels * audio_frame.no_samples];
        memcpy(audio_buffer, m_audio_frame.frame->data[0], sizeof(int16_t) * audio_frame.no_channels * audio_frame.no_samples);

        audio_frame.p_data = audio_buffer;

        NDIlib_util_send_send_audio_interleaved_16s(m_ndi_send_instance, &audio_frame);

        delete[] audio_buffer;

        // Free the frame
        m_FreeAVFrame(m_audio_frame.frame);
    }

    DEBUG("Audio thread shutdown");
}

} // namespace AV::Utils