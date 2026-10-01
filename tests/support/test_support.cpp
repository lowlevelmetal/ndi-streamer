/**
 * @file test_support.cpp
 * @brief Shared helpers: generated media, the fake NDI runtime and its log.
 */

#include "support/test_support.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <unistd.h>

namespace test {

std::string MediaPath(const std::string &name) {
    return std::string(TEST_MEDIA_DIR) + "/" + name;
}

int64_t FakeLog::TotalAudioSamples() const {
    int64_t total = 0;
    for (const auto &event : audio) total += event.samples;
    return total;
}

FakeNdiLog::FakeNdiLog() {
    static std::atomic<int> counter{0};
    m_path = std::filesystem::temp_directory_path() /
             ("ndistreamer-test-" + std::to_string(getpid()) + "-" + std::to_string(counter++) + ".log");
    setenv("FAKE_NDI_LOG", m_path.c_str(), 1);
}

FakeNdiLog::~FakeNdiLog() {
    unsetenv("FAKE_NDI_LOG");
    std::error_code ignored;
    std::filesystem::remove(m_path, ignored);
}

FakeLog FakeNdiLog::Read() const {
    FakeLog log;
    std::ifstream file(m_path);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream in(line);
        std::string kind;
        in >> kind;
        if (kind == "create") {
            log.create = line.substr(7);
        } else if (kind == "metadata") {
            log.metadata.push_back(line.substr(9));
        } else if (kind == "video") {
            VideoEvent e;
            in >> e.ns >> e.width >> e.height >> e.fourcc >> e.fps_num >> e.fps_den >> e.aspect >> e.format_type >>
                e.timecode >> e.stride;
            log.video.push_back(e);
        } else if (kind == "audio") {
            AudioEvent e;
            in >> e.ns >> e.sample_rate >> e.channels >> e.samples >> e.stride_bytes >> e.timecode >> e.rms;
            log.audio.push_back(e);
        } else if (kind == "flush") {
            log.flushes++;
        } else if (kind == "destroy") {
            log.destroyed = true;
        }
    }
    return log;
}

ndistreamer::Options MakeOptions(const std::string &media) {
    ndistreamer::Options options;
    options.input.url = MediaPath(media);
    options.ndi_library = FAKE_NDI_LIBRARY;
    options.sender.name = "test";
    return options;
}

FakeLog Stream(const ndistreamer::Options &options, std::optional<std::chrono::milliseconds> stop_after) {
    FakeNdiLog log;
    {
        std::stop_source stop;
        ndistreamer::Streamer streamer(options, stop);
        streamer.Start();

        auto deadline = std::chrono::steady_clock::now() + stop_after.value_or(std::chrono::seconds(60));
        while (!streamer.WaitFor(std::chrono::milliseconds(10))) {
            if (std::chrono::steady_clock::now() >= deadline) stop.request_stop();
        }
        streamer.Wait();
    } // the sender closes the log when it is destroyed
    return log.Read();
}

double Median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

} // namespace test
