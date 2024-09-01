/*
 * ndi-streamer
 * decoder.hpp
 *
 * 09-20-2024
 * Matthew Todd Geiger
 */

#pragma once

// Standard includes
#include <string>

// Local includes
#include "macro.hpp"
#include "averror.hpp"

// FFMPEG Includes
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/time.h>
    #include <libswscale/swscale.h>
    #include <libavcodec/codec.h>
    #include <libavcodec/codec_par.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
    #include <libavcodec/packet.h>
    #include <libswresample/swresample.h>
}

namespace AV {

    class Decoder {
        public:
            Decoder(std::string &file);
            Decoder();
            ~Decoder();

            AvErrorCode OpenFile(std::string &file);
            AvErrorCode ReadFrame();
            AvErrorCode DecodeVideoPacket();
            AvErrorCode DecodeAudioPacket();
            bool IsCurrentFrameVideo();
            bool IsCurrentFrameAudio();
            int GetFrameFormat();
            void GetPacketDimensions(int *resx, int *resy);
            void GetPacketFrameRate(int *num, int *den);
            int GetPacketStride();
            int GetUVYVPacketStride();
            int GetSampleRate();
            int GetSampleCount();
            int GetBytesPerSample();
            int GetAudioChannels();
            uint8_t *GetPacketData();
            uint8_t *ConvertToUVYV();
            enum AVSampleFormat GetAudioSampleFormat();

        private:
            std::string m_file;
            bool m_Initialized = false;
            bool m_fileopened = false;
            bool m_video_packet_in_decoder = false;
            bool m_audio_packet_in_decoder = false;
            AVFormatContext *m_pformat_context = nullptr;
            const AVCodec *m_video_codec = nullptr;
            const AVCodec *m_audio_codec = nullptr;
            AVCodecParameters *m_vcodec_parameters = nullptr;
            AVCodecParameters *m_acodec_parameters = nullptr;
            AVCodecContext *m_vcodec_context = nullptr;
            AVCodecContext *m_acodec_context = nullptr;
            AVFrame *m_pframe = nullptr;
            AVFrame *m_puyvy_frame = nullptr;
            AVPacket *m_ppacket = nullptr;
            int m_video_stream_index = -1;
            int m_audio_stream_index = -1;
            int m_video_total_frames = -1;
            int m_audio_total_frames = -1;

            void m_Initialize();
            void m_OpenFile(std::string &file);
    };

} // namespace AV