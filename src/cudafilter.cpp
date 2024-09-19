/**
 * @file cudafilter.cpp
 * @brief CudaFilter class
 * @version 1.0
 * @date 2024-09-19
 * @author Matthew Todd Geiger
 */

#include "cudafilter.hpp"
#include "macro.hpp"

namespace AV::Utils {

CudaFilterResult CudaFilter::Create(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base, AVPixelFormat output_format) {
    FUNCTION_CALL_DEBUG();

    try {
        return {std::unique_ptr<CudaFilter>(new CudaFilter(filter_description, codec_parameters, time_base, output_format)), AvError::NOERROR};
    } catch(const AvException &e) {
        return {nullptr, e};
    }
}

CudaFilter::CudaFilter(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base, AVPixelFormat output_format) {
    FUNCTION_CALL_DEBUG();

    AvError error = _Initialize(filter_description, codec_parameters, time_base, output_format);
    if (error != AvError::NOERROR) {
        throw AvException(error);
    }
}

CudaFilter::~CudaFilter() {
    FUNCTION_CALL_DEBUG();

    if(_filter_graph) {
        avfilter_graph_free(&_filter_graph);
    }
}

CudaFilterOutput CudaFilter::FilterFrame(AVFrame *frame) {
    FUNCTION_CALL_DEBUG();

    std::vector<AVFrame *> filtered_frames;

    // Create a frame
    AVFrame *filtered_frame = av_frame_alloc();
    if (!filtered_frame) {
        return {filtered_frames, AvError::FRAMEALLOC};
    }

    // Set the frame parameters
    int ret = av_buffersrc_add_frame(_buffersrc_ctx, frame);
    if (ret < 0) {
        return {filtered_frames, AvError::BUFFERSRC_ADD_FRAME};
    }

    // Pull filtered frames from the sink
    while (true) {
        ret = av_buffersink_get_frame(_buffersink_ctx, filtered_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }

        if (ret < 0) {
            return {filtered_frames, AvError::BUFFERSINK_GET_FRAME};
        }

        // Copy the frame
        AVFrame *new_frame = av_frame_clone(filtered_frame);
        if (!new_frame) {
            return {filtered_frames, AvError::FRAMEALLOC};
        }

        filtered_frames.push_back(new_frame);
        av_frame_unref(filtered_frame);
    }

    av_frame_free(&filtered_frame);
    return {filtered_frames, AvError::NOERROR};
}

AvError CudaFilter::_Initialize(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base, AVPixelFormat output_format) {
    FUNCTION_CALL_DEBUG();

    // Create filter graph
    _filter_graph = avfilter_graph_alloc();
    if (_filter_graph == nullptr) {
        return AvError::FILTERGRAPHALLOC;
    }

    // Create input filter
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    if (buffersrc == nullptr) {
        return AvError::FILTER_GET_BY_NAME;
    }

    // Create output filter
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    if (buffersink == nullptr) {
        return AvError::FILTER_GET_BY_NAME;
    }

    // Define input filter parameters
    char args[512];
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             codec_parameters->width, codec_parameters->height, codec_parameters->format,
             time_base.num, time_base.den, codec_parameters->sample_aspect_ratio.num, codec_parameters->sample_aspect_ratio.den);

    // Create input filter
    int ret = avfilter_graph_create_filter(&_buffersrc_ctx, buffersrc, "in", args, nullptr, _filter_graph);
    if (ret < 0) {
        return AvError::FILTER_GRAPH_CREATE_FILTER;
    }
    
    // Create output filter
    ret = avfilter_graph_create_filter(&_buffersink_ctx, buffersink, "out", nullptr, nullptr, _filter_graph);
    if (ret < 0) {
        return AvError::FILTER_GRAPH_CREATE_FILTER;
    }

    // Set output pixel formats
    enum AVPixelFormat pix_fmts[] = {output_format, AV_PIX_FMT_NONE};
    if ((ret = av_opt_set_int_list(_buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0) {
        return AvError::FILTER_GRAPH_CONFIG;
    }

    // Define input and output filter parameters
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();

    // Set input filter parameters
    outputs->name = av_strdup("in");
    outputs->filter_ctx = _buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    // Set output filter parameters
    inputs->name = av_strdup("out");
    inputs->filter_ctx = _buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // Add input and output filters to the graph
    if ((ret = avfilter_graph_parse_ptr(_filter_graph, filter_description.c_str(), &inputs, &outputs, nullptr)) < 0) {
        return AvError::FILTER_GRAPH_PARSE;
    }

    // Configure graph
    ret = avfilter_graph_config(_filter_graph, nullptr);
    if (ret < 0) {
        return AvError::FILTER_GRAPH_CONFIG;
    }

    return AvError::NOERROR;
}

} // namespace AV::Utils