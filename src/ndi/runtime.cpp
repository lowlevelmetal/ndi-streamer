/**
 * @file runtime.cpp
 * @brief Locates and loads the NDI runtime library (libndi.so) at run time.
 */

#include "ndi/runtime.hpp"
#include "util/log.hpp"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <vector>

#include <dlfcn.h>

namespace ndistreamer::ndi {

namespace {

constexpr const char *kLibraryNames[] = {"libndi.so.6", "libndi.so.5", "libndi.so"};

// Directory names used by the NDI SDK for Linux for this architecture.
#if defined(__x86_64__)
constexpr const char *kSdkArch = "x86_64-linux-gnu";
#elif defined(__aarch64__)
constexpr const char *kSdkArch = "aarch64-rpi4-linux-gnueabi";
#elif defined(__i386__)
constexpr const char *kSdkArch = "i686-linux-gnu";
#elif defined(__arm__)
constexpr const char *kSdkArch = "arm-rpi4-linux-gnueabihf";
#else
constexpr const char *kSdkArch = "";
#endif

void AddDirectory(std::vector<std::string> &candidates, const std::filesystem::path &dir) {
    for (const char *name : kLibraryNames) candidates.push_back((dir / name).string());
}

std::vector<std::string> Candidates(const std::string &hint) {
    std::vector<std::string> candidates;

    if (!hint.empty()) {
        if (std::filesystem::is_directory(hint)) {
            AddDirectory(candidates, hint);
        } else {
            candidates.push_back(hint);
        }
        return candidates; // an explicit choice is never silently replaced
    }

    for (const char *var : {"NDI_RUNTIME_DIR_V6", "NDI_RUNTIME_DIR_V5"}) {
        if (const char *dir = std::getenv(var); dir && *dir) AddDirectory(candidates, dir);
    }

    for (const char *name : kLibraryNames) candidates.push_back(name); // system search path

    std::filesystem::path sdk_lib = std::filesystem::path("lib") / kSdkArch;
    AddDirectory(candidates, "/usr/local/lib");
    AddDirectory(candidates, std::filesystem::path("/opt/ndi") / sdk_lib);
    AddDirectory(candidates, std::filesystem::path("/opt/ndi/NDI SDK for Linux") / sdk_lib);
    AddDirectory(candidates, "/opt/ndi/lib");
    return candidates;
}

template <typename Fn>
void Resolve(void *handle, const char *symbol, Fn &fn, bool required, const std::string &path) {
    fn = reinterpret_cast<Fn>(dlsym(handle, symbol));
    if (!fn && required) {
        throw std::runtime_error(std::format("'{}' is not a usable NDI runtime: missing {}", path, symbol));
    }
}

} // namespace

std::shared_ptr<Runtime> Runtime::Load(const std::string &hint) {
    std::string last_error;

    for (const std::string &candidate : Candidates(hint)) {
        void *handle = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            const char *error = dlerror();
            log::Trace("NDI runtime not loaded from {}: {}", candidate, error ? error : "unknown error");
            // Report why an existing file failed to load; "not found" errors are expected.
            if (error && (!hint.empty() || std::filesystem::exists(candidate))) last_error = error;
            continue;
        }

        // Report the real path rather than a bare soname.
        std::string path = candidate;
        if (char resolved[4096]; dlinfo(handle, RTLD_DI_ORIGIN, resolved) == 0) {
            path = (std::filesystem::path(resolved) / std::filesystem::path(candidate).filename()).string();
        }

        return std::shared_ptr<Runtime>(new Runtime(handle, path));
    }

    std::string message = hint.empty() ? "the NDI runtime (libndi.so) was not found"
                                       : std::format("cannot load the NDI runtime from '{}'", hint);
    if (!last_error.empty()) message += std::format(" ({})", last_error);
    message += ". Install the NDI SDK or NDI Tools from https://ndi.video, then point --ndi-lib or "
               "NDI_RUNTIME_DIR_V6 at the directory containing libndi.so.6";
    throw std::runtime_error(message);
}

Runtime::Runtime(void *handle, std::string path) : m_handle(handle), m_path(std::move(path)) {
    Resolve(m_handle, "NDIlib_initialize", m_api.initialize, true, m_path);
    Resolve(m_handle, "NDIlib_destroy", m_api.destroy, true, m_path);
    Resolve(m_handle, "NDIlib_version", m_api.version, true, m_path);
    Resolve(m_handle, "NDIlib_is_supported_CPU", m_api.is_supported_cpu, true, m_path);
    Resolve(m_handle, "NDIlib_send_create", m_api.send_create, true, m_path);
    Resolve(m_handle, "NDIlib_send_destroy", m_api.send_destroy, true, m_path);
    Resolve(m_handle, "NDIlib_send_send_video_async_v2", m_api.send_video_async_v2, true, m_path);
    Resolve(m_handle, "NDIlib_send_send_audio_v2", m_api.send_audio_v2, true, m_path);
    Resolve(m_handle, "NDIlib_send_get_no_connections", m_api.send_get_no_connections, true, m_path);
    Resolve(m_handle, "NDIlib_send_get_source_name", m_api.send_get_source_name, false, m_path);
    Resolve(m_handle, "NDIlib_send_add_connection_metadata", m_api.send_add_connection_metadata, false, m_path);

    if (!m_api.is_supported_cpu()) {
        throw std::runtime_error("this CPU is not supported by the NDI runtime (SSE4.2 or NEON is required)");
    }
    if (!m_api.initialize()) {
        throw std::runtime_error("the NDI runtime failed to initialize");
    }
}

Runtime::~Runtime() {
    m_api.destroy();
    // The library stays mapped: unloading it while its worker threads wind down is not safe.
}

std::string Runtime::Version() const {
    const char *version = m_api.version();
    return version ? version : "unknown";
}

} // namespace ndistreamer::ndi
