/**
 * @file simplefilter.cpp
 * @brief SimpleFilter class
 * @version 1.0
 * @date 2024-09-18
 * @author Matthew Todd Geiger
 */

#include "simplefilter.hpp"
#include "macro.hpp"

namespace AV::Utils {

/**
 * @brief Construct a new Simple Filter object
 * 
 * @param filter_description The filter description
 * @param codec_parameters The codec parameters
 * @param time_base The time base
 * @return SimpleFilterResult The SimpleFilterResult
 */
SimpleFilterResult SimpleFilter::CreateFilter(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base) {
    FUNCTION_CALL_DEBUG();

    try {
        return {std::unique_ptr<SimpleFilter>(new SimpleFilter(filter_description, codec_parameters, time_base)), AvError::NOERROR};
    } catch(const AvException &e) {
        return {nullptr, e};
    }
}

/**
 * @brief Construct a new Simple Filter object
 * 
 * @param filter_description The filter description
 * @param codec_parameters The codec parameters
 * @param time_base The time base
 */
SimpleFilter::SimpleFilter(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base) {
    FUNCTION_CALL_DEBUG();

    AvError error = _Initialize(filter_description, codec_parameters, time_base);
    if (error != AvError::NOERROR) {
        throw AvException(error);
    }
}

/**
 * @brief Deconstruct the Simple Filter object
 */
SimpleFilter::~SimpleFilter() {
    FUNCTION_CALL_DEBUG();

    if(_filter_graph) {
        avfilter_graph_free(&_filter_graph);
    }

}

AvError SimpleFilter::_Initialize(const std::string &filter_description, const AVCodecParameters *codec_parameters, const AVRational &time_base) {
    FUNCTION_CALL_DEBUG();

    // Allocate memory for filter graph
    _filter_graph = avfilter_graph_alloc();
    if (!_filter_graph) {
        return AvError::FILTER_GRAPH_ALLOC;
    }

    // Get buffer and buffersink filters
    const AVFilter *_buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *_buffersink = avfilter_get_by_name("buffersink");

    // Define input filter parameters
    char args[512];
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             codec_parameters->width, codec_parameters->height, codec_parameters->format,
             time_base.num, time_base.den, codec_parameters->sample_aspect_ratio.num, codec_parameters->sample_aspect_ratio.den);

    // Create input filter
    int ret = avfilter_graph_create_filter(&_buffersrc_ctx, _buffersrc, "in", args, nullptr, _filter_graph);
    if (ret < 0) {
        return AvError::FILTER_GRAPH_CREATE_FILTER;
    }

    // Create output filter
    ret = avfilter_graph_create_filter(&_buffersink_ctx, _buffersink, "out", nullptr, nullptr, _filter_graph);
    if (ret < 0) {
        return AvError::FILTER_GRAPH_CREATE_FILTER;
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
        return AvError::FILTER_GRAPH_ALLOC;
    }

    return AvError::NOERROR;
}

SimpleFilterOutput SimpleFilter::FilterFrame(AVFrame *frame) {
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

void SimpleFilter::PrintFilters() {
    FUNCTION_CALL_DEBUG();

    // Get the filter list
    const AVFilter *filter = nullptr;
    void *filter_iterator = nullptr;

    while ((filter = av_filter_iterate(&filter_iterator))) {
        PRINT("Filter: %s", filter->name);
    }
}

} // namespace AV::Utils