/**
 * @file frametimer.hpp
 * @brief FrameTimer class header file
 * @version 1.0
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 */

#pragma once

// Local includes
#include "averror.hpp"

// 3rd Party Dependencies
extern "C" {
#include <libavformat/avformat.h>
}

// Standard C++ Dependencies
#include <vector>

// Defines
#define AVUTILS_FRAMETIMER_DEFAULT_CAPACITY 10

/**
 * In Audio/Video, frames can be in a file out of order.
 * This class is used to keep track of the order of frames.
 *
 * We can accomplish this by using the pts, time_base, and frame_rate.
 *
 * The pts is the presentation timestamp, which is the time the frame should be displayed.
 * The time_base is the time unit of the pts.
 * The frame_rate is the number of frames per second.
 *
 * An easy way to predict the next PTS is to use the formula:
 * Video: pts = last_pts + (1 / frame_rate)
 * Audio: pts = last_pts + (1 / sample_rate)
 */

namespace AV::Utils {

class FrameTimer {
public:
    FrameTimer(const int capacity = AVUTILS_FRAMETIMER_DEFAULT_CAPACITY);
    ~FrameTimer();

    /**
     * @brief Add a frame to the FrameTimer
     * Keep in mind that a copy(referenced) of the frame is made.
     * @param frame The frame to add
     */
    AvException AddFrame(AVFrame *frame);
    
    /**
     * @brief Get the next frame in the sequence
     * This function will give you a copy of the frame and remove it from the FrameTimer
     * @return AVFrame* The next frame in the sequence
     */
    AVFrame *GetFrame();

    /**
     * @brief Check if the FrameTimer is full
     */
    bool IsFull();

private:
    AvError _ReorderFrames();

    std::vector<AVFrame *> _frames;
    int _capacity;
};

} // namespace AV::Utils