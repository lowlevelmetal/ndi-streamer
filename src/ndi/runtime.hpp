/**
 * @file runtime.hpp
 * @brief Locates and loads the NDI runtime library (libndi.so) at run time.
 */

#pragma once

#include "ndi/abi.hpp"

#include <memory>
#include <string>

namespace ndistreamer::ndi {

/**
 * @brief A loaded and initialized NDI runtime.
 *
 * Search order: the explicit path given by the user (file or directory), the NDI_RUNTIME_DIR_V6 and
 * NDI_RUNTIME_DIR_V5 environment variables, the system library path, then common SDK install
 * locations such as /opt/ndi.
 */
class Runtime {
public:
    struct Api {
        abi::InitializeFn initialize = nullptr;
        abi::DestroyFn destroy = nullptr;
        abi::VersionFn version = nullptr;
        abi::IsSupportedCpuFn is_supported_cpu = nullptr;
        abi::SendCreateFn send_create = nullptr;
        abi::SendDestroyFn send_destroy = nullptr;
        abi::SendVideoAsyncV2Fn send_video_async_v2 = nullptr;
        abi::SendAudioV2Fn send_audio_v2 = nullptr;
        abi::SendGetNoConnectionsFn send_get_no_connections = nullptr;
        // Optional; may be null on older runtimes.
        abi::SendGetSourceNameFn send_get_source_name = nullptr;
        abi::SendAddConnectionMetadataFn send_add_connection_metadata = nullptr;
    };

    /**
     * @brief Load the runtime. Throws std::runtime_error with guidance if it cannot be found or used.
     * @param hint a library file or a directory containing it; empty to search.
     */
    static std::shared_ptr<Runtime> Load(const std::string &hint);

    ~Runtime();

    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;

    const Api &api() const { return m_api; }
    const std::string &Path() const { return m_path; }
    std::string Version() const;

private:
    Runtime(void *handle, std::string path);

    void *m_handle;
    std::string m_path;
    Api m_api;
};

} // namespace ndistreamer::ndi
