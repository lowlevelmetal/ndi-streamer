/**
 * @file ndisource.cpp
 * @brief This file includes utilities for working with NDI.
 * @date 2024-09-08
 * @author Matthew Todd Geiger
 */

#include "ndisource.hpp"
#include "macro.hpp"

namespace AV::Utils {

AvException NdiSource::SendVideoFrame(AVFrame *frame, CodecFrameRate framerate, AVPixelFormat format) {
    // Setup the video frame
    NDIlib_video_frame_v2_t video_frame;

    switch (format) {
    case AV_PIX_FMT_YUV422P:
        video_frame.FourCC = NDIlib_FourCC_type_UYVY;
        break;
    default:
        return AvException(AvError::NDIINVALIDPIXFMT);
    }

    //video_frame.frame_format_type = NDIlib_frame_format_type_progressive;
    video_frame.xres = frame->width;
    video_frame.yres = frame->height;
    video_frame.frame_rate_N = framerate.first;
    video_frame.frame_rate_D = framerate.second;
    video_frame.p_data = frame->data[0];
    video_frame.line_stride_in_bytes = frame->linesize[0];

    NDIlib_send_send_video_v2(m_send_instance, &video_frame);

    return AvException(AvError::NOERROR);
}

NdiSourceResult NdiSource::Create(const std::string &ndi_name) {
    DEBUG("NdiSource factory called");
    AvException error(AvError::NOERROR);

    // Create a new NdiSource object, return nullopt if error
    try {
        return {std::unique_ptr<NdiSource>(new NdiSource(ndi_name)), AvException(AvError::NOERROR)};
    } catch (AvException err) {
        DEBUG("NdiSource error: %s", err.what());
        error = err;
    }

    return {nullptr, error};
}

NdiSource::NdiSource(const std::string &ndi_name) : m_name(ndi_name) {
    DEBUG("Constructing NdiSource object");

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}

NdiSource::~NdiSource() {
    DEBUG("Destroying NdiSource object");

    if (m_send_instance) {
        DEBUG("Destroying NDI send instance");
        NDIlib_send_destroy(m_send_instance);
    }
}

AvError NdiSource::m_Initialize() {
    // Create a new send instance
    DEBUG("Creating NDI send instance");
    NDIlib_send_create_t send_create_desc;
    send_create_desc.p_ndi_name = m_name.c_str();
    m_send_instance = NDIlib_send_create(&send_create_desc);
    if (!m_send_instance) {
        return AvError::NDISENDINSTANCE;
    }

    return AvError::NOERROR;
}

} // namespace AV::Utils