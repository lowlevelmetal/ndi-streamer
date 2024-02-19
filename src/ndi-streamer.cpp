/*
 * ndi-streamer
 * ndi-streamer.cpp
 *
 * 02-19-2024
 * Matthew Todd Geiger
 */

// Standard library
#include <cstdlib>
#include <string>

// POSIX includes
#include <unistd.h>

// NDI SDK
#include <Processing.NDI.Lib.h>

// Local includes
#include "macro.hpp"

// FFMPEG Includes
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/time.h>
    #include <libswscale/swscale.h>
}

typedef struct CommandLineArguments {
    std::string videofile;

    CommandLineArguments() : videofile(std::string("")) {}
} COMMANDLINEARGUMENTS, *PCOMMANDLINEARGUMENTS;

// Process command line arguments
ERRORTYPE ParseCommandLineArguments(COMMANDLINEARGUMENTS &cmdlineargs, int argc, char **argv) {

    int opt = 0;
    while ((opt = getopt(argc, argv, "i:")) != -1) {
        switch (opt) {
            case 'i':
                cmdlineargs.videofile = std::string(optarg);
                break;
            default:
                return FAILED;
        }
    }

    DEBUG("Video file --> %s", cmdlineargs.videofile.c_str());

    if (cmdlineargs.videofile == "") {
        ERROR("videofile required");
        return FAILED;
    }

    return SUCCESSFUL;
}

int main(int argc, char** argv) {
    COMMANDLINEARGUMENTS cmdlineargs;
    
    // Initialize NDIlib
    if(!NDIlib_initialize())
        FATAL("Failed to initialize NDI library");

    // Parse command line arguments
    if(ParseCommandLineArguments(cmdlineargs, argc, argv) == FAILED) {
        ERROR("Failed to process command line input");
        return EXIT_FAILURE;
    }


    // Destroy NDIlib instance
    NDIlib_destroy();

    return EXIT_SUCCESS;
}
