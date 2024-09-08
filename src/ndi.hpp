/**
 * @file ndi.hpp
 * @brief This file includes utilities for working with NDI.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

// Standard includes
#include <cstdlib> // Needed for NDI SDK
#include <string>

// NDI SDK
#include <Processing.NDI.Lib.h>

// This class is only used to handle initializing and deinitializing the NDI sdk
namespace AV::Utils {

    class Ndi {
        public:
            Ndi();
            ~Ndi();

        private:
            static int m_open_instances;
    };

} // namespace AV::Utils