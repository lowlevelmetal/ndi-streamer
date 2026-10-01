/**
 * @file test_queue.cpp
 * @brief BoundedQueue behaviour.
 */

#include "util/queue.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using ndistreamer::BoundedQueue;
using namespace std::chrono_literals;

TEST(BoundedQueue, DeliversInOrder) {
    BoundedQueue<int> queue(4);
    std::stop_source stop;
    for (int i = 0; i < 4; i++) ASSERT_TRUE(queue.Push(i, stop.get_token()));
    EXPECT_EQ(queue.Size(), 4u);
    for (int i = 0; i < 4; i++) EXPECT_EQ(queue.Pop(stop.get_token()), i);
}

TEST(BoundedQueue, CloseDrainsRemainingItems) {
    BoundedQueue<int> queue(4);
    std::stop_source stop;
    queue.Push(1, stop.get_token());
    queue.Push(2, stop.get_token());
    queue.Close();

    EXPECT_FALSE(queue.Push(3, stop.get_token()));
    EXPECT_EQ(queue.Pop(stop.get_token()), 1);
    EXPECT_EQ(queue.Pop(stop.get_token()), 2);
    EXPECT_EQ(queue.Pop(stop.get_token()), std::nullopt);
}

TEST(BoundedQueue, PushWaitsForSpace) {
    BoundedQueue<int> queue(1);
    std::stop_source stop;
    queue.Push(1, stop.get_token());

    std::jthread consumer([&] {
        std::this_thread::sleep_for(50ms);
        queue.Pop(stop.get_token());
    });

    auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(queue.Push(2, stop.get_token()));
    EXPECT_GE(std::chrono::steady_clock::now() - start, 40ms);
    EXPECT_EQ(queue.Pop(stop.get_token()), 2);
}

TEST(BoundedQueue, StopUnblocksWaiters) {
    BoundedQueue<int> empty(1);
    BoundedQueue<int> full(1);
    std::stop_source stop;
    full.Push(1, stop.get_token());

    std::jthread stopper([&] {
        std::this_thread::sleep_for(30ms);
        stop.request_stop();
    });

    EXPECT_EQ(empty.Pop(stop.get_token()), std::nullopt);
    EXPECT_FALSE(full.Push(2, stop.get_token()));
}

TEST(BoundedQueue, CloseWakesBlockedConsumer) {
    BoundedQueue<int> queue(1);
    std::stop_source stop;
    std::jthread closer([&] {
        std::this_thread::sleep_for(30ms);
        queue.Close();
    });
    EXPECT_EQ(queue.Pop(stop.get_token()), std::nullopt);
}
