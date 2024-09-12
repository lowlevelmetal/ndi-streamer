/**
 * @file asyncndisource_test.cpp
 * @brief This file includes tests for working with NDI asynchronously.
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "asyncndisource.hpp"

TEST(AsyncNdiSource, Create) {
	auto [source, err] = AV::Utils::AsyncNdiSource::Create("Test");
	sleep(1);
	EXPECT_EQ(err.code(), 0);
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}