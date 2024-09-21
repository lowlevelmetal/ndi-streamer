/**
 * @file vaapiapp.cpp
 * @brief This file includes the main application class.
 * @date 2024-09-19
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#include "vaapiapp.hpp"
#include "audioresampler.hpp"
#include "averror.hpp"
#include "macro.hpp"
#include "pixelencoder.hpp"

extern "C" {
	#include <libavcodec/codec_par.h>
	#include <libavutil/pixfmt.h>
}

AV::Utils::AvException VAAPIApp::Run() {
	bool packets_exhausted = false;
	bool packet_in_decoder = false;
	AVPacket *current_packet = nullptr;
	
	while(1) {

		// Get packet from demuxer
		if(!packet_in_decoder && !packets_exhausted) {
			auto [packet, packet_err] = _demuxer->ReadFrame();
			if(packet_err.code()) {
				if((AV::Utils::AvError)packet_err.code() == AV::Utils::AvError::DEMUXEREOF) {
					DEBUG("Packets exhausted");
					packets_exhausted = true;
				} else {
					ERROR("Failed to read packet: %s", packet_err.what());
					break;
				}
			}

			current_packet = packet;
		}

		if(packets_exhausted) {
			while(!_frame_timer.IsEmpty()) {
				DEBUG("Draining frames!");
				auto frame = _frame_timer.GetFrame();

				auto err = _ndi_source->SendFrame(frame);
				if(err.code()) {
					ERROR("Failed to send frame: %s", err.what());
					break;
				}

				av_frame_free(&frame);
			}

			break;
		}

		if(current_packet->stream_index == _video_stream_index) {
			if(!packet_in_decoder) {
				auto err = _vaapi_video_decoder->FillVAAPIDecoder(current_packet);
				if(err.code()) {
					ERROR("Failed to fill video decoder: %s", err.what());
					break;
				}

				packet_in_decoder = true;
			}

			auto [decoded_frame, decoder_err] = _vaapi_video_decoder->Decode();
			if(decoder_err.code()) {
				if((AV::Utils::AvError)decoder_err.code() == AV::Utils::AvError::DECODEREXHAUSTED) {
					DEBUG("Decoder exhausted");
					packet_in_decoder = false;
					continue;
				}

				ERROR("Failure in decoder: %s", decoder_err.what());
				break;
			}

			auto err = _frame_timer.AddFrame(decoded_frame);
			if(err.code()) {
				ERROR("Failed to add frame to timer: %s", err.what());
				break;
			}

		} else if(current_packet->stream_index == _audio_stream_index) {
			if(!packet_in_decoder) {
				auto err = _audio_decoder->FillDecoder(current_packet);
				if(err.code()) {
					ERROR("Failed to fill audio decoder: %s", err.what());
					break;
				}

				packet_in_decoder = true;
			}

			auto [decoded_frame, decoder_err] = _audio_decoder->Decode();
			if(decoder_err.code()) {
				if((AV::Utils::AvError)decoder_err.code() == AV::Utils::AvError::DECODEREXHAUSTED) {
					DEBUG("Decoder exhausted");
					packet_in_decoder = false;
					continue;
				}

				ERROR("Failure in decoder: %s", decoder_err.what());
				break;
			}

			auto [resampled_frame, resampled_frame_err] = _audio_resampler->Resample(decoded_frame);
			if(resampled_frame_err.code()) {
				ERROR("Failure in resampler: %s", resampled_frame_err.what());
				break;
			}

			auto err = _frame_timer.AddFrame(resampled_frame);
			if (err.code()) {
				ERROR("Failed to add frame to timer: %s", err.what());
				break;
			}
		}

		while(_frame_timer.IsHalf()) {
			DEBUG("Sending out frames");
			auto frame = _frame_timer.GetFrame();

			auto err = _ndi_source->SendFrame(frame);
			if(err.code()) {
				ERROR("Failed to send frame: %s", err.what());
				break;
			}

			av_frame_free(&frame);
		}
	}

	return AV::Utils::AvError::NOERROR;
}

VAAPIAppResult VAAPIApp::Create(const std::string &ndi_source_name, const std::string &video_file_path) {
	AV::Utils::AvException err;

	try {
		return {std::shared_ptr<VAAPIApp>(new VAAPIApp(ndi_source_name, video_file_path)), AV::Utils::AvError::NOERROR};
	} catch(AV::Utils::AvException e) {
		err = e;
		DEBUG("Error while creating app: %s", e.what());
	}

	return {nullptr, err};
}

VAAPIApp::VAAPIApp(const std::string &ndi_source_name, const std::string &video_file_path) : _ndi_source_name(ndi_source_name), _video_file_path(video_file_path) {
	auto err = _Initialize();
	if(err != AV::Utils::AvError::NOERROR) {
		throw err;
	}
}

AV::Utils::AvError VAAPIApp::_Initialize() {
	// Create the demuxer
	auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create(_video_file_path);
	if(demuxer_err.code() != (int)AV::Utils::AvError::NOERROR) {
		DEBUG("Demuxer error: %s", demuxer_err.what());
		return (AV::Utils::AvError)demuxer_err.code();
	}

	_demuxer = std::move(demuxer);

	// Get codecs and stream ids
	AVCodecParameters *video_cparam = nullptr, *audio_cparam = nullptr;
	int vcount = 0, acount = 0;
	AVRational video_time_base{};
	for (auto stream : _demuxer->GetStreamPointers()) {
		if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_cparam = stream->codecpar;
			_video_stream_index = stream->index;
			video_time_base = stream->time_base;
			vcount++;
		} else if(stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_cparam = stream->codecpar;
			_audio_stream_index = stream->index;
			acount++;
		}
	}

	if(acount != 1 && vcount != 1) {
		DEBUG("Invalid amount of streams");
		return AV::Utils::AvError::STREAMCOUNT;
	}

	// Create the video decoder
	auto [vaapi_video_decoder, vaapi_video_decoder_err] = AV::Utils::VAAPIDecoder::Create(video_cparam);
	if(vaapi_video_decoder_err.code() != (int)AV::Utils::AvError::NOERROR) {
		DEBUG("Video decoder error: %s", vaapi_video_decoder_err.what());
		return (AV::Utils::AvError)vaapi_video_decoder_err.code();
	}

	_vaapi_video_decoder = std::move(vaapi_video_decoder);
	
	// Create the audio decoder
	auto [audio_decoder, audio_decoder_err] = AV::Utils::Decoder::Create(audio_cparam);
	if(audio_decoder_err.code() != (int)AV::Utils::AvError::NOERROR) {
		DEBUG("Audio decoder error: %s", audio_decoder_err.what());
		return (AV::Utils::AvError)audio_decoder_err.code();
	}

	_audio_decoder = std::move(audio_decoder);

	// Create the audio resampler
	AV::Utils::AudioResamplerConfig audio_resampler_config{};
	audio_resampler_config.srcsamplerate = audio_cparam->sample_rate;
	audio_resampler_config.dstsamplerate = audio_cparam->sample_rate;
	audio_resampler_config.srcchannellayout = audio_cparam->ch_layout;
	audio_resampler_config.dstchannellayout = AV_CHANNEL_LAYOUT_STEREO;
	audio_resampler_config.srcsampleformat = (AVSampleFormat)audio_cparam->format;
	audio_resampler_config.dstsampleformat = AV_SAMPLE_FMT_S16;

	auto [audio_resampler, audio_resampler_err] = AV::Utils::AudioResampler::Create(audio_resampler_config);
	if(audio_resampler_err.code()) {
		DEBUG("Audio resampler error: %s", audio_resampler_err.what());
		return (AV::Utils::AvError)audio_resampler_err.code();
	}

	_audio_resampler = std::move(audio_resampler);

	// Create NDI Source
	auto [ndi_source, ndi_source_err] = AV::Utils::AsyncNDISource::Create(_ndi_source_name, video_cparam->framerate);
	if(ndi_source_err.code()) {
		DEBUG("NDI Source error: %s", ndi_source_err.what());
		return (AV::Utils::AvError)ndi_source_err.code();
	}
	
	_ndi_source = std::move(ndi_source);

	return AV::Utils::AvError::NOERROR;
}