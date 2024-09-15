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

namespace AV::Utils {

/**
 * @brief This class just makes sure that the NDI library is initialized and deinitialized properly.
 * Attach this to anything that uses the NDI SDK.
 */
class Ndi {
public:
    /**
     * @brief Construct a new Ndi object
     */
    Ndi();

    /**
     * @brief Destroy the Ndi object
     */
    ~Ndi();

private:
    static int m_open_instances;
};

} // namespace AV::Utils