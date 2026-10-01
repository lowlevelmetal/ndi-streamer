/**
 * @file options.hpp
 * @brief Command line options.
 */

#pragma once

#include "av/decoder.hpp"
#include "av/input.hpp"
#include "av/video_processor.hpp"
#include "ndi/sender.hpp"
#include "util/log.hpp"

#include <string>

namespace ndistreamer {

struct Options {
    av::InputConfig input;
    av::DecoderConfig decoder;
    std::string video_filter;
    av::OutputPixelFormat pixel_format = av::OutputPixelFormat::Auto;
    int audio_rate = 0;
    int audio_channels = 0;

    ndi::SenderConfig sender{"NDI Source", ""};
    std::string ndi_library;

    bool loop = false;
    bool pacing = true;
    int stats_interval = 0; ///< Seconds between statistics lines; 0 disables them.
    log::Level log_level = log::Level::Info;
};

enum class ParseOutcome {
    Run,         ///< Options are valid; start streaming.
    ExitSuccess, ///< Help or version was requested; print the message and exit 0.
    ExitError,   ///< Invalid usage; print the message and exit 2.
};

/**
 * @brief Parse and validate the command line.
 * @param message receives help/version text or an error description
 */
ParseOutcome ParseOptions(int argc, char *argv[], Options &options, std::string &message);

} // namespace ndistreamer
