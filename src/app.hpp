/**
 * @file app.hpp
 * @brief This file includes the main application class.
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#pragma once

// Local includes
#include "averror.hpp"
#include "demuxer.hpp"
#include "ndisource.hpp"
#include "simplefilter.hpp"
#include "decoder.hpp"
#include "pixelencoder.hpp"
#include "audioresampler.hpp"
#include "frametimer.hpp"
#include "cudafilter.hpp"
#include "cudadecoder.hpp"
#include "vaapidecoder.hpp"

extern "C" {
	#include <libavcodec/codec_par.h>
}

// Standard C++ includes
#include <string>
#include <memory>

class App;
using AppResult = std::pair<std::shared_ptr<App>, AV::Utils::AvException>;

class App {
private:
	App(const std::string &ndi_source_name, const std::string &video_file_path);
	AV::Utils::AvError _Initialize();

public:
	~App() = default;

	// Factory
	static AppResult Create(const std::string &ndi_source_name, const std::string &video_file_path);

	AV::Utils::AvException Run();

private:
	std::string _ndi_source_name;
	std::string _video_file_path;
	std::shared_ptr<AV::Utils::Demuxer> _demuxer;
	std::shared_ptr<AV::Utils::CudaFilter> _cuda_filter;
	std::shared_ptr<AV::Utils::Decoder> _audio_decoder;
	std::shared_ptr<AV::Utils::VAAPIDecoder> _vaapi_video_decoder;
	std::shared_ptr<AV::Utils::Decoder> _video_decoder;
	std::shared_ptr<AV::Utils::AudioResampler> _audio_resampler;
	std::shared_ptr<AV::Utils::NDISource> _ndi_source;
	
	AV::Utils::FrameTimer _frame_timer;

	int _video_stream_index = -1;
	int _audio_stream_index = -1;
};