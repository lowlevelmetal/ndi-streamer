/**
 * @file test_runtime.cpp
 * @brief Locating and loading the NDI runtime.
 */

#include "ndi/runtime.hpp"
#include "ndi/sender.hpp"
#include "support/test_support.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

using namespace ndistreamer;

namespace {

std::string LoadError(const std::string &hint) {
    try {
        ndi::Runtime::Load(hint);
    } catch (const std::exception &error) {
        return error.what();
    }
    return {};
}

} // namespace

TEST(Runtime, LoadsLibraryFile) {
    auto runtime = ndi::Runtime::Load(FAKE_NDI_LIBRARY);
    EXPECT_EQ(runtime->Version(), "FAKE NDI 6.0.0");
    EXPECT_NE(runtime->api().send_get_source_name, nullptr);
}

TEST(Runtime, LoadsFromDirectory) {
    auto runtime = ndi::Runtime::Load(FAKE_NDI_DIR);
    EXPECT_EQ(runtime->Version(), "FAKE NDI 6.0.0");
    EXPECT_NE(runtime->Path().find("libndi.so.6"), std::string::npos) << runtime->Path();
}

TEST(Runtime, HonoursRuntimeDirectoryVariable) {
    setenv("NDI_RUNTIME_DIR_V6", FAKE_NDI_DIR, 1);
    auto runtime = ndi::Runtime::Load("");
    unsetenv("NDI_RUNTIME_DIR_V6");
    EXPECT_EQ(runtime->Version(), "FAKE NDI 6.0.0");
}

TEST(Runtime, ExplainsMissingLibrary) {
    std::string error = LoadError("/nonexistent/libndi.so.6");
    EXPECT_NE(error.find("cannot load the NDI runtime from '/nonexistent/libndi.so.6'"), std::string::npos) << error;
    EXPECT_NE(error.find("--ndi-lib"), std::string::npos) << error;
}

TEST(Runtime, RejectsLibrariesThatAreNotNdi) {
    std::string error = LoadError(NOT_NDI_LIBRARY);
    EXPECT_NE(error.find("not a usable NDI runtime"), std::string::npos) << error;
}

TEST(Runtime, RejectsUnsupportedCpu) {
    setenv("FAKE_NDI_UNSUPPORTED_CPU", "1", 1);
    std::string error = LoadError(FAKE_NDI_LIBRARY);
    unsetenv("FAKE_NDI_UNSUPPORTED_CPU");
    EXPECT_NE(error.find("CPU is not supported"), std::string::npos) << error;
}

TEST(Sender, CreatesUnclockedSourceWithProductMetadata) {
    test::FakeNdiLog log;
    {
        ndi::Sender sender(ndi::Runtime::Load(FAKE_NDI_LIBRARY), {"Studio A", "cameras,studio"});
        EXPECT_EQ(sender.SourceName(), "FAKEHOST (Studio A)");
        EXPECT_EQ(sender.Connections(), 1);
    }
    test::FakeLog sent = log.Read();
    EXPECT_EQ(sent.create, "Studio A|cameras,studio|0|0");
    ASSERT_EQ(sent.metadata.size(), 1u);
    EXPECT_NE(sent.metadata[0].find("short_name=\"ndistreamer\""), std::string::npos);
    EXPECT_TRUE(sent.destroyed);
}
