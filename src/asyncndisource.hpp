/**
 * @file asyncndisource.hpp
 * @brief This file includes utilities for working with NDI asynchronously.
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#pragma once

#include <string>
#include <memory>
#include <thread>
#include <condition_variable>
#include <deque>

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "ndi.hpp"
#include "averror.hpp"
#include "decoder.hpp"
#include "conditionalsleep.hpp"

#define MAX_FRAMES_IN_BUFFER 10

namespace AV::Utils {

class AsyncNdiSource;
using AsyncNdiSourceResult = std::pair<std::unique_ptr<AsyncNdiSource>, const AvException>;

class AsyncNdiSource : public Ndi {
private:
	AsyncNdiSource(const std::string &ndi_source_name);
	AvError m_Initialize();

public:
	~AsyncNdiSource();

	// Factory method
	static AsyncNdiSourceResult Create(const std::string &ndi_source_name);

	AvException LoadVideoFrame(AVFrame *frame);
	AvException LoadAudioFrame(AVFrame *frame);

	void Start();

private:
	std::string m_ndi_source_name;

	// These are basically shared memory buffers filled with frames
	std::deque<AVFrame> m_audio_frames;
	std::deque<AVFrame> m_video_frames;

	std::thread m_audio_thread;
	std::thread m_video_thread;

	// Use these mutexes when accessing shared memory
	std::mutex m_audio_buffer_mutex;
	std::mutex m_video_buffer_mutex;

	// Use this mutexes when using NDI functions
	std::mutex m_ndi_mutex;

	std::atomic<bool> m_shutdown = false;

	// NDI Source
	NDIlib_send_instance_t m_ndi_send_instance = nullptr;
};

} // namespace AV::Utils