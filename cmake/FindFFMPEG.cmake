# Find FFMPEG development libraries

find_package(PkgConfig)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libavdevice
    libavfilter
)

if (FFMPEG_FOUND)
    message(STATUS "FFmpeg libraries found.")
    message(STATUS "FFmpeg include directories: ${FFMPEG_INCLUDE_DIRS}")
    message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")
else()
    message(FATAL_ERROR "FFmpeg libraries not found")
endif()

