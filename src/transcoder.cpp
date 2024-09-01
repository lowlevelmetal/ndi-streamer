/*
 * ndi-streamer
 * transcoder.cpp
 *
 * 09-01-2024
 * Matthew Todd Geiger
 */

// local includes
#include "transcoder.hpp"
#include "macro.hpp"

namespace AV::Transcode {

DynamicTranscoder::DynamicTranscoder(TranscoderConfig &config) : m_config(config) {
    DEBUG("Initializing transcoder");

    auto err = m_InitializeTranscoder();
    if (err != TranscoderErrorCode::NoError) {
        m_last_error = err;
        return;
    }

    m_initialized = true;
}

DynamicTranscoder::~DynamicTranscoder() {
    // Close input first
    if (m_pformat_context) {
        avformat_close_input(&m_pformat_context);
    }

    // Free the codec contexts
    if (m_pvideo_codec_context) {
        avcodec_free_context(&m_pvideo_codec_context);
    }

    if (m_paudio_codec_context) {
        avcodec_free_context(&m_paudio_codec_context);
    }

    // Free the format context
    if (m_pformat_context) {
        avformat_free_context(m_pformat_context);
    }
}

TranscoderErrorCode DynamicTranscoder::GetLastError() {
    return m_last_error;
}

bool DynamicTranscoder::IsInitialized() {
    return m_initialized;
}

// Here we need to setup the ffmpeg context and look for video and audio
// streams. We will also need to setup the codec context for each stream
TranscoderErrorCode DynamicTranscoder::m_InitializeTranscoder() {
    // An input file is required
    if (m_config.input_file.empty()) {
        ERROR("No input file specified");
        return TranscoderErrorCode::InvalidInputFile;
    }

    // Check if we have an output file
    if (!m_config.output_file.empty())
        m_file_output = true;

    // Create the FFMPEG context with the input file
    if (avformat_open_input(&m_pformat_context, m_config.input_file.c_str(), nullptr, nullptr) != 0) {
        ERROR("Could not open input file: %s", m_config.input_file.c_str());
        return TranscoderErrorCode::InvalidInputFile;
    }

    // Get the stream information
    if (avformat_find_stream_info(m_pformat_context, nullptr) < 0) {
        ERROR("Could not find stream information");
        return TranscoderErrorCode::InvalidInputFile;
    }

    // Find the video and audio streams
    const AVCodec *video_codec = nullptr;
    const AVCodec *audio_codec = nullptr;

    for (int i = 0; i < m_pformat_context->nb_streams; i++) {
        AVStream *stream = m_pformat_context->streams[i];
        AVCodecParameters *codec_parameters = stream->codecpar;

        // We found a video stream
        if (codec_parameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            // Record the stream index for later
            m_video_stream_index = i;

            // What video codec are we using?
            video_codec = avcodec_find_decoder(codec_parameters->codec_id);
            if (!video_codec) { // Unsupported video codec
                ERROR("Unsupported video codec detected");
                return TranscoderErrorCode::UnsupportedVideoCodec;
            }

            // Create the codec context
            m_pvideo_codec_context = avcodec_alloc_context3(video_codec);
            if (!m_pvideo_codec_context) {
                ERROR("Could not allocate video codec context");
                return TranscoderErrorCode::UnsupportedVideoCodec;
            }

            // Copy the codec parameters to the codec context
            if (avcodec_parameters_to_context(m_pvideo_codec_context, codec_parameters) < 0) {
                ERROR("Could not copy codec parameters to codec context");
                return TranscoderErrorCode::UnsupportedVideoCodec;
            }

            // open the codec
            if (avcodec_open2(m_pvideo_codec_context, video_codec, nullptr) < 0) {
                ERROR("Could not open video codec");
                return TranscoderErrorCode::UnsupportedVideoCodec;
            }
        } else if (codec_parameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            // Record the stream index for later
            m_audio_stream_index = i;

            // What audio codec are we using?
            audio_codec = avcodec_find_decoder(codec_parameters->codec_id);
            if (!audio_codec) {
                ERROR("Unsupported audio codec detected");
                return TranscoderErrorCode::UnsupportedAudioCodec;
            }

            // Create the codec context
            m_paudio_codec_context = avcodec_alloc_context3(audio_codec);
            if (!m_paudio_codec_context) {
                ERROR("Could not allocate audio codec context");
                return TranscoderErrorCode::UnsupportedAudioCodec;
            }

            // Copy the codec parameters to the codec context
            if (avcodec_parameters_to_context(m_paudio_codec_context, codec_parameters) < 0) {
                ERROR("Could not copy codec parameters to codec context");
                return TranscoderErrorCode::UnsupportedAudioCodec;
            }

            // open the codec
            if (avcodec_open2(m_paudio_codec_context, audio_codec, nullptr) < 0) {
                ERROR("Could not open audio codec");
                return TranscoderErrorCode::UnsupportedAudioCodec;
            }
        }
    }

    if (m_video_stream_index == -1 && m_audio_stream_index == -1) {
        ERROR("No video or audio streams found");
        return TranscoderErrorCode::InvalidInputFile;
    }

#ifdef _DEBUG
    DEBUG("\tVideo Stream Index: %d\n"
          "\tAudio Stream Index: %d\n",
          m_video_stream_index, m_audio_stream_index);

	if (m_video_stream_index != -1) {
		DEBUG("\tVideo Codec: %s\n"
			  "\tVideo Width: %d\n"
			  "\tVideo Height: %d\n"
              "\tVideo Pixel Format: %s\n",
			  m_pvideo_codec_context->codec->long_name,
			  m_pvideo_codec_context->width,
			  m_pvideo_codec_context->height,
              av_get_pix_fmt_name(m_pvideo_codec_context->pix_fmt));
	}

	if (m_audio_stream_index != -1) {
		DEBUG("\tAudio Codec: %s\n"
			  "\tAudio Sample Rate: %d\n"
			  "\tAudio Channels: %d\n"
              "\tAudio Sample Format: %s\n",
			  m_paudio_codec_context->codec->long_name,
			  m_paudio_codec_context->sample_rate,
			  m_paudio_codec_context->channels,
              av_get_sample_fmt_name(m_paudio_codec_context->sample_fmt));
	}

#endif

    return TranscoderErrorCode::NoError;
}

} // namespace av::transcode