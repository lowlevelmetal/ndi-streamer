/**
 * @file frametimer.cpp
 * @brief FrameTimer class implementation file
 * @version 1.0
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 */

#include "frametimer.hpp"
#include "frame.hpp"
#include "macro.hpp"

#include <algorithm>
#include <chrono>

namespace AV::Utils {

/**
 * @brief Construct a new FrameTimer object
 * 
 * @param capacity The capacity of the FrameTimer
 */
FrameTimer::FrameTimer(const int capacity) {
    FUNCTION_CALL_DEBUG();

    _capacity = capacity;
}

/**
 * @brief Destroy the FrameTimer object
 */
FrameTimer::~FrameTimer() {
    FUNCTION_CALL_DEBUG();

    // Free all frames
    for (auto frame : _frames) {
        av_frame_free(&frame);
    }
}

/**
 * @brief Add a frame to the FrameTimer
 * Keep in mind that a copy(referenced) of the frame is made.
 * 
 * @param frame The frame to add
 * @return AvException
 */
AvException FrameTimer::AddFrame(AVFrame *frame) {
    FUNCTION_CALL_DEBUG();

    if(_frames.size() >= _capacity) {
        return AvError::BUFFERFULL;
    }

    // Check if frame has valid time_base and pts
    if (frame->pts == AV_NOPTS_VALUE || frame->time_base.den == 0) {
        PRINT("Invalid Frame Info: PTS: %ld, Time Base: %d/%d", frame->pts, frame->time_base.num, frame->time_base.den);

        // What type of frame is this?
        PrintPictType(frame->pict_type);

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

/**
 * @brief Get the next frame in the sequence
 * This function will give you a copy of the frame and remove it from the FrameTimer
 * 
 * @return AVFrame* The next frame in the sequence
 */
AVFrame *FrameTimer::GetFrame() {
    FUNCTION_CALL_DEBUG();

#ifdef _DEBUG
    // profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif

    if (_frames.empty()) {
        return nullptr;
    }

    // Pop a frame off
    AVFrame *frame = _frames.back();
    _frames.pop_back();

#ifdef _DEBUG
    // profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("Frame get time (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif

    return frame;
}

/**
 * @brief Check if the FrameTimer is full
 * @return bool True if full, false otherwise
 */
bool FrameTimer::IsFull() {
    FUNCTION_CALL_DEBUG();

    return _frames.size() >= _capacity;
}

/**
 * @brief Check if the FrameTimer is empty
 * @return bool True if empty, false otherwise
 */
bool FrameTimer::IsEmpty() {
    FUNCTION_CALL_DEBUG();

    return _frames.empty();
}

bool FrameTimer::IsHalf() {
    FUNCTION_CALL_DEBUG();

    return _frames.size() >= _capacity / 2;
}

/**
 * @brief This function will order the frame based on the
 * pts, time_base, and frame_rate.
 */
AvError FrameTimer::_ReorderFrames() {
    FUNCTION_CALL_DEBUG();

#ifdef _DEBUG
    // profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif

    // Sort the frames based on pts order
    std::sort(_frames.begin(), _frames.end(), [](AVFrame *a, AVFrame *b) {
        // Force a universal time unit of microseconds
        auto apts = av_rescale_q(a->pts, a->time_base, {1, 1000000});
        auto bpts = av_rescale_q(b->pts, b->time_base, {1, 1000000});

        return apts > bpts;
    });

#ifdef _DEBUG
    // profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("Frame reorder time (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif

    return AvError::NOERROR;
}

} // namespace AV::Utils