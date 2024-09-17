/**
 * @file ndisource.cpp
 * @brief This file includes utilities for working with NDI sources.
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#include "ndisource.hpp"

namespace AV::Utils {

AvError NDISource::_SendVideoFrame(const AVFrame *frame) {
    
    // Build NDI packet from frame
    NDIlib_video_frame_v2_t video_frame;

    switch(frame->format) {
    case AV_PIX_FMT_UYVY422:
        video_frame.FourCC = NDIlib_FourCC_type_UYVY;
        break;
    default:
        return AvError::NDIINVALIDPIXFMT;
    }

    video_frame.xres = frame->width;
    video_frame.yres = frame->height;
    video_frame.line_stride_in_bytes = frame->linesize[0];
    video_frame.frame_rate_N = _frame_rate.num;
    video_frame.frame_rate_D = _frame_rate.den;
    video_frame.timecode = NDIlib_send_timecode_synthesize;
    video_frame.p_data = frame->data[0];

    // Send the frame
    NDIlib_send_send_video_v2(_ndi_send_instance, &video_frame);
}

/**
 * Only send audio frames that are interleaved 16-bit signed PCM.
 */
AvError NDISource::_SendAudioFrame(const AVFrame *frame) {
    
    // Build NDI packet from frame
    NDIlib_audio_frame_interleaved_16s_t audio_frame;

    audio_frame.sample_rate = frame->sample_rate;
    audio_frame.no_channels = frame->channels;
    audio_frame.no_samples = frame->nb_samples;
    audio_frame.timecode = NDIlib_send_timecode_synthesize;
    audio_frame.p_data = (int16_t *)frame->data[0];

    // Send the frame
    NDIlib_util_send_send_audio_interleaved_16s(_ndi_send_instance, &audio_frame);
}

} // namespace AV::Utils