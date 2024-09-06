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
            NDIlib_initialize();
        }

        m_open_instances++;
    }

    Ndi::~Ndi() {
        DEBUG("Ndi destructor called");

        m_open_instances--;

        if(!m_open_instances) {
            NDIlib_destroy();
        }
    }

} // namespace AV::Utils