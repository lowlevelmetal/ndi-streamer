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

// POSIX includes
#include <unistd.h>

// Local includes
#include "macro.hpp"
#include "app.hpp"

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

    PRINT("NDI Streamer");
    PRINT("NDI Source: %s", cmdlineargs.ndisource.c_str());
    PRINT("Video File: %s", cmdlineargs.videofile.c_str());

    // Create the application
    auto [app, app_error] = App::Create(cmdlineargs.ndisource, cmdlineargs.videofile);
    if (app_error.code() != (int)AV::Utils::AvError::NOERROR) {
        ERROR("Error creating application: %s", app_error.what());
        return EXIT_FAILURE;
    }

#ifdef _DEBUG
    std::cout << "Press any key to continue..." << std::endl;
    std::cin.get();
#endif

    PRINT("Running application...");

    // Run the application
    auto run_error = app->Run();
    if (run_error.code() != (int)AV::Utils::AvError::NOERROR) {
        ERROR("Error running application: %s", run_error.what());
        return EXIT_FAILURE;
    }


    fflush(stdout);
    fflush(stderr);

    return EXIT_SUCCESS;
}