/*
 * ndi-streamer
 * ndi.cpp
 *
 * 09-21-2024
 * Matthew Todd Geiger
 */

// local includes
#include "ndi.hpp"

namespace AV {
    bool Ndi::m_initialized = false;
    int Ndi::m_open_instances = 0;

    Ndi::Ndi() {
        if(!m_initialized) {
            NDIlib_initialize();
            m_initialized = true;
        }

        m_open_instances++;
    }

    Ndi::~Ndi() {
        m_open_instances--;

        if(m_open_instances <= 0) {
            NDIlib_destroy();
        }
    }

} // namespace AV