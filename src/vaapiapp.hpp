/**
 * @file vaapiapp.hpp
 * @brief This file includes the main application class.
 * @date 2024-09-19
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#pragma once

// Local includes
#include "averror.hpp"
#include "demuxer.hpp"
#include "asyncndisource.hpp"
#include "decoder.hpp"
#include "pixelencoder.hpp"
#include "audioresampler.hpp"
#include "frametimer.hpp"
#include "vaapidecoder.hpp"
#include "app.hpp"

extern "C" {
	#include <libavcodec/codec_par.h>
}

// Standard C++ includes
#include <string>
#include <memory>

class VAAPIApp;
using VAAPIAppResult = std::pair<std::shared_ptr<VAAPIApp>, AV::Utils::AvException>;

class VAAPIApp : public App {
private:
	VAAPIApp(const std::string &ndi_source_name, const std::string &video_file_path);
	AV::Utils::AvError _Initialize();

public:
	~VAAPIApp() = default;

	// Factory
	static VAAPIAppResult Create(const std::string &ndi_source_name, const std::string &video_file_path);

	AV::Utils::AvException Run();

private:
	std::string _ndi_source_name;
	std::string _video_file_path;
	std::shared_ptr<AV::Utils::Demuxer> _demuxer;
	std::shared_ptr<AV::Utils::Decoder> _audio_decoder;
	std::shared_ptr<AV::Utils::VAAPIDecoder> _vaapi_video_decoder;
	std::shared_ptr<AV::Utils::AudioResampler> _audio_resampler;
	std::shared_ptr<AV::Utils::AsyncNDISource> _ndi_source;
	
	AV::Utils::FrameTimer _frame_timer;

	int _video_stream_index = -1;
	int _audio_stream_index = -1;
};