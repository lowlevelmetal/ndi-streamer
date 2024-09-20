/**
 * @file asyncndisource.cpp
 * @brief This file includes utilities for working with NDI sources.
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#include "asyncndisource.hpp"
#include "macro.hpp"
#include "frame.hpp"

#include <iostream>
#include <chrono>

extern "C" {
#include <libavutil/imgutils.h>
}

namespace AV::Utils {

AsyncNDISourceResult AsyncNDISource::Create(const std::string &source_name, const AVRational &frame_rate) {
    FUNCTION_CALL_DEBUG();
    AvException err;

    try {
        return {std::shared_ptr<AsyncNDISource>(new AsyncNDISource(source_name, frame_rate)), AvError::NOERROR};
    } catch(const AvException e) {
        err = e;
        DEBUG("Error creating NDI source: %s", e.what());
    }

    return {nullptr, err};
}

AvException AsyncNDISource::SendFrame(const AVFrame *frame) {
    FUNCTION_CALL_DEBUG();

    // Clone frame
    AVFrame *frame_copy = av_frame_clone(frame);
    if(frame_copy == nullptr) {
        return AvError::FRAMEALLOC;
    }

    // Sleep until ready to send frame
    _frame_queue_mutex.lock();
    auto size = _frame_queue.size();
    _frame_queue_mutex.unlock();
    while(size >= FRAME_QUEUE_SIZE) {
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        _frame_queue_mutex.lock();
        size = _frame_queue.size();
        _frame_queue_mutex.unlock();
    }

    _frame_queue_mutex.lock();
    _frame_queue.push_back(frame_copy);
    _frame_queue_mutex.unlock();

    return AvError::NOERROR;
}

AsyncNDISource::AsyncNDISource(const std::string &source_name, const AVRational &frame_rate) : _source_name(source_name), _frame_rate(frame_rate) {
    FUNCTION_CALL_DEBUG();

    AvError err = _Initialize();
    if(err != AvError::NOERROR) {
        throw AvException(err);
    }
}

AsyncNDISource::~AsyncNDISource() {
    FUNCTION_CALL_DEBUG();

    _running = false;
    if(_frame_sender_thread.joinable()) {
        _frame_sender_thread.join();
    }

    if(_ndi_send_instance != nullptr) {
        NDIlib_send_destroy(_ndi_send_instance);
    }
}

AvError AsyncNDISource::_Initialize() {
    FUNCTION_CALL_DEBUG();

    // Create NDI send instance
    NDIlib_send_create_t send_create_desc;
    send_create_desc.p_ndi_name = _source_name.c_str();
    send_create_desc.clock_video = true;
    _ndi_send_instance = NDIlib_send_create(&send_create_desc);

    if(_ndi_send_instance == nullptr) {
        return AvError::NDISENDINSTANCE;
    }

    _frame_sender_thread = std::thread(&AsyncNDISource::_Thread_FrameSender, this);

    return AvError::NOERROR;
}

void AsyncNDISource::_Thread_FrameSender() {
    FUNCTION_CALL_DEBUG();

    while(_running || [this]{ std::unique_lock<std::mutex> lock(_frame_queue_mutex); return !_frame_queue.empty(); }()) {
        std::unique_lock<std::mutex> lock(_frame_queue_mutex);
        if(!_frame_queue.empty()) {
            AVFrame *frame = _frame_queue.front();
            _frame_queue.pop_front();
            lock.unlock();

            if(frame->width != 0 && frame->height != 0) {
                _SendVideoFrame(frame);
            } else {
                _SendAudioFrame(frame);
            }

            av_frame_free(&frame);
        } else {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
    
}

AvError AsyncNDISource::_SendVideoFrame(const AVFrame *frame) {
    FUNCTION_CALL_DEBUG();

#ifdef _DEBUG
    // profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif


    bool manual_free = false;

    // Build NDI packet from frame
    NDIlib_video_frame_v2_t video_frame;

    DEBUG("Frame metadata"
            "\n\tWidth: %d"
            "\n\tHeight: %d"
            "\n\tFormat: %d"
            "\n\tLinesize 1: %d"
            "\n\tLinesize 2: %d",
            frame->width, frame->height, frame->format, frame->linesize[0], frame->linesize[1]);

    switch(frame->format) {
    case AV_PIX_FMT_UYVY422:
        DEBUG("Sending UYVY frame");
        video_frame.FourCC = NDIlib_FourCC_type_UYVY;
        video_frame.p_data = frame->data[0];
        video_frame.line_stride_in_bytes = frame->linesize[0];
        break;
    case AV_PIX_FMT_RGB24:
        DEBUG("Sending RGB24 frame");
        video_frame.FourCC = NDIlib_FourCC_type_RGBA;
        video_frame.p_data = frame->data[0];
        video_frame.line_stride_in_bytes = frame->linesize[0];
        break;
    case AV_PIX_FMT_NV12:
        DEBUG("Sending NV12 frame");
        video_frame.FourCC = NDIlib_FourCC_type_NV12;
        video_frame.line_stride_in_bytes = frame->linesize[0];

        DEBUG("data[0]: %p", frame->data[0]);
        DEBUG("data[1]: %p", frame->data[1]);
        DEBUG("data[1] - data[0]: %ld | Frame 0 Size: %d", frame->data[1] - frame->data[0], frame->linesize[0] * frame->height);

        video_frame.p_data = CombinePlanesNV12(frame, 2);
        manual_free = true;

        break;
    default:
        return AvError::NDIINVALIDPIXFMT;
    }

    video_frame.xres = frame->width;
    video_frame.yres = frame->height;
    video_frame.frame_rate_N = _frame_rate.num;
    video_frame.frame_rate_D = _frame_rate.den;
    video_frame.timecode = NDIlib_send_timecode_synthesize;
    video_frame.frame_format_type = NDIlib_frame_format_type_progressive;

    // Send the frame
    NDIlib_send_send_video_v2(_ndi_send_instance, &video_frame);

    if(manual_free) {
        delete[] video_frame.p_data;
    }

#ifdef _DEBUG
    // profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("NDI Send Video Frame time (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif

    return AvError::NOERROR;
}

/**
 * Only send audio frames that are interleaved 16-bit signed PCM.
 */
AvError AsyncNDISource::_SendAudioFrame(const AVFrame *frame) {
    FUNCTION_CALL_DEBUG();

    if(frame->format != AV_SAMPLE_FMT_S16) {
        return AvError::INVALIDSMPLFMT;
    }
    
    // Build NDI packet from frame
    NDIlib_audio_frame_interleaved_16s_t audio_frame;

    audio_frame.sample_rate = frame->sample_rate;
    audio_frame.no_channels = frame->ch_layout.nb_channels;
    audio_frame.no_samples = frame->nb_samples;
    audio_frame.timecode = NDIlib_send_timecode_synthesize;
    audio_frame.p_data = (int16_t *)frame->data[0];

    // Send the frame
    NDIlib_util_send_send_audio_interleaved_16s(_ndi_send_instance, &audio_frame);

    return AvError::NOERROR;
}

} // namespace AV::Utils