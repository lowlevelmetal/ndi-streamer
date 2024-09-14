/**
 * @file ndiavserver.cpp
 * @brief This file includes utilities for working with NDI.
 * @date 2024-09-08
 * @author Matthew Todd Geiger
 */

#include "ndiavserver.hpp"
#include "macro.hpp"

namespace AV::Utils {

double NdiAvServer::GetTargetFramerate() {
    return m_video_decoder->GetFPS();
}

AvException NdiAvServer::ProcessNextFrame() {
    static bool still_decoding_video = false;
    static bool still_decoding_audio = false;

    static AVPacket *frame = nullptr;
    static AvException frame_err;

    DEBUG("Decoder Status\n"
          "Video: %s\n"
          "Audio: %s\n",
          still_decoding_video ? "Decoding" : "Not Decoding",
          still_decoding_audio ? "Decoding" : "Not Decoding");

    // Grab frame
    if (!still_decoding_video && !still_decoding_audio) {
        auto [captured_frame, captured_frame_err] = m_demuxer->ReadFrame();

        frame = captured_frame;
        frame_err = captured_frame_err;

        if (frame_err.code() != (int)AvError::NOERROR) {
            return frame_err;
        }
    }

    // Process frame
    if (frame->stream_index == m_video_stream_index) {
        if (!still_decoding_video) {
            auto fill_err = m_video_decoder->FillDecoder(frame);
            if (fill_err.code() != (int)AvError::NOERROR) {
                return fill_err;
            }
            still_decoding_video = true;
        }

        auto [decoded_frame, decoded_frame_err] = m_video_decoder->Decode();
        if (decoded_frame_err.code() == (int)AvError::DECODEREXHAUSTED) {
            still_decoding_video = false;
            return AvException(AvError::NOERROR);
        } else if (decoded_frame_err.code() != (int)AvError::NOERROR) {
            return decoded_frame_err;
        }

        auto [encoded_frame, encoded_frame_err] = m_pixel_encoder->Encode(decoded_frame);
        if (encoded_frame_err.code() != (int)AvError::NOERROR) {
            return encoded_frame_err;
        }

        while(1) {
            auto send_err = m_ndi_source->LoadVideoFrame(encoded_frame, m_pixel_encoder->GetPixelFormat(), m_video_time_base, m_video_decoder->GetFrameRate());
            if (send_err.code() != (int)AvError::NOERROR) {
                if(send_err.code() == (int)AvError::BUFFERFULL) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                
                return send_err;
            }

            break;
        }

    } else if (frame->stream_index == m_audio_stream_index) {
        if (!still_decoding_audio) {
            auto fill_err = m_audio_decoder->FillDecoder(frame);
            if (fill_err.code() != (int)AvError::NOERROR) {
                return fill_err;
            }
            still_decoding_audio = true;
        }

        auto [decoded_frame, decoded_frame_err] = m_audio_decoder->Decode();
        if (decoded_frame_err.code() == (int)AvError::DECODEREXHAUSTED) {
            still_decoding_audio = false;
            return AvException(AvError::NOERROR);
        } else if (decoded_frame_err.code() != (int)AvError::NOERROR) {
            return decoded_frame_err;
        }

        auto [resampled_frame, resampled_frame_err] = m_audio_resampler->Resample(decoded_frame);
        if (resampled_frame_err.code() != (int)AvError::NOERROR) {
            return resampled_frame_err;
        }

        while(1) {
            auto send_err = m_ndi_source->LoadAudioFrame(resampled_frame, m_audio_time_base);
            if (send_err.code() != (int)AvError::NOERROR) {
                if (send_err.code() == (int)AvError::BUFFERFULL) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                
                return send_err;
            }

            break;
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
            m_video_time_base = stream->time_base;
            video_count++;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            DEBUG("Found audio stream");
            m_audio_stream_index = stream->index;
            m_audio_time_base = stream->time_base;
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
    m_audio_resampler_config.dstsamplerate = audio_codecpar->sample_rate;
    m_audio_resampler_config.srcchannellayout = audio_codecpar->ch_layout;
    m_audio_resampler_config.dstchannellayout = AV_CHANNEL_LAYOUT_STEREO;
    m_audio_resampler_config.srcsampleformat = (AVSampleFormat)audio_codecpar->format;
    m_audio_resampler_config.dstsampleformat = AV_SAMPLE_FMT_S16;

    auto [audio_resampler, audio_resampler_err] = AudioResampler::Create(m_audio_resampler_config);
    if (audio_resampler_err.code() != (int)AvError::NOERROR) {
        return (AvError)audio_resampler_err.code();
    }

    m_audio_resampler = std::move(audio_resampler);

    // Create the NDI source
    auto [ndi_source, ndi_source_err] = AsyncNdiSource::Create(m_ndi_name);
    if (ndi_source_err.code() != (int)AvError::NOERROR) {
        return (AvError)ndi_source_err.code();
    }

    m_ndi_source = std::move(ndi_source);

    m_ndi_source->Start();

    return AvError::NOERROR;
}

} // namespace AV::Utils