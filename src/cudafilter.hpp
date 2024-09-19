/**
 * @file cudafilter.hpp
 * @brief CudaFilter class
 * @version 1.0
 * @date 2024-09-19
 * @author Matthew Todd Geiger
 */

#pragma once

#include "averror.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

#include <memory>
#include <string>
#include <vector>

namespace AV::Utils {

class CudaFilter;
using CudaFilterResult = std::pair<std::unique_ptr<CudaFilter>, const AvException>;
using CudaFilterOutput = std::pair<std::vector<AVFrame *>, const AvException>;

class CudaFilter {
private:
    CudaFilter() = delete;
    CudaFilter(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base, AVPixelFormat output_format);
    AvError _Initialize(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base, AVPixelFormat output_format);

public:
    ~CudaFilter();
    static CudaFilterResult Create(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base, AVPixelFormat output_format);
    CudaFilterOutput FilterFrame(AVFrame *frame);

private:
    AVFilterGraph *_filter_graph = nullptr;
    AVFilterContext *_buffersrc_ctx = nullptr;
    AVFilterContext *_buffersink_ctx = nullptr;
};

} // namespace AV::Utils