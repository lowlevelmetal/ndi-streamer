/**
 * @file decoder.cpp
 * @brief This file includes utilities for decoding media files.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#include "decoder.hpp"
#include "macro.hpp"

namespace AV::Utils {

// Factory methods

/**
 * @brief Create a Decoder object
 * 
 * @param codec_id codec ID
 * @return DecoderResult
 */
DecoderResult Decoder::Create(AVCodecID codec_id) {
    DEBUG("Decoder factory called");
    AvException error(AvError::NOERROR);

    // Create a new decoder object, return nullopt if error
    try {
        return {std::unique_ptr<Decoder>(new Decoder(codec_id)), error};
    } catch (AvException e) {
        error = e;
        DEBUG("Decoder error: %s", error.what());
    }

    return {nullptr, error};
}

/**
 * @brief Create a Decoder object
 * 
 * @param config decoder configuration
 * @return DecoderResult
 */
DecoderResult Decoder::Create(const DecoderConfig &config) {
    DEBUG("Decoder factory called");
    AvException error(AvError::NOERROR);

    // Create a new decoder object, return nullopt if error
    try {
        return {std::unique_ptr<Decoder>(new Decoder(config)), error};
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
Decoder::Decoder(AVCodecID codec_id) {
    DEBUG("Constructing Decoder object");

    m_config.codec_id = codec_id;

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}

/**
 * @brief Construct a new Decoder:: Decoder object
 * 
 * @param config decoder configuration
 */
Decoder::Decoder(const DecoderConfig &config) : m_config(config) {
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

    if(m_codec) {
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

    const AVCodec *codec = avcodec_find_decoder(m_config.codec_id);
    if (!codec) {
        DEBUG("avcodec_find_decoder failed");
        return AvError::FINDDECODER;
    }

    m_codec = avcodec_alloc_context3(codec);
    if(!m_codec) {
        DEBUG("avcodec_alloc_context3 failed");
        return AvError::DECODERALLOC;
    }


    return AvError::NOERROR;
}

} // namespace AV::Utils