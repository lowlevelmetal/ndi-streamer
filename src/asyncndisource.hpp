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

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "ndi.hpp"
#include "averror.hpp"
#include "decoder.hpp"

namespace AV::Utils {

class AsyncNdiSource;
using AsyncNdiSourceResult = std::pair<std::unique_ptr<AsyncNdiSource>, const AvException>;
using AtmoicNdiSendInstance = std::atomic<NDIlib_send_instance_t>;


/**
 * @brief An asynchronous NDI source.
 * 
 * Ok so let me explain a little behind this..
 * Basically, I want something like the NdiSource class but I want the ability to decode/encode/etc... while
 * compressing and sending the previous frame.
 * 
 * Each set of () is a thread running a loop.
 * The (compress -> send) would be this class.
 * Which means that this class will manage two threads,
 * one for video, and one for audio.
 * 
 * Video
 * (demux -> decode -> encode) -> (compress -> send)
 * 
 * Audio
 * (demux -> deocde -> resample) -> (compress -> send)
 */
class AsyncNdiSource : public Ndi {
private:
	AsyncNdiSource(const std::string &ndi_name);


public:
	~AsyncNdiSource();

	static AsyncNdiSourceResult Create(const std::string &ndi_name);

	/**
	 * Just some notes here...
	 * When you are loading a frame of any kind, it will block until it is safe to load the frame.
	 */
	AvException LoadVideoFrame(AVFrame *frame, AVPixelFormat format, const AVRational &time_base, const CodecFrameRate &fps);
	AvException LoadAudioFrameS16(AVFrame *frame, const AVRational &time_base);

	/**
	 * @brief Get the last error that occurred.
	 * 
	 * @return AvException The last error that occurred.
	 */
	AvException LastError() const;

private:
	AvError m_Initialize();

	// Thread functions
	void m_VideoThread();
	void m_AudioS16Thread();

	AtmoicNdiSendInstance m_send_instance = nullptr;
	AtomicAvException m_last_error;
	std::string m_name;
	std::thread m_video_thread;
	std::thread m_audio_s16_thread;
	std::atomic<bool> m_shutdown_video_thread = false;
	std::atomic<bool> m_shutdown_audio_s16_thread = false;
	std::atomic<bool> m_thread_using_video_frame = false;
	std::atomic<bool> m_thread_using_audio_s16_frame = false;
	NDIlib_video_frame_v2_t m_video_frame{};
	NDIlib_audio_frame_interleaved_16s_t m_audio_s16_frame{};


};

} // namespace AV::Utils