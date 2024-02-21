/*
 * ndi-streamer
 * ndisource.hpp
 *
 * 09-21-2024
 * Matthew Todd Geiger
 */

#pragma once

// Standard includes
#include <string>

// Local includes
#include "ndi.hpp"

namespace AV {

    class NdiSource : public Ndi {
        public:
            NdiSource(std::string &ndi_source_name);
            ~NdiSource();

        private:
            NDIlib_send_instance_t m_pNDI_send;
            std::string m_ndi_source_name;
            bool m_initialized = false;

            void m_Initialize();

    };

} // namespace AV