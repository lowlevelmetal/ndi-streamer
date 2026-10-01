/**
 * @file log.hpp
 * @brief Thread-safe, leveled logging to stderr.
 */

#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace ndistreamer::log {

enum class Level {
    Error = 0,
    Warn,
    Info,
    Debug,
    Trace,
};

void SetLevel(Level level);
Level GetLevel();
bool Enabled(Level level);

/**
 * @brief Write a pre-formatted message. Prefer the typed helpers below.
 */
void Write(Level level, std::string_view message);

/**
 * @brief Route FFmpeg's internal log output through this logger.
 * FFmpeg verbosity follows the current log level, so call it after SetLevel().
 */
void InstallFFmpegBridge();

template <typename... Args>
void Error(std::format_string<Args...> fmt, Args &&...args) {
    if (Enabled(Level::Error)) Write(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void Warn(std::format_string<Args...> fmt, Args &&...args) {
    if (Enabled(Level::Warn)) Write(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void Info(std::format_string<Args...> fmt, Args &&...args) {
    if (Enabled(Level::Info)) Write(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void Debug(std::format_string<Args...> fmt, Args &&...args) {
    if (Enabled(Level::Debug)) Write(Level::Debug, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void Trace(std::format_string<Args...> fmt, Args &&...args) {
    if (Enabled(Level::Trace)) Write(Level::Trace, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace ndistreamer::log
