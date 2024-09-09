/**
 * @file ndiavserver.cpp
 * @brief This file includes utilities for working with NDI.
 * @date 2024-09-08
 * @author Matthew Todd Geiger
 */

#include "ndiavserver.hpp"
#include "macro.hpp"

namespace AV::Utils {

AvException NdiAvServer::ProcessNextFrame() {
    // Grab frame
    auto [video_frame, video_frame_err] = m_demuxer->ReadFrame();
    if (video_frame_err.code() != (int)AvError::NOERROR) {
        return video_frame_err;
    }

    // Check if video or audio frame
    if (video_frame->stream_index == m_video_stream_index) {
        // Decode video frame
        auto [decoded_video_frame, decoded_video_frame_err] = m_video_decoder->Decode(video_frame);
        if (decoded_video_frame_err.code() != (int)AvError::NOERROR) {
            return decoded_video_frame_err;
        }

        // Encode video frame
        auto [encoded_video_frame, encoded_video_frame_err] = m_pixel_encoder->Encode(decoded_video_frame);
        if (encoded_video_frame_err.code() != (int)AvError::NOERROR) {
            return encoded_video_frame_err;
        }

        // Send video frame
        auto send_video_err = m_ndi_source->SendVideoFrame(encoded_video_frame, m_video_decoder->GetFrameRate(), m_pixel_encoder->GetPixelFormat());
        if (send_video_err.code() != (int)AvError::NOERROR) {
            return send_video_err;
        }
    } else if (video_frame->stream_index == m_audio_stream_index) {
        // Decode audio frame
        auto [decoded_audio_frame, decoded_audio_frame_err] = m_audio_decoder->Decode(video_frame);
        if (decoded_audio_frame_err.code() != (int)AvError::NOERROR) {
            return decoded_audio_frame_err;
        }

        // Resample audio frame
        auto [resampled_audio_frame, resampled_audio_frame_err] = m_audio_resampler->Resample(decoded_audio_frame);
        if (resampled_audio_frame_err.code() != (int)AvError::NOERROR) {
            return resampled_audio_frame_err;
        }

        // Send audio frame
        auto send_audio_err = m_ndi_source->SendAudioFrame(resampled_audio_frame);
        if (send_audio_err.code() != (int)AvError::NOERROR) {
            return send_audio_err;
        }
    }

    return AvException(AvError::NOERROR);
}

NdiAvServerResult NdiAvServer::Create(const std::string &ndi_name, const std::string &media_path) {
    DEBUG("NdiAvServer factory called");

    AvException error(AvError::NOERROR);

    // Create a new NdiAvServer object, return null if error
    try {
        return {std::unique_ptr<NdiAvServer>(new NdiAvServer(ndi_name, media_path)), AvException(AvError::NOERROR)};
    } catch (const AvException e) {
        error = e;
        DEBUG("NdiAvServer error: %s", error.what());
    }

    return {nullptr, error};
}

NdiAvServer::NdiAvServer(const std::string &ndi_name, const std::string &media_path) : m_ndi_name(ndi_name), m_media_path(media_path) {
    DEBUG("Constructing NdiAvServer object");

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}

NdiAvServer::~NdiAvServer() {
    DEBUG("Destroying NdiAvServer object");
}

AvError NdiAvServer::m_Initialize() {
    // Create a new demuxer object
    auto [demuxer, demuxer_err] = Demuxer::Create(m_media_path);
    if (demuxer_err.code() != (int)AvError::NOERROR) {
        return (AvError)demuxer_err.code();
    }

    m_demuxer = std::move(demuxer);

    auto streams = m_demuxer->GetStreams();
    AVCodecParameters *video_codecpar = nullptr;
    AVCodecParameters *audio_codecpar = nullptr;
    uint video_count = 0, audio_count = 0;
    for (auto stream : streams) {
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            DEBUG("Found video stream");
            m_video_stream_index = stream->index;
            video_codecpar = stream->codecpar;
            video_count++;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            DEBUG("Found audio stream");
            m_audio_stream_index = stream->index;
            audio_codecpar = stream->codecpar;
            audio_count++;
        }
    }

    // Make sure there is only one video and one audio stream
    if (video_count != 1 || audio_count != 1) {
        return AvError::STREAMCOUNT;
    }

    auto [video_decoder, video_decoder_err] = Decoder::Create(video_codecpar);
    if (video_decoder_err.code() != (int)AvError::NOERROR) {
        return (AvError)video_decoder_err.code();
    }

    auto [audio_decoder, audio_decoder_err] = Decoder::Create(audio_codecpar);
    if (audio_decoder_err.code() != (int)AvError::NOERROR) {
        return (AvError)audio_decoder_err.code();
    }

    m_video_decoder = std::move(video_decoder);
    m_audio_decoder = std::move(audio_decoder);

    // Create the pixel encoder
    m_pixel_encoder_config.src_width = video_codecpar->width;
    m_pixel_encoder_config.src_height = video_codecpar->height;
    m_pixel_encoder_config.src_pix_fmt = (AVPixelFormat)video_codecpar->format;
    m_pixel_encoder_config.dst_width = video_codecpar->width;
    m_pixel_encoder_config.dst_height = video_codecpar->height;
    m_pixel_encoder_config.dst_pix_fmt = AV_PIX_FMT_UYVY422;

    auto [pixel_encoder, pixel_encoder_err] = PixelEncoder::Create(m_pixel_encoder_config);
    if (pixel_encoder_err.code() != (int)AvError::NOERROR) {
        return (AvError)pixel_encoder_err.code();
    }

    m_pixel_encoder = std::move(pixel_encoder);

    // Create the audio resampler
    m_audio_resampler_config.srcsamplerate = audio_codecpar->sample_rate;
    m_audio_resampler_config.dstsamplerate = 48000;
    m_audio_resampler_config.srcchannellayout = audio_codecpar->ch_layout;
    m_audio_resampler_config.dstchannellayout = AV_CHANNEL_LAYOUT_STEREO;
    m_audio_resampler_config.srcsampleformat = (AVSampleFormat)audio_codecpar->format;
    m_audio_resampler_config.dstsampleformat = AV_SAMPLE_FMT_FLT;

    auto [audio_resampler, audio_resampler_err] = AudioResampler::Create(m_audio_resampler_config);
    if (audio_resampler_err.code() != (int)AvError::NOERROR) {
        return (AvError)audio_resampler_err.code();
    }

    m_audio_resampler = std::move(audio_resampler);

    // Create the NDI source
    auto [ndi_source, ndi_source_err] = NdiSource::Create(m_ndi_name);
    if (ndi_source_err.code() != (int)AvError::NOERROR) {
        return (AvError)ndi_source_err.code();
    }

    m_ndi_source = std::move(ndi_source);

    return AvError::NOERROR;
}

} // namespace AV::Utils