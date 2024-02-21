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
#include <asm-generic/errno-base.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>

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

        if(m_pcodec_context) {
            avcodec_free_context(&m_pcodec_context);
        }

        if(m_pframe) {
            av_frame_free(&m_pframe);
        }

        if(m_ppacket) {
            av_packet_free(&m_ppacket);
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
                if(m_video_stream_index == -1) {
                    DEBUG("Stream selected");
                    m_video_stream_index = i; // Stream index
                    m_pcodec = pcodec; // Codec
                    m_pcodec_parameters = pcodec_parameters; // Codec information
                    m_video_total_frames = m_pformat_context->streams[i]->nb_frames;
                }  
            }
        }

        if(m_video_stream_index == -1)
            return AvErrorCode::StreamMissing;

        // Allocate space for ffmpeg codec context
        if(!(m_pcodec_context = avcodec_alloc_context3(m_pcodec)))
            return AvErrorCode::CodecContext;

        // Fill the new codec context with the selected codec parameters
        if(avcodec_parameters_to_context(m_pcodec_context, m_pcodec_parameters) < 0)
            return AvErrorCode::CodecParameters;

        // Finish codec initialization
        if(avcodec_open2(m_pcodec_context, m_pcodec, nullptr) < 0)
            return AvErrorCode::CodecOpen;

        // Allocate space for frame
        if(!(m_pframe = av_frame_alloc()))
            return AvErrorCode::FrameAlloc;

        // Allocate space for packet
        if(!(m_ppacket = av_packet_alloc()))
            return AvErrorCode::PacketAlloc;

        m_fileopened = true;

        return AvErrorCode::NoError;
    }

    // Read next packet
    AvErrorCode Decoder::ReadFrame() {        
        if(!m_fileopened)
            return AvErrorCode::FileNotOpened;

        // clear packet before processing next one
        av_packet_unref(m_ppacket);

        // Fill packet with stream data
        int ret = av_read_frame(m_pformat_context, m_ppacket);
        if(ret < 0) {
            if (ret != AVERROR_EOF) {
                ERROR("av_read_frame debug information --> 0x%04x,%d", ret, m_video_total_frames);
                return AvErrorCode::FrameRead;
            }

            return AvErrorCode::PacketsClaimed;
        }

        return AvErrorCode::NoError;
    }

    AvErrorCode Decoder::DecodeVideoPacket() {
        if(!IsCurrentFrameVideo())
                return AvErrorCode::NotVideoFrame;

        int response;

        if(!m_packet_in_decoder) {
            // Load packet in decoder
            response = avcodec_send_packet(m_pcodec_context, m_ppacket);
            if(response < 0)
                return AvErrorCode::PacketSend;

            m_packet_in_decoder = true;
        }

        // Load frame with decoded packet
        response = avcodec_receive_frame(m_pcodec_context, m_pframe);
        if(response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            m_packet_in_decoder = false;
            return AvErrorCode::PacketsClaimed;
        } else if (response < 0) {
            m_packet_in_decoder = false;
            return AvErrorCode::RecieveFrame;
        }

        return AvErrorCode::NoError;
    }

    void Decoder::GetPacketDimensions(int *resx, int *resy) {
        *resx = m_pcodec_context->width;
        *resy = m_pcodec_context->height;
    }

    void Decoder::GetPacketFrameRate(int *num, int *den) {
        *num = m_pcodec_context->framerate.num;
        *den = m_pcodec_context->framerate.den;
    }

    int Decoder::GetPacketStride() {
        return m_pframe->linesize[0];
    }

    int Decoder::GetFrameFormat() {
        return m_pframe->format;
    }

    uint8_t *Decoder::GetPacketData() {
        return m_pframe->data[0];
    }

    bool Decoder::IsCurrentFrameVideo() {
        if(m_ppacket->stream_index == m_video_stream_index)
            return true;

        return false;
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