/**
 * @file conditionalsleep.cpp
 * @brief This file includes utilities for conditionally sleeping.
 * @date 2024-09-12
 * @author Matthew Todd Geiger
 */

#include "conditionalsleep.hpp"

namespace OS::Utils {

bool conditional_sleep::wait_for(std::unique_lock<std::mutex> &lock, std::function<bool()> condition, std::chrono::milliseconds timeout) {
    lock.lock();
    if (!condition()) {
        return m_cv.wait_for(lock, timeout, condition);
    }

    return false;
}

void conditional_sleep::notify_one() {
    m_cv.notify_one();
}

void conditional_sleep::notify_all() {
    m_cv.notify_all();
}

} // namespace OS::Utils