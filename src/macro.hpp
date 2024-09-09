/*
 * ndi-streamer
 * macro.hpp
 *
 * 02-19-2024
 * Matthew Todd Geiger
 */

#pragma once

#include <cstdio>
#include <cstdlib>

#define ERROR(str, ...) fprintf(stderr, "ERROR: " str "\n", ##__VA_ARGS__)
#define FATAL(str, ...) { ERROR(str, ##__VA_ARGS__); exit(EXIT_FAILURE); }
#define PRINT(str, ...) printf(str "\n", ##__VA_ARGS__)

#ifdef _DEBUG
#define DEBUG(str, ...) printf("DEBUG: " str "\n", ##__VA_ARGS__); fflush(stdout)
#define PRINT_FFMPEG_ERR(x) { char errbuf[AV_ERROR_MAX_STRING_SIZE]; av_strerror(x, errbuf, sizeof(errbuf)); DEBUG("FFmpeg error: %s", errbuf); }
#else
#define DEBUG(str, ...)
#define PRINT_FFMPEG_ERR(x)
#endif

#define ERRORTYPE unsigned char
#define FAILED 0
#define SUCCESSFUL 1
