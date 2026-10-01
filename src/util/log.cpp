/**
 * @file log.cpp
 * @brief Thread-safe, leveled logging to stderr.
 */

#include "util/log.hpp"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>

extern "C" {
#include <libavutil/log.h>
}

namespace ndistreamer::log {

namespace {

std::atomic<Level> g_level{Level::Info};
std::mutex g_write_mutex;

const char *LevelName(Level level) {
    switch (level) {
    case Level::Error:
        return "error";
    case Level::Warn:
        return "warn ";
    case Level::Info:
        return "info ";
    case Level::Debug:
        return "debug";
    case Level::Trace:
        return "trace";
    }
    return "?????";
}

// FFmpeg reports many recoverable conditions (corrupt packets, missing references after a seek) as
// errors. We handle failures ourselves, so its messages are demoted by one level.
Level FromFFmpegLevel(int level) {
    if (level <= AV_LOG_ERROR) return Level::Warn;
    if (level <= AV_LOG_WARNING) return Level::Debug;
    return Level::Trace;
}

int ToFFmpegLevel(Level level) {
    switch (level) {
    case Level::Error:
    case Level::Warn:
    case Level::Info:
        return AV_LOG_ERROR;
    case Level::Debug:
        return AV_LOG_WARNING;
    case Level::Trace:
        return AV_LOG_VERBOSE;
    }
    return AV_LOG_ERROR;
}

void FFmpegCallback(void *avcl, int level, const char *fmt, va_list args) {
    if (level > av_log_get_level()) return;

    // FFmpeg may build one line out of several calls, so buffer until we see a newline.
    thread_local std::string pending;
    thread_local int print_prefix = 1;

    char line[1024];
    int prefix = print_prefix;
    va_list copy;
    va_copy(copy, args);
    int length = av_log_format_line2(avcl, level, fmt, copy, line, sizeof(line), &print_prefix);
    va_end(copy);

    if (length >= static_cast<int>(sizeof(line))) {
        // Too long for the stack buffer: format again into one that fits, so the newline is kept.
        std::string long_line(static_cast<size_t>(length) + 1, '\0');
        print_prefix = prefix;
        av_log_format_line2(avcl, level, fmt, args, long_line.data(), length + 1, &print_prefix);
        long_line.resize(static_cast<size_t>(length));
        pending += long_line;
    } else if (length > 0) {
        pending += line;
    }

    if (pending.empty() || pending.back() != '\n') return;

    pending.pop_back();
    if (!pending.empty()) {
        Level ours = FromFFmpegLevel(level);
        if (Enabled(ours)) Write(ours, "ffmpeg: " + pending);
    }
    pending.clear();
}

} // namespace

void SetLevel(Level level) {
    g_level.store(level, std::memory_order_relaxed);
    av_log_set_level(ToFFmpegLevel(level));
}

Level GetLevel() {
    return g_level.load(std::memory_order_relaxed);
}

bool Enabled(Level level) {
    return level <= GetLevel();
}

void Write(Level level, std::string_view message) {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto millis = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    std::time_t seconds = system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&seconds, &local);

    char stamp[16];
    std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &local);

    std::lock_guard lock(g_write_mutex);
    std::fprintf(stderr, "[%s.%03lld] [%s] %.*s\n", stamp, static_cast<long long>(millis), LevelName(level),
                 static_cast<int>(message.size()), message.data());
}

void InstallFFmpegBridge() {
    av_log_set_level(ToFFmpegLevel(GetLevel()));
    av_log_set_callback(FFmpegCallback);
}

} // namespace ndistreamer::log
