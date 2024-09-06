/**
 * @file decoder.hpp
 * @brief This file includes utilities for decoding media files.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

#include <string>
#include <memory>

#include "averror.hpp"
#include "demuxer.hpp"

namespace AV::Utils {

class Decoder;
using DecoderResult = std::pair<std::unique_ptr<Decoder>, const AvException>;

typedef struct DecoderConfig {
    demuxerconfig demuxer_config{};
} decoderconfig, *pdecoderconfig;

/**
 * @brief The Decoder class provides utilities for decoding media files.
 */
class Decoder {
private:
    Decoder(const std::string &path);
    Decoder(const DecoderConfig &config);

public:
    ~Decoder();

    // Factory methods
    static DecoderResult Create(const std::string &path);
    static DecoderResult Create(const DecoderConfig &config);

private:
    AvError m_Initialize();

    std::shared_ptr<Demuxer> m_demuxer;
    decoderconfig m_config;


};

} // namespace AV::Utils