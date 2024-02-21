/*
 * ndi-streamer
 * ndi.hpp
 *
 * 09-21-2024
 * Matthew Todd Geiger
 */

#pragma once

// Standard includes
#include <cstdlib> // Needed for NDI SDK

// NDI SDK
#include <Processing.NDI.Lib.h>

namespace AV {

    class Ndi {
        public:
            Ndi();
            ~Ndi();

        private:
            static int m_open_instances;
            static bool m_initialized;
    };

} // namespace AV