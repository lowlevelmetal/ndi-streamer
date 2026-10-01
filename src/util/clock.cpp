/**
 * @file clock.cpp
 * @brief Shared presentation clock that maps media timestamps onto wall-clock time.
 */

#include "util/clock.hpp"

#include <algorithm>

namespace ndistreamer {

namespace {

int64_t UtcNow100ns(PresentationClock::Clock::time_point at) {
    using namespace std::chrono;
    auto offset = at - PresentationClock::Clock::now();
    auto utc = system_clock::now() + duration_cast<system_clock::duration>(offset);
    return duration_cast<nanoseconds>(utc.time_since_epoch()).count() / 100;
}

} // namespace

PresentationClock::PresentationClock(int streams, Clock::duration preroll, Clock::duration start_timeout)
    : m_expected(streams), m_preroll(preroll), m_start_timeout(start_timeout) {}

bool PresentationClock::WaitForStart(int64_t first_pts_us, std::stop_token stop) {
    std::unique_lock lock(m_mutex);

    m_ready++;
    m_min_first_pts = std::min(m_min_first_pts, first_pts_us);
    if (!m_first_ready) m_first_ready = Clock::now();
    m_cv.notify_all();

    while (!m_started) {
        auto now = Clock::now();
        auto deadline = *m_first_ready + m_start_timeout;
        if (m_ready >= m_expected || now >= deadline) {
            StartLocked(now);
            break;
        }

        m_cv.wait_until(lock, stop, deadline, [this] { return m_started || m_ready >= m_expected; });
        if (stop.stop_requested()) return false;
    }

    return true;
}

void PresentationClock::StreamFinished() {
    std::lock_guard lock(m_mutex);
    m_expected--;
    m_cv.notify_all();
}

void PresentationClock::StartLocked(Clock::time_point now) {
    m_origin_us = m_min_first_pts;
    m_epoch = now + m_preroll;
    m_utc_epoch_100ns = UtcNow100ns(m_epoch);
    m_started = true;
    m_cv.notify_all();
}

PresentationClock::Clock::time_point PresentationClock::DueTime(int64_t pts_us) const {
    std::lock_guard lock(m_mutex);
    return m_epoch + std::chrono::microseconds(pts_us - m_origin_us);
}

int64_t PresentationClock::Timecode(int64_t pts_us) const {
    std::lock_guard lock(m_mutex);
    return m_utc_epoch_100ns + (pts_us - m_origin_us) * 10;
}

void PresentationClock::Resync(int64_t pts_us, Clock::time_point when) {
    std::lock_guard lock(m_mutex);
    auto offset_us = pts_us - m_origin_us;
    m_epoch = when - std::chrono::microseconds(offset_us);
    m_utc_epoch_100ns = UtcNow100ns(when) - offset_us * 10;
}

bool PresentationClock::Started() const {
    std::lock_guard lock(m_mutex);
    return m_started;
}

} // namespace ndistreamer
