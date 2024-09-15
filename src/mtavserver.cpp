/**
 * @file mtavserver.cpp
 * @brief Main source file for the mtavserver library
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#include "mtavserver.hpp"
#include "macro.hpp"

#define MAX_QUEUE 50

namespace AV::Utils {

MtAvServer::MtAvServer(const std::string &ndi_source_name, const std::string &media_path) : m_ndi_source_name(ndi_source_name), m_media_path(media_path) {
	DEBUG("MtAvServer constructor called");
   
    auto err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw AvException(err);
    }
}

MtAvServer::~MtAvServer() {
	DEBUG("MtAvServer destructor called");

	// Stop the threads
	m_shutdown = true;

	// Just incase the threads are sleeping, wake them up
	m_audio_cv.notify_one();
	m_video_cv.notify_one();

	if (m_audio_thread.joinable()) {
		m_audio_thread.join();
		DEBUG("Audio thread joined");
	}

	if (m_video_thread.joinable()) {
		m_video_thread.join();
		DEBUG("Video thread joined");
	}
}

MtAvServerResult MtAvServer::Create(const std::string &ndi_source_name, const std::string &media_path) {
	AvException err;

	try {
		return {std::unique_ptr<MtAvServer>(new MtAvServer(ndi_source_name, media_path)), AvException(AvError::NOERROR)};
	} catch (const AvException &e) {
		err = e;
		DEBUG("Error creating MtAvServer: %s", e.what());
	}

	return {nullptr, err};
}

AvException MtAvServer::ProcessNextFrame() {
	// Read next frame
	auto [packet, packet_err] = m_demuxer->ReadFrame();
	if (packet_err.code() != (int)AvError::NOERROR) {
		return packet_err;
	}

	// Copy packet to queue
	if (packet->stream_index == m_video_stream_index) {
		while(1) {
			auto copy_err = m_CopyPacketToVideoQueue(packet);
			if (copy_err != AvError::NOERROR) {
				if (copy_err == AvError::BUFFERFULL) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}

				return copy_err;
			}

			break;
		}

	} else if (packet->stream_index == m_audio_stream_index) {
		while(1) {
			auto copy_err = m_CopyPacketToAudioQueue(packet);
			if (copy_err != AvError::NOERROR) {
				if (copy_err == AvError::BUFFERFULL) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}

				return copy_err;
			}

			break;
		}
	}

	return AvException(AvError::NOERROR);
}

void MtAvServer::start() {
	// Start the ndi source threads
	m_ndi_source->Start();

	// Start the video thread
	m_video_thread = std::thread(&MtAvServer::m_VideoThread, this);

	// Start the audio thread
	m_audio_thread = std::thread(&MtAvServer::m_AudioThread, this);
}

AvError MtAvServer::m_CopyPacketToVideoQueue(AVPacket *src_packet) {
	AvError err = AvError::NOERROR;
	AVPacket *packet = nullptr;


	// Check if queue is filled
	{
		std::unique_lock<std::mutex> lock(m_video_buffer_mutex);
		if (m_video_packets.size() >= MAX_QUEUE) {
			err = AvError::BUFFERFULL;
			goto END;
		}
	}

	packet = av_packet_alloc();
	if (!packet) {
		return AvError::PACKETALLOC;
	}

	if (av_packet_ref(packet, src_packet) < 0) {
		av_packet_free(&packet);
		return AvError::PACKETREF;
	}

	{
		std::unique_lock<std::mutex> lock(m_video_buffer_mutex);
		m_video_packets.push_back(packet);
	}

END:
	if(m_video_thread_sleeping) {
		m_video_cv.notify_one();
		m_video_thread_sleeping = false;
	}

	return err;
}

AvError MtAvServer::m_CopyPacketToAudioQueue(AVPacket *src_packet) {
	AvError err = AvError::NOERROR;
	AVPacket *packet = nullptr;

	// Check if queue is filled
	{
		std::unique_lock<std::mutex> lock(m_audio_buffer_mutex);
		if (m_audio_packets.size() >= MAX_QUEUE) {
			err = AvError::BUFFERFULL;
			goto END;
		}
	}

	packet = av_packet_alloc();
	if (!packet) {
		return AvError::PACKETALLOC;
	}

	if (av_packet_ref(packet, src_packet) < 0) {
		av_packet_free(&packet);
		return AvError::PACKETREF;
	}

	{
		std::unique_lock<std::mutex> lock(m_audio_buffer_mutex);
		m_audio_packets.push_back(packet);
	}

END:
	if(m_audio_thread_sleeping) {
		m_audio_cv.notify_one();
		m_audio_thread_sleeping = false;
	}

	return err;
}

void MtAvServer::m_VideoThread() {
	bool still_decoding = false;
	AVPacket *packet = nullptr;

	while(!m_shutdown || [this]() { std::unique_lock<std::mutex> lock(m_video_buffer_mutex); return !m_video_packets.empty(); }()) {
		// Copy packet
		if (!still_decoding) {
			packet = av_packet_alloc();
			if (!packet) {
				DEBUG("Failed to allocate packet");
				break;
			}


			std::unique_lock<std::mutex> lock(m_video_buffer_mutex);

			// If queue is empty, unlock and continue
			if (m_video_packets.empty()) {
				lock.unlock();
				av_packet_free(&packet);

				m_video_thread_sleeping = true;
				std::unique_lock<std::mutex> lock(m_video_sleep_mutex);
				m_video_cv.wait_for(lock, std::chrono::milliseconds(100));

				continue;
			}

			// Copy packet
			auto packet = m_video_packets.front();

			if (av_packet_ref(packet, m_video_packets.front()) < 0) {
				DEBUG("Failed to copy packet");
				av_packet_free(&packet);
				break;
			}

			m_video_packets.pop_front();

			lock.unlock();

			// Fill decoder
			auto fill_err = m_video_decoder->FillDecoder(packet);
			if (fill_err.code() != (int)AvError::NOERROR) {
				DEBUG("Failed to fill decoder");
				av_packet_free(&packet);
				break;
			}

			still_decoding = true;
		}

		// Decode
		auto [decoded_frame, decoded_frame_err] = m_video_decoder->Decode();
		if (decoded_frame_err.code() == (int)AvError::DECODEREXHAUSTED) {
			still_decoding = false;
			av_packet_free(&packet);
			continue;
		} else if (decoded_frame_err.code() != (int)AvError::NOERROR) {
			DEBUG("Failed to decode frame");
			av_packet_free(&packet);
			break;
		}


		// Encode
		auto [encoded_frame, encoded_frame_err] = m_pixel_encoder->Encode(decoded_frame);
		if (encoded_frame_err.code() != (int)AvError::NOERROR) {
			DEBUG("Failed to encode frame");
			av_packet_free(&packet);
			break;
		}

		// Send
		while(1) {
			auto send_err = m_ndi_source->LoadVideoFrame(encoded_frame, m_pixel_encoder_config.dst_pix_fmt, m_video_time_base, m_video_decoder->GetFrameRate());
			if (send_err.code() != (int)AvError::NOERROR) {
				if (send_err.code() == (int)AvError::BUFFERFULL) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}

				DEBUG("Failed to send video frame");
				av_packet_free(&packet);
			}

			break;
		}
	}

	DEBUG("Video thread shutdown");
}

void MtAvServer::m_AudioThread() {
	bool still_decoding = false;
	AVPacket *packet = nullptr;

	while(!m_shutdown || [this]() { std::unique_lock<std::mutex> lock(m_audio_buffer_mutex); return !m_audio_packets.empty(); }()) {
		// Copy packet
		if (!still_decoding) {
			packet = av_packet_alloc();
			if (!packet) {
				DEBUG("Failed to allocate packet");
				break;
			}

			std::unique_lock<std::mutex> lock(m_audio_buffer_mutex);

			// If queue is empty, unlock and continue
			if (m_audio_packets.empty()) {
				lock.unlock();
				av_packet_free(&packet);

				m_audio_thread_sleeping = true;
				std::unique_lock<std::mutex> lock(m_audio_sleep_mutex);
				m_audio_cv.wait_for(lock, std::chrono::milliseconds(100));

				continue;
			}

			// Copy packet
			auto packet = m_audio_packets.front();

			if (av_packet_ref(packet, m_audio_packets.front()) < 0) {
				DEBUG("Failed to copy packet");
				av_packet_free(&packet);
				break;
			}

			m_audio_packets.pop_front();

			lock.unlock();

			// Fill decoder
			auto fill_err = m_audio_decoder->FillDecoder(packet);
			if (fill_err.code() != (int)AvError::NOERROR) {
				DEBUG("Failed to fill decoder");
				av_packet_free(&packet);
				break;
			}

			still_decoding = true;
		}

		// Decode
		auto [decoded_frame, decoded_frame_err] = m_audio_decoder->Decode();
		if (decoded_frame_err.code() == (int)AvError::DECODEREXHAUSTED) {
			still_decoding = false;
			av_packet_free(&packet);
			continue;
		} else if (decoded_frame_err.code() != (int)AvError::NOERROR) {
			DEBUG("Failed to decode frame");
			av_packet_free(&packet);
			break;
		}

		// Resample
		auto [resampled_frame, resampled_frame_err] = m_audio_resampler->Resample(decoded_frame);
		if (resampled_frame_err.code() != (int)AvError::NOERROR) {
			DEBUG("Failed to resample frame");
			av_packet_free(&packet);
			break;
		}

		// Send
		while(1) {
			auto send_err = m_ndi_source->LoadAudioFrame(resampled_frame, m_audio_time_base);
			if (send_err.code() != (int)AvError::NOERROR) {
				if (send_err.code() == (int)AvError::BUFFERFULL) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}

				DEBUG("Failed to send audio frame");
				av_packet_free(&packet);
			}

			break;
		}



	}

	DEBUG("Audio thread shutdown");
}

AvError MtAvServer::m_Initialize() {
	// Create the demuxer
	auto [demuxer, demuxer_err] = Demuxer::Create(m_media_path);
	if (demuxer_err.code() != (int)AvError::NOERROR) {
		return (AvError)demuxer_err.code();
	}

	m_demuxer = std::move(demuxer);

	// Locate streams
	auto streams = m_demuxer->GetStreams();
	AVCodecParameters *video_codec_params = nullptr;
	AVCodecParameters *audio_codec_params = nullptr;
	uint video_count = 0, audio_count = 0;
	for (auto &stream : streams) {
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_codec_params = stream->codecpar;
			m_video_stream_index = stream->index;
			m_video_time_base = stream->time_base;
			video_count++;
		} else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_codec_params = stream->codecpar;
			m_audio_stream_index = stream->index;
			m_audio_time_base = stream->time_base;
			audio_count++;
		}
	}

	// Make sure there is only one video and one audio stream
	if (video_count != 1 || audio_count != 1) {
		return AvError::STREAMCOUNT;
	}

	// Create the video decoder
	auto [video_decoder, video_decoder_err] = Decoder::Create(video_codec_params);
	if (video_decoder_err.code() != (int)AvError::NOERROR) {
		return (AvError)video_decoder_err.code();
	}

	m_video_decoder = std::move(video_decoder);

	// Create the audio decoder
	auto [audio_decoder, audio_decoder_err] = Decoder::Create(audio_codec_params);
	if (audio_decoder_err.code() != (int)AvError::NOERROR) {
		return (AvError)audio_decoder_err.code();
	}

	m_audio_decoder = std::move(audio_decoder);

	// Create the pixel encoder
	m_pixel_encoder_config.src_width = video_codec_params->width;
	m_pixel_encoder_config.src_height = video_codec_params->height;
	m_pixel_encoder_config.src_pix_fmt = (AVPixelFormat)video_codec_params->format;
	m_pixel_encoder_config.dst_width = video_codec_params->width;
	m_pixel_encoder_config.dst_height = video_codec_params->height;
	m_pixel_encoder_config.dst_pix_fmt = AV_PIX_FMT_UYVY422;

	auto [pixel_encoder, pixel_encoder_err] = PixelEncoder::Create(m_pixel_encoder_config);
	if (pixel_encoder_err.code() != (int)AvError::NOERROR) {
		return (AvError)pixel_encoder_err.code();
	}

	m_pixel_encoder = std::move(pixel_encoder);

	// Create the audio resampler
	m_audio_resampler_config.srcsamplerate = audio_codec_params->sample_rate;
	m_audio_resampler_config.dstsamplerate = audio_codec_params->sample_rate;
	m_audio_resampler_config.srcchannellayout = audio_codec_params->ch_layout;
	m_audio_resampler_config.dstchannellayout = AV_CHANNEL_LAYOUT_STEREO;
	m_audio_resampler_config.srcsampleformat = (AVSampleFormat)audio_codec_params->format;
	m_audio_resampler_config.dstsampleformat = AV_SAMPLE_FMT_S16;

	auto [audio_resampler, audio_resampler_err] = AudioResampler::Create(m_audio_resampler_config);
	if (audio_resampler_err.code() != (int)AvError::NOERROR) {
		return (AvError)audio_resampler_err.code();
	}

	m_audio_resampler = std::move(audio_resampler);

	// Create the NDI source
	auto [ndi_source, ndi_source_err] = AsyncNdiSource::Create(m_ndi_source_name);
	if (ndi_source_err.code() != (int)AvError::NOERROR) {
		return (AvError)ndi_source_err.code();
	}

	m_ndi_source = std::move(ndi_source);

	return AvError::NOERROR;
}

} // namespace AV::Utils