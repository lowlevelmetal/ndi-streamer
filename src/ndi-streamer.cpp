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
#include "averror.hpp"
#include "macro.hpp"
#include "decoder.hpp"

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
    if(ParseCommandLineArguments(cmdlineargs, argc, argv) == FAILED)
        FATAL("Failed to process command line input");

    AV::Decoder decoder(cmdlineargs.videofile);
    const AVFrame *packet;
    AV::AvErrorCode ret;
    while((ret = decoder.ReadFrame(&packet)) == AV::AvErrorCode::NoError) {
        DEBUG("Packet read");
    }

    if(ret != AV::AvErrorCode::PacketsClaimed)
        FATAL("%s", AV::AvErrorStr(ret).c_str());

    // Destroy NDIlib instance
    NDIlib_destroy();

    return EXIT_SUCCESS;
}
