/*
 * ndi-streamer
 * ndi-streamer.cpp
 *
 * 02-19-2024
 * Matthew Todd Geiger
 */

// Standard library
#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono>

// POSIX includes
#include <unistd.h>

// Local includes
#include "macro.hpp"
#include "mtavserver.hpp"

typedef struct CommandLineArguments {
    std::string videofile;
    std::string ndisource;

    CommandLineArguments() : videofile(std::string("")), ndisource(std::string("NDI Source")) {}
} COMMANDLINEARGUMENTS, *PCOMMANDLINEARGUMENTS;

void Usage(const char *const argv0) {
    printf("\n%s\n"
           "\t-i /path/to/media.mp4\n"
           "\t-s \"NDI Source Name\"\n\n",
           argv0);
}

// Process command line arguments
ERRORTYPE ParseCommandLineArguments(COMMANDLINEARGUMENTS &cmdlineargs, int argc, char **argv) {

    int opt = 0;
    while ((opt = getopt(argc, argv, "i:s:")) != -1) {
        switch (opt) {
        case 'i':
            cmdlineargs.videofile = std::string(optarg);
            break;
        case 's':
            cmdlineargs.ndisource = std::string(optarg);
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

int main(int argc, char **argv) {
    COMMANDLINEARGUMENTS cmdlineargs;

    // Parse command line arguments
    if (!ParseCommandLineArguments(cmdlineargs, argc, argv)) {
        Usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Create NDI AV Server
    auto [ndiavserver, ndiavserver_err] = AV::Utils::MtAvServer::Create(cmdlineargs.ndisource, cmdlineargs.videofile);
    if (ndiavserver_err.code() != (int)AV::Utils::AvError::NOERROR) {
        ERROR("Error creating NDI AV Server: %s", ndiavserver_err.what());
        return EXIT_FAILURE;
    }

#ifdef _DEBUG
    std::cout << "Press any key to continue..." << std::endl;
    std::cin.get();
#endif

    ndiavserver->start();

    // Main loop
    while(1) {
        auto err = ndiavserver->ProcessNextFrame();
        if (err.code() != (int)AV::Utils::AvError::NOERROR) {
            ERROR("Error processing next frame: %s", err.what());
            break;
        }
    }

    fflush(stdout);
    fflush(stderr);

    return EXIT_SUCCESS;
}
