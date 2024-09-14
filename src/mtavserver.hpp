/**
 * @file mtavserver.hpp
 * @brief Main header file for the mtavserver library
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#pragma once

#include "asyncndisource.hpp"
#include "demuxer.hpp"
#include "decoder.hpp"
#include "pixelencoder.hpp"
#include "audioresampler.hpp"

#include <string>
#include <memory>
#include <thread>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace AV::Utils {

class MtAvServer;
using MtAvServerResult = std::pair<std::unique_ptr<MtAvServer>, const AvException>;

class MtAvServer {
private:
	MtAvServer(const std::string &ndi_source_name, const std::string &media_path);
	AvError m_Initialize();

	// Thread funcs
	void m_VideoThread();
	void m_AudioThread();

	// Copy packet
	AvError m_CopyPacketToVideoQueue(AVPacket *src_packet);
	AvError m_CopyPacketToAudioQueue(AVPacket *src_packet);

public:
	~MtAvServer();

	// Factory method for better control over constructor error handling
	static MtAvServerResult Create(const std::string &ndi_source_name, const std::string &media_path);

	// Read next AVPacket
	AvException ProcessNextFrame();

	// Start the threads
	void start();

private:
	std::string m_ndi_source_name;
	std::string m_media_path;

	// Demuxer
	std::unique_ptr<Demuxer> m_demuxer;

	// Decoders
	std::unique_ptr<Decoder> m_video_decoder;
	std::unique_ptr<Decoder> m_audio_decoder;

	// Pixel Encoder
	PixelEncoderConfig m_pixel_encoder_config;
	std::unique_ptr<PixelEncoder> m_pixel_encoder;

	// Audio Resampler
	AudioResamplerConfig m_audio_resampler_config;
	std::unique_ptr<AudioResampler> m_audio_resampler;

	// NDI Source
	std::unique_ptr<AsyncNdiSource> m_ndi_source;

	// Stream Indexes
	int m_video_stream_index = 0;
	int m_audio_stream_index = 0;

	AVRational m_video_time_base;
	AVRational m_audio_time_base;

	// Shared buffers
	std::deque<AVPacket *> m_video_packets;
	std::deque<AVPacket *> m_audio_packets;

	// Threads
	std::thread m_video_thread;
	std::thread m_audio_thread;

	// Mutexes
	std::mutex m_video_buffer_mutex;
	std::mutex m_audio_buffer_mutex;

	// Shutdown flag for threads
	std::atomic<bool> m_shutdown = false;
};

} // namespace AV::Utils