/**
 * @file ndi.cpp
 * @brief This file includes utilities for working with NDI.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 **/

// local includes
#include "ndi.hpp"
#include "macro.hpp"

namespace AV::Utils {
    int Ndi::m_open_instances = 0;

    Ndi::Ndi() {
        DEBUG("Ndi constructor called");

        if(!m_open_instances) {
            DEBUG("NDI library has not been initialized yet, initializing now");
            NDIlib_initialize();
        }

        m_open_instances++;
    }

    Ndi::~Ndi() {
        DEBUG("Ndi destructor called");

        m_open_instances--;

        if(!m_open_instances) {
            DEBUG("No more NDI instances open, deinitializing NDI library");
            NDIlib_destroy();
        }
    }

} // namespace AV::Utils