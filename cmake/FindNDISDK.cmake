# FindNDISDK
# ----------
# Locates the NDI SDK for Linux. ndistreamer loads the NDI runtime at run time, so the SDK is only
# used by the test suite (ABI verification and end-to-end tests).
#
# Hints:
#   NDI_SDK_DIR   root of the extracted SDK ("NDI SDK for Linux")
#
# Result variables:
#   NDISDK_FOUND, NDISDK_INCLUDE_DIR, NDISDK_LIBRARY, NDISDK_BIN_DIR
#
# Imported target:
#   NDISDK::NDISDK

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(_ndi_arch x86_64-linux-gnu)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(_ndi_arch aarch64-rpi4-linux-gnueabi)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
    set(_ndi_arch i686-linux-gnu)
else()
    set(_ndi_arch arm-rpi4-linux-gnueabihf)
endif()

set(_ndi_roots ${NDI_SDK_DIR} $ENV{NDI_SDK_DIR} "/opt/ndi/NDI SDK for Linux" "/opt/ndi")

find_path(NDISDK_INCLUDE_DIR
    NAMES Processing.NDI.Lib.h
    HINTS ${_ndi_roots}
    PATH_SUFFIXES include)

find_library(NDISDK_LIBRARY
    NAMES ndi
    HINTS ${_ndi_roots}
    PATH_SUFFIXES lib/${_ndi_arch} lib)

find_path(NDISDK_BIN_DIR
    NAMES ndi-discovery-server
    HINTS ${_ndi_roots}
    PATH_SUFFIXES bin/${_ndi_arch} bin)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NDISDK REQUIRED_VARS NDISDK_INCLUDE_DIR NDISDK_LIBRARY)

if(NDISDK_FOUND AND NOT TARGET NDISDK::NDISDK)
    add_library(NDISDK::NDISDK UNKNOWN IMPORTED)
    set_target_properties(NDISDK::NDISDK PROPERTIES
        IMPORTED_LOCATION "${NDISDK_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${NDISDK_INCLUDE_DIR}")
endif()

mark_as_advanced(NDISDK_INCLUDE_DIR NDISDK_LIBRARY NDISDK_BIN_DIR)
unset(_ndi_arch)
unset(_ndi_roots)
