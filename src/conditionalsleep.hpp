/**
 * @file conditionalsleep.hpp
 * @brief This file includes utilities for conditionally sleeping.
 * @date 2024-09-12
 * @author Matthew Todd Geiger
 */

#pragma once

#include <condition_variable>
#include <mutex>
#include <functional>

namespace OS::Utils {

class conditional_sleep {
public:
    conditional_sleep() = default;
    ~conditional_sleep() = default;

    bool wait_for(std::unique_lock<std::mutex> &lock, std::function<bool()> condition, std::chrono::milliseconds timeout);
    void notify_one();
    void notify_all();

private:
    std::condition_variable m_cv;
};

} // namespace OS::Utils