/**
 * @file clock.hpp
 * @brief Shared presentation clock that maps media timestamps onto wall-clock time.
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stop_token>

namespace ndistreamer {

/**
 * @brief Maps media timestamps (microseconds) to send deadlines and NDI timecodes.
 *
 * Every output stream (video, audio) owns a presenter that calls WaitForStart() with its first
 * timestamp. Playback starts once all expected streams are ready, or shortly after the first one if
 * another is slow to produce data. The earliest first timestamp becomes the media origin, so streams
 * that start later in the file keep their relative offset.
 */
class PresentationClock {
public:
    using Clock = std::chrono::steady_clock;

    /**
     * @param streams       number of presenters that will call WaitForStart()
     * @param preroll       delay between the start decision and the first deadline, letting queues fill
     * @param start_timeout how long to wait for slower streams after the first one is ready
     */
    explicit PresentationClock(int streams, Clock::duration preroll = std::chrono::milliseconds(100),
                               Clock::duration start_timeout = std::chrono::milliseconds(500));

    /**
     * @brief Report a stream's first timestamp and block until playback starts.
     * @return false if a stop was requested while waiting.
     */
    bool WaitForStart(int64_t first_pts_us, std::stop_token stop);

    /**
     * @brief Report that a stream ended without ever calling WaitForStart().
     */
    void StreamFinished();

    /**
     * @brief Wall-clock deadline at which the given media timestamp should be sent.
     */
    Clock::time_point DueTime(int64_t pts_us) const;

    /**
     * @brief NDI timecode (100 ns units, UTC based) for the given media timestamp.
     *
     * Deriving timecodes from media timestamps keeps audio and video exactly aligned for receivers that
     * synchronize on timecode, independent of send jitter.
     */
    int64_t Timecode(int64_t pts_us) const;

    /**
     * @brief Re-anchor the clock so that @p pts_us is due at @p when.
     * Used to recover from timestamp discontinuities or from falling far behind real time.
     */
    void Resync(int64_t pts_us, Clock::time_point when);

    bool Started() const;

private:
    void StartLocked(Clock::time_point now);

    mutable std::mutex m_mutex;
    std::condition_variable_any m_cv;

    int m_expected;
    int m_ready = 0;
    bool m_started = false;
    std::optional<Clock::time_point> m_first_ready;
    int64_t m_min_first_pts = INT64_MAX;

    const Clock::duration m_preroll;
    const Clock::duration m_start_timeout;

    int64_t m_origin_us = 0;
    Clock::time_point m_epoch{};
    int64_t m_utc_epoch_100ns = 0;
};

} // namespace ndistreamer
