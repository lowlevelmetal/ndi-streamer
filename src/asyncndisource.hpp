/**
 * @file asyncndisource.hpp
 * @brief This file includes utilities for working with NDI sources asynchronously.
 * @date 2024-09-20
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#pragma once

// Local includes
#include "averror.hpp"
#include "ndi.hpp"

// NDI SDK
#include <Processing.NDI.Lib.h>

// FFMPEG includes
extern "C" {
#include <libavutil/frame.h>
}

// Standard C++ includes
#include <string>
#include <memory>
#include <deque>
#include <thread>
#include <mutex>

#define FRAME_QUEUE_SIZE 50

namespace AV::Utils {

// Forward declarations and type definitions
class AsyncNDISource;
using AsyncNDISourceResult = std::pair<std::shared_ptr<AsyncNDISource>, const AvException>;

class AsyncNDISource : public NDI {
private:
    AsyncNDISource(const std::string &source_name, const AVRational &frame_rate);
    AvError _Initialize();
    AvError _SendVideoFrame(const AVFrame *frame);
    AvError _SendAudioFrame(const AVFrame *frame);
    void _Thread_FrameSender();

public:
    ~AsyncNDISource();

    // Factory
    static AsyncNDISourceResult Create(const std::string &source_name, const AVRational &frame_rate);

    AvException SendFrame(const AVFrame *frame);

private:
    std::thread _frame_sender_thread;
    std::string _source_name;
    NDIlib_send_instance_t _ndi_send_instance = nullptr;
    AVRational _frame_rate;
    std::atomic<bool> _running = true;

    std::deque<AVFrame *> _frame_queue;
    std::mutex _frame_queue_mutex;

};

} // namespace AV::Utils