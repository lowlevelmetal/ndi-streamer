/**
 * @file test_clock.cpp
 * @brief PresentationClock start-up and timestamp mapping.
 */

#include "util/clock.hpp"

#include <gtest/gtest.h>

#include <thread>

using ndistreamer::PresentationClock;
using namespace std::chrono_literals;

TEST(PresentationClock, StartsWhenAllStreamsAreReady) {
    PresentationClock clock(2, 0ms, 5s);
    std::stop_source stop;

    auto start = std::chrono::steady_clock::now();
    std::jthread other([&] { EXPECT_TRUE(clock.WaitForStart(40000, stop.get_token())); });
    EXPECT_TRUE(clock.WaitForStart(0, stop.get_token()));
    other.join();

    EXPECT_TRUE(clock.Started());
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}

TEST(PresentationClock, EarliestTimestampIsTheOrigin) {
    PresentationClock clock(2, 0ms, 5s);
    std::stop_source stop;
    std::jthread other([&] { clock.WaitForStart(500000, stop.get_token()); });
    clock.WaitForStart(100000, stop.get_token());
    other.join();

    // The later stream keeps its 400 ms offset.
    EXPECT_EQ(clock.DueTime(500000) - clock.DueTime(100000), 400ms);
    EXPECT_LE(clock.DueTime(100000), std::chrono::steady_clock::now());
}

TEST(PresentationClock, StartsAfterTimeoutWhenAStreamIsSlow) {
    PresentationClock clock(2, 0ms, 50ms);
    std::stop_source stop;

    auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(clock.WaitForStart(0, stop.get_token()));
    auto waited = std::chrono::steady_clock::now() - start;
    EXPECT_GE(waited, 45ms);
    EXPECT_LT(waited, 1s);
}

TEST(PresentationClock, FinishedStreamIsNotAwaited) {
    PresentationClock clock(2, 0ms, 10s);
    std::stop_source stop;
    std::jthread finisher([&] {
        std::this_thread::sleep_for(20ms);
        clock.StreamFinished();
    });

    auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(clock.WaitForStart(0, stop.get_token()));
    EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
}

TEST(PresentationClock, StopInterruptsWaiting) {
    PresentationClock clock(2, 0ms, 10s);
    std::stop_source stop;
    std::jthread stopper([&] {
        std::this_thread::sleep_for(20ms);
        stop.request_stop();
    });
    EXPECT_FALSE(clock.WaitForStart(0, stop.get_token()));
}

TEST(PresentationClock, PrerollDelaysFirstDeadline) {
    PresentationClock clock(1, 100ms, 1s);
    std::stop_source stop;
    auto before = std::chrono::steady_clock::now();
    clock.WaitForStart(0, stop.get_token());
    EXPECT_GE(clock.DueTime(0) - before, 100ms);
}

TEST(PresentationClock, TimecodesFollowTimestamps) {
    PresentationClock clock(1, 0ms, 1s);
    std::stop_source stop;
    clock.WaitForStart(0, stop.get_token());

    // 100 ns units.
    EXPECT_EQ(clock.Timecode(1000000) - clock.Timecode(0), 10000000);

    // UTC based: within a second of now.
    auto utc_now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count() /
                   100;
    EXPECT_NEAR(static_cast<double>(clock.Timecode(0)), static_cast<double>(utc_now), 1e7);
}

TEST(PresentationClock, ResyncReanchorsSchedule) {
    PresentationClock clock(1, 0ms, 1s);
    std::stop_source stop;
    clock.WaitForStart(0, stop.get_token());

    auto when = std::chrono::steady_clock::now() + 2s;
    clock.Resync(5000000, when);
    EXPECT_EQ(clock.DueTime(5000000), when);
    EXPECT_EQ(clock.DueTime(5040000) - when, 40ms);
}
