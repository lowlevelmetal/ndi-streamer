/**
 * @file main.cpp
 * @brief ndistreamer entry point.
 */

#include "options.hpp"
#include "streamer.hpp"
#include "util/log.hpp"
#include "version.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stop_token>
#include <thread>

#include <pthread.h>
#include <signal.h>

using namespace ndistreamer;

namespace {

std::atomic<bool> g_interrupted{false};

/**
 * Termination signals are blocked in every thread and received here, where it is safe to request a
 * stop. A second signal exits immediately in case shutdown is stuck.
 */
void StartSignalThread(std::stop_source stop) {
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);

    std::thread([signals, stop]() mutable {
        pthread_setname_np(pthread_self(), "nds-signals");
        for (int count = 0;;) {
            int signal = 0;
            if (sigwait(&signals, &signal) != 0) continue;
            if (++count == 1) {
                log::Info("received {}; stopping (press Ctrl+C again to force)", strsignal(signal));
                g_interrupted = true;
                stop.request_stop();
            } else {
                log::Warn("forced exit");
                std::_Exit(130);
            }
        }
    }).detach();
}

} // namespace

int main(int argc, char *argv[]) {
    Options options;
    std::string message;
    switch (ParseOptions(argc, argv, options, message)) {
    case ParseOutcome::ExitSuccess:
        std::printf("%s\n", message.c_str());
        return EXIT_SUCCESS;
    case ParseOutcome::ExitError:
        std::fprintf(stderr, "%s\n", message.c_str());
        return 2;
    case ParseOutcome::Run:
        break;
    }

    log::SetLevel(options.log_level);
    log::InstallFFmpegBridge();
    std::signal(SIGPIPE, SIG_IGN);

    std::stop_source stop;
    StartSignalThread(stop);

    log::Info("ndistreamer {}", NDISTREAMER_VERSION);

    try {
        Streamer streamer(options, stop);
        streamer.Start();
        log::Info("streaming as NDI source \"{}\"", streamer.SourceName());

        auto interval = std::chrono::seconds(options.stats_interval);
        auto next_stats = std::chrono::steady_clock::now() + interval;
        while (!streamer.WaitFor(std::chrono::milliseconds(100))) {
            if (options.stats_interval > 0 && std::chrono::steady_clock::now() >= next_stats) {
                streamer.LogStats();
                next_stats += interval;
            }
        }

        streamer.Wait();
        if (options.stats_interval > 0) streamer.LogStats();
        log::Info("{}", g_interrupted ? "stopped" : "end of input reached");
    } catch (const std::exception &error) {
        if (g_interrupted) {
            log::Info("stopped");
            return EXIT_SUCCESS;
        }
        log::Error("{}", error.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
