/**
 * @file decoder.hpp
 * @brief This file includes utilities for decoding media files.
 * @date 2024-09-06
 * @author Matthew Todd Geiger
 */

#pragma once

#include <string>

namespace AV::Utils {

class Decoder {
private:
    Decoder(const std::string &path);

public:
    ~Decoder();


};

} // namespace AV::Utils