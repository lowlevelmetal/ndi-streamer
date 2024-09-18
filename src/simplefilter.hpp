/**
 * @file simplefilter.hpp
 * @brief SimpleFilter class
 * @version 1.0
 * @date 2024-09-18
 * @author Matthew Todd Geiger
 */

#pragma once

#include "averror.hpp"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include <string>
#include <memory>
#include <vector>

namespace AV::Utils {

class SimpleFilter;
using SimpleFilterResult = std::pair<std::unique_ptr<SimpleFilter>, const AvException>;
using SimpleFilterOutput = std::pair<std::vector<AVFrame *>, const AvException>;

class SimpleFilter {
private:
    SimpleFilter() = delete;
    SimpleFilter(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base);

    AvError _Initialize(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base);

public:
    ~SimpleFilter();

    static void PrintFilters(); // There if you are curious, but no serious use.

    // Factory methods
    static SimpleFilterResult CreateFilter(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base);

    // Filter methods
    SimpleFilterOutput FilterFrame(AVFrame *frame);

private:
    AVFilterGraph *_filter_graph = nullptr;

    AVFilterContext *_buffersrc_ctx = nullptr;
    AVFilterContext *_buffersink_ctx = nullptr;
};

} // namespace AV::Utils