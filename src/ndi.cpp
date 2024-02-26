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
    int Ndi::m_open_instances = 0;

    Ndi::Ndi() {
        if(!m_open_instances) {
            NDIlib_initialize();
        }

        m_open_instances++;
    }

    Ndi::~Ndi() {
        m_open_instances--;

        if(!m_open_instances) {
            NDIlib_destroy();
        }
    }

} // namespace AV