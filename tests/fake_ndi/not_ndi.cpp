/**
 * @file not_ndi.cpp
 * @brief A shared library that is not an NDI runtime, for loader error tests.
 */

extern "C" __attribute__((visibility("default"))) int not_ndi() {
    return 0;
}
