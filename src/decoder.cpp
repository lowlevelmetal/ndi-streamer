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
DecoderResult Decoder::Create(const std::string &path) {
    AvException error(AvError::NOERROR);

    // Create a new decoder object, return nullopt if error
    try {
        return {std::unique_ptr<Decoder>(new Decoder(path)), error};
    } catch (AvException e) {
        error = e;
        DEBUG("Decoder error: %s", error.what());
    }

    return {nullptr, error};
}

DecoderResult Decoder::Create(const DecoderConfig &config) {
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

Decoder::Decoder(const std::string &path) {
    m_config.demuxer_config.path = path;
}

Decoder::Decoder(const DecoderConfig &config) : m_config(config) {

}

AvError Decoder::m_Initialize() {
    
}

} // namespace AV::Utils