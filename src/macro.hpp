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
#define DEBUG(str, ...) printf("DEBUG: " str "\n", ##__VA_ARGS__)
#else
#define DEBUG(str, ...)
#endif

#define ERRORTYPE unsigned char
#define FAILED 0
#define SUCCESSFUL 1
