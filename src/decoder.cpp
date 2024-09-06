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
DecoderResult Decoder::Create(AVCodecID codec_id, AVCodecParameters *codecpar) {
    DEBUG("Decoder factory called");
    AvException error(AvError::NOERROR);

    // Create a new decoder object, return nullopt if error
    try {
        return {std::unique_ptr<Decoder>(new Decoder(codec_id, codecpar)), error};
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
Decoder::Decoder(AVCodecID codec_id, AVCodecParameters *codecpar) {
    DEBUG("Constructing Decoder object");

    m_config.codec_id = codec_id;
    m_config.codecpar = codecpar;

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

    // Close and free the decoder context
    if(m_codec) {
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
    const AVCodec *codec = avcodec_find_decoder(m_config.codec_id);
    if (!codec) {
        DEBUG("avcodec_find_decoder failed");
        return AvError::FINDDECODER;
    }

    // Allocate the codec context
    m_codec = avcodec_alloc_context3(codec);
    if(!m_codec) {
        DEBUG("avcodec_alloc_context3 failed");
        return AvError::DECODERALLOC;
    }

    // Copy the codec parameters
    if(avcodec_parameters_to_context(m_codec, m_config.codecpar) < 0) {
        DEBUG("avcodec_parameters_to_context failed");
        return AvError::DECPARAMS;
    }

    // Open the decoder context
    if(avcodec_open2(m_codec, codec, nullptr) < 0) {
        DEBUG("avcodec_open2 failed");
        return AvError::DECPARAMS;
    }

    return AvError::NOERROR;
}

} // namespace AV::Utils