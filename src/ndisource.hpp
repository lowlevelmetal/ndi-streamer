/**
 * @file ndisource.hpp
 * @brief This file includes utilities for working with NDI.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

#include <memory>

#include "ndi.hpp"
#include "averror.hpp"
#include "decoder.hpp"

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
}

namespace AV::Utils {

class NdiSource;
using NdiSourceResult = std::pair<std::unique_ptr<NdiSource>, const AvException>;

class NdiSource : public Ndi {
private:
    NdiSource(const std::string &ndi_name);

public:
    ~NdiSource();

    static NdiSourceResult Create(const std::string &ndi_name);

    AvException SendVideoFrame(AVFrame *frame, CodecFrameRate framerate, AVPixelFormat format);
    AvException SendAudioFrame(AVFrame *frame);
    AvException SendAudioFrameS16(AVFrame *frame);

private:
    AvError m_Initialize();

    NDIlib_send_instance_t m_send_instance = nullptr;
    std::string m_name;
};

} // namespace AV::Utils