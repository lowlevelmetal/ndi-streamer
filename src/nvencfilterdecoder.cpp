/**
 * @file nvencfilterdecoder.cpp
 * @brief This file includes the NVENCFilterDecoder class.
 * @date 2024-09-17
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#include "nvencfilterdecoder.hpp"
#include "macro.hpp"

namespace AV::Utils {
NVENCFilterDecoder::~NVENCFilterDecoder() {
    if (_codec_ctx) {
        avcodec_free_context(&_codec_ctx);
    }

    if (_last_frame) {
        av_frame_free(&_last_frame);
    }

    if (_filtered_frame) {
        av_frame_free(&_filtered_frame);
    }

    if (_hw_device_ctx) {
        av_buffer_unref(&_hw_device_ctx);
    }

    if (_filter_graph) {
        avfilter_graph_free(&_filter_graph);
    }

    if (_buffersink_ctx) {
        avfilter_free(_buffersink_ctx);
    }

    if (_buffersrc_ctx) {
        avfilter_free(_buffersrc_ctx);
    }

    if (_outputs) {
        avfilter_inout_free(&_outputs);
    }

    if (_inputs) {
        avfilter_inout_free(&_inputs);
    }
}

NVENCFilterDecoder::NVENCFilterDecoder(AVCodecParameters *cparams, AVRational time_base) : _cparams(cparams), _time_base(time_base) {
    FUNCTION_CALL_DEBUG();

    AvError err = _Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}

NVENCFilterDecoderResult NVENCFilterDecoder::Create(AVCodecParameters *cparams, AVRational time_base) {
    FUNCTION_CALL_DEBUG();

    try {
        return {std::unique_ptr<NVENCFilterDecoder>(new NVENCFilterDecoder(cparams, time_base)), AvError::NOERROR};
    } catch (AvError e) {
        return {nullptr, e};
    }
}

DecoderOutput NVENCFilterDecoder::Decode() {
    FUNCTION_CALL_DEBUG();

    av_frame_unref(_last_frame);
    av_frame_unref(_filtered_frame);

    int ret = avcodec_receive_frame(_codec_ctx, _last_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return {nullptr, AvError::DECODEREXHAUSTED};
    } else if (ret < 0) {
        DEBUG("avcodec_receive_frame failed");
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvError::RECIEVEFRAME};
    }

    // Send frame to filter
    ret = av_buffersrc_add_frame_flags(_buffersrc_ctx, _last_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        DEBUG("av_buffersrc_add_frame_flags failed");
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvError::FILTERGRAPHALLOC};
    }

    // Get filtered frame
    ret = av_buffersink_get_frame(_buffersink_ctx, _filtered_frame);
    if (ret < 0) {
        DEBUG("av_buffersink_get_frame failed");
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvError::FILTERGRAPHALLOC};
    }

    DEBUG("Filtered frame pixel format: %d", _filtered_frame->format);
    if (_filtered_frame->format == AV_PIX_FMT_NV12) {
        uint8_t *y_data = _filtered_frame->data[0];
        uint8_t *uv_data = _filtered_frame->data[1];
        DEBUG("Filtered frame first Y bytes: %02x %02x %02x %02x", y_data[0], y_data[1], y_data[2], y_data[3]);
        DEBUG("Filtered frame first UV bytes: %02x %02x %02x %02x", uv_data[0], uv_data[1], uv_data[2], uv_data[3]);
    }

    return {_filtered_frame, AvError::NOERROR};
}

AvException NVENCFilterDecoder::FillDecoder(AVPacket *packet) {
    FUNCTION_CALL_DEBUG();

    int ret = avcodec_send_packet(_codec_ctx, packet);
    if (ret < 0) {
        DEBUG("avcodec_send_packet failed");
        PRINT_FFMPEG_ERR(ret);
        return AvError::SENDPACKET;
    }

    return AvError::NOERROR;
}

AvError NVENCFilterDecoder::_Initialize() {
    // Find decoder
    const AVCodec *codec = avcodec_find_decoder(_cparams->codec_id);
    if (codec == nullptr) {
        DEBUG("avcodec_find_decoder failed");
        return AvError::FINDDECODER;
    }

    // Allocate codec context
    _codec_ctx = avcodec_alloc_context3(codec);
    if (_codec_ctx == nullptr) {
        DEBUG("avcodec_alloc_context3 failed");
        return AvError::DECODERALLOC;
    }

    // Copy codec parameters
    int ret = avcodec_parameters_to_context(_codec_ctx, _cparams);
    if (ret < 0) {
        DEBUG("avcodec_parameters_to_context failed");
        PRINT_FFMPEG_ERR(ret);
        return AvError::DECPARAMS;
    }

    // Initialize hardware decoder
    ret = av_hwdevice_ctx_create(&_hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
    if (ret < 0) {
        DEBUG("av_hwdevice_ctx_create failed");
        PRINT_FFMPEG_ERR(ret);
        return AvError::HWDEVICECTXALLOC;
    }

    _codec_ctx->hw_device_ctx = av_buffer_ref(_hw_device_ctx);
    if (_codec_ctx->hw_device_ctx == nullptr) {
        DEBUG("av_buffer_ref failed");
        return AvError::HWDEVICEGETBUF;
    }

    // Open decoder
    ret = avcodec_open2(_codec_ctx, codec, nullptr);
    if (ret < 0) {
        DEBUG("avcodec_open2 failed");
        PRINT_FFMPEG_ERR(ret);
        return AvError::DECPARAMS;
    }

    // Setup filter
    const char *filter_description = "hwupload_cuda,scale_npp=format=nv12,hwdownload";

    _filter_graph = avfilter_graph_alloc();
    if (_filter_graph == nullptr) {
        DEBUG("avfilter_graph_alloc failed");
        return AvError::FILTERGRAPHALLOC;
    }

    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");

    _outputs = avfilter_inout_alloc();
    _inputs = avfilter_inout_alloc();

    if (!_outputs || !_inputs) {
        DEBUG("avfilter_inout_alloc failed");
        return AvError::FILTERGRAPHALLOC;
    }

    char args[512];
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             _codec_ctx->width, _codec_ctx->height, _codec_ctx->pix_fmt,
             _time_base.num, _time_base.den,
             _codec_ctx->sample_aspect_ratio.num, _codec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&_buffersrc_ctx, buffersrc, "in", args, nullptr, _filter_graph);
    if (ret < 0) {
        DEBUG("avfilter_graph_create_filter failed");
        return AvError::FILTERGRAPHALLOC;
    }

    ret = avfilter_graph_create_filter(&_buffersink_ctx, buffersink, "out", nullptr, nullptr, _filter_graph);
    if (ret < 0) {
        DEBUG("avfilter_graph_create_filter failed");
        return AvError::FILTERGRAPHALLOC;
    }

    // Set output pixel formats
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_NV12, AV_PIX_FMT_NONE};
    if ((ret = av_opt_set_int_list(_buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0) {
        DEBUG("av_opt_set_int_list failed");
        return AvError::FILTERGRAPHALLOC;
    }

    // connect filters
    _outputs->name = av_strdup("in");
    _outputs->filter_ctx = _buffersrc_ctx;
    _outputs->pad_idx = 0;
    _outputs->next = nullptr;

    _inputs->name = av_strdup("out");
    _inputs->filter_ctx = _buffersink_ctx;
    _inputs->pad_idx = 0;
    _inputs->next = nullptr;

    if ((ret = avfilter_graph_parse_ptr(_filter_graph, filter_description, &_inputs, &_outputs, nullptr)) < 0) {
        DEBUG("avfilter_graph_parse_ptr failed");
        return AvError::FILTERGRAPHALLOC;
    }

    if ((ret = avfilter_graph_config(_filter_graph, nullptr)) < 0) {
        DEBUG("avfilter_graph_config failed");
        return AvError::FILTERGRAPHALLOC;
    }

    _last_frame = av_frame_alloc();
    if (_last_frame == nullptr) {
        DEBUG("av_frame_alloc failed");
        return AvError::FRAMEALLOC;
    }

    _filtered_frame = av_frame_alloc();
    if (_filtered_frame == nullptr) {
        DEBUG("av_frame_alloc failed");
        return AvError::FRAMEALLOC;
    }

    return AvError::NOERROR;
}
} // namespace AV::Utils