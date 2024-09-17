/**
 * @file nvencfilterdecoder.hpp
 * @brief This file includes the NVENCFilterDecoder class.
 * @date 2024-09-17
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#pragma once

// Local includes
#include "averror.hpp"
#include "decoder.hpp"


// 3rd party includes
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

// Standard C++ includes
#include <memory>

namespace AV::Utils {

class NVENCFilterDecoder;
using NVENCFilterDecoderResult = std::pair<std::unique_ptr<NVENCFilterDecoder>, AvException>;

class NVENCFilterDecoder {
private:
    NVENCFilterDecoder(AVCodecParameters *cparams, AVRational time_base);
    AvError _Initialize();

public:
    ~NVENCFilterDecoder();

    static NVENCFilterDecoderResult Create(AVCodecParameters *cparams, AVRational time_base);

    AvException FillDecoder(AVPacket *packet);
    DecoderOutput Decode();

private:
    AVCodecContext *_codec_ctx = nullptr;
    AVCodecParameters *_cparams = nullptr;
    AVFrame *_last_frame = nullptr;
    AVFrame *_filtered_frame = nullptr;
    AVBufferRef *_hw_device_ctx = nullptr;
    AVFilterGraph *_filter_graph = nullptr;
    AVFilterContext *_buffersink_ctx = nullptr;
    AVFilterContext *_buffersrc_ctx = nullptr;
    AVFilterInOut *_outputs = nullptr, *_inputs = nullptr;
    AVRational _time_base;

};


} // namespace AV::Utils