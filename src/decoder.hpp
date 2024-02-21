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
            bool IsCurrentFrameVideo();
            int GetFrameFormat();

        private:
            std::string m_file;
            bool m_Initialized = false;
            bool m_fileopened = false;
            bool m_packet_in_decoder = false;
            AVFormatContext *m_pformat_context = nullptr;
            const AVCodec *m_pcodec = nullptr;
            AVCodecParameters *m_pcodec_parameters = nullptr;
            AVCodecContext *m_pcodec_context = nullptr;
            AVFrame *m_pframe = nullptr;
            AVPacket *m_ppacket = nullptr;
            int m_video_stream_index = -1;
            int m_video_total_frames = -1;

            void m_Initialize();
            void m_OpenFile(std::string &file);
    };

} // namespace AV