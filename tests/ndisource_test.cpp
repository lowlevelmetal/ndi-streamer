/**
 * @file ndisource_test.cpp
 * @brief This file includes tests for the NdiSource class.
 * @date 2024-09-08
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "ndisource.hpp"

TEST(NdiSourceTest, CreateNdiSourceSimple) {
    auto [ndisource, ndisource_err] = AV::Utils::NdiSource::Create("Test NDI Source");
    EXPECT_EQ(ndisource_err.code(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}