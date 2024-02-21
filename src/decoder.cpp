/*
 * ndi-streamer
 * decoder.cpp
 *
 * 09-20-2024
 * Matthew Todd Geiger
 */

// Standard includes


// Local includes
#include "decoder.hpp"
#include "averror.hpp"
#include "macro.hpp"
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>

namespace AV {
    Decoder::Decoder(std::string &file) : m_file(file) {
        try {
            m_Initialize();
            m_OpenFile(file);
        } catch (AvErrorCode &err) {
            ERROR("%s", AvErrorStr(err).c_str());
        }
    }

    Decoder::Decoder() {
        try {
            m_Initialize();
        } catch(AvErrorCode &err) {
            ERROR("%s", AvErrorStr(err).c_str());
        }
    }

    Decoder::~Decoder() {
        if(m_pformat_context) {
            avformat_close_input(&m_pformat_context);
            avformat_free_context(m_pformat_context);
        }
    }

    // Public member functions
    AvErrorCode Decoder::OpenFile(std::string &file) {
        if (!m_Initialized)
            return AvErrorCode::DecoderNotInitialized;

        // Parse the av file header
        if(m_pformat_context && avformat_open_input(&m_pformat_context, file.c_str(), nullptr, nullptr) != 0)
            return AvErrorCode::FormatHeader;

        DEBUG("format %s, duration %ld us, bit_rate %ld", m_pformat_context->iformat->name, m_pformat_context->duration, m_pformat_context->bit_rate);

        // Get information for each av stream in the file
        if(avformat_find_stream_info(m_pformat_context, nullptr) < 0)
            return AvErrorCode::FormatStreamInfo;

        // Find AV stream
        for(int i = 0; i < m_pformat_context->nb_streams; i++) {
            // Get codec information
            AVCodecParameters *pcodec_parameters = m_pformat_context->streams[i]->codecpar;

            // Print some debug information (in debug build only)
            DEBUG("AVStream->time_base before open coded %d/%d", m_pformat_context->streams[i]->time_base.num, m_pformat_context->streams[i]->time_base.den);
            DEBUG("AVStream->r_frame_rate before open coded %d/%d", m_pformat_context->streams[i]->r_frame_rate.num, m_pformat_context->streams[i]->r_frame_rate.den);
            DEBUG("AVStream->start_time %" PRId64, m_pformat_context->streams[i]->start_time);
            DEBUG("AVStream->duration %" PRId64, m_pformat_context->streams[i]->duration);

            DEBUG("finding the proper decoder (CODEC)\n");

            const AVCodec *pcodec = avcodec_find_decoder(pcodec_parameters->codec_id);
            if(pcodec == nullptr) {
                DEBUG("Unsupported codec detected");
                continue;
            }

            // If the stream is audio/video, then select it as primary stream
            // This will not exit the loop, in the debug build we might want to
            // see debug information on the other streams
            if(pcodec_parameters->codec_type == AVMEDIA_TYPE_VIDEO) {
                DEBUG("Video codec detected");
                if(m_video_stream_index == -1) {
                    m_video_stream_index = i; // Stream index
                    m_pcodec = pcodec; // Codec
                    m_pcodec_parameters = pcodec_parameters; // Codec information
                }  
            }
        }

        if(m_video_stream_index == -1)
            return AvErrorCode::StreamMissing;

        return AvErrorCode::NoError;
    }

    // Private member functions
    void Decoder::m_OpenFile(std::string &file) {
        AvErrorCode err = OpenFile(file);
        if (err != AvErrorCode::NoError)
            throw err;
    }


    void Decoder::m_Initialize() {
        if((m_pformat_context = avformat_alloc_context()) == nullptr)
            throw AvErrorCode::FormatContextAlloc;

        m_Initialized = true;
    }

} // namespace AV