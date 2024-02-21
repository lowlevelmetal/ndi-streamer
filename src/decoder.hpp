/*
 * ndi-streamer
 * decoder.hpp
 *
 * 09-20-2024
 * Matthew Todd Geiger
 */

#pragma once

// Standard includes
#include <libavcodec/codec_par.h>
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
}

namespace AV {

    class Decoder {
        public:
            Decoder(std::string &file);
            Decoder();
            ~Decoder();

            AvErrorCode OpenFile(std::string &file);

        private:
            std::string m_file;
            bool m_Initialized = false;
            AVFormatContext *m_pformat_context = nullptr;
            const AVCodec *m_pcodec = nullptr;
            AVCodecParameters *m_pcodec_parameters = nullptr;
            int m_video_stream_index = -1;

            void m_Initialize();
            void m_OpenFile(std::string &file);
    };

} // namespace AV