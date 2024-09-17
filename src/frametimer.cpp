/**
 * @file frametimer.cpp
 * @brief FrameTimer class implementation file
 * @version 1.0
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 */

#include "frametimer.hpp"
#include "frame.hpp"

#include <algorithm>

namespace AV::Utils {

FrameTimer::FrameTimer(const int capacity) {
    _capacity = capacity;
}

FrameTimer::~FrameTimer() {
    // Free all frames
    for (auto frame : _frames) {
        av_frame_free(&frame);
    }
}


AvException FrameTimer::AddFrame(AVFrame *frame) {
    if(_frames.size() >= _capacity) {
        return AvError::BUFFERFULL;
    }

    // Check if frame has valid time_base and pts
    if (frame->time_base.num == 0 || frame->time_base.den == 0 || frame->pts == AV_NOPTS_VALUE) {
        return AvError::INVALIDFRAME;
    }

    // Copy the frame
    AVFrame *new_frame = CopyFrame(frame);
    if (!new_frame) {
        return AvError::FRAMEALLOC;
    }

    // Add the frame to the vector
    _frames.push_back(new_frame);

    // Reorder frames
    auto err = _ReorderFrames();
    if (err != AvError::NOERROR) {
        return err;
    }

    return AvError::NOERROR;
}

AVFrame *FrameTimer::GetFrame() {
    if (_frames.empty()) {
        return nullptr;
    }

    // Pop a frame off
    AVFrame *frame = _frames.back();
    _frames.pop_back();

    return frame;
}

bool FrameTimer::IsFull() {
    return _frames.size() >= _capacity;
}

/**
 * @brief This function will order the frame based on the
 * pts, time_base, and frame_rate.
 */
AvError FrameTimer::_ReorderFrames() {
    // Sort the frames based on pts order
    std::sort(_frames.begin(), _frames.end(), [](AVFrame *a, AVFrame *b) {
        // Force a universal time unit of microseconds
        auto apts = av_rescale_q(a->pts, a->time_base, {1, 1000000});
        auto bpts = av_rescale_q(b->pts, b->time_base, {1, 1000000});

        return apts > bpts;
    });

    return AvError::NOERROR;
}

} // namespace AV::Utils