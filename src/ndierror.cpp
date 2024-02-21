/*
 * ndi-streamer
 * ndierror.cpp
 *
 * 09-21-2024
 * Matthew Todd Geiger
 */

// local includes
#include "ndierror.hpp"

namespace AV {
    std::string NdiErrorStr(NdiErrorCode &err) {

        switch(err) {
            case NdiErrorCode::NoError:
                return std::string("No ndi error");
            case NdiErrorCode::SendCreate:
                return std::string("Failed to create NDI send instance");
            case NdiErrorCode::UnsupportedPixFormat:
                return std::string("Unsupported pixel format");
        }

        return std::string("Invalid NDI error code");
    }

}