/**
 * @file mtavserver_test.cpp
 * @brief This file includes tests for the mtavserver library
 * @date 2024-09-12
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "mtavserver.hpp"

TEST(MtAvServer, Create) {
	auto [ndiavserver, err] = AV::Utils::MtAvServer::Create("test", "testcontent/rickroll.mp4");
	EXPECT_EQ(err.code(), (int)AV::Utils::AvError::NOERROR);
}

TEST(MtAvServer, Start) {
	auto [ndiavserver, err] = AV::Utils::MtAvServer::Create("test", "testcontent/rickroll.mp4");
	ndiavserver->start();
	EXPECT_EQ(err.code(), (int)AV::Utils::AvError::NOERROR);
}

TEST(MtAvServer, ProcessNextFrame) {
	auto [ndiavserver, err] = AV::Utils::MtAvServer::Create("test", "testcontent/rickroll.mp4");
	ndiavserver->start();
	auto process_err = ndiavserver->ProcessNextFrame();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	EXPECT_EQ(process_err.code(), (int)AV::Utils::AvError::NOERROR);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}