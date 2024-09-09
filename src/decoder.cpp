/**
 * @file decoder.cpp
 * @brief This file includes utilities for decoding media files.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#include "decoder.hpp"
#include "macro.hpp"

namespace AV::Utils {

CodecFrameRate Decoder::GetFrameRate() {
    return {m_codec->framerate.num, m_codec->framerate.den};
}

/**
 * @brief Decode a packet
 *
 * @param pkt packet to decode
 * @return DecoderOutput
 */
DecoderOutput Decoder::Decode(AVPacket *pkt) {
    static int ret = 0;

    // Reset the frame to default settings every call
    av_frame_unref(m_last_frame);

    // If we need to fill the decoder, fill it
    if (ret == 0) {
        ret = avcodec_send_packet(m_codec, pkt);
        if (ret < 0) {
#ifdef _DEBUG
            char errbuf[AV_ERROR_MAX_STRING_SIZE];    // AV_ERROR_MAX_STRING_SIZE is defined in FFmpeg
            av_strerror(ret, errbuf, sizeof(errbuf)); // Use av_strerror to copy the error message to errbuf
            DEBUG("avcodec_send_packet failed: %s", errbuf);
#endif
            return {nullptr, AvException(AvError::SENDPACKET)};
        }
    }

    ret = avcodec_receive_frame(m_codec, m_last_frame);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            ret = 0;
            return {nullptr, AvException(AvError::DECODEREXHAUSTED)};
        }

        return {nullptr, AvException(AvError::RECIEVEFRAME)};
    }

    return {m_last_frame, AvException(AvError::NOERROR)};
}

// Factory methods

/**
 * @brief Create a Decoder object
 *
 * @param codecpar codec parameters
 * @return DecoderResult
 */
DecoderResult Decoder::Create(AVCodecParameters *codecpar) {
    DEBUG("Decoder factory called");
    AvException error(AvError::NOERROR);

    // Create a new decoder object, return nullopt if error
    try {
        return {std::unique_ptr<Decoder>(new Decoder(codecpar)), error};
    } catch (AvException e) {
        error = e;
        DEBUG("Decoder error: %s", error.what());
    }

    return {nullptr, error};
}

/**
 * @brief Construct a new Decoder:: Decoder object
 *
 * @param codec_id codec ID
 */
Decoder::Decoder(AVCodecParameters *codecpar) : m_codecpar(codecpar) {
    DEBUG("Constructing Decoder object");

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}


/**
 * @brief Destroy the Decoder:: Decoder object
 */
Decoder::~Decoder() {
    DEBUG("Destroying Decoder object");

    // Free the frame
    if (m_last_frame) {
        av_frame_free(&m_last_frame);
        DEBUG("av_frame_free called");
    }

    // Close and free the decoder context
    if (m_codec) {
        avcodec_close(m_codec);
        avcodec_free_context(&m_codec);
        DEBUG("avcodec_free_context called");
    }
}
/**
 * @brief Initialize the decoder
 *
 * @return AvError
 */
AvError Decoder::m_Initialize() {

    // Find the appropriate decoder
    const AVCodec *codec = avcodec_find_decoder(m_codecpar->codec_id);
    if (!codec) {
        DEBUG("avcodec_find_decoder failed");
        return AvError::FINDDECODER;
    }

    // Allocate the codec context
    m_codec = avcodec_alloc_context3(codec);
    if (!m_codec) {
        DEBUG("avcodec_alloc_context3 failed");
        return AvError::DECODERALLOC;
    }

    // Copy the codec parameters
    if (avcodec_parameters_to_context(m_codec, m_codecpar) < 0) {
        DEBUG("avcodec_parameters_to_context failed");
        return AvError::DECPARAMS;
    }

    // Open the decoder context
    if (avcodec_open2(m_codec, codec, nullptr) < 0) {
        DEBUG("avcodec_open2 failed");
        return AvError::DECPARAMS;
    }

    // Allocate frame
    m_last_frame = av_frame_alloc();
    if (!m_last_frame) {
        DEBUG("av_frame_alloc failed");
        return AvError::FRAMEALLOC;
    }

    return AvError::NOERROR;
}

} // namespace AV::Utils