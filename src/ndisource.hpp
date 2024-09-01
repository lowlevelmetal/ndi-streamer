/*
 * ndi-streamer
 * ndisource.hpp
 *
 * 09-21-2024
 * Matthew Todd Geiger
 */

#pragma once

// Standard includes
#include <string>

// Local includes
#include "ndi.hpp"
#include "ndierror.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

namespace AV {

    class NdiSource : public Ndi {
        public:
            NdiSource(std::string &ndi_source_name);
            ~NdiSource();

            NdiErrorCode SendVideoPacket(int pixel_format, int width, int height, int fr_num, int fr_den, int stride, uint8_t *video_data);
            NdiErrorCode SendAudioPacket(uint8_t *audio_data, int sample_rate, int channels, int samples, int stride, enum AVSampleFormat sample_fmt);

        private:
            NDIlib_send_instance_t m_pNDI_send;
            std::string m_ndi_source_name;
            bool m_initialized = false;

            void m_Initialize();

    };

} // namespace AV