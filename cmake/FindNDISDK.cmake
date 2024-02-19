# Find NDI SDK
# This script find the NDI SDK includes and libraries

if(NOT DEFINED NDI_SDK_DIR)
    set(NDI_SDK_DIR "/opt/ndi/NDI SDK for Linux" CACHE PATH "Path to NDI SDK")
endif()

# Find NDI SDK includes
find_path(NDI_INCLUDE_DIR NAMES Processing.NDI.Lib.h
    HINTS ${NDI_SDK_DIR}/include
    REQUIRED
)

# FIND NDI SDK Libraries
find_library(NDI_LIB NAMES libndi.so
    HINTS ${NDI_SDK_DIR}/lib/x86_64-linux-gnu/
    REQUIRED
)



