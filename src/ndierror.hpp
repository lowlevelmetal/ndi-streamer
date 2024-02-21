/*
 * ndi-streamer
 * ndierror.hpp
 *
 * 09-21-2024
 * Matthew Todd Geiger
 */

#pragma once

// Standard includes
#include <string>

namespace AV {

    enum class NdiErrorCode {
        NoError,
        SendCreate
    };

    std::string NdiErrorStr(NdiErrorCode &err);

} // namespace AV