/**
 * @file asyncndisource.cpp
 * @brief This file includes utilities for working with NDI asynchronously.
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#include "asyncndisource.hpp"
#include "macro.hpp"

namespace AV::Utils {

AsyncNdiSourceResult AsyncNdiSource::Create(const std::string &ndi_name) {
    AvException err;
    try {
        return {std::unique_ptr<AsyncNdiSource>(new AsyncNdiSource(ndi_name)), AvError::NOERROR};
    } catch (AvException &e) {
        DEBUG("Error creating AsyncNdiSource: %s", e.what());
        err = e;
    }

    return {nullptr, err};
}

AsyncNdiSource::AsyncNdiSource(const std::string &ndi_name) : m_name(ndi_name) {
    DEBUG("Creating AsyncNdiSource");
    auto err = m_Initialize();
    if (err != AvError::NOERROR) {
        m_last_error = err;
        throw AvException(err);
    }
}

AsyncNdiSource::~AsyncNdiSource() {
    DEBUG("Destroying AsyncNdiSource");

    // Shutdown threads
    m_shutdown_audio_s16_thread = true;
    m_shutdown_video_thread = true;
    m_video_thread.join();
    m_audio_s16_thread.join();

    DEBUG("Threads shutdown");

    // Destroy NDI send instance
    if (m_send_instance != nullptr) {
        NDIlib_send_destroy(m_send_instance);
        DEBUG("NDI send instance destroyed");
    }

    // Free audio buffer
    if(m_audio_s16_frame.p_data != nullptr) {
        delete[] m_audio_s16_frame.p_data;
    }
}

AvException AsyncNdiSource::LoadVideoFrame(AVFrame *frame, AVPixelFormat format, const AVRational &time_base, const CodecFrameRate &fps) {
	// Create video frame
	DEBUG("Loading frame into video thread"
			"Width: %d\n"
			"Height: %d\n"
			"Format: %d\n"
			"Linesize: %d\n"
			"Timebase: %d/%d\n"
			"PTS: %ld\n"
			"FPS: %d/%d\n",
			frame->width, frame->height, format, frame->linesize[0], time_base.num, time_base.den, frame->pts, fps.first, fps.second);

	NDIlib_video_frame_v2_t video_frame{};

	switch(format) {
		case AV_PIX_FMT_UYVY422:
			video_frame.FourCC = NDIlib_FourCC_type_UYVY;
			break;
		default:
			return AvError::NDIINVALIDPIXFMT;
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

	while (m_thread_using_video_frame); // Block until it is safe to load the frame
	
	// Load the frame into atomic space
	m_video_frame = video_frame;

	// Lets set this ourselves, the thread will change this to false when it is done
	m_thread_using_video_frame = true;

	return AvError::NOERROR;
}

AvException AsyncNdiSource::LoadAudioFrameS16(AVFrame *frame, const AVRational &time_base) {
    DEBUG("Loading audio frame\n"
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

	while (m_thread_using_audio_s16_frame); // Block until it is safe to load the frame

	// Delete the old buffer
    if(m_audio_s16_frame.p_data != nullptr) {
        delete[] m_audio_s16_frame.p_data;
    }

    audio_frame.p_data = audio_buffer;

	// Load the frame into atomic space
	m_audio_s16_frame = audio_frame;

	// Lets set this ourselves, the thread will change this to false when it is done
	m_thread_using_audio_s16_frame = true;

	return AvError::NOERROR;
}

AvError AsyncNdiSource::m_Initialize() {
    // Create NDI send instance
    NDIlib_send_create_t send_create_desc;
    send_create_desc.p_ndi_name = m_name.c_str();
    // send_create_desc.clock_video = true;

    m_send_instance = NDIlib_send_create(&send_create_desc);
    if (m_send_instance == nullptr) {
        return AvError::NDISENDINSTANCE;
    }

    // Start threads
    m_video_thread = std::thread(&AsyncNdiSource::m_VideoThread, this);
    m_audio_s16_thread = std::thread(&AsyncNdiSource::m_AudioS16Thread, this);

    return AvError::NOERROR;
}

void AsyncNdiSource::m_VideoThread() {
    DEBUG("Video thread started");

    while (!m_shutdown_video_thread) {
		// Send!
		if(m_thread_using_video_frame) {
			NDIlib_send_send_video_v2(m_send_instance, &m_video_frame);
			m_thread_using_video_frame = false;
            DEBUG("Video frame sent");
		}
    }

    DEBUG("Video thread stopped");
}

void AsyncNdiSource::m_AudioS16Thread() {
    DEBUG("Audio S16 thread started");

    while (!m_shutdown_audio_s16_thread) {
		// Send!
		if(m_thread_using_audio_s16_frame) {
			NDIlib_util_send_send_audio_interleaved_16s(m_send_instance, &m_audio_s16_frame);
			m_thread_using_audio_s16_frame = false;
            DEBUG("Audio S16 frame sent");
		}
    }

    DEBUG("Audio S16 thread stopped");
}

} // namespace AV::Utils