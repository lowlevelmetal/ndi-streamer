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

// This class is only used to handle initializing and deinitializing the NDI sdk
namespace AV {

    class Ndi {
        public:
            Ndi();
            ~Ndi();

        private:
            static int m_open_instances;
    };

} // namespace AV