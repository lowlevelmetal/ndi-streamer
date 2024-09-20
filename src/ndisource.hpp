/**
 * @file ndisource.hpp
 * @brief This file includes utilities for working with NDI sources.
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#pragma once

// Local includes
#include "ndi.hpp"
#include "averror.hpp"

// NDI SDK
#include <Processing.NDI.Lib.h>

// FFMPEG includes
extern "C" {
#include <libavutil/frame.h>
}

// Standard C++ includes
#include <string>
#include <memory>

namespace AV::Utils {

// Forward declarations and type definitions
class NDISource;
using NDISourceResult = std::pair<std::shared_ptr<NDISource>, const AvException>;

class NDISource : public NDI {
private:
    NDISource(const std::string &source_name, const AVRational &frame_rate);
    AvError _Initialize();
    AvError _SendVideoFrame(const AVFrame *frame);
    AvError _SendAudioFrame(const AVFrame *frame);

public:
    ~NDISource();

    // Factory
    static NDISourceResult Create(const std::string &source_name, const AVRational &frame_rate);

    AvException SendFrame(const AVFrame *frame);

private:
    std::string _source_name;
    NDIlib_send_instance_t _ndi_send_instance = nullptr;
    AVRational _frame_rate;

};

} // namespace AV::Utils