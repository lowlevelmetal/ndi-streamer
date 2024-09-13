/**
 * @file asyncndisource.cpp
 * @brief This file includes utilities for working with NDI asynchronously.
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#include "asyncndisource.hpp"
#include "macro.hpp"

namespace AV::Utils {

AsyncNdiSource::AsyncNdiSource(const std::string &ndi_source_name) : m_ndi_source_name(ndi_source_name) {
    auto err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw AvException(err);
    }
}

AsyncNdiSource::~AsyncNdiSource() {
    DEBUG("AsyncNdiSource destructor called");
    
    // Stop the threads
    m_shutdown = true;
    
    if(m_audio_thread.joinable()) {
        m_audio_thread.join();
        DEBUG("Audio thread joined");
    }

    if(m_video_thread.joinable()) {
        m_video_thread.join();
        DEBUG("Video thread joined");
    }

    // Shutdown ndi source
    if(m_ndi_send_instance) {
        NDIlib_send_destroy(m_ndi_send_instance);
        DEBUG("NDI send instance destroyed");
    }
}


AsyncNdiSourceResult AsyncNdiSource::Create(const std::string &ndi_source_name) {
    AvException err;

    try {
        return {std::make_unique<AsyncNdiSource>(new AsyncNdiSource(ndi_source_name)), AvException(AvError::NOERROR)};
    } catch (const AvException &e) {
        err = e;
        DEBUG("Error creating AsyncNdiSource: %s", e.what());
    }

    return {nullptr, err};
}

AvError AsyncNdiSource::m_Initialize() {

}

} // namespace AV::Utils